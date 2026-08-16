#include "app/ToolbarStyle.hpp"

#include <QSize>
#include <QToolButton>

namespace pictureviewer {

QString secondaryToolbarStyle()
{
    return QStringLiteral(
        "QToolButton { border: none; border-radius: 3px; "
        "  padding: 2px; min-width: %1px; width: %1px; min-height: %1px; height: %1px; "
        "  background: transparent; font-size: 14px; } "
        "QToolButton:hover { background-color: rgba(0, 0, 0, 0.05); } "
        "QToolBar::separator { background: transparent; width: 0px; }")
        .arg(kSecondaryToolbarIconSize);
}

void applyToolbarButtonSize(QToolButton *button, int size)
{
    if (button == nullptr) {
        return;
    }
    button->setFixedSize(size, size);
    // Ikona o 2 px menší než tlačítko — bez toho se dotýká jeho okraje.
    button->setIconSize(QSize(size - 2, size - 2));
}

void applyMainToolbarButtonSize(QToolButton *button)
{
    if (button == nullptr) {
        return;
    }
    button->setFixedSize(kMainToolbarButtonWidth, kMainToolbarIconSize);
    button->setIconSize(QSize(kMainToolbarIconSize - 2, kMainToolbarIconSize - 2));
}

} // namespace pictureviewer
