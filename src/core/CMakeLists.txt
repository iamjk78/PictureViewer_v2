set(CORE_SOURCES
    ImageInfo.hpp
    ImageFormats.hpp
    ImageMetadataReader.hpp
    ImageMetadataReader.cpp
    ImageCatalog.hpp
    ImageCatalog.cpp
)

add_library(picture_viewer_core INTERFACE ${CORE_SOURCES})
target_include_directories(picture_viewer_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
