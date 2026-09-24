// Smoke test spuštění aplikace + end-to-end test mazání výběru náhledů.
//
// Běží s QT_QPA_PLATFORM=offscreen (nastaveno v CMakeLists.txt), takže funguje
// i na CI bez displeje — na macOS i na Windows.
//
// CO POKRÝVÁ: postaví skutečné MainWindow, zobrazí ho, zavře (což uloží
// rozložení oken) a celý cyklus zopakuje, takže druhé a třetí kolo startuje
// nad uloženým stavem. Odhalí hrubé pády při sestavení okna — výjimku
// v konstruktoru, dereferenci nullptr, chybějící prostředek.
//
// CO NEPOKRÝVÁ (ověřeno 2026-08-16): pád, kvůli kterému test vznikl —
// poškození QToolBarAreaLayout přesunem toolbaru po restoreState() ve verzích
// 0.29–0.31. Chybu jsme do kódu dočasně vrátili a test ji NEODHALIL, a to ani
// s offscreen platformou, ani se skutečnou. Ten pád závisí na podmínkách,
// které se v testovacím prostředí nesejdou (skutečná geometrie okna, obsah
// panelu náhledů, konkrétní uložené rozložení).
//
// Test tedy NENÍ pojistkou proti opakování té konkrétní chyby. Startovací
// cesta zůstává odkázaná na ruční ověření na obou platformách.

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QAbstractButton>
#include <QMessageBox>
#include <QProgressDialog>
#include <QLabel>
#include <QListWidget>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QThread>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/BatchProgress.hpp"
#include "app/MainWindow.hpp"
#include "app/ThumbnailPanel.hpp"

using namespace pictureviewer;

namespace {

void writeConfigForDelete()
{
    const QString cfgDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString profileCfg = cfgDir + "/profiles/Výchozí/config.ini";
    QDir().mkpath(QFileInfo(profileCfg).absolutePath());
    QSettings s(profileCfg, QSettings::IniFormat);
    s.setValue("FileHandling/enable_delete_image", false);
    s.setValue("FileHandling/enable_move_to_delete", true);
    s.setValue("FileHandling/ask_confirmation_delete", false);
    s.setValue("FileHandling/move_companion_files", false);
    s.setValue("Processing/enable_images", true);
    s.sync();
}

QStringList makeImages(const QString &dir, int count)
{
    QStringList names;
    for (int i = 1; i <= count; ++i) {
        const QString name = QStringLiteral("img_%1.jpg").arg(i);
        QImage img(16, 16, QImage::Format_RGB32);
        img.fill(Qt::blue);
        img.save(QDir(dir).filePath(name), "JPEG");
        names.append(name);
    }
    return names;
}

QStringList jpgNames(const QString &dir)
{
    return QDir(dir).entryList({QStringLiteral("*.jpg")}, QDir::Files, QDir::Name);
}

// Zapamatovaná poslední složka + zapnutý navigační toolbar (kvůli sondě).
void writeConfigForRestore(const QString &folder, bool remember)
{
    const QString cfgDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString profileCfg = cfgDir + "/profiles/Výchozí/config.ini";
    QDir().mkpath(QFileInfo(profileCfg).absolutePath());
    QSettings s(profileCfg, QSettings::IniFormat);
    s.setValue("General/remember_last_folder", remember);
    s.setValue("General/last_folder", folder);
    s.setValue("Navigation/toolbar_visible", true);
    s.setValue("FileHandling/enable_delete_image", false);
    s.setValue("FileHandling/enable_move_to_delete", false);
    s.sync();
}

bool statusMentions(const QWidget &window, const QString &needle)
{
    for (const QLabel *label : window.findChildren<QLabel *>()) {
        if (label->text().contains(needle)) {
            return true;
        }
    }
    return false;
}

QStringList makeImagePaths(const QString &dir, int count)
{
    QStringList paths;
    for (int i = 1; i <= count; ++i) {
        const QString path = QDir(dir).filePath(QStringLiteral("img_%1.jpg").arg(i, 4, 10, QLatin1Char('0')));
        QImage img(16, 16, QImage::Format_RGB32);
        img.fill(Qt::green);
        img.save(path, "JPEG");
        paths.append(path);
    }
    return paths;
}

int cachedThumbFiles(const QString &cacheDir)
{
    int n = 0;
    QDirIterator it(cacheDir, {QStringLiteral("*.thumb")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}

// Za chvíli klikne v modálním QMessageBoxu na tlačítko s daným textem.
void clickMessageBoxButtonSoon(const QString &text, int delayMs)
{
    QTimer::singleShot(delayMs, [text] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(w)) {
                for (QAbstractButton *b : box->buttons()) {
                    if (b->text() == text) {
                        b->click();
                        return;
                    }
                }
            }
        }
    });
}

// Za chvíli zruší otevřený QProgressDialog (jako klik na Zrušit).
void cancelProgressDialogSoon(int delayMs)
{
    QTimer::singleShot(delayMs, [] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *pd = qobject_cast<QProgressDialog *>(w)) {
                pd->cancel();
                return;
            }
        }
    });
}

void writeConfigForBatch(bool companions)
{
    const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString profileCfg = cfgDir + "/profiles/Výchozí/config.ini";
    QDir().mkpath(QFileInfo(profileCfg).absolutePath());
    QSettings s(profileCfg, QSettings::IniFormat);
    s.setValue("General/remember_last_folder", false);
    s.setValue("FileHandling/enable_delete_image", false);
    s.setValue("FileHandling/enable_move_to_delete", true);
    s.setValue("FileHandling/ask_confirmation_delete", false);
    s.setValue("FileHandling/move_companion_files", companions);
    s.setValue("Processing/enable_images", true);
    s.setValue("Processing/enable_videos", true);
    s.setValue("Sort/key", 0);
    s.setValue("Sort/ascending", true);
    s.sync();
}

int loadedThumbnails(const ThumbnailPanel &panel)
{
    int n = 0;
    for (int i = 0; i < panel.count(); ++i) {
        if (!panel.iconAt(i).isNull()) {
            ++n;
        }
    }
    return n;
}

} // namespace

class TestGuiStartup : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Přesměrovat config/profily do testovacího umístění — test nesmí
        // sahat na skutečné nastavení uživatele ani ho přepsat.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("PictureViewerTest"));
        QCoreApplication::setApplicationName(QStringLiteral("PictureViewerGuiTest"));
    }

    void startupSurvivesSavedWindowState()
    {
        // Zviditelnit všechny sekundární toolbary — uložené rozložení pak má
        // víc řádků, tedy blíž reálnému stavu, ve kterém se pády projevovaly.
        {
            const QString cfgDir =
                QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            const QString profileCfg = cfgDir + "/profiles/Výchozí/config.ini";
            QDir().mkpath(QFileInfo(profileCfg).absolutePath());
            QSettings s(profileCfg, QSettings::IniFormat);
            s.setValue("Favorites/toolbar_visible", true);
            s.setValue("Categories/toolbar_visible", true);
            s.setValue("Move/toolbar_visible", true);
            s.setValue("Navigation/toolbar_visible", true);
            s.sync();
        }

        // Tři kola: druhé a třetí startují nad stavem uloženým tím předchozím —
        // právě takhle se projevil pád v 0.29–0.31 (poprvé aplikace naběhla,
        // podruhé už ne).
        for (int run = 1; run <= 3; ++run) {
            MainWindow window;
            window.show();
            // Layout se aktivuje během show(); průchody event loopou navíc
            // pokryjí i práci odloženou přes QTimer::singleShot().
            QTest::qWait(200);

            QVERIFY2(window.isVisible(),
                     qPrintable(QStringLiteral("okno není viditelné v %1. kole").arg(run)));

            // close() spustí closeEvent(), který uloží geometrii i stav oken.
            window.close();
            QTest::qWait(100);
        }
    }

    void cleanupTestCase()
    {
        // Uklidit testovací konfiguraci. Pojistka proti smazání něčeho jiného:
        // mazat jen cestu, kterou Qt v testovacím režimu skutečně označí.
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (dir.contains(QLatin1String("qttest"))
            && dir.contains(QLatin1String("PictureViewerGuiTest"))) {
            QDir(dir).removeRecursively();
        }
    }
    // ── Hromadné mazání výběru náhledů ───────────────────────────────────────
    // Skutečné MainWindow nad složkou s obrázky. Režim "přesun do složky Delete"
    // místo koše — test tak nesahá do uživatelova koše a jde přesně ověřit,
    // kam soubory přistály. Klávesa jde na panel náhledů (tam má fokus po
    // Ctrl/Shift+kliku) — ověřuje tedy i to, že ji panel nepohltí.

    void deleteKeyRemovesAllSelectedThumbnails()
    {
        writeConfigForDelete();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImages(dir.path(), 6);

        MainWindow window;
        window.show();
        window.openFile(QDir(dir.path()).filePath("img_1.jpg"));

        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 6, 5000);
        // Sken musí skončit — hromadné mazání za jeho běhu se ptá na potvrzení.
        QTRY_VERIFY_WITH_TIMEOUT(!statusMentions(window, QStringLiteral("Načítám složku…")), 5000);

        // Označit náhledy 2, 3 a 4 (indexy 1–3) — aktuální zůstává první.
        panel->clearSelection();
        for (int row : {1, 2, 3}) {
            panel->item(row)->setSelected(true);
        }
        QCOMPARE(panel->selectedIndices().size(), 3);

        QTest::keyClick(panel, Qt::Key_Delete);

        // Zbývají 1, 5, 6 — označené 2, 3, 4 odešly do složky Delete.
        QTRY_COMPARE_WITH_TIMEOUT(jpgNames(dir.path()),
                                  (QStringList{"img_1.jpg", "img_5.jpg", "img_6.jpg"}), 5000);
        QCOMPARE(jpgNames(QDir(dir.path()).filePath("Delete")),
                 (QStringList{"img_2.jpg", "img_3.jpg", "img_4.jpg"}));
        QCOMPARE(panel->count(), 3);

        window.close();
    }

    void deleteKeyWithoutSelectionRemovesOnlyCurrent()
    {
        writeConfigForDelete();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImages(dir.path(), 4);

        MainWindow window;
        window.show();
        window.openFile(QDir(dir.path()).filePath("img_1.jpg"));

        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 4, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!statusMentions(window, QStringLiteral("Načítám složku…")), 5000);

        // Žádný vícenásobný výběr — smaže se jen zobrazený soubor (img_1),
        // ostatní zůstanou, ať je výběr jakýkoli.
        panel->clearSelection();
        QTest::keyClick(&window, Qt::Key_D);

        QTRY_COMPARE_WITH_TIMEOUT(jpgNames(dir.path()),
                                  (QStringList{"img_2.jpg", "img_3.jpg", "img_4.jpg"}), 5000);
        QCOMPARE(jpgNames(QDir(dir.path()).filePath("Delete")), (QStringList{"img_1.jpg"}));

        window.close();
    }

    // ── Časový limit obnovy poslední složky ──────────────────────────────────
    void restoreLastFolder_opensFolderWhenStorageIsFast()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImages(dir.path(), 3);
        writeConfigForRestore(dir.path(), true);
        auto reset = qScopeGuard([] {
            MainWindow::setRestoreHooksForTesting(0, {});
            writeConfigForRestore(QString(), false);
        });

        MainWindow::setRestoreHooksForTesting(0, {});   // výchozí sonda i limit
        MainWindow window;
        window.show();

        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 3, 5000);
        QVERIFY(!statusMentions(window, QStringLiteral("trvalo déle než")));
        window.close();
    }

    void restoreLastFolder_givesUpWhenStorageIsSlow()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImages(dir.path(), 3);
        writeConfigForRestore(dir.path(), true);
        auto reset = qScopeGuard([] {
            MainWindow::setRestoreHooksForTesting(0, {});
            writeConfigForRestore(QString(), false);
        });

        // Pomalé úložiště: sonda spí 1,5 s, limit je 300 ms.
        MainWindow::setRestoreHooksForTesting(300, [](const QString &) {
            QThread::msleep(1500);
        });
        MainWindow window;
        window.show();

        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);

        // Limit vypršel → aplikace je bez složky a řekla proč.
        QTRY_VERIFY_WITH_TIMEOUT(statusMentions(window, QStringLiteral("trvalo déle než")), 3000);
        QCOMPARE(panel->count(), 0);

        // Sonda doběhne až POTOM — její pozdní výsledek nesmí složku otevřít.
        QTest::qWait(2000);
        QCOMPARE(panel->count(), 0);
        window.close();
    }

    void restoreLastFolder_doesNothingWhenRememberingIsOff()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImages(dir.path(), 3);
        writeConfigForRestore(dir.path(), false);
        auto reset = qScopeGuard([] { writeConfigForRestore(QString(), false); });

        MainWindow window;
        window.show();
        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTest::qWait(500);
        QCOMPARE(panel->count(), 0);
        window.close();
    }
    // ── Líné generování miniatur ─────────────────────────────────────────────
    // Samostatný panel (bez okna) — cílem je chování "jen pro viditelné".

    void thumbnails_areGeneratedOnlyForVisibleItems()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 300);

        ThumbnailPanel panel;
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.loadImages(paths);

        // Nahoře (viditelné) se miniatury objeví…
        QTRY_VERIFY_WITH_TIMEOUT(!panel.iconAt(0).isNull(), 5000);
        QTest::qWait(800);   // ať se dokončí i okolí

        // …ale ne pro celou složku: dřív se generovalo všech 300 hned.
        const int loaded = loadedThumbnails(panel);
        QVERIFY2(loaded < 60, qPrintable(QStringLiteral("načteno %1 z 300").arg(loaded)));
        QVERIFY(panel.iconAt(299).isNull());
        QVERIFY(panel.iconAt(150).isNull());
    }

    void thumbnails_followTheScrollPosition()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 300);

        ThumbnailPanel panel;
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.loadImages(paths);
        QTRY_VERIFY_WITH_TIMEOUT(!panel.iconAt(0).isNull(), 5000);

        // Skok na konec seznamu: miniatury se dogenerují tam, kde uživatel je,
        // a prostředek seznamu, který přeskočil, zůstane nedotčený.
        panel.setCurrentIndex(299);
        QTRY_VERIFY_WITH_TIMEOUT(!panel.iconAt(299).isNull(), 5000);
        QVERIFY(panel.iconAt(150).isNull());
    }

    void thumbnails_hiddenPanelDoesNoWork()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 40);

        ThumbnailPanel panel;   // záměrně NEzobrazený (jako zavřený dock)
        panel.resize(220, 480);
        panel.loadImages(paths);
        QTest::qWait(600);
        QCOMPARE(loadedThumbnails(panel), 0);

        // Po zobrazení se dogenerují.
        panel.show();
        QTRY_VERIFY_WITH_TIMEOUT(!panel.iconAt(0).isNull(), 5000);
    }

    void thumbnails_requestOnlyVisibleVideos()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // 3 videa nahoře a 3 na úplném konci dlouhého seznamu.
        QStringList paths;
        for (int i = 0; i < 3; ++i) {
            paths << QDir(dir.path()).filePath(QStringLiteral("a_clip%1.mp4").arg(i));
        }
        paths << makeImagePaths(dir.path(), 200);
        for (int i = 0; i < 3; ++i) {
            paths << QDir(dir.path()).filePath(QStringLiteral("z_clip%1.mp4").arg(i));
        }
        for (const QString &p : paths) {
            if (p.endsWith(".mp4")) {
                QFile f(p);
                QVERIFY(f.open(QIODevice::WriteOnly));   // obsah nevadí, panel jen čte název
            }
        }

        ThumbnailPanel panel;
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QSignalSpy spy(&panel, &ThumbnailPanel::videoThumbnailsWanted);
        panel.loadImages(paths);

        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
        const QStringList wanted = spy.last().at(1).toStringList();
        // Videa nahoře ano, videa 200 položek níž ne — a žádný obrázek mezi nimi.
        QVERIFY(wanted.contains(paths.first()));
        for (const QString &p : wanted) {
            QVERIFY2(p.endsWith(".mp4"), qPrintable(p));
            QVERIFY2(!p.contains("z_clip"), qPrintable(p));
        }
    }

    void thumbnails_replacingTheListMidFlightIsSafe()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 120);

        ThumbnailPanel panel;
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        // Rychlé střídání seznamů (jako přepínání složek/filtrů) — rozpracované
        // workery starého seznamu se nesmí dotknout nového.
        for (int i = 0; i < 6; ++i) {
            panel.loadImages(i % 2 == 0 ? paths : paths.mid(40));
            QTest::qWait(30);
        }
        panel.loadImages(paths);
        QTRY_VERIFY_WITH_TIMEOUT(!panel.iconAt(0).isNull(), 5000);
        panel.shutdown();   // po něm už žádná práce nesmí vzniknout
        QTest::qWait(300);
    }
    // ── Zahřívání cache na pozadí ────────────────────────────────────────────
    void warmup_cachesTheWholeFolderWithoutHoldingIcons()
    {
        QTemporaryDir dir, cache;
        QVERIFY(dir.isValid() && cache.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 80);

        ThumbnailPanel panel;
        panel.setDiskCache(true, cache.path());
        panel.setWarmupTimingForTesting(100, 0);
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.loadImages(paths);

        // Postupně se do cache dostanou VŠECHNY miniatury složky, i ty, které
        // nikdo nikdy neviděl…
        QTRY_COMPARE_WITH_TIMEOUT(cachedThumbFiles(cache.path()), 80, 15000);
        // …ale panel kvůli tomu nedrží ikony celé složky (paměť): vzdálené
        // položky ikonu nemají.
        QVERIFY(panel.iconAt(79).isNull());
        QVERIFY(panel.iconAt(40).isNull());
        QVERIFY(loadedThumbnails(panel) < 60);
    }

    void warmup_pausesWhileTheUserIsActive()
    {
        QTemporaryDir dir, cache;
        QVERIFY(dir.isValid() && cache.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 60);

        ThumbnailPanel panel;
        panel.setDiskCache(true, cache.path());
        panel.setWarmupTimingForTesting(100, 40);   // pomalé, ať je co přerušit
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.loadImages(paths);

        // Počkat, až zahřívání rozběhne (víc než jen viditelné položky)…
        QTRY_VERIFY_WITH_TIMEOUT(cachedThumbFiles(cache.path()) >= 12, 10000);
        QVERIFY(cachedThumbFiles(cache.path()) < 60);

        // …a pak uživatel "něco dělá" (stále ta samá pozice, takže popředí
        // nemá co nového — případný přírůstek v cache jde jen z pozadí).
        QTest::qWait(150);   // nechat doběhnout rozdělanou miniaturu
        const int before = cachedThumbFiles(cache.path());
        for (int i = 0; i < 16; ++i) {
            panel.setCurrentIndex(0);
            QTest::qWait(50);
        }
        const int during = cachedThumbFiles(cache.path());
        QVERIFY2(during <= before + 2,
                 qPrintable(QStringLiteral("během aktivity přibylo %1 souborů").arg(during - before)));

        // Po klidu zahřívání naváže a dokončí.
        QTRY_COMPARE_WITH_TIMEOUT(cachedThumbFiles(cache.path()), 60, 15000);
    }
    void warmup_addsOtherVideosOnlyWhenIdle()
    {
        QTemporaryDir dir, cache;
        QVERIFY(dir.isValid() && cache.isValid());
        QStringList paths;
        paths << QDir(dir.path()).filePath("a_clip.mp4");
        paths << makeImagePaths(dir.path(), 200);
        paths << QDir(dir.path()).filePath("z_clip.mp4");
        for (const QString &p : {paths.first(), paths.last()}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        ThumbnailPanel panel;
        panel.setDiskCache(true, cache.path());
        panel.setWarmupTimingForTesting(150, 0);
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QSignalSpy spy(&panel, &ThumbnailPanel::videoThumbnailsWanted);
        panel.loadImages(paths);

        auto lastHasFarVideo = [&spy] {
            return !spy.isEmpty() && spy.last().at(1).toStringList().join('|').contains("z_clip");
        };

        // Nejdřív jen viditelné video, vzdálené ne…
        QTRY_VERIFY_WITH_TIMEOUT(!spy.isEmpty(), 5000);
        QVERIFY(!spy.first().at(1).toStringList().join('|').contains("z_clip"));
        // …v klidu se za něj přidá i zbytek (nízká priorita)…
        QTRY_VERIFY_WITH_TIMEOUT(lastHasFarVideo(), 8000);
        // …a jakmile uživatel něco udělá, zase jen viditelná.
        panel.setCurrentIndex(0);
        QTRY_VERIFY_WITH_TIMEOUT(!lastHasFarVideo(), 3000);
    }
    void warmup_waitsWhileViewerLoadsAnImage()
    {
        QTemporaryDir dir, cache;
        QVERIFY(dir.isValid() && cache.isValid());
        const QStringList paths = makeImagePaths(dir.path(), 40);

        ThumbnailPanel panel;
        panel.setDiskCache(true, cache.path());
        panel.setWarmupTimingForTesting(100, 0);
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.setViewerBusy(true);   // právě se načítá prohlížený obrázek
        panel.loadImages(paths);

        // I po uplynutí doby klidu zahřívání nezačne: prohlížení má přednost.
        QTest::qWait(1200);
        const int whileBusy = cachedThumbFiles(cache.path());
        QVERIFY2(whileBusy < 40, qPrintable(QStringLiteral("zahřáto %1 i přes busy").arg(whileBusy)));

        // Až prohlížeč skončí, po klidu naváže a dokončí.
        panel.setViewerBusy(false);
        QTRY_COMPARE_WITH_TIMEOUT(cachedThumbFiles(cache.path()), 40, 10000);
    }

    void warmup_videosComeOnlyAfterImagesAreCached()
    {
        QTemporaryDir dir, cache;
        QVERIFY(dir.isValid() && cache.isValid());
        QStringList paths;
        paths << QDir(dir.path()).filePath("a_clip.mp4");
        paths << makeImagePaths(dir.path(), 60);
        paths << QDir(dir.path()).filePath("z_clip.mp4");
        for (const QString &p : {paths.first(), paths.last()}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        ThumbnailPanel panel;
        panel.setDiskCache(true, cache.path());
        panel.setWarmupTimingForTesting(100, 20);   // obrázky trvají ~1 s
        panel.resize(220, 480);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QSignalSpy spy(&panel, &ThumbnailPanel::videoThumbnailsWanted);
        panel.loadImages(paths);

        // Vzdálené video se objeví až POTÉ, co jsou v cache všechny obrázky…
        QTRY_VERIFY_WITH_TIMEOUT(
            !spy.isEmpty() && spy.last().at(1).toStringList().join('|').contains("z_clip"), 15000);
        QCOMPARE(cachedThumbFiles(cache.path()), 60);
        // …a viditelné video je v popředí (jedno), vzdálené v ocasu.
        QCOMPARE(spy.last().at(2).toInt(), 1);
    }
    // ── Hromadné operace: páry z paměti, průběh se zrušením, varování ─────────
    void batch_deleteMovesPairsTogether()
    {
        writeConfigForBatch(/*companions*/ true);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // 30 samotných obrázků nahoře (a_*) a 6 dvojic obrázek+video dole (z_*),
        // ať videa nejsou vidět a nespouští generátor miniatur.
        for (int i = 1; i <= 30; ++i) {
            QImage img(16, 16, QImage::Format_RGB32);
            img.fill(Qt::red);
            img.save(QDir(dir.path()).filePath(QStringLiteral("a_%1.jpg").arg(i, 2, 10, QLatin1Char('0'))), "JPEG");
        }
        for (int i = 1; i <= 6; ++i) {
            QImage img(16, 16, QImage::Format_RGB32);
            img.fill(Qt::blue);
            img.save(QDir(dir.path()).filePath(QStringLiteral("z_%1.jpg").arg(i)), "JPEG");
            QFile f(QDir(dir.path()).filePath(QStringLiteral("z_%1.mp4").arg(i)));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        MainWindow window;
        window.show();
        window.openFile(QDir(dir.path()).filePath("a_01.jpg"));
        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 42, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(!statusMentions(window, QStringLiteral("Načítám složku…")), 5000);

        // Označit tři z dvojic (jejich .jpg) — s nimi se musí přesunout i videa.
        panel->clearSelection();
        int selected = 0;
        for (int row = 0; row < panel->count(); ++row) {
            const QString name = QFileInfo(panel->item(row)->data(Qt::UserRole).toString()).fileName();
            if (name == "z_1.jpg" || name == "z_2.jpg" || name == "z_3.jpg") {
                panel->item(row)->setSelected(true);
                ++selected;
            }
        }
        QCOMPARE(selected, 3);
        QTest::keyClick(panel, Qt::Key_Delete);

        const QString deleted = QDir(dir.path()).filePath("Delete");
        QTRY_COMPARE_WITH_TIMEOUT(QDir(deleted).entryList(QDir::Files, QDir::Name),
            (QStringList{"z_1.jpg", "z_1.mp4", "z_2.jpg", "z_2.mp4", "z_3.jpg", "z_3.mp4"}), 8000);
        // ostatní dvojice zůstaly
        QVERIFY(QFile::exists(QDir(dir.path()).filePath("z_4.mp4")));
        QVERIFY(QFile::exists(QDir(dir.path()).filePath("z_4.jpg")));
        window.close();
    }

    void batch_cancelStopsTheOperationHalfway()
    {
        writeConfigForBatch(false);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImagePaths(dir.path(), 40);
        auto reset = qScopeGuard([] { BatchProgress::setStepDelayForTesting(0); });

        MainWindow window;
        window.show();
        window.openFile(QDir(dir.path()).filePath("img_0001.jpg"));
        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 40, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(!statusMentions(window, QStringLiteral("Načítám složku…")), 5000);

        panel->clearSelection();
        for (int row = 0; row < 30; ++row) {
            panel->item(row)->setSelected(true);
        }
        BatchProgress::setStepDelayForTesting(120);   // pomalé úložiště
        cancelProgressDialogSoon(700);                // uživatel klikne na Zrušit
        QTest::keyClick(panel, Qt::Key_Delete);

        const QString deleted = QDir(dir.path()).filePath("Delete");
        const int moved = QDir(deleted).entryList(QDir::Files).size();
        QVERIFY2(moved > 0 && moved < 30,
                 qPrintable(QStringLiteral("přesunuto %1 z 30").arg(moved)));
        QCOMPARE(QDir(dir.path()).entryList({"*.jpg"}, QDir::Files).size(), 40 - moved);
        QVERIFY(statusMentions(window, QStringLiteral("Přerušeno")));
        window.close();
    }

    // Důkaz, že se páry berou ze seznamu v aplikaci, ne z disku (na síťovém
    // úložišti stál dotaz na disk ~30 síťových volání na soubor): video, které
    // se na disku objevilo AŽ PO dokončení načtení, seznam nezná, takže se s
    // obrázkem nepřesune. Je to vědomý kompromis — F5 seznam obnoví.
    void batch_pairsComeFromTheListNotFromTheDisk()
    {
        writeConfigForBatch(/*companions*/ true);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        makeImagePaths(dir.path(), 5);

        MainWindow window;
        window.show();
        window.openFile(QDir(dir.path()).filePath("img_0001.jpg"));
        auto *panel = window.findChild<ThumbnailPanel *>();
        QVERIFY(panel != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(panel->count(), 5, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(!statusMentions(window, QStringLiteral("Načítám složku…")), 5000);

        // "Nové" video k img_0001 přibude na disk až teď.
        QFile late(QDir(dir.path()).filePath("img_0001.mp4"));
        QVERIFY(late.open(QIODevice::WriteOnly));
        late.close();

        QMetaObject::invokeMethod(panel, "imageSelected", Q_ARG(int, 0));
        panel->clearSelection();
        panel->item(0)->setSelected(true);
        panel->item(1)->setSelected(true);
        QTest::keyClick(panel, Qt::Key_Delete);

        const QString deleted = QDir(dir.path()).filePath("Delete");
        QTRY_COMPARE_WITH_TIMEOUT(QDir(deleted).entryList(QDir::Files, QDir::Name),
                                  (QStringList{"img_0001.jpg", "img_0002.jpg"}), 5000);
        QVERIFY(QFile::exists(QDir(dir.path()).filePath("img_0001.mp4")));   // pár z disku se nehledal
        window.close();
    }
};

QTEST_MAIN(TestGuiStartup)
#include "test_gui_startup.moc"
