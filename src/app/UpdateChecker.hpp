#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace pictureviewer {

// Kontrola a instalace aktualizací přes GitHub Releases.
//
// Bezpečnostní zásady:
//  - pouze HTTPS, výchozí TLS validace Qt (žádné ignoreSslErrors)
//  - přesměrování povolena jen na whitelistované GitHub hosty
//  - stažený instalátor je před spuštěním ověřen proti SHA256SUMS.txt
//  - instalace vždy jen se souhlasem uživatele a jen na novější verzi
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    // Zjistí nejnovější release na GitHubu. silent=true → tichý check při
    // startu (signály nesou příznak, UI podle něj volí nenápadné zobrazení).
    void checkForUpdates(bool silent);

    // Stáhne instalátor + checksum soubor, ověří SHA256 a spustí instalaci.
    // Pouze Windows; na ostatních platformách emituje installFailed.
    void downloadAndInstall(const QUrl &installerUrl, const QUrl &checksumsUrl,
                            const QString &installerName);

    bool isBusy() const { return m_busy; }

    // true pokud je remote (např. "0.16") novější než current (např. "0.15").
    static bool isNewerVersion(const QString &remote, const QString &current);

signals:
    // silent se propaguje z checkForUpdates() — UI podle něj volí prezentaci.
    void updateAvailable(const QString &version, const QString &notes,
                         const QUrl &releasePageUrl, const QUrl &installerUrl,
                         const QUrl &checksumsUrl, const QString &installerName,
                         bool silent);
    void upToDate(bool silent);
    void checkFailed(const QString &error, bool silent);

    void downloadProgress(qint64 received, qint64 total);
    // Instalátor byl spuštěn — volající má ukončit aplikaci.
    void installerStarted();
    void installFailed(const QString &error);

private:
    void onCheckFinished(QNetworkReply *reply, bool silent);
    void onChecksumsFinished(QNetworkReply *reply);
    void onInstallerFinished(QNetworkReply *reply);
    void launchInstaller(const QByteArray &installerData);

    // Nastaví bezpečnou redirect policy: povolí jen HTTPS na GitHub hosty.
    QNetworkReply *startGet(const QUrl &url, int timeoutMs);
    static bool isAllowedHost(const QUrl &url);

    QNetworkAccessManager *m_nam = nullptr;
    bool m_busy = false;

    // Kontext probíhající instalace.
    QUrl    m_pendingInstallerUrl;
    QString m_pendingInstallerName;
    QString m_expectedSha256;   // hex, lowercase
};

} // namespace pictureviewer
