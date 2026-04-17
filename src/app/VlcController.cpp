#include "app/VlcController.hpp"
#include "app/SettingsManager.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace pictureviewer {

// ── VLC Utilities ────────────────────────────────────────────────────────────

QString VlcUtils::autoDetectVlcPath()
{
#ifdef Q_OS_WIN
    // Windows: Try registry first
    const QString registryPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\VideoLAN\\VLC";
    QSettings regSettings(registryPath, QSettings::NativeFormat);
    const QString installDir = regSettings.value("InstallDir", QString()).toString();

    if (!installDir.isEmpty()) {
        const QString vlcExe = installDir + "/vlc.exe";
        if (isValidVlcPath(vlcExe)) {
            return vlcExe;
        }
    }

    // Try standard installation paths
    const QStringList standardPaths = {
        "C:/Program Files/VideoLAN/VLC/vlc.exe",
        "C:/Program Files (x86)/VideoLAN/VLC/vlc.exe"
    };

    for (const QString &path : standardPaths) {
        if (isValidVlcPath(path)) {
            return path;
        }
    }

#elif defined(Q_OS_MAC)
    const QString vlcPath = "/Applications/VLC.app/Contents/MacOS/VLC";
    if (isValidVlcPath(vlcPath)) {
        return vlcPath;
    }

#else  // Linux
    // Try 'which vlc' command
    QProcess which;
    which.start("which", QStringList() << "vlc");
    if (which.waitForFinished(2000)) {
        const QString path = QString::fromLocal8Bit(which.readAllStandardOutput()).trimmed();
        if (!path.isEmpty() && isValidVlcPath(path)) {
            return path;
        }
    }
#endif

    return QString();  // Not found
}

bool VlcUtils::isValidVlcPath(const QString &path)
{
    return QFileInfo::exists(path) && QFileInfo(path).isFile();
}

QString VlcUtils::selectVlcPathDialog(QWidget *parent, SettingsManager *settings)
{
    const int maxRetries = 3;
    QString selectedPath;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        const QString filter = "VLC Media Player (vlc.exe vlc)";
        selectedPath = QFileDialog::getOpenFileName(
            parent,
            "Vyberte VLC Media Player",
            QString(),
            filter
        );

        // User cancelled
        if (selectedPath.isEmpty()) {
            return QString();
        }

        // Validate path
        if (isValidVlcPath(selectedPath)) {
            // Save to settings
            settings->setVlcPath(selectedPath);
            return selectedPath;
        }

        // Invalid path - show error and retry
        const QString errorMsg = QString("Soubor VLC nebyl nalezen na:\n%1\n\nZkuste znova.").arg(selectedPath);
        QMessageBox::warning(parent, "Neplatná cesta", errorMsg);
    }

    // Max retries reached
    const QString finalError = "Dosáhli jste maximálního počtu pokusů.\nVLC nelze spustit.";
    QMessageBox::critical(parent, "Chyba", finalError);

    return QString();
}

// ── VLC Controller ───────────────────────────────────────────────────────────

VlcController::VlcController(SettingsManager *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_process(nullptr)
    , m_socket(nullptr)
    , m_monitorTimer(nullptr)
    , m_state(VlcState::Idle)
    , m_vlcGeneration(0)
{
}

VlcController::~VlcController()
{
    stop();
    cleanup();
}

bool VlcController::findVideoFile(const QString &imagePath, QString &outVideoPath)
{
    const QFileInfo imageFile(imagePath);
    const QString imageDir = imageFile.absolutePath();
    const QString imageName = imageFile.baseName();  // without extension

    // Video extensions to check (first found wins)
    const QStringList videoExtensions = { "mp4", "mkv", "avi", "mov", "ts", "mpg" };

    for (const QString &ext : videoExtensions) {
        const QString videoPath = imageDir + "/" + imageName + "." + ext;
        if (QFileInfo::exists(videoPath)) {
            outVideoPath = videoPath;
            return true;
        }
    }

    return false;
}

bool VlcController::initialize(const QString &videoPath, QString &outErrorMsg)
{
    ++m_vlcGeneration;  // Invalidate any stale signals
    m_videoPath = videoPath;

    setStateAndEmit(VlcState::Starting);

    // Get VLC path from settings or auto-detect
    QString vlcPath = m_settings->vlcPath();

    if (!VlcUtils::isValidVlcPath(vlcPath)) {
        // Try auto-detect
        vlcPath = VlcUtils::autoDetectVlcPath();
    }

    if (!VlcUtils::isValidVlcPath(vlcPath)) {
        // Need to prompt user
        vlcPath = VlcUtils::selectVlcPathDialog(nullptr, m_settings);
    }

    if (vlcPath.isEmpty()) {
        outErrorMsg = "VLC nebyl nalezen. Přihlaste se k instalaci VLC.";
        setStateAndEmit(VlcState::Error);
        return false;
    }

    // Save resolved path so it's available next launch without re-detection
    m_settings->setVlcPath(vlcPath);

    // Start VLC process
    if (!startVlcProcess(vlcPath, videoPath)) {
        outErrorMsg = "Nepodařilo se spustit VLC.";
        setStateAndEmit(VlcState::Error);
        return false;
    }

    // Start process monitor — early VLC exit detected within first tick
    if (!m_monitorTimer) {
        m_monitorTimer = new QTimer(this);
        connect(m_monitorTimer, &QTimer::timeout, this, &VlcController::onMonitorTimeout);
    }
    m_monitorTimer->start(500);

    // Try to connect RC socket asynchronously (non-blocking)
    connectToVlcSocket();

    setStateAndEmit(VlcState::Running);
    return true;
}

bool VlcController::startVlcProcess(const QString &vlcPath, const QString &videoPath)
{
    if (m_process) {
        delete m_process;
    }

    m_process = new QProcess(this);

    connect(m_process, &QProcess::finished, this, &VlcController::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &VlcController::onProcessError);

    QStringList args;
    args << "--extraintf=rc"
         << "--rc-host=127.0.0.1:4444"
         << videoPath;

    m_lastVlcPath = vlcPath;
    m_lastVlcArgs = args;

    m_process->start(vlcPath, args);

    if (!m_process->waitForStarted(2000)) {
        qWarning() << "Failed to start VLC process";
        writeDiagnosticLog(-1, QProcess::CrashExit, QString(), "Process failed to start within 2000ms");
        return false;
    }

    qDebug() << "VLC process started:" << vlcPath << args;
    return true;
}

bool VlcController::connectToVlcSocket()
{
    if (m_socket) {
        m_socket->abort();
        delete m_socket;
    }

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &VlcController::onSocketConnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &VlcController::onSocketError);

    // Non-blocking: just initiate connection, result delivered via onSocketConnected / onSocketError
    m_socket->connectToHost("127.0.0.1", 4444);
    return true;
}

void VlcController::sendCommand(const QString &command)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "VLC socket not connected, cannot send command:" << command;
        return;
    }

    const QString msg = command + "\n";
    m_socket->write(msg.toUtf8());
    m_socket->waitForBytesWritten(1000);

    qDebug() << "Sent VLC command:" << command;
}

void VlcController::stop()
{
    if (m_state == VlcState::Stopped || m_state == VlcState::Idle)
        return;

    // Stop timer and update state FIRST — prevents re-entry from timer or onProcessFinished
    if (m_monitorTimer)
        m_monitorTimer->stop();
    setStateAndEmit(VlcState::Stopped);

    if (m_process && m_process->state() == QProcess::Running) {
        // Disconnect slots before sending quit so onProcessFinished won't fire after this
        m_process->disconnect(this);
        sendCommand("quit");
        QTimer::singleShot(1500, this, [this]() {
            if (m_process && m_process->state() == QProcess::Running)
                m_process->terminate();
        });
    }

    // Defer object deletion — never delete from within a connected slot
    QTimer::singleShot(0, this, &VlcController::cleanup);
}

bool VlcController::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void VlcController::onMonitorTimeout()
{
    if (m_state == VlcState::Stopped || m_state == VlcState::Idle)
        return;

    if (!m_process || m_process->state() != QProcess::Running) {
        // Stop timer immediately — prevent re-entry while dialogs are open
        m_monitorTimer->stop();
        setStateAndEmit(VlcState::Stopped);

        // Capture output and write log BEFORE emitting any signal
        const QString stdOut = m_process ? QString::fromLocal8Bit(m_process->readAllStandardOutput()) : QString();
        const QString stdErr = m_process ? QString::fromLocal8Bit(m_process->readAllStandardError()) : QString();
        const int exitCode   = m_process ? m_process->exitCode() : -1;
        const auto exitStatus = m_process ? m_process->exitStatus() : QProcess::CrashExit;
        writeDiagnosticLog(exitCode, exitStatus, stdOut, stdErr);

        emit processCrashed();
        QTimer::singleShot(0, this, &VlcController::cleanup);
    }
}

void VlcController::onProcessStarted()
{
    qDebug() << "VLC process started";
}

void VlcController::onProcessError(QProcess::ProcessError error)
{
    if (m_state == VlcState::Stopped || m_state == VlcState::Idle)
        return;

    if (m_monitorTimer)
        m_monitorTimer->stop();
    setStateAndEmit(VlcState::Stopped);

    const QString errStr = m_process ? m_process->errorString() : QString("unknown");
    writeDiagnosticLog(-1, QProcess::CrashExit, QString(), errStr);

    emit processCrashed();
    QTimer::singleShot(0, this, &VlcController::cleanup);
}

void VlcController::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Guard — onMonitorTimeout may have already handled this
    if (m_state == VlcState::Stopped || m_state == VlcState::Idle)
        return;

    if (m_monitorTimer)
        m_monitorTimer->stop();

    const QString stdOut = m_process ? QString::fromLocal8Bit(m_process->readAllStandardOutput()) : QString();
    const QString stdErr = m_process ? QString::fromLocal8Bit(m_process->readAllStandardError()) : QString();

    writeDiagnosticLog(exitCode, exitStatus, stdOut, stdErr);
    setStateAndEmit(VlcState::Stopped);

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        emit processCrashed();
    }

    // Defer cleanup — never delete signal sender (m_process) from within its own slot
    QTimer::singleShot(0, this, &VlcController::cleanup);
}

void VlcController::writeDiagnosticLog(int exitCode, QProcess::ExitStatus exitStatus,
                                        const QString &stdOut, const QString &stdErr)
{
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_lastLogPath = downloads + "/vlc_debug.log";

    QFile file(m_lastLogPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastLogPath.clear();
        return;
    }

    QTextStream out(&file);
    out << "=== VLC Diagnostic Log ===\n";
    out << "Time:        " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "VLC path:    " << m_lastVlcPath << "\n";
    out << "Arguments:   " << m_lastVlcArgs.join(" ") << "\n";
    out << "Video:       " << m_videoPath << "\n";
    out << "Exit code:   " << exitCode << "\n";
    out << "Exit status: " << (exitStatus == QProcess::CrashExit ? "CrashExit" : "NormalExit") << "\n";
    if (!stdOut.isEmpty())
        out << "\n--- stdout ---\n" << stdOut << "\n";
    if (!stdErr.isEmpty())
        out << "\n--- stderr ---\n" << stdErr << "\n";
}

void VlcController::onSocketConnected()
{
    qDebug() << "VLC RC socket connected";
}

void VlcController::onSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "VLC socket error:" << m_socket->errorString();
    emit connectionLost();
}

void VlcController::cleanup()
{
    if (m_monitorTimer) {
        m_monitorTimer->stop();
        delete m_monitorTimer;
        m_monitorTimer = nullptr;
    }

    if (m_socket) {
        m_socket->disconnectFromHost();
        delete m_socket;
        m_socket = nullptr;
    }

    if (m_process) {
        delete m_process;
        m_process = nullptr;
    }
}

void VlcController::setStateAndEmit(VlcState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit statusChanged(newState);
    }
}

} // namespace pictureviewer
