#pragma once

#include "core/DiagLog.hpp"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

namespace pictureviewer {

// Opakuje souborovou operaci, dokud neuspěje nebo nedojdou pokusy.
// Mezi pokusy zpracovává event loop (bez uživatelského vstupu) — na Windows
// uvolňuje Media Foundation handle videa asynchronně, takže rename/delete
// hned po zastavení přehrávání může selhat a o chvíli později uspět.
// Výchozí 8 × 250 ms = okno 2 sekundy.
template <typename Op>
bool tryWithRetry(Op op, int attempts = 8, int delayMs = 250)
{
    // Do logu jen pomalé nebo opakované operace (přes síť trvá i jeden pokus
    // desetiny sekundy, a UI je po tu dobu zablokované).
    QElapsedTimer timer;
    timer.start();
    auto report = [&](bool ok, int tries) {
        if (tries > 1 || timer.elapsed() >= 150) {
            diag::log(QStringLiteral("souborová operace: %1 po %2 pokusech, %3 ms (UI vlákno)")
                          .arg(ok ? QStringLiteral("OK") : QStringLiteral("SELHALA")).arg(tries).arg(timer.elapsed()));
        }
    };
    for (int i = 0; i < attempts; ++i) {
        if (op()) {
            report(true, i + 1);
            return true;
        }
        if (i + 1 < attempts) {
            QEventLoop loop;
            QTimer::singleShot(delayMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }
    report(false, attempts);
    return false;
}

} // namespace pictureviewer
