# Reuse the SDK roots from an existing standalone plugin build. Some temporary
# SDK exports keep libobsConfig.cmake in .deps/cmake while headers live in a
# sibling obs-deps-<version>-<arch>/include tree. No downloads or recursive scan.
set(_fdc_sdk_roots ${OBS_DEPS_ROOT} ${OBS_SOURCE_ROOT} ${CMAKE_PREFIX_PATH})
if(libobs_DIR)
    list(APPEND _fdc_sdk_roots "${libobs_DIR}")
    get_filename_component(_fdc_sdk_parent "${libobs_DIR}" DIRECTORY)
    list(APPEND _fdc_sdk_roots "${_fdc_sdk_parent}")
    get_filename_component(_fdc_sdk_parent "${_fdc_sdk_parent}" DIRECTORY)
    list(APPEND _fdc_sdk_roots "${_fdc_sdk_parent}")
endif()
list(REMOVE_DUPLICATES _fdc_sdk_roots)
set(FDC_OBS_SDK_PREFIXES)
foreach(_fdc_sdk_root IN LISTS _fdc_sdk_roots)
    if(NOT IS_DIRECTORY "${_fdc_sdk_root}")
        continue()
    endif()
    list(APPEND FDC_OBS_SDK_PREFIXES "${_fdc_sdk_root}" "${_fdc_sdk_root}/.deps")
    file(GLOB _fdc_sdk_bundles LIST_DIRECTORIES TRUE
        "${_fdc_sdk_root}/obs-deps-*" "${_fdc_sdk_root}/windows-deps-*"
        "${_fdc_sdk_root}/obs-studio-*"
        "${_fdc_sdk_root}/.deps/obs-deps-*" "${_fdc_sdk_root}/.deps/windows-deps-*"
        "${_fdc_sdk_root}/.deps/obs-studio-*")
    list(APPEND FDC_OBS_SDK_PREFIXES ${_fdc_sdk_bundles})
endforeach()
list(REMOVE_DUPLICATES FDC_OBS_SDK_PREFIXES)
foreach(_fdc_sdk_prefix IN LISTS FDC_OBS_SDK_PREFIXES)
    if(NOT IS_DIRECTORY "${_fdc_sdk_prefix}")
        continue()
    endif()
    if(IS_DIRECTORY "${_fdc_sdk_prefix}/include" OR IS_DIRECTORY "${_fdc_sdk_prefix}/lib")
        list(APPEND CMAKE_PREFIX_PATH "${_fdc_sdk_prefix}")
    endif()
    foreach(_fdc_sdk_finders IN ITEMS cmake/finders finders)
        if(IS_DIRECTORY "${_fdc_sdk_prefix}/${_fdc_sdk_finders}")
            list(APPEND CMAKE_MODULE_PATH "${_fdc_sdk_prefix}/${_fdc_sdk_finders}")
        endif()
    endforeach()
endforeach()
# The local SIMDe finder is a fallback when the SDK did not ship its finders.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
