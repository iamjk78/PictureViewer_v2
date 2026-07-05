set(CORE_SOURCES
    ImageInfo.hpp
    ImageFormats.hpp
    ImageMetadataReader.hpp
    ImageMetadataReader.cpp
    ImageCatalog.hpp
    ImageCatalog.cpp
    FolderNavigator.hpp
    FolderNavigator.cpp
    PdfHandler.hpp
    PdfHandler.cpp
)

add_library(picture_viewer_core STATIC ${CORE_SOURCES})
target_include_directories(picture_viewer_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(picture_viewer_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Pdf
)
