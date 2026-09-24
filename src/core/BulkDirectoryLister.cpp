#include "core/BulkDirectoryLister.hpp"

#include "core/DiagLog.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QFileInfo>

#include <atomic>

#ifdef Q_OS_MACOS
#include <fcntl.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <vector>
#endif

namespace pictureviewer {

namespace {

std::atomic_bool g_bulkDisabled{false};

bool listWithDirIterator(const QString &directory,
                         const std::function<bool(const ListedFile &)> &onFile)
{
    const QDir dir(directory);
    if (!dir.exists()) {
        return false;
    }
    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        // fileInfo() dělá stat jen tam, kde ho výpis nedodal (Windows ne).
        const QFileInfo info = it.fileInfo();
        if (!onFile(ListedFile{it.fileName(), info.size(),
                               info.lastModified().toSecsSinceEpoch()})) {
            break;
        }
    }
    return true;
}

#ifdef Q_OS_MACOS

enum class BulkResult { Done, Unsupported, OpenFailed };

BulkResult listWithGetattrlistbulk(const QString &directory,
                                   const std::function<bool(const ListedFile &)> &onFile)
{
    const QByteArray path = QFile::encodeName(QDir(directory).absolutePath());
    QElapsedTimer timer;
    timer.start();
    const int fd = ::open(path.constData(), O_RDONLY | O_DIRECTORY);
    diag::log(QStringLiteral("výpis: open() složky %1 ms").arg(timer.elapsed()));
    if (fd < 0) {
        return BulkResult::OpenFailed;
    }

    attrlist request{};
    request.bitmapcount = ATTR_BIT_MAP_COUNT;
    request.commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME | ATTR_CMN_OBJTYPE | ATTR_CMN_MODTIME;
    request.fileattr = ATTR_FILE_DATALENGTH;

    // 256 kB ≈ stovky položek na jedno volání (na jednu síťovou odpověď).
    std::vector<char> buffer(256 * 1024);
    BulkResult result = BulkResult::Done;
    bool first = true;
    bool stop = false;

    int calls = 0;
    qint64 total = 0;
    while (!stop) {
        timer.restart();
        const int count = ::getattrlistbulk(fd, &request, buffer.data(), buffer.size(), 0);
        ++calls;
        // Do logu první volání a každé pomalé — ukáže, jestli se čeká na úložiště.
        if (calls == 1 || timer.elapsed() >= 500) {
            diag::log(QStringLiteral("výpis: getattrlistbulk #%1 vrátil %2 položek za %3 ms")
                          .arg(calls).arg(count).arg(timer.elapsed()));
        }
        total += qMax(count, 0);
        if (count < 0) {
            // Nepodporuje-li to souborový systém a ještě nic nevrátil, zkusí se záloha.
            result = (first && (errno == ENOTSUP || errno == EINVAL))
                ? BulkResult::Unsupported : BulkResult::Done;
            break;
        }
        if (count == 0) {
            break;
        }
        first = false;

        const char *entry = buffer.data();
        for (int i = 0; i < count && !stop; ++i) {
            const char *field = entry;
            uint32_t length;
            std::memcpy(&length, field, sizeof(length));
            field += sizeof(length);

            attribute_set_t returned;
            std::memcpy(&returned, field, sizeof(returned));
            field += sizeof(returned);

            const char *next = entry + length;

            QString name;
            if (returned.commonattr & ATTR_CMN_NAME) {
                attrreference_t ref;
                std::memcpy(&ref, field, sizeof(ref));
                // decodeName sjednotí Unicode na NFC jako QDirIterator — souborový
                // systém vrací rozložené znaky (ě = e + háček) a cesty by pak
                // nesouhlasily s ostatními místy aplikace (cache klíče, páry).
                name = QFile::decodeName(field + ref.attr_dataoffset);
                field += sizeof(ref);
            }
            uint32_t objType = 0;
            if (returned.commonattr & ATTR_CMN_OBJTYPE) {
                std::memcpy(&objType, field, sizeof(objType));
                field += sizeof(objType);
            }
            timespec mtime{};
            if (returned.commonattr & ATTR_CMN_MODTIME) {
                std::memcpy(&mtime, field, sizeof(mtime));
                field += sizeof(mtime);
            }
            off_t size = 0;
            if (returned.fileattr & ATTR_FILE_DATALENGTH) {
                std::memcpy(&size, field, sizeof(size));
            }

            entry = next;
            if (name.isEmpty() || name.startsWith(QLatin1Char('.'))) {
                continue;   // skryté soubory QDir::Files vynechává také
            }

            ListedFile file{name, static_cast<qint64>(size), static_cast<qint64>(mtime.tv_sec)};
            if (objType == VLNK) {
                // Symbolický odkaz: QDir::Files bere odkazy na soubory; údaje
                // cíle se dohledají zvlášť (vzácné).
                struct stat st{};
                const QByteArray full = path + '/' + QFile::encodeName(name);
                if (::stat(full.constData(), &st) != 0 || !S_ISREG(st.st_mode)) {
                    continue;
                }
                file.size = st.st_size;
                file.mtimeSecs = st.st_mtimespec.tv_sec;
            } else if (objType != VREG) {
                continue;
            }
            if (!onFile(file)) {
                stop = true;
            }
        }
    }
    ::close(fd);
    diag::log(QStringLiteral("výpis: hotovo, %1 volání getattrlistbulk, %2 položek").arg(calls).arg(total));
    return result;
}

#endif // Q_OS_MACOS

} // namespace

void setBulkListingDisabledForTesting(bool disabled)
{
    g_bulkDisabled = disabled;
}

bool listFilesBulk(const QString &directory,
                   const std::function<bool(const ListedFile &)> &onFile)
{
#ifdef Q_OS_MACOS
    if (!g_bulkDisabled) {
        switch (listWithGetattrlistbulk(directory, onFile)) {
        case BulkResult::Done:
            return true;
        case BulkResult::OpenFailed:
            return false;
        case BulkResult::Unsupported:
            break;
        }
    }
#endif
    return listWithDirIterator(directory, onFile);
}

} // namespace pictureviewer
