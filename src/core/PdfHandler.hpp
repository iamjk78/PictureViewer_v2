#pragma once

#include <QString>
#include <QImage>
#include <memory>

class QPdfDocument;

namespace pictureviewer {

class PdfHandler
{
public:
    PdfHandler();
    ~PdfHandler();

    // Load PDF file, returns true on success
    bool load(const QString &filePath);

    // Check if a PDF is currently loaded
    bool isLoaded() const;

    // Get total page count
    int pageCount() const;

    // Render a specific page at given size
    // Returns null QImage if page is invalid or rendering fails
    QImage renderPage(int pageIndex, const QSize &size);

    // Get the native size of a page (in points)
    QSizeF pageSize(int pageIndex) const;

    // Clear the loaded document
    void unload();

private:
    std::unique_ptr<QPdfDocument> m_document;
};

} // namespace pictureviewer
