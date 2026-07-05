#include "app/VideoPlayer.hpp"

#include "app/SettingsManager.hpp"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace pictureviewer {

VideoPlayer::VideoPlayer(SettingsManager *settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setFocusPolicy(Qt::StrongFocus);

    // ── Přehrávač ────────────────────────────────────────────────────────────
    m_player = new QMediaPlayer(this);
    auto *audioOutput = new QAudioOutput(this);
    audioOutput->setVolume(m_settings->videoVolume() / 100.0f);
    m_player->setAudioOutput(audioOutput);

    // ── Video plocha: QGraphicsView + QGraphicsVideoItem ─────────────────────
    // QGraphicsVideoItem umožňuje zoom a posun myší přes standardní transformace.
    m_scene     = new QGraphicsScene(this);
    m_videoItem = new QGraphicsVideoItem();
    m_scene->addItem(m_videoItem);

    m_view = new QGraphicsView(m_scene, this);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setRenderHint(QPainter::Antialiasing, false);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, false);
    // Pro video je překreslení celého viewportu každý snímek efektivnější
    // než počítání minimálních dirty regionů.
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setBackgroundBrush(Qt::black);
    m_view->setInteractive(true);

    m_player->setVideoOutput(m_videoItem);

    // Po získání nativní velikosti videa přizpůsobit pohled.
    connect(m_videoItem, &QGraphicsVideoItem::nativeSizeChanged,
            this, [this](const QSizeF &size) {
                if (size.isValid()) {
                    m_videoItem->setPos(0, 0);
                    m_scene->setSceneRect(QRectF(QPointF(0, 0), size));
                    applyZoom();
                }
            });

    // Debounce resize — layout se usadí asynchronně.
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(0);
    connect(m_resizeTimer, &QTimer::timeout, this, [this] {
        applyZoom();
        positionOverlay();
    });

    // Timeout pro poškozené/zaseknuté soubory — 15 s od playFile() do načtení.
    m_loadTimeoutTimer = new QTimer(this);
    m_loadTimeoutTimer->setSingleShot(true);
    m_loadTimeoutTimer->setInterval(15000);
    connect(m_loadTimeoutTimer, &QTimer::timeout, this, [this] {
        m_bufferOverlay->hide();
        stopPlayback();
    });

    // ── Overlay pro stav bufferu ──────────────────────────────────────────────
    // Zobrazuje se přes m_view při načítání/stallování ze sítě.
    m_bufferOverlay = new QLabel(this);
    m_bufferOverlay->setAlignment(Qt::AlignCenter);
    m_bufferOverlay->setStyleSheet(
        "background-color: rgba(0,0,0,160);"
        "color: #ffffff;"
        "font-size: 16px;"
        "border-radius: 8px;"
        "padding: 8px 20px;");
    m_bufferOverlay->hide();

    // ── Ovládací tlačítka ────────────────────────────────────────────────────
    auto *stopBtn     = new QPushButton(QStringLiteral("⏹"), this);
    auto *seekBackBtn = new QPushButton(QStringLiteral("⏮ -10s"), this);
    m_playPauseBtn    = new QPushButton(QStringLiteral("⏸"), this);
    auto *seekFwdBtn  = new QPushButton(QStringLiteral("+10s ⏭"), this);

    stopBtn->setToolTip(tr("Zastavit (Esc)"));
    seekBackBtn->setToolTip(tr("−10 sekund (←)"));
    m_playPauseBtn->setToolTip(tr("Přehrát / Pauza (Mezerník)"));
    seekFwdBtn->setToolTip(tr("+10 sekund (→)"));

    // Seek slider
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setSingleStep(1000);
    m_seekSlider->setPageStep(10000);

    // Časový popis
    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setMinimumWidth(110);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Hlasitost
    auto *volumeLabel = new QLabel(QStringLiteral("🔊"), this);
    m_volumeSlider    = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(static_cast<int>(audioOutput->volume() * 100));
    m_volumeSlider->setMaximumWidth(80);
    m_volumeSlider->setToolTip(tr("Hlasitost"));

    auto *controlBar = new QWidget(this);
    auto *controls   = new QHBoxLayout(controlBar);
    controls->setContentsMargins(8, 4, 8, 4);
    controls->setSpacing(6);
    controls->addWidget(stopBtn);
    controls->addWidget(seekBackBtn);
    controls->addWidget(m_playPauseBtn);
    controls->addWidget(seekFwdBtn);
    controls->addWidget(m_seekSlider, 1);
    controls->addWidget(m_timeLabel);
    controls->addWidget(volumeLabel);
    controls->addWidget(m_volumeSlider);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);
    layout->addWidget(controlBar);

    // ── Signály přehrávače ────────────────────────────────────────────────────
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState s) {
                onPlaybackStateChanged(static_cast<int>(s));
            });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus s) {
                onMediaStatusChanged(static_cast<int>(s));
            });
    connect(m_player, &QMediaPlayer::bufferProgressChanged, this,
            [this](float progress) {
                if (m_bufferOverlay->isVisible()) {
                    m_bufferOverlay->setText(
                        tr("Načítám…  %1 %").arg(qRound(progress * 100)));
                }
            });
    connect(m_player, &QMediaPlayer::positionChanged,  this, &VideoPlayer::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,  this, &VideoPlayer::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error /*e*/, const QString & /*msg*/) {
                emit stopped();
            });

    // Metadata videa — backend je vydává postupně; posloucháme na obou místech.
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &VideoPlayer::emitVideoMeta);


    // Seek slider
    connect(m_seekSlider, &QSlider::sliderPressed,  this, [this] { m_dragging = true;  });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this] {
        m_dragging = false;
        m_player->setPosition(m_seekSlider->value());
    });

    // Hlasitost — uložení do settings
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this, audioOutput](int v) {
        audioOutput->setVolume(v / 100.0f);
        m_settings->setVideoVolume(v);
    });

    // Tlačítka
    connect(m_playPauseBtn, &QPushButton::clicked, this, &VideoPlayer::onPlayPauseClicked);
    connect(stopBtn,        &QPushButton::clicked, this, &VideoPlayer::onStopClicked);
    connect(seekBackBtn,    &QPushButton::clicked, this, [this] {
        m_player->setPosition(qMax(0LL, m_player->position() - 10000));
    });
    connect(seekFwdBtn,     &QPushButton::clicked, this, [this] {
        m_player->setPosition(qMin(m_player->duration(), m_player->position() + 10000));
    });
}

VideoPlayer::~VideoPlayer() = default;

void VideoPlayer::setSettingsManager(SettingsManager *settings)
{
    m_settings = settings;
    if (m_settings != nullptr) {
        // Převzít hlasitost nového profilu; setValue propíše i do audio výstupu
        // přes existující valueChanged connect.
        m_volumeSlider->setValue(m_settings->videoVolume());
    }
}

// ── Veřejné rozhraní ─────────────────────────────────────────────────────────

void VideoPlayer::playFile(const QString &path)
{
    m_currentPath  = path;
    m_rotationDeg  = 0;
    m_videoItem->setRotation(0);
    resetZoom();
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->setLoops(QMediaPlayer::Infinite);
    m_player->play();
    m_playPauseBtn->setText(QStringLiteral("⏸"));
    m_loadTimeoutTimer->start();
    setFocus();
}

void VideoPlayer::stopPlayback()
{
    m_loadTimeoutTimer->stop();
    m_player->stop();
    m_player->setSource(QUrl());
    emit stopped();
}

void VideoPlayer::stopQuietly()
{
    m_loadTimeoutTimer->stop();
    m_stoppingQuietly = true;
    m_player->stop();
    m_player->setSource(QUrl());
    m_stoppingQuietly = false;
}

bool VideoPlayer::findVideoFile(const QString &imagePath, QString &outVideoPath)
{
    const QFileInfo imageFile(imagePath);
    const QString dir  = imageFile.absolutePath();
    const QString base = imageFile.completeBaseName();

    const QStringList exts = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("ts"),  QStringLiteral("mpg"),
        QStringLiteral("webm"),QStringLiteral("wmv"), QStringLiteral("m4v")
    };
    for (const QString &ext : exts) {
        const QString candidate =
            dir + QStringLiteral("/") + base + QStringLiteral(".") + ext;
        if (QFileInfo::exists(candidate)) {
            outVideoPath = candidate;
            return true;
        }
    }
    return false;
}

// ── Klávesy ──────────────────────────────────────────────────────────────────

void VideoPlayer::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        onPlayPauseClicked();
        return;
    case Qt::Key_Escape:
        stopPlayback();
        return;
    case Qt::Key_Left:
        m_player->setPosition(qMax(0LL, m_player->position() - m_player->duration() / 10));
        return;
    case Qt::Key_Right:
        m_player->setPosition(qMin(m_player->duration(), m_player->position() + m_player->duration() / 10));
        return;
    case Qt::Key_Up:
        m_player->setPosition(0);
        return;
    case Qt::Key_Down:
        stopPlayback();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal: {
        const double step = m_firstZoomOp ? 0.05 : 0.10;
        m_firstZoomOp = false;
        zoomBy(step);
        return;
    }
    case Qt::Key_Minus: {
        const double step = m_firstZoomOp ? -0.05 : -0.10;
        m_firstZoomOp = false;
        zoomBy(step);
        return;
    }
    case Qt::Key_F:
        emit fullscreenToggleRequested();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

// ── Resize ───────────────────────────────────────────────────────────────────

void VideoPlayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Layout se usadí asynchronně — applyZoom spustíme po dokončení.
    m_resizeTimer->start();
}

// ── Zoom ─────────────────────────────────────────────────────────────────────

void VideoPlayer::zoomBy(double delta)
{
    // Minimum 1.0 — video nikdy menší než okno. Maximum 8×.
    m_zoomMultiplier = qBound(1.0, m_zoomMultiplier + delta, 8.0);
    applyZoom();
}

void VideoPlayer::applyZoom()
{
    if (!m_videoItem->nativeSize().isValid())
        return;

    // Zoom je vždy ≥ 1.0 — video nikdy menší než okno.
    // fitInView přesně přizpůsobí video oknu, scale() ho pak zvětší.
    m_videoItem->setPos(0, 0);
    m_scene->setSceneRect(QRectF(QPointF(0, 0), m_videoItem->nativeSize()));
    m_view->fitInView(m_videoItem, Qt::KeepAspectRatio);
    if (m_zoomMultiplier > 1.0)
        m_view->scale(m_zoomMultiplier, m_zoomMultiplier);

    // Povolení posunu myší (ScrollHandDrag) pouze při zvětšení nad 100 %.
    m_view->setDragMode(m_zoomMultiplier > 1.005
                            ? QGraphicsView::ScrollHandDrag
                            : QGraphicsView::NoDrag);
}

void VideoPlayer::resetZoom()
{
    m_zoomMultiplier = 1.0;
    m_firstZoomOp    = true;
    m_view->setDragMode(QGraphicsView::NoDrag);
}

void VideoPlayer::rotateLeft()
{
    m_rotationDeg = (m_rotationDeg - 90 + 360) % 360;
    m_videoItem->setRotation(m_rotationDeg);
    applyZoom();
}

void VideoPlayer::rotateRight()
{
    m_rotationDeg = (m_rotationDeg + 90) % 360;
    m_videoItem->setRotation(m_rotationDeg);
    applyZoom();
}

// ── Privátní sloty ───────────────────────────────────────────────────────────

void VideoPlayer::onPlayPauseClicked()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void VideoPlayer::onStopClicked()
{
    stopPlayback();
}

void VideoPlayer::onPlaybackStateChanged(int state)
{
    const auto s = static_cast<QMediaPlayer::PlaybackState>(state);
    switch (s) {
    case QMediaPlayer::PlayingState:
        m_playPauseBtn->setText(QStringLiteral("⏸"));
        break;
    case QMediaPlayer::PausedState:
        m_playPauseBtn->setText(QStringLiteral("▶"));
        break;
    case QMediaPlayer::StoppedState:
        m_loadTimeoutTimer->stop();
        m_player->setSource(QUrl());
        if (!m_stoppingQuietly) {
            emit stopped();
        }
        break;
    }
}

void VideoPlayer::onPositionChanged(qint64 position)
{
    if (!m_dragging)
        m_seekSlider->setValue(static_cast<int>(position));
    m_timeLabel->setText(
        formatTime(position) + QStringLiteral(" / ") + formatTime(m_player->duration()));
}

void VideoPlayer::onDurationChanged(qint64 duration)
{
    m_seekSlider->setRange(0, static_cast<int>(duration));
}

void VideoPlayer::emitVideoMeta()
{
    if (m_currentPath.isEmpty())
        return;
    const QMediaMetaData md = m_player->metaData();
    VideoMeta vm;
    vm.path          = m_currentPath;
    vm.fileSizeBytes = QFileInfo(m_currentPath).size();
    vm.resolution    = md.value(QMediaMetaData::Resolution).toSize();
    vm.durationMs    = md.value(QMediaMetaData::Duration).toLongLong();
    // Tato pole závisí na backendu — na macOS/AVFoundation nejsou dostupná přes Qt.
    vm.videoCodec    = md.value(QMediaMetaData::VideoCodec).toString();
    vm.videoBitRate  = md.value(QMediaMetaData::VideoBitRate).toInt();
    vm.frameRate     = md.value(QMediaMetaData::VideoFrameRate).toDouble();
    vm.audioCodec    = md.value(QMediaMetaData::AudioCodec).toString();
    emit videoMetadataReady(vm);
}

void VideoPlayer::onMediaStatusChanged(int status)
{
    const auto s = static_cast<QMediaPlayer::MediaStatus>(status);
    switch (s) {
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::StalledMedia: {
        const int pct = qRound(m_player->bufferProgress() * 100);
        m_bufferOverlay->setText(tr("Načítám…  %1 %").arg(pct));
        m_bufferOverlay->adjustSize();
        positionOverlay();
        m_bufferOverlay->show();
        m_bufferOverlay->raise();
        break;
    }
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        m_loadTimeoutTimer->stop();
        m_bufferOverlay->hide();
        emitVideoMeta();
        break;
    default:
        m_bufferOverlay->hide();
        break;
    }
}

void VideoPlayer::positionOverlay()
{
    // Vycentrovat overlay přes oblast videa (m_view).
    const QPoint viewPos = m_view->mapTo(this, QPoint(0, 0));
    const int x = viewPos.x() + (m_view->width()  - m_bufferOverlay->width())  / 2;
    const int y = viewPos.y() + (m_view->height() - m_bufferOverlay->height()) / 2;
    m_bufferOverlay->move(x, y);
}

// ── Pomocné ──────────────────────────────────────────────────────────────────

QString VideoPlayer::formatTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    const int totalSec = static_cast<int>(ms / 1000);
    const int hours    = totalSec / 3600;
    const int minutes  = (totalSec % 3600) / 60;
    const int seconds  = totalSec % 60;
    if (hours > 0)
        return QString::asprintf("%d:%02d:%02d", hours, minutes, seconds);
    return QString::asprintf("%d:%02d", minutes, seconds);
}

} // namespace pictureviewer
