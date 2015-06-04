# - Find LAPACK library
# This module finds an installed fortran library that implements the LAPACK
# linear-algebra interface (see http://www.netlib.org/lapack/).
#
# The approach follows that taken for the autoconf macro file, acx_lapack.m4
# (distributed at http://ac-archive.sourceforge.net/ac-archive/acx_lapack.html).
#
# This module sets the following variables:
#  LAPACK_FOUND - set to true if a library implementing the LAPACK interface is found
#
#  LAPACK_LDFLAGS      : uncached string of all required linker flags.
#  LAPACK_LIBRARIES    : uncached list of required linker flags (without -l and -L).
#  LAPACK_LIBRARY_PATH : uncached path of the library directory of fxt installation.
#
#  LAPACK_LIBS - uncached list of libraries (using full path name) to link against to use LAPACK
#  LAPACK95_LIBS - uncached list of libraries (using full path name) to link against to use LAPACK95
#  LAPACK95_FOUND - set to true if a library implementing the LAPACK f95 interface is found
#
#  BLA_STATIC  if set on this determines what kind of linkage we do (static)
#  BLA_VENDOR  if set checks only the specified vendor, if not set checks all the possibilities
#  BLA_F95     if set on tries to find the f95 interfaces for BLAS/LAPACK
#
###

include(CheckFunctionExists)
include(CheckFortranFunctionExists)

set(_lapack_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

# Check the language being used
get_property(_LANGUAGES_ GLOBAL PROPERTY ENABLED_LANGUAGES)
if (NOT _LANGUAGES_ MATCHES Fortran)
    include(CheckFunctionExists)
else (NOT _LANGUAGES_ MATCHES Fortran)
    include(CheckFortranFunctionExists)
endif (NOT _LANGUAGES_ MATCHES Fortran)

set(LAPACK_FOUND FALSE)
set(LAPACK95_FOUND FALSE)

macro(Check_Lapack_Libraries LIBRARIES _prefix _name _flags _list _blas _threads)
# This macro checks for the existence of the combination of fortran libraries
# given by _list.  If the combination is found, this macro checks (using the
# Check_Fortran_Function_Exists macro) whether can link against that library
# combination using the name of a routine given by _name using the linker
# flags given by _flags.  If the combination of libraries is found and passes
# the link test, LIBRARIES is set to the list of complete library paths that
# have been found.  Otherwise, LIBRARIES is set to FALSE.

# N.B. _prefix is the prefix applied to the names of all cached variables that
# are generated internally and marked advanced by this macro.

set(_libdir ${ARGN})

set(_libraries_work TRUE)
set(${LIBRARIES})
set(_combined_name)
set(${_prefix}_LIBRARY_PATH "${_prefix}_LIBRARY_PATH-NOTFOUND")
if (NOT _libdir)
    if (WIN32)
        set(_libdir ENV LIB)
    elseif (APPLE)
        string(REPLACE ":" ";" _lib_env "$ENV{DYLD_LIBRARY_PATH}")
        set(_libdir "${LAPACK_DIR};${BLAS_LIBRARY_PATH};${_lib_env};/usr/local/lib64;/usr/lib64;/usr/local/lib;/usr/lib")
    else ()
        string(REPLACE ":" ";" _lib_env "$ENV{LD_LIBRARY_PATH}")
        set(_libdir "${LAPACK_DIR};${BLAS_LIBRARY_PATH};${_lib_env};/usr/local/lib64;/usr/lib64;/usr/local/lib;/usr/lib")
    endif ()
endif ()
foreach(_library ${_list})
    set(_combined_name ${_combined_name}_${_library})
    set(${_prefix}_${_library}_LIBRARY_FILENAME "${_prefix}_${_library}_LIBRARY_FILENAME-NOTFOUND")

    if(_libraries_work)
        if (BLA_STATIC)
            if (WIN32)
                set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
            endif ( WIN32 )
            if (APPLE)
                set(CMAKE_FIND_LIBRARY_SUFFIXES .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})
            else (APPLE)
                set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
            endif (APPLE)
        else (BLA_STATIC)
            if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
                # for ubuntu's libblas3gf and liblapack3gf packages
                set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES} .so.3gf)
            endif ()
        endif (BLA_STATIC)
        find_library(${_prefix}_${_library}_LIBRARY
                     NAMES ${_library}
                     PATHS ${_libdir}
                     )
        if(NOT ${${_prefix}_${_library}_LIBRARY})
            get_filename_component(${_prefix}_LIBRARY_FILENAME ${${_prefix}_${_library}_LIBRARY} NAME)
            string(REGEX REPLACE "(.*)/${${_prefix}_LIBRARY_FILENAME}" "\\1" ${_prefix}_LIBRARY_PATH "${${_prefix}_${_library}_LIBRARY}")
        endif()
        set(${LIBRARIES} ${${LIBRARIES}} ${${_prefix}_${_library}_LIBRARY})
        set(_libraries_work ${${_prefix}_${_library}_LIBRARY})
        mark_as_advanced(${_prefix}_${_library}_LIBRARY)
        mark_as_advanced(${_prefix}_LIBRARY_PATH)
    endif(_libraries_work)
endforeach(_library ${_list})

if(_libraries_work)
    # Test this combination of libraries.
    if(UNIX AND BLA_STATIC)
        set(CMAKE_REQUIRED_LIBRARIES ${_flags} "-Wl,--start-group ${${LIBRARIES}} ;-Wl,--end-group" ${_blas} ${_threads})
    else(UNIX AND BLA_STATIC)
        set(CMAKE_REQUIRED_LIBRARIES ${_flags} ${${LIBRARIES}} ${_blas} ${_threads})
    endif(UNIX AND BLA_STATIC)
    if (NOT _LANGUAGES_ MATCHES Fortran)
        check_function_exists("${_name}_" ${_prefix}${_combined_name}_WORKS)
    else (NOT _LANGUAGES_ MATCHES Fortran)
        check_fortran_function_exists(${_name} ${_prefix}${_combined_name}_WORKS)
    endif (NOT _LANGUAGES_ MATCHES Fortran)
    set(CMAKE_REQUIRED_LIBRARIES)
    mark_as_advanced(${_prefix}${_combined_name}_WORKS)
    set(_libraries_work ${${_prefix}${_combined_name}_WORKS})
endif(_libraries_work)

if(NOT _libraries_work)
    set(${LIBRARIES} FALSE)
else(NOT _libraries_work)
    set(LAPACK_LIBRARIES ${_flags})
    set(LAPACK_LIBRARY_PATH ${${_prefix}_LIBRARY_PATH})
    set(LAPACK_LDFLAGS "-L${${_prefix}_LIBRARY_PATH}")
    foreach(_flag ${_flags})
        set(LAPACK_LDFLAGS "${LAPACK_LDFLAGS} -l${_flag}")
    endforeach()
    set(LAPACK_LDFLAGS "${LAPACK_LDFLAGS} ${_threads}")
endif(NOT _libraries_work)
#message("DEBUG: ${LIBRARIES}        = ${${LIBRARIES}}")
#message("DEBUG: _prefix             = ${_prefix}") 
#message("DEBUG: _name               = ${_name}") 
#message("DEBUG: _flags              = ${_flags}") 
#message("DEBUG: _list               = ${_list}") 
#message("DEBUG: _blas               = ${_blas}") 
#message("DEBUG: _threads            = ${_threads}")
#message("DEBUG: LAPACK_LIBRARY_PATH = ${LAPACK_LIBRARY_PATH}")
#message("DEBUG: LAPACK_LIBRARIES    = ${LAPACK_LIBRARIES}")
#message("DEBUG: LAPACK_LDFLAGS      = ${LAPACK_LDFLAGS}")
endmacro(Check_Lapack_Libraries)


set(LAPACK_LIBRARIES)
set(LAPACK_LIBS)
set(LAPACK95_LIBS)


if(LAPACK_FIND_QUIETLY OR NOT LAPACK_FIND_REQUIRED)
    IF(NOT ${BLAS_FOUND})
        find_package(BLAS)
    ENDIF(NOT ${BLAS_FOUND})
else(LAPACK_FIND_QUIETLY OR NOT LAPACK_FIND_REQUIRED)
    IF(NOT ${BLAS_FOUND})
        find_package(BLAS REQUIRED)
    ENDIF(NOT ${BLAS_FOUND})
endif(LAPACK_FIND_QUIETLY OR NOT LAPACK_FIND_REQUIRED)


if(BLAS_FOUND)
    set(LAPACK_LIBRARIES ${BLAS_LIBRARIES})
    if ($ENV{BLA_VENDOR} MATCHES ".+")
        set(BLA_VENDOR $ENV{BLA_VENDOR})
    else ($ENV{BLA_VENDOR} MATCHES ".+")
        if(NOT BLA_VENDOR)
            set(BLA_VENDOR "All")
        endif(NOT BLA_VENDOR)
    endif ($ENV{BLA_VENDOR} MATCHES ".+")

    if (BLA_VENDOR STREQUAL "Goto" OR BLA_VENDOR STREQUAL "All")
        if(NOT LAPACK_LIBS)
            check_lapack_libraries(
            LAPACK_LIBS
            LAPACK
            cheev
            "goto2"
            "goto2"
            "${BLAS_LDFLAGS}"
            ""
            )
        endif(NOT LAPACK_LIBS)
    endif (BLA_VENDOR STREQUAL "Goto" OR BLA_VENDOR STREQUAL "All")


    # ACML lapack
    if (BLA_VENDOR MATCHES "ACML.*" OR BLA_VENDOR STREQUAL "All")
        if (BLAS_LIBS MATCHES ".+acml.+")
            set (LAPACK_LIBS ${BLAS_LIBS})
        endif ()
    endif ()

    # Apple LAPACK library?
    if (BLA_VENDOR STREQUAL "Apple" OR BLA_VENDOR STREQUAL "All")
        if(NOT LAPACK_LIBS)
            check_lapack_libraries(
            LAPACK_LIBS
            LAPACK
            cheev
            "Accelerate"
            "Accelerate"
            "${BLAS_LDFLAGS}"
            ""
            )
        endif(NOT LAPACK_LIBS)
    endif (BLA_VENDOR STREQUAL "Apple" OR BLA_VENDOR STREQUAL "All")

    # NAS lapack
    if (BLA_VENDOR STREQUAL "NAS" OR BLA_VENDOR STREQUAL "All")
        if ( NOT LAPACK_LIBS )
            check_lapack_libraries(
            LAPACK_LIBS
            LAPACK
            cheev
            "vecLib"
            "vecLib"
            "${BLAS_LDFLAGS}"
            ""
            )
        endif ( NOT LAPACK_LIBS )
    endif (BLA_VENDOR STREQUAL "NAS" OR BLA_VENDOR STREQUAL "All")

    # Intel lapack
    if (BLA_VENDOR MATCHES "Intel*" OR BLA_VENDOR STREQUAL "All")
        if (_LANGUAGES_ MATCHES C OR _LANGUAGES_ MATCHES CXX)
            if(BLAS_FIND_QUIETLY OR NOT BLAS_FIND_REQUIRED)
                find_package(Threads)
            else(BLAS_FIND_QUIETLY OR NOT BLAS_FIND_REQUIRED)
                find_package(Threads REQUIRED)
            endif(BLAS_FIND_QUIETLY OR NOT BLAS_FIND_REQUIRED)
            if (BLA_F95)
                if(NOT LAPACK95_LIBS)
                    check_lapack_libraries(
                    LAPACK95_LIBS
                    LAPACK
                    cheev
                    "mkl_lapack95"
                    "mkl_lapack95"
                    "${BLAS95_LDFLAGS}"
                    "${CMAKE_THREAD_LIBS_INIT}"
                    )
                endif(NOT LAPACK95_LIBS)
                if(NOT LAPACK95_LIBS)
                    check_lapack_libraries(
                    LAPACK95_LIBS
                    LAPACK
                    cheev
                    "mkl_lapack95_lp64"
                    "mkl_lapack95_lp64"
                    "${BLAS95_LDFLAGS}"
                    "${CMAKE_THREAD_LIBS_INIT}"
                    )
                endif(NOT LAPACK95_LIBS)
            else(BLA_F95)
                if(NOT LAPACK_LIBS)
                    check_lapack_libraries(
                    LAPACK_LIBS
                    LAPACK
                    cheev
                    "mkl_lapack"
                    "mkl_lapack"
                    "${BLAS_LDFLAGS}"
                    "${CMAKE_THREAD_LIBS_INIT}"
                    )
                endif(NOT LAPACK_LIBS)
            endif(BLA_F95)
            if(NOT LAPACK_LIBS)
                set(BLAS_mkl_LIBRARIES ${BLAS_LIBRARIES})
                list(REMOVE_ITEM BLAS_mkl_LIBRARIES pthread m)
                check_lapack_libraries(
                LAPACK_LIBS
                LAPACK
                cheev
                "${BLAS_LIBRARIES}"
                "${BLAS_mkl_LIBRARIES}"
                "${BLAS_LDFLAGS}"
                "${CMAKE_THREAD_LIBS_INIT}"
                )
            endif(NOT LAPACK_LIBS)
        endif (_LANGUAGES_ MATCHES C OR _LANGUAGES_ MATCHES CXX)
    endif(BLA_VENDOR MATCHES "Intel*" OR BLA_VENDOR STREQUAL "All")

    # Generic LAPACK library?
    if (BLA_VENDOR STREQUAL "Generic" OR
        BLA_VENDOR STREQUAL "ATLAS" OR
        BLA_VENDOR STREQUAL "All")
        if ( NOT LAPACK_LIBS )
            check_lapack_libraries(
            LAPACK_LIBS
            LAPACK
            cheev
            "lapack"
            "lapack"
            "${BLAS_LDFLAGS}"
            ""
            )
        endif ( NOT LAPACK_LIBS )
    endif ()

else(BLAS_FOUND)
    message(STATUS "LAPACK requires BLAS")
endif(BLAS_FOUND)

if(BLA_F95)
    if(LAPACK95_LIBS)
        set(LAPACK95_FOUND TRUE)
    else(LAPACK95_LIBS)
        set(LAPACK95_FOUND FALSE)
    endif(LAPACK95_LIBS)
    if(NOT LAPACK_FIND_QUIETLY)
        if(LAPACK95_FOUND)
            message(STATUS "A library with LAPACK95 API found.")
        else(LAPACK95_FOUND)
            if(LAPACK_FIND_REQUIRED)
                message(FATAL_ERROR
                        "A required library with LAPACK95 API not found. Please specify library location.")
            else(LAPACK_FIND_REQUIRED)
                message(STATUS
                        "A library with LAPACK95 API not found. Please specify library location.")
            endif(LAPACK_FIND_REQUIRED)
        endif(LAPACK95_FOUND)
    endif(NOT LAPACK_FIND_QUIETLY)
    set(LAPACK_FOUND "${LAPACK95_FOUND}")
    set(LAPACK_LIBS "${LAPACK95_LIBS}")
else(BLA_F95)
    if(LAPACK_LIBS)
        set(LAPACK_FOUND TRUE)
    else(LAPACK_LIBS)
        set(LAPACK_FOUND FALSE)
    endif(LAPACK_LIBS)

    if(NOT LAPACK_FIND_QUIETLY)
        if(LAPACK_FOUND)
            message(STATUS "A library with LAPACK API found.")
        else(LAPACK_FOUND)
            if(LAPACK_FIND_REQUIRED)
                message(FATAL_ERROR
                        "A required library with LAPACK API not found. Please specify library location.")
            else(LAPACK_FIND_REQUIRED)
                message(STATUS
                        "A library with LAPACK API not found. Please specify library location.")
            endif(LAPACK_FIND_REQUIRED)
        endif(LAPACK_FOUND)
    endif(NOT LAPACK_FIND_QUIETLY)
endif(BLA_F95)

set(CMAKE_FIND_LIBRARY_SUFFIXES ${_lapack_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})