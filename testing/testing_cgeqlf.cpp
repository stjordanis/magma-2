/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated c Sun Nov 13 20:48:50 2011

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

// Flops formula
#define PRECISION_c
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(m, n) ( 6.*FMULS_GEQLF(m, n) + 2.*FADDS_GEQLF(m, n) )
#else
#define FLOPS(m, n) (    FMULS_GEQLF(m, n) +    FADDS_GEQLF(m, n) )
#endif

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgeqlf
*/
int main( int argc, char** argv)
{
    TESTING_CUDA_INIT();

    magma_timestr_t       start, end;
    float           flops, gpu_perf, cpu_perf;
    float           matnorm, work[1];
    cuFloatComplex  mzone= MAGMA_C_NEG_ONE;
    cuFloatComplex *h_A, *h_R, *tau, *h_work, tmp[1];

    /* Matrix size */
    magma_int_t M = 0, N = 0, n2, lda, ldda, lwork;
    magma_int_t size[10] = {1024,2048,3072,4032,5184,6016,7040,8064,9088,9984};

    magma_int_t i, info, min_mn, nb;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
                N = atoi(argv[++i]);
            else if (strcmp("-M", argv[i])==0)
                M = atoi(argv[++i]);
        }
        if ( M == 0 ) {
            M = N;
        }
        if ( N == 0 ) {
            N = M;
        }
        if (N>0 && M>0)
            printf("  testing_cgeqlf -M %d -N %d\n\n", M, N);
        else
            {
                printf("\nUsage: \n");
                printf("  testing_cgeqlf -M %d -N %d\n\n", M, N);
                exit(1);
            }
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_cgeqlf -M %d -N %d\n\n", 1024, 1024);
        M = N = size[9];
    }

    n2  = M * N;
    min_mn = min(M, N);
    nb = magma_get_cgeqrf_nb(M);

    TESTING_MALLOC(    tau, cuFloatComplex, min_mn );
    TESTING_MALLOC(    h_A, cuFloatComplex, n2     );
    TESTING_HOSTALLOC( h_R, cuFloatComplex, n2     );

    lwork = -1;
    lapackf77_cgeqlf(&M, &N, h_A, &M, tau, tmp, &lwork, &info);
    lwork = (magma_int_t)MAGMA_C_REAL( tmp[0] );
    lwork = max( lwork, N*nb );

    TESTING_MALLOC( h_work, cuFloatComplex, lwork );

    printf("\n\n");
    printf("  M     N   CPU GFlop/s   GPU GFlop/s    ||R||_F / ||A||_F\n");
    printf("==========================================================\n");
    for(i=0; i<10; i++){
        if (argc == 1){
            M = N = size[i];
        }
        min_mn= min(M, N);
        lda   = M;
        n2    = lda*N;
        ldda  = ((M+31)/32)*32;
        flops = FLOPS( (float)M, (float)N ) / 1000000;

        /* Initialize the matrix */
        lapackf77_clarnv( &ione, ISEED, &n2, h_A );
        lapackf77_clacpy( MagmaUpperLowerStr, &M, &N, h_A, &lda, h_R, &lda );

        /* ====================================================================
           Performs operation using MAGMA
           =================================================================== */
        start = get_current_time();
        magma_cgeqlf( M, N, h_R, lda, tau, h_work, lwork, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_cgeqlf had an illegal value.\n", -info);
        
        gpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Performs operation using LAPACK
           =================================================================== */
        start = get_current_time();
        lapackf77_cgeqlf(&M, &N, h_A, &lda, tau, h_work, &lwork, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of lapack_cgeqlf had an illegal value.\n", -info);
        
        cpu_perf = flops / GetTimerValue(start, end);

        /* =====================================================================
           Check the result compared to LAPACK
           =================================================================== */
        matnorm = lapackf77_clange("f", &M, &N, h_A, &lda, work);
        blasf77_caxpy(&n2, &mzone, h_A, &ione, h_R, &ione);

        printf("%5d %5d  %6.2f         %6.2f        %e\n",
               M, N, cpu_perf, gpu_perf,
               lapackf77_clange("f", &M, &N, h_R, &lda, work) / matnorm);

        if (argc != 1)
            break;
    }

    /* Memory clean up */
    TESTING_FREE( tau );
    TESTING_FREE( h_A );
    TESTING_HOSTFREE( h_R );
    TESTING_FREE( h_work );

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
}
