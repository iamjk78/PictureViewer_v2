#include "core/DiagLog.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QThread>

namespace pictureviewer::diag {

namespace {

// Strop velikosti jednoho logu — dlouhý běh nesmí zaplnit disk.
constexpr qint64 kMaxLogBytes = 30LL * 1024 * 1024;

struct State {
    QMutex mutex;
    QFile file;
    QElapsedTimer process;
    QElapsedTimer folder;
    bool folderStarted = false;
    bool truncated = false;
    std::atomic_bool active{false};
};

State &state()
{
    static State s;
    return s;
}

void writeLocked(State &s, const QString &msg)
{
    if (!s.file.isOpen() || s.truncated) {
        return;
    }
    if (s.file.size() > kMaxLogBytes) {
        s.truncated = true;
        s.file.write("[log dosáhl maximální velikosti, další zápis vypnut]\n");
        s.file.flush();
        return;
    }
    const bool onUi = QCoreApplication::instance() != nullptr
        && QThread::currentThread() == QCoreApplication::instance()->thread();
    const QString folder = s.folderStarted ? QString::number(s.folder.elapsed()) : QStringLiteral("-");
    const QString line = QStringLiteral("[%1 ms | složka +%2 ms | %3] %4\n")
        .arg(s.process.elapsed(), 8)
        .arg(folder, 7)
        .arg(onUi ? QStringLiteral("UI     ") : QStringLiteral("pozadí "), msg);
    s.file.write(line.toUtf8());
    s.file.flush();
}

} // namespace

QString defaultDirectory()
{
#ifdef Q_OS_MACOS
    return QDir::homePath() + QStringLiteral("/Library/Logs/PictureViewer");
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
#endif
}

void start(const QString &directory, int keepFiles)
{
    State &s = state();
    QMutexLocker lock(&s.mutex);
    if (s.file.isOpen()) {
        return;
    }
    QDir dir(directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        return;
    }

    // Nejstarší logy pryč (názvy obsahují čas, takže abecední = chronologické).
    QStringList existing = dir.entryList({QStringLiteral("pictureviewer-*.log")},
                                         QDir::Files, QDir::Name);
    while (existing.size() >= keepFiles && !existing.isEmpty()) {
        dir.remove(existing.takeFirst());
    }

    const QString name = QStringLiteral("pictureviewer-%1-%2.log")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
        .arg(QCoreApplication::applicationPid());
    s.file.setFileName(dir.absoluteFilePath(name));
    if (!s.file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }
    s.process.start();
    s.truncated = false;
    s.folderStarted = false;
    s.active = true;
    writeLocked(s, QStringLiteral("=== PictureViewer %1 — start logu ===")
                       .arg(QCoreApplication::applicationVersion()));
}

void stop()
{
    State &s = state();
    QMutexLocker lock(&s.mutex);
    s.active = false;
    s.file.close();
}

bool active()
{
    return state().active;
}

QString logFilePath()
{
    State &s = state();
    QMutexLocker lock(&s.mutex);
    return s.file.isOpen() ? s.file.fileName() : QString();
}

void log(const QString &message)
{
    State &s = state();
    if (!s.active) {
        return;
    }
    QMutexLocker lock(&s.mutex);
    writeLocked(s, message);
}

void markFolderStart(const QString &path)
{
    State &s = state();
    if (!s.active) {
        return;
    }
    QMutexLocker lock(&s.mutex);
    s.folder.restart();
    s.folderStarted = true;
    writeLocked(s, QStringLiteral("=========== OTEVÍRÁM SLOŽKU: %1").arg(path));
}

ScopedTimer::ScopedTimer(QString label, int thresholdMs)
    : m_label(std::move(label))
    , m_thresholdMs(thresholdMs)
{
    m_timer.start();
}

ScopedTimer::~ScopedTimer()
{
    if (!active()) {
        return;
    }
    const qint64 ms = m_timer.elapsed();
    if (ms >= m_thresholdMs) {
        log(QStringLiteral("%1: %2 ms").arg(m_label).arg(ms));
    }
}

void ThumbCounters::reset()
{
    hit = 0; generated = 0; failed = 0; exifUsed = 0;
    keyNs = 0; cacheReadNs = 0; generateNs = 0; srcBytes = 0;
}

ThumbCounters &thumb()
{
    static ThumbCounters c;
    return c;
}

QString thumbSummary(int done, qint64 elapsedMs)
{
    ThumbCounters &c = thumb();
    const double sec = elapsedMs / 1000.0;
    const qint64 n = qMax<qint64>(1, c.hit + c.generated + c.failed);
    const qint64 gen = qMax<qint64>(1, c.generated.load());
    return QStringLiteral("miniatury hotovo %1 za %2 s (%3/s) | z cache %4, vygenerováno %5 (z toho EXIF %6), selhalo %7 | "
                          "průměr: klíč %8 ms, čtení cache %9 ms, generování %10 ms | zdroj ~%11 MB")
        .arg(done).arg(sec, 0, 'f', 1).arg(sec > 0 ? done / sec : 0.0, 0, 'f', 1)
        .arg(c.hit.load()).arg(c.generated.load()).arg(c.exifUsed.load()).arg(c.failed.load())
        .arg(c.keyNs / 1e6 / n, 0, 'f', 1).arg(c.cacheReadNs / 1e6 / n, 0, 'f', 1)
        .arg(c.generateNs / 1e6 / gen, 0, 'f', 1)
        .arg(c.srcBytes / (1024.0 * 1024.0), 0, 'f', 1);
}

} // namespace pictureviewer::diag
