add_library(cartridgexr_common INTERFACE)
add_library(CartridgeXR::Common ALIAS cartridgexr_common)

target_include_directories(cartridgexr_common INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(cartridgexr_common INTERFACE
    spdlog::spdlog
    fmt::fmt
)
