# FindOpenXR.cmake
# Locates the Khronos OpenXR SDK source clone.
#
# Honors:
#   OPENXR_SDK_ROOT  CMake var or env var, pointing at OpenXR-SDK source clone
# Default search path: C:/dev/OpenXR-SDK
#
# Provides:
#   OpenXR_FOUND
#   OpenXR_INCLUDE_DIRS
#   OpenXR_LOADER_INCLUDE_DIRS
#   Imported target: OpenXR::Headers (interface-only; loader is built as subdir)

if(NOT DEFINED OPENXR_SDK_ROOT)
    if(DEFINED ENV{OPENXR_SDK_ROOT})
        set(OPENXR_SDK_ROOT "$ENV{OPENXR_SDK_ROOT}")
    else()
        set(OPENXR_SDK_ROOT "C:/dev/OpenXR-SDK")
    endif()
endif()

find_path(OpenXR_INCLUDE_DIR
    NAMES openxr/openxr.h
    PATHS "${OPENXR_SDK_ROOT}/include"
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenXR
    REQUIRED_VARS OpenXR_INCLUDE_DIR OPENXR_SDK_ROOT
)

if(OpenXR_FOUND)
    set(OpenXR_INCLUDE_DIRS "${OpenXR_INCLUDE_DIR}")
    set(OpenXR_LOADER_INCLUDE_DIRS "${OPENXR_SDK_ROOT}/src/loader")

    if(NOT TARGET OpenXR::Headers)
        add_library(OpenXR::Headers INTERFACE IMPORTED)
        set_target_properties(OpenXR::Headers PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OpenXR_INCLUDE_DIRS}"
        )
    endif()

    # The loader is added via add_subdirectory in the consumer (see root CMakeLists.txt).
    # That produces the `openxr_loader` target.
endif()

mark_as_advanced(OpenXR_INCLUDE_DIR)
