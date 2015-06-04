/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @author Stan Tomov
       @author Raffaele Solca

       @generated s Sun Nov 13 20:48:25 2011

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_s

#if (defined(PRECISION_s))
//  #define cublasSsyr2k magmablas_ssyr2k
#endif
// === End defining what BLAS to use ======================================

#define  A(i, j) ( a+(j)*lda  + (i))
#define dA(i, j) (da+(j)*ldda + (i))

extern "C" magma_int_t
magma_ssytrd(char uplo, magma_int_t n, 
             float *a, magma_int_t lda, 
             float *d, float *e, float *tau,
             float *work, magma_int_t lwork, 
             magma_int_t *info)
{
/*  -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

    Purpose   
    =======   
    SSYTRD reduces a real symmetric matrix A to real symmetric   
    tridiagonal form T by an orthogonal similarity transformation:   
    Q\*\*H * A * Q = T.   

    Arguments   
    =========   
    UPLO    (input) CHARACTER*1   
            = 'U':  Upper triangle of A is stored;   
            = 'L':  Lower triangle of A is stored.   

    N       (input) INTEGER   
            The order of the matrix A.  N >= 0.   

    A       (input/output) REAL array, dimension (LDA,N)   
            On entry, the symmetric matrix A.  If UPLO = 'U', the leading   
            N-by-N upper triangular part of A contains the upper   
            triangular part of the matrix A, and the strictly lower   
            triangular part of A is not referenced.  If UPLO = 'L', the   
            leading N-by-N lower triangular part of A contains the lower   
            triangular part of the matrix A, and the strictly upper   
            triangular part of A is not referenced.   
            On exit, if UPLO = 'U', the diagonal and first superdiagonal   
            of A are overwritten by the corresponding elements of the   
            tridiagonal matrix T, and the elements above the first   
            superdiagonal, with the array TAU, represent the orthogonal   
            matrix Q as a product of elementary reflectors; if UPLO   
            = 'L', the diagonal and first subdiagonal of A are over-   
            written by the corresponding elements of the tridiagonal   
            matrix T, and the elements below the first subdiagonal, with   
            the array TAU, represent the orthogonal matrix Q as a product   
            of elementary reflectors. See Further Details.   

    LDA     (input) INTEGER   
            The leading dimension of the array A.  LDA >= max(1,N).   

    D       (output) REAL array, dimension (N)   
            The diagonal elements of the tridiagonal matrix T:   
            D(i) = A(i,i).   

    E       (output) REAL array, dimension (N-1)   
            The off-diagonal elements of the tridiagonal matrix T:   
            E(i) = A(i,i+1) if UPLO = 'U', E(i) = A(i+1,i) if UPLO = 'L'.   

    TAU     (output) REAL array, dimension (N-1)   
            The scalar factors of the elementary reflectors (see Further   
            Details).   

    WORK    (workspace/output) REAL array, dimension (MAX(1,LWORK))   
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.   

    LWORK   (input) INTEGER   
            The dimension of the array WORK.  LWORK >= 1.   
            For optimum performance LWORK >= N*NB, where NB is the   
            optimal blocksize.   

            If LWORK = -1, then a workspace query is assumed; the routine   
            only calculates the optimal size of the WORK array, returns   
            this value as the first entry of the WORK array, and no error   
            message related to LWORK is issued by XERBLA.   

    INFO    (output) INTEGER   
            = 0:  successful exit   
            < 0:  if INFO = -i, the i-th argument had an illegal value   

    Further Details   
    ===============   
    If UPLO = 'U', the matrix Q is represented as a product of elementary   
    reflectors   

       Q = H(n-1) . . . H(2) H(1).   

    Each H(i) has the form   

       H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with   
    v(i+1:n) = 0 and v(i) = 1; v(1:i-1) is stored on exit in   
    A(1:i-1,i+1), and tau in TAU(i).   

    If UPLO = 'L', the matrix Q is represented as a product of elementary   
    reflectors   

       Q = H(1) H(2) . . . H(n-1).   

    Each H(i) has the form   

       H(i) = I - tau * v * v'   

    where tau is a real scalar, and v is a real vector with   
    v(1:i) = 0 and v(i+1) = 1; v(i+2:n) is stored on exit in A(i+2:n,i),   
    and tau in TAU(i).

    The contents of A on exit are illustrated by the following examples   
    with n = 5:   

    if UPLO = 'U':                       if UPLO = 'L':   

      (  d   e   v2  v3  v4 )              (  d                  )   
      (      d   e   v3  v4 )              (  e   d              )   
      (          d   e   v4 )              (  v1  e   d          )   
      (              d   e  )              (  v1  v2  e   d      )   
      (                  d  )              (  v1  v2  v3  e   d  )   

    where d and e denote diagonal and off-diagonal elements of T, and vi   
    denotes an element of the vector defining H(i).   
    =====================================================================    */  

    char uplo_[2] = {uplo, 0};

    magma_int_t ldda = lda;
    magma_int_t nb = magma_get_ssytrd_nb(n); 

    float z_neg_one = MAGMA_S_NEG_ONE;
    float z_one = MAGMA_S_ONE;
    float  d_one = MAGMA_D_ONE;
    
    static magma_int_t kk, nx;
    static magma_int_t i, j, i_n;
    static magma_int_t iinfo;
    static magma_int_t ldwork, lddwork, lwkopt;
    static magma_int_t lquery;

    *info = 0;
    long int upper = lapackf77_lsame(uplo_, "U");
    lquery = lwork == -1;
    if (! upper && ! lapackf77_lsame(uplo_, "L")) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (lda < max(1,n)) {
        *info = -4;
    } else if (lwork < nb*n && ! lquery) {
        *info = -9;
    }

    if (*info == 0) {
      /* Determine the block size. */
      ldwork = lddwork = n;
      lwkopt = n * nb;
      MAGMA_S_SET2REAL( work[0], lwkopt );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return MAGMA_ERR_ILLEGAL_VALUE;
    }
    else if (lquery)
      return 0;

    /* Quick return if possible */
    if (n == 0) {
        work[0] = z_one;
        return 0;
    }

    float *da;
    cublasStatus status;
    status = cublasAlloc(n*ldda+2*n*nb, sizeof(float), (void**)&da);
    if (status != CUBLAS_STATUS_SUCCESS) {
      fprintf (stderr, "!!!! device memory allocation error (magma_ssytrd)\n");
      return 0;
    }

    float *dwork = da + (n)*ldda;

    if (n < 2048)
      nx = n;
    else
      nx = 512;

    if (upper) {

        /* Copy the matrix to the GPU */ 
        cublasSetMatrix(n, n, sizeof(float), A(0, 0), lda, dA(0, 0), ldda);

        /*  Reduce the upper triangle of A.   
            Columns 1:kk are handled by the unblocked method. */
        kk = n - (n - nx + nb - 1) / nb * nb;

        for (i = n - nb; i >= kk; i -= nb) 
          {
            /* Reduce columns i:i+nb-1 to tridiagonal form and form the   
               matrix W which is needed to update the unreduced part of   
               the matrix */
            
            /*   Get the current panel (no need for the 1st iteration) */
            if (i!=n-nb)
              cublasGetMatrix(i+nb, nb, sizeof(float), 
                              dA(0, i), ldda, A(0, i), lda);
            
            magma_slatrd(uplo, i+nb, nb, A(0, 0), lda, e, tau, 
                         work, ldwork, dA(0, 0), ldda, dwork, lddwork);

            /* Update the unreduced submatrix A(0:i-2,0:i-2), using an   
               update of the form:  A := A - V*W' - W*V' */
            cublasSetMatrix(i + nb, nb, sizeof(float),
                            work, ldwork,
                            dwork, lddwork);

            cublasSsyr2k(uplo, MagmaNoTrans, i, nb, z_neg_one, 
                         dA(0, i), ldda, dwork, 
                         lddwork, d_one, dA(0, 0), ldda);
            
            /* Copy superdiagonal elements back into A, and diagonal   
               elements into D */
            for (j = i; j < i+nb; ++j) {
                MAGMA_S_SET2REAL( *A(j-1, j), e[j - 1] );
                d[j] = MAGMA_S_GET_X( *A(j, j) );
            }

          }
      
        cublasGetMatrix(kk, kk, sizeof(float), dA(0, 0), ldda,
                        A(0, 0), lda);
      
        /*  Use unblocked code to reduce the last or only block */
        lapackf77_ssytd2(uplo_, &kk, A(0, 0), &lda, d, e, tau, &iinfo);
    } 
    else 
      {
        /* Copy the matrix to the GPU */
        if (1<=n-nx)
          cublasSetMatrix(n, n, sizeof(float), A(0,0), lda, dA(0,0), ldda);

        /* Reduce the lower triangle of A */
        for (i = 0; i < n-nx; i += nb) 
          {
            /* Reduce columns i:i+nb-1 to tridiagonal form and form the
               matrix W which is needed to update the unreduced part of
               the matrix */

            /*   Get the current panel (no need for the 1st iteration) */
            if (i!=0)
              cublasGetMatrix(n-i, nb, sizeof(float),
                              dA(i, i), ldda,
                              A(i, i), lda);
            
            magma_slatrd(uplo, n-i, nb, A(i, i), lda, &e[i], 
                         &tau[i], work, ldwork, 
                         dA(i, i), ldda,
                         dwork, lddwork);
            
            /* Update the unreduced submatrix A(i+ib:n,i+ib:n), using   
               an update of the form:  A := A - V*W' - W*V' */
            cublasSetMatrix(n-i, nb, sizeof(float),
                            work, ldwork,
                            dwork, lddwork);

            cublasSsyr2k('L', 'N', n-i-nb, nb, z_neg_one, 
                         dA(i+nb, i), ldda, 
                         &dwork[nb], lddwork, d_one, 
                         dA(i+nb, i+nb), ldda);
            
            /* Copy subdiagonal elements back into A, and diagonal   
               elements into D */
            for (j = i; j < i+nb; ++j) {
                MAGMA_S_SET2REAL( *A(j+1, j), e[j] );
                d[j] = MAGMA_S_GET_X( *A(j, j) );
            }
          }

        /* Use unblocked code to reduce the last or only block */
        if (1<=n-nx)
          cublasGetMatrix(n-i, n-i, sizeof(float),
                          dA(i, i), ldda,
                          A(i, i), lda);
        i_n = n-i;
        lapackf77_ssytrd(uplo_, &i_n, A(i, i), &lda, &d[i], &e[i],
                         &tau[i], work, &lwork, &iinfo);
        
      }
    
    cublasFree(da);
    MAGMA_S_SET2REAL( work[0], lwkopt );

    return MAGMA_SUCCESS;
} /* magma_ssytrd */
