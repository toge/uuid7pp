cmake_minimum_required(VERSION 3.25)
project(uuidv7pp LANGUAGES CXX)

include(GNUInstallDirs)

## Options
option(ENABLE_TEST "Build and run tests" ON)
option(STATIC "static binary for sample" OFF)

## C++ Standard selection
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++26" COMPILER_SUPPORTS_CXX26)
CHECK_CXX_COMPILER_FLAG("-std=c++23" COMPILER_SUPPORTS_CXX23)
if(COMPILER_SUPPORTS_CXX26)
    set(STD_CPP cxx_std_26)
elseif(COMPILER_SUPPORTS_CXX23)
    set(STD_CPP cxx_std_23)
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++23 support. C++23 or higher is required.")
endif()

## Optimization and Warning flags
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -O3 -march=native")
    if(STATIC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc")
    endif()
elseif (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /Ox")
endif()

## LTO (Interprocedural Optimization)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## Dependencies
find_path(SIMDE_INCLUDE_DIRS "simde/simde-common.h")

## Library Definition (Header-only)
add_library(uuidv7pp INTERFACE)
add_library(uuidv7pp::uuidv7pp ALIAS uuidv7pp)

target_include_directories(uuidv7pp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# vcpkg等のビルドディレクトリ内のインクルードパスはBUILD_INTERFACEでのみ含める
if(SIMDE_INCLUDE_DIRS)
    target_include_directories(uuidv7pp INTERFACE $<BUILD_INTERFACE:${SIMDE_INCLUDE_DIRS}>)
endif()

target_compile_features(uuidv7pp INTERFACE ${STD_CPP})

## Testing
if(ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

## Sample Application
add_executable(sample sample.cpp)
target_link_libraries(sample PRIVATE uuidv7pp)

## Installation
install(TARGETS uuidv7pp EXPORT uuidv7ppTargets)
install(FILES uuidv7pp.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT uuidv7ppTargets
    FILE uuidv7ppTargets.cmake
    NAMESPACE uuidv7pp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uuidv7pp
)

# Config file for find_package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/uuidv7ppConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/uuidv7ppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uuidv7pp
)
