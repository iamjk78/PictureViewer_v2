#pragma once

#include <QObject>
#include <QRunnable>
#include <QStringList>

#include <atomic>

namespace pictureviewer {

class FolderScanWorker : public QObject, public QRunnable
{
    Q_OBJECT

public:
    FolderScanWorker(QString folderPath, int generation, QObject *parent = nullptr);

    void cancel();
    void run() override;

signals:
    void scanComplete(int generation, const QStringList &paths);
    void scanError(int generation, const QString &error);
    void finished(int generation);

private:
    QString m_folderPath;
    int m_generation;
    std::atomic_bool m_cancelled;
};

} // namespace pictureviewer
