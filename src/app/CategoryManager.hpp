#pragma once

#include <QColor>
#include <QList>
#include <QString>

class QSqlDatabase;

namespace pictureviewer {

struct Category {
    int id;
    QString name;
    QColor color;
};

// Spravuje kategorie obrázků — SQLite database umístěná vedle config.ini.
// Schéma se vytvoří automaticky při prvním spuštění.
//
// Klíč: absolutní cesta obrázku. Když se obrázek přesune, kategorie se ztratí.
// Max 5 kategorií na obrázek. 20 předdefinovaných barev; nevybraná barva se
// volí náhodně z nepoužitých.
class CategoryManager
{
public:
    explicit CategoryManager(const QString &dbPath);
    ~CategoryManager();

    // Vrátí všechny dostupné kategorie
    QList<Category> allCategories() const;

    // Vytvoří novou kategorii. Pokud color není platná, vybere náhodně
    // z nepoužitých barev (priorita). Vrátí vytvořenou kategorii (s ID).
    Category addCategory(const QString &name, const QColor &color = QColor());

    // Smaže kategorii (a všechna přiřazení k obrázků)
    void deleteCategory(int categoryId);

    // Aktualizuje kategorii (jméno a/nebo barvu)
    bool updateCategory(int categoryId, const QString &newName, const QColor &newColor);

    // Přiřadí kategorii obrázku. Ignoruje pokud už má, max 5 na obrázek.
    void assignCategory(const QString &imagePath, int categoryId);

    // Odebere kategorii z obrázku
    void unassignCategory(const QString &imagePath, int categoryId);

    // Odebere všechny kategorie z obrázku
    void unassignAll(const QString &imagePath);

    // Vrátí kategorie přiřazené obrázku
    QList<Category> categoriesForImage(const QString &imagePath) const;

    // Popis poslední chyby DB operace; prázdný řetězec = žádná chyba.
    QString lastError() const { return m_lastError; }
    void clearLastError() { m_lastError.clear(); }

    // Vrátí cesty obrázků, které mají **všechny** z categoryIds (AND logika)
    // Pokud categoryIds prázdný, vrátí všechny cesty
    QStringList imagePathsWithAllCategories(const QList<int> &categoryIds) const;

    // Vrátí pouze kategorie, které jsou přiřazeny alespoň jednomu obrázku v seznamu
    // Efektivní: jedním SQL dotazem místo volání categoriesForImage() pro každý obrázek
    QList<Category> categoriesUsedInPaths(const QStringList &imagePaths) const;

private:
    bool initializeDatabase();
    void migrateSchema(QSqlDatabase &db);
    int  currentSchemaVersion(QSqlDatabase &db) const;
    QColor pickRandomUnusedColor() const;

    QString m_dbPath;
    QString m_connectionName;   // unikátní název SQL připojení (umožňuje více instancí / přepínání profilů)
    mutable QString m_lastError;
    static constexpr int MaxCategoriesPerImage = 5;
    static constexpr int PredefinedColorCount = 20;
};

} // namespace pictureviewer
