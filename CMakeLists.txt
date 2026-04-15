cmake_minimum_required(VERSION 3.21)

project(PictureViewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic MOC, RCC, and UIC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Build subdirectories for core, app, and workers
add_subdirectory(src)

# Main executable combining all modules
qt_add_executable(PictureViewer
    src/main.cpp
    resources/resources.qrc
    $<TARGET_OBJECTS:picture_viewer_app>
    $<TARGET_OBJECTS:picture_viewer_workers>
)

target_link_libraries(PictureViewer
    PRIVATE
        picture_viewer_core
        picture_viewer_app
        picture_viewer_workers
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_include_directories(PictureViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(PictureViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "eye_icon.ico"
    MACOSX_BUNDLE_BUNDLE_NAME "PictureViewer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# Copy icon to the macOS bundle's Resources directory
if(APPLE)
    add_custom_command(TARGET PictureViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/eye_icon.ico"
            "$<TARGET_BUNDLE_CONTENT_DIR:PictureViewer>/Resources/eye_icon.ico"
        COMMENT "Copying icon to macOS bundle"
    )
endif()
