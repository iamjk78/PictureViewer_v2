#pragma once

#include <QGraphicsView>

class QGraphicsPixmapItem;
class QGraphicsScene;
class QKeyEvent;
class QMouseEvent;
class QString;
class QResizeEvent;
class QWheelEvent;

namespace pictureviewer {

class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);

    void clearImage();
    bool loadImage(const QString &path);
    void fitToWindow();
    void resetZoom();
    void zoomIn();
    void zoomOut();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void applyZoom(double factor);

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    double m_zoomLevel;
    bool m_manuallyZoomed;
};

} // namespace pictureviewer
