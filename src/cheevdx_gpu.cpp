/*
   -- MAGMA (version 1.1) --
      Univ. of Tennessee, Knoxville
      Univ. of California, Berkeley
      Univ. of Colorado, Denver
      November 2011

      @author Raffaele Solca
 
      @generated c Sun Nov 13 20:48:29 2011
       
*/
#include "common_magma.h"

extern"C"{
    void Mylapackf77_cstedc(char *compz, magma_int_t *n, float *D, float *E, 
                            cuFloatComplex *Z, magma_int_t *ldz,
                            cuFloatComplex *work, magma_int_t *ldwork, 
                            float* rwork, magma_int_t *lrwork, magma_int_t *iwork, 
                            magma_int_t *liwork, magma_int_t *info)
    {
        lapackf77_cstedc(compz, n, D, E, Z, ldz, work, ldwork, rwork, lrwork, iwork, 
                         liwork, info);
    }  
}

extern"C"
void dmove_eig(char range, magma_int_t n, float *w, magma_int_t *il, 
                   magma_int_t *iu, float vl, float vu, magma_int_t *m)
{
    char range_[2] = {range, 0};
    
    static magma_int_t valeig, indeig, i;
    
    valeig = lapackf77_lsame( range_, "V" );
    indeig = lapackf77_lsame( range_, "I" );
    
    if (indeig){
      *m = *iu - *il + 1;
      if(*il > 1)
        for (i = 0; i < *m; ++i)
          w[i] = w[*il - 1 + i];
    }
    else if(valeig){
        *il=1;
        *iu=n;
        for (i = 0; i < n; ++i){
            if (w[i] > vu){
                *iu = i;
                break;
            }
            else if (w[i] < vl)
                ++il;
            else if (*il > 1)
                w[i-*il+1]=w[i];
        }
        *m = *iu - *il + 1;
    } 
    else 
        *m = n;
    
    return;
}

extern "C" magma_int_t 
magma_cheevdx_gpu(char jobz, char range, char uplo, 
                  magma_int_t n, 
                  cuFloatComplex *da, magma_int_t ldda,
                  float vl, float vu, magma_int_t il, magma_int_t iu,
                  magma_int_t *m, float *w,
                  cuFloatComplex *wa,  magma_int_t ldwa,
                  cuFloatComplex *work, magma_int_t lwork,
                  float *rwork, magma_int_t lrwork,
                  magma_int_t *iwork, magma_int_t liwork,
                  magma_int_t *info)
{
/*  -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

    Purpose   
    =======
    CHEEVD computes all eigenvalues and, optionally, eigenvectors of a   
    complex Hermitian matrix A.  If eigenvectors are desired, it uses a   
    divide and conquer algorithm.   

    The divide and conquer algorithm makes very mild assumptions about   
    floating point arithmetic. It will work on machines with a guard   
    digit in add/subtract, or on those binary machines without guard   
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or   
    Cray-2. It could conceivably fail on hexadecimal or decimal machines   
    without guard digits, but we know of none.   

    Arguments   
    =========   
    JOBZ    (input) CHARACTER*1   
            = 'N':  Compute eigenvalues only;   
            = 'V':  Compute eigenvalues and eigenvectors.
 
    RANGE   (input) CHARACTER*1   
            = 'A': all eigenvalues will be found.   
            = 'V': all eigenvalues in the half-open interval (VL,VU]   
                   will be found.   
            = 'I': the IL-th through IU-th eigenvalues will be found. 

    UPLO    (input) CHARACTER*1   
            = 'U':  Upper triangle of A is stored;   
            = 'L':  Lower triangle of A is stored.   

    N       (input) INTEGER   
            The order of the matrix A.  N >= 0.   

    DA      (device input/output) COMPLEX array, dimension (LDDA, N)   
            On entry, the Hermitian matrix A.  If UPLO = 'U', the   
            leading N-by-N upper triangular part of A contains the   
            upper triangular part of the matrix A.  If UPLO = 'L',   
            the leading N-by-N lower triangular part of A contains   
            the lower triangular part of the matrix A.   
            On exit, if JOBZ = 'V', then if INFO = 0, A contains the   
            orthonormal eigenvectors of the matrix A.   
            If JOBZ = 'N', then on exit the lower triangle (if UPLO='L')   
            or the upper triangle (if UPLO='U') of A, including the   
            diagonal, is destroyed.   

    LDDA    (input) INTEGER   
            The leading dimension of the array DA.  LDDA >= max(1,N).   

    VL      (input) DOUBLE PRECISION   
    VU      (input) DOUBLE PRECISION   
            If RANGE='V', the lower and upper bounds of the interval to   
            be searched for eigenvalues. VL < VU.   
            Not referenced if RANGE = 'A' or 'I'.   

    IL      (input) INTEGER   
    IU      (input) INTEGER   
            If RANGE='I', the indices (in ascending order) of the   
            smallest and largest eigenvalues to be returned.   
            1 <= IL <= IU <= N, if N > 0; IL = 1 and IU = 0 if N = 0.   
            Not referenced if RANGE = 'A' or 'V'. 
 
    M       (output) INTEGER   
            The total number of eigenvalues found.  0 <= M <= N.   
            If RANGE = 'A', M = N, and if RANGE = 'I', M = IU-IL+1.
 
    W       (output) DOUBLE PRECISION array, dimension (N)   
            If INFO = 0, the eigenvalues in ascending order.   

    WA      (workspace) COMPLEX array, dimension (LDWA, N)    

    LDWA    (input) INTEGER   
            The leading dimension of the array WA.  LDWA >= max(1,N). 
 
    WORK    (workspace/output) COMPLEX array, dimension (MAX(1,LWORK))   
            On exit, if INFO = 0, WORK(1) returns the optimal LWORK.   

    LWORK   (input) INTEGER   
            The length of the array WORK.   
            If N <= 1,                LWORK must be at least 1.   
            If JOBZ  = 'N' and N > 1, LWORK must be at least N + 1.   
            If JOBZ  = 'V' and N > 1, LWORK must be at least 2*N + N**2.   

            If LWORK = -1, then a workspace query is assumed; the routine   
            only calculates the optimal sizes of the WORK, RWORK and   
            IWORK arrays, returns these values as the first entries of   
            the WORK, RWORK and IWORK arrays, and no error message   
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.    

    RWORK   (workspace/output) DOUBLE PRECISION array,   
                                           dimension (LRWORK)   
            On exit, if INFO = 0, RWORK(1) returns the optimal LRWORK.   

    LRWORK  (input) INTEGER   
            The dimension of the array RWORK.   
            If N <= 1,                LRWORK must be at least 1.   
            If JOBZ  = 'N' and N > 1, LRWORK must be at least N.   
            If JOBZ  = 'V' and N > 1, LRWORK must be at least   
                           1 + 5*N + 2*N**2.   

            If LRWORK = -1, then a workspace query is assumed; the   
            routine only calculates the optimal sizes of the WORK, RWORK   
            and IWORK arrays, returns these values as the first entries   
            of the WORK, RWORK and IWORK arrays, and no error message   
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.   

    IWORK   (workspace/output) INTEGER array, dimension (MAX(1,LIWORK))   
            On exit, if INFO = 0, IWORK(1) returns the optimal LIWORK.   

    LIWORK  (input) INTEGER   
            The dimension of the array IWORK.   
            If N <= 1,                LIWORK must be at least 1.   
            If JOBZ  = 'N' and N > 1, LIWORK must be at least 1.   
            If JOBZ  = 'V' and N > 1, LIWORK must be at least 3 + 5*N.   

            If LIWORK = -1, then a workspace query is assumed; the   
            routine only calculates the optimal sizes of the WORK, RWORK   
            and IWORK arrays, returns these values as the first entries   
            of the WORK, RWORK and IWORK arrays, and no error message   
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.   

    INFO    (output) INTEGER   
            = 0:  successful exit   
            < 0:  if INFO = -i, the i-th argument had an illegal value   
            > 0:  if INFO = i and JOBZ = 'N', then the algorithm failed   
                  to converge; i off-diagonal elements of an intermediate   
                  tridiagonal form did not converge to zero;   
                  if INFO = i and JOBZ = 'V', then the algorithm failed   
                  to compute an eigenvalue while working on the submatrix   
                  lying in rows and columns INFO/(N+1) through   
                  mod(INFO,N+1).   

    Further Details   
    ===============   
    Based on contributions by   
       Jeff Rutter, Computer Science Division, University of California   
       at Berkeley, USA   

    Modified description of INFO. Sven, 16 Feb 05.   
    =====================================================================   */

    char uplo_[2] = {uplo, 0};
    char jobz_[2] = {jobz, 0};
    char range_[2] = {range, 0};
    static magma_int_t c__1 = 1;
  
    float d__1;

    static float eps;
    static magma_int_t inde;
    static float anrm;
    static magma_int_t imax;
    static float rmin, rmax;
    static float sigma;
    static magma_int_t iinfo, lwmin;
    static magma_int_t lower;
    static magma_int_t llrwk;
    static magma_int_t wantz;
    static magma_int_t indwk2, llwrk2;
    static magma_int_t iscale;
    static float safmin;
    static float bignum;
    static magma_int_t indtau;
    static magma_int_t indrwk, indwrk, liwmin;
    static magma_int_t lrwmin, llwork;
    static float smlnum;
    static magma_int_t lquery;
    static magma_int_t alleig, valeig, indeig;
  
    float *dwork;
    cuFloatComplex *dc;
    magma_int_t lddc = ldda;
  
    wantz = lapackf77_lsame(jobz_, MagmaVectorsStr);
    lower = lapackf77_lsame(uplo_, MagmaLowerStr);
    
    alleig = lapackf77_lsame( range_, "A" );
    valeig = lapackf77_lsame( range_, "V" );
    indeig = lapackf77_lsame( range_, "I" );
  
    lquery = lwork == -1 || lrwork == -1 || liwork == -1;

    *info = 0;
    if (! (wantz || lapackf77_lsame(jobz_, MagmaNoVectorsStr))) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (lower || lapackf77_lsame(uplo_, MagmaUpperStr))) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (ldda < max(1,n)) {
        *info = -6;
    } else if (ldwa < max(1,n)) {
        *info = -14;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -8;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -9;
            } else if (iu < min(n,il) || iu > n) {
                *info = -10;
            }
        }
    }
    
    magma_int_t nb = magma_get_chetrd_nb(n); 
    
    if (wantz) {
        lwmin = 2 * n + n * n;
        lrwmin = 1 + 5 * n + 2 * n * n;
        liwmin = 5 * n + 3;
    } else {
        lwmin = n * (nb + 1);
        lrwmin = n;
        liwmin = 1;
    }

    MAGMA_C_SET2REAL(work[0],(float)lwmin);
    rwork[0] = lrwmin;
    iwork[0] = liwmin;

    if ((lwork < lwmin) && !lquery) {
        *info = -16;
    } else if ((lrwork < lrwmin) && ! lquery) {
        *info = -18;
    } else if ((liwork < liwmin) && ! lquery) {
        *info = -20;
    }
  
    if (*info != 0) {
        magma_xerbla( __func__, -(*info));
        return MAGMA_ERR_ILLEGAL_VALUE;
    } else if (lquery) {
        return MAGMA_SUCCESS;
    }
    
    /* Quick return if possible */
    if (n == 0) {
        return MAGMA_SUCCESS;
    }

    if (n == 1) {
        cuFloatComplex tmp;
        cublasGetVector(1, sizeof(cuFloatComplex), da, 1, &tmp, 1);
        w[0] = MAGMA_C_REAL(tmp);
        if (wantz) {
            tmp = MAGMA_C_ONE;
            cublasSetVector(1, sizeof(cuFloatComplex), &tmp, 1, da, 1);
        }
        return MAGMA_SUCCESS;
    }

    static cudaStream_t stream;
    cudaStreamCreate(&stream);
  
    if (cudaSuccess != cudaMalloc((void**)&dc, n*lddc*sizeof(cuFloatComplex))) {
        fprintf (stderr, "!!!! device memory allocation error (magma_cheevdx_gpu)\n");
        return MAGMA_ERR_CUBLASALLOC;
    }
  
    if (cudaSuccess != cudaMalloc((void**)&dwork, n*sizeof(float))) {
        fprintf (stderr, "!!!! device memory allocation error (magma_cheevdx_gpu)\n");
        return MAGMA_ERR_CUBLASALLOC;
    }
   
    --w;
    --work;
    --rwork;
    --iwork;

    /* Get machine constants. */
    safmin = lapackf77_slamch("Safe minimum");
    eps = lapackf77_slamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_ssqrt(smlnum);
    rmax = magma_ssqrt(bignum);

    /* Scale matrix to allowable range, if necessary. */
    anrm = magmablas_clanhe('M', uplo, n, da, ldda, dwork);
    iscale = 0;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        magmablas_clascl(uplo, 0, 0, 1., sigma, n, n, da, 
                         ldda, info);
    }

    /* Call CHETRD to reduce Hermitian matrix to tridiagonal form. */
    inde = 1;
    indtau = 1;
    indwrk = indtau + n;
    indrwk = inde + n;
    indwk2 = indwrk + n * n;
    llwork = lwork - indwrk + 1;
    llwrk2 = lwork - indwk2 + 1;
    llrwk = lrwork - indrwk + 1;

#ifdef FAST_HEMV
    magma_chetrd2_gpu(uplo, n, da, ldda, &w[1], &rwork[inde],
                      &work[indtau], wa, ldwa, &work[indwrk], llwork, dc, lddc*n, &iinfo);
#else
    magma_chetrd_gpu (uplo, n, da, ldda, &w[1], &rwork[inde],
                      &work[indtau], wa, ldwa, &work[indwrk], llwork, &iinfo);
#endif  
  
    /* For eigenvalues only, call DSTERF.  For eigenvectors, first call   
       ZSTEDC to generate the eigenvector matrix, WORK(INDWRK), of the   
       tridiagonal matrix, then call CUNMTR to multiply it to the Householder 
       transformations represented as Householder vectors in A. */
  
    if (! wantz) {
        lapackf77_ssterf(&n, &w[1], &rwork[inde], info);
        
        dmove_eig(range, n, &w[1], &il, &iu, vl, vu, m);
        
    } else {
        Mylapackf77_cstedc("I", &n, &w[1], &rwork[inde], &work[indwrk], &n, &work[indwk2], 
                           &llwrk2, &rwork[indrwk], &llrwk, &iwork[1], &liwork, info);
        
        dmove_eig(range, n, &w[1], &il, &iu, vl, vu, m);
        
        cublasSetMatrix(n, *m, sizeof(cuFloatComplex), &work[indwrk + n * (il-1) ], n, dc, lddc);
        
        magma_cunmtr_gpu(MagmaLeft, uplo, MagmaNoTrans, n, *m, da, ldda, &work[indtau],
                         dc, lddc, wa, ldwa, &iinfo);
        
        cudaMemcpy2D(da, ldda * sizeof(cuFloatComplex), dc, lddc * sizeof(cuFloatComplex),
                     sizeof(cuFloatComplex) * n, *m, cudaMemcpyDeviceToDevice);
    }

    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        if (*info == 0) {
            imax = n;
        } else {
            imax = *info - 1;
        }
        d__1 = 1. / sigma;
        blasf77_sscal(&imax, &d__1, &w[1], &c__1);
    }

    work[1]  = MAGMA_C_MAKE((float) lwmin, 0.);
    rwork[1] = (float) lrwmin;
    iwork[1] = liwmin;
    
    cudaStreamDestroy(stream);
    cudaFree(dc);
    
    return MAGMA_SUCCESS;
} /* magma_cheevdx_gpu */
