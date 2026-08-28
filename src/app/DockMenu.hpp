#pragma once

namespace pictureviewer {

// macOS: doplní do dokovací nabídky (pravý klik / dlouhý stisk na ikonu
// v Docku) položku „Spustit další", která otevře novou instanci aplikace
// (stejné chování jako `open -n`) — i když už jedna instance běží, macOS by
// jinak jen aktivoval ji. Volat jednou po vytvoření QApplication.
// Na ostatních platformách no-op (koncept dokovací nabídky je macOS-specifický;
// Windows má jinou obdobu — jump listy — kterou aplikace nevyužívá).
void setupDockMenu();

} // namespace pictureviewer
