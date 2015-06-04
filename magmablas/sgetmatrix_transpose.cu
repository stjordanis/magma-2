/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated s Sun Nov 13 20:48:40 2011

*/
#include "common_magma.h"
#define PRECISION_s
#include "commonblas.h"

extern "C" void
magmablas_stranspose2s(float *odata, int ldo,
                       float *idata, int ldi,
                       int m, int n, cudaStream_t *stream );


//
//      m, n - dimensions in the output (ha) matrix.
//             This routine copies the dat matrix from the GPU
//             to ha on the CPU. In addition, the output matrix
//             is transposed. The routine uses a buffer of size
//             2*lddb*nb pointed to by dB (lddb > m) on the GPU. 
//             Note that lda >= m and lddat >= n.
//
extern "C" void 
magmablas_sgetmatrix_transpose( int m, int n,
                                float *dat, int ldda,
                                float  *ha, int lda,
                                float  *dB, int lddb, int nb )
{
    int i = 0, j = 0, ib;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || ldda < n || lddb < m){
        printf("Wrong arguments in zdtoht.\n");
        return;
    }

    static cudaStream_t stream[2];
    cudaStreamCreate(&stream[0]);
    cudaStreamCreate(&stream[1]);

    for(i=0; i<n; i+=nb){
       /* Move data from GPU to CPU using 2 buffers; 1st transpose the data on the GPU */
       ib   = min(n-i, nb);

       //magmablas_stranspose2 ( dB + (j%2)*nb*lddb, lddb, dat+i, ldda, ib, m);
       magmablas_stranspose2s( dB + (j%2)*nb*lddb, lddb, dat+i, ldda, ib, m, &stream[j%2]);
       cudaMemcpy2DAsync(ha+i*lda, lda*sizeof(float),
                         dB + (j%2) * nb * lddb, lddb*sizeof(float),
                         sizeof(float)*m, ib, 
                         cudaMemcpyDeviceToHost, stream[j%2]);
       j++;
    }

    cudaStreamDestroy( stream[0] );
    cudaStreamDestroy( stream[1] );
}

//===========================================================================
//  This version is similar to the above but for multiGPUs. The distribution
//  is 1D block cyclic. The input arrays are pointers for the corresponding
//  GPUs. The streams are passed as argument, in contrast to the single GPU
//  routine.
//===========================================================================
extern "C" void
magmablas_sgetmatrix_transpose2( int m, int n,
                                 float **dat, int *ldda,
                                 float  *ha,  int  lda,
                                 float **dB,  int  lddb, int nb,
                                 int num_gpus, cudaStream_t stream[][2] )
{
    int i = 0, j[4] = {0, 0, 0, 0}, ib, k;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || lddb < m){
        printf("Wrong arguments in zdtoht.\n");
        return;
    }

    for(i=0; i<n; i+=nb){
       /* Move data from GPU to CPU using 2 buffers; 1st transpose the data on the GPU */
       k = (i/nb)%num_gpus;
       ib   = min(n-i, nb);
       cudaSetDevice(k);

       //cudaStreamSynchronize(stream[k][j[k]%2]);
       //magmablas_stranspose2( dB[k] + (j[k]%2)*nb*lddb, lddb, 
       //                       dat[k]+i/(nb*num_gpus)*nb, ldda[k], ib, m);
       magmablas_stranspose2s(dB[k] + (j[k]%2)*nb*lddb, lddb,
                              dat[k]+i/(nb*num_gpus)*nb, ldda[k], 
                              ib, m, &stream[k][j[k]%2]);
       cudaMemcpy2DAsync(ha+i*lda, lda*sizeof(float),
                         dB[k] + (j[k]%2) * nb * lddb, lddb*sizeof(float),
                         sizeof(float)*m, ib,
                         cudaMemcpyDeviceToHost,
                         stream[k][j[k]%2]);
       j[k]++;
    }
}

