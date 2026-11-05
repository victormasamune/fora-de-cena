# Standalone plugin fallback: locate real SIMD Everywhere headers. This creates
# the same public header-only target expected by libobsConfig.cmake, never a stub.
include(FindPackageHandleStandardArgs)

if(SIMDe_INCLUDE_DIR AND NOT EXISTS "${SIMDe_INCLUDE_DIR}/simde/simde-common.h")
    # The earlier temporary SDK may have been moved or removed.
    unset(SIMDe_INCLUDE_DIR CACHE)
    unset(SIMDe_INCLUDE_DIR)
endif()
find_path(SIMDe_INCLUDE_DIR
    NAMES simde/simde-common.h
    HINTS ${FDC_OBS_SDK_PREFIXES}
    PATH_SUFFIXES include
    DOC "Directory containing the real simde/simde-common.h header")

unset(SIMDe_VERSION)
if(EXISTS "${SIMDe_INCLUDE_DIR}/simde/simde-common.h")
    foreach(_fdc_simde_part IN ITEMS MAJOR MINOR MICRO)
        unset(_fdc_simde_${_fdc_simde_part})
        file(STRINGS "${SIMDe_INCLUDE_DIR}/simde/simde-common.h" _fdc_simde_line
            REGEX "^[ \t]*#[ \t]*define[ \t]+SIMDE_VERSION_${_fdc_simde_part}[ \t]+[0-9]+")
        if(_fdc_simde_line)
            string(REGEX MATCH "SIMDE_VERSION_${_fdc_simde_part}[ \t]+([0-9]+)" _fdc_simde_match "${_fdc_simde_line}")
            set(_fdc_simde_${_fdc_simde_part} "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(DEFINED _fdc_simde_MAJOR AND DEFINED _fdc_simde_MINOR AND DEFINED _fdc_simde_MICRO)
        set(SIMDe_VERSION "${_fdc_simde_MAJOR}.${_fdc_simde_MINOR}.${_fdc_simde_MICRO}")
    endif()
endif()
find_package_handle_standard_args(SIMDe
    REQUIRED_VARS SIMDe_INCLUDE_DIR
    VERSION_VAR SIMDe_VERSION
    REASON_FAILURE_MESSAGE
        "The real simde/simde-common.h header was not found in the cached OBS SDK. Set SIMDe_INCLUDE_DIR to its include directory or OBS_DEPS_ROOT/CMAKE_PREFIX_PATH to the extracted obs-deps package.")
mark_as_advanced(SIMDe_INCLUDE_DIR)
if(SIMDe_FOUND AND NOT TARGET SIMDe::SIMDe)
    add_library(SIMDe::SIMDe INTERFACE IMPORTED)
    set_target_properties(SIMDe::SIMDe PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SIMDe_INCLUDE_DIR}")
endif()
