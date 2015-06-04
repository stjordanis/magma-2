/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated s Fri Jun 28 19:34:06 2013

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <cblas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_s
#define REAL


// DLAPY2 returns sqrt(x**2+y**2), taking care not to cause unnecessary overflow.
// TODO: put into auxiliary file. It's useful elsewhere.
extern "C"
float magma_slapy2(float x, float y)
{
    float ret_val, d;
    float w, z, xabs, yabs;
    
    xabs = MAGMA_S_ABS(x);
    yabs = MAGMA_S_ABS(y);
    w    = max(xabs, yabs);
    z    = min(xabs, yabs);
    if (z == 0.) {
        ret_val = w;
    } else {
        d = z / w;
        ret_val = w * sqrt(d * d + 1.);
    }
    return ret_val;
}


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeev
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    real_Double_t   gpu_time, cpu_time;
    float *h_A, *h_R, *VL, *VR, *h_work, *w1, *w2;
    float c_neg_one = MAGMA_S_NEG_ONE;
    float *w1i, *w2i, work[1];
    float matnorm, tnrm, result[8];
    magma_int_t N, n2, lda, nb, lwork, info;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );
    
    printf("    N   CPU Time (sec)   GPU Time (sec)   ||R||_F / ||A||_F\n");
    printf("===========================================================\n");
    for( int i = 0; i < opts.ntest; ++i ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[i];
            lda   = N;
            n2    = lda*N;
            nb    = magma_get_sgehrd_nb(N);
            lwork = N*(2 + nb);
            // generous workspace - required by sget22
            lwork = max( lwork, N*(5 + 2*N) );
            
            TESTING_MALLOC( w1,  float, N );
            TESTING_MALLOC( w2,  float, N );
            TESTING_MALLOC( w1i, float, N );
            TESTING_MALLOC( w2i, float, N );
            TESTING_MALLOC( h_A, float, n2 );
            TESTING_HOSTALLOC( h_R, float, n2 );
            TESTING_HOSTALLOC( VL,  float, n2 );
            TESTING_HOSTALLOC( VR,  float, n2 );
            TESTING_HOSTALLOC( h_work, float, lwork );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            magma_sgeev_m( opts.jobvl, opts.jobvr,
                           N, h_R, lda, w1, w1i,
                           VL, lda, VR, lda,
                           h_work, lwork, &info );
            gpu_time = magma_wtime() - gpu_time;
            if (info != 0)
                printf("magma_sgeev returned error %d: %s.\n",
                       (int) info, magma_strerror( info ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_sgeev( &opts.jobvl, &opts.jobvr,
                                 &N, h_A, &lda, w2, w2i,
                                 VL, &lda, VR, &lda,
                                 h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                if (info != 0)
                    printf("lapackf77_sgeev returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                
                printf("%5d   %7.2f          %7.2f\n",
                       (int) N, cpu_time, gpu_time);
            }
            else {
                printf("%5d     ---            %7.2f\n",
                       (int) N, gpu_time);
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.check ) {
                /* ===================================================================
                 * Check the result following LAPACK's [zcds]drvev routine.
                 * The following 7 tests are performed:
                 *     (1)     | A * VR - VR * W | / ( n |A| )
                 *
                 *       Here VR is the matrix of unit right eigenvectors.
                 *       W is a diagonal matrix with diagonal entries W(j).
                 *
                 *     (2)     | A**T * VL - VL * W**T | / ( n |A| )
                 *
                 *       Here VL is the matrix of unit left eigenvectors, A**T is the
                 *       transpose of A, and W is as above.
                 *
                 *     (3)     | |VR(i)| - 1 |   and whether largest component real
                 *
                 *       VR(i) denotes the i-th column of VR.
                 *
                 *     (4)     | |VL(i)| - 1 |   and whether largest component real
                 *
                 *       VL(i) denotes the i-th column of VL.
                 *
                 *     (5)     W(full) = W(partial)
                 *
                 *       W(full) denotes the eigenvalues computed when both VR and VL
                 *       are also computed, and W(partial) denotes the eigenvalues
                 *       computed when only W, only W and VR, or only W and VL are
                 *       computed.
                 *
                 *     (6)     VR(full) = VR(partial)
                 *
                 *       VR(full) denotes the right eigenvectors computed when both VR
                 *       and VL are computed, and VR(partial) denotes the result
                 *       when only VR is computed.
                 *
                 *     (7)     VL(full) = VL(partial)
                 *
                 *       VL(full) denotes the left eigenvectors computed when both VR
                 *       and VL are also computed, and VL(partial) denotes the result
                 *       when only VL is computed.
                 ================================================================= */
                float ulp, ulpinv, vmx, vrmx, vtst;
                float *LRE, DUM;
                TESTING_HOSTALLOC( LRE, float, n2 );
                
                ulp = lapackf77_slamch( "P" );
                ulpinv = 1./ulp;
                
                // Initialize RESULT
                for( int j = 0; j < 8; ++j )
                    result[j] = -1.;
                
                lapackf77_slarnv( &ione, ISEED, &n2, h_A );
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                
                // ----------
                // Compute eigenvalues, left and right eigenvectors, and test them
                magma_sgeev_m( MagmaVec, MagmaVec,
                               N, h_R, lda, w1, w1i,
                               VL, lda, VR, lda,
                               h_work, lwork, &info );
                
                // Do test 1
                lapackf77_sget22( MagmaNoTransStr, MagmaNoTransStr, MagmaNoTransStr,
                                  &N, h_A, &lda, VR, &lda, w1, w1i,
                                  h_work, &result[0] );
                result[0] *= ulp;
                
                // Do test 2
                lapackf77_sget22( MagmaTransStr, MagmaNoTransStr, MagmaTransStr,
                                  &N, h_A, &lda, VL, &lda, w1, w1i,
                                  h_work, &result[1] );
                result[1] *= ulp;
                
                // Do test 3
                result[2] = -1.;
                for( int j = 0; j < N; ++j ) {
                    tnrm = 1.;
                    if (w1i[j] == 0.)
                        tnrm = cblas_snrm2(N, &VR[j*lda], ione);
                    else if (w1i[j] > 0.)
                        tnrm = magma_slapy2( cblas_snrm2(N, &VR[j    *lda], ione),
                                             cblas_snrm2(N, &VR[(j+1)*lda], ione) );
                    
                    result[2] = fmax(result[2], fmin(ulpinv, MAGMA_S_ABS(tnrm-1.)/ulp));
                    
                    if (w1i[j] > 0.) {
                        vmx  = vrmx = 0.;
                        for( int jj = 0; jj < N; ++jj ) {
                            vtst = magma_slapy2( VR[jj+j*lda], VR[jj+(j+1)*lda]);
                            if (vtst > vmx)
                                vmx = vtst;
                            
                            if ( (VR[jj + (j+1)*lda])==0. &&
                                 MAGMA_S_ABS( VR[jj+j*lda] ) > vrmx)
                            {
                                vrmx = MAGMA_S_ABS( VR[jj+j*lda] );
                            }
                        }
                        if (vrmx / vmx < 1. - ulp*2.)
                            result[2] = ulpinv;
                    }
                }
                result[2] *= ulp;
                
                // Do test 4
                result[3] = -1.;
                for( int j = 0; j < N; ++j ) {
                    tnrm = 1.;
                    if (w1i[j] == 0.)
                        tnrm = cblas_snrm2(N, &VL[j*lda], ione);
                    else if (w1i[j] > 0.)
                        tnrm = magma_slapy2( cblas_snrm2(N, &VL[j    *lda], ione),
                                             cblas_snrm2(N, &VL[(j+1)*lda], ione) );
                    
                    result[3] = fmax(result[3], fmin(ulpinv, MAGMA_S_ABS(tnrm-1.)/ulp));
                    
                    if (w1i[j] > 0.) {
                        vmx  = vrmx = 0.;
                        for( int jj = 0; jj < N; ++jj ) {
                            vtst = magma_slapy2( VL[jj+j*lda], VL[jj+(j+1)*lda]);
                            if (vtst > vmx)
                                vmx = vtst;
                            
                            if ( (VL[jj + (j+1)*lda])==0. &&
                                 MAGMA_S_ABS( VL[jj+j*lda]) > vrmx)
                            {
                                vrmx = MAGMA_S_ABS( VL[jj+j*lda] );
                            }
                        }
                        if (vrmx / vmx < 1. - ulp*2.)
                            result[3] = ulpinv;
                    }
                }
                result[3] *= ulp;
                
                // ----------
                // Compute eigenvalues only, and test them
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                magma_sgeev_m( MagmaNoVec, MagmaNoVec,
                               N, h_R, lda, w2, w2i,
                               &DUM, 1, &DUM, 1,
                               h_work, lwork, &info );
                
                if (info != 0) {
                    result[0] = ulpinv;
                    printf("magma_sgeev (case N, N) returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
                
                // Do test 5
                result[4] = 1;
                for( int j = 0; j < N; ++j )
                    if ( w1[j] != w2[j] || w1i[j] != w2i[j] )
                        result[4] = 0;
                //if (result[4] == 0) printf("test 5 failed with N N\n");
                
                // ----------
                // Compute eigenvalues and right eigenvectors, and test them
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                magma_sgeev_m( MagmaNoVec, MagmaVec,
                               N, h_R, lda, w2, w2i,
                               &DUM, 1, LRE, lda,
                               h_work, lwork, &info );
                
                if (info != 0) {
                    result[0] = ulpinv;
                    printf("magma_sgeev (case N, V) returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
                
                // Do test 5 again
                result[4] = 1;
                for( int j = 0; j < N; ++j )
                    if ( w1[j] != w2[j] || w1i[j] != w2i[j] )
                        result[4] = 0;
                //if (result[4] == 0) printf("test 5 failed with N V\n");
                
                // Do test 6
                result[5] = 1;
                for( int j = 0; j < N; ++j )
                    for( int jj = 0; jj < N; ++jj )
                        if ( ! MAGMA_S_EQUAL( VR[j+jj*lda], LRE[j+jj*lda] ))
                            result[5] = 0;
                
                // ----------
                // Compute eigenvalues and left eigenvectors, and test them
                lapackf77_slacpy( MagmaUpperLowerStr, &N, &N, h_A, &lda, h_R, &lda );
                magma_sgeev_m( MagmaVec, MagmaNoVec,
                               N, h_R, lda, w2, w2i,
                               LRE, lda, &DUM, 1,
                               h_work, lwork, &info );
                
                if (info != 0) {
                    result[0] = ulpinv;
                    printf("magma_sgeev (case V, N) returned error %d: %s.\n",
                           (int) info, magma_strerror( info ));
                }
                
                // Do test 5 again
                result[4] = 1;
                for( int j = 0; j < N; ++j )
                    if ( w1[j] != w2[j] || w1i[j] != w2i[j] )
                        result[4] = 0;
                //if (result[4] == 0) printf("test 5 failed with V N\n");
                
                // Do test 7
                result[6] = 1;
                for( int j = 0; j < N; ++j )
                    for( int jj = 0; jj < N; ++jj )
                        if ( ! MAGMA_S_EQUAL( VL[j+jj*lda], LRE[j+jj*lda] ))
                            result[6] = 0;
                
                printf("Test 1: | A * VR - VR * W | / ( n |A| ) = %8.2e\n", result[0]);
                printf("Test 2: | A'* VL - VL * W'| / ( n |A| ) = %8.2e\n", result[1]);
                printf("Test 3: |  |VR(i)| - 1    |             = %8.2e\n", result[2]);
                printf("Test 4: |  |VL(i)| - 1    |             = %8.2e\n", result[3]);
                printf("Test 5:   W (full)  ==  W (partial)     = %s\n",   (result[4] == 1. ? "okay" : "fail"));
                printf("Test 6:  VR (full)  == VR (partial)     = %s\n",   (result[5] == 1. ? "okay" : "fail"));
                printf("Test 7:  VL (full)  == VL (partial)     = %s\n\n", (result[6] == 1. ? "okay" : "fail"));
                
                TESTING_HOSTFREE( LRE );
            }
            
            TESTING_FREE( w1  );
            TESTING_FREE( w2  );
            TESTING_FREE( w1i );
            TESTING_FREE( w2i );
            TESTING_FREE( h_A );
            TESTING_HOSTFREE( h_R );
            TESTING_HOSTFREE( VL  );
            TESTING_HOSTFREE( VR  );
            TESTING_HOSTFREE( h_work );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    TESTING_FINALIZE();
    return 0;
}