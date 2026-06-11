#include "app/SettingsManager.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

// ── Key names ────────────────────────────────────────────────────────────────
// Group prefix ("General/", "Updates/") produces named INI sections, making
// the file human-readable and easy to edit by hand:
//
//   [General]
//   remember_last_folder=false
//   last_folder=
//
//   [Updates]
//   update_check_delay_minutes=5
//   update_check_interval_days=1
//   last_update_check=

constexpr auto kRememberLastFolderKey    = "General/remember_last_folder";
constexpr auto kLastFolderKey            = "General/last_folder";

constexpr auto kUpdateDelayMinutesKey    = "Updates/update_check_delay_minutes";
constexpr auto kUpdateIntervalDaysKey    = "Updates/update_check_interval_days";
constexpr auto kLastUpdateCheckKey       = "Updates/last_update_check";

constexpr auto kEnableDeleteImageKey     = "FileHandling/enable_delete_image";
constexpr auto kEnableMoveToDeleteKey    = "FileHandling/enable_move_to_delete";
constexpr auto kAskConfirmationDeleteKey = "FileHandling/ask_confirmation_delete";

constexpr auto kVlcPathKey               = "VLC/vlc_path";
constexpr auto kVlcTimeoutMsKey          = "VLC/vlc_timeout_ms";

constexpr auto kEnablePdfProcessingKey   = "PDF/enable_pdf_processing";

constexpr auto kUiLayoutKey              = "UI/layout";

constexpr auto kSortKeyKey               = "Sort/key";
constexpr auto kSortAscendingKey         = "Sort/ascending";

constexpr auto kFavoriteFoldersKey       = "Favorites/folders";
constexpr auto kFavoriteColorsKey        = "Favorites/colors";
constexpr auto kFavoritesToolbarVisibleKey = "Favorites/toolbar_visible";
constexpr int kMaxFavoriteFolders        = 10;

constexpr auto kCategoriesToolbarVisibleKey = "Categories/toolbar_visible";

constexpr auto kWindowGeometryKey        = "UI/window_geometry";
constexpr auto kSavedScreenSizeKey       = "UI/saved_screen_size";

constexpr auto kThumbCacheEnabledKey     = "Cache/thumbnail_cache_enabled";
constexpr auto kThumbCacheRootKey        = "Cache/thumbnail_cache_root";

constexpr int kDefaultUpdateDelayMinutes = 5;
constexpr int kDefaultUpdateIntervalDays = 1;
constexpr int kDefaultVlcTimeoutMs       = 5000;

} // namespace

namespace pictureviewer {

// static
QString SettingsManager::configFilePath()
{
    // QStandardPaths uses the org/app names set on QCoreApplication:
    //   macOS   → ~/Library/Preferences/JiriKrejci/PictureViewer/config.ini
    //   Windows → %APPDATA%\JiriKrejci\PictureViewer\config.ini
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + "/config.ini";
}

SettingsManager::SettingsManager()
{
    const QString path = configFilePath();
    // Ensure the directory exists before handing the path to QSettings.
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_settings = new QSettings(path, QSettings::IniFormat);
}

SettingsManager::~SettingsManager()
{
    delete m_settings;
}

// ── General ──────────────────────────────────────────────────────────────────

QString SettingsManager::lastFolder() const
{
    return m_settings->value(kLastFolderKey, QString()).toString();
}

bool SettingsManager::rememberLastFolder() const
{
    return m_settings->value(kRememberLastFolderKey, false).toBool();
}

void SettingsManager::clearLastFolder()
{
    m_settings->remove(kLastFolderKey);
}

void SettingsManager::setLastFolder(const QString &folderPath)
{
    m_settings->setValue(kLastFolderKey, folderPath);
}

void SettingsManager::setRememberLastFolder(bool enabled)
{
    m_settings->setValue(kRememberLastFolderKey, enabled);
}

// ── Updates ──────────────────────────────────────────────────────────────────

int SettingsManager::updateCheckDelayMinutes() const
{
    return m_settings->value(kUpdateDelayMinutesKey, kDefaultUpdateDelayMinutes).toInt();
}

int SettingsManager::updateCheckIntervalDays() const
{
    return m_settings->value(kUpdateIntervalDaysKey, kDefaultUpdateIntervalDays).toInt();
}

QDateTime SettingsManager::lastUpdateCheck() const
{
    const QString s = m_settings->value(kLastUpdateCheckKey, QString()).toString();
    if (s.isEmpty()) {
        return QDateTime{};
    }
    return QDateTime::fromString(s, Qt::ISODate);
}

void SettingsManager::setUpdateCheckDelayMinutes(int minutes)
{
    m_settings->setValue(kUpdateDelayMinutesKey, minutes);
}

void SettingsManager::setUpdateCheckIntervalDays(int days)
{
    m_settings->setValue(kUpdateIntervalDaysKey, days);
}

void SettingsManager::setLastUpdateCheck(const QDateTime &dt)
{
    m_settings->setValue(kLastUpdateCheckKey, dt.toString(Qt::ISODate));
}

// ── File Handling ────────────────────────────────────────────────────────────

bool SettingsManager::enableDeleteImage() const
{
    return m_settings->value(kEnableDeleteImageKey, false).toBool();
}

void SettingsManager::setEnableDeleteImage(bool enabled)
{
    m_settings->setValue(kEnableDeleteImageKey, enabled);
}

bool SettingsManager::enableMoveToDelete() const
{
    return m_settings->value(kEnableMoveToDeleteKey, false).toBool();
}

void SettingsManager::setEnableMoveToDelete(bool enabled)
{
    m_settings->setValue(kEnableMoveToDeleteKey, enabled);
}

bool SettingsManager::askConfirmationDelete() const
{
    return m_settings->value(kAskConfirmationDeleteKey, false).toBool();
}

void SettingsManager::setAskConfirmationDelete(bool enabled)
{
    m_settings->setValue(kAskConfirmationDeleteKey, enabled);
}

// ── VLC Settings ─────────────────────────────────────────────────────────────

QString SettingsManager::vlcPath() const
{
    return m_settings->value(kVlcPathKey, QString()).toString();
}

void SettingsManager::setVlcPath(const QString &path)
{
    m_settings->setValue(kVlcPathKey, path);
}

int SettingsManager::vlcTimeoutMs() const
{
    return m_settings->value(kVlcTimeoutMsKey, kDefaultVlcTimeoutMs).toInt();
}

void SettingsManager::setVlcTimeoutMs(int ms)
{
    m_settings->setValue(kVlcTimeoutMsKey, ms);
}

// ── PDF Settings ─────────────────────────────────────────────────────────────

bool SettingsManager::enablePdfProcessing() const
{
    return m_settings->value(kEnablePdfProcessingKey, true).toBool();
}

void SettingsManager::setEnablePdfProcessing(bool enabled)
{
    m_settings->setValue(kEnablePdfProcessingKey, enabled);
}

// ── UI Settings ──────────────────────────────────────────────────────────────

QString SettingsManager::uiLayout() const
{
    return m_settings->value(kUiLayoutKey, QStringLiteral("classic")).toString();
}

void SettingsManager::setUiLayout(const QString &layout)
{
    m_settings->setValue(kUiLayoutKey, layout);
}

// ── Řazení souborů ─────────────────────────────────────────────────────────

int SettingsManager::sortKey() const
{
    return m_settings->value(kSortKeyKey, 0).toInt();   // 0 = název
}

void SettingsManager::setSortKey(int key)
{
    m_settings->setValue(kSortKeyKey, key);
}

bool SettingsManager::sortAscending() const
{
    return m_settings->value(kSortAscendingKey, true).toBool();
}

void SettingsManager::setSortAscending(bool ascending)
{
    m_settings->setValue(kSortAscendingKey, ascending);
}

// ── Oblíbené složky ─────────────────────────────────────────────────────

QStringList SettingsManager::favoriteFolders() const
{
    return m_settings->value(kFavoriteFoldersKey, QStringList()).toStringList();
}

QStringList SettingsManager::favoriteFolderColors() const
{
    return m_settings->value(kFavoriteColorsKey, QStringList()).toStringList();
}

bool SettingsManager::addFavoriteFolder(const QString &folderPath, const QString &colorHex)
{
    QStringList favorites = favoriteFolders();
    if (favorites.size() >= kMaxFavoriteFolders) {
        return false;   // limit dosažen
    }
    if (!favorites.contains(folderPath)) {
        favorites.append(folderPath);
        m_settings->setValue(kFavoriteFoldersKey, favorites);

        // Uložit barvu na stejný index
        QStringList colors = favoriteFolderColors();
        while (colors.size() < favorites.size() - 1) {
            colors.append(QString());  // doplnit prázdné pro předchozí záznamy
        }
        colors.append(colorHex.isEmpty() ? QStringLiteral("#4ECDC4") : colorHex);
        m_settings->setValue(kFavoriteColorsKey, colors);
    }
    return true;
}

void SettingsManager::removeFavoriteFolder(const QString &folderPath)
{
    QStringList favorites = favoriteFolders();
    QStringList colors = favoriteFolderColors();

    int idx = favorites.indexOf(folderPath);
    if (idx >= 0) {
        favorites.removeAt(idx);
        if (idx < colors.size()) {
            colors.removeAt(idx);
        }
        m_settings->setValue(kFavoriteFoldersKey, favorites);
        m_settings->setValue(kFavoriteColorsKey, colors);
    }
}

bool SettingsManager::isFavoriteFolder(const QString &folderPath) const
{
    return favoriteFolders().contains(folderPath);
}

void SettingsManager::setFavoriteFolderColor(const QString &folderPath, const QString &colorHex)
{
    QStringList favorites = favoriteFolders();
    QStringList colors = favoriteFolderColors();
    int idx = favorites.indexOf(folderPath);
    if (idx < 0) {
        return;
    }
    while (colors.size() <= idx) {
        colors.append(QString());
    }
    colors[idx] = colorHex;
    m_settings->setValue(kFavoriteColorsKey, colors);
}

bool SettingsManager::favoritesToolbarVisible() const
{
    return m_settings->value(kFavoritesToolbarVisibleKey, false).toBool();
}

void SettingsManager::setFavoritesToolbarVisible(bool visible)
{
    m_settings->setValue(kFavoritesToolbarVisibleKey, visible);
}

// ── Kategorie toolbar ────────────────────────────────────────────────────────

bool SettingsManager::categoriesToolbarVisible() const
{
    return m_settings->value(kCategoriesToolbarVisibleKey, false).toBool();
}

void SettingsManager::setCategoriesToolbarVisible(bool visible)
{
    m_settings->setValue(kCategoriesToolbarVisibleKey, visible);
}

// ── Window geometry ───────────────────────────────────────────────────────────

QByteArray SettingsManager::windowGeometry() const
{
    return QByteArray::fromBase64(
        m_settings->value(kWindowGeometryKey, QByteArray()).toByteArray());
}

void SettingsManager::setWindowGeometry(const QByteArray &geometry)
{
    m_settings->setValue(kWindowGeometryKey, geometry.toBase64());
}

QSize SettingsManager::savedScreenSize() const
{
    const QString s = m_settings->value(kSavedScreenSizeKey, QString()).toString();
    if (s.isEmpty()) {
        return {};
    }
    const QStringList parts = s.split('x');
    if (parts.size() != 2) {
        return {};
    }
    return {parts[0].toInt(), parts[1].toInt()};
}

void SettingsManager::setSavedScreenSize(const QSize &size)
{
    m_settings->setValue(kSavedScreenSizeKey,
                         QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
}

// ── Persistence ──────────────────────────────────────────────────────────────

void SettingsManager::syncToDisk() const
{
    m_settings->sync();
}

// ── Thumbnail Cache ──────────────────────────────────────────────────────────

bool SettingsManager::thumbnailCacheEnabled() const
{
    return m_settings->value(kThumbCacheEnabledKey, true).toBool();
}

void SettingsManager::setThumbnailCacheEnabled(bool enabled)
{
    m_settings->setValue(kThumbCacheEnabledKey, enabled);
}

QString SettingsManager::thumbnailCacheRoot() const
{
    return m_settings->value(kThumbCacheRootKey, QString()).toString();
}

void SettingsManager::setThumbnailCacheRoot(const QString &path)
{
    m_settings->setValue(kThumbCacheRootKey, path);
}

QString SettingsManager::effectiveThumbnailCacheDir() const
{
    QString root = thumbnailCacheRoot();
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    return root + QStringLiteral("/PictureViewerThumbs");
}

} // namespace pictureviewer
