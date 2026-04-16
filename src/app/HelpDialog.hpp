#pragma once

#include <QDialog>

class QTextBrowser;

namespace pictureviewer {

// ── HelpDialog ────────────────────────────────────────────────────────────────
// Modální dialog s HTML obsahem pro sekce menu Nápověda.
//
// DŮLEŽITÉ: Při přidání nových funkcí nebo klávesových zkratek je nutné
//           aktualizovat odpovídající statickou metodu v HelpDialog.cpp!
//
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(const QString &title, const QString &html, QWidget *parent = nullptr);

    // Jednotlivé sekce nápovědy
    static void showAbout(QWidget *parent);
    static void showFormats(QWidget *parent);
    static void showShortcuts(QWidget *parent);
    static void showWhatsNew(QWidget *parent);

private:
    QTextBrowser *m_browser;
};

} // namespace pictureviewer
