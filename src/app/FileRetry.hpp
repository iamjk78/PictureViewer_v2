#pragma once

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
    for (int i = 0; i < attempts; ++i) {
        if (op()) {
            return true;
        }
        if (i + 1 < attempts) {
            QEventLoop loop;
            QTimer::singleShot(delayMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }
    return false;
}

} // namespace pictureviewer
