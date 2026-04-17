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
        "<li>Panel náhledů — přehled všech obrázků ve složce</li>"
        "<li>Automatická prezentace (slideshow) s nastavitelným intervalem</li>"
        "<li>Přiblížení a oddálení kolečkem myši nebo gesty trackpadu</li>"
        "<li>Přizpůsobení obrázku oknu nebo zobrazení v originální velikosti (1:1)</li>"
        "<li>Plnohodnotný režim celé obrazovky</li>"
        "<li>Zapamatování poslední otevřené složky</li>"
        "<li>Otevření souboru přímo z Finderu (macOS) nebo Průzkumníka (Windows)</li>"
        "</ul>"
        "<h3>Technologie</h3>"
        "<p>Napsáno v C++17 s frameworkem Qt&nbsp;6. "
        "Sestaveno pro macOS a Windows (GitHub Actions).</p>"
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
        "<h2>Podporované formáty obrázků</h2>"
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
        "<p class='note'>Filtr souborů ignoruje velikost písmen&nbsp;—"
        " <kbd>.JPG</kbd> i <kbd>.jpg</kbd> jsou rozpoznány shodně.</p>";

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
        "<table>"
        "<tr><th>Akce</th><th>macOS</th><th>Windows&nbsp;/&nbsp;Linux</th></tr>"
        "<tr><td>Otevřít složku</td>"
        "    <td><kbd>⌘ Shift O</kbd></td>"
        "    <td><kbd>Ctrl+Shift+O</kbd></td></tr>"
        "<tr><td>Otevřít soubor</td>"
        "    <td><kbd>⌘ O</kbd></td>"
        "    <td><kbd>Ctrl+O</kbd></td></tr>"
        "<tr><td>Předchozí obrázek</td>"
        "    <td><kbd>←</kbd></td>"
        "    <td><kbd>←</kbd></td></tr>"
        "<tr><td>Další obrázek</td>"
        "    <td><kbd>→</kbd></td>"
        "    <td><kbd>→</kbd></td></tr>"
        "<tr><td>První obrázek</td>"
        "    <td><kbd>↑</kbd></td>"
        "    <td><kbd>↑</kbd></td></tr>"
        "<tr><td>Poslední obrázek</td>"
        "    <td><kbd>↓</kbd></td>"
        "    <td><kbd>↓</kbd></td></tr>"
        "<tr><td>Zoom na 100% (1:1)</td>"
        "    <td><kbd>Mezerník</kbd></td>"
        "    <td><kbd>Mezerník</kbd></td></tr>"
        "<tr><td>Spustit / zastavit slideshow</td>"
        "    <td><kbd>Mezerník</kbd></td>"
        "    <td><kbd>Mezerník</kbd></td></tr>"
        "<tr><td>Celá obrazovka</td>"
        "    <td><kbd>F</kbd></td>"
        "    <td><kbd>F</kbd></td></tr>"
        "<tr><td>Ukončit celou obrazovku</td>"
        "    <td><kbd>Esc</kbd></td>"
        "    <td><kbd>Esc</kbd></td></tr>"
        "<tr><td>Odstranit/přesunout obrázek<br/><span style='font-size:11px;color:#666;'>(podle nastavení)</span></td>"
        "    <td><kbd>Delete</kbd>,&nbsp;<kbd>d</kbd>,&nbsp;<kbd>D</kbd></td>"
        "    <td><kbd>Delete</kbd>,&nbsp;<kbd>d</kbd>,&nbsp;<kbd>D</kbd></td></tr>"
        "<tr><td>Ukončit aplikaci</td>"
        "    <td><kbd>⌘ Q</kbd></td>"
        "    <td><kbd>Ctrl+Q</kbd></td></tr>"
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
        "<h3>Verze 0.2</h3>"
        "<ul>"
        "<li>Sestava pro <b>Windows</b> — automatické sestavení přes GitHub Actions</li>"
        "<li>Oprava: panel náhledů se nyní správně obnoví po opuštění celé obrazovky</li>"
        "<li>Menu Nápověda s přehledem funkcí, formátů, zkratek a changelogu</li>"
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
