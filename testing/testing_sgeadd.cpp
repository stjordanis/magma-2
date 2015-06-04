/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated s Fri Jun 28 19:33:44 2013
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeadd
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float          error, work[1];
    float *h_A, *h_B, *d_A, *d_B;
    float alpha = MAGMA_S_MAKE( 3.1415, 2.718 );
    float c_neg_one = MAGMA_S_NEG_ONE;
    
    magma_int_t M, N, size, lda, ldda;
    magma_int_t ione = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    /* Uncomment these lines to check parameters.
     * magma_xerbla calls lapack's xerbla to print out error. */
    //magmablas_sgeadd( -1,  N, alpha, d_A, ldda, d_B, ldda );
    //magmablas_sgeadd(  M, -1, alpha, d_A, ldda, d_B, ldda );
    //magmablas_sgeadd(  M,  N, alpha, d_A, M-1,  d_B, ldda );
    //magmablas_sgeadd(  M,  N, alpha, d_A, ldda, d_B, N-1  );

    printf("    M     N   CPU GFlop/s (sec)   GPU GFlop/s (sec)   |Bl-Bm|/|Bl|\n");
    printf("=========================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[i];
            N = opts.nsize[i];
            lda    = M;
            ldda   = ((M+31)/32)*32;
            size   = lda*N;
            gflops = 2.*M*N / 1e9;
            
            TESTING_MALLOC(   h_A, float, lda *N );
            TESTING_MALLOC(   h_B, float, lda *N );
            TESTING_DEVALLOC( d_A, float, ldda*N );
            TESTING_DEVALLOC( d_B, float, ldda*N );
            
            lapackf77_slarnv( &ione, ISEED, &size, h_A );
            lapackf77_slarnv( &ione, ISEED, &size, h_B );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_ssetmatrix( M, N, h_A, lda, d_A, ldda );
            magma_ssetmatrix( M, N, h_B, lda, d_B, ldda );
            
            gpu_time = magma_sync_wtime( NULL );
            magmablas_sgeadd( M, N, alpha, d_A, ldda, d_B, ldda );
            gpu_time = magma_sync_wtime( NULL ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            for( int j = 0; j < N; ++j ) {
                blasf77_saxpy( &M, &alpha, &h_A[j*lda], &ione, &h_B[j*lda], &ione );
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check result
               =================================================================== */
            magma_sgetmatrix( M, N, d_B, ldda, h_A, lda );
            
            error = lapackf77_slange( "F", &M, &N, h_B, &lda, work );
            blasf77_saxpy( &size, &c_neg_one, h_A, &ione, h_B, &ione );
            error = lapackf77_slange( "F", &M, &N, h_B, &lda, work ) / error;
            
            printf("%5d %5d   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e\n",
                   (int) M, (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time, error );
            
            TESTING_FREE( h_A );
            TESTING_FREE( h_B );
            TESTING_DEVFREE(  d_A );
            TESTING_DEVFREE(  d_B );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}
