#pragma once

#include <QSize>
#include <QString>
#include <QWidget>

class QGraphicsScene;
class QGraphicsVideoItem;
class QGraphicsView;
class QKeyEvent;
class QLabel;
class QMediaPlayer;
class QPushButton;
class QResizeEvent;
class QSlider;
class QTimer;

namespace pictureviewer {

class SettingsManager;

struct VideoMeta {
    QString path;
    qint64  fileSizeBytes = 0;
    QSize   resolution;
    qint64  durationMs    = 0;
    // Pole níže závisí na backendu — na macOS (AVFoundation přes Qt) nejsou dostupná.
    QString videoCodec;
    int     videoBitRate  = 0;   // b/s
    double  frameRate     = 0.0; // fps
    QString audioCodec;
};

class VideoPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayer(SettingsManager *settings, QWidget *parent = nullptr);
    ~VideoPlayer() override;

    // Spustí přehrávání souboru path (resetuje zoom na výchozí).
    void playFile(const QString &path);

    // Zastaví přehrávání a emituje stopped().
    void stopPlayback();

    // Zastaví přehrávání BEZ emitu stopped() — pro navigaci na jiný soubor.
    void stopQuietly();

    // Vizuální rotace o 90° (nepíše do souboru).
    void rotateLeft();
    void rotateRight();

    // Hledá video se stejným názvem jako imagePath (jiná přípona, stejný adresář).
    static bool findVideoFile(const QString &imagePath, QString &outVideoPath);

signals:
    void stopped();
    void fullscreenToggleRequested();
    void videoMetadataReady(const pictureviewer::VideoMeta &meta);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void onPlayPauseClicked();
    void onStopClicked();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(int status);
    void positionOverlay();
    void emitVideoMeta();

    // Zoom: delta je absolutní přírůstek (kladný = zvětšit, záporný = zmenšit).
    void zoomBy(double delta);
    // Aplikuje m_zoomMultiplier na view (fit + škálování).
    void applyZoom();
    // Resets zoom to fit-to-window.
    void resetZoom();

    static QString formatTime(qint64 ms);

    QString             m_currentPath;
    SettingsManager    *m_settings      = nullptr;
    QMediaPlayer       *m_player        = nullptr;
    QGraphicsScene     *m_scene         = nullptr;
    QGraphicsView      *m_view          = nullptr;
    QGraphicsVideoItem *m_videoItem     = nullptr;
    QPushButton        *m_playPauseBtn  = nullptr;
    QSlider            *m_seekSlider    = nullptr;
    QSlider            *m_volumeSlider  = nullptr;
    QLabel             *m_timeLabel      = nullptr;
    QLabel             *m_bufferOverlay  = nullptr;   // překryv "Načítám… X %"
    QTimer             *m_resizeTimer    = nullptr;
    bool                m_dragging       = false;

    // Zoom: 1.0 = přizpůsobeno oknu, >1 = zvětšeno, <1 = zmenšeno.
    double  m_zoomMultiplier = 1.0;
    // Po prvním stisku +/- je krok 5 %, každý další 10 %.
    bool    m_firstZoomOp    = true;
    // Vizuální rotace videa v násobcích 90°.
    int     m_rotationDeg    = 0;
};

} // namespace pictureviewer
