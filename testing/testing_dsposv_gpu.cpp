/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated ds Sun Nov 13 20:48:56 2011
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_d
// Flops formula
#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS_POTRF(n)       ( 6.*FMULS_POTRF(n)       + 2.*FADDS_POTRF(n)       )
#define FLOPS_POTRS(n, nrhs) ( 6.*FMULS_POTRS(n, nrhs) + 2.*FADDS_POTRS(n, nrhs) )
#else
#define FLOPS_POTRF(n)       (    FMULS_POTRF(n)       +    FADDS_POTRF(n)       )
#define FLOPS_POTRS(n, nrhs) (    FMULS_POTRS(n, nrhs) +    FADDS_POTRS(n, nrhs) )
#endif

int main(int argc, char **argv)
{
    TESTING_CUDA_INIT();

    magma_timestr_t  start, end;
    double      flopsF, flopsS, gpu_perf;
    double      gpu_perfdf, gpu_perfds;
    double      gpu_perfsf, gpu_perfss;
    double      Rnorm, Anorm;
    double zone  = MAGMA_D_ONE;
    double mzone = MAGMA_D_NEG_ONE;
    double *h_A, *h_B, *h_X;
    double *d_A, *d_B, *d_X, *d_WORKD;
    float  *d_As, *d_Bs, *d_WORKS;
    double          *h_workd;
    magma_int_t lda, ldb, ldx;
    magma_int_t i, iter, info, size;
    magma_int_t N        = 0;
    magma_int_t ione     = 1;
    magma_int_t NRHS     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t sizetest[10] = {1024,2048,3072,4032,5184,6016,7040,7520,8064,8192};
    // const char *uplo = MagmaUpperStr;
    const char *uplo = MagmaLowerStr;
    
    if (argc != 1){
        for(i = 1; i<argc; i++){        
            if (strcmp("-N", argv[i])==0)
                N = atoi(argv[++i]);
            else if (strcmp("-nrhs", argv[i])==0)
                NRHS = atoi(argv[++i]);
        }
        if (N>0) sizetest[0] = sizetest[9] = N;
        else exit(1);
    }
    else {
        printf("\nUsage: \n");
        printf("  testing_dsposv_gpu -nrhs %d -N %d\n\n", NRHS, 1024);
    }
    printf("Epsilon(double): %8.6e\n"
           "Epsilon(single): %8.6e\n\n", 
           lapackf77_dlamch("Epsilon"), lapackf77_slamch("Epsilon") );
    
    N = sizetest[9];
    ldb = ldx = lda = N ;
    TESTING_MALLOC( h_A, double, lda*N    );
    TESTING_MALLOC( h_B, double, ldb*NRHS );
    TESTING_MALLOC( h_X, double, ldx*NRHS );
    TESTING_MALLOC( h_workd, double, N );
    
    TESTING_DEVALLOC( d_A,     double, lda*N       );
    TESTING_DEVALLOC( d_B,     double, ldb*NRHS    );
    TESTING_DEVALLOC( d_X,     double, ldx*NRHS    );
    TESTING_DEVALLOC( d_WORKS, float,  lda*(N+NRHS));
    TESTING_DEVALLOC( d_WORKD, double, N*NRHS      );

    printf("  N   DP-Factor  DP-Solve  SP-Factor  SP-Solve  MP-Solve  ||b-Ax||/||A||  NumIter\n");
    printf("==================================================================================\n");
    for(i=0; i<10; i++){
        N = sizetest[i] ;
        
        flopsF = FLOPS_POTRF( (double)N ) / 1000000;
        flopsS = flopsF + ( FLOPS_POTRS( (double)N, (double)NRHS ) / 1000000 );
        ldb = ldx = lda = N;
        
        /* Initialize the matrix */
        size = lda * N ;
        lapackf77_dlarnv( &ione, ISEED, &size, h_A );
        /* Increase the diagonal (We don't set the matrix as 
           hermitian since we use only a triangular part) */
        {
            magma_int_t i;
            for(i=0; i<N; i++) {
                MAGMA_D_SET2REAL( h_A[i*lda+i], ( MAGMA_D_GET_X(h_A[i*lda+i]) + 1.*N ) );
            }
        }
        
        size = ldb * NRHS ;
        lapackf77_dlarnv( &ione, ISEED, &size, h_B );
      
        cublasSetMatrix( N, N,    sizeof(double), h_A, lda, d_A, lda );
        cublasSetMatrix( N, NRHS, sizeof(double), h_B, ldb, d_B, ldb );
    
        printf("%5d  ", N); fflush(stdout);

        //=====================================================================
        //              Mixed Precision Iterative Refinement - GPU 
        //=====================================================================
        start = get_current_time();
        magma_dsposv_gpu(uplo[0], N, NRHS, d_A, lda, d_B, ldb, d_X, ldx, 
                         d_WORKD, d_WORKS, &iter, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_dsposv had an illegal value.\n", -info);
        gpu_perf = flopsS / GetTimerValue(start, end);

        //=====================================================================
        //                 Error Computation 
        //=====================================================================
        cublasGetMatrix( N, NRHS, sizeof(double), d_X, ldx, h_X, ldx ) ;

        Anorm = lapackf77_dlansy( "I", uplo, &N, h_A, &N, h_workd);
        blasf77_dsymm( "L", uplo, &N, &NRHS, 
                       &zone,  h_A, &lda,
                               h_X, &ldx,
                       &mzone, h_B, &ldb);
        Rnorm = lapackf77_dlange( "I", &N, &NRHS, h_B, &ldb, h_workd);

        //=====================================================================
        //                 Double Precision Factor 
        //=====================================================================
        cublasSetMatrix( N, N, sizeof(double), h_A, lda, d_A, lda );

        start = get_current_time();
        magma_dpotrf_gpu(uplo[0], N, d_A, lda, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_dpotrf had an illegal value.\n", -info);
        gpu_perfdf = flopsF / GetTimerValue(start, end);

        printf("%6.2f    ", gpu_perfdf); fflush(stdout);

        //=====================================================================
        //                 Double Precision Solve 
        //=====================================================================
        cublasSetMatrix( N, N,    sizeof(double), h_A, lda, d_A, lda );
        cublasSetMatrix( N, NRHS, sizeof(double), h_B, ldb, d_B, ldb );
    
        start = get_current_time();
        magma_dpotrf_gpu(uplo[0], N, d_A, lda, &info);
        magma_dpotrs_gpu(uplo[0], N, NRHS, d_A, lda, d_B, ldb, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_dpotrs had an illegal value.\n", -info);

        gpu_perfds = flopsS / GetTimerValue(start, end);

        printf("%6.2f    ", gpu_perfds); fflush(stdout);

        //=====================================================================
        //                 Single Precision Factor 
        //=====================================================================
        d_As = d_WORKS;
        d_Bs = d_WORKS + lda*N;
        cublasSetMatrix( N, N,    sizeof(double), h_A, lda, d_A, lda );
        cublasSetMatrix( N, NRHS, sizeof(double), h_B, ldb, d_B, ldb );
        magmablas_dlag2s(N, N,    d_A, lda, d_As, N, &info ); 
        magmablas_dlag2s(N, NRHS, d_B, ldb, d_Bs, N, &info );

        start = get_current_time();
        magma_spotrf_gpu(uplo[0], N, d_As, N, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_spotrf had an illegal value.\n", -info);

        gpu_perfsf = flopsF / GetTimerValue(start, end);
        printf("%6.2f     ", gpu_perfsf); fflush(stdout);

        //=====================================================================
        //                 Single Precision Solve 
        //=====================================================================
        magmablas_dlag2s(N, N,    d_A, lda, d_As, N, &info ); 
        magmablas_dlag2s(N, NRHS, d_B, ldb, d_Bs, N, &info );

        start = get_current_time();
        magma_spotrf_gpu(uplo[0], N, d_As, lda, &info);
        magma_spotrs_gpu(uplo[0], N, NRHS, d_As, N, d_Bs, N, &info);
        end = get_current_time();
        if (info < 0)
            printf("Argument %d of magma_spotrs had an illegal value.\n", -info);

        gpu_perfss = flopsS / GetTimerValue(start, end);
        printf("%6.2f     ", gpu_perfss); fflush(stdout);

        printf("%6.2f     ", gpu_perf);
        printf("%e    %3d\n", Rnorm/Anorm, iter); fflush(stdout);

        if( argc != 1 ){
            break;
        } 
    }

    /* Memory clean up */
    TESTING_FREE( h_A );
    TESTING_FREE( h_B );
    TESTING_FREE( h_X );
    TESTING_FREE( h_workd );
    
    TESTING_DEVFREE( d_A );
    TESTING_DEVFREE( d_B );
    TESTING_DEVFREE( d_X );
    TESTING_DEVFREE( d_WORKS );
    TESTING_DEVFREE( d_WORKD );

    /* Shutdown */
    TESTING_CUDA_FINALIZE();
}
