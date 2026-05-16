#include "core/PdfHandler.hpp"

#include <QPdfDocument>
#include <QPdfPageRenderer>
#include <QDebug>

namespace pictureviewer {

PdfHandler::PdfHandler()
    : m_document(std::make_unique<QPdfDocument>())
{
}

PdfHandler::~PdfHandler() = default;

bool PdfHandler::load(const QString &filePath)
{
    if (!m_document) {
        m_document = std::make_unique<QPdfDocument>();
    }

    m_document->load(filePath);

    if (m_document->status() != QPdfDocument::Ready) {
        qWarning() << "Failed to load PDF:" << filePath;
        m_document->close();
        return false;
    }

    qDebug() << "PDF loaded successfully:" << filePath << "Pages:" << m_document->pageCount();
    return true;
}

bool PdfHandler::isLoaded() const
{
    return m_document && m_document->status() == QPdfDocument::Ready;
}

int PdfHandler::pageCount() const
{
    if (!isLoaded()) {
        return 0;
    }
    return m_document->pageCount();
}

QImage PdfHandler::renderPage(int pageIndex, const QSize &size)
{
    if (!isLoaded() || pageIndex < 0 || pageIndex >= pageCount()) {
        return QImage();
    }

    QPdfPageRenderer renderer;
    const QImage image = renderer.render(m_document.get(), pageIndex, size);

    if (image.isNull()) {
        qWarning() << "Failed to render PDF page" << pageIndex;
        return QImage();
    }

    return image;
}

QSizeF PdfHandler::pageSize(int pageIndex) const
{
    if (!isLoaded() || pageIndex < 0 || pageIndex >= pageCount()) {
        return QSizeF();
    }

    return m_document->pagePointSize(pageIndex);
}

void PdfHandler::unload()
{
    if (m_document) {
        m_document->close();
    }
}

} // namespace pictureviewer
