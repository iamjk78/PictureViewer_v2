set(APP_SOURCES
    Application.hpp
    Application.cpp
    MainWindow.hpp
    MainWindow.cpp
    ImageView.hpp
    ImageView.cpp
    ThumbnailPanel.hpp
    ThumbnailPanel.cpp
    SlideshowController.hpp
    SlideshowController.cpp
    SettingsManager.hpp
    SettingsManager.cpp
)

add_library(picture_viewer_app OBJECT ${APP_SOURCES})
target_include_directories(picture_viewer_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(picture_viewer_app
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
