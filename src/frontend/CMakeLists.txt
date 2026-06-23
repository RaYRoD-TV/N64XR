add_executable(N64XR
    src/main.cpp
)

target_link_libraries(N64XR
    PRIVATE
        N64XR::Common
        N64XR::VrScene
)

# Console subsystem on Windows so logs land in a terminal during dev.
# Switch to WIN32 + a small WinMain shim later if a "no-console" launch is wanted.
if(WIN32)
    set_target_properties(N64XR PROPERTIES
        WIN32_EXECUTABLE OFF
    )
endif()
