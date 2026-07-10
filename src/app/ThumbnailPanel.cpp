#include "app/ThumbnailPanel.hpp"

#include "core/ImageFormats.hpp"
#include "workers/ThumbnailWorker.hpp"

#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QSize>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QThreadPool>
#include <algorithm>

namespace {

constexpr int kThumbnailSize = 96;

class CenteredIconDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QVariant data = index.data(Qt::DecorationRole);
        if (data.isNull()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QIcon icon = qvariant_cast<QIcon>(data);
        // Požádat o pixmapu v menší velikosti (72px) a pak ji sami škálujeme
        // Tím zabráníme Qt v deformaci aspect ratio
        const int maxSize = option.decorationSize.width();
        QPixmap pixmap = icon.pixmap(maxSize);

        if (pixmap.isNull()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        painter->fillRect(option.rect, option.palette.base());
        if (option.state & QStyle::State_Selected) {
            painter->fillRect(option.rect, option.palette.highlight());
        }

        if (pixmap.width() > maxSize || pixmap.height() > maxSize) {
            pixmap = pixmap.scaledToWidth(maxSize, Qt::SmoothTransformation);
            if (pixmap.height() > maxSize) {
                pixmap = pixmap.scaledToHeight(maxSize, Qt::SmoothTransformation);
            }
        }

        // Vycentrovat pixmapu v buňce
        QRect iconRect(QPoint(0, 0), pixmap.size());
        iconRect.moveCenter(option.rect.center());
        painter->drawPixmap(iconRect, pixmap);
    }
};

} // namespace

namespace pictureviewer {

ThumbnailPanel::ThumbnailPanel(QWidget *parent)
    : QListWidget(parent)
    , m_currentWorker(nullptr)
    , m_generation(0)
{
    setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    setSortingEnabled(false);
    setMovement(QListWidget::Static);
    // Ctrl/Shift+klik umožňuje vybrat více položek pro hromadný přesun.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setUniformItemSizes(true);  // Zpět povoleno - size hint zabraňuje přesahu
    setItemDelegate(new CenteredIconDelegate(this));
    setStyleSheet(
        "QListWidget { background-color: #2b2b2b; border: none; }"
        "QListWidget::item:selected { background-color: #0d6efd; }"
    );
    setDisplayMode(DisplayMode::Vertical);
    connect(this, &QListWidget::itemClicked, this, &ThumbnailPanel::onItemClicked);
}

void ThumbnailPanel::setDiskCache(bool enabled, const QString &cacheDir)
{
    m_diskCacheEnabled = enabled;
    m_diskCacheDir = cacheDir;
}

void ThumbnailPanel::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;

    // Zrušit případné pevné rozměry z předchozího režimu
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    switch (mode) {
    case DisplayMode::Vertical:
        setViewMode(QListWidget::ListMode);
        setFlow(QListWidget::TopToBottom);
        setWrapping(false);
        setResizeMode(QListWidget::Adjust);
        setSpacing(8);
        // Šířka je volná — uživatel ji táhne za pravý okraj docku.
        // Miniatury se přizpůsobí v resizeEvent.
        setMinimumWidth(32 + 24);
        setMaximumWidth(256 + 24);
        break;
    case DisplayMode::Horizontal:
        setViewMode(QListWidget::ListMode);
        setFlow(QListWidget::LeftToRight);
        setWrapping(false);
        setResizeMode(QListWidget::Adjust);
        setSpacing(8);
        setFixedHeight(kThumbnailSize + 24);
        break;
    case DisplayMode::Grid:
        setViewMode(QListWidget::IconMode);
        setFlow(QListWidget::LeftToRight);
        setWrapping(true);
        setResizeMode(QListWidget::Adjust);
        setSpacing(12);
        break;
    }
}

ThumbnailPanel::~ThumbnailPanel()
{
    // MainWindow::~MainWindow() calls shutdown() + waitForDone() before Qt
    // destroys child widgets, so by the time we reach here the worker is
    // guaranteed to have stopped. The call below is a defensive fallback for
    // cases where ThumbnailPanel is used outside of MainWindow.
    shutdown();
}

void ThumbnailPanel::shutdown()
{
    m_shuttingDown = true;

    if (m_currentWorker == nullptr) {
        return;
    }
    m_currentWorker->cancel();
    // Sever every signal from the worker to this widget so it cannot call
    // back into us after we return (the worker may still be running in the
    // thread pool until waitForDone() is called by the owner).
    disconnect(m_currentWorker, nullptr, this, nullptr);
    m_currentWorker = nullptr;
}

void ThumbnailPanel::loadImages(const QStringList &paths)
{
    if (m_shuttingDown) {
        return;
    }

    if (m_currentWorker != nullptr) {
        m_currentWorker->cancel();
        disconnect(m_currentWorker, nullptr, this, nullptr);
        m_currentWorker = nullptr;
    }

    ++m_generation;
    clear();
    m_pathToItem.clear();

    QStringList imagePaths;
    for (const QString &path : paths) {
        auto *item = new QListWidgetItem();
        item->setToolTip(path.section('/', -1));
        item->setData(Qt::UserRole, path);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(m_thumbSize, m_thumbSize));

        const QString suffix = QStringLiteral(".") + QFileInfo(path).suffix();
        if (isVideoFile(suffix)) {
            // Video placeholder: standardní ikona přehrávání
            item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        } else {
            imagePaths.append(path);
        }

        addItem(item);
        m_pathToItem[path] = item;
    }

    startThumbnailLoader(imagePaths);
}

void ThumbnailPanel::setCurrentIndex(int index)
{
    if (index >= 0 && index < count()) {
        setCurrentRow(index);
        scrollToItem(item(index));
    }
}

QIcon ThumbnailPanel::iconAt(int index) const
{
    if (index >= 0 && index < count()) {
        return item(index)->icon();
    }
    return {};
}

void ThumbnailPanel::removeImage(int index)
{
    if (index >= 0 && index < count()) {
        const QString path = item(index)->data(Qt::UserRole).toString();
        m_pathToItem.remove(path);
        delete takeItem(index);
    }
}

void ThumbnailPanel::updateImagePath(const QString &oldPath, const QString &newPath)
{
    auto it = m_pathToItem.find(oldPath);
    if (it != m_pathToItem.end()) {
        QListWidgetItem *target = *it;
        target->setData(Qt::UserRole, newPath);
        target->setToolTip(newPath.section('/', -1));
        m_pathToItem.remove(oldPath);
        m_pathToItem[newPath] = target;
    }
}

void ThumbnailPanel::keyPressEvent(QKeyEvent *event)
{
    // Let Space and 0 propagate to MainWindow for zoom reset instead of
    // being consumed by QListWidget's item-activation behavior.
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_0) {
        event->ignore();
        return;
    }
    QListWidget::keyPressEvent(event);
}

void ThumbnailPanel::onItemClicked(QListWidgetItem *item)
{
    // Ctrl/Shift+klik jen rozšiřuje výběr (pro hromadný přesun) — nenavigovat,
    // aby se neresetoval zbytek výběru přes setCurrentIndex() z showImage().
    if (QGuiApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
        return;
    }
    emit imageSelected(row(item));
}

QList<int> ThumbnailPanel::selectedIndices() const
{
    QList<int> result;
    for (QListWidgetItem *item : selectedItems()) {
        result.append(row(item));
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ThumbnailPanel::onThumbnailReady(int generation, const QString &path, const QImage &image)
{
    if (generation != m_generation || path.isEmpty() || image.isNull()) {
        return;
    }

    auto it = m_pathToItem.find(path);
    if (it != m_pathToItem.end()) {
        (*it)->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

void ThumbnailPanel::onThumbnailsFinished(int generation)
{
    if (generation != m_generation || m_currentWorker == nullptr) {
        return;
    }

    m_currentWorker = nullptr;
}

void ThumbnailPanel::setVideoThumbnail(int generation, const QString &path, const QImage &image)
{
    if (generation != m_generation || image.isNull()) {
        return;
    }
    auto it = m_pathToItem.find(path);
    if (it != m_pathToItem.end()) {
        (*it)->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

void ThumbnailPanel::startThumbnailLoader(const QStringList &paths)
{
    if (m_shuttingDown) {
        return;
    }

    // Parent must be nullptr — this object is managed by the thread pool
    // (deleted via deleteLater on workerFinished). A Qt parent would create a
    // second deletion path and cause a double-free crash.
    auto *worker = new ThumbnailWorker(paths, m_generation,
                                       m_diskCacheEnabled, m_diskCacheDir, nullptr);
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

void ThumbnailPanel::applyThumbSize(int size)
{
    m_thumbSize = size;
    setIconSize(QSize(size, size));
    for (int i = 0; i < count(); ++i) {
        item(i)->setSizeHint(QSize(size, size));
    }
}

void ThumbnailPanel::resizeEvent(QResizeEvent *event)
{
    QListWidget::resizeEvent(event);
    if (m_displayMode == DisplayMode::Vertical) {
        const int newSize = qBound(32, width() - 24, 256);
        if (newSize != m_thumbSize) {
            applyThumbSize(newSize);
        }
    }
}

QSize ThumbnailPanel::sizeHint() const
{
    if (m_displayMode == DisplayMode::Vertical) {
        return QSize(m_thumbSize + 24, 200);
    }
    return QListWidget::sizeHint();
}

} // namespace pictureviewer
