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
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

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

        // Žádný vícenásobný výběr — smaže se jen zobrazený soubor (img_1),
        // ostatní zůstanou, ať je výběr jakýkoli.
        panel->clearSelection();
        QTest::keyClick(&window, Qt::Key_D);

        QTRY_COMPARE_WITH_TIMEOUT(jpgNames(dir.path()),
                                  (QStringList{"img_2.jpg", "img_3.jpg", "img_4.jpg"}), 5000);
        QCOMPARE(jpgNames(QDir(dir.path()).filePath("Delete")), (QStringList{"img_1.jpg"}));

        window.close();
    }

};

QTEST_MAIN(TestGuiStartup)
#include "test_gui_startup.moc"
