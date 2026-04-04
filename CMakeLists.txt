cmake_minimum_required(VERSION 3.25)
project(uuid7pp VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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
find_package(simde REQUIRED)

## Library Definition (Header-only)
add_library(uuid7pp INTERFACE)
add_library(uuid7pp::uuid7pp ALIAS uuid7pp)
target_include_directories(uuid7pp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(uuid7pp INTERFACE simde::simde)
target_compile_features(uuid7pp INTERFACE ${STD_CPP})

## Testing
if(ENABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

## Sample Application
add_executable(sample sample.cpp)
target_link_libraries(sample PRIVATE uuid7pp)

## Installation
install(TARGETS uuid7pp EXPORT uuid7ppTargets)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/uuid7pp.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT uuid7ppTargets
    FILE uuid7ppTargets.cmake
    NAMESPACE uuid7pp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uuid7pp
)

# Config file for find_package
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uuid7ppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uuid7ppConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/uuid7pp"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/uuid7ppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/uuid7ppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/uuid7ppConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Findsimde.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/uuid7pp
)
