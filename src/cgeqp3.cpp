/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013
  
       @generated c Fri Jun 28 19:32:26 2013

*/

#include "common_magma.h"
#include <cblas.h>

#define PRECISION_c

extern "C" magma_int_t
magma_cgeqp3( magma_int_t m, magma_int_t n,
              magmaFloatComplex *A, magma_int_t lda,
              magma_int_t *jpvt, magmaFloatComplex *tau,
              magmaFloatComplex *work, magma_int_t lwork,
#if defined(PRECISION_z) || defined(PRECISION_c)
              float *rwork,
#endif
              magma_int_t *info )
{
/*  -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

    Purpose
    =======
    CGEQP3 computes a QR factorization with column pivoting of a
    matrix A:  A*P = Q*R  using Level 3 BLAS.

    Arguments
    =========
    M       (input) INTEGER
            The number of rows of the matrix A. M >= 0.

    N       (input) INTEGER
            The number of columns of the matrix A.  N >= 0.

    A       (input/output) COMPLEX array, dimension (LDA,N)
            On entry, the M-by-N matrix A.
            On exit, the upper triangle of the array contains the
            min(M,N)-by-N upper trapezoidal matrix R; the elements below
            the diagonal, together with the array TAU, represent the
            unitary matrix Q as a product of min(M,N) elementary
            reflectors.

    LDA     (input) INTEGER
            The leading dimension of the array A. LDA >= max(1,M).

    JPVT    (input/output) INTEGER array, dimension (N)
            On entry, if JPVT(J).ne.0, the J-th column of A is permuted
            to the front of A*P (a leading column); if JPVT(J)=0,
            the J-th column of A is a free column.
            On exit, if JPVT(J)=K, then the J-th column of A*P was the
            the K-th column of A.

    TAU     (output) COMPLEX array, dimension (min(M,N))
            The scalar factors of the elementary reflectors.

    WORK    (workspace/output) COMPLEX array, dimension (MAX(1,LWORK))
            On exit, if INFO=0, WORK(1) returns the optimal LWORK.

    LWORK   (input) INTEGER
            The dimension of the array WORK.
            For [sd]geqp3, LWORK >= (N+1)*NB + 2*N;
            for [cz]geqp3, LWORK >= (N+1)*NB,
            where NB is the optimal blocksize.

            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    For [cz]geqp3 only:
    RWORK   (workspace) DOUBLE PRECISION array, dimension (2*N)

    INFO    (output) INTEGER
            = 0: successful exit.
            < 0: if INFO = -i, the i-th argument had an illegal value.

    Further Details
    ===============
    The matrix Q is represented as a product of elementary reflectors

      Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

      H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector
    with v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in
    A(i+1:m,i), and tau in TAU(i).
    =====================================================================   */

#define  A(i, j) (A     + (i) + (j)*(lda ))
#define dA(i, j) (dwork + (i) + (j)*(ldda))

    magmaFloatComplex   *dwork, *df;

    magma_int_t ione = 1;

    magma_int_t n_j, ldda, ldwork;
    magma_int_t j, jb, na, nb, sm, sn, fjb, nfxd, minmn;
    magma_int_t topbmn, sminmn, lwkopt, lquery;
    
    *info = 0;
    lquery = (lwork == -1);
    if (m < 0) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (lda < max(1,m)) {
        *info = -4;
    }
    
    if (*info == 0) {
        minmn = min(m,n);
        if (minmn == 0) {
            lwkopt = 1;
        } else {
            nb = magma_get_cgeqp3_nb(min(m, n));
            lwkopt = (n + 1)*nb;
#if defined(PRECISION_d) || defined(PRECISION_s)
            lwkopt += 2*n;
#endif
        }
        work[0] = MAGMA_C_MAKE( lwkopt, 0. );

        if (lwork < lwkopt && ! lquery) {
            *info = -8;
        }
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    } else if (lquery) {
        return *info;
    }

    if (minmn == 0)
        return *info;

#if defined(PRECISION_d) || defined(PRECISION_s)
    float *rwork = work + (n + 1)*nb;
#endif

    ldda = ((m+31)/32)*32;
    ldwork = n*ldda + (n+1)*nb;
    if (MAGMA_SUCCESS != magma_cmalloc( &dwork, ldwork )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    df = dwork + n*ldda;
    // dwork used for dA

    magma_queue_t stream;
    magma_queue_create( &stream );

    /* Move initial columns up front.
     * Note jpvt uses 1-based indices for historical compatibility. */
    nfxd = 0;
    for (j = 0; j < n; ++j) {
        if (jpvt[j] != 0) {
            if (j != nfxd) {
                blasf77_cswap(&m, A(0, j), &ione, A(0, nfxd), &ione);
                jpvt[j]    = jpvt[nfxd];
                jpvt[nfxd] = j + 1;
            }
            else {
                jpvt[j] = j + 1;
            }
            ++nfxd;
        }
        else {
            jpvt[j] = j + 1;
        }
    }

    /*     Factorize fixed columns
           =======================
           Compute the QR factorization of fixed columns and update
           remaining columns. */
    if (nfxd > 0) {
        na = min(m,nfxd);
        lapackf77_cgeqrf(&m, &na, A, &lda, tau, work, &lwork, info);
        if (na < n) {
            n_j = n - na;
            lapackf77_cunmqr( MagmaLeftStr, MagmaConjTransStr, &m, &n_j, &na,
                              A, &lda, tau, A(0, na), &lda,
                              work, &lwork, info );
        }
    }
    
    /*  Factorize free columns */
    if (nfxd < minmn) {
        sm = m - nfxd;
        sn = n - nfxd;
        sminmn = minmn - nfxd;
        
        if (nb < sminmn) {
            j = nfxd;
            
            // Set the original matrix to the GPU
            magma_csetmatrix_async( m, sn,
                                    A (0,j), lda,
                                    dA(0,j), ldda, stream );
        }

        /* Initialize partial column norms. */
        for (j = nfxd; j < n; ++j) {
            rwork[j] = cblas_scnrm2(sm, A(nfxd, j), ione);
            rwork[n + j] = rwork[j];
        }
        
        j = nfxd;
        if (nb < sminmn) {
            /* Use blocked code initially. */
            magma_queue_sync( stream );
            
            /* Compute factorization: while loop. */
            topbmn = minmn - nb;
            while(j < topbmn) {
                jb = min(nb, topbmn - j);
                
                /* Factorize JB columns among columns J:N. */
                n_j = n - j;
                
                if (j>nfxd) {
                    // Get panel to the CPU
                    magma_cgetmatrix( m-j, jb,
                                      dA(j,j), ldda,
                                      A (j,j), lda );
                    
                    // Get the rows
                    magma_cgetmatrix( jb, n_j - jb,
                                      dA(j,j + jb), ldda,
                                      A (j,j + jb), lda );
                }

                magma_claqps( m, n_j, j, jb, &fjb,
                              A (0, j), lda,
                              dA(0, j), ldda,
                              &jpvt[j], &tau[j], &rwork[j], &rwork[n + j],
                              work,
                              &work[jb], n_j,
                              &df[jb],   n_j );
                
                j += fjb;  /* fjb is actual number of columns factored */
            }
        }
        
        /* Use unblocked code to factor the last or only block. */
        if (j < minmn) {
            n_j = n - j;
            if (j > nfxd) {
                magma_cgetmatrix( m-j, n_j,
                                  dA(j,j), ldda,
                                  A (j,j), lda );
            }
            lapackf77_claqp2(&m, &n_j, &j, A(0, j), &lda, &jpvt[j],
                             &tau[j], &rwork[j], &rwork[n+j], work );
        }
    }

    work[0] = MAGMA_C_MAKE( lwkopt, 0. );
    magma_free( dwork );

    magma_queue_destroy( stream );

    return *info;
} /* cgeqp3 */
