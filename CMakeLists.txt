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
)

target_link_libraries(PictureViewer
    PRIVATE
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
)
