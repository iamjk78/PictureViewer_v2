#pragma once

#include <QMetaType>
#include <QString>

namespace pictureviewer {

// Výsledek jednoho směru navigace mezi složkami (viz FolderNavigator).
// `name`/`path` je nejbližší cíl v daném směru (kam klik skutečně skočí),
// `count` je celkový počet kandidátů v tomto směru (informativní číslo
// zobrazené na tlačítku). `available` odpovídá `count > 0`.
struct FolderNavResult {
    QString name;
    QString path;
    int count = 0;
    bool available = false;
};

// Čistá (bez Qt GUI/widgetů) logika pro navigační toolbar — pohyb mezi
// sourozeneckými složkami, do podsložky a do rodiče. Řazení je locale-aware
// (stejný QCollator vzor jako ImageCatalog — numericMode, case-insensitive).
// Složka jménem "Delete" (case-insensitive) je vždy vyloučena jako kandidát —
// slouží jen jako interní koš aplikace, ne jako cílová složka pro procházení.
class FolderNavigator
{
public:
    // Abecedně nejbližší předcházející sourozenec; count = kolik jich je před.
    static FolderNavResult siblingBefore(const QString &currentFolder);
    // Abecedně nejbližší následující sourozenec; count = kolik jich je za.
    static FolderNavResult siblingAfter(const QString &currentFolder);
    // Abecedně první podsložka; count = celkový počet podsložek (bez "Delete").
    static FolderNavResult firstSubfolder(const QString &currentFolder);
    // Rodičovská složka; count je vždy 0 (na kořeni disku) nebo 1.
    static FolderNavResult parentFolder(const QString &currentFolder);
};

} // namespace pictureviewer

// Nutné pro doručení FolderNavResult přes queued signál napříč vlákny
// (FolderNavWorker běží v QThreadPool, výsledek se doručuje na hlavní vlákno).
Q_DECLARE_METATYPE(pictureviewer::FolderNavResult)
