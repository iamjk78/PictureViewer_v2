#include "app/ImageView.hpp"

#include "core/PdfHandler.hpp"

#include <QBrush>
#include <QContextMenuEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImageReader>
#include <QKeyEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSize>
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
    stopAnimation();
    m_pdfHandler->unload();
    m_pdfRerenderTimer->stop();
    m_pixmapItem->setPixmap(createPlaceholderPixmap());
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    m_hasContent = false;
    setTransform(QTransform());
    fitToWindow();
}

void ImageView::emitZoomChanged()
{
    // Zoom % má smysl jen pro obrázky (100 % = 1:1 pixel). U PDF se rozlišení
    // renderu přizpůsobuje zoomu, takže transform scale neodpovídá vizuálnímu
    // zvětšení; prázdný pohled žádný zoom nemá. V obou případech pošleme -1,
    // což indikátor ve status baru skryje.
    const bool meaningful = m_hasContent && !isPdfLoaded();
    emit zoomChanged(meaningful ? m_zoomLevel * 100.0 : -1.0);
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

    // Uvolnit případné PDF/GIF — jinak by PageDown/zoom re-render dál pracoval
    // se stránkami dokumentu nebo by pod statickým obrázkem běžela animace.
    stopAnimation();
    m_pdfHandler->unload();
    m_pdfRerenderTimer->stop();

    showImageReset(image);
    return true;
}

bool ImageView::loadAnimation(const QString &path)
{
    stopAnimation();
    m_pdfHandler->unload();
    m_pdfRerenderTimer->stop();

    m_movie = new QMovie(path, QByteArray(), this);
    if (!m_movie->isValid()) {
        stopAnimation();
        return false;
    }

    // Každý snímek přemaluje pixmapu. Rozměry snímků jsou konstantní, takže
    // scénu a fit stačí nastavit jednou (níže z prvního snímku).
    connect(m_movie, &QMovie::frameChanged, this, [this] {
        m_pixmapItem->setPixmap(m_movie->currentPixmap());
    });

    m_movie->jumpToFrame(0);
    m_pixmapItem->setPixmap(m_movie->currentPixmap());
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_hasContent = true;
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    setTransform(QTransform());
    fitToWindow();

    m_movie->start();
    return true;
}

void ImageView::stopAnimation()
{
    if (m_movie != nullptr) {
        m_movie->stop();
        delete m_movie;   // má parent this; delete ho korektně odpojí
        m_movie = nullptr;
    }
}

void ImageView::showImageReset(const QImage &image)
{
    m_pixmapItem->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    m_hasContent = true;
    setTransform(QTransform());
    fitToWindow();
}

void ImageView::fitToWindow()
{
    if (m_pixmapItem->pixmap().isNull()) {
        return;
    }

    fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    m_zoomLevel = transform().m11();
    m_manuallyZoomed = false;
    emitZoomChanged();
}

void ImageView::resetZoom()
{
    setTransform(QTransform());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = true;
    emitZoomChanged();
}

void ImageView::zoomIn()
{
    applyZoom(kZoomStep);
}

void ImageView::zoomOut()
{
    applyZoom(1.0 / kZoomStep);
}

void ImageView::rotateBy(int degrees)
{
    // Otáčíme jen statické obrázky. U PDF by rotaci přepsal re-render při zoomu,
    // u GIFu další snímek — proto je tam neumožňujeme. Čistě vizuální (neukládá).
    if (!m_hasContent || isPdfLoaded() || m_movie != nullptr) {
        return;
    }
    const QPixmap current = m_pixmapItem->pixmap();
    if (current.isNull()) {
        return;
    }

    QTransform rotation;
    rotation.rotate(degrees);
    m_pixmapItem->setPixmap(current.transformed(rotation, Qt::SmoothTransformation));
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    setTransform(QTransform());
    fitToWindow();
}

void ImageView::rotateLeft()
{
    rotateBy(-90);
}

void ImageView::rotateRight()
{
    rotateBy(90);
}

QImage ImageView::displayedImage() const
{
    if (!m_hasContent && !isPdfLoaded()) {
        return {};
    }
    const QPixmap pixmap = m_pixmapItem->pixmap();
    return pixmap.isNull() ? QImage() : pixmap.toImage();
}

void ImageView::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->globalPos());
    event->accept();
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
    emitZoomChanged();

    if (isPdfLoaded()) {
        m_pdfRerenderTimer->start();
    }
}

bool ImageView::loadPdf(const QString &path)
{
    stopAnimation();
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

    const QImage image = m_pdfHandler->renderPage(pageIndex, pdfRenderSize(pageIndex, targetWidth));
    if (image.isNull()) {
        return;
    }

    showImageReset(image);

    m_currentPdfPage = pageIndex;
    emit pdfPageChanged(pageIndex + 1, m_pdfHandler->pageCount());
}

QSize ImageView::pdfRenderSize(int pageIndex, int targetWidth) const
{
    const QSizeF pageSize = m_pdfHandler->pageSize(pageIndex);
    if (pageSize.isValid() && pageSize.width() > 0) {
        return QSize(targetWidth, qRound(targetWidth * pageSize.height() / pageSize.width()));
    }
    // Fallback, když rozměr stránky není dostupný — výchozí poměr 4:3.
    return QSize(targetWidth, qRound(targetWidth * 4.0 / 3.0));
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

    const QSize renderSize = pdfRenderSize(m_currentPdfPage, static_cast<int>(desiredWidth));
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

void ImageView::emitCurrentPdfPageInfo()
{
    if (isPdfLoaded()) {
        emit pdfPageChanged(m_currentPdfPage + 1, m_pdfHandler->pageCount());
    }
}

} // namespace pictureviewer
