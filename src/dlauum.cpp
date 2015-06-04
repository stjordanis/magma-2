/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated d Sun Nov 13 20:48:32 2011

*/
#include "common_magma.h"

// === Define what BLAS to use ============================================
#define PRECISION_d
#if (defined(PRECISION_s) || defined(PRECISION_d))
        #define cublasDgemm magmablas_dgemm
        #define cublasDtrsm magmablas_dtrsm
#endif

#if (GPUSHMEM >= 200)
        #if (defined(PRECISION_s))
                #undef  cublasSgemm
                #define cublasSgemm magmablas_sgemm_fermi80
        #endif
#endif
// === End defining what BLAS to use ======================================

#define A(i, j)  (a   +(j)*lda  + (i))
#define dA(i, j) (work+(j)*ldda + (i))

 
extern "C" magma_int_t
magma_dlauum(char uplo, magma_int_t n,
             double *a, magma_int_t lda, magma_int_t *info)
{
/*  -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

        Purpose
        =======

        ZLAUUM computes the product U * U' or L' * L, where the triangular
        factor U or L is stored in the upper or lower triangular part of
        the array A.

        If UPLO = 'U' or 'u' then the upper triangle of the result is stored,
        overwriting the factor U in A.
        If UPLO = 'L' or 'l' then the lower triangle of the result is stored,
        overwriting the factor L in A.
        This is the blocked form of the algorithm, calling Level 3 BLAS.

        Arguments
        =========

        UPLO    (input) CHARACTER*1
                        Specifies whether the triangular factor stored in the array A
                        is upper or lower triangular:
                        = 'U':  Upper triangular
                        = 'L':  Lower triangular

        N       (input) INTEGER
                        The order of the triangular factor U or L.  N >= 0.

        A       (input/output) COPLEX_16 array, dimension (LDA,N)
                        On entry, the triangular factor U or L.
                        On exit, if UPLO = 'U', the upper triangle of A is
                        overwritten with the upper triangle of the product U * U';
                        if UPLO = 'L', the lower triangle of A is overwritten with
                        the lower triangle of the product L' * L.

        LDA     (input) INTEGER
                        The leading dimension of the array A.  LDA >= max(1,N).

        INFO    (output) INTEGER
                        = 0: successful exit
                        < 0: if INFO = -k, the k-th argument had an illegal value

        ===================================================================== */


        /* Local variables */
        char uplo_[2] = {uplo, 0};
        magma_int_t     ldda, nb;
        static magma_int_t i, ib;
        double    zone  = MAGMA_D_ONE;
        double             done  = MAGMA_D_ONE;
        double    *work;
        long int           upper = lapackf77_lsame(uplo_, "U");

        *info = 0;
        if ((! upper) && (! lapackf77_lsame(uplo_, "L")))
                *info = -1;
        else if (n < 0)
                *info = -2;
        else if (lda < max(1,n))
                *info = -4;

        if (*info != 0) {
                magma_xerbla( __func__, -(*info) );
                return MAGMA_ERR_ILLEGAL_VALUE;
        }

        /* Quick return */
        if ( n == 0 )
                return MAGMA_SUCCESS;

        ldda = ((n+31)/32)*32;

        if (CUBLAS_STATUS_SUCCESS != cublasAlloc((n)*ldda, sizeof(double), (void**)&work))
        {
                *info = -6;
                return MAGMA_ERR_CUBLASALLOC;
        }

        static cudaStream_t stream[2];
        cudaStreamCreate(&stream[0]);
        cudaStreamCreate(&stream[1]);

        nb = magma_get_dpotrf_nb(n);

        if (nb <= 1 || nb >= n)
                lapackf77_dlauum(uplo_, &n, a, &lda, info);
        else
        {
                if (upper)
                {
                        /* Compute the product U * U'. */
                        for (i=0; i<n; i=i+nb)
                        {
                                ib=min(nb,n-i);

                                //cublasSetMatrix(ib, (n-i), sizeof(double), A(i, i), lda, dA(i, i), ldda);
                                
                                cudaMemcpy2DAsync( dA(i, i), ldda *sizeof(double),
                                                   A(i,i), lda*sizeof(double),
                                                   sizeof(double)*ib, ib,
                                                   cudaMemcpyHostToDevice,stream[1]);

                                cudaMemcpy2DAsync( dA(i,i+ib),  ldda *sizeof(double),
                                                   A(i,i+ib), lda*sizeof(double),
                                                   sizeof(double)*ib, (n-i-ib),
                                                   cudaMemcpyHostToDevice,stream[0]);

                                cudaStreamSynchronize(stream[1]);

                                cublasDtrmm( MagmaRight, MagmaUpper,
                                             MagmaTrans, MagmaNonUnit, i, ib,
                                             zone, dA(i,i), ldda, dA(0, i),ldda);


                                lapackf77_dlauum(MagmaUpperStr, &ib, A(i,i), &lda, info);

                                cudaMemcpy2DAsync( dA(i, i), ldda * sizeof(double),
                                                   A(i, i), lda  * sizeof(double),
                                                   sizeof(double)*ib, ib,
                                                   cudaMemcpyHostToDevice,stream[0]);

                                if (i+ib < n)
                                {
                                        cublasDgemm( MagmaNoTrans, MagmaTrans,
                                                     i, ib, (n-i-ib), zone, dA(0,i+ib),
                                                     ldda, dA(i, i+ib),ldda, zone,
                                                     dA(0,i), ldda);

                                        cudaStreamSynchronize(stream[0]);

                                        cublasDsyrk( MagmaUpper, MagmaNoTrans, ib,(n-i-ib),
                                                     done, dA(i, i+ib), ldda,
                                                     done,  dA(i, i), ldda);
                                }
                                
                                cublasGetMatrix( i+ib,ib, sizeof(double),
                                                 dA(0, i), ldda, A(0, i), lda);
                        }
                }
                else
                {
                        /* Compute the product L' * L. */
                        for(i=0; i<n; i=i+nb)
                        {
                                ib=min(nb,n-i);
                                //cublasSetMatrix((n-i), ib, sizeof(double),
                                //                A(i, i), lda, dA(i, i), ldda);

                                cudaMemcpy2DAsync( dA(i, i), ldda *sizeof(double),
                                                   A(i,i), lda*sizeof(double),
                                                   sizeof(double)*ib, ib,
                                                   cudaMemcpyHostToDevice,stream[1]);

                                cudaMemcpy2DAsync( dA(i+ib, i),  ldda *sizeof(double),
                                                   A(i+ib, i), lda*sizeof(double),
                                                   sizeof(double)*(n-i-ib), ib,
                                                   cudaMemcpyHostToDevice,stream[0]);

                                cudaStreamSynchronize(stream[1]);

                                cublasDtrmm( MagmaLeft, MagmaLower,
                                             MagmaTrans, MagmaNonUnit, ib,
                                             i, zone, dA(i,i), ldda,
                                             dA(i, 0),ldda);


                                lapackf77_dlauum(MagmaLowerStr, &ib, A(i,i), &lda, info);

                                //cublasSetMatrix(ib, ib, sizeof(double),
                                //                A(i, i), lda, dA(i, i), ldda);

                                cudaMemcpy2DAsync( dA(i, i), ldda * sizeof(double),
                                                   A(i, i), lda  * sizeof(double),
                                                   sizeof(double)*ib, ib,
                                                   cudaMemcpyHostToDevice,stream[0]);

                                if (i+ib < n)
                                {
                                        cublasDgemm(MagmaTrans, MagmaNoTrans,
                                                        ib, i, (n-i-ib), zone, dA( i+ib,i),
                                                        ldda, dA(i+ib, 0),ldda, zone,
                                                        dA(i,0), ldda);

                                        cudaStreamSynchronize(stream[0]);
                                        
                                        cublasDsyrk(MagmaLower, MagmaTrans, ib, (n-i-ib),
                                                        done, dA(i+ib, i), ldda,
                                                        done,  dA(i, i), ldda);
                                }
                                cublasGetMatrix(ib, i+ib, sizeof(double),
                                        dA(i, 0), ldda, A(i, 0), lda);
                        }
                }
        }
        cudaStreamDestroy(stream[0]);
        cudaStreamDestroy(stream[1]);

        cublasFree(work);

        return MAGMA_SUCCESS;

}
