#pragma once

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTcpSocket>

class QTimer;
class QWidget;

namespace pictureviewer {

class SettingsManager;

// ── VLC Utilities ────────────────────────────────────────────────────────────

class VlcUtils {
public:
    // Auto-detect VLC executable path (platform-specific)
    // Returns empty string if not found
    static QString autoDetectVlcPath();

    // Show dialog to manually select VLC executable path
    // Returns selected path or empty string if cancelled
    // Shows error dialog and retries up to 3 times if invalid path
    static QString selectVlcPathDialog(QWidget *parent, SettingsManager *settings);

    // Verify that VLC executable exists at given path
    static bool isValidVlcPath(const QString &path);
};

// ── VLC Controller ───────────────────────────────────────────────────────────

enum class VlcState {
    Idle,
    Starting,
    Running,
    Paused,
    Stopped,
    Error
};

class VlcController : public QObject {
    Q_OBJECT

public:
    explicit VlcController(SettingsManager *settings, QObject *parent = nullptr);
    ~VlcController() override;

    // Find video file matching image name (same directory, different extension)
    // Checks: mp4, mkv, avi, mov, ts, mpg (first found wins)
    // Returns true if found, outVideoPath contains full path
    static bool findVideoFile(const QString &imagePath, QString &outVideoPath);

    // Initialize and start VLC playback
    // Returns true on success, false on error
    // Shows dialogs for VLC path selection if needed
    bool initialize(const QString &videoPath, QString &outErrorMsg);

    // Send command to VLC (via RC interface)
    // Commands: "pause", "seek +10", "seek -10", "volup", "voldown", "quit", "f", etc.
    void sendCommand(const QString &command);

    // Stop VLC gracefully
    void stop();

    // Check if VLC is currently running
    bool isRunning() const;

    // Get current VLC state
    VlcState state() const { return m_state; }

signals:
    void statusChanged(VlcState newState);
    void connectionLost();
    void processCrashed();

private slots:
    void onProcessStarted();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onMonitorTimeout();
    void onSocketConnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    bool startVlcProcess(const QString &videoPath);
    bool connectToVlcSocket();
    void cleanup();
    void setStateAndEmit(VlcState newState);
    void writeDiagnosticLog(int exitCode, QProcess::ExitStatus exitStatus,
                            const QString &stdOut, const QString &stdErr);

    SettingsManager *m_settings;
    QProcess *m_process;
    QTcpSocket *m_socket;
    QTimer *m_monitorTimer;

    QString m_videoPath;
    QString m_lastVlcPath;
    QStringList m_lastVlcArgs;
    VlcState m_state;
    int m_vlcGeneration;  // For stale signal prevention
};

} // namespace pictureviewer
