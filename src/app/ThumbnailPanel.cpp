#include "app/ThumbnailPanel.hpp"

#include "workers/ThumbnailWorker.hpp"

#include <QIcon>
#include <QImage>
#include <QListWidgetItem>
#include <QPixmap>
#include <QSize>
#include <QThreadPool>

namespace {

constexpr int kThumbnailSize = 96;

} // namespace

namespace pictureviewer {

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QListWidget(parent)
    , m_currentWorker(nullptr)
    , m_generation(0)
{
    setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    setFixedWidth(kThumbnailSize + 24);
    setSpacing(4);
    setViewMode(QListWidget::ListMode);
    setFlow(QListWidget::TopToBottom);
    setResizeMode(QListWidget::Adjust);
    setSortingEnabled(false);
    setMovement(QListWidget::Static);
    setStyleSheet(
        "QListWidget { background-color: #2b2b2b; border: none; }"
        "QListWidget::item:selected { background-color: #0d6efd; }"
    );
    connect(this, &QListWidget::itemClicked, this, &ThumbnailPanel::onItemClicked);
}

ThumbnailPanel::~ThumbnailPanel()
{
    if (m_currentWorker != nullptr) {
        m_currentWorker->cancel();
    }
}

void ThumbnailPanel::loadImages(const QStringList &paths)
{
    if (m_currentWorker != nullptr) {
        m_currentWorker->cancel();
        m_currentWorker = nullptr;
    }

    ++m_generation;
    clear();

    for (const QString &path : paths) {
        auto *item = new QListWidgetItem();
        item->setToolTip(path.section('/', -1));
        item->setData(Qt::UserRole, path);
        addItem(item);
    }

    startThumbnailLoader(paths);
}

void ThumbnailPanel::setCurrentIndex(int index)
{
    if (index >= 0 && index < count()) {
        setCurrentRow(index);
        scrollToItem(item(index));
    }
}

void ThumbnailPanel::onItemClicked(QListWidgetItem *item)
{
    emit imageSelected(row(item));
}

void ThumbnailPanel::onThumbnailReady(int generation, int index, const QImage &image)
{
    if (generation != m_generation || index < 0 || index >= count()) {
        return;
    }

    if (!image.isNull()) {
        item(index)->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

void ThumbnailPanel::onThumbnailsFinished(int generation)
{
    if (generation != m_generation || m_currentWorker == nullptr) {
        return;
    }

    m_currentWorker = nullptr;
}

void ThumbnailPanel::startThumbnailLoader(const QStringList &paths)
{
    auto *worker = new ThumbnailWorker(paths, m_generation, this);
    connect(worker, &ThumbnailWorker::thumbnailReady, this, &ThumbnailPanel::onThumbnailReady);
    connect(worker, &ThumbnailWorker::workerFinished, this, &ThumbnailPanel::onThumbnailsFinished);
    connect(worker, &ThumbnailWorker::workerFinished, worker, &ThumbnailWorker::deleteLater);
    connect(worker, &ThumbnailWorker::workerError, this, [this](int generation, const QString &error) {
        if (generation == m_generation && count() > 0) {
            item(0)->setToolTip(error);
        }
    });
    m_currentWorker = worker;
    QThreadPool::globalInstance()->start(worker);
}

} // namespace pictureviewer
