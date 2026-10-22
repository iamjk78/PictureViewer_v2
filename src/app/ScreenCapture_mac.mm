// macOS implementace výřezu obrazovky — Objective-C++ soubor.

#include "ScreenCapture.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QWidget>

#import <AppKit/AppKit.h>

namespace pictureviewer {

// screenshotTempDir() / makeScreenshotPath() jsou sdílené — viz ScreenCapture.cpp.

ScreenCaptureResult captureScreenRegion(QWidget *parentWindow)
{
    // ── Oprávnění ────────────────────────────────────────────────────────────
    // Záměrně NEVOLÁME CGPreflightScreenCaptureAccess / CGRequestScreenCaptureAccess.
    // Tato volání způsobovala nekonečnou smyčku — vrátí false pro aktuální
    // běžící process dokud není aplikace restartována po udělení oprávnění v TCC.
    // screencapture je systémový nástroj s vlastní TCC správou; macOS ukáže dialog
    // pro PictureViewer při prvním spuštění a poté ho uloží pro bundle ID aplikace.
    // Po restartu aplikace oprávnění přetrvá.

    // Pojistka proti vnoření (smyčka událostí níže zpracovává i položky menu).
    static bool s_capturing = false;
    if (s_capturing) {
        return {};
    }
    s_capturing = true;
    struct Reset { ~Reset() { s_capturing = false; } } resetCapturing;

    // ── Skrýt okno aplikace ───────────────────────────────────────────────────
    const bool wasVisible = parentWindow && parentWindow->isVisible();
    if (parentWindow) {
        parentWindow->hide();
        QApplication::processEvents();
    }

    // ── Spustit screencapture do souboru ─────────────────────────────────────
    // Výstup do souboru je spolehlivější než clipboard (-c) — snadno ověříme
    // zda soubor vznikl a neřešíme TIFF/PNG deserializaci ze schránky.
    // -i: interaktivní výběr  -s: obdélník  -x: bez zvuku
    const QString tempPath = makeScreenshotPath();
    QProcess proc;
    // Místní smyčka událostí místo waitForFinished(-1): to držel UI vlákno po celou
    // dobu výběru (a navždy, kdyby screencapture visel na dialogu oprávnění).
    QEventLoop loop;
    QObject::connect(&proc, &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&proc, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    proc.start(QStringLiteral("/usr/sbin/screencapture"),
               {QStringLiteral("-i"), QStringLiteral("-s"),
                QStringLiteral("-x"), tempPath});
    if (proc.state() != QProcess::NotRunning) {
        loop.exec();
    }

    // ── Vrátit okno aplikace ──────────────────────────────────────────────────
    if (parentWindow && wasVisible) {
        parentWindow->show();
        parentWindow->raise();
        parentWindow->activateWindow();
    }

    // ── Zkontrolovat výsledek ─────────────────────────────────────────────────
    const QFileInfo fi(tempPath);
    if (!fi.exists() || fi.size() == 0) {
        // Soubor nevznikl = uživatel stiskl Esc, nebo oprávnění nebylo uděleno.
        QFile::remove(tempPath);
        return {};
    }

    const QImage img(tempPath);
    if (img.isNull()) {
        QFile::remove(tempPath);
        return {};
    }

    // Kanonická cesta vyřeší /var → /private/var symlink na macOS, aby cesta
    // souhlasila s tím, co vrátí FolderScanWorker při hledání m_requestedFile.
    const QString canonical = fi.canonicalFilePath();
    return { img, canonical.isEmpty() ? tempPath : canonical };
}

} // namespace pictureviewer
