set(WORKERS_SOURCES
    FolderScanWorker.hpp
    FolderScanWorker.cpp
    ThumbnailWorker.hpp
    ThumbnailWorker.cpp
)

add_library(picture_viewer_workers OBJECT ${WORKERS_SOURCES})
target_include_directories(picture_viewer_workers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(picture_viewer_workers
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Pdf
)
