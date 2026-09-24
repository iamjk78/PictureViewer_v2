#pragma once

#include <QCoreApplication>
#include <QProgressDialog>
#include <QString>
#include <QThread>

#include <memory>

namespace pictureviewer {

// Průběh a možnost zrušení pro dávkové operace nad soubory (hromadné mazání,
// přesun). Na pomalém úložišti trvá jedna operace sekundy až desítky sekund;
// bez tohohle okna vypadala aplikace celou dobu zamrzle a nešlo ji přerušit.
//
// Dialog se vyrábí jen pro dávku (total >= 2). Pro 5 a víc souborů se ukáže
// hned (u síťového úložiště by se po prahu zobrazil až po skončení první,
// dlouhé operace), pro méně až když operace trvá.
class BatchProgress
{
public:
    BatchProgress(QWidget *parent, const QString &title, int total)
        : m_total(total)
    {
        if (total < 2) {
            return;
        }
        m_dialog = std::make_unique<QProgressDialog>(
            QString(), QCoreApplication::translate("pictureviewer::BatchProgress", "Zrušit"),
            0, total, parent);
        m_dialog->setWindowTitle(title);
        m_dialog->setWindowModality(Qt::WindowModal);
        m_dialog->setMinimumDuration(total >= 5 ? 0 : 400);
        m_dialog->setAutoClose(false);
        m_dialog->setAutoReset(false);
        m_dialog->setMinimumWidth(420);
    }

    // Volat PŘED zpracováním souboru číslo `done` (od 0). Dovolí okna
    // překreslit a zpracovat kliknutí na Zrušit. Vrací false, pokud uživatel
    // operaci zrušil — volající má smyčku přerušit.
    bool step(int done, const QString &fileName)
    {
        if (!m_dialog) {
            return true;
        }
        if (m_canceled) {
            return false;
        }
        m_dialog->setLabelText(
            QCoreApplication::translate("pictureviewer::BatchProgress", "%1 / %2\n%3")
                .arg(done + 1).arg(m_total).arg(fileName));
        m_dialog->setValue(done);
        QCoreApplication::processEvents();
        if (stepDelayMs() > 0) {   // jen testy: simulace pomalého úložiště
            QThread::msleep(static_cast<unsigned long>(stepDelayMs()));
            QCoreApplication::processEvents();
        }
        if (m_dialog->wasCanceled()) {
            m_canceled = true;
            return false;
        }
        return true;
    }

    bool canceled() const { return m_canceled; }

    // Jen pro testy: každý krok navíc počká (simulace pomalé operace).
    static void setStepDelayForTesting(int ms) { stepDelayMs() = ms; }

private:
    static int &stepDelayMs()
    {
        static int delay = 0;
        return delay;
    }

    std::unique_ptr<QProgressDialog> m_dialog;
    int m_total = 0;
    bool m_canceled = false;
};

} // namespace pictureviewer
