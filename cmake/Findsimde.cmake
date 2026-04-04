find_path(SIMDE_INCLUDE_DIR
    NAMES simde/x86/ssse3.h
    DOC "Path to the simde header files"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(simde
    REQUIRED_VARS SIMDE_INCLUDE_DIR
)

if(simde_FOUND AND NOT TARGET simde::simde)
    add_library(simde::simde INTERFACE IMPORTED)
    set_target_properties(simde::simde PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SIMDE_INCLUDE_DIR}"
    )
endif()

set(SIMDE_INCLUDE_DIRS "${SIMDE_INCLUDE_DIR}")
mark_as_advanced(SIMDE_INCLUDE_DIR)
