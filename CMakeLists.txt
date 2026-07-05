cmake_minimum_required(VERSION 3.21)

project(PictureViewer VERSION 0.17 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Varování překladače — zachytí nepoužité proměnné, znaménková porovnání apod.
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Enable Qt's automatic MOC, RCC, and UIC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Concurrent Gui Widgets Network Pdf Sql Multimedia MultimediaWidgets)

# Build subdirectories for core, app, and workers
add_subdirectory(src)

# Jednotkové testy (volitelné — zapni -DBUILD_TESTING=ON, výchozí ON).
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Main executable combining all modules
set(_app_icon_sources)
if(WIN32)
    set(_app_icon_sources resources/PictureViewer.rc)
endif()

qt_add_executable(PictureViewer
    src/main.cpp
    resources/resources.qrc
    ${_app_icon_sources}
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
        Qt6::Pdf
)

target_include_directories(PictureViewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-agnostic target properties
set_target_properties(PictureViewer PROPERTIES
    WIN32_EXECUTABLE TRUE      # hides the console window on Windows
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "eye_icon.ico"
    MACOSX_BUNDLE_BUNDLE_NAME "PictureViewer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "cz.jirikrejci.PictureViewer"
)

# macOS-specific settings
if(APPLE)
    # Generate Info.plist from template (macOS only — not needed on Windows)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PictureViewerInfo.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/PictureViewerInfo.plist"
        @ONLY
    )

    set_target_properties(PictureViewer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/PictureViewerInfo.plist"
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

# Copy icon to the macOS bundle's Resources directory
if(APPLE)
    add_custom_command(TARGET PictureViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/eye_icon.ico"
            "$<TARGET_BUNDLE_CONTENT_DIR:PictureViewer>/Resources/eye_icon.ico"
        COMMENT "Copying icon to macOS bundle"
    )

    # Code sign with entitlements for file access
    add_custom_command(TARGET PictureViewer POST_BUILD
        COMMAND codesign -s - --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/cmake/entitlements.plist" -f --deep "$<TARGET_BUNDLE_DIR:PictureViewer>"
        COMMENT "Code signing macOS bundle with file access entitlements"
    )
endif()
