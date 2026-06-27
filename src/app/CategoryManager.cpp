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
    // Unikátní název připojení — umožňuje souběžné/přepínané instance bez kolize
    // (Qt varuje, pokud se znovu zaregistruje stejný connection name).
    static quint64 s_counter = 0;
    m_connectionName = QStringLiteral("categories_%1").arg(++s_counter);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        qWarning() << "Nelze otevřít databázi kategorií:" << m_lastError;
    } else {
        initializeDatabase();
    }
}

CategoryManager::~CategoryManager()
{
    // Uvolnit připojení, aby šel název znovu použít a soubor DB uvolnit.
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, /*open=*/false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

// Aktuální verze schématu databáze. Zvyšte při každé strukturální změně.
static constexpr int kCurrentSchemaVersion = 1;

bool CategoryManager::initializeDatabase()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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

    // Tabulka verzování schématu — jeden řádek s aktuální verzí
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)"
        )) {
        qWarning() << "Chyba vytvoření tabulky schema_version:" << query.lastError().text();
        return false;
    }

    // Inicializovat verzi pro nové databáze
    if (!query.exec("SELECT COUNT(*) FROM schema_version") || !query.next()
            || query.value(0).toInt() == 0) {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO schema_version (version) VALUES (?)");
        insert.addBindValue(kCurrentSchemaVersion);
        insert.exec();
    }

    migrateSchema(db);
    return true;
}

int CategoryManager::currentSchemaVersion(QSqlDatabase &db) const
{
    QSqlQuery q(db);
    if (q.exec("SELECT version FROM schema_version") && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

void CategoryManager::migrateSchema(QSqlDatabase &db)
{
    int ver = currentSchemaVersion(db);

    // Šablona pro budoucí migrace:
    // if (ver < 2) {
    //     QSqlQuery(db).exec("ALTER TABLE categories ADD COLUMN ...");
    //     QSqlQuery(db).exec("UPDATE schema_version SET version = 2");
    //     ver = 2;
    // }

    if (ver != kCurrentSchemaVersion) {
        qWarning() << "Neznámá verze schématu databáze:" << ver
                   << "(očekáváno" << kCurrentSchemaVersion << ")";
    }
}

QList<Category> CategoryManager::allCategories() const
{
    QList<Category> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
    m_lastError.clear();
    Category result{-1, "", QColor()};
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("Databáze není dostupná.");
        return result;
    }

    QColor finalColor = color.isValid() ? color : pickRandomUnusedColor();

    QSqlQuery query(db);
    query.prepare("INSERT INTO categories (name, color) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(finalColor.name());

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "Chyba vytvoření kategorie:" << m_lastError;
        return result;
    }

    result.id = query.lastInsertId().toInt();
    result.name = name;
    result.color = finalColor;

    return result;
}

void CategoryManager::deleteCategory(int categoryId)
{
    m_lastError.clear();
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("Databáze není dostupná.");
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM categories WHERE id = ?");
    query.addBindValue(categoryId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "Chyba smazání kategorie:" << m_lastError;
    }
}

bool CategoryManager::updateCategory(int categoryId, const QString &newName, const QColor &newColor)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        return false;
    }

    // Pokud newName není prázdný, zkontroluj, zda není duplicitní
    if (!newName.isEmpty()) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT id FROM categories WHERE name = ? AND id != ?");
        checkQuery.addBindValue(newName);
        checkQuery.addBindValue(categoryId);

        if (checkQuery.exec() && checkQuery.next()) {
            qWarning() << "Kategorie s tímto jménem již existuje";
            return false;
        }
    }

    // Připravit UPDATE dotaz
    QString updateSql = "UPDATE categories SET ";
    QStringList updates;
    QVariantList values;

    if (!newName.isEmpty()) {
        updates.append("name = ?");
        values.append(newName);
    }

    if (newColor.isValid()) {
        updates.append("color = ?");
        values.append(newColor.name());
    }

    if (updates.isEmpty()) {
        return true;  // Nic se neměnilo
    }

    updateSql += updates.join(", ");
    updateSql += " WHERE id = ?";
    values.append(categoryId);

    QSqlQuery query(db);
    query.prepare(updateSql);

    for (const QVariant &val : values) {
        query.addBindValue(val);
    }

    if (!query.exec()) {
        qWarning() << "Chyba aktualizace kategorie:" << query.lastError().text();
        return false;
    }

    return true;
}

void CategoryManager::assignCategory(const QString &imagePath, int categoryId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
        m_lastError = query.lastError().text();
        qWarning() << "Chyba přiřazení kategorie:" << m_lastError;
    }
}

void CategoryManager::unassignCategory(const QString &imagePath, int categoryId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
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

QList<Category> CategoryManager::categoriesUsedInPaths(const QStringList &imagePaths) const
{
    QList<Category> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() || imagePaths.isEmpty()) {
        return result;
    }

    // Vytvořit dotaz se placeholder pro všechny cesty
    QStringList placeholders;
    for (int i = 0; i < imagePaths.size(); ++i) {
        placeholders.append("?");
    }

    QString sql = QString(
        "SELECT DISTINCT c.id, c.name, c.color "
        "FROM categories c "
        "INNER JOIN image_categories ic ON c.id = ic.category_id "
        "WHERE ic.image_path IN (%1) "
        "ORDER BY c.name"
    ).arg(placeholders.join(","));

    QSqlQuery query(db);
    query.prepare(sql);

    // Vázat všechny cesty
    for (const QString &path : imagePaths) {
        query.addBindValue(path);
    }

    if (!query.exec()) {
        qWarning() << "Chyba čtení použitých kategorií:" << query.lastError().text();
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

} // namespace pictureviewer
