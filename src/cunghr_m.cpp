/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated c Fri Jun 28 19:32:49 2013

*/
#include "common_magma.h"

#define PRECISION_c

extern "C" magma_int_t
magma_cunghr_m( magma_int_t n, magma_int_t ilo, magma_int_t ihi,
                magmaFloatComplex *A, magma_int_t lda,
                magmaFloatComplex *tau,
                magmaFloatComplex *T, magma_int_t nb,
                magma_int_t *info)
{
/*  -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    Purpose
    =======
    CUNGHR generates a COMPLEX unitary matrix Q which is defined as the
    product of IHI-ILO elementary reflectors of order N, as returned by
    CGEHRD:

    Q = H(ilo) H(ilo+1) . . . H(ihi-1).

    Arguments
    =========
    N       (input) INTEGER
            The order of the matrix Q. N >= 0.

    ILO     (input) INTEGER
    IHI     (input) INTEGER
            ILO and IHI must have the same values as in the previous call
            of CGEHRD. Q is equal to the unit matrix except in the
            submatrix Q(ilo+1:ihi,ilo+1:ihi).
            1 <= ILO <= IHI <= N, if N > 0; ILO=1 and IHI=0, if N=0.

    A       (input/output) COMPLEX array, dimension (LDA,N)
            On entry, the vectors which define the elementary reflectors,
            as returned by CGEHRD.
            On exit, the N-by-N unitary matrix Q.

    LDA     (input) INTEGER
            The leading dimension of the array A. LDA >= max(1,N).

    TAU     (input) COMPLEX array, dimension (N-1)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by CGEHRD.

    T       (input) COMPLEX array on the GPU device.
            T contains the T matrices used in blocking the elementary
            reflectors H(i), e.g., this can be the 9th argument of
            magma_cgehrd.

    NB      (input) INTEGER
            This is the block size used in CGEHRD, and correspondingly
            the size of the T matrices, used in the factorization, and
            stored in T.

    INFO    (output) INTEGER
            = 0:  successful exit
            < 0:  if INFO = -i, the i-th argument had an illegal value
    ===================================================================== */

    #define A(i,j) (A + (i) + (j)*lda)

    magma_int_t i, j, nh, iinfo;

    *info = 0;
    nh = ihi - ilo;
    if (n < 0)
        *info = -1;
    else if (ilo < 1 || ilo > max(1,n))
        *info = -2;
    else if (ihi < min(ilo,n) || ihi > n)
        *info = -3;
    else if (lda < max(1,n))
        *info = -5;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (n == 0)
        return *info;

    /* Shift the vectors which define the elementary reflectors one
       column to the right, and set the first ilo and the last n-ihi
       rows and columns to those of the unit matrix */
    for (j = ihi-1; j >= ilo; --j) {
        for (i = 0; i < j; ++i)
            *A(i, j) = MAGMA_C_ZERO;
        
        for (i = j+1; i < ihi; ++i)
            *A(i, j) = *A(i, j - 1);
        
        for (i = ihi; i < n; ++i)
            *A(i, j) = MAGMA_C_ZERO;
    }
    for (j = 0; j < ilo; ++j) {
        for (i = 0; i < n; ++i)
            *A(i, j) = MAGMA_C_ZERO;
        
        *A(j, j) = MAGMA_C_ONE;
    }
    for (j = ihi; j < n; ++j) {
        for (i = 0; i < n; ++i)
            *A(i, j) = MAGMA_C_ZERO;
        
        *A(j, j) = MAGMA_C_ONE;
    }

    if (nh > 0) {
        /* Generate Q(ilo+1:ihi,ilo+1:ihi) */
        magma_cungqr_m( nh, nh, nh,
                        A(ilo, ilo), lda,
                        tau+ilo-1, T, nb, &iinfo );
    }
    
    return *info;
} /* magma_cunghr */

#undef A
