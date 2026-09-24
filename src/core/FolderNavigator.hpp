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
// Oba směry sourozenecké navigace najednou — viz FolderNavigator::siblings().
struct FolderNavSiblings {
    FolderNavResult before;
    FolderNavResult after;
};

class FolderNavigator
{
public:
    // Abecedně nejbližší předcházející sourozenec; count = kolik jich je před.
    static FolderNavResult siblingBefore(const QString &currentFolder);
    // Abecedně nejbližší následující sourozenec; count = kolik jich je za.
    static FolderNavResult siblingAfter(const QString &currentFolder);
    // Oba směry z JEDNOHO výpisu rodičovské složky — siblingBefore()+siblingAfter()
    // volané zvlášť ji čtou dvakrát; na síťovém disku to zdvojnásobí latenci.
    // Použij, když potřebuješ oba směry současně (viz refreshFolderNavData()).
    static FolderNavSiblings siblings(const QString &currentFolder);

    // Seřazené podsložky dané složky (bez "Delete") — čistě I/O čtení, bez
    // vztahu ke konkrétní "aktuální" složce. Použij spolu se
    // siblingsFromParentListing(), když chceš sdílet JEDEN výpis rodiče mezi
    // více voláními (např. před přechodem na souseda i po něm — POZOR: který
    // je "před" a "který "po" závisí na POZICI KONKRÉTNÍ složky v tomto
    // seznamu, ne jen na tom, že je rodič stejný — nikdy si tedy neukládej
    // hotový FolderNavSiblings výsledek pro pozdější použití u JINÉ složky,
    // jen tenhle seznam jmen).
    static QStringList subfolderNames(const QString &parentFolder);

    // Sourozenci dané složky, POKUD UŽ MÁŠ seřazený seznam podsložek jejího
    // rodiče (ze subfolderNames() pro STEJNÉHO rodiče) — žádné další čtení
    // z disku, jen vyhledání pozice v seznamu.
    static FolderNavSiblings siblingsFromParentListing(const QString &currentFolder,
                                                        const QStringList &parentSubfolderNames);
    // Abecedně první podsložka; count = celkový počet podsložek (bez "Delete").
    static FolderNavResult firstSubfolder(const QString &currentFolder);
    // Rodičovská složka; count je vždy 0 (na kořeni disku) nebo 1.
    static FolderNavResult parentFolder(const QString &currentFolder);
};

} // namespace pictureviewer

// Nutné pro doručení FolderNavResult přes queued signál napříč vlákny
// (FolderNavWorker běží v QThreadPool, výsledek se doručuje na hlavní vlákno).
Q_DECLARE_METATYPE(pictureviewer::FolderNavResult)
