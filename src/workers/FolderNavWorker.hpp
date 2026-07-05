#pragma once

#include "core/FolderNavigator.hpp"

#include <QObject>
#include <QRunnable>

#include <atomic>

namespace pictureviewer {

// Spočítá na pozadí sourozence (doleva/doprava) a podsložky (dolů) aktuální
// složky. "Nahoru" se nepočítá zde — je to triviální kontrola bez čtení
// adresáře, dělá se synchronně přímo v MainWindow.
//
// Spouští se JEN pokud je navigační toolbar viditelný (viz MainWindow_FolderNav.cpp) —
// když je skrytý, žádné čtení adresářové struktury neprobíhá.
class FolderNavWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FolderNavWorker(QString currentFolder, int generation, QObject *parent = nullptr);

    void cancel();
    void run() override;

signals:
    void navDataReady(int generation, FolderNavResult left, FolderNavResult right, FolderNavResult down);
    void finished(int generation);

private:
    QString m_currentFolder;
    int m_generation;
    std::atomic_bool m_cancelled;
};

} // namespace pictureviewer
