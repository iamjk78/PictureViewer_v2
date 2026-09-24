#pragma once

#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QString>

namespace pictureviewer {

// Čas změny a velikost souboru tak, jak je dodal výpis složky.
struct FileStamp {
    qint64 mtimeSecs = 0;
    qint64 size = 0;
};

// Otisky souborů z posledního výpisu složky, sdílené mezi vlákny (skener je
// plní, generátory miniatur čtou). Klíč diskové cache miniatur obsahuje mtime
// a velikost; bez tohoto indexu by každá miniatura stála jedno stat() — přes
// síť desetiny sekundy. Soubor, který aplikace sama změnila, se z indexu
// odebere (remove) a klíč se pak vezme z čerstvého stat().
class FileStampIndex
{
public:
    void set(const QString &path, FileStamp stamp)
    {
        QMutexLocker lock(&m_mutex);
        m_stamps.insert(path, stamp);
    }
    void remove(const QString &path)
    {
        QMutexLocker lock(&m_mutex);
        m_stamps.remove(path);
    }
    bool get(const QString &path, FileStamp *out) const
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_stamps.constFind(path);
        if (it == m_stamps.cend()) {
            return false;
        }
        *out = it.value();
        return true;
    }

    // Otisk z indexu, jinak (nebo bez indexu) čerstvý stat().
    static FileStamp resolve(const QSharedPointer<FileStampIndex> &index, const QString &path)
    {
        FileStamp stamp;
        if (index && index->get(path, &stamp)) {
            return stamp;
        }
        const QFileInfo info(path);
        return {info.lastModified().toSecsSinceEpoch(), info.size()};
    }

private:
    mutable QMutex m_mutex;
    QHash<QString, FileStamp> m_stamps;
};

} // namespace pictureviewer
