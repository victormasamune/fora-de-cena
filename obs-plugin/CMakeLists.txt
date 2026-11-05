cmake_minimum_required(VERSION 3.20)
project(fora_de_cena_obs VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional hints from an existing build. The libobs SDK owns discovery of its
# public dependencies (SIMDe, Threads and w32-pthreads on MSVC).
if(DEFINED OBS_SOURCE_ROOT AND EXISTS "${OBS_SOURCE_ROOT}/cmake/finders")
    list(PREPEND CMAKE_MODULE_PATH "${OBS_SOURCE_ROOT}/cmake/finders")
endif()
if(DEFINED OBS_DEPS_ROOT AND EXISTS "${OBS_DEPS_ROOT}/cmake/finders")
    list(PREPEND CMAKE_MODULE_PATH "${OBS_DEPS_ROOT}/cmake/finders")
endif()
# The IPC filter does not call FFmpeg. Those libraries are private dependencies
# of libobs, not prerequisites that this plugin should discover independently.
find_package(libobs CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(fora_de_cena_obs MODULE src/plugin-main.cpp src/ipc-bridge.cpp)
target_link_libraries(fora_de_cena_obs PRIVATE OBS::libobs Threads::Threads)
if(WIN32)
    target_link_libraries(fora_de_cena_obs PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(fora_de_cena_obs PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(fora_de_cena_obs PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_target_properties(fora_de_cena_obs PROPERTIES PREFIX "" OUTPUT_NAME "fora-de-cena-obs")
