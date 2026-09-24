#pragma once

#include "core/FileStampIndex.hpp"

#include "core/ImageCatalog.hpp"

#include <QObject>
#include <QRunnable>
#include <QStringList>

#include <atomic>

namespace pictureviewer {

class SettingsManager;

class FolderScanWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    // stamps: sem sken ukládá čas změny a velikost souborů z výpisu složky.
    FolderScanWorker(const SettingsManager *settings, QString folderPath, int generation,
                     QSharedPointer<FileStampIndex> stamps, QObject *parent = nullptr);

    void cancel();
    void run() override;

    // Jen pro testy: zpomalí každou dávku (simulace pomalého úložiště).
    static void setStreamDelayForTesting(int msPerBatch);

signals:
    // Dílčí výsledek při řazení podle jména: soubory v pořadí, v jakém je
    // vrací úložiště (nesetříděné). Kompletní seřazený seznam přijde
    // v scanComplete().
    void scanProgress(int generation, const QStringList &batch);
    void scanComplete(int generation, const QStringList &paths);
    void scanError(int generation, const QString &error);
    void finished(int generation);

private:
    // Hodnoty nastavení se kopírují v konstruktoru (na hlavním vlákně).
    // Worker NESMÍ držet ukazatel na SettingsManager — při přepnutí profilu
    // se manager maže a běžící vlákno by četlo uvolněnou paměť.
    bool    m_includePdf    = true;
    bool    m_includeImages = true;
    bool    m_includeVideos = false;
    SortKey m_sortKey       = SortKey::Name;
    bool    m_ascending     = true;

    QString m_folderPath;
    QSharedPointer<FileStampIndex> m_stamps;
    int m_generation;
    std::atomic_bool m_cancelled;
};

} // namespace pictureviewer
