#pragma once

#include <QGraphicsView>
#include <memory>

class QGraphicsPixmapItem;
class QGraphicsScene;
class QKeyEvent;
class QMouseEvent;
class QString;
class QResizeEvent;
class QTimer;
class QWheelEvent;

namespace pictureviewer {

class PdfHandler;

class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);
    ~ImageView();

    void clearImage();
    bool loadImage(const QString &path);
    bool setImage(const QImage &image);   // zobrazí už dekódovaný obrázek (cache/worker)
    bool loadPdf(const QString &path);
    void fitToWindow();
    void resetZoom();
    void zoomIn();
    void zoomOut();

    // PDF navigation
    bool nextPage();
    bool previousPage();
    bool isPdfLoaded() const;
    int currentPdfPage() const;
    int pdfPageCount() const;
    void emitCurrentPdfPageInfo();   // re-emit current page info (for handlers connecting late)

signals:
    void pdfPageChanged(int page, int totalPages);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void applyZoom(double factor);
    void renderPdfPage(int pageIndex);
    void rerenderPdfForZoom();   // re-render po ustálení zoomu/resize (debounce)

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    double m_zoomLevel;
    bool m_manuallyZoomed;
    std::unique_ptr<PdfHandler> m_pdfHandler;
    int m_currentPdfPage;
    QTimer *m_pdfRerenderTimer = nullptr;
};

} // namespace pictureviewer
