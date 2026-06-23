add_library(n64xr_common INTERFACE)
add_library(N64XR::Common ALIAS n64xr_common)

target_include_directories(n64xr_common INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(n64xr_common INTERFACE
    spdlog::spdlog
    fmt::fmt
)
