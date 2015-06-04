/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated s Sun Nov 13 20:48:39 2011

*/
#include "common_magma.h"

#define slascl_bs 64


__global__ void
l_slascl (int m, int n, float mul, float* A, int lda){
    int ind =  blockIdx.x * slascl_bs + threadIdx.x ;

    int break_d = (ind < n)? ind: n-1;

    A += ind;
    if (ind < m)
       for(int j=0; j<=break_d; j++ )
           A[j*lda] *= mul;
}

__global__ void
u_slascl (int m, int n, float mul, float* A, int lda){
    int ind =  blockIdx.x * slascl_bs + threadIdx.x ;

    A += ind;
    if (ind < m)
      for(int j=n-1; j>= ind; j--)
         A[j*lda] *= mul;
}


extern "C" void
magmablas_slascl(char type, int kl, int ku, 
                 float cfrom, float cto,
                 int m, int n, 
                 float *A, int lda, int *info )
{
    int blocks;
    if (m % slascl_bs==0)
        blocks = m/ slascl_bs;
    else
        blocks = m/ slascl_bs + 1;

    dim3 grid(blocks, 1, 1);
    dim3 threads(slascl_bs, 1, 1);

    /* To do : implment the accuracy procedure */
    float mul = cto / cfrom;

    if (type == 'L' || type =='l')  
       l_slascl <<< grid, threads, 0, magma_stream >>> (m, n, mul, A, lda);
    else if (type == 'U' || type =='u')
       u_slascl <<< grid, threads, 0, magma_stream >>> (m, n, mul, A, lda);  
    else {
       printf("Only type L and U are available in slascl. Exit.\n");
       exit(1);
    }
}


