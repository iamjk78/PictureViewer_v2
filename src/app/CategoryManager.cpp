#include "app/CategoryManager.hpp"

#include <QColor>
#include <QDebug>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace pictureviewer {

// 20 předdefinovaných barev (hex)
static constexpr const char *PredefinedColors[] = {
    "#FF6B6B", "#4ECDC4", "#45B7D1", "#FFA07A", "#98D8C8",
    "#F7DC6F", "#BB8FCE", "#85C1E2", "#F8B88B", "#A9DFBF",
    "#F5B7B1", "#D7BDE2", "#F9E79F", "#AED6F1", "#F8B4B8",
    "#B7E8D6", "#FDBFED", "#D4EFDF", "#FADBD8", "#EBD5B4"
};

CategoryManager::CategoryManager(const QString &dbPath)
    : m_dbPath(dbPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "categories");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Nelze otevřít databázi kategorií:" << db.lastError().text();
    } else {
        initializeDatabase();
    }
}

CategoryManager::~CategoryManager() = default;

bool CategoryManager::initializeDatabase()
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);

    // Tabulka kategorií
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS categories ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE,"
            "  color TEXT NOT NULL"
            ")"
        )) {
        qWarning() << "Chyba vytvoření tabulky categories:" << query.lastError().text();
        return false;
    }

    // Tabulka přiřazení obrázků k kategoriím
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS image_categories ("
            "  image_path TEXT NOT NULL,"
            "  category_id INTEGER NOT NULL,"
            "  PRIMARY KEY (image_path, category_id),"
            "  FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE CASCADE"
            ")"
        )) {
        qWarning() << "Chyba vytvoření tabulky image_categories:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<Category> CategoryManager::allCategories() const
{
    QList<Category> result;
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return result;
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT id, name, color FROM categories ORDER BY name")) {
        qWarning() << "Chyba čtení kategorií:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Category cat;
        cat.id = query.value(0).toInt();
        cat.name = query.value(1).toString();
        cat.color = QColor(query.value(2).toString());
        result.append(cat);
    }

    return result;
}

QColor CategoryManager::pickRandomUnusedColor() const
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    QList<QString> usedColors;

    if (db.isOpen()) {
        QSqlQuery query(db);
        if (query.exec("SELECT DISTINCT color FROM categories")) {
            while (query.next()) {
                usedColors.append(query.value(0).toString());
            }
        }
    }

    // Vybrat náhodnou nepoužitou barvu
    for (int attempt = 0; attempt < PredefinedColorCount; ++attempt) {
        int idx = QRandomGenerator::global()->bounded(PredefinedColorCount);
        QString colorHex = PredefinedColors[idx];
        if (!usedColors.contains(colorHex)) {
            return QColor(colorHex);
        }
    }

    // Fallback (všechny barvy použity) — vybrat prostě náhodně
    int idx = QRandomGenerator::global()->bounded(PredefinedColorCount);
    return QColor(PredefinedColors[idx]);
}

Category CategoryManager::addCategory(const QString &name, const QColor &color)
{
    Category result{-1, "", QColor()};
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return result;
    }

    QColor finalColor = color.isValid() ? color : pickRandomUnusedColor();

    QSqlQuery query(db);
    query.prepare("INSERT INTO categories (name, color) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(finalColor.name());

    if (!query.exec()) {
        qWarning() << "Chyba vytvoření kategorie:" << query.lastError().text();
        return result;
    }

    result.id = query.lastInsertId().toInt();
    result.name = name;
    result.color = finalColor;

    return result;
}

void CategoryManager::deleteCategory(int categoryId)
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM categories WHERE id = ?");
    query.addBindValue(categoryId);

    if (!query.exec()) {
        qWarning() << "Chyba smazání kategorie:" << query.lastError().text();
    }
}

void CategoryManager::assignCategory(const QString &imagePath, int categoryId)
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return;
    }

    // Ověřit limit 5 kategorií
    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT COUNT(*) FROM image_categories WHERE image_path = ?");
    countQuery.addBindValue(imagePath);

    if (countQuery.exec() && countQuery.next()) {
        int count = countQuery.value(0).toInt();
        if (count >= MaxCategoriesPerImage) {
            qWarning() << "Limit 5 kategorií dosažen pro:" << imagePath;
            return;
        }
    }

    // Vložit přiřazení (IGNORE duplicitní případ)
    QSqlQuery query(db);
    query.prepare(
        "INSERT OR IGNORE INTO image_categories (image_path, category_id) VALUES (?, ?)"
    );
    query.addBindValue(imagePath);
    query.addBindValue(categoryId);

    if (!query.exec()) {
        qWarning() << "Chyba přiřazení kategorie:" << query.lastError().text();
    }
}

void CategoryManager::unassignCategory(const QString &imagePath, int categoryId)
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM image_categories WHERE image_path = ? AND category_id = ?");
    query.addBindValue(imagePath);
    query.addBindValue(categoryId);

    if (!query.exec()) {
        qWarning() << "Chyba odebrání kategorie:" << query.lastError().text();
    }
}

void CategoryManager::unassignAll(const QString &imagePath)
{
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM image_categories WHERE image_path = ?");
    query.addBindValue(imagePath);

    if (!query.exec()) {
        qWarning() << "Chyba odebrání všech kategorií:" << query.lastError().text();
    }
}

QList<Category> CategoryManager::categoriesForImage(const QString &imagePath) const
{
    QList<Category> result;
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return result;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT c.id, c.name, c.color FROM categories c "
        "INNER JOIN image_categories ic ON c.id = ic.category_id "
        "WHERE ic.image_path = ? "
        "ORDER BY c.name"
    );
    query.addBindValue(imagePath);

    if (!query.exec()) {
        qWarning() << "Chyba čtení kategorií obrázku:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Category cat;
        cat.id = query.value(0).toInt();
        cat.name = query.value(1).toString();
        cat.color = QColor(query.value(2).toString());
        result.append(cat);
    }

    return result;
}

QStringList CategoryManager::imagePathsWithAllCategories(const QList<int> &categoryIds) const
{
    QStringList result;
    QSqlDatabase db = QSqlDatabase::database("categories");
    if (!db.isOpen()) {
        return result;
    }

    // Pokud žádné kategorie — vrátit všechny cesty
    if (categoryIds.isEmpty()) {
        QSqlQuery query(db);
        if (query.exec("SELECT DISTINCT image_path FROM image_categories ORDER BY image_path")) {
            while (query.next()) {
                result.append(query.value(0).toString());
            }
        }
        return result;
    }

    // Najít obrázky s **všemi** danými kategoriemi (AND logika).
    // SELECT cesty, které se vyskytují tolikrát, kolik je kategorií v filtru.
    QStringList idStrings;
    for (int id : categoryIds) {
        idStrings.append(QString::number(id));
    }
    QString placeholders = QString("(%1)").arg(idStrings.join(","));
    QString sql = QString(
        "SELECT image_path FROM image_categories "
        "WHERE category_id IN %1 "
        "GROUP BY image_path "
        "HAVING COUNT(DISTINCT category_id) = %2 "
        "ORDER BY image_path"
    ).arg(placeholders, QString::number(categoryIds.size()));

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qWarning() << "Chyba filtrování kategorií:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        result.append(query.value(0).toString());
    }

    return result;
}

} // namespace pictureviewer
