/*
    -- MAGMA (version 1.4.0-beta2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       June 2013

       @generated d Fri Jun 28 19:31:30 2013
       @author Mark Gates
*/

#ifndef MAGMA_DGEHRD_H
#define MAGMA_DGEHRD_H

#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dgehrd_data
{
    int ngpu;
    
    magma_int_t ldda;
    magma_int_t ldv;
    magma_int_t ldvd;
    
    double *A    [ MagmaMaxGPUs ];  // ldda*nlocal
    double *V    [ MagmaMaxGPUs ];  // ldv *nb, whole panel
    double *Vd   [ MagmaMaxGPUs ];  // ldvd*nb, block-cyclic
    double *Y    [ MagmaMaxGPUs ];  // ldda*nb
    double *W    [ MagmaMaxGPUs ];  // ldda*nb
    double *Ti   [ MagmaMaxGPUs ];  // nb*nb
    
    magma_queue_t streams[ MagmaMaxGPUs ];
};

#ifdef __cplusplus
}
#endif

#endif        //  #ifndef MAGMA_DGEHRD_H
