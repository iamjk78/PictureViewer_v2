#include "core/PdfHandler.hpp"

#include <QPdfDocument>
#include <QPdfPageRenderer>
#include <QDebug>
#include <QPainter>

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

    // Check if document loaded successfully - use pageCount() > 0 as indicator
    if (m_document->pageCount() <= 0) {
        qWarning() << "Failed to load PDF or PDF is empty:" << filePath;
        m_document->close();
        return false;
    }

    qDebug() << "PDF loaded successfully:" << filePath << "Pages:" << m_document->pageCount();
    return true;
}

bool PdfHandler::isLoaded() const
{
    return m_document && m_document->pageCount() > 0;
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

    // Qt6 renderToImage is synchronous; result has transparent background (ARGB32).
    // Composite onto white so the page looks correct against any viewer background.
    const QImage rendered = m_document->render(pageIndex, size);

    if (rendered.isNull()) {
        qWarning() << "Failed to render PDF page" << pageIndex;
        return QImage();
    }

    QImage image(rendered.size(), QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.drawImage(0, 0, rendered);
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
