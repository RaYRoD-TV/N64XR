qt_add_executable(CartridgeXR WIN32
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.h
)

target_include_directories(CartridgeXR
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(CartridgeXR
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        CartridgeXR::Common
        CartridgeXR::VrScene
)

# Qt: enable auto MOC for QObject-derived classes.
set_target_properties(CartridgeXR PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Drop a windeployqt invocation post-build so the Qt DLLs land next to the .exe.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET CartridgeXR POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler --no-opengl-sw
                "$<TARGET_FILE:CartridgeXR>"
            COMMENT "Running windeployqt for CartridgeXR"
        )
    endif()
endif()
