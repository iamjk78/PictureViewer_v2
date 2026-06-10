#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "app/ImageLoader.hpp"
#include "app/ThumbnailCacheManager.hpp"
#include "core/ImageCatalog.hpp"
#include "core/ImageFormats.hpp"

using namespace pictureviewer;

namespace {

// Vytvoří prázdný soubor dané velikosti (v bajtech) na dané cestě.
bool writeFileOfSize(const QString &path, qint64 bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (bytes > 0) {
        file.seek(bytes - 1);
        file.write("\0", 1);
    }
    file.close();
    return true;
}

} // namespace

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    // ── ImageFormats ─────────────────────────────────────────────────────────
    void imageFormats_commonExtensionsSupported()
    {
        // JPEG a PNG jsou v každém rozumném Qt buildu.
        QVERIFY(isSupportedImageExtension(".jpg"));
        QVERIFY(isSupportedImageExtension(".png"));
    }

    void imageFormats_caseInsensitive()
    {
        QVERIFY(isSupportedImageExtension(".JPG"));
        QVERIFY(isSupportedImageExtension(".PnG"));
    }

    void imageFormats_unknownRejected()
    {
        QVERIFY(!isSupportedImageExtension(".xyz"));
        QVERIFY(!isSupportedImageExtension(".txt"));
    }

    void imageFormats_pdfDetection()
    {
        QVERIFY(isPdfFile(".pdf"));
        QVERIFY(isPdfFile(".PDF"));
        QVERIFY(!isPdfFile(".jpg"));
        QVERIFY(isSupportedDocumentExtension(".pdf"));
    }

    void imageFormats_combinedFileCheck()
    {
        QVERIFY(isSupportedFileExtension(".jpg"));
        QVERIFY(isSupportedFileExtension(".pdf"));
        QVERIFY(!isSupportedFileExtension(".exe"));
    }

    // ── ImageCatalog: přirozené řazení ───────────────────────────────────────
    void catalog_naturalSort()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Záměrně vytvořeno v "lexikograficky matoucím" pořadí.
        const QStringList names = {
            "img10.jpg", "img2.jpg", "img1.jpg", "img20.jpg", "img3.jpg"
        };
        for (const QString &name : names) {
            QVERIFY(writeFileOfSize(dir.filePath(name), 1));
        }

        ImageCatalog catalog;
        const QStringList result = catalog.loadFolder(dir.path(), true);

        QStringList fileNames;
        for (const QString &path : result) {
            fileNames.append(QFileInfo(path).fileName());
        }

        const QStringList expected = {
            "img1.jpg", "img2.jpg", "img3.jpg", "img10.jpg", "img20.jpg"
        };
        QCOMPARE(fileNames, expected);
    }

    void catalog_sortByNameDescending()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &name : {"a.jpg", "b.jpg", "c.jpg"}) {
            QVERIFY(writeFileOfSize(dir.filePath(name), 1));
        }

        ImageCatalog catalog;
        const QStringList result =
            catalog.loadFolder(dir.path(), true, SortKey::Name, /*ascending=*/false);

        QStringList names;
        for (const QString &p : result) {
            names.append(QFileInfo(p).fileName());
        }
        const QStringList expected = {"c.jpg", "b.jpg", "a.jpg"};
        QCOMPARE(names, expected);
    }

    void catalog_sortBySize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFileOfSize(dir.filePath("small.jpg"), 100));
        QVERIFY(writeFileOfSize(dir.filePath("big.jpg"), 5000));
        QVERIFY(writeFileOfSize(dir.filePath("medium.jpg"), 1000));

        ImageCatalog catalog;

        const QStringList asc =
            catalog.loadFolder(dir.path(), true, SortKey::Size, /*ascending=*/true);
        QCOMPARE(QFileInfo(asc.first()).fileName(), QString("small.jpg"));
        QCOMPARE(QFileInfo(asc.last()).fileName(), QString("big.jpg"));

        const QStringList desc =
            catalog.loadFolder(dir.path(), true, SortKey::Size, /*ascending=*/false);
        QCOMPARE(QFileInfo(desc.first()).fileName(), QString("big.jpg"));
        QCOMPARE(QFileInfo(desc.last()).fileName(), QString("small.jpg"));
    }

    void catalog_sortByDate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Vytvořit soubory a nastavit jim různé časy poslední změny.
        const QString older = dir.filePath("older.jpg");
        const QString newer = dir.filePath("newer.jpg");
        QVERIFY(writeFileOfSize(older, 1));
        QVERIFY(writeFileOfSize(newer, 1));

        QFile fOlder(older);
        QVERIFY(fOlder.open(QIODevice::ReadWrite));
        fOlder.setFileTime(QDateTime::currentDateTime().addDays(-2),
                           QFileDevice::FileModificationTime);
        fOlder.close();

        QFile fNewer(newer);
        QVERIFY(fNewer.open(QIODevice::ReadWrite));
        fNewer.setFileTime(QDateTime::currentDateTime(),
                           QFileDevice::FileModificationTime);
        fNewer.close();

        ImageCatalog catalog;
        const QStringList asc =
            catalog.loadFolder(dir.path(), true, SortKey::Date, /*ascending=*/true);
        QCOMPARE(QFileInfo(asc.first()).fileName(), QString("older.jpg"));
        QCOMPARE(QFileInfo(asc.last()).fileName(), QString("newer.jpg"));
    }

    void catalog_pdfInclusionToggle()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFileOfSize(dir.filePath("photo.jpg"), 1));
        QVERIFY(writeFileOfSize(dir.filePath("doc.pdf"), 1));
        QVERIFY(writeFileOfSize(dir.filePath("notes.txt"), 1));   // nepodporováno

        ImageCatalog catalog;

        const QStringList withPdf = catalog.loadFolder(dir.path(), true);
        QCOMPARE(withPdf.size(), 2);   // jpg + pdf

        const QStringList withoutPdf = catalog.loadFolder(dir.path(), false);
        QCOMPARE(withoutPdf.size(), 1);   // jen jpg
        QCOMPARE(QFileInfo(withoutPdf.first()).fileName(), QString("photo.jpg"));
    }

    void catalog_missingFolderThrows()
    {
        ImageCatalog catalog;
        bool threw = false;
        try {
            catalog.loadFolder("/nonexistent/path/xyz123", true);
        } catch (const std::exception &) {
            threw = true;
        }
        QVERIFY(threw);
    }

    // ── ImageLoader::cacheKey ────────────────────────────────────────────────
    void cacheKey_includesSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("a.bin");

        QVERIFY(writeFileOfSize(path, 100));
        const QString key100 = ImageLoader::cacheKey(path);

        // Stejná cesta, jiná velikost → jiný klíč (chrání před mtime kolizí
        // ve stejné sekundě, kdy se obsah liší velikostí).
        QVERIFY(writeFileOfSize(path, 200));
        const QString key200 = ImageLoader::cacheKey(path);

        QVERIFY(key100 != key200);
        QVERIFY(key100.contains(path));   // klíč obsahuje cestu
    }

    void cacheKey_stableForSameFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("b.bin");
        QVERIFY(writeFileOfSize(path, 50));

        const QString first = ImageLoader::cacheKey(path);
        const QString second = ImageLoader::cacheKey(path);
        QCOMPARE(first, second);   // beze změny souboru je klíč stabilní
    }

    // ── ThumbnailCacheManager::calculateCacheSize ────────────────────────────
    void cacheSize_sumsFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFileOfSize(dir.filePath("t1.png"), 1000));
        QVERIFY(writeFileOfSize(dir.filePath("t2.png"), 2000));
        QVERIFY(writeFileOfSize(dir.filePath("t3.png"), 3000));

        const qint64 size = ThumbnailCacheManager::calculateCacheSize(dir.path());
        QCOMPARE(size, qint64(6000));
    }

    void cacheSize_emptyDirIsZero()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QCOMPARE(ThumbnailCacheManager::calculateCacheSize(dir.path()), qint64(0));
    }

    void cacheSize_nonexistentDirIsZero()
    {
        QCOMPARE(ThumbnailCacheManager::calculateCacheSize("/nonexistent/xyz123"),
                 qint64(0));
    }

    void cleanup_belowLimitKeepsFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Malá cache hluboko pod limitem 500 MB — nic se nesmí smazat,
        // i kdyby byly soubory staré (logika maže až při dosažení limitu).
        QVERIFY(writeFileOfSize(dir.filePath("keep.png"), 1000));

        ThumbnailCacheManager::cleanupIfNeeded(dir.path());

        QVERIFY(QFile::exists(dir.filePath("keep.png")));
    }
};

QTEST_MAIN(TestCore)
#include "test_core.moc"
