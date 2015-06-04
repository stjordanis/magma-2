/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @author Raffaele Solca
       @author Stan Tomov
       @author Azzam Haidar

       @generated c Fri Jun 28 19:34:02 2013

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

#define PRECISION_c

// CHETRD2 uses much faster CHEMV (from MAGMA BLAS) but requires extra space
// #define USE_CHETRD2

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing chetrd_gpu
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t    gflops, gpu_perf, cpu_perf, gpu_time, cpu_time;
    float           eps;
    magmaFloatComplex *h_A, *h_R, *d_R, *h_Q, *h_work, *work;
    magmaFloatComplex *tau;
    float          *diag, *offdiag;
    float           result[2] = {0., 0.};
    magma_int_t N, n2, lda, lwork, info, nb;
    magma_int_t ione     = 1;
    magma_int_t itwo     = 2;
    magma_int_t ithree   = 3;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    #ifdef USE_CHETRD2
    magma_int_t ldwork;
    magmaFloatComplex *dwork;
    #endif
    
    #if defined(PRECISION_z) || defined(PRECISION_c)
    float *rwork;
    #endif

    eps = lapackf77_slamch( "E" );

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("  N     CPU GFlop/s (sec)   GPU GFlop/s (sec)   |A-QHQ'|/N|A|   |I-QQ'|/N\n");
    printf("===========================================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda    = N;
            n2     = N*lda;
            nb     = magma_get_chetrd_nb(N);
            lwork  = N*nb;  /* We suppose the magma nb is bigger than lapack nb */
            gflops = FLOPS_CHETRD( N ) / 1e9;
            
            TESTING_MALLOC(    h_A,     magmaFloatComplex, lda*N );
            TESTING_DEVALLOC(  d_R,     magmaFloatComplex, lda*N );
            #ifdef USE_CHETRD2
            ldwork = lda*N; // was: lda*N/16+32;
            TESTING_DEVALLOC(  dwork,   magmaFloatComplex, ldwork );
            #endif
            TESTING_HOSTALLOC( h_R,     magmaFloatComplex, lda*N );
            TESTING_HOSTALLOC( h_work,  magmaFloatComplex, lwork );
            TESTING_MALLOC(    tau,     magmaFloatComplex, N     );
            TESTING_MALLOC(    diag,    float, N   );
            TESTING_MALLOC(    offdiag, float, N-1 );
            
            if ( opts.check ) {
                TESTING_MALLOC( h_Q,  magmaFloatComplex, lda*N );
                TESTING_MALLOC( work, magmaFloatComplex, 2*N*N );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_MALLOC( rwork, float, N );
                #endif
            }
            
            /* ====================================================================
               Initialize the matrix
               =================================================================== */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            magma_cmake_hermitian( N, h_A, lda );
            magma_csetmatrix( N, N, h_A, lda, d_R, lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            #ifdef USE_CHETRD2
            magma_chetrd2_gpu( opts.uplo, N, d_R, lda, diag, offdiag,
                               tau, h_R, lda, h_work, lwork, dwork, ldwork, &info );
            #else
            magma_chetrd_gpu( opts.uplo, N, d_R, lda, diag, offdiag,
                              tau, h_R, lda, h_work, lwork, &info );
            #endif
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0)
                printf("magma_chetrd_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.check ) {
                magma_cgetmatrix( N, N, d_R, lda, h_R, lda );
                magma_cgetmatrix( N, N, d_R, lda, h_Q, lda );
                lapackf77_cungtr( &opts.uplo, &N, h_Q, &lda, tau, h_work, &lwork, &info );
                
                #if defined(PRECISION_z) || defined(PRECISION_c)
                lapackf77_chet21( &itwo, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, rwork, &result[0] );
                
                lapackf77_chet21( &ithree, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, rwork, &result[1] );
                #else
                lapackf77_chet21( &itwo, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, &result[0] );
                
                lapackf77_chet21( &ithree, &opts.uplo, &N, &ione,
                                  h_A, &lda, diag, offdiag,
                                  h_Q, &lda, h_R, &lda,
                                  tau, work, &result[1] );
                #endif
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_chetrd( &opts.uplo, &N, h_A, &lda, diag, offdiag, tau,
                                  h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0)
                    printf("lapackf77_chetrd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Print performance and error.
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5d   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (int) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            } else {
                printf("%5d     ---   (  ---  )   %7.2f (%7.2f)",
                       (int) N, gpu_perf, gpu_time );
            }
            if ( opts.check ) {
                printf("   %8.2e        %8.2e\n", result[0]*eps, result[1]*eps );
            } else {
                printf("     ---             ---\n" );
            }
            
            TESTING_FREE( h_A );
            TESTING_FREE( tau );
            TESTING_FREE( diag );
            TESTING_FREE( offdiag );
            TESTING_HOSTFREE( h_R );
            TESTING_HOSTFREE( h_work );
            TESTING_DEVFREE ( d_R );
            #ifdef USE_CHETRD2
            TESTING_DEVFREE ( dwork );
            #endif
            
            if ( opts.check ) {
                TESTING_FREE( h_Q );
                TESTING_FREE( work );
                #if defined(PRECISION_z) || defined(PRECISION_c)
                TESTING_FREE( rwork );
                #endif
            }
        }
    }

    TESTING_FINALIZE();
    return 0;
}
