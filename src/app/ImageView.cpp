#include "app/ImageView.hpp"

#include "core/PdfHandler.hpp"

#include <QBrush>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImageReader>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QTransform>
#include <QWheelEvent>

namespace {

constexpr double kZoomStep = 1.15;
constexpr double kZoomMin = 0.05;
constexpr int kPlaceholderWidth = 1200;
constexpr int kPlaceholderHeight = 800;

QPixmap createPlaceholderPixmap()
{
    QPixmap pixmap(kPlaceholderWidth, kPlaceholderHeight);
    pixmap.fill(QColor("#2f3640"));
    return pixmap;
}

} // namespace

namespace pictureviewer {

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(m_scene->addPixmap(createPlaceholderPixmap()))
    , m_zoomLevel(1.0)
    , m_manuallyZoomed(false)
    , m_pdfHandler(std::make_unique<PdfHandler>())
    , m_currentPdfPage(0)
{
    setScene(m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QBrush(QColor("#1e1e1e")));
    setFrameShape(QFrame::NoFrame);
    // Bilineární filtrování při škálování — bez něj jsou zmenšené obrázky
    // zubaté (nearest-neighbor) a zvětšené kostičkované.
    setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
    m_scene->setSceneRect(m_pixmapItem->boundingRect());

    // Debounce pro re-render PDF: renderovat až po ustálení zoomu,
    // ne při každém kroku kolečka myši.
    m_pdfRerenderTimer = new QTimer(this);
    m_pdfRerenderTimer->setSingleShot(true);
    m_pdfRerenderTimer->setInterval(250);
    connect(m_pdfRerenderTimer, &QTimer::timeout, this, &ImageView::rerenderPdfForZoom);

    fitToWindow();
}

ImageView::~ImageView() = default;

void ImageView::clearImage()
{
    m_pdfHandler->unload();
    m_pdfRerenderTimer->stop();
    m_pixmapItem->setPixmap(createPlaceholderPixmap());
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    setTransform(QTransform());
    fitToWindow();
}

bool ImageView::loadImage(const QString &path)
{
    // QImageReader s autoTransform aplikuje EXIF orientaci (fotky z mobilu);
    // prosté QPixmap(path) ji ignoruje.
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return setImage(reader.read());
}

bool ImageView::setImage(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }

    // Uvolnit případné PDF — jinak by PageDown/zoom re-render dál pracoval
    // se stránkami dokumentu, který už není zobrazen.
    m_pdfHandler->unload();
    m_pdfRerenderTimer->stop();

    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    setTransform(QTransform());
    fitToWindow();
    return true;
}

void ImageView::fitToWindow()
{
    if (m_pixmapItem->pixmap().isNull()) {
        return;
    }

    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    m_zoomLevel = transform().m11();
    m_manuallyZoomed = false;
}

void ImageView::resetZoom()
{
    setTransform(QTransform());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = true;
}

void ImageView::zoomIn()
{
    applyZoom(kZoomStep);
}

void ImageView::zoomOut()
{
    applyZoom(1.0 / kZoomStep);
}

void ImageView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        event->accept();
        return;
    case Qt::Key_Minus:
        zoomOut();
        event->accept();
        return;
    case Qt::Key_0:
    case Qt::Key_Space:
        resetZoom();
        event->accept();
        return;
    default:
        event->ignore();
        return;
    }
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);

    if (!m_pixmapItem->pixmap().isNull() && !m_manuallyZoomed) {
        fitToWindow();
        if (isPdfLoaded()) {
            m_pdfRerenderTimer->start();   // přizpůsobit rozlišení nové velikosti okna
        }
    }
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
        event->accept();
        return;
    }

    if (event->angleDelta().y() < 0) {
        zoomOut();
        event->accept();
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        resetZoom();
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void ImageView::applyZoom(double factor)
{
    const double nextZoom = m_zoomLevel * factor;
    if (nextZoom < kZoomMin) {
        return;
    }

    scale(factor, factor);
    m_zoomLevel = nextZoom;
    m_manuallyZoomed = true;

    if (isPdfLoaded()) {
        m_pdfRerenderTimer->start();
    }
}

bool ImageView::loadPdf(const QString &path)
{
    if (!m_pdfHandler->load(path)) {
        return false;
    }

    m_currentPdfPage = 0;
    renderPdfPage(0);
    return true;
}

void ImageView::renderPdfPage(int pageIndex)
{
    if (!m_pdfHandler->isLoaded() || pageIndex < 0 || pageIndex >= m_pdfHandler->pageCount()) {
        return;
    }

    // Rozlišení podle viewportu a DPI displeje (Retina = 2×), se zachováním
    // poměru stran stránky — fixní velikost by stránku deformovala a na
    // HiDPI displejích rozmazala.
    const qreal dpr = devicePixelRatioF();
    const int targetWidth = qMax(600, static_cast<int>(viewport()->width() * dpr));

    const QSizeF pageSize = m_pdfHandler->pageSize(pageIndex);
    QSize renderSize(targetWidth, qRound(targetWidth * 4.0 / 3.0));
    if (pageSize.isValid() && pageSize.width() > 0) {
        renderSize.setHeight(qRound(targetWidth * pageSize.height() / pageSize.width()));
    }

    const QImage image = m_pdfHandler->renderPage(pageIndex, renderSize);
    if (image.isNull()) {
        return;
    }

    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    setTransform(QTransform());
    fitToWindow();

    m_currentPdfPage = pageIndex;
    emit pdfPageChanged(pageIndex + 1, m_pdfHandler->pageCount());
}

void ImageView::rerenderPdfForZoom()
{
    if (!isPdfLoaded() || m_pixmapItem->pixmap().isNull()) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const qreal scale = transform().m11();
    const int currentWidth = m_pixmapItem->pixmap().width();

    // Cílová šířka: 1 pixel pixmapy = 1 fyzický pixel displeje při aktuálním zoomu
    qreal desiredWidth = qBound(600.0, currentWidth * scale * dpr, 8192.0);
    if (qAbs(desiredWidth / currentWidth - 1.0) < 0.2) {
        return;   // rozdíl pod 20 % — re-render by nepřinesl viditelné zlepšení
    }

    const QSizeF pageSize = m_pdfHandler->pageSize(m_currentPdfPage);
    if (!pageSize.isValid() || pageSize.width() <= 0) {
        return;
    }

    const QSize renderSize(static_cast<int>(desiredWidth),
                           qRound(desiredWidth * pageSize.height() / pageSize.width()));
    const QImage image = m_pdfHandler->renderPage(m_currentPdfPage, renderSize);
    if (image.isNull()) {
        return;
    }

    // Vyměnit pixmapu při zachování vizuální velikosti a středu pohledu
    const QPointF oldCenter = mapToScene(viewport()->rect().center());
    const qreal ratio = static_cast<qreal>(image.width()) / currentWidth;

    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());

    const qreal newScale = scale / ratio;
    setTransform(QTransform::fromScale(newScale, newScale));
    m_zoomLevel = newScale;
    centerOn(oldCenter * ratio);
}

bool ImageView::nextPage()
{
    if (!isPdfLoaded()) {
        return false;
    }

    if (m_currentPdfPage + 1 >= m_pdfHandler->pageCount()) {
        return false;
    }

    renderPdfPage(m_currentPdfPage + 1);
    return true;
}

bool ImageView::previousPage()
{
    if (!isPdfLoaded()) {
        return false;
    }

    if (m_currentPdfPage == 0) {
        return false;
    }

    renderPdfPage(m_currentPdfPage - 1);
    return true;
}

bool ImageView::isPdfLoaded() const
{
    return m_pdfHandler && m_pdfHandler->isLoaded();
}

int ImageView::currentPdfPage() const
{
    return m_pdfHandler ? m_currentPdfPage : -1;
}

int ImageView::pdfPageCount() const
{
    return m_pdfHandler ? m_pdfHandler->pageCount() : 0;
}

} // namespace pictureviewer
