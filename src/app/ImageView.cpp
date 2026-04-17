#include "app/ImageView.hpp"

#include <QBrush>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
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
{
    setScene(m_scene);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QBrush(QColor("#1e1e1e")));
    setFrameShape(QFrame::NoFrame);
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    fitToWindow();
}

void ImageView::clearImage()
{
    m_pixmapItem->setPixmap(createPlaceholderPixmap());
    m_scene->setSceneRect(m_pixmapItem->boundingRect());
    m_zoomLevel = 1.0;
    m_manuallyZoomed = false;
    setTransform(QTransform());
    fitToWindow();
}

bool ImageView::loadImage(const QString &path)
{
    const QPixmap pixmap(path);
    if (pixmap.isNull()) {
        return false;
    }

    m_pixmapItem->setPixmap(pixmap);
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
}

} // namespace pictureviewer
