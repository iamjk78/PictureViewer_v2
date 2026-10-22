#pragma once

#include <QAction>
#include <QToolBar>
#include <QWidget>

namespace pictureviewer {

// Odebere z toolbaru kontejner vložený přes addWidget/insertWidget a uvolní ho.
// QToolBar::removeAction() akci ani její widget nemaže, takže při každém
// přestavění tlačítek (štítky, oblíbené, přesun) zůstával v paměti starý
// kontejner i akce.
inline void removeToolbarContainer(QToolBar *toolbar, QAction *action)
{
    QWidget *widget = toolbar->widgetForAction(action);
    toolbar->removeAction(action);
    if (widget != nullptr) {
        widget->deleteLater();
    }
    action->deleteLater();
}

} // namespace pictureviewer
