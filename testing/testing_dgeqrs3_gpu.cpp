/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated d Fri Jun 28 19:33:56 2013

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_d

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dgeqrs
*/
int main( int argc, char** argv)
{
    TESTING_INIT();
    
    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           gpu_error, cpu_error, matnorm, work[1];
    double  c_one     = MAGMA_D_ONE;
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *h_A, *h_A2, *h_B, *h_X, *h_R, *tau, *h_work, tmp[1];
    double *d_A, *d_B;
    magma_int_t M, N, n2, nrhs, lda, ldb, ldda, lddb, min_mn, max_mn, nb, info;
    magma_int_t lworkgpu, lhwork, lhwork2;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    nrhs = opts.nrhs;
    
    printf("                                                            ||b-Ax|| / (N||A||)\n");
    printf("    M     N  NRHS   CPU GFlop/s (sec)   GPU GFlop/s (sec)   CPU        GPU     \n");
    printf("===============================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[i];
            N = opts.nsize[i];
            if ( M < N ) {
                printf( "skipping M=%d, N=%d because M < N is not yet supported.\n", M, N );
                continue;
            }
            min_mn = min(M, N);
            max_mn = max(M, N);
            lda    = M;
            ldb    = max_mn;
            n2     = lda*N;
            ldda   = ((M+31)/32)*32;
            lddb   = ((max_mn+31)/32)*32;
            nb     = magma_get_dgeqrf_nb(M);
            gflops = (FLOPS_DGEQRF( M, N ) + FLOPS_DGEQRS( M, N, nrhs )) / 1e9;
            
            // query for workspace size
            lworkgpu = (M - N + nb)*(nrhs + nb) + nrhs*nb;
            
            lhwork = -1;
            lapackf77_dgeqrf(&M, &N, h_A, &M, tau, tmp, &lhwork, &info);
            lhwork2 = (magma_int_t) MAGMA_D_REAL( tmp[0] );
            
            lhwork = -1;
            lapackf77_dormqr( MagmaLeftStr, MagmaTransStr,
                              &M, &nrhs, &min_mn, h_A, &lda, tau,
                              h_X, &ldb, tmp, &lhwork, &info);
            lhwork = (magma_int_t) MAGMA_D_REAL( tmp[0] );
            lhwork = max( max( lhwork, lhwork2 ), lworkgpu );
            
            TESTING_MALLOC( tau,  double, min_mn   );
            TESTING_MALLOC( h_A,  double, lda*N    );
            TESTING_MALLOC( h_A2, double, lda*N    );
            TESTING_MALLOC( h_B,  double, ldb*nrhs );
            TESTING_MALLOC( h_X,  double, ldb*nrhs );
            TESTING_MALLOC( h_R,  double, ldb*nrhs );
            TESTING_MALLOC( h_work, double, lhwork );
            
            TESTING_DEVALLOC( d_A, double, ldda*N    );
            TESTING_DEVALLOC( d_B, double, lddb*nrhs );
            
            /* Initialize the matrices */
            lapackf77_dlarnv( &ione, ISEED, &n2, h_A );
            lapackf77_dlacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_A2, &lda );
            
            // make random RHS
            n2 = M*nrhs;
            lapackf77_dlarnv( &ione, ISEED, &n2, h_B );
            lapackf77_dlacpy( MagmaUpperLowerStr, &M, &nrhs, h_B, &ldb, h_R, &ldb );
            
            // make consistent RHS
            //n2 = N*nrhs;
            //lapackf77_dlarnv( &ione, ISEED, &n2, h_X );
            //blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &M, &nrhs, &N,
            //               &c_one,  h_A, &lda,
            //                        h_X, &ldb,
            //               &c_zero, h_B, &ldb );
            //lapackf77_dlacpy( MagmaUpperLowerStr, &M, &nrhs, h_B, &ldb, h_R, &ldb );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_dsetmatrix( M, N,    h_A, lda, d_A, ldda );
            magma_dsetmatrix( M, nrhs, h_B, ldb, d_B, lddb );
            
            gpu_time = magma_wtime();
            magma_dgels3_gpu( MagmaNoTrans, M, N, nrhs, d_A, ldda,
                              d_B, lddb, h_work, lworkgpu, &info);
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_dgels returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            // Get the solution in h_X
            magma_dgetmatrix( N, nrhs, d_B, lddb, h_X, ldb );
            
            // compute the residual
            blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &M, &nrhs, &N,
                           &c_neg_one, h_A, &lda,
                                       h_X, &ldb,
                           &c_one,     h_R, &ldb);
            matnorm = lapackf77_dlange("f", &M, &N, h_A, &lda, work);
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            lapackf77_dlacpy( MagmaUpperLowerStr, &M, &nrhs, h_B, &ldb, h_X, &ldb );
            
            cpu_time = magma_wtime();
            lapackf77_dgels( MagmaNoTransStr, &M, &N, &nrhs,
                             h_A, &lda, h_X, &ldb, h_work, &lhwork, &info);
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            if (info != 0)
                printf("lapackf77_dgels returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr, &M, &nrhs, &N,
                           &c_neg_one, h_A2, &lda,
                                       h_X,  &ldb,
                           &c_one,     h_B,  &ldb);
            
            cpu_error = lapackf77_dlange("f", &M, &nrhs, h_B, &ldb, work) / (min_mn*matnorm);
            gpu_error = lapackf77_dlange("f", &M, &nrhs, h_R, &ldb, work) / (min_mn*matnorm);
            
            printf("%5d %5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %8.2e\n",
                   (int) M, (int) N, (int) nrhs,
                   cpu_perf, cpu_time, gpu_perf, gpu_time, cpu_error, gpu_error );
            
            TESTING_FREE( tau  );
            TESTING_FREE( h_A  );
            TESTING_FREE( h_A2 );
            TESTING_FREE( h_B  );
            TESTING_FREE( h_X  );
            TESTING_FREE( h_R  );
            TESTING_FREE( h_work );
            TESTING_DEVFREE( d_A );
            TESTING_DEVFREE( d_B );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
