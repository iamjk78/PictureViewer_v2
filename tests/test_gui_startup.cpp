// Smoke test spuštění aplikace.
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

#include "app/MainWindow.hpp"

using namespace pictureviewer;

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
};

QTEST_MAIN(TestGuiStartup)
#include "test_gui_startup.moc"
