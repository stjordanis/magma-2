/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated d Fri Jun 28 19:33:16 2013

*/
#include "common_magma.h"
#define PRECISION_d
#include "commonblas.h"


//
//      m, n - dimensions in the source (input) matrix.
//             This routine copies the ha matrix from the CPU
//             to dat on the GPU. In addition, the output matrix
//             is transposed. The routine uses a buffer of size
//             2*lddb*nb pointed to by dB (lddb > m) on the GPU. 
//             Note that lda >= m and lddat >= n.
//
extern "C" void 
magmablas_dsetmatrix_transpose( magma_int_t m, magma_int_t n,
                                const double  *ha, magma_int_t lda, 
                                double       *dat, magma_int_t ldda,
                                double        *dB, magma_int_t lddb, magma_int_t nb )
{
    magma_int_t i = 0, j = 0, ib;

    /* Quick return */
    if ( (m == 0) || (n == 0) )
        return;

    if (lda < m || ldda < n || lddb < m){
        printf("Wrong arguments in %s.\n", __func__);
        return;
    }

    magma_queue_t stream[2];
    magma_queue_create( &stream[0] );
    magma_queue_create( &stream[1] );
   
    /* Move data from CPU to GPU in the first panel in the dB buffer */
    ib   = min(n-i, nb);
    magma_dsetmatrix_async( m, ib,
                            ha + i*lda,             lda,
                            dB + (j%2) * nb * lddb, lddb, stream[j%2] );
    j++;

    for(i=nb; i<n; i+=nb){
       /* Move data from CPU to GPU in the second panel in the dB buffer */
       ib   = min(n-i, nb);
       magma_dsetmatrix_async( m, ib,
                               ha+i*lda,               lda,
                               dB + (j%2) * nb * lddb, lddb, stream[j%2] );
       j++;
  
       /* Note that the previous panel (i.e., j%2) comes through the stream
          for the kernel so there is no need to synchronize.             */
       // magmablas_dtranspose2( dat+i-nb, ldda, dB + (j%2)*nb*lddb, lddb, m, nb);
       magmablas_dtranspose2s( dat+i-nb, ldda, dB + (j%2)*nb*lddb, lddb, m, nb, stream[j%2]);
    }

    /* Transpose the last part of the matrix.                            */
    j++;
    // magmablas_dtranspose2( dat+i-nb, ldda, dB + (j%2)*nb*lddb, lddb, m, ib);
    magmablas_dtranspose2s( dat+i-nb, ldda, dB + (j%2)*nb*lddb, lddb, m, ib, stream[j%2]);

    magma_queue_destroy( stream[0] );
    magma_queue_destroy( stream[1] );
}
