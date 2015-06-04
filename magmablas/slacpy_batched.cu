/*
    -- MAGMA (version 1.4.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       December 2013

       @generated s Tue Dec 17 13:18:45 2013
       @author Mark Gates
*/
#include "common_magma.h"
#include <assert.h>

#define NB 64

/* =====================================================================
    Batches slacpy of multiple arrays;
    y-dimension of grid is different arrays,
    x-dimension of grid is blocks for each array.
    Matrix is m x n, and is divided into block rows, each NB x n.
    Each CUDA block has NB threads to handle one block row.
    Each thread copies one row, iterating across all columns.
    The bottom block of rows may be partially outside the matrix;
    if so, rows outside the matrix (i >= m) are disabled.
*/
__global__ void
slacpy_batched_kernel(
    int m, int n,
    const float * const *dAarray, int ldda,
    float              **dBarray, int lddb )
{
    // dA and dB iterate across row i
    const float *dA = dAarray[ blockIdx.y ];
    float       *dB = dBarray[ blockIdx.y ];
    int i = blockIdx.x*blockDim.x + threadIdx.x;
    if ( i < m ) {
        dA += i;
        dB += i;
        const float *dAend = dA + n*ldda;
        while( dA < dAend ) {
            *dB = *dA;
            dA += ldda;
            dB += lddb;
        }
    }
}


/* ===================================================================== */
extern "C" void
magmablas_slacpy_batched(
    char uplo, magma_int_t m, magma_int_t n,
    const float * const *dAarray, magma_int_t ldda,
    float              **dBarray, magma_int_t lddb,
    magma_int_t batchCount )
{
/*
      Note
    ========
    - UPLO Parameter is disabled
    - Do we want to provide a generic function to the user with all the options?
    
    Purpose
    =======
    SLACPY copies all or part of a set of two-dimensional matrices dAarray[i]
    to another set of matrices dBarray[i], for i = 0, ..., batchCount-1.
    
    Arguments
    =========
    
    UPLO    (input) CHARACTER*1
            Specifies the part of each matrix dAarray[i] to be copied to dBarray[i].
            = 'U':      Upper triangular part
            = 'L':      Lower triangular part
            Otherwise:  All of each matrix dAarray[i]
    
    M       (input) INTEGER
            The number of rows of each matrix dAarray[i].  M >= 0.
    
    N       (input) INTEGER
            The number of columns of each matrix dAarray[i].  N >= 0.
    
    dAarray (input) array on GPU, dimension(batchCount), of pointers to arrays,
            with each array a COMPLEX REAL array, dimension (LDDA,N)
            The m by n matrices dAarray[i].
            If UPLO = 'U', only the upper triangle or trapezoid is accessed;
            if UPLO = 'L', only the lower triangle or trapezoid is accessed.
    
    LDDA    (input) INTEGER
            The leading dimension of each array dAarray[i].  LDDA >= max(1,M).
    
    dBarray (output) array on GPU, dimension(batchCount), of pointers to arrays,
            with each array a COMPLEX REAL array, dimension (LDDB,N)
            The m by n matrices dBarray[i].
            On exit, matrix dBarray[i] = matrix dAarray[i] in the locations
            specified by UPLO.
    
    LDDB    (input) INTEGER
            The leading dimension of each array dBarray[i].  LDDB >= max(1,M).
    
    batchCount (input) INTEGER
            The number of matrices to add; length of dAarray and dBarray.
            batchCount >= 0.
    
    =====================================================================   */

    magma_int_t info = 0;
    if ( m < 0 )
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( ldda < max(1,m))
        info = -5;
    else if ( lddb < max(1,m))
        info = -7;
    else if ( batchCount < 0 )
        info = -8;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    if ( m == 0 || n == 0 || batchCount == 0 )
        return;
    
    dim3 threads( NB );
    dim3 grid( (m + NB - 1)/NB, batchCount );
    
    if ( (uplo == 'U') || (uplo == 'u') ) {
        fprintf(stderr, "lacpy upper is not implemented\n");
    }
    else if ( (uplo == 'L') || (uplo == 'l') ) {
        fprintf(stderr, "lacpy lower is not implemented\n");
    }
    else {
        slacpy_batched_kernel<<< grid, threads, 0, magma_stream >>>(
            m, n, dAarray, ldda, dBarray, lddb );
    }
}
