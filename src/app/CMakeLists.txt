set(APP_SOURCES
    Application.hpp
    Application.cpp
    CategoryManager.hpp
    CategoryManager.cpp
    CategoryDialogs.hpp
    CategoryDialogs.cpp
    ClipboardHelper.hpp
    HelpDialog.hpp
    HelpDialog.cpp
    ImageLoader.hpp
    ImageLoader.cpp
    MainWindow.hpp
    MainWindow.cpp
    MainWindow_FileOps.cpp
    MainWindow_Labels.cpp
    MainWindow_Layout.cpp
    MainWindow_Profiles.cpp
    MainWindow_Toolbar.cpp
    MetadataPanel.hpp
    MetadataPanel.cpp
    ProfileManager.hpp
    ProfileManager.cpp
    ImageView.hpp
    ImageView.cpp
    ScreenCapture.hpp
    ScreenCapture.cpp
    ThumbnailPanel.hpp
    ThumbnailPanel.cpp
    SlideshowController.hpp
    SlideshowController.cpp
    SettingsManager.hpp
    SettingsManager.cpp
    ThumbnailCacheManager.hpp
    ThumbnailCacheManager.cpp
    UpdateChecker.hpp
    UpdateChecker.cpp
    VideoPlayer.hpp
    VideoPlayer.cpp
)

if(APPLE)
    list(APPEND APP_SOURCES ClipboardHelper_mac.mm ScreenCapture_mac.mm)
    set_source_files_properties(ClipboardHelper_mac.mm ScreenCapture_mac.mm PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
else()
    list(APPEND APP_SOURCES ClipboardHelper.cpp)
endif()

add_library(picture_viewer_app OBJECT ${APP_SOURCES})
# Jediný zdroj pravdy pro verzi aplikace je project(VERSION …) v kořenovém
# CMakeLists.txt — Application.cpp ji čte z této definice.
target_compile_definitions(picture_viewer_app
    PRIVATE PV_APP_VERSION="${PROJECT_VERSION}"
)
target_include_directories(picture_viewer_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(picture_viewer_app
    PUBLIC
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)
