#include "app/SettingsManager.hpp"

#include "app/PredefinedColors.hpp"

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

constexpr auto kSettingsVersionKey       = "General/settings_version";
constexpr auto kRememberLastFolderKey    = "General/remember_last_folder";
constexpr auto kLastFolderKey            = "General/last_folder";

constexpr auto kUpdateDelayMinutesKey    = "Updates/update_check_delay_minutes";
constexpr auto kUpdateIntervalDaysKey    = "Updates/update_check_interval_days";
constexpr auto kLastUpdateCheckKey       = "Updates/last_update_check";
constexpr auto kSkippedUpdateVersionKey  = "Updates/skipped_version";

constexpr auto kEnableDeleteImageKey     = "FileHandling/enable_delete_image";
constexpr auto kEnableMoveToDeleteKey    = "FileHandling/enable_move_to_delete";
constexpr auto kAskConfirmationDeleteKey = "FileHandling/ask_confirmation_delete";
constexpr auto kMoveCompanionFilesKey    = "FileHandling/move_companion_files";

constexpr auto kVideoVolumeKey           = "Video/volume";
constexpr int  kDefaultVideoVolume       = 50;

constexpr auto kEnableImagesKey          = "Processing/enable_images";
constexpr auto kEnableVideosKey          = "Processing/enable_videos";
constexpr auto kEnablePdfProcessingKey   = "PDF/enable_pdf_processing";

constexpr auto kUiLayoutKey              = "UI/layout";

constexpr auto kSortKeyKey               = "Sort/key";
constexpr auto kSortAscendingKey         = "Sort/ascending";

constexpr auto kFavoriteFoldersKey       = "Favorites/folders";
constexpr auto kFavoriteColorsKey        = "Favorites/colors";
constexpr auto kFavoritesToolbarVisibleKey = "Favorites/toolbar_visible";
constexpr int kMaxFavoriteFolders        = 10;

constexpr auto kCategoriesToolbarVisibleKey = "Categories/toolbar_visible";

constexpr auto kMoveButtonIdsKey         = "Move/ids";
constexpr auto kMoveButtonNamesKey       = "Move/names";
constexpr auto kMoveButtonColorsKey      = "Move/colors";
constexpr auto kMoveButtonFoldersKey     = "Move/folders";
constexpr auto kMoveNextIdKey            = "Move/next_id";
constexpr auto kMoveToolbarVisibleKey    = "Move/toolbar_visible";

constexpr auto kNavigationToolbarVisibleKey = "Navigation/toolbar_visible";

constexpr auto kWindowGeometryKey        = "UI/window_geometry";
constexpr auto kWindowStateKey           = "UI/window_state";
constexpr auto kSavedScreenSizeKey       = "UI/saved_screen_size";

constexpr auto kThumbCacheEnabledKey     = "Cache/thumbnail_cache_enabled";
constexpr auto kThumbCacheRootKey        = "Cache/thumbnail_cache_root";

constexpr int kDefaultUpdateDelayMinutes = 5;
constexpr int kDefaultUpdateIntervalDays = 1;

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
    : SettingsManager(configFilePath())
{
}

SettingsManager::SettingsManager(const QString &path, const QString &profileName)
    : m_profileName(profileName)
{
    // Ensure the directory exists before handing the path to QSettings.
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_settings = new QSettings(path, QSettings::IniFormat);

    // Při prvním spuštění (nebo po ruční editaci) zapsat verzi schématu nastavení.
    if (!m_settings->contains(kSettingsVersionKey)) {
        m_settings->setValue(kSettingsVersionKey, kCurrentSettingsVersion);
    }

    // Migrace v2: přidán toolbar Přesun a objectName() na všechny toolbary
    // (nutné pro QMainWindow::restoreState()). Staré uložené window_state
    // neobsahuje tyto identifikátory — Qt by ho přesto částečně napárovalo
    // na nově pojmenované toolbary a přepsalo jejich viditelnost/pořadí
    // nesmyslnými hodnotami ze staré, neodpovídající struktury oken.
    // Zahodit staré window_state donutí aplikaci použít výchozí layout,
    // který se při příštím zavření uloží už se správnou strukturou.
    if (settingsVersion() < 2) {
        m_settings->remove(kWindowStateKey);
        m_settings->setValue(kSettingsVersionKey, 2);
    }

    m_settings->sync();
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
    syncToDisk();
}

void SettingsManager::setLastFolder(const QString &folderPath)
{
    m_settings->setValue(kLastFolderKey, folderPath);
    syncToDisk();
}

void SettingsManager::setRememberLastFolder(bool enabled)
{
    m_settings->setValue(kRememberLastFolderKey, enabled);
    syncToDisk();
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
    syncToDisk();
}

void SettingsManager::setUpdateCheckIntervalDays(int days)
{
    m_settings->setValue(kUpdateIntervalDaysKey, days);
    syncToDisk();
}

void SettingsManager::setLastUpdateCheck(const QDateTime &dt)
{
    m_settings->setValue(kLastUpdateCheckKey, dt.toString(Qt::ISODate));
    syncToDisk();
}

QString SettingsManager::skippedUpdateVersion() const
{
    return m_settings->value(kSkippedUpdateVersionKey, QString()).toString();
}

void SettingsManager::setSkippedUpdateVersion(const QString &version)
{
    m_settings->setValue(kSkippedUpdateVersionKey, version);
    syncToDisk();
}

// ── File Handling ────────────────────────────────────────────────────────────

bool SettingsManager::enableDeleteImage() const
{
    return m_settings->value(kEnableDeleteImageKey, false).toBool();
}

void SettingsManager::setEnableDeleteImage(bool enabled)
{
    m_settings->setValue(kEnableDeleteImageKey, enabled);
    syncToDisk();
}

bool SettingsManager::enableMoveToDelete() const
{
    return m_settings->value(kEnableMoveToDeleteKey, false).toBool();
}

void SettingsManager::setEnableMoveToDelete(bool enabled)
{
    m_settings->setValue(kEnableMoveToDeleteKey, enabled);
    syncToDisk();
}

bool SettingsManager::askConfirmationDelete() const
{
    return m_settings->value(kAskConfirmationDeleteKey, false).toBool();
}

void SettingsManager::setAskConfirmationDelete(bool enabled)
{
    m_settings->setValue(kAskConfirmationDeleteKey, enabled);
    syncToDisk();
}

bool SettingsManager::moveCompanionFiles() const
{
    return m_settings->value(kMoveCompanionFilesKey, false).toBool();
}

void SettingsManager::setMoveCompanionFiles(bool enabled)
{
    m_settings->setValue(kMoveCompanionFilesKey, enabled);
    syncToDisk();
}

int SettingsManager::videoVolume() const
{
    return m_settings->value(kVideoVolumeKey, kDefaultVideoVolume).toInt();
}

void SettingsManager::setVideoVolume(int volume)
{
    m_settings->setValue(kVideoVolumeKey, qBound(0, volume, 100));
    syncToDisk();
}

// ── Processing Settings ───────────────────────────────────────────────────────

bool SettingsManager::enableImages() const
{
    return m_settings->value(kEnableImagesKey, true).toBool();
}

void SettingsManager::setEnableImages(bool enabled)
{
    m_settings->setValue(kEnableImagesKey, enabled);
    syncToDisk();
}

bool SettingsManager::enableVideos() const
{
    return m_settings->value(kEnableVideosKey, false).toBool();
}

void SettingsManager::setEnableVideos(bool enabled)
{
    m_settings->setValue(kEnableVideosKey, enabled);
    syncToDisk();
}

// ── PDF Settings ─────────────────────────────────────────────────────────────

bool SettingsManager::enablePdfProcessing() const
{
    return m_settings->value(kEnablePdfProcessingKey, true).toBool();
}

void SettingsManager::setEnablePdfProcessing(bool enabled)
{
    m_settings->setValue(kEnablePdfProcessingKey, enabled);
    syncToDisk();
}

// ── UI Settings ──────────────────────────────────────────────────────────────

QString SettingsManager::uiLayout() const
{
    return m_settings->value(kUiLayoutKey, QStringLiteral("classic")).toString();
}

void SettingsManager::setUiLayout(const QString &layout)
{
    m_settings->setValue(kUiLayoutKey, layout);
    syncToDisk();
}

// ── Řazení souborů ─────────────────────────────────────────────────────────

int SettingsManager::sortKey() const
{
    return m_settings->value(kSortKeyKey, 0).toInt();   // 0 = název
}

void SettingsManager::setSortKey(int key)
{
    m_settings->setValue(kSortKeyKey, key);
    syncToDisk();
}

bool SettingsManager::sortAscending() const
{
    return m_settings->value(kSortAscendingKey, true).toBool();
}

void SettingsManager::setSortAscending(bool ascending)
{
    m_settings->setValue(kSortAscendingKey, ascending);
    syncToDisk();
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
        colors.append(colorHex.isEmpty() ? defaultItemColor() : colorHex);
        m_settings->setValue(kFavoriteColorsKey, colors);
        syncToDisk();
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
        syncToDisk();
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
    syncToDisk();
}

bool SettingsManager::favoritesToolbarVisible() const
{
    return m_settings->value(kFavoritesToolbarVisibleKey, false).toBool();
}

void SettingsManager::setFavoritesToolbarVisible(bool visible)
{
    m_settings->setValue(kFavoritesToolbarVisibleKey, visible);
    syncToDisk();
}

// ── Kategorie toolbar ────────────────────────────────────────────────────────

bool SettingsManager::categoriesToolbarVisible() const
{
    return m_settings->value(kCategoriesToolbarVisibleKey, false).toBool();
}

void SettingsManager::setCategoriesToolbarVisible(bool visible)
{
    m_settings->setValue(kCategoriesToolbarVisibleKey, visible);
    syncToDisk();
}

// ── Přesun do složky (Move buttons) ──────────────────────────────────────────

QList<MoveButtonInfo> SettingsManager::moveButtons() const
{
    const QStringList ids     = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    const QStringList names   = m_settings->value(kMoveButtonNamesKey, QStringList()).toStringList();
    const QStringList colors  = m_settings->value(kMoveButtonColorsKey, QStringList()).toStringList();
    const QStringList folders = m_settings->value(kMoveButtonFoldersKey, QStringList()).toStringList();

    QList<MoveButtonInfo> result;
    for (int i = 0; i < ids.size(); ++i) {
        MoveButtonInfo info;
        info.id     = ids.at(i).toInt();
        info.name   = i < names.size()   ? names.at(i)   : QString();
        info.color  = i < colors.size()  ? colors.at(i)  : QString();
        info.folder = i < folders.size() ? folders.at(i) : QString();
        result.append(info);
    }
    return result;
}

int SettingsManager::addMoveButton(const QString &name, const QString &folder, const QString &colorHex)
{
    if (name.trimmed().isEmpty() || folder.trimmed().isEmpty()) {
        return -1;
    }

    const int newId = m_settings->value(kMoveNextIdKey, 1).toInt();
    m_settings->setValue(kMoveNextIdKey, newId + 1);

    QStringList ids     = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    QStringList names   = m_settings->value(kMoveButtonNamesKey, QStringList()).toStringList();
    QStringList colors  = m_settings->value(kMoveButtonColorsKey, QStringList()).toStringList();
    QStringList folders = m_settings->value(kMoveButtonFoldersKey, QStringList()).toStringList();

    ids.append(QString::number(newId));
    names.append(name);
    colors.append(colorHex);
    folders.append(folder);

    m_settings->setValue(kMoveButtonIdsKey, ids);
    m_settings->setValue(kMoveButtonNamesKey, names);
    m_settings->setValue(kMoveButtonColorsKey, colors);
    m_settings->setValue(kMoveButtonFoldersKey, folders);
    syncToDisk();

    return newId;
}

bool SettingsManager::renameMoveButton(int id, const QString &newName)
{
    if (newName.trimmed().isEmpty()) {
        return false;
    }
    QStringList ids   = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    const int idx = ids.indexOf(QString::number(id));
    if (idx < 0) {
        return false;
    }
    QStringList names = m_settings->value(kMoveButtonNamesKey, QStringList()).toStringList();
    while (names.size() <= idx) {
        names.append(QString());
    }
    names[idx] = newName;
    m_settings->setValue(kMoveButtonNamesKey, names);
    syncToDisk();
    return true;
}

bool SettingsManager::setMoveButtonColor(int id, const QString &colorHex)
{
    QStringList ids = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    const int idx = ids.indexOf(QString::number(id));
    if (idx < 0) {
        return false;
    }
    QStringList colors = m_settings->value(kMoveButtonColorsKey, QStringList()).toStringList();
    while (colors.size() <= idx) {
        colors.append(QString());
    }
    colors[idx] = colorHex;
    m_settings->setValue(kMoveButtonColorsKey, colors);
    syncToDisk();
    return true;
}

bool SettingsManager::setMoveButtonFolder(int id, const QString &folder)
{
    if (folder.trimmed().isEmpty()) {
        return false;
    }
    QStringList ids = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    const int idx = ids.indexOf(QString::number(id));
    if (idx < 0) {
        return false;
    }
    QStringList folders = m_settings->value(kMoveButtonFoldersKey, QStringList()).toStringList();
    while (folders.size() <= idx) {
        folders.append(QString());
    }
    folders[idx] = folder;
    m_settings->setValue(kMoveButtonFoldersKey, folders);
    syncToDisk();
    return true;
}

bool SettingsManager::removeMoveButton(int id)
{
    QStringList ids = m_settings->value(kMoveButtonIdsKey, QStringList()).toStringList();
    const int idx = ids.indexOf(QString::number(id));
    if (idx < 0) {
        return false;
    }
    QStringList names   = m_settings->value(kMoveButtonNamesKey, QStringList()).toStringList();
    QStringList colors  = m_settings->value(kMoveButtonColorsKey, QStringList()).toStringList();
    QStringList folders = m_settings->value(kMoveButtonFoldersKey, QStringList()).toStringList();

    ids.removeAt(idx);
    if (idx < names.size())   names.removeAt(idx);
    if (idx < colors.size())  colors.removeAt(idx);
    if (idx < folders.size()) folders.removeAt(idx);

    m_settings->setValue(kMoveButtonIdsKey, ids);
    m_settings->setValue(kMoveButtonNamesKey, names);
    m_settings->setValue(kMoveButtonColorsKey, colors);
    m_settings->setValue(kMoveButtonFoldersKey, folders);
    syncToDisk();
    return true;
}

bool SettingsManager::moveToolbarVisible() const
{
    return m_settings->value(kMoveToolbarVisibleKey, false).toBool();
}

void SettingsManager::setMoveToolbarVisible(bool visible)
{
    m_settings->setValue(kMoveToolbarVisibleKey, visible);
    syncToDisk();
}

// ── Navigace mezi složkami toolbar ───────────────────────────────────────────
// Per-profil (na rozdíl od dřívějšího globálního uložení v ProfileManager) —
// sjednoceno s ostatními toolbary (Oblíbené/Štítky/Přesun).

bool SettingsManager::navigationToolbarVisible() const
{
    return m_settings->value(kNavigationToolbarVisibleKey, false).toBool();
}

void SettingsManager::setNavigationToolbarVisible(bool visible)
{
    m_settings->setValue(kNavigationToolbarVisibleKey, visible);
    syncToDisk();
}

// ── Settings version ─────────────────────────────────────────────────────────

int SettingsManager::settingsVersion() const
{
    return m_settings->value(kSettingsVersionKey, 0).toInt();
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
    syncToDisk();
}

QByteArray SettingsManager::windowState() const
{
    return QByteArray::fromBase64(
        m_settings->value(kWindowStateKey, QByteArray()).toByteArray());
}

void SettingsManager::setWindowState(const QByteArray &state)
{
    m_settings->setValue(kWindowStateKey, state.toBase64());
    syncToDisk();
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
    syncToDisk();
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
    syncToDisk();
}

QString SettingsManager::thumbnailCacheRoot() const
{
    return m_settings->value(kThumbCacheRootKey, QString()).toString();
}

void SettingsManager::setThumbnailCacheRoot(const QString &path)
{
    m_settings->setValue(kThumbCacheRootKey, path);
    syncToDisk();
}

QString SettingsManager::effectiveThumbnailCacheDir() const
{
    QString root = thumbnailCacheRoot();
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    QString dir = root + QStringLiteral("/PictureViewerThumbs");
    if (!m_profileName.isEmpty()) {
        dir += QLatin1Char('/') + m_profileName;
    }
    return dir;
}

} // namespace pictureviewer
