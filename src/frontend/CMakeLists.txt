qt_add_executable(N64XR WIN32
    src/main.cpp
    src/MainWindow.cpp
    include/MainWindow.h
)

target_include_directories(N64XR
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(N64XR
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        N64XR::Common
        N64XR::VrScene
)

# Qt: enable auto MOC for QObject-derived classes.
set_target_properties(N64XR PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Drop a windeployqt invocation post-build so the Qt DLLs land next to the .exe.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET N64XR POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler --no-opengl-sw
                "$<TARGET_FILE:N64XR>"
            COMMENT "Running windeployqt for N64XR"
        )
    endif()
endif()
