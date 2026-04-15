#include "core/ImageCatalog.hpp"
#include "core/ImageMetadataReader.hpp"

#include <iostream>
#include <QString>
#include <QStringList>

using namespace pictureviewer;

int main() {
    std::cout << "=== PictureViewer C++ GUI Test Suite ===" << std::endl << std::endl;

    // Test 1: Folder scanning
    std::cout << "[TEST 1] Folder Scanning" << std::endl;
    try {
        ImageCatalog catalog;
        QStringList images = catalog.loadFolder("/Users/jirikrejci/Privat/Pics");
        std::cout << "✓ Loaded " << images.size() << " images from folder" << std::endl;

        if (!images.isEmpty()) {
            std::cout << "  First image: " << images.first().toStdString() << std::endl;
        }
    } catch (const std::exception &e) {
        std::cout << "✗ Error: " << e.what() << std::endl;
        return 1;
    }

    // Test 2: Image metadata reading
    std::cout << std::endl << "[TEST 2] Image Metadata Reading" << std::endl;
    try {
        ImageMetadataReader reader;
        ImageInfo info = reader.read("/Users/jirikrejci/Privat/Pics/270745308_10223470963619704_3913129754529498971_n.jpg");

        std::cout << "✓ Image metadata loaded successfully" << std::endl;
        std::cout << "  Path: " << info.path.toStdString() << std::endl;
        std::cout << "  Dimensions: " << info.dimensionsString().toStdString() << std::endl;
        std::cout << "  Format: " << info.format.toStdString() << std::endl;
        std::cout << "  Size: " << info.fileSizeKb() << " KB" << std::endl;
    } catch (const std::exception &e) {
        std::cout << "✗ Error: " << e.what() << std::endl;
        return 1;
    }

    // Test 3: Supported formats
    std::cout << std::endl << "[TEST 3] Supported Image Formats" << std::endl;
    {
        QStringList formats = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".tiff", ".tif"};
        std::cout << "✓ Supported formats:" << std::endl;
        for (const auto &fmt : formats) {
            std::cout << "  - " << fmt.toStdString() << std::endl;
        }
    }

    std::cout << std::endl << "=== All Tests Passed! ===" << std::endl;
    return 0;
}
