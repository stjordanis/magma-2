/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated s Fri Jun 28 19:32:10 2013

*/
#include "common_magma.h"


extern "C" magma_int_t
magma_sgetrf_nopiv(magma_int_t *m, magma_int_t *n, float *a,
                   magma_int_t *lda, magma_int_t *info);

extern "C" magma_int_t
magma_sgetrf_nopiv_gpu(magma_int_t m, magma_int_t n,
                       float *dA, magma_int_t ldda,
                       magma_int_t *info)
{
/*  -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    Purpose
    =======
    SGETRF_NOPIV_GPU computes an LU factorization of a general M-by-N
    matrix A without any pivoting.

    The factorization has the form
       A = P * L * U
    where P is a permutation matrix, L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.

    Arguments
    =========
    M       (input) INTEGER
            The number of rows of the matrix A.  M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    A       (input/output) REAL array on the GPU, dimension (LDDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    LDDA     (input) INTEGER
            The leading dimension of the array A.  LDDA >= max(1,M).

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.
            > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.
    =====================================================================    */

#define inA(i,j) (dA + (i)*nb + (j)*nb*ldda)

    float c_one     = MAGMA_S_ONE;
    float c_neg_one = MAGMA_S_NEG_ONE;

    magma_int_t iinfo, nb;
    magma_int_t maxm, maxn, mindim;
    magma_int_t i, rows, cols, s, lddwork;
    float *work;

    /* Check arguments */
    *info = 0;
    if (m < 0)
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (ldda < max(1,m))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        return *info;

    /* Function Body */
    mindim = min(m, n);
    nb     = 2*magma_get_sgetrf_nb(m);
    s      = mindim / nb;

    if (nb <= 1 || nb >= min(m,n)) {
        /* Use CPU code. */
        magma_smalloc_cpu( &work, m * n );
        if ( work == NULL ) {
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        magma_sgetmatrix( m, n, dA, ldda, work, m );
        magma_sgetrf_nopiv(&m, &n, work, &m, info);
        magma_ssetmatrix( m, n, work, m, dA, ldda );
        magma_free_cpu(work);
    }
    else {
        /* Use hybrid blocked code. */
        maxm = ((m + 31)/32)*32;
        maxn = ((n + 31)/32)*32;

        lddwork = maxm;

        if (MAGMA_SUCCESS != magma_smalloc_pinned( &work, maxm*nb )) {
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }

        for( i=0; i<s; i++ ) {
            // download i-th panel
            cols = maxm - i*nb;
            magma_sgetmatrix( m-i*nb, nb, inA(i,i), ldda, work, lddwork );
            
            // make sure that gpu queue is empty
            magma_device_sync();
            
            if ( i>0 ){
                magma_strsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,
                             nb, n - (i+1)*nb,
                             c_one, inA(i-1,i-1), ldda,
                             inA(i-1,i+1), ldda );
                magma_sgemm( MagmaNoTrans, MagmaNoTrans,
                             m-i*nb, n-(i+1)*nb, nb,
                             c_neg_one, inA(i,  i-1), ldda, inA(i-1,i+1), ldda,
                             c_one,     inA(i,  i+1), ldda );
            }

            // do the cpu part
            rows = m - i*nb;
            magma_sgetrf_nopiv(&rows, &nb, work, &lddwork, &iinfo);
            if ( (*info == 0) && (iinfo > 0) )
                *info = iinfo + i*nb;

            // upload i-th panel
            magma_ssetmatrix( m-i*nb, nb, work, lddwork, inA(i, i), ldda );
            
            // do the small non-parallel computations
            if ( s > (i+1) ) {
                magma_strsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,
                             nb, nb,
                             c_one, inA(i, i  ), ldda,
                             inA(i, i+1), ldda);
                magma_sgemm( MagmaNoTrans, MagmaNoTrans,
                             m-(i+1)*nb, nb, nb,
                             c_neg_one, inA(i+1, i  ), ldda, inA(i,   i+1), ldda,
                             c_one,     inA(i+1, i+1), ldda );
            }
            else {
                magma_strsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,
                             nb, n-s*nb,
                             c_one, inA(i, i  ), ldda,
                             inA(i, i+1), ldda);
                magma_sgemm( MagmaNoTrans, MagmaNoTrans,
                             m-(i+1)*nb, n-(i+1)*nb, nb,
                             c_neg_one, inA(i+1, i  ), ldda, inA(i,   i+1), ldda,
                             c_one,     inA(i+1, i+1), ldda );
            }
        }

        magma_int_t nb0 = min(m - s*nb, n - s*nb);
        rows = m - s*nb;
        cols = maxm - s*nb;
        magma_sgetmatrix( rows, nb0, inA(s,s), ldda, work, lddwork );

        // make sure that gpu queue is empty
        magma_device_sync();

        // do the cpu part
        magma_sgetrf_nopiv( &rows, &nb0, work, &lddwork, &iinfo);
        if ( (*info == 0) && (iinfo > 0) )
            *info = iinfo + s*nb;

        // upload i-th panel
        magma_ssetmatrix( rows, nb0, work, lddwork, inA(s,s), ldda );

        magma_strsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,
                     nb0, n-s*nb-nb0,
                     c_one, inA(s,s),     ldda,
                            inA(s,s)+nb0, ldda);

        magma_free_pinned( work );
    }

    return *info;
} /* magma_sgetrf_nopiv_gpu */

#undef inA
