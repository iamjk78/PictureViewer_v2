#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QSize>
#include <QString>
#include <QStringList>

class QSettings;

namespace pictureviewer {

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager();

    // Returns the absolute path to config.ini (useful for diagnostics).
    static QString configFilePath();

    // ── General ──────────────────────────────────────────────────────────────
    QString lastFolder() const;
    bool rememberLastFolder() const;
    void clearLastFolder();
    void setLastFolder(const QString &folderPath);
    void setRememberLastFolder(bool enabled);

    // ── Updates ──────────────────────────────────────────────────────────────
    int updateCheckDelayMinutes() const;
    int updateCheckIntervalDays() const;
    QDateTime lastUpdateCheck() const;
    void setUpdateCheckDelayMinutes(int minutes);
    void setUpdateCheckIntervalDays(int days);
    void setLastUpdateCheck(const QDateTime &dt);

    // ── File Handling ────────────────────────────────────────────────────────
    bool enableDeleteImage() const;
    void setEnableDeleteImage(bool enabled);
    bool enableMoveToDelete() const;
    void setEnableMoveToDelete(bool enabled);
    bool askConfirmationDelete() const;
    void setAskConfirmationDelete(bool enabled);

    // ── VLC Settings ─────────────────────────────────────────────────────────
    QString vlcPath() const;
    void setVlcPath(const QString &path);
    int vlcTimeoutMs() const;
    void setVlcTimeoutMs(int ms);

    // ── PDF Settings ─────────────────────────────────────────────────────────
    bool enablePdfProcessing() const;
    void setEnablePdfProcessing(bool enabled);

    // ── UI Settings ──────────────────────────────────────────────────────────
    // Layout values: "classic", "filmstrip", "immersive", "gallery", "pro"
    QString uiLayout() const;
    void setUiLayout(const QString &layout);

    // ── Řazení souborů ───────────────────────────────────────────────────────
    // sortKey: 0 = název, 1 = datum, 2 = velikost (odpovídá ImageCatalog::SortKey)
    int sortKey() const;
    void setSortKey(int key);
    bool sortAscending() const;
    void setSortAscending(bool ascending);

    // ── Oblíbené složky ──────────────────────────────────────────────────────
    // Max 10 složek. addFavoriteFolder vrátí false pokud je dosažen limit.
    QStringList favoriteFolders() const;
    QStringList favoriteFolderColors() const;
    bool addFavoriteFolder(const QString &folderPath, const QString &colorHex = QString());
    void removeFavoriteFolder(const QString &folderPath);
    bool isFavoriteFolder(const QString &folderPath) const;
    void setFavoriteFolderColor(const QString &folderPath, const QString &colorHex);

    // ── Oblíbené toolbar ─────────────────────────────────────────────────────
    bool favoritesToolbarVisible() const;
    void setFavoritesToolbarVisible(bool visible);

    // ── Kategorie toolbar ────────────────────────────────────────────────────
    // Pamatovat si, zda byl kategoriální toolbar viditelný při posledním vypnutí
    bool categoriesToolbarVisible() const;
    void setCategoriesToolbarVisible(bool visible);

    // ── Settings version ─────────────────────────────────────────────────────
    // Zvyšte kCurrentSettingsVersion při každé změně formátu nastavení
    // a přidejte migraci do SettingsManager konstruktoru.
    static constexpr int kCurrentSettingsVersion = 1;
    int settingsVersion() const;

    // ── Window geometry ───────────────────────────────────────────────────────
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);
    // Stav doků a toolbarů (QMainWindow::saveState / restoreState).
    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);
    QSize savedScreenSize() const;
    void setSavedScreenSize(const QSize &size);

    // ── Persistence ──────────────────────────────────────────────────────────
    // Explicitly flush settings to disk (QSettings caches changes in memory)
    void syncToDisk() const;

    // ── Thumbnail Cache ──────────────────────────────────────────────────────
    bool thumbnailCacheEnabled() const;
    void setThumbnailCacheEnabled(bool enabled);
    // Uživatelem zvolený kořen cache (prázdné = systémový cache adresář)
    QString thumbnailCacheRoot() const;
    void setThumbnailCacheRoot(const QString &path);
    // Skutečný adresář s miniaturami: <kořen>/PictureViewerThumbs.
    // Podsložka záměrně — mazání cache nikdy nesmaže uživatelský adresář.
    QString effectiveThumbnailCacheDir() const;

private:
    QSettings *m_settings;
};

} // namespace pictureviewer
