/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated s Fri Jun 28 19:33:48 2013
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include <cblas.h>

#include "flops.h"
#include "magma.h"
#include "magmablas.h"
#include "magma_lapack.h"
#include "testings.h"

#define PRECISION_s

#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(n) ( 6. * FMULS_SYMV(n) + 2. * FADDS_SYMV(n))
#else
#define FLOPS(n) (      FMULS_SYMV(n) +      FADDS_SYMV(n))
#endif

#define MultiGPUs
#define validate


#if (GPUSHMEM >= 200)

extern "C"
magma_int_t
magmablas_ssymv2_mgpu_offset( char uplo, magma_int_t n,
                      float alpha,
                      float **A, magma_int_t lda,
                      float **X, magma_int_t incx,
                      float beta,
                      float **Y, magma_int_t incy,
                      float **work, magma_int_t lwork,
              magma_int_t num_gpus, 
              magma_int_t nb,
              magma_int_t offset);


extern "C"
magma_int_t
magmablas_ssymv2_mgpu_32_offset( char uplo, magma_int_t n,
                      float alpha,
                      float **A, magma_int_t lda,
                      float **X, magma_int_t incx,
                      float beta,
                      float **Y, magma_int_t incy,
                      float **work, magma_int_t lwork,
              magma_int_t num_gpus, 
              magma_int_t nb,
              magma_int_t offset);



#endif

int main(int argc, char **argv)
{        
#if (GPUSHMEM >= 200)
    TESTING_INIT();
    magma_setdevice(0);

    magma_timestr_t  start, end;
    float      flops, magma_perf, cuda_perf, error, work[1];
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t n_local[4];

    FILE        *fp ; 
    magma_int_t N, m, i, j, lda, LDA, M;
    magma_int_t matsize;
    magma_int_t vecsize;
    magma_int_t istart = 64;
    magma_int_t incx = 1;
    char        uplo = MagmaLower;

    float alpha = MAGMA_S_MAKE(1., 0.); // MAGMA_S_MAKE(  1.5, -2.3 );
    float beta  = MAGMA_S_MAKE(0., 0.); // MAGMA_S_MAKE( -0.6,  0.8 );
    float *A, *X, *Y[4], *Ycublas, *Ymagma;
    float *dA, *dX[4], *dY[4], *d_lA[4], *dYcublas ;

    magma_queue_t stream[4][10];
    float *C_work;
    float *dC_work[4];

    int max_num_gpus;
    magma_int_t num_gpus = 1, nb;
    magma_int_t blocks, workspace;
    magma_int_t offset;
    
//    offset = 257;

    if (argc != 1){
        for(i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0)
            {
                N = atoi(argv[++i]);
                istart = N;
            }
            else if (strcmp("-M", argv[i])==0)
                M = atoi(argv[++i]);
            else if (strcmp("-NGPU", argv[i])==0)
              num_gpus = atoi(argv[++i]);
            else if (strcmp("-offset", argv[i])==0)
              offset = atoi(argv[++i]);
        }
        if ( M == 0 ) {
            M = N;
        }
        if ( N == 0 ) {
            N = M;
        }
        if (M>0 && N>0)
        {    printf("  testing_ssymv_mgpu -M %d -N %d -NGPU %d\n\n", M, N, num_gpus);
            printf("  in %c side \n", uplo);
        }
        else
            {
                printf("\nUsage: \n");
                printf("  testing_ssymv_mgpu -M %d -N %d -NGPU %d\n\n", 
                       1024, 1024, 1);
                exit(1);
            }
    }
    else {
#if defined(PRECISION_z)
        M = N = 8000;
      
#else
        M = N = 12480;

#endif 
        num_gpus = 2;
        offset = 0;
        printf("\nUsage: \n");
        printf("  testing_ssymv_mgpu -M %d -N %d -NGPU %d\n\n", M, N, num_gpus);
    }
         

///////////////////////////////////////////////////////////////////////////////////////
    cudaGetDeviceCount(&max_num_gpus);
    if (num_gpus > max_num_gpus){
      printf("More GPUs requested than available. Have to change it.\n");
      num_gpus = max_num_gpus;
    }
    printf("Number of GPUs to be used = %d\n", num_gpus);
    for(int i=0; i< num_gpus; i++)
    {
        magma_queue_create(&stream[i][0]);
    }
    

    LDA = ((N+31)/32)*32;
    matsize = N*LDA;
    vecsize = N*incx;
    nb = 32;
    //nb = 64;

    printf("block size = %d\n", nb);
   
    TESTING_MALLOC( A, float, matsize );
    TESTING_MALLOC( X, float, vecsize );
    TESTING_MALLOC( Ycublas, float, vecsize );
    TESTING_MALLOC( Ymagma,  float, vecsize );
    for(i=0; i<num_gpus; i++)
    {     
    TESTING_MALLOC( Y[i], float, vecsize );
    }

    magma_setdevice(0);
    TESTING_DEVALLOC( dA, float, matsize );
    TESTING_DEVALLOC( dYcublas, float, vecsize );

    for(i=0; i<num_gpus; i++)
    {      
      n_local[i] = ((N/nb)/num_gpus)*nb;
      if (i < (N/nb)%num_gpus)
        n_local[i] += nb;
      else if (i == (N/nb)%num_gpus)
        n_local[i] += N%nb;

      magma_setdevice(i);

      TESTING_DEVALLOC( d_lA[i], float, LDA*n_local[i] );// potentially bugged 
      TESTING_DEVALLOC( dX[i], float, vecsize );
      TESTING_DEVALLOC( dY[i], float, vecsize );
      
      printf("device %2d n_local = %4d\n", i, n_local[i]); 

    }
    magma_setdevice(0);

      

///////////////////////////////////////////////////////////////////////

    /* Initialize the matrix */
    lapackf77_slarnv( &ione, ISEED, &matsize, A );
    /* Make A symmetric */
    { 
        magma_int_t i, j;
        for(i=0; i<N; i++) {
            A[i*LDA+i] = MAGMA_S_MAKE( MAGMA_S_REAL(A[i*LDA+i]), 0. );
            for(j=0; j<i; j++)
                A[i*LDA+j] = (A[j*LDA+i]);
        }
    }
        

      blocks    = N / nb + (N % nb != 0);
      workspace = LDA * (blocks + 1);
      TESTING_MALLOC(    C_work, float, workspace );
      for(i=0; i<num_gpus; i++){
             magma_setdevice(i);  
             TESTING_DEVALLOC( dC_work[i], float, workspace );
             //fillZero(dC_work[i], workspace);
      }
      
     magma_setdevice(0);


//////////////////////////////////////////////////////////////////////////////////////////////
   
    fp = fopen ("results_ssymv_mgpu.csv", "w") ;
    if( fp == NULL ){ printf("Couldn't open output file\n"); exit(1);}

    printf("SSYMV float precision\n\n");

    printf( "   n   CUBLAS,Gflop/s   MAGMABLAS,Gflop/s      \"error\"\n" 
            "==============================================================\n");
    fprintf(fp, "   n   CUBLAS,Gflop/s   MAGMABLAS,Gflop/s      \"error\"\n" 
            "==============================================================\n");


//    for( offset = 0; offset< N; offset ++ )
    
    for(int size = istart ; size <= N ; size += 128)
    {
    //    printf("offset = %d ", offset);
        m = size ;
    //    m = N;
        // lda = ((m+31)/32)*32;// 
        lda = LDA; 
        flops = FLOPS( (float)m ) / 1e6;

        printf(      "N %5d ", m );
        fprintf( fp, "%5d, ", m );

        vecsize = m * incx;
        lapackf77_slarnv( &ione, ISEED, &vecsize, X );
        lapackf77_slarnv( &ione, ISEED, &vecsize, Y[0] );

        /* =====================================================================
           Performs operation using CUDA-BLAS
           =================================================================== */
        magma_setdevice(0);
        magma_ssetmatrix_1D_col_bcyclic(m, m, A, LDA, d_lA, lda, num_gpus, nb); 
        magma_setdevice(0);

    
    
    magma_ssetmatrix( m, m, A, LDA, dA, lda );
        magma_ssetvector( m, Y[0], incx, dYcublas, incx );
        
        for(i=0; i<num_gpus; i++){
            magma_setdevice(i);
            magma_ssetvector( m, X, incx, dX[i], incx );
            magma_ssetvector( m, Y[0], incx, dY[i], incx );


            blocks    = m / nb + (m % nb != 0);
            magma_ssetmatrix( lda, blocks, C_work, LDA, dC_work[i], lda );
        }

        magma_setdevice(0);
        start = get_current_time();
        cublasSsymv( uplo, m-offset, alpha, dA + offset + offset * lda, lda, dX[0] + offset, incx, beta, dYcublas + offset, incx );
         
        end = get_current_time();

        magma_sgetvector( m, dYcublas, incx, Ycublas, incx );
                
        
        cuda_perf = flops / GetTimerValue(start,end);
        printf(     "%11.2f", cuda_perf );
        fprintf(fp, "%11.2f,", cuda_perf );
       
        
        magma_setdevice(0);

        
        start = get_current_time();
        

        if(nb == 32)
       { 

        magmablas_ssymv2_mgpu_32_offset( uplo, m, alpha, d_lA, lda, dX, incx, beta, dY, incx, 
                dC_work, workspace, num_gpus, nb, offset);
 
        }
        else // nb = 64
       { 

        magmablas_ssymv2_mgpu_offset( uplo, m, alpha, d_lA, lda, dX, incx, beta, dY, incx, 
                dC_work, workspace, num_gpus, nb, offset);
 
        }
    
            
        for(i=1; i<num_gpus; i++)
        {
           magma_setdevice(i);
           cudaDeviceSynchronize();
        }
      
        end = get_current_time();
        magma_perf = flops / GetTimerValue(start,end); 
        printf(     "%11.2f", magma_perf );
        fprintf(fp, "%11.2f,", magma_perf );
       

        for(i=0; i<num_gpus; i++)
        {        
            magma_setdevice(i);
            magma_sgetvector( m, dY[i], incx, Y[i], incx );
        }
        magma_setdevice(0);

        
#ifdef validate        

        for( j= offset;j<m;j++)
        {
            for(i=1; i<num_gpus; i++)
            {

//            printf("Y[%d][%d] = %15.14f\n", i, j, Y[i][j].x);
#if defined(PRECISION_z) || defined(PRECISION_c)
            Y[0][j].x = Y[0][j].x + Y[i][j].x;
                        Y[0][j].y = Y[0][j].y + Y[i][j].y;
#else 
            Y[0][j] = Y[0][j] + Y[i][j];
            
#endif 

            }
        }

/*

#if defined(PRECISION_z) || defined(PRECISION_c)
        
        for( j=offset;j<m;j++)
        {
            if(Y[0][j].x != Ycublas[j].x)
            {
                     printf("Y-multi[%d] = %f, %f\n",  j, Y[0][j].x, Y[0][j].y );
                     printf("Ycublas[%d] = %f, %f\n",  j, Ycublas[j].x, Ycublas[j].y);
            }
        }

#else 

        for( j=offset;j<m;j++)
        {
            if(Y[0][j] != Ycublas[j])
            {
                     printf("Y-multi[%d] = %f\n",  j, Y[0][j] );
                     printf("Ycublas[%d] = %f\n",  j, Ycublas[j]);
            }
        }

#endif

*/        
        /* =====================================================================
           Computing the Difference Cublas VS Magma
           =================================================================== */
       
        magma_int_t nw = m - offset ;
        blasf77_saxpy( &nw, &c_neg_one, Y[0] + offset, &incx, Ycublas + offset, &incx);
        error = lapackf77_slange( "M", &nw, &ione, Ycublas + offset, &nw, work );
            
#if  0
        printf(      "\t\t %8.6e", error / m );
        fprintf( fp, "\t\t %8.6e", error / m );

        /*
         * Extra check with cblas vs magma
         */
        cblas_scopy( m, Y, incx, Ycublas, incx );
        cblas_ssymv( CblasColMajor, CblasLower, m, 
                     (alpha), A, LDA, X, incx, 
                     (beta), Ycublas, incx );
 
        blasf77_saxpy( &m, &c_neg_one, Ymagma, &incx, Ycublas, &incx);
        error = lapackf77_slange( "M", &m, &ione, Ycublas, &m, work );
#endif

        printf(      "\t\t %8.6e", error / m );
        fprintf( fp, "\t\t %8.6e", error / m );
 
#endif 
        printf("\n");        
        fprintf(fp, "\n");        
    }
    
    fclose( fp ) ; 

    /* Free Memory */
    TESTING_FREE( A );
    TESTING_FREE( X );
    TESTING_FREE( Ycublas );
    TESTING_FREE( Ymagma );
    TESTING_FREE( C_work );

    TESTING_DEVFREE( dA );
    TESTING_DEVFREE( dYcublas );
    
    for(i=0; i<num_gpus; i++)
    { 
        TESTING_FREE( Y[i] );
        magma_setdevice(i);

        TESTING_DEVFREE( d_lA[i] );
        TESTING_DEVFREE( dX[i] );
        TESTING_DEVFREE( dY[i] );


        TESTING_DEVFREE( dC_work[i] );

    }

    magma_setdevice(0);
 ///////////////////////////////////////////////////////////   
      

    /* Free device */
    TESTING_FINALIZE();
#endif
    return 0;
}        
