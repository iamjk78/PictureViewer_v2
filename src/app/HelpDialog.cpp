#include "app/HelpDialog.hpp"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace pictureviewer {

// ── Sdílený CSS styl pro všechny dialogy ─────────────────────────────────────
static const QString s_htmlStyle =
    "<style>"
    "body { font-family: -apple-system, 'Segoe UI', sans-serif; font-size: 13px;"
    "       color: #222; margin: 12px; }"
    "h2   { color: #1a5fa8; margin-bottom: 4px; }"
    "h3   { color: #444; margin-top: 14px; margin-bottom: 4px; }"
    "table{ border-collapse: collapse; width: 100%; }"
    "th   { background-color: #e8eef5; text-align: left; padding: 6px 10px;"
    "       font-weight: bold; }"
    "td   { padding: 5px 10px; border-bottom: 1px solid #ddd; }"
    "tr:last-child td { border-bottom: none; }"
    "ul   { margin: 4px 0; padding-left: 20px; }"
    "li   { margin-bottom: 4px; }"
    "kbd  { background: #f0f0f0; border: 1px solid #bbb; border-radius: 3px;"
    "       padding: 1px 5px; font-family: monospace; font-size: 12px; }"
    "p.note { color: #666; font-size: 12px; margin-top: 10px; }"
    "</style>";

// ── Konstruktor ───────────────────────────────────────────────────────────────
HelpDialog::HelpDialog(const QString &title, const QString &html, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumSize(540, 420);
    resize(580, 480);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setHtml(s_htmlStyle + html);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(m_browser);
    layout->addWidget(buttons);
    setLayout(layout);
}

// ── O programu ────────────────────────────────────────────────────────────────
// Aktualizuj při přidání nových funkcí nebo změně popisu aplikace.
void HelpDialog::showAbout(QWidget *parent)
{
    const QString version = QCoreApplication::applicationVersion();
    const QString html = QString(
        "<h2>PictureViewer v.%1</h2>"
        "<p>Rychlý a přehledný prohlížeč obrázků pro <b>macOS</b> a <b>Windows</b>.</p>"
        "<h3>Funkce</h3>"
        "<ul>"
        "<li><b>Procházení souborů</b> — panel náhledů s obrázky a PDF dokumenty</li>"
        "<li><b>Podpora PDF</b> — PDF se zobrazují stejně jako obrázky s listováním stránek</li>"
        "<li><b>Slideshow</b> — automatická prezentace s nastavitelným intervalem (1–60 sekund)</li>"
        "<li><b>Přiblížení/oddálení</b> — kolečkem myši nebo gesty trackpadu u obrázků i PDF</li>"
        "<li><b>Přizpůsobení oknu</b> — nebo zobrazení v originální velikosti (1:1)</li>"
        "<li><b>Plná obrazovka</b> — imerzivní režim pro prohlížení</li>"
        "<li><b>Přejmenování souborů</b> — obrázků i PDF přímo z aplikace</li>"
        "<li><b>Odstranění/přesunutí</b> — smazání nebo přesunutí do složky Delete</li>"
        "<li><b>Přehrávání videa</b> — MP4, MOV, WebM a další formáty (deaktivuje se pro PDF)</li>"
        "<li><b>Metadata</b> — rozměry, velikost, formát, počet stránek (PDF)</li>"
        "<li><b>Zapamatování složky</b> — aplikace si pamatuje poslední otevřenou cestu</li>"
        "<li><b>Otevření z Finderu</b> — přímé otevření souborů z macOS/Windows</li>"
        "<li><b>Kategorie obrázků</b> — přiřazení až 5 barevných štítků na obrázek; filtrování složky podle kategorií; přejmenování, změna barvy a smazání kategorie přes pravý klik</li>"
        "<li><b>Oblíbené složky</b> — toolbar s barevnými tlačítky pro rychlé přepínání mezi oblíbenými složkami (max 10); klik otevře složku, pravý klik odebere z oblíbených</li>"
        "<li><b>Ořez obrázku</b> — tlačítko ✂ aktivuje výběr oblasti myší; zobrazení se ořízne na vybranou část; pravý klik → Kopírovat obrázek uloží výřez jako JPEG soubor do schránky</li>"
        "<li><b>Uložit / Uložit jako</b> — uložení upraveného obrázku (ořez, otočení) jako JPEG;"
        " Uložit nabídne přepsání originálu nebo uložení pod novým názvem;"
        " Uložit jako otevře dialog pro výběr názvu a cílové složky"
        " (originál nebo oblíbená složka)</li>"
        "<li><b>Recyklace (♻)</b> — tlačítko vedle popelnic vrací soubory přesunuté do Delete"
        " zpět do původní složky (LIFO — nejnovější přesunutý jako první);"
        " aktivní jen pokud byl v&nbsp;tomto sezení přesunut alespoň jeden soubor</li>"
        "<li><b>PDF toolbar</b> — při otevření PDF se automaticky zobrazí lišta s tlačítky"
        " ◀&nbsp;/&nbsp;▶ pro listování, indikátorem aktuální strany, tlačítkem"
        " <i>Přejít na stranu</i> a tlačítkem <i>Screenshot</i> pro zachycení stránky"
        " jako obrázku (pak ji lze uložit přes Uložit jako)</li>"
        "<li><b>Pamatování velikosti okna</b> — aplikace si pamatuje velikost a polohu okna;"
        " při změně monitoru (jiné rozlišení) se spustí v&nbsp;předvolené velikosti</li>"
        "</ul>"
        "<h3>Technologie</h3>"
        "<p>Napsáno v C++17 s frameworkem Qt&nbsp;6. "
        "Postaveno na macOS a Windows (GitHub Actions).</p>"
        "<p style='margin-top:16px; color:#888; font-size:12px;'>"
        "© 2025–2026 Jiří Krejčí</p>"
    ).arg(version);

    auto *dlg = new HelpDialog(tr("O programu"), html, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

// ── Podporované formáty ───────────────────────────────────────────────────────
// Aktualizuj při přidání nebo odebrání formátu v ImageFormats.hpp.
void HelpDialog::showFormats(QWidget *parent)
{
    const QString html =
        "<h2>Podporované formáty</h2>"
        "<h3>Obrázky</h3>"
        "<table>"
        "<tr><th>Přípona</th><th>Formát</th><th>Poznámka</th></tr>"
        "<tr><td><kbd>.jpg</kbd>&nbsp;/&nbsp;<kbd>.jpeg</kbd></td>"
        "    <td>JPEG</td>"
        "    <td>Nejpoužívanější formát pro fotografie</td></tr>"
        "<tr><td><kbd>.png</kbd></td>"
        "    <td>PNG</td>"
        "    <td>Bezztrátová komprese, podpora průhlednosti</td></tr>"
        "<tr><td><kbd>.gif</kbd></td>"
        "    <td>GIF</td>"
        "    <td>Statické i animované obrázky</td></tr>"
        "<tr><td><kbd>.bmp</kbd></td>"
        "    <td>BMP</td>"
        "    <td>Bitmapový formát Windows</td></tr>"
        "<tr><td><kbd>.webp</kbd></td>"
        "    <td>WebP</td>"
        "    <td>Moderní webový formát (Google)</td></tr>"
        "<tr><td><kbd>.tiff</kbd>&nbsp;/&nbsp;<kbd>.tif</kbd></td>"
        "    <td>TIFF</td>"
        "    <td>Profesionální bezztrátový formát</td></tr>"
        "</table>"
        "<h3>Dokumenty</h3>"
        "<table>"
        "<tr><th>Přípona</th><th>Formát</th><th>Poznámka</th></tr>"
        "<tr><td><kbd>.pdf</kbd></td>"
        "    <td>PDF</td>"
        "    <td>Přenosný formát dokumentů (lze vypnout v nastavení)</td></tr>"
        "</table>"
        "<p class='note'>Filtr souborů ignoruje velikost písmen&nbsp;—"
        " <kbd>.JPG</kbd> i <kbd>.jpg</kbd> jsou rozpoznány shodně.</p>"
        "<p class='note'>Seznam obrázkových formátů se odvozuje z Qt pluginů"
        " nainstalovaných v systému, takže podle prostředí mohou být dostupné"
        " i&nbsp;<kbd>.heic</kbd>, <kbd>.heif</kbd>, <kbd>.svg</kbd>,"
        " <kbd>.jp2</kbd> a&nbsp;další.</p>";

    auto *dlg = new HelpDialog(tr("Podporované formáty"), html, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

// ── Klávesové zkratky ─────────────────────────────────────────────────────────
// Aktualizuj při přidání nebo změně klávesových zkratek v MainWindow.cpp.
void HelpDialog::showShortcuts(QWidget *parent)
{
    const QString html =
        "<h2>Klávesové zkratky</h2>"
        "<h3>Navigace a zobrazení</h3>"
        "<table>"
        "<tr><th>Akce</th><th>Zkratka</th></tr>"
        "<tr><td>Otevřít složku</td>"
        "    <td><kbd>⌘ Shift O</kbd> (Mac) / <kbd>Ctrl+Shift+O</kbd> (Windows)</td></tr>"
        "<tr><td>Otevřít soubor</td>"
        "    <td><kbd>⌘ O</kbd> (Mac) / <kbd>Ctrl+O</kbd> (Windows)</td></tr>"
        "<tr><td>Předchozí obrázek</td>"
        "    <td><kbd>←</kbd> (šipka vlevo)</td></tr>"
        "<tr><td>Další obrázek</td>"
        "    <td><kbd>→</kbd> (šipka vpravo)</td></tr>"
        "<tr><td>První obrázek</td>"
        "    <td><kbd>↑</kbd> (šipka nahoru)</td></tr>"
        "<tr><td>Poslední obrázek</td>"
        "    <td><kbd>↓</kbd> (šipka dolů)</td></tr>"
        "<tr><td>Přizpůsobit oknu / původní velikost</td>"
        "    <td><kbd>Mezerník</kbd> nebo <kbd>Střed myši</kbd></td></tr>"
        "<tr><td>Přepnout panel náhledů</td>"
        "    <td><kbd>⌘ T</kbd> (Mac) / <kbd>Ctrl+T</kbd> (Windows)</td></tr>"
        "<tr><td>Předchozí stránka PDF</td>"
        "    <td><kbd>PgUp</kbd></td></tr>"
        "<tr><td>Následující stránka PDF</td>"
        "    <td><kbd>PgDn</kbd></td></tr>"
        "</table>"
        "<h3>Slideshow a videa</h3>"
        "<table>"
        "<tr><th>Akce</th><th>Zkratka</th></tr>"
        "<tr><td>Spustit / zastavit slideshow</td>"
        "    <td><kbd>S</kbd></td></tr>"
        "<tr><td>Přehrát video</td>"
        "    <td><kbd>V</kbd></td></tr>"
        "</table>"
        "<h3>Úpravy a režimy</h3>"
        "<table>"
        "<tr><th>Akce</th><th>Zkratka</th></tr>"
        "<tr><td>Přejmenovat obrázek</td>"
        "    <td><kbd>R</kbd></td></tr>"
        "<tr><td>Otočit doleva / doprava</td>"
        "    <td><kbd>[</kbd> nebo <kbd>L</kbd> / <kbd>]</kbd></td></tr>"
        "<tr><td>Odstranit / přesunout obrázek</td>"
        "    <td><kbd>Delete</kbd>, <kbd>d</kbd>, <kbd>D</kbd></td></tr>"
        "<tr><td>Plná obrazovka</td>"
        "    <td><kbd>F</kbd></td></tr>"
        "<tr><td>Ukončit plnou obrazovku</td>"
        "    <td><kbd>Esc</kbd></td></tr>"
        "<tr><td>Ukončit aplikaci</td>"
        "    <td><kbd>⌘ Q</kbd> (Mac) / <kbd>Ctrl+Q</kbd> (Windows)</td></tr>"
        "</table>";

    auto *dlg = new HelpDialog(tr("Klávesové zkratky"), html, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

// ── Co je nového ─────────────────────────────────────────────────────────────
// Aktualizuj při každém vydání nové verze.
void HelpDialog::showWhatsNew(QWidget *parent)
{
    const QString html =
        "<h2>Co je nového</h2>"
        "<h3>Verze 0.11</h3>"
        "<ul>"
        "<li><b>PDF toolbar</b> — při zobrazení PDF se automaticky objeví lišta;"
        " tlačítka ◀&nbsp;/&nbsp;▶ listují stránky (stejné jako PgUp/PgDn);"
        " indikátor zobrazuje aktuální stranu a celkový počet</li>"
        "<li><b>Přejít na stranu</b> — dialog pro zadání čísla stránky (1&nbsp;–&nbsp;N),"
        " okamžitý skok na zvolenou stranu</li>"
        "<li><b>Screenshot PDF stránky</b> — tlačítko Screenshot zachytí aktuálně"
        " zobrazenou stránku jako statický obrázek; aktivují se tlačítka Uložit / Uložit jako"
        " a stránku lze uložit jako JPEG</li>"
        "</ul>"
        "<h3>Verze 0.10</h3>"
        "<ul>"
        "<li><b>Recyklace (♻)</b> — třetí tlačítko u popelnic vrací soubory přesunuté do Delete"
        " zpět do původní složky, v&nbsp;obráceném pořadí (nejnovější přesunutý jako první);"
        " po obnovení se aplikace přepne na obnovenou složku a soubor;"
        " tlačítko se deaktivuje, pokud nejsou žádné přesunuté soubory ke vrácení</li>"
        "</ul>"
        "<h3>Verze 0.9</h3>"
        "<ul>"
        "<li><b>Uložit</b> — po ořezu nebo otočení obrázku se aktivuje tlačítko Uložit;"
        " aplikace se zeptá, zda přepsat originál, nebo uložit pod novým názvem</li>"
        "<li><b>Uložit jako</b> — kdykoli dostupné pro statické obrázky; dialog pro zadání"
        " názvu souboru a výběr cílové složky (originál nebo oblíbená složka);"
        " soubor se vždy uloží jako JPEG</li>"
        "<li><b>Po uložení pod novým názvem</b> se aplikace automaticky přepne na nově uložený soubor</li>"
        "<li><b>Pamatování velikosti okna</b> — aplikace si pamatuje velikost a polohu okna"
        " při zavření; při jiném rozlišení obrazovky (jiný monitor) se spustí v předvolené velikosti</li>"
        "</ul>"
        "<h3>Verze 0.8</h3>"
        "<ul>"
        "<li><b>Systém kategorií</b> — každému obrázku lze přiřadit až 5 barevných kategorií"
        " přes tlačítka v toolbaru; přiřazení se uchovává v SQLite databázi vedle nastavení</li>"
        "<li><b>Filtrování složky podle kategorií</b> — filtr toolbar zobrazuje jen kategorie"
        " skutečně přiřazené obrázkům v aktuální složce; filtr se načítá líně (jen když je viditelný)</li>"
        "<li><b>Správa kategorií přes pravý klik</b> — kliknutí pravým tlačítkem na libovolný"
        " štítek kategorie zobrazí menu s možnostmi Přejmenovat, Změnit barvu, Odstranit</li>"
        "<li><b>Oblíbené složky</b> — rychlý přístup k naposledy používaným složkám v menu Soubor</li>"
        "<li><b>Oprava výběru barvy</b> — při vytvoření nové kategorie bez zvolené barvy se nyní"
        " skutečně vybere náhodná barva z dosud nepoužitých (dříve vždy první barva)</li>"
        "<li><b>Toolbar oblíbených složek</b> — tlačítko ⭐ Oblíbené zobrazí/skryje lištu"
        " s barevnými tlačítky složek; klik otevře složku, pravý klik ji odebere;"
        " [+ Přidat] přidá aktuální složku (max 10, při překročení limitu zobrazí varování)</li>"
        "<li><b>Ořez obrázku</b> — tlačítko ✂ v toolbaru aktivuje výběr oblasti; po označení"
        " se zobrazení ořízne; Kopírovat obrázek uloží výřez jako dočasný JPEG soubor do schránky"
        " (lze vložit do mailu, dokumentu nebo webové aplikace jako JPEG);"
        " ESC nebo pravý klik zruší výběr bez ořezu</li>"
        "</ul>"
        "<h3>Verze 0.7</h3>"
        "<ul>"
        "<li><b>Animované GIFy</b> — přehrávají se přes QMovie (dříve jen první snímek)</li>"
        "<li><b>Volba řazení</b> — Nastavení → Řazení souborů: podle názvu, data"
        " změny nebo velikosti, vzestupně i sestupně</li>"
        "<li><b>Indikátor zoomu</b> — aktuální zvětšení (%) vpravo ve status baru"
        " (u obrázků; 100&nbsp;% = originální velikost)</li>"
        "<li><b>Otočení obrázku</b> — klávesy <kbd>[</kbd>/<kbd>L</kbd> doleva,"
        " <kbd>]</kbd> doprava</li>"
        "<li><b>Drag &amp; drop</b> — přetažení složky nebo souboru přímo do okna</li>"
        "<li><b>Kontextové menu</b> — pravým tlačítkem: Zobrazit ve Finderu,"
        " kopírovat obrázek nebo cestu</li>"
        "<li><b>Více formátů</b> — seznam podporovaných obrázků se odvozuje"
        " z nainstalovaných Qt pluginů (HEIC, HEIF, SVG, JP2…)</li>"
        "<li>Jednotkové testy jádra (řazení, formáty, cache) proti regresím</li>"
        "</ul>"
        "<h3>Verze 0.6</h3>"
        "<ul>"
        "<li><b>Přepínatelná rozložení UI</b> — Klasický, Filmový pás, Imerzivní, "
        "Galerie a Pro režim (volba se pamatuje)</li>"
        "<li><b>Disková cache náhledů</b> — rychlejší opětovné načítání složek; "
        "automatický úklid při 500&nbsp;MB, zobrazení velikosti v nastavení</li>"
        "<li><b>Směrový prefetch</b> — při listování se přednačítá 5 souborů vpřed</li>"
        "<li><b>Přirozené řazení</b> — <kbd>img2</kbd> se řadí před <kbd>img10</kbd></li>"
        "<li><b>Vycentrované náhledy</b> — miniatury na výšku i PDF se vejdou do buňky</li>"
        "<li>Řada oprav stability — odstranění race conditions při načítání, "
        "mazání a přehrávání videa</li>"
        "</ul>"
        "<h3>Verze 0.5</h3>"
        "<ul>"
        "<li><b>Podpora PDF souborů</b> — PDF se zobrazují jako obrázky vedle ostatních souborů</li>"
        "<li><b>Stránkování PDF</b> — <kbd>PgDn</kbd> a <kbd>PgUp</kbd> pro listování mezi stránkami</li>"
        "<li><b>Náhledy PDF</b> — první stránka se generuje jako miniatura v panelu</li>"
        "<li><b>Nastavení PDF</b> — lze vypnout zpracování PDF souborů v nastaveních</li>"
        "<li><b>Status bar</b> — zobrazuje počet stránek u PDF souborů</li>"
        "<li>Stejné funkce pro PDF jako pro obrázky (zoom, delete, move to Delete folder)</li>"
        "</ul>"
        "<h3>Verze 0.4</h3>"
        "<ul>"
        "<li><b>Přehrávání videa</b> — podpora MP4, MOV, WebM a dalších formátů přes VLC</li>"
        "<li><b>Přejmenování obrázků</b> — nové tlačítko v liště a klávesová zkratka (R)</li>"
        "<li><b>Oprava spouštění</b> — aplikace se nyní spouští v jedné instanci při otevření souboru z Finderu</li>"
        "<li>Vyčištěn legacy Python kód — přechod na čistě C++ projekt</li>"
        "<li>Vylepšení VLC integrace — opravy chybějících klávesových zkratek a padajících procesů</li>"
        "</ul>"
        "<h3>Verze 0.3</h3>"
        "<ul>"
        "<li><b>Tlačítko přejmenování</b> — s ikonou v nástrojové liště</li>"
        "<li>Oprava při otevření obrázku z Finderu bez běžící aplikace</li>"
        "<li>Vylepšená detekce spouštěcího stavu aplikace</li>"
        "</ul>"
        "<h3>Verze 0.2</h3>"
        "<ul>"
        "<li>Sestava pro <b>Windows</b> — automatické sestavení přes GitHub Actions</li>"
        "<li>Oprava: panel náhledů se správně obnoví po opuštění celé obrazovky</li>"
        "<li>Menu Nápověda s přehledem funkcí, formátů, zkratek a changelogu</li>"
        "<li>Funkce pro mazání a přesunutí obrázků</li>"
        "</ul>"
        "<h3>Verze 0.1</h3>"
        "<ul>"
        "<li>Počáteční verze pro macOS</li>"
        "<li>Panel náhledů s rychlým načítáním ve vlákně na pozadí</li>"
        "<li>Slideshow s nastavitelným intervalem</li>"
        "<li>Přiblížení kolečkem myši / gesty trackpadu</li>"
        "<li>Režim celé obrazovky</li>"
        "<li>Zapamatování poslední otevřené složky</li>"
        "<li>Otevření souboru přímo z Finderu</li>"
        "</ul>";

    auto *dlg = new HelpDialog(tr("Co je nového"), html, parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

} // namespace pictureviewer
