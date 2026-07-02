#pragma once

#include <QHash>
#include <QListWidget>
#include <QStringList>

class QImage;
class QListWidgetItem;

namespace pictureviewer {

class ThumbnailWorker;

class ThumbnailPanel : public QListWidget
{
    Q_OBJECT

public:
    // Vertical   – sloupec v levém docku (klasické rozložení)
    // Horizontal – filmový pás ve spodním docku
    // Grid       – mřížka přes celé okno (režim Galerie)
    enum class DisplayMode { Vertical, Horizontal, Grid };

    explicit ThumbnailPanel(QWidget *parent = nullptr);
    ~ThumbnailPanel() override;

    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    // Konfigurace diskové cache miniatur; projeví se při dalším loadImages().
    void setDiskCache(bool enabled, const QString &cacheDir);

    void loadImages(const QStringList &paths);
    void setCurrentIndex(int index);
    void removeImage(int index);
    void updateImagePath(const QString &oldPath, const QString &newPath);
    QIcon iconAt(int index) const;   // náhled pro placeholder při async načítání

    // Aktualizuje miniaturu videa (voláno z VideoThumbnailWorker přes MainWindow).
    void setVideoThumbnail(const QString &path, const QImage &image);

    // Cancel the running worker and disconnect all its signals.
    // Must be called before QThreadPool::waitForDone() so the worker cannot
    // emit into a widget that is about to be destroyed.
    void shutdown();

    // Aktuální velikost miniatury v pixelech (= šířka docku − 24).
    // V Horizontal/Grid režimu vrací výchozí hodnotu.
    int thumbSize() const { return m_thumbSize; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;

signals:
    void imageSelected(int index);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onThumbnailReady(int generation, const QString &path, const QImage &image);
    void onThumbnailsFinished(int generation);

private:
    void startThumbnailLoader(const QStringList &paths);
    void applyThumbSize(int size);

    ThumbnailWorker *m_currentWorker;
    int m_generation;
    bool m_shuttingDown = false;
    DisplayMode m_displayMode = DisplayMode::Vertical;
    int m_thumbSize = 96;
    bool m_diskCacheEnabled = true;
    QString m_diskCacheDir;
    // O(1) lookup by path. Stores item pointers (stable across index shifts on
    // removal), NOT indices — indices become stale when items shift after a
    // mid-list removeImage().
    QHash<QString, QListWidgetItem *> m_pathToItem;
};

} // namespace pictureviewer
