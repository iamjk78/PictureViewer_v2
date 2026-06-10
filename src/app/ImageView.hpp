#pragma once

#include <QGraphicsView>
#include <memory>

class QGraphicsPixmapItem;
class QGraphicsScene;
class QImage;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QMovie;
class QSize;
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
    bool loadAnimation(const QString &path);   // přehraje animovaný GIF přes QMovie
    bool loadPdf(const QString &path);
    void fitToWindow();
    void resetZoom();
    void zoomIn();
    void zoomOut();
    void rotateLeft();    // otočit zobrazený obrázek o 90° proti směru hodin
    void rotateRight();   // otočit zobrazený obrázek o 90° po směru hodin

    // PDF navigation
    bool nextPage();
    bool previousPage();
    bool isPdfLoaded() const;
    int currentPdfPage() const;
    int pdfPageCount() const;
    void emitCurrentPdfPageInfo();   // re-emit current page info (for handlers connecting late)

    // Aktuálně zobrazený obrázek (full-res u obrázku, vyrenderovaná stránka u
    // PDF). Prázdný QImage, pokud není co zobrazit. Pro kopírování do schránky.
    QImage displayedImage() const;

signals:
    void pdfPageChanged(int page, int totalPages);
    void contextMenuRequested(const QPoint &globalPos);   // pravý klik nad pohledem
    // Procento zvětšení obrázku (100.0 = 1:1). Záporná hodnota = skrýt
    // indikátor (PDF nebo prázdný pohled — tam zoom % nedává smysl).
    void zoomChanged(double percent);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applyZoom(double factor);
    void rotateBy(int degrees);
    void stopAnimation();     // zastaví a uvolní případný běžící QMovie
    void emitZoomChanged();   // odešle aktuální zoom % (nebo -1 pro skrytí)
    void renderPdfPage(int pageIndex);
    void rerenderPdfForZoom();   // re-render po ustálení zoomu/resize (debounce)

    // Zobrazí dekódovaný obrázek a resetuje pohled (zoom 1:1 → fit).
    // Sdíleno mezi setImage() a renderPdfPage().
    void showImageReset(const QImage &image);
    // Výška renderu PDF stránky pro danou šířku se zachováním poměru stran;
    // fallback 4:3, pokud rozměr stránky není k dispozici. Sdíleno mezi
    // renderPdfPage() a rerenderPdfForZoom().
    QSize pdfRenderSize(int pageIndex, int targetWidth) const;

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    double m_zoomLevel;
    bool m_manuallyZoomed;
    bool m_hasContent = false;   // je zobrazen skutečný obrázek (ne placeholder)?
    std::unique_ptr<PdfHandler> m_pdfHandler;
    int m_currentPdfPage;
    QTimer *m_pdfRerenderTimer = nullptr;
    QMovie *m_movie = nullptr;   // aktivní animovaný GIF (jinak nullptr)
};

} // namespace pictureviewer
