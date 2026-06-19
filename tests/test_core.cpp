#include <QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "app/CategoryManager.hpp"
#include "app/ImageLoader.hpp"
#include "app/SettingsManager.hpp"
#include "app/SlideshowController.hpp"
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

    // ── ImageCatalog: hraniční případy ──────────────────────────────────────
    void catalog_emptyFolderReturnsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        ImageCatalog catalog;
        const QStringList result = catalog.loadFolder(dir.path(), true);
        QVERIFY(result.isEmpty());
    }

    void catalog_singleFileReturnsOne()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFileOfSize(dir.filePath("only.jpg"), 1));

        ImageCatalog catalog;
        const QStringList result = catalog.loadFolder(dir.path(), true);
        QCOMPARE(result.size(), 1);
        QCOMPARE(QFileInfo(result.first()).fileName(), QString("only.jpg"));
    }

    void catalog_unsupportedFilesIgnored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFileOfSize(dir.filePath("image.jpg"), 1));
        QVERIFY(writeFileOfSize(dir.filePath("document.txt"), 1));
        QVERIFY(writeFileOfSize(dir.filePath("binary.exe"), 1));

        ImageCatalog catalog;
        const QStringList result = catalog.loadFolder(dir.path(), false);
        QCOMPARE(result.size(), 1);
    }

    // ── CategoryManager ──────────────────────────────────────────────────────
    // Po každém testu odstraníme pojmenované DB spojení, aby příští test
    // začínal s čistým stavem (CategoryManager vždy otevírá spojení "categories").
    void categoryManager_cleanup()
    {
        QSqlDatabase::removeDatabase(QStringLiteral("categories"));
    }

    void categoryManager_createAndRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        QVERIFY(mgr.lastError().isEmpty());
        QVERIFY(mgr.allCategories().isEmpty());

        const Category cat = mgr.addCategory(QStringLiteral("Práce"), QColor("#FF0000"));
        QVERIFY(cat.id > 0);
        QCOMPARE(cat.name, QStringLiteral("Práce"));
        QCOMPARE(cat.color, QColor("#FF0000"));
        QVERIFY(mgr.lastError().isEmpty());

        const QList<Category> all = mgr.allCategories();
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().name, QStringLiteral("Práce"));

        categoryManager_cleanup();
    }

    void categoryManager_duplicateNameFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        QVERIFY(mgr.addCategory(QStringLiteral("Dovolená")).id > 0);
        // Druhé vložení se stejným jménem musí selhat
        const Category dup = mgr.addCategory(QStringLiteral("Dovolená"));
        QCOMPARE(dup.id, -1);
        QVERIFY(!mgr.lastError().isEmpty());

        categoryManager_cleanup();
    }

    void categoryManager_delete()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const Category cat = mgr.addCategory(QStringLiteral("Dočasný"));
        QVERIFY(cat.id > 0);

        mgr.deleteCategory(cat.id);
        QVERIFY(mgr.lastError().isEmpty());
        QVERIFY(mgr.allCategories().isEmpty());

        categoryManager_cleanup();
    }

    void categoryManager_rename()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const Category cat = mgr.addCategory(QStringLiteral("Starý název"));
        QVERIFY(mgr.updateCategory(cat.id, QStringLiteral("Nový název"), QColor()));

        const QList<Category> all = mgr.allCategories();
        QCOMPARE(all.first().name, QStringLiteral("Nový název"));

        categoryManager_cleanup();
    }

    void categoryManager_renameToDuplicateFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        mgr.addCategory(QStringLiteral("A"));
        const Category b = mgr.addCategory(QStringLiteral("B"));
        // Přejmenovat B → A musí selhat
        QVERIFY(!mgr.updateCategory(b.id, QStringLiteral("A"), QColor()));

        categoryManager_cleanup();
    }

    void categoryManager_assignAndRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const Category cat = mgr.addCategory(QStringLiteral("Štítek"));
        const QString img = QStringLiteral("/tmp/foto.jpg");

        mgr.assignCategory(img, cat.id);
        QVERIFY(mgr.lastError().isEmpty());

        const QList<Category> assigned = mgr.categoriesForImage(img);
        QCOMPARE(assigned.size(), 1);
        QCOMPARE(assigned.first().id, cat.id);

        categoryManager_cleanup();
    }

    void categoryManager_maxFiveLabels()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const QString img = QStringLiteral("/tmp/foto.jpg");
        // Vytvořit a přiřadit 6 štítků; 6. musí být tiše ignorováno
        for (int i = 1; i <= 6; ++i) {
            const Category cat = mgr.addCategory(QString("Label%1").arg(i));
            QVERIFY(cat.id > 0);
            mgr.assignCategory(img, cat.id);
        }

        const QList<Category> assigned = mgr.categoriesForImage(img);
        QCOMPARE(assigned.size(), 5);   // max 5

        categoryManager_cleanup();
    }

    void categoryManager_unassignAll()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const QString img = QStringLiteral("/tmp/foto.jpg");
        for (int i = 1; i <= 3; ++i) {
            const Category cat = mgr.addCategory(QString("L%1").arg(i));
            mgr.assignCategory(img, cat.id);
        }
        QCOMPARE(mgr.categoriesForImage(img).size(), 3);

        mgr.unassignAll(img);
        QVERIFY(mgr.categoriesForImage(img).isEmpty());

        categoryManager_cleanup();
    }

    void categoryManager_deleteCascadesToAssignments()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const Category cat = mgr.addCategory(QStringLiteral("Dočasný"));
        const QString img = QStringLiteral("/tmp/foto.jpg");
        mgr.assignCategory(img, cat.id);
        QCOMPARE(mgr.categoriesForImage(img).size(), 1);

        mgr.deleteCategory(cat.id);
        // Kaskádové smazání — přiřazení musí zmizet
        QVERIFY(mgr.categoriesForImage(img).isEmpty());

        categoryManager_cleanup();
    }

    void categoryManager_filterByCategory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        const Category red = mgr.addCategory(QStringLiteral("Červená"), QColor("#FF0000"));
        const Category blue = mgr.addCategory(QStringLiteral("Modrá"), QColor("#0000FF"));

        const QString img1 = QStringLiteral("/tmp/a.jpg");
        const QString img2 = QStringLiteral("/tmp/b.jpg");
        mgr.assignCategory(img1, red.id);
        mgr.assignCategory(img2, blue.id);
        mgr.assignCategory(img2, red.id);   // img2 má oba štítky

        // Filtr na červenou → img1 + img2
        const QStringList byRed = mgr.imagePathsWithAllCategories({red.id});
        QCOMPARE(byRed.size(), 2);

        // Filtr na oba → jen img2
        const QStringList byBoth = mgr.imagePathsWithAllCategories({red.id, blue.id});
        QCOMPARE(byBoth.size(), 1);
        QCOMPARE(byBoth.first(), img2);

        // Prázdný filtr → všechny cesty
        const QStringList all = mgr.imagePathsWithAllCategories({});
        QVERIFY(all.size() >= 2);

        categoryManager_cleanup();
    }

    void categoryManager_schemaVersionIsOne()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CategoryManager mgr(dir.filePath("labels.db"));

        int version = 0;
        {
            // QSqlQuery musí být zničen před removeDatabase (jinak Qt varuje)
            QSqlDatabase db = QSqlDatabase::database(QStringLiteral("categories"));
            QVERIFY(db.isOpen());
            QSqlQuery q(db);
            QVERIFY(q.exec("SELECT version FROM schema_version"));
            QVERIFY(q.next());
            version = q.value(0).toInt();
        }
        QCOMPARE(version, 1);

        categoryManager_cleanup();
    }

    // ── SettingsManager ──────────────────────────────────────────────────────
    // Testy používají jedinečné jméno aplikace → vlastní INI soubor,
    // který po sobě uklidí v cleanup.
    void settingsManager_setup()
    {
        qApp->setApplicationName(QStringLiteral("PictureViewerTest"));
        qApp->setOrganizationName(QStringLiteral("PictureViewerTestOrg"));
    }

    void settingsManager_versionIsOne()
    {
        settingsManager_setup();
        SettingsManager mgr;
        QCOMPARE(mgr.settingsVersion(), SettingsManager::kCurrentSettingsVersion);
        QFile::remove(SettingsManager::configFilePath());
    }

    void settingsManager_lastFolderRoundtrip()
    {
        settingsManager_setup();
        SettingsManager mgr;
        const QString folder = QStringLiteral("/Users/test/photos");
        mgr.setLastFolder(folder);
        QCOMPARE(mgr.lastFolder(), folder);
        QFile::remove(SettingsManager::configFilePath());
    }

    void settingsManager_sortKeyRoundtrip()
    {
        settingsManager_setup();
        SettingsManager mgr;
        mgr.setSortKey(2);
        QCOMPARE(mgr.sortKey(), 2);
        mgr.setSortKey(0);
        QCOMPARE(mgr.sortKey(), 0);
        QFile::remove(SettingsManager::configFilePath());
    }

    void settingsManager_boolDefaults()
    {
        settingsManager_setup();
        SettingsManager mgr;
        // Výchozí hodnoty nesmí způsobit crash ani vrátit nesmyslný výsledek
        QVERIFY(mgr.sortAscending());             // výchozí: vzestupně
        QVERIFY(mgr.thumbnailCacheEnabled());     // výchozí: cache zapnutá
        QFile::remove(SettingsManager::configFilePath());
    }

    void settingsManager_windowGeometryRoundtrip()
    {
        settingsManager_setup();
        SettingsManager mgr;
        const QByteArray geom = QByteArray::fromHex("deadbeef");
        mgr.setWindowGeometry(geom);
        QCOMPARE(mgr.windowGeometry(), geom);
        QFile::remove(SettingsManager::configFilePath());
    }

    // ── SlideshowController ──────────────────────────────────────────────────
    void slideshow_initiallyNotRunning()
    {
        SlideshowController ctrl;
        QVERIFY(!ctrl.isRunning());
        QCOMPARE(ctrl.intervalMs(), SlideshowController::DefaultIntervalMs);
    }

    void slideshow_startStop()
    {
        SlideshowController ctrl;
        ctrl.start();
        QVERIFY(ctrl.isRunning());
        ctrl.stop();
        QVERIFY(!ctrl.isRunning());
    }

    void slideshow_toggle()
    {
        SlideshowController ctrl;
        QVERIFY(!ctrl.isRunning());
        ctrl.toggle();
        QVERIFY(ctrl.isRunning());
        ctrl.toggle();
        QVERIFY(!ctrl.isRunning());
    }

    void slideshow_setIntervalClampsToMinimum()
    {
        SlideshowController ctrl;
        ctrl.setInterval(10);   // pod minimem (500 ms)
        QVERIFY(ctrl.intervalMs() >= SlideshowController::MinimumIntervalMs);
    }

    void slideshow_setIntervalValid()
    {
        SlideshowController ctrl;
        ctrl.setInterval(5000);
        QCOMPARE(ctrl.intervalMs(), 5000);
    }

    void slideshow_signalEmittedWhenRunning()
    {
        SlideshowController ctrl;
        QSignalSpy spy(&ctrl, &SlideshowController::nextImageRequested);
        ctrl.setInterval(50);   // velmi krátký interval pro test
        ctrl.start();
        // QTRY_VERIFY opakovaně volí event loop, dokud podmínka není splněna
        // nebo nevyprší 2 s timeout — robustnější než pevný qWait.
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
        ctrl.stop();
    }

    void slideshow_noSignalWhenStopped()
    {
        SlideshowController ctrl;
        QSignalSpy spy(&ctrl, &SlideshowController::nextImageRequested);
        ctrl.setInterval(50);
        // Záměrně nespustit — žádný signál nesmí přijít
        QTest::qWait(200);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestCore)
#include "test_core.moc"
