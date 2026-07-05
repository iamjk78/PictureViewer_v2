#include "app/UpdateChecker.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {

constexpr auto kLatestReleaseUrl =
    "https://api.github.com/repos/iamjk78/PictureViewer_v2/releases/latest";

constexpr int kApiTimeoutMs      = 15'000;
constexpr int kDownloadTimeoutMs = 5 * 60'000;

// Maximální velikost JSON odpovědi API (ochrana proti nesmyslně velké odpovědi).
constexpr qint64 kMaxApiResponseBytes = 1 * 1024 * 1024;

} // namespace

namespace pictureviewer {

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// ── Porovnání verzí ───────────────────────────────────────────────────────────

bool UpdateChecker::isNewerVersion(const QString &remote, const QString &current)
{
    const QVersionNumber r = QVersionNumber::fromString(remote).normalized();
    const QVersionNumber c = QVersionNumber::fromString(current).normalized();
    if (r.isNull() || c.isNull()) {
        return false;   // neparsovatelná verze → nikdy nenabízet instalaci
    }
    return QVersionNumber::compare(r, c) > 0;
}

// ── Bezpečné HTTP GET ─────────────────────────────────────────────────────────

bool UpdateChecker::isAllowedHost(const QUrl &url)
{
    if (url.scheme() != QLatin1String("https")) {
        return false;
    }
    static const QStringList allowed = {
        QStringLiteral("api.github.com"),
        QStringLiteral("github.com"),
        QStringLiteral("objects.githubusercontent.com"),
        QStringLiteral("release-assets.githubusercontent.com"),
    };
    return allowed.contains(url.host().toLower());
}

QNetworkReply *UpdateChecker::startGet(const QUrl &url, int timeoutMs)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(timeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("PictureViewer/")
                          + QCoreApplication::applicationVersion());

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::redirected, reply,
            [reply](const QUrl &target) {
                if (isAllowedHost(target)) {
                    emit reply->redirectAllowed();
                } else {
                    reply->abort();
                }
            });
    return reply;
}

// ── Kontrola aktualizací ──────────────────────────────────────────────────────

void UpdateChecker::checkForUpdates(bool silent)
{
    if (m_busy) {
        return;
    }
    m_busy = true;

    QNetworkReply *reply = startGet(QUrl(QString::fromLatin1(kLatestReleaseUrl)),
                                    kApiTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent] {
        reply->deleteLater();
        m_busy = false;
        onCheckFinished(reply, silent);
    });
}

void UpdateChecker::onCheckFinished(QNetworkReply *reply, bool silent)
{
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 404 = repozitář zatím nemá žádný release → žádná aktualizace, ne chyba.
    if (httpStatus == 404) {
        emit upToDate(silent);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString(), silent);
        return;
    }

    const QByteArray body = reply->read(kMaxApiResponseBytes);
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        emit checkFailed(tr("Neplatná odpověď serveru."), silent);
        return;
    }

    const QJsonObject release = doc.object();
    QString tag = release.value(QLatin1String("tag_name")).toString();
    if (tag.startsWith(QLatin1Char('v'))) {
        tag.remove(0, 1);
    }

    const QString current = QCoreApplication::applicationVersion();
    if (!isNewerVersion(tag, current)) {
        emit upToDate(silent);
        return;
    }

    // Najít assety: instalátor (.exe) a SHA256SUMS.txt.
    QUrl installerUrl;
    QUrl checksumsUrl;
    QString installerName;
    const QJsonArray assets = release.value(QLatin1String("assets")).toArray();
    for (const QJsonValue &v : assets) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value(QLatin1String("name")).toString();
        const QUrl url(asset.value(QLatin1String("browser_download_url")).toString());
        if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            installerUrl  = url;
            installerName = name;
        } else if (name.compare(QLatin1String("SHA256SUMS.txt"),
                                Qt::CaseInsensitive) == 0) {
            checksumsUrl = url;
        }
    }

    const QUrl releasePage(release.value(QLatin1String("html_url")).toString());
    const QString notes = release.value(QLatin1String("body")).toString();

    emit updateAvailable(tag, notes, releasePage, installerUrl, checksumsUrl,
                         installerName, silent);
}

// ── Stažení a instalace ───────────────────────────────────────────────────────

void UpdateChecker::downloadAndInstall(const QUrl &installerUrl,
                                       const QUrl &checksumsUrl,
                                       const QString &installerName)
{
#ifndef Q_OS_WIN
    Q_UNUSED(installerUrl);
    Q_UNUSED(checksumsUrl);
    Q_UNUSED(installerName);
    emit installFailed(tr("Automatická instalace je podporována jen na Windows."));
    return;
#else
    if (m_busy) {
        return;
    }
    if (!isAllowedHost(installerUrl) || !isAllowedHost(checksumsUrl)) {
        emit installFailed(tr("Adresa ke stažení není důvěryhodná."));
        return;
    }
    if (!installerName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        emit installFailed(tr("Neočekávaný typ instalačního souboru."));
        return;
    }
    m_busy = true;
    m_pendingInstallerUrl  = installerUrl;
    m_pendingInstallerName = installerName;
    m_expectedSha256.clear();

    // Nejdřív malý checksum soubor, teprve pak velký instalátor.
    QNetworkReply *reply = startGet(checksumsUrl, kApiTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        onChecksumsFinished(reply);
    });
#endif
}

void UpdateChecker::onChecksumsFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        m_busy = false;
        emit installFailed(tr("Stažení kontrolního souboru selhalo: %1")
                               .arg(reply->errorString()));
        return;
    }

    // Formát řádku: "<sha256-hex>  <název souboru>"
    const QString text = QString::fromUtf8(reply->read(kMaxApiResponseBytes));
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts =
            line.trimmed().split(QRegularExpression(QStringLiteral("\\s+")));
        if (parts.size() >= 2 && parts.at(1) == m_pendingInstallerName
            && parts.at(0).size() == 64) {
            m_expectedSha256 = parts.at(0).toLower();
            break;
        }
    }

    if (m_expectedSha256.isEmpty()) {
        m_busy = false;
        emit installFailed(
            tr("Kontrolní součet pro %1 nebyl nalezen.").arg(m_pendingInstallerName));
        return;
    }

    QNetworkReply *dl = startGet(m_pendingInstallerUrl, kDownloadTimeoutMs);
    connect(dl, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::downloadProgress);
    connect(dl, &QNetworkReply::finished, this, [this, dl] {
        dl->deleteLater();
        onInstallerFinished(dl);
    });
}

void UpdateChecker::onInstallerFinished(QNetworkReply *reply)
{
    m_busy = false;

    if (reply->error() != QNetworkReply::NoError) {
        emit installFailed(tr("Stažení instalátoru selhalo: %1")
                               .arg(reply->errorString()));
        return;
    }

    const QByteArray data = reply->readAll();

    const QString actual = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    if (actual != m_expectedSha256) {
        emit installFailed(
            tr("Ověření integrity staženého souboru selhalo — instalace zrušena."));
        return;
    }

    launchInstaller(data);
}

void UpdateChecker::launchInstaller(const QByteArray &installerData)
{
    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dirPath = tempRoot + QStringLiteral("/PictureViewerUpdate");
    QDir().mkpath(dirPath);
    const QString exePath = dirPath + QLatin1Char('/') + m_pendingInstallerName;

    QFile file(exePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit installFailed(tr("Nelze uložit instalátor do dočasné složky."));
        return;
    }
    file.write(installerData);
    file.close();

    // /VERYSILENT = bez průvodce, /NORESTART = nikdy nerestartovat systém.
    // Instalátor přepíše soubory programu; uživatelská data v AppData zůstávají.
    const bool started = QProcess::startDetached(
        exePath, {QStringLiteral("/VERYSILENT"), QStringLiteral("/NORESTART")});
    if (!started) {
        emit installFailed(tr("Instalátor se nepodařilo spustit."));
        return;
    }

    emit installerStarted();
}

} // namespace pictureviewer
