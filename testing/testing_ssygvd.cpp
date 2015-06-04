/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    @author Stan Tomov
    @author Raffaele Solca
    @author Azzam Haidar

    @generated s Fri Jun 28 19:34:04 2013

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
   -- Testing ssygvd
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gpu_time, cpu_time;
    float *h_A, *h_R, *h_B, *h_S, *h_work;
    float *w1, *w2;
    magma_int_t *iwork;
    magma_int_t N, n2, info, nb, lwork, liwork, lda;
    float result[4];

    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;

    float d_zero        =  0.;
    float d_one         =  1.;
    float d_neg_one     = -1.;
    //magma_int_t izero    = 0;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    if ( opts.check && opts.jobz == MagmaNoVec ) {
        fprintf( stderr, "checking results requires vectors; setting jobz=V (option -JV)\n" );
        opts.jobz = MagmaVec;
    }
    
    printf("    N   CPU Time (sec)   GPU Time(sec)\n");
    printf("======================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda    = N;
            n2     = N*lda;
            nb     = magma_get_ssytrd_nb(N);
            lwork  = 1 + 6*N*nb + 2* N*N;
            liwork = 3 + 5*N;

            TESTING_MALLOC(    h_A,    float, n2 );
            TESTING_MALLOC(    h_B,    float, n2 );
            TESTING_MALLOC(    w1,     float, N  );
            TESTING_MALLOC(    w2,     float, N  );
            TESTING_HOSTALLOC( h_R,    float, n2 );
            TESTING_HOSTALLOC( h_S,    float, n2 );
            TESTING_HOSTALLOC( h_work, float,      lwork  );
            TESTING_MALLOC(    iwork,  magma_int_t, liwork );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            lapackf77_slarnv( &ione, ISEED, &n2, h_B );
            magma_smake_hpd( N, h_B, lda );
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_B, &lda, h_S, &lda );
            
            /* warmup */
            if ( opts.warmup ) {
                magma_ssygvd( opts.itype, opts.jobz, opts.uplo,
                              N, h_R, lda, h_S, lda, w1,
                              h_work, lwork,
                              iwork, liwork,
                              &info );
                if (info != 0)
                    printf("magma_ssygvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_B, &lda, h_S, &lda );
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            magma_ssygvd( opts.itype, opts.jobz, opts.uplo,
                          N, h_R, lda, h_S, lda, w1,
                          h_work, lwork,
                          iwork, liwork,
                          &info );
            gpu_time = magma_wtime() - gpu_time;
            if (info != 0)
                printf("magma_ssygvd returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            if ( opts.check ) {
                /* =====================================================================
                   Check the results following the LAPACK's [zc]hegvd routine.
                   A x = lambda B x is solved
                   and the following 3 tests computed:
                   (1)    | A Z - B Z D | / ( |A||Z| N )   (itype = 1)
                          | A B Z - Z D | / ( |A||Z| N )   (itype = 2)
                          | B A Z - Z D | / ( |A||Z| N )   (itype = 3)
                   (2)    | I - V V' B | / ( N )           (itype = 1,2)
                          | B - V V' | / ( |B| N )         (itype = 3)
                   (3)    | S(with V) - S(w/o V) | / | S |
                   =================================================================== */
                float temp1, temp2;
                //float *tau;
                
                if ( opts.itype == 1 || opts.itype == 2 ) {
                    lapackf77_slaset( "A", &N, &N, &d_zero, &c_one, h_S, &lda);
                    blasf77_sgemm("N", "C", &N, &N, &N, &c_one, h_R, &lda, h_R, &lda, &d_zero, h_work, &N);
                    blasf77_ssymm("R", &opts.uplo, &N, &N, &c_neg_one, h_B, &lda, h_work, &N, &c_one, h_S, &lda);
                    result[1] = lapackf77_slange("1", &N, &N, h_S, &lda, h_work) / N;
                }
                else if ( opts.itype == 3 ) {
                    lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_B, &lda, h_S, &lda);
                    blasf77_ssyrk(&opts.uplo, "N", &N, &N, &d_neg_one, h_R, &lda, &d_one, h_S, &lda);
                    result[1] = lapackf77_slansy("1", &opts.uplo, &N, h_S, &lda, h_work) / N
                              / lapackf77_slansy("1", &opts.uplo, &N, h_B, &lda, h_work);
                }
                
                result[0] = 1.;
                result[0] /= lapackf77_slansy("1", &opts.uplo, &N, h_A, &lda, h_work);
                result[0] /= lapackf77_slange("1", &N, &N, h_R, &lda, h_work);
                
                if ( opts.itype == 1 ) {
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_one, h_A, &lda, h_R, &lda, &d_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_neg_one, h_B, &lda, h_R, &lda, &c_one, h_work, &N);
                    result[0] *= lapackf77_slange("1", &N, &N, h_work, &N, &temp1)/N;
                }
                else if ( opts.itype == 2 ) {
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_one, h_B, &lda, h_R, &lda, &d_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_one, h_A, &lda, h_work, &N, &c_neg_one, h_R, &lda);
                    result[0] *= lapackf77_slange("1", &N, &N, h_R, &lda, &temp1)/N;
                }
                else if ( opts.itype == 3 ) {
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_one, h_A, &lda, h_R, &lda, &d_zero, h_work, &N);
                    for(int i=0; i<N; ++i)
                        blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                    blasf77_ssymm("L", &opts.uplo, &N, &N, &c_one, h_B, &lda, h_work, &N, &c_neg_one, h_R, &lda);
                    result[0] *= lapackf77_slange("1", &N, &N, h_R, &lda, &temp1)/N;
                }
                
                /*
                lapackf77_ssyt21(&ione, &opts.uplo, &N, &izero,
                                 h_A, &lda,
                                 w1, w1,
                                 h_R, &lda,
                                 h_R, &lda,
                                 tau, h_work, rwork, &result[0]);
                */
                
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_B, &lda, h_S, &lda );
                
                magma_ssygvd( opts.itype, MagmaNoVec, opts.uplo,
                              N, h_R, lda, h_S, lda, w2,
                              h_work, lwork,
                              iwork, liwork,
                              &info );
                if (info != 0)
                    printf("magma_ssygvd returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                temp1 = temp2 = 0;
                for(int j=0; j<N; j++) {
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
                lapackf77_ssygvd( &opts.itype, &opts.jobz, &opts.uplo,
                                  &N, h_A, &lda, h_B, &lda, w2,
                                  h_work, &lwork,
                                  iwork, &liwork,
                                  &info );
                cpu_time = magma_wtime() - cpu_time;
                if (info != 0)
                    printf("lapackf77_ssygvd returned error %d: %s.\n",
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
                printf("Testing the eigenvalues and eigenvectors for correctness:\n");
                if ( opts.itype==1 )
                    printf("(1)    | A Z - B Z D | / (|A| |Z| N) = %8.2e\n", result[0]);
                else if ( opts.itype==2 )
                    printf("(1)    | A B Z - Z D | / (|A| |Z| N) = %8.2e\n", result[0]);
                else if ( opts.itype==3 )
                    printf("(1)    | B A Z - Z D | / (|A| |Z| N) = %8.2e\n", result[0]);
                if ( opts.itype==1 || opts.itype==2 )
                    printf("(2)    | I -   Z Z' B | /  N         = %8.2e\n", result[1]);
                else
                    printf("(2)    | B -  Z Z' | / (|B| N)       = %8.2e\n", result[1]);
                printf(    "(3)    | D(w/ Z) - D(w/o Z) | / |D|  = %8.2e\n\n", result[2]);
            }
            
            TESTING_FREE( h_A );
            TESTING_FREE( h_B );
            TESTING_FREE( w1  );
            TESTING_FREE( w2  );
            TESTING_FREE( iwork );
            TESTING_HOSTFREE( h_work );
            TESTING_HOSTFREE( h_R );
            TESTING_HOSTFREE( h_S );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    TESTING_FINALIZE();
    return 0;
}
