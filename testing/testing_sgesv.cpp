/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated s Fri Jun 28 19:33:54 2013
       @author Mark Gates
*/
// includes, system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgesv
*/
int main(int argc, char **argv)
{
    TESTING_INIT();

    real_Double_t   gflops, cpu_perf, cpu_time, gpu_perf, gpu_time;
    float          Rnorm, Anorm, Xnorm, *work;
    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;
    float *h_A, *h_LU, *h_B, *h_X;
    magma_int_t *ipiv;
    magma_int_t N, nrhs, lda, ldb, info, sizeA, sizeB;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    nrhs = opts.nrhs;
    
    printf("ngpu %d\n", opts.ngpu );
    printf("    N  NRHS   CPU Gflop/s (sec)   GPU GFlop/s (sec)   ||B - AX|| / ||A||*||X||\n");
    printf("==============================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda    = N;
            ldb    = lda;
            gflops = ( FLOPS_SGETRF( N, N ) + FLOPS_SGETRS( N, nrhs ) ) / 1e9;
            
            TESTING_MALLOC( h_A,  float, lda*N    );
            TESTING_MALLOC( h_LU, float, lda*N    );
            TESTING_MALLOC( h_B,  float, ldb*nrhs );
            TESTING_MALLOC( h_X,  float, ldb*nrhs );
            TESTING_MALLOC( work, float,          N        );
            TESTING_MALLOC( ipiv, magma_int_t,     N        );
            
            /* Initialize the matrices */
            sizeA = lda*N;
            sizeB = ldb*nrhs;
            lapackf77_slarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_slarnv( &ione, ISEED, &sizeB, h_B );
            
            // copy A to LU and B to X; save A and B for residual
            lapackf77_slacpy( "F", &N, &N,    h_A, &lda, h_LU, &lda );
            lapackf77_slacpy( "F", &N, &nrhs, h_B, &ldb, h_X,  &ldb );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            magma_sgesv( N, nrhs, h_LU, lda, ipiv, h_X, ldb, &info );
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_sgesv returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            //=====================================================================
            // Residual
            //=====================================================================
            Anorm = lapackf77_slange("I", &N, &N,    h_A, &lda, work);
            Xnorm = lapackf77_slange("I", &N, &nrhs, h_X, &ldb, work);
            
            blasf77_sgemm( MagmaNoTransStr, MagmaNoTransStr, &N, &nrhs, &N,
                           &c_one,     h_A, &lda,
                                       h_X, &ldb,
                           &c_neg_one, h_B, &ldb);
            
            Rnorm = lapackf77_slange("I", &N, &nrhs, h_B, &ldb, work);
            
            
            /* ====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_sgesv( &N, &nrhs, h_A, &lda, ipiv, h_B, &ldb, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_sgesv returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                printf( "%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e\n",
                        (int) N, (int) nrhs, cpu_perf, cpu_time, gpu_perf, gpu_time, Rnorm/(Anorm*Xnorm) );
            }
            else {
                printf( "%5d %5d     ---   (  ---  )   %7.2f (%7.2f)   %8.2e\n",
                        (int) N, (int) nrhs, gpu_perf, gpu_time, Rnorm/(Anorm*Xnorm) );
            }
            
            TESTING_FREE( h_A  );
            TESTING_FREE( h_LU );
            TESTING_FREE( h_B  );
            TESTING_FREE( h_X  );
            TESTING_FREE( work );
            TESTING_FREE( ipiv );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
