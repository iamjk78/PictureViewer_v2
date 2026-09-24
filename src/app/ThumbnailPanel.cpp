#include "app/ThumbnailPanel.hpp"
#include "core/DiagLog.hpp"

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
#include <QScrollBar>
#include <QShowEvent>
#include <QThreadPool>
#include <QTimer>
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

    m_playIcon = style()->standardIcon(QStyle::SP_MediaPlay);
    m_thumbPool.setMaxThreadCount(kThumbnailThreads);
    m_claims = QSharedPointer<ThumbnailClaims>::create();

    // Zahřívání cache: jedno vlákno, nízká priorita, ať nebere výkon ani
    // pásmo tomu, co uživatel právě dělá.
    m_warmPool.setMaxThreadCount(1);
    m_warmPool.setThreadPriority(QThread::LowPriority);
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    connect(m_idleTimer, &QTimer::timeout, this, [this] {
        m_idleReady = true;
        maybeStartWarmup();
    });
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &ThumbnailPanel::updateWarmupProgress);
    m_videoIdleTimer = new QTimer(this);
    m_videoIdleTimer->setSingleShot(true);
    connect(m_videoIdleTimer, &QTimer::timeout, this, [this] {
        m_videoIdleReady = true;
        maybeStartVideoWarmup();
    });

    // Přepočet potřebných miniatur se pouští s krátkým zpožděním a nejvýše
    // jednou za interval (viz scheduleThumbnailUpdate()) — při plynulém
    // posouvání tak vzniká práce průběžně, ale ne při každém pixelu.
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &ThumbnailPanel::updateWantedThumbnails);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleThumbnailUpdate(); });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { scheduleThumbnailUpdate(); });
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
    m_updateTimer->stop();
    m_idleTimer->stop();
    m_videoIdleTimer->stop();
    m_pendingThumbs.clear();
    cancelActiveWorkers();
}

void ThumbnailPanel::cancelActiveWorkers()
{
    for (ThumbnailWorker *worker : std::as_const(m_activeWorkers)) {
        worker->cancel();
        // Odpojit každý signál workeru do tohoto widgetu, ať se po návratu
        // nemůže dovolat zpět (worker ještě chvíli běží ve vlákně z fondu,
        // dokud majitel nezavolá waitForDone()). Připojení deleteLater()
        // worker→worker zůstává.
        disconnect(worker, nullptr, this, nullptr);
    }
    m_activeWorkers.clear();

    if (m_warmWorker != nullptr) {
        m_warmWorker->cancel();
        disconnect(m_warmWorker, nullptr, this, nullptr);
        m_warmWorker = nullptr;
    }
    m_videoWarmupActive = false;
}

void ThumbnailPanel::loadImages(const QStringList &paths)
{
    if (m_shuttingDown) {
        return;
    }

    cancelActiveWorkers();
    m_pendingThumbs.clear();
    m_claims = QSharedPointer<ThumbnailClaims>::create();   // nový seznam = nová evidence
    m_lastWantedVideos.clear();
    m_visibleVideos.clear();
    m_warmVideoTail.clear();
    m_warmPrepared = false;
    m_idleReady = false;
    m_videoIdleReady = false;
    m_videoTailPrepared = false;

    ++m_generation;
    clear();
    m_pathToIndex.clear();
    m_imageItemCount = 0;
    m_videoItemCount = 0;
    m_warmSkipped = 0;
    m_videosHandled.clear();
    m_progressWasActive = false;
    diag::thumb().reset();
    m_doneCount = 0;
    m_statsTimer.start();

    addImageItems(paths);
    updateWarmupProgress();   // nový seznam → ukazatel průběhu se schová

    // Miniatury nejsou potřeba hned pro všechny — jen pro viditelné (viz
    // updateWantedThumbnails()). Rozložení se ustálí až v event loopě, proto
    // odloženě.
    scheduleThumbnailUpdate();
}

void ThumbnailPanel::addImageItems(const QStringList &paths)
{
    // Bez překreslování po každé položce — u tisíců souborů by to trvalo sekundy.
    setUpdatesEnabled(false);
    m_pathToIndex.reserve(m_pathToIndex.size() + static_cast<qsizetype>(paths.size()));
    for (const QString &path : paths) {
        auto *item = new QListWidgetItem();
        item->setToolTip(path.section('/', -1));
        item->setData(Qt::UserRole, path);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(m_thumbSize, m_thumbSize));

        const QString suffix = QStringLiteral(".") + QFileInfo(path).suffix();
        if (isVideoFile(suffix)) {
            item->setIcon(m_playIcon);   // placeholder videa
            ++m_videoItemCount;
        } else {
            ++m_imageItemCount;
        }

        addItem(item);
        m_pathToIndex[path] = QPersistentModelIndex(indexFromItem(item));
    }
    setUpdatesEnabled(true);
}

void ThumbnailPanel::countItem(const QString &path, int delta)
{
    if (isVideoFile(QStringLiteral(".") + QFileInfo(path).suffix())) {
        m_videoItemCount = qMax(0, m_videoItemCount + delta);
    } else {
        m_imageItemCount = qMax(0, m_imageItemCount + delta);
    }
}

void ThumbnailPanel::setProgressTimingForTesting(int showDelayMs, int intervalMs)
{
    m_progressShowDelayMs = showDelayMs;
    m_progressIntervalMs = intervalMs;
}

void ThumbnailPanel::noteVideoHandled(int generation, const QString &path)
{
    if (generation != m_generation || path.isEmpty()) {
        return;
    }
    m_videosHandled.insert(path);
}

void ThumbnailPanel::updateWarmupProgress()
{
    WarmupProgress p;
    const bool cacheOn = m_diskCacheEnabled && !m_diskCacheDir.isEmpty();
    if (cacheOn && count() > 0 && !m_shuttingDown) {
        p.imagesActive = m_warmWorker != nullptr;
        p.imagesTotal = m_imageItemCount;
        if (p.imagesActive) {
            p.imagesDone = qMin(p.imagesTotal, m_warmSkipped + m_warmWorker->processedCount());
        }
        p.videosTotal = m_videoItemCount;
        p.videosDone = qMin(p.videosTotal, static_cast<int>(m_videosHandled.size()));
        p.videosActive = m_videoTailPrepared && p.videosDone < p.videosTotal;

        const bool imagesRunning = p.imagesActive && m_idleReady && !m_viewerBusy && !m_scanRunning;
        const bool videosRunning = p.videosActive && m_videoWarmupActive;
        p.paused = !imagesRunning && !videosRunning;
    }

    const bool active = p.imagesActive || p.videosActive;
    if (active && !m_progressWasActive) {
        m_progressActiveFor.start();
    }
    m_progressWasActive = active;
    // Krátké zahřívání (typicky vše už v cache) se neukazuje, ať ukazatel neblikne.
    p.visible = active && m_progressActiveFor.elapsed() >= m_progressShowDelayMs;
    if (!p.visible) {
        p = WarmupProgress{};
    }
    if (!active) {
        m_progressTimer->stop();
    }
    if (p != m_lastProgress) {
        m_lastProgress = p;
        emit warmupProgressChanged(p);
    }
}

void ThumbnailPanel::appendImages(const QStringList &paths)
{
    if (m_shuttingDown || paths.isEmpty()) {
        return;
    }
    addImageItems(paths);
    scheduleThumbnailUpdate();
}

void ThumbnailPanel::scheduleThumbnailUpdate()
{
    if (m_shuttingDown || m_updateTimer == nullptr) {
        return;
    }
    noteActivity();
    // Nerestartovat běžící timer: při souvislém posouvání se přepočet pustí
    // pravidelně každých ~100 ms, a poslední událost tak vždy dostane svůj
    // vlastní přepočet.
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
}

void ThumbnailPanel::updateWantedThumbnails()
{
    if (m_shuttingDown) {
        return;
    }

    struct Candidate {
        int row;
        bool visible;
    };
    QList<Candidate> images;
    QList<Candidate> videos;

    // Skrytý panel (dock zavřený, Galerie zrovna ukazuje obrázek…) nic
    // nepotřebuje — miniatury se dogenerují, až se zase zobrazí (showEvent).
    if (viewport()->isVisible() && count() > 0) {
        const QRect vp = viewport()->rect();
        // Okolí jen ve směru posouvání: ve sloupci/mřížce nahoru a dolů, ve
        // filmovém pásu doleva a doprava. Půl obrazovky stačí, aby při
        // pomalém posunu bylo co ukázat, a zbytečně se nečte víc.
        const bool horizontalStrip = displayMode() == DisplayMode::Horizontal;
        const QRect area = horizontalStrip
            ? vp.adjusted(-vp.width() / 2, 0, vp.width() / 2, 0)
            : vp.adjusted(0, -vp.height() / 2, 0, vp.height() / 2);

        for (int row = 0; row < count(); ++row) {
            QListWidgetItem *it = item(row);
            const QRect r = visualItemRect(it);
            if (!r.intersects(area)) {
                continue;
            }
            const QString path = it->data(Qt::UserRole).toString();
            const bool isVideo = isVideoFile(QStringLiteral(".") + QFileInfo(path).suffix());
            (isVideo ? videos : images).append({row, r.intersects(vp)});
        }
    }

    // Střed viditelné oblasti — od něj se řadí priorita (nejdřív to, na co se
    // uživatel právě dívá).
    int minVisible = -1;
    int maxVisible = -1;
    for (const QList<Candidate> *list : {&images, &videos}) {
        for (const Candidate &c : *list) {
            if (c.visible) {
                minVisible = minVisible < 0 ? c.row : qMin(minVisible, c.row);
                maxVisible = qMax(maxVisible, c.row);
            }
        }
    }
    const int centerRow = minVisible >= 0 ? (minVisible + maxVisible) / 2 : currentRow();
    m_centerRow = qMax(0, centerRow);

    auto byPriority = [centerRow](const Candidate &a, const Candidate &b) {
        if (a.visible != b.visible) {
            return a.visible;   // viditelné před okolím
        }
        return qAbs(a.row - centerRow) < qAbs(b.row - centerRow);
    };
    std::sort(images.begin(), images.end(), byPriority);
    std::sort(videos.begin(), videos.end(), byPriority);

    m_pendingThumbs.clear();
    int wantedImages = 0;
    for (const Candidate &c : images) {
        ++wantedImages;
        const QString path = item(c.row)->data(Qt::UserRole).toString();
        if (!m_claims->contains(path)) {
            m_pendingThumbs.append(path);
        }
    }

    QStringList wantedVideos;
    for (const Candidate &c : videos) {
        wantedVideos.append(item(c.row)->data(Qt::UserRole).toString());
    }
    m_visibleVideos = wantedVideos;
    emitWantedVideos();

    dispatchThumbnails();
}

void ThumbnailPanel::dispatchThumbnails()
{
    while (!m_shuttingDown && m_activeWorkers.size() < kThumbnailThreads
           && !m_pendingThumbs.isEmpty()) {
        const QString path = m_pendingThumbs.takeFirst();
        if (itemForPath(path) == nullptr || !m_claims->claim(path)) {
            continue;   // mezitím smazáno, nebo už zpracováno
        }

        // Parent musí být nullptr — objekt spravuje fond vláken (smaže se
        // přes deleteLater po workerFinished). Qt parent by vytvořil druhou
        // cestu mazání a způsobil double-free.
        auto *worker = new ThumbnailWorker(QStringList{path}, m_generation,
                                           m_diskCacheEnabled, m_diskCacheDir, nullptr);
        worker->setFileStamps(m_stamps);
        connect(worker, &ThumbnailWorker::thumbnailReady, this, &ThumbnailPanel::onThumbnailReady);
        connect(worker, &ThumbnailWorker::workerFinished, worker, &ThumbnailWorker::deleteLater);
        connect(worker, &ThumbnailWorker::workerFinished, this, [this, worker](int generation) {
            m_activeWorkers.remove(worker);
            if (generation != m_generation) {
                return;
            }
            ++m_doneCount;
            dispatchThumbnails();
            if (m_activeWorkers.isEmpty() && m_pendingThumbs.isEmpty()) {
                diag::log(diag::thumbSummary(m_doneCount, m_statsTimer.elapsed()));
            }
            maybeStartWarmup();   // popředí dojelo — případně navázat zahříváním
        });
        m_activeWorkers.insert(worker);
        m_thumbPool.start(worker);
    }
}

void ThumbnailPanel::setWarmupTimingForTesting(int idleMs, int throttleMs, int videoIdleMs)
{
    m_warmupIdleMs = idleMs;
    m_warmupThrottleMs = throttleMs;
    m_videoWarmupIdleMs = videoIdleMs >= 0 ? videoIdleMs : idleMs;
}

void ThumbnailPanel::setViewerBusy(bool busy)
{
    m_viewerBusy = busy;
    // Ať busy začíná, nebo končí, doba klidu se odměřuje znovu od teď.
    noteActivity();
}

void ThumbnailPanel::setScanRunning(bool running)
{
    m_scanRunning = running;
    noteActivity();   // po skončení skenu se klid odměřuje znovu od teď
}

void ThumbnailPanel::noteActivity()
{
    // Cokoli, co uživatel dělá, má přednost před zahříváním: pozastavit worker
    // na pozadí a videa vrátit jen na viditelná (rozdělané video z ocasu se
    // zruší). Klid se odměřuje znovu.
    if (m_idleTimer == nullptr || m_videoIdleTimer == nullptr) {
        return;
    }
    m_idleReady = false;
    m_videoIdleReady = false;
    if (m_warmWorker != nullptr) {
        m_warmWorker->setPaused(true);
    }
    if (m_videoWarmupActive) {
        m_videoWarmupActive = false;
        emitWantedVideos();
    }
    m_idleTimer->start(m_warmupIdleMs);
    m_videoIdleTimer->start(m_videoWarmupIdleMs);
}

void ThumbnailPanel::maybeStartWarmup()
{
    if (m_shuttingDown || !m_idleReady || m_viewerBusy || m_scanRunning || count() == 0) {
        return;
    }
    // Popředí (viditelné miniatury) má přednost — zahřívání navazuje, až dojede.
    if (!m_activeWorkers.isEmpty() || !m_pendingThumbs.isEmpty()) {
        return;
    }

    if (m_warmPrepared) {
        // Už sestaveno dřív a přerušeno aktivitou uživatele — jen navázat.
        if (m_warmWorker != nullptr) {
            m_warmWorker->setPaused(false);
        }
        return;
    }
    m_warmPrepared = true;

    // Bez použitelné diskové cache by zahřívání nemělo smysl (nic by se
    // neuložilo) a jen zbytečně četlo ze sítě.
    if (!m_diskCacheEnabled || m_diskCacheDir.isEmpty()) {
        return;
    }

    // Od středu (kde uživatel je) směrem k okrajům. Viditelné obrázky už má
    // popředí (claims).
    QList<QPair<int, QString>> images;
    for (int row = 0; row < count(); ++row) {
        const QString path = item(row)->data(Qt::UserRole).toString();
        if (!isVideoFile(QStringLiteral(".") + QFileInfo(path).suffix()) && !m_claims->contains(path)) {
            images.append({qAbs(row - m_centerRow), path});
        }
    }
    std::sort(images.begin(), images.end(),
              [](const QPair<int, QString> &a, const QPair<int, QString> &b) { return a.first < b.first; });
    QStringList remaining;
    for (const auto &entry : std::as_const(images)) {
        remaining.append(entry.second);
    }
    if (remaining.isEmpty()) {
        maybeStartVideoWarmup();   // není co zahřívat u obrázků — případně rovnou videa
        return;
    }

    auto *worker = new ThumbnailWorker(remaining, m_generation, m_diskCacheEnabled, m_diskCacheDir, nullptr);
    worker->setCacheOnly(m_claims, m_warmupThrottleMs);
    worker->setFileStamps(m_stamps);
    connect(worker, &ThumbnailWorker::workerFinished, worker, &ThumbnailWorker::deleteLater);
    connect(worker, &ThumbnailWorker::workerFinished, this, [this, worker](int generation) {
        if (worker == m_warmWorker && generation == m_generation) {
            m_warmWorker = nullptr;
            updateWarmupProgress();
            maybeStartVideoWarmup();   // obrázky hotové — na řadě videa (pokud je klid)
        }
    });
    m_warmWorker = worker;
    m_warmSkipped = m_imageItemCount - static_cast<int>(remaining.size());
    m_warmPool.start(worker);
    m_progressTimer->start(m_progressIntervalMs);
    updateWarmupProgress();
}

void ThumbnailPanel::maybeStartVideoWarmup()
{
    // Videa jsou zdaleka nejtěžší (desítky MB po síti na jednu miniaturu), proto
    // jen v PLNÉM klidu a až po obrázcích: klid ≥ kVideoWarmupIdleMs, nic
    // se nenačítá, popředí nemá práci a zahřívání obrázků skončilo.
    if (m_shuttingDown || !m_videoIdleReady || m_viewerBusy || m_scanRunning || count() == 0
        || m_videoWarmupActive) {
        return;
    }
    if (!m_activeWorkers.isEmpty() || !m_pendingThumbs.isEmpty() || m_warmWorker != nullptr) {
        return;
    }
    if (!m_diskCacheEnabled || m_diskCacheDir.isEmpty()) {
        return;   // bez cache by generování videí nemělo smysl
    }

    if (!m_videoTailPrepared) {
        m_videoTailPrepared = true;
        QList<QPair<int, QString>> videos;
        for (int row = 0; row < count(); ++row) {
            const QString path = item(row)->data(Qt::UserRole).toString();
            if (isVideoFile(QStringLiteral(".") + QFileInfo(path).suffix())
                && !m_visibleVideos.contains(path)) {
                videos.append({qAbs(row - m_centerRow), path});
            }
        }
        std::sort(videos.begin(), videos.end(),
                  [](const QPair<int, QString> &a, const QPair<int, QString> &b) { return a.first < b.first; });
        m_warmVideoTail.clear();
        for (const auto &entry : std::as_const(videos)) {
            m_warmVideoTail.append(entry.second);
        }
    }
    m_videoWarmupActive = true;
    emitWantedVideos();
    m_progressTimer->start(m_progressIntervalMs);
    updateWarmupProgress();
}

void ThumbnailPanel::emitWantedVideos()
{
    QStringList wanted = m_visibleVideos;
    const int foreground = static_cast<int>(wanted.size());
    if (m_videoWarmupActive) {
        wanted += m_warmVideoTail;   // nízká priorita: až za viditelnými
    }
    if (wanted != m_lastWantedVideos) {
        m_lastWantedVideos = wanted;
        emit videoThumbnailsWanted(m_generation, wanted, foreground);
    }
}

QListWidgetItem *ThumbnailPanel::itemForPath(const QString &path) const
{
    const QPersistentModelIndex idx = m_pathToIndex.value(path);
    if (!idx.isValid()) {
        return nullptr;   // cesta neznámá, nebo řádek už byl smazán
    }
    return item(idx.row());
}

void ThumbnailPanel::setCurrentIndex(int index)
{
    if (index >= 0 && index < count()) {
        scheduleThumbnailUpdate();   // uživatel listuje — zahřívání ustoupí
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
        countItem(path, -1);
        m_pathToIndex.remove(path);
        m_claims->release(path);
        delete takeItem(index);
    }
}

void ThumbnailPanel::updateImagePath(const QString &oldPath, const QString &newPath)
{
    if (QListWidgetItem *target = itemForPath(oldPath)) {
        target->setData(Qt::UserRole, newPath);
        target->setToolTip(newPath.section('/', -1));
        m_pathToIndex[newPath] = m_pathToIndex.take(oldPath);
        // Miniatura (nebo pokus o ni) patří k souboru, ne k jeho starému názvu.
        m_claims->rename(oldPath, newPath);
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
    // D / Delete mažou (i výběr více náhledů) — obsluhuje je MainWindow. Po
    // Ctrl/Shift+kliku má fokus tento panel, takže QListWidget nesmí klávesu
    // pohltit vyhledáváním podle písmene.
    if (event->key() == Qt::Key_D || event->key() == Qt::Key_Delete) {
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

    if (QListWidgetItem *target = itemForPath(path)) {
        target->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

void ThumbnailPanel::setVideoThumbnail(int generation, const QString &path, const QImage &image)
{
    if (generation != m_generation || image.isNull()) {
        return;
    }
    if (QListWidgetItem *target = itemForPath(path)) {
        target->setIcon(QIcon(QPixmap::fromImage(image)));
    }
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
    scheduleThumbnailUpdate();   // jiná velikost = jiná sada viditelných položek
}

void ThumbnailPanel::showEvent(QShowEvent *event)
{
    QListWidget::showEvent(event);
    scheduleThumbnailUpdate();   // panel byl skrytý — dogenerovat, co je teď vidět
}

QSize ThumbnailPanel::sizeHint() const
{
    if (m_displayMode == DisplayMode::Vertical) {
        return QSize(m_thumbSize + 24, 200);
    }
    return QListWidget::sizeHint();
}

} // namespace pictureviewer
