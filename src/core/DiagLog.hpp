#pragma once

#include <QElapsedTimer>
#include <QString>

#include <atomic>

// Trvalý lokální diagnostický log (časování načítání složek, miniatur, zásek UI).
// Nikdy se neodesílá — zapisuje se jen do složky s logy na disku uživatele.
// Dokud se nezavolá start(), log() nedělá nic (unit/GUI testy tak nic nezapisují).
namespace pictureviewer::diag {

// Otevře soubor logu pro tento běh aplikace (jeden soubor na proces, žádné
// přepisování cizího logu) a smaže nejstarší, ať jich zůstane nejvýš keepFiles.
void start(const QString &directory, int keepFiles = 10);
void stop();
bool active();
QString logFilePath();

// Výchozí umístění: macOS ~/Library/Logs/PictureViewer, jinde <AppLocalData>/logs.
QString defaultDirectory();

void log(const QString &message);

// Začátek otevírání složky — další řádky ukazují i čas "složka +N ms".
void markFolderStart(const QString &path);

// Zapíše řádek, jen pokud měřený úsek trval aspoň thresholdMs (nebo vždy při 0).
class ScopedTimer
{
public:
    explicit ScopedTimer(QString label, int thresholdMs = 0);
    ~ScopedTimer();
    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

private:
    QString m_label;
    int m_thresholdMs;
    QElapsedTimer m_timer;
};

// Souhrnné čítače miniatur — plní ThumbnailWorker, vypisuje ThumbnailPanel.
struct ThumbCounters {
    std::atomic<qint64> hit{0}, generated{0}, failed{0}, exifUsed{0};
    std::atomic<qint64> keyNs{0}, cacheReadNs{0}, generateNs{0}, srcBytes{0};
    void reset();
};
ThumbCounters &thumb();
QString thumbSummary(int done, qint64 elapsedMs);

} // namespace pictureviewer::diag
