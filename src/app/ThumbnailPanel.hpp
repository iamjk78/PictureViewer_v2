#pragma once

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
    explicit ThumbnailPanel(QWidget *parent = nullptr);
    ~ThumbnailPanel() override;

    void loadImages(const QStringList &paths);
    void setCurrentIndex(int index);
    void removeImage(int index);

    // Cancel the running worker and disconnect all its signals.
    // Must be called before QThreadPool::waitForDone() so the worker cannot
    // emit into a widget that is about to be destroyed.
    void shutdown();

signals:
    void imageSelected(int index);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onThumbnailReady(int generation, int index, const QImage &image);
    void onThumbnailsFinished(int generation);

private:
    void startThumbnailLoader(const QStringList &paths);

    ThumbnailWorker *m_currentWorker;
    int m_generation;
    bool m_shuttingDown = false;
};

} // namespace pictureviewer
