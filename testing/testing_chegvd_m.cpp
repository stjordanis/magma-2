/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    @author Raffaele Solca
    @author Azzam Haidar

    @generated c Fri Jun 28 19:34:04 2013

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

#define PRECISION_c


#include "testings.h"

#define absv(v1) ((v1)>0? (v1): -(v1))

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing chegvd
*/
int main( int argc, char** argv)
{
    TESTING_INIT_MGPU();

    magmaFloatComplex *h_A, *h_Ainit, *h_B, *h_Binit, *h_work;
#if defined(PRECISION_z) || defined(PRECISION_c)
    float *rwork;
#endif
    float *w1, *w2, result;
    magma_int_t *iwork;
    float mgpu_time, gpu_time, cpu_time;

    /* Matrix size */
    magma_int_t N=0, n2;

    magma_int_t info;
    magma_int_t ione = 1;

    magmaFloatComplex c_zero    = MAGMA_C_ZERO;
    magmaFloatComplex c_one     = MAGMA_C_ONE;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;

    magma_int_t ISEED[4] = {0,0,0,1};

    magma_timestr_t start, end;

    magma_opts opts;
    parse_opts( argc, argv, &opts );

    char jobz = opts.jobz;
    int checkres = opts.check;

    char uplo = opts.uplo;
    magma_int_t itype = opts.itype;

    if ( checkres && jobz == MagmaNoVec ) {
        fprintf( stderr, "checking results requires vectors; setting jobz=V (option -JV)\n" );
        jobz = MagmaVec;
    }

    printf("using: nrgpu = %d, itype = %d, jobz = %c, uplo = %c, checkres = %d\n", opts.ngpu, itype, jobz, uplo, checkres);

    printf("  N     M   nr GPU     MGPU Time(s) \n");
    printf("====================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            n2     = N*N;
#if defined(PRECISION_z) || defined(PRECISION_c)
            magma_int_t lwork = 2*N + N*N;
            magma_int_t lrwork = 1 + 5*N +2*N*N;
#else
            magma_int_t lwork  = 1 + 6*N + 2*N*N;
#endif
            magma_int_t liwork = 3 + 5*N;

            TESTING_HOSTALLOC(h_A, magmaFloatComplex, n2);
            TESTING_HOSTALLOC(h_B, magmaFloatComplex, n2);
            TESTING_MALLOC(   w1,  float         ,  N);
            TESTING_MALLOC(   w2,  float         ,  N);
            TESTING_HOSTALLOC(h_work, magmaFloatComplex,  lwork);
#if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_HOSTALLOC( rwork,          float, lrwork);
#endif
            TESTING_MALLOC(    iwork,     magma_int_t, liwork);

            printf("  N     CPU Time(s)    GPU Time(s)   MGPU Time(s) \n");
            printf("==================================================\n");

            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            lapackf77_clarnv( &ione, ISEED, &n2, h_B );
            /* increase the diagonal */
            {
                for(magma_int_t i=0; i<N; i++) {
                    MAGMA_C_SET2REAL( h_B[i*N+i], ( MAGMA_C_REAL(h_B[i*N+i]) + 1.*N ) );
                    MAGMA_C_SET2REAL( h_A[i*N+i], MAGMA_C_REAL(h_A[i*N+i]) );
                }
            }

            if((opts.warmup)||( checkres )){
                TESTING_MALLOC(h_Ainit, magmaFloatComplex, n2);
                TESTING_MALLOC(h_Binit, magmaFloatComplex, n2);
                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_A, &N, h_Ainit, &N );
                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_B, &N, h_Binit, &N );
            }

            if(opts.warmup){

                // ==================================================================
                // Warmup using MAGMA.
                // ==================================================================
                magma_chegvd_m(opts.ngpu, itype, jobz, uplo,
                               N, h_A, N, h_B, N, w1,
                               h_work, lwork,
#if defined(PRECISION_z) || defined(PRECISION_c)
                               rwork, lrwork,
#endif
                               iwork, liwork,
                               &info);
                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_Ainit, &N, h_A, &N );
                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_Binit, &N, h_B, &N );
            }

            // ===================================================================
            // Performs operation using MAGMA
            // ===================================================================

            start = get_current_time();
            magma_chegvd_m(opts.ngpu, itype, jobz, uplo,
                           N, h_A, N, h_B, N, w1,
                           h_work, lwork,
#if defined(PRECISION_z) || defined(PRECISION_c)
                           rwork, lrwork,
#endif
                           iwork, liwork,
                           &info);
            end = get_current_time();

            if(info != 0)
                printf("magma_chegvd_m returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));

            mgpu_time = GetTimerValue(start,end)/1000.;

            if ( checkres ) {
                /* =====================================================================
                 Check the results following the LAPACK's [zc]hegvd routine.
                 A x = lambda B x is solved
                 and the following 3 tests computed:
                 (1)    | A Z - B Z D | / ( |A||Z| N )  (itype = 1)
                 | A B Z - Z D | / ( |A||Z| N )  (itype = 2)
                 | B A Z - Z D | / ( |A||Z| N )  (itype = 3)
                 =================================================================== */

#if defined(PRECISION_d) || defined(PRECISION_s)
                float *rwork = h_work + N*N;
#endif

                result = 1.;
                result /= lapackf77_clanhe("1",&uplo, &N, h_Ainit, &N, rwork);
                result /= lapackf77_clange("1",&N , &N, h_A, &N, rwork);

                if (itype == 1){
                    blasf77_chemm("L", &uplo, &N, &N, &c_one, h_Ainit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_csscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_chemm("L", &uplo, &N, &N, &c_neg_one, h_Binit, &N, h_A, &N, &c_one, h_work, &N);
                    result *= lapackf77_clange("1", &N, &N, h_work, &N, rwork)/N;
                }
                else if (itype == 2){
                    blasf77_chemm("L", &uplo, &N, &N, &c_one, h_Binit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_csscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_chemm("L", &uplo, &N, &N, &c_one, h_Ainit, &N, h_work, &N, &c_neg_one, h_A, &N);
                    result *= lapackf77_clange("1", &N, &N, h_A, &N, rwork)/N;
                }
                else if (itype == 3){
                    blasf77_chemm("L", &uplo, &N, &N, &c_one, h_Ainit, &N, h_A, &N, &c_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_csscal(&N, &w1[i], &h_A[i*N], &ione);
                    blasf77_chemm("L", &uplo, &N, &N, &c_one, h_Binit, &N, h_work, &N, &c_neg_one, h_A, &N);
                    result *= lapackf77_clange("1", &N, &N, h_A, &N, rwork)/N;
                }

                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_Ainit, &N, h_A, &N );
                lapackf77_clacpy( MagmaUpperLowerStr, &N, &N, h_Binit, &N, h_B, &N );

                /* ====================================================================
                 Performs operation using MAGMA
                 =================================================================== */
                start = get_current_time();
                magma_chegvd(itype, jobz, uplo,
                             N, h_A, N, h_B, N, w2,
                             h_work, lwork,
#if defined(PRECISION_z) || defined(PRECISION_c)
                             rwork, lrwork,
#endif
                             iwork, liwork,
                             &info);
                end = get_current_time();

                if(info != 0)
                    printf("magma_chegvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));

                gpu_time = GetTimerValue(start,end)/1000.;

                /* =====================================================================
                 Performs operation using LAPACK
                 =================================================================== */
                start = get_current_time();
                lapackf77_chegvd(&itype, &jobz, &uplo,
                                 &N, h_Ainit, &N, h_Binit, &N, w2,
                                 h_work, &lwork,
#if defined(PRECISION_z) || defined(PRECISION_c)
                                 rwork, &lrwork,
#endif
                                 iwork, &liwork,
                                 &info);
                end = get_current_time();
                if (info != 0)
                    printf("lapackf77_chegvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));

                cpu_time = GetTimerValue(start,end)/1000.;

                float temp1 = 0;
                float temp2 = 0;
                for(int j=0; j<N; j++){
                    temp1 = max(temp1, absv(w1[j]));
                    temp1 = max(temp1, absv(w2[j]));
                    temp2 = max(temp2, absv(w1[j]-w2[j]));
                }
                float result2 = temp2 / temp1;

                /* =====================================================================
                 Print execution time
                 =================================================================== */
                printf("%5d     %6.2f         %6.2f         %6.2f\n",
                       N, cpu_time, gpu_time, mgpu_time);
                printf("Testing the eigenvalues and eigenvectors for correctness:\n");
                if(itype==1)
                    printf("(1)    | A Z - B Z D | / (|A| |Z| N) = %e\n", result);
                else if(itype==2)
                    printf("(1)    | A B Z - Z D | / (|A| |Z| N) = %e\n", result);
                else if(itype==3)
                    printf("(1)    | B A Z - Z D | / (|A| |Z| N) = %e\n", result);

                printf("(3)    | D(MGPU)-D(LAPACK) |/ |D| = %e\n\n", result2);
            }
            else {
                printf("%5d     ------         ------         %6.2f\n",
                       N, mgpu_time);
            }

            /* Memory clean up */
            TESTING_HOSTFREE(   h_A);
            TESTING_HOSTFREE(   h_B);
            TESTING_FREE(        w1);
            TESTING_FREE(        w2);
            TESTING_FREE(     iwork);
            TESTING_HOSTFREE(h_work);
#if defined(PRECISION_z) || defined(PRECISION_c)
            TESTING_HOSTFREE( rwork);
#endif

            if((opts.warmup)||( checkres )){
                TESTING_FREE(   h_Ainit);
                TESTING_FREE(   h_Binit);
            }
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    /* Shutdown */
    TESTING_FINALIZE_MGPU();
}
