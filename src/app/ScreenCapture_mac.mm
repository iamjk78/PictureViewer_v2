// macOS implementace výřezu obrazovky — Objective-C++ soubor.

#include "ScreenCapture.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QWidget>

#import <AppKit/AppKit.h>

namespace pictureviewer {

namespace {

QString screenshotTempDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dir  = QDir(base).filePath(QStringLiteral("PictureViewer_Screenshots"));
    QDir().mkpath(dir);
    return dir;
}

QString makeScreenshotPath()
{
    const QString name = QStringLiteral("screenshot_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    return QDir(screenshotTempDir()).filePath(name);
}

} // namespace

ScreenCaptureResult captureScreenRegion(QWidget *parentWindow)
{
    // ── Oprávnění ────────────────────────────────────────────────────────────
    // Záměrně NEVOLÁME CGPreflightScreenCaptureAccess / CGRequestScreenCaptureAccess.
    // Tato volání způsobovala nekonečnou smyčku — vrátí false pro aktuální
    // běžící process dokud není aplikace restartována po udělení oprávnění v TCC.
    // screencapture je systémový nástroj s vlastní TCC správou; macOS ukáže dialog
    // pro PictureViewer při prvním spuštění a poté ho uloží pro bundle ID aplikace.
    // Po restartu aplikace oprávnění přetrvá.

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
    proc.start(QStringLiteral("/usr/sbin/screencapture"),
               {QStringLiteral("-i"), QStringLiteral("-s"),
                QStringLiteral("-x"), tempPath});
    proc.waitForFinished(-1);

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
