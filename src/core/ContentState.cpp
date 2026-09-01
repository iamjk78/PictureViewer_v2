#include "core/ContentState.hpp"

namespace pictureviewer::contentstate {

ActionStates deriveActionStates(const ContentStatus &status)
{
    const ContentKind kind = status.kind;

    // Statický rastr, se kterým jde manipulovat (otočit, oříznout) a uložit ho
    // jako JPEG. GIF ne — QMovie přemaluje pixmapu dalším snímkem, takže by se
    // úprava jen zdánlivě provedla a Uložit by animaci přepsalo jedním snímkem.
    // PDF ne — re-render při zoomu úpravu zahodí. Video ne — ImageView je pod
    // ním skrytý a úprava by se týkala naposledy zobrazeného obrázku.
    const bool isStaticRaster = kind == ContentKind::Image || kind == ContentKind::Capture;

    // Akce nad SOUBOREM ve složce (smazat, přejmenovat, přesunout, oštítkovat).
    // U Capture musí být zakázané: index sice na platný soubor ukazuje, ale je
    // to soubor, který uživatel nevidí — zásah do něj by byl pro uživatele
    // neviditelný a u mazání nevratný.
    const bool actsOnCurrentFile =
        status.hasCurrentFile && kind != ContentKind::Capture && !status.browsingLocked;

    ActionStates s;

    s.previous  = status.hasFiles && !status.browsingLocked;
    s.next      = s.previous;
    s.slideshow = s.previous;

    s.rotate = kind == ContentKind::Image || kind == ContentKind::Capture;
    s.crop   = isStaticRaster;

    // Uložit = přepsat originál. Snímek žádný originál nemá (index ukazuje na
    // cizí soubor), takže jen Uložit jako.
    s.save = kind == ContentKind::Image && status.modified && status.hasCurrentFile;

    // Uložit jako umí: obrázek i snímek jako JPEG, video jako kopii souboru.
    // Snímek ZÁMĚRNĚ i bez otevřené složky — jinak by po startu aplikace
    // nešel uložit vůbec.
    s.saveAs = kind == ContentKind::Capture
               || (kind == ContentKind::Image && status.hasCurrentFile)
               || (kind == ContentKind::Video && status.hasCurrentFile);

    s.deleteFile   = actsOnCurrentFile;
    s.rename       = actsOnCurrentFile;
    s.moveToFolder = actsOnCurrentFile;
    s.labels       = actsOnCurrentFile;

    s.pdfToolbar = kind == ContentKind::Pdf;

    // Zoom v procentech dává smysl jen tam, kde 100 % = 1:1 pixel. U PDF se
    // rozlišení renderu přizpůsobuje zoomu, u videa má vlastní ovládání.
    s.fitControls = kind == ContentKind::Image
                    || kind == ContentKind::AnimatedGif
                    || kind == ContentKind::Capture;

    return s;
}

} // namespace pictureviewer::contentstate
