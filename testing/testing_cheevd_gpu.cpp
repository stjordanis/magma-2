/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    @author Raffaele Solca
    @author Azzam Haidar
    @author Stan Tomov

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
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define absv(v1) ((v1)>0? (v1): -(v1))

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cheevd_gpu
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gpu_time, cpu_time;
    magmaFloatComplex *h_A, *h_R, *d_R, *h_work, aux_work[1];
    float *rwork, *w1, *w2, result[3], eps, aux_rwork[1];
    magma_int_t *iwork, aux_iwork[1];
    magma_int_t N, n2, info, lwork, lrwork, liwork, lda, ldda;
    magma_int_t izero    = 0;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    eps = lapackf77_slamch( "E" );

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    if ( opts.check && opts.jobz == MagmaNoVec ) {
        fprintf( stderr, "checking results requires vectors; setting jobz=V (option -JV)\n" );
        opts.jobz = MagmaVec;
    }
    
    printf("    N   CPU Time (sec)   GPU Time (sec)\n");
    printf("=======================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            n2   = N*N;
            lda  = N;
            ldda = ((N + 31)/32)*32;
            
            /* Query for workspace sizes */
            magma_cheevd_gpu( opts.jobz, opts.uplo,
                              N, d_R, ldda, w1,
                              h_R, lda,
                              aux_work,  -1,
                              aux_rwork, -1,
                              aux_iwork, -1,
                              &info );
            lwork  = (magma_int_t) MAGMA_C_REAL( aux_work[0] );
            lrwork = (magma_int_t) aux_rwork[0];
            liwork = aux_iwork[0];
            
            /* Allocate host memory for the matrix */
            TESTING_MALLOC(    h_A, magmaFloatComplex, N*lda  );
            TESTING_MALLOC(    w1,  float,          N      );
            TESTING_MALLOC(    w2,  float,          N      );
            TESTING_HOSTALLOC( h_R, magmaFloatComplex, N*lda  );
            TESTING_DEVALLOC(  d_R, magmaFloatComplex, N*ldda );
            TESTING_HOSTALLOC( h_work, magmaFloatComplex, lwork  );
            TESTING_MALLOC(    rwork,  float,          lrwork );
            TESTING_MALLOC(    iwork,  magma_int_t,     liwork );
            
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            for( int j=0; j < N; j++ ) {
                h_A[j*N+j] = MAGMA_C_MAKE( MAGMA_C_REAL(h_A[j*N+j]), 0. );
            }
            magma_csetmatrix( N, N, h_A, lda, d_R, ldda );
            
            /* warm up run */
            if ( opts.warmup ) {
                magma_cheevd_gpu( opts.jobz, opts.uplo,
                                  N, d_R, ldda, w1,
                                  h_R, lda,
                                  h_work, lwork,
                                  rwork, lrwork,
                                  iwork, liwork,
                                  &info );
                if (info != 0)
                    printf("magma_cheevd_gpu returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                magma_csetmatrix( N, N, h_A, lda, d_R, ldda );
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            magma_cheevd_gpu( opts.jobz, opts.uplo,
                              N, d_R, ldda, w1,
                              h_R, lda,
                              h_work, lwork,
                              rwork, lrwork,
                              iwork, liwork,
                              &info );
            gpu_time = magma_wtime() - gpu_time;
            if (info != 0)
                printf("magma_cheevd_gpu returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            if ( opts.check ) {
                /* =====================================================================
                   Check the results following the LAPACK's [zcds]drvst routine.
                   A is factored as A = U S U' and the following 3 tests computed:
                   (1)    | A - U S U' | / ( |A| N )
                   (2)    | I - U'U | / ( N )
                   (3)    | S(with U) - S(w/o U) | / | S |
                   =================================================================== */
                float temp1, temp2;
                
                // tau=NULL is unused since itype=1
                magma_cgetmatrix( N, N, d_R, ldda, h_R, lda );
                lapackf77_chet21( &ione, &opts.uplo, &N, &izero,
                                  h_A, &lda,
                                  w1, w1,
                                  h_R, &lda,
                                  h_R, &lda,
                                  NULL, h_work, rwork, &result[0]);
                
                magma_csetmatrix( N, N, h_A, lda, d_R, ldda );
                magma_cheevd_gpu( MagmaNoVec, opts.uplo,
                                  N, d_R, ldda, w2,
                                  h_R, lda,
                                  h_work, lwork,
                                  rwork, lrwork,
                                  iwork, liwork,
                                  &info);
                if (info != 0)
                    printf("magma_cheevd_gpu returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                temp1 = temp2 = 0;
                for( int j=0; j<N; j++ ) {
                    temp1 = max(temp1, absv(w1[j]));
                    temp1 = max(temp1, absv(w2[j]));
                    temp2 = max(temp2, absv(w1[j]-w2[j]));
                }
                result[2] = temp2 / temp1;
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_cheevd(&opts.jobz, &opts.uplo,
                                 &N, h_A, &lda, w2,
                                 h_work, &lwork,
                                 rwork, &lrwork,
                                 iwork, &liwork,
                                 &info);
                cpu_time = magma_wtime() - cpu_time;
                if (info != 0)
                    printf("lapackf77_cheevd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                printf("%5d     %7.2f         %7.2f\n",
                       (int) N, cpu_time, gpu_time);
            }
            else {
                printf("%5d       ---           %7.2f\n",
                       (int) N, gpu_time);
            }
            
            /* =====================================================================
               Print execution time
               =================================================================== */
            if ( opts.check ) {
                printf("Testing the factorization A = U S U' for correctness:\n");
                printf("(1)    | A - U S U' | / (|A| N)     = %8.2e\n",   result[0]*eps );
                printf("(2)    | I -   U'U  | /  N          = %8.2e\n",   result[1]*eps );
                printf("(3)    | S(w/ U) - S(w/o U) | / |S| = %8.2e\n\n", result[2]     );
            }

            TESTING_FREE(     h_A    );
            TESTING_FREE(     w1     );
            TESTING_FREE(     w2     );
            TESTING_FREE(     rwork  );
            TESTING_FREE(     iwork  );
            TESTING_HOSTFREE( h_work );
            TESTING_HOSTFREE( h_R    );
            TESTING_DEVFREE(  d_R    );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return 0;
}
