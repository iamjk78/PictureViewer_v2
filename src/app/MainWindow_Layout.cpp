// MainWindow_Layout.cpp — layout switching, dock, fullscreen, overlay toolbar
// QPushButton must be included BEFORE MainWindow.hpp to resolve the
// elaborated-type-specifier "class QPushButton*" in the MainWindow class body.
#include <QPushButton>
#include "app/MainWindow.hpp"

#include "app/ImageView.hpp"
#include "app/MetadataPanel.hpp"
#include "app/ThumbnailPanel.hpp"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

namespace pictureviewer {

void MainWindow::setupDock()
{
    auto *dock = new QDockWidget(tr("Náhledy"), this);
    dock->setWidget(m_thumbnailPanel);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    m_thumbnailDock = dock;
    m_togglePanelAction = dock->toggleViewAction();
    m_togglePanelAction->setText(tr("Panel náhledů"));

    m_metadataPanel = new MetadataPanel(this);
    m_metadataDock = new QDockWidget(tr("Informace"), this);
    m_metadataDock->setWidget(m_metadataPanel);
    m_metadataDock->setFeatures(QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);
    m_metadataDock->hide();
}

void MainWindow::applyUiLayout(UiLayout layout)
{
    if (m_galleryGridActive) {
        leaveGalleryGrid();
    }

    m_uiLayout = layout;
    m_overlayToolbar->hide();

    switch (layout) {
    case UiLayout::Classic:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Vertical);
        addDockWidget(Qt::LeftDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        break;

    case UiLayout::Filmstrip:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Horizontal);
        addDockWidget(Qt::BottomDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        break;

    case UiLayout::Immersive:
        m_thumbnailDock->hide();
        m_metadataDock->hide();
        m_mainToolbar->hide();
        statusBar()->hide();
        break;

    case UiLayout::Gallery:
        m_metadataDock->hide();
        m_mainToolbar->show();
        statusBar()->show();
        enterGalleryGrid();
        break;

    case UiLayout::Pro:
        m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Horizontal);
        addDockWidget(Qt::BottomDockWidgetArea, m_thumbnailDock);
        m_thumbnailDock->show();
        m_metadataDock->show();
        m_mainToolbar->show();
        statusBar()->show();
        break;
    }
}

void MainWindow::enterGalleryGrid()
{
    if (m_galleryGridActive) {
        return;
    }
    m_thumbnailDock->hide();
    m_thumbnailDock->setWidget(nullptr);
    m_thumbnailPanel->setDisplayMode(ThumbnailPanel::DisplayMode::Grid);
    m_centralStack->addWidget(m_thumbnailPanel);
    m_centralStack->setCurrentWidget(m_thumbnailPanel);
    m_galleryGridActive = true;
}

void MainWindow::leaveGalleryGrid()
{
    if (!m_galleryGridActive) {
        return;
    }
    m_thumbnailDock->setWidget(m_thumbnailPanel);
    m_centralStack->removeWidget(m_thumbnailPanel);
    m_centralStack->setCurrentWidget(m_imageView);
    m_galleryGridActive = false;
}

void MainWindow::showGalleryGrid()
{
    if (m_galleryGridActive) {
        m_centralStack->setCurrentWidget(m_thumbnailPanel);
    }
}

void MainWindow::setupOverlayToolbar()
{
    m_overlayToolbar = new QWidget(this);
    m_overlayToolbar->setObjectName("overlayToolbar");
    m_overlayToolbar->setStyleSheet(
        "#overlayToolbar { background-color: rgba(30, 30, 32, 220); border-radius: 22px; }"
        "QToolButton { color: #e8e6df; background: transparent; border: none;"
        "              font-size: 16px; padding: 4px 8px; }"
        "QToolButton:hover { color: #ffffff; background-color: rgba(255, 255, 255, 30);"
        "                    border-radius: 8px; }"
    );

    auto *layout = new QHBoxLayout(m_overlayToolbar);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(6);

    const auto addButton = [this, layout](const QString &glyph, const QString &tooltip, auto slot) {
        auto *button = new QToolButton(m_overlayToolbar);
        button->setText(glyph);
        button->setToolTip(tooltip);
        connect(button, &QToolButton::clicked, this, slot);
        layout->addWidget(button);
    };

    addButton(QStringLiteral("◀"), tr("Předchozí"), &MainWindow::showPreviousImage);
    addButton(QStringLiteral("⏯"), tr("Slideshow"), &MainWindow::toggleSlideshow);
    addButton(QStringLiteral("▶"), tr("Další"), &MainWindow::showNextImage);
    addButton(QStringLiteral("⛶"), tr("Celá obrazovka"), &MainWindow::toggleFullscreen);
    addButton(QStringLiteral("✎"), tr("Přejmenovat"), &MainWindow::renameCurrentImage);
    addButton(QStringLiteral("🗑"), tr("Smazat"), &MainWindow::deleteOrMoveCurrentImage);

    m_overlayToolbar->hide();

    m_overlayHideTimer = new QTimer(this);
    m_overlayHideTimer->setSingleShot(true);
    m_overlayHideTimer->setInterval(2500);
    connect(m_overlayHideTimer, &QTimer::timeout, m_overlayToolbar, &QWidget::hide);
}

void MainWindow::positionOverlayToolbar()
{
    m_overlayToolbar->adjustSize();
    const int x = (width() - m_overlayToolbar->width()) / 2;
    const int y = height() - m_overlayToolbar->height() - 24;
    m_overlayToolbar->move(x, y);
}

void MainWindow::showOverlayToolbar()
{
    positionOverlayToolbar();
    m_overlayToolbar->show();
    m_overlayToolbar->raise();
    m_overlayHideTimer->start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_overlayToolbar != nullptr && m_overlayToolbar->isVisible()) {
        positionOverlayToolbar();
    }
}

} // namespace pictureviewer
