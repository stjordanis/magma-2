###
#
# @file      : Summary.cmake
#
# @description   :
#
# @version       :
# @created by    : Cedric Castagnede
# @creation date : 23-01-2012
# @last modified : mar. 22 mai 2012 11:46:40 CEST
#
###

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

MACRO(SUMMARY)
    MESSAGE(STATUS "-----------------------------------------------------------------------------")
    MESSAGE(STATUS "Status of your MAGMA project configuration:")
    MESSAGE(STATUS "-----------------------------------------------------------------------------")
    MESSAGE(STATUS "MAGMA modules requested:")
    MESSAGE(STATUS "  - MAGMA_1GPU              : ${MAGMA_1GPU}")
    #MESSAGE(STATUS "  - MAGMA_MGPUS_STATIC      : ${MAGMA_MGPUS_STATIC}")
    MESSAGE(STATUS "  - MAGMA_MORSE             : ${MAGMA_MORSE}")
    IF(MAGMA_1GPU)
        MESSAGE(STATUS "")
        MESSAGE(STATUS "Configuration of the MAGMA project:")
        MESSAGE(STATUS "  - MAGMA_USE_FERMI         : ${MAGMA_USE_FERMI}")
        MESSAGE(STATUS "  - MAGMA_USE_PLASMA        : ${MAGMA_USE_PLASMA}")
    ENDIF(MAGMA_1GPU)
    IF(MAGMA_MORSE)
        MESSAGE(STATUS "")
        MESSAGE(STATUS "Configuration of the MORSE module:")
        #MESSAGE(STATUS "  - MORSE_USE_MULTICORE : ${MORSE_USE_MULTICORE}")
        #MESSAGE(STATUS "")
        MESSAGE(STATUS "  - MORSE_USE_CUDA      : ${MORSE_USE_CUDA}")
        MESSAGE(STATUS "  - MORSE_USE_MPI       : ${MORSE_USE_MPI}")
        MESSAGE(STATUS "")
        MESSAGE(STATUS "  - MORSE_SCHED_STARPU  : ${MORSE_SCHED_STARPU}")
        MESSAGE(STATUS "  - MORSE_SCHED_QUARK   : ${MORSE_SCHED_QUARK}")
    ENDIF()
    MESSAGE(STATUS "")
    MESSAGE(STATUS "Configuration of the external libraries:")
    IF(MORSE_USE_CUDA)
        MESSAGE(STATUS "  - CUDA :")
        IF(HAVE_CUDA)
            MESSAGE(STATUS "    --> CUDA_TOOLKIT_ROOT_DIR : ${CUDA_TOOLKIT_ROOT_DIR}")
        ELSE(HAVE_CUDA)
            MESSAGE(STATUS "      --> CUDA was not found !!!")
            MESSAGE(STATUS "      (v4.0 required at least)")
        ENDIF()
    ENDIF()
    MESSAGE(STATUS "")
    MESSAGE(STATUS "  - BLAS :")
    IF(${BLAS_FOUND})
        MESSAGE(STATUS "    --> BLAS_FOUND        : ${BLAS_FOUND}")
    ENDIF()
    IF(DEFINED BLAS_BUILD_MODE)
        MESSAGE(STATUS "    --> BLAS_BUILD        : ${BLAS_BUILD_MODE}")
        IF(NOT "ON" MATCHES "${MORSE_USE_BLAS}")
            MESSAGE(STATUS "    --> BLAS_VERSION      : ${MORSE_USE_BLAS}")
        ENDIF()
    ENDIF()
    MESSAGE(STATUS "    --> BLAS_LIBRARY_PATH : ${BLAS_LIBRARY_PATH}")
    MESSAGE(STATUS "    --> BLAS_LIBRARIES    : ${BLAS_LIBRARIES}")
    MESSAGE(STATUS "")
    MESSAGE(STATUS "  - LAPACK :")
    IF(${LAPACK_FOUND})
        MESSAGE(STATUS "    --> LAPACK_FOUND        : ${LAPACK_FOUND}")
    ENDIF()
    IF(DEFINED LAPACK_BUILD_MODE)
        MESSAGE(STATUS "    --> LAPACK_BUILD        : ${LAPACK_BUILD_MODE}")
    ENDIF()
    MESSAGE(STATUS "    --> LAPACK_LIBRARY_PATH : ${LAPACK_LIBRARY_PATH}")
    MESSAGE(STATUS "    --> LAPACK_LIBRARIES    : ${LAPACK_LIBRARIES}")
    MESSAGE(STATUS "")
    IF(MAGMA_MORSE OR MAGMA_USE_PLASMA)
        MESSAGE(STATUS "  - CBLAS :")
        IF(${CBLAS_FOUND})
            MESSAGE(STATUS "    --> CBLAS_FOUND        : ${CBLAS_FOUND}")
        ENDIF()
        IF(DEFINED CBLAS_BUILD_MODE)
            MESSAGE(STATUS "    --> CBLAS_BUILD        : ${CBLAS_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> CBLAS_LIBRARY_PATH : ${CBLAS_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> CBLAS_INCLUDE_PATH : ${CBLAS_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> CBLAS_LIBRARIES    : ${CBLAS_LIBRARIES}")
        MESSAGE(STATUS "")
        MESSAGE(STATUS "  - LAPACKE :")
        IF(${LAPACKE_FOUND})
            MESSAGE(STATUS "    --> LAPACKE_FOUND        : ${LAPACKE_FOUND}")
        ENDIF()
        IF(DEFINED LAPACKE_BUILD_MODE)
            MESSAGE(STATUS "    --> LAPACKE_BUILD        : ${LAPACKE_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> LAPACKE_LIBRARY_PATH : ${LAPACKE_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> LAPACKE_INCLUDE_PATH : ${LAPACKE_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> LAPACKE_LIBRARIES    : ${LAPACKE_LIBRARIES}")
        MESSAGE(STATUS "")
        MESSAGE(STATUS "  - PLASMA :")
        IF(${PLASMA_FOUND})
            MESSAGE(STATUS "    --> PLASMA_FOUND        : ${PLASMA_FOUND}")
        ENDIF()
        IF(DEFINED PLASMA_BUILD_MODE)
            MESSAGE(STATUS "    --> PLASMA_BUILD        : ${PLASMA_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> PLASMA_LIBRARY_PATH : ${PLASMA_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> PLASMA_INCLUDE_PATH : ${PLASMA_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> PLASMA_LIBRARIES    : ${PLASMA_LIBRARIES}")
        MESSAGE(STATUS "")
    ENDIF()
    IF(MORSE_USE_MPI)
        MESSAGE(STATUS "  - MPI : ${MORSE_USE_MPI}")
        IF(MPI_FOUND)
            MESSAGE(STATUS "    --> MPI_LIBRARY_PATH : ${MPI_LIBRARY_PATH}")
            MESSAGE(STATUS "    --> MPI_INCLUDE_PATH : ${MPI_INCLUDE_PATH}")
            MESSAGE(STATUS "    --> MPI_LIBRARIES    : ${MPI_LIBRARIES}")
            MESSAGE(STATUS "")
        ELSE(MPI_FOUND)
            MESSAGE(STATUS "    --> MPI was not found!!!")
            MESSAGE(STATUS "")
        ENDIF()
    ENDIF()
    MESSAGE(STATUS "Status of required packages by the MORSE project:")
    IF(MAGMA_MORSE OR MAGMA_USE_PLASMA)
        MESSAGE(STATUS "  - HWLOC :")
        IF(${HWLOC_FOUND})
            MESSAGE(STATUS "    --> HWLOC_FOUND        : ${HWLOC_FOUND}")
        ENDIF()
        IF(DEFINED HWLOC_BUILD_MODE)
            MESSAGE(STATUS "    --> HWLOC_BUILD        : ${HWLOC_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> HWLOC_LIBRARY_PATH : ${HWLOC_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> HWLOC_INCLUDE_PATH : ${HWLOC_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> HWLOC_LIBRARIES    : ${HWLOC_LIBRARIES}")
        MESSAGE(STATUS "")
    ENDIF()
    IF(MAGMA_MORSE)
        MESSAGE(STATUS "  - STARPU :")
        IF(${STARPU_FOUND})
            MESSAGE(STATUS "    --> STARPU_FOUND        : ${STARPU_FOUND}")
        ENDIF()
        IF(DEFINED STARPU_BUILD_MODE)
            MESSAGE(STATUS "    --> STARPU_BUILD        : ${STARPU_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> STARPU_LIBRARY_PATH : ${STARPU_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> STARPU_INCLUDE_PATH : ${STARPU_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> STARPU_LIBRARIES    : ${STARPU_LIBRARIES}")
        MESSAGE(STATUS "")
    ENDIF()
    IF(HAVE_FXT)
        MESSAGE(STATUS "Status of third-party packages:")
        MESSAGE(STATUS "  - FXT :")
        IF(${FXT_FOUND})
            MESSAGE(STATUS "    --> FXT_FOUND        : ${FXT_FOUND}")
        ENDIF()
        IF(DEFINED FXT_BUILD_MODE)
            MESSAGE(STATUS "    --> FXT_BUILD        : ${FXT_BUILD_MODE}")
        ENDIF()
        MESSAGE(STATUS "    --> FXT_LIBRARY_PATH : ${FXT_LIBRARY_PATH}")
        MESSAGE(STATUS "    --> FXT_INCLUDE_PATH : ${FXT_INCLUDE_PATH}")
        MESSAGE(STATUS "    --> FXT_LIBRARIES    : ${FXT_LIBRARIES}")
    ENDIF()
    #MESSAGE(STATUS "  - EZTRACE :")
    MESSAGE(STATUS "")
    MESSAGE(STATUS "--------------+--------------------------------------------------------------")
    MESSAGE(STATUS "Command       | Description")
    MESSAGE(STATUS "--------------+--------------------------------------------------------------")
    IF(MAGMA_MORSE)
        MESSAGE(STATUS "make          | Compile MORSE project in:")
    ELSE()
        MESSAGE(STATUS "make          | Compile MAGMA project in:")
    ENDIF()
    MESSAGE(STATUS "              |     ${CMAKE_BINARY_DIR}")
    MESSAGE(STATUS "              |")
    MESSAGE(STATUS "make test     | Build and run the unit-tests.")
    MESSAGE(STATUS "              |")
    IF(MAGMA_MORSE)
        MESSAGE(STATUS "make install  | Install MORSE projecs in:")
    ELSE()
        MESSAGE(STATUS "make install  | Install MAGMA projecs in:")
    ENDIF()
    MESSAGE(STATUS "              |     ${CMAKE_INSTALL_PREFIX}")
    MESSAGE(STATUS "              | To change that:")
    MESSAGE(STATUS "              |     cmake ../ -DCMAKE_INSTALL_PREFIX=yourpath")
    MESSAGE(STATUS "              |")
    MESSAGE(STATUS "--------------+--------------------------------------------------------------")

ENDMACRO(SUMMARY)