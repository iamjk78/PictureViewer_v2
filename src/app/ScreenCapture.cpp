// Windows / Linux implementace výřezu obrazovky přes Qt overlay.
// macOS má vlastní implementaci v ScreenCapture_mac.mm.

#ifndef __APPLE__

#include "ScreenCapture.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>

namespace pictureviewer {

namespace {

QString screenshotTempDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dir  = QDir(base).filePath(QStringLiteral("PictureViewer_Screenshots"));
    QDir().mkpath(dir);
    return dir;
}

QString makeScreenshotPath()
{
    const QString name = QStringLiteral("screenshot_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    return QDir(screenshotTempDir()).filePath(name);
}

// Celoobrazovkový overlay přes virtuální plochu všech monitorů.
class CaptureOverlay : public QWidget
{
public:
    CaptureOverlay(QPixmap background, QRect virtualGeom)
        : m_background(std::move(background)), m_virtualGeom(virtualGeom)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint
                       | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
        setGeometry(m_virtualGeom);
        setCursor(Qt::CrossCursor);
    }

    QRect run()
    {
        show();
        raise();
        activateWindow();
        m_loop.exec();
        return m_selection;
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.drawPixmap(0, 0, m_background);
        p.fillRect(rect(), QColor(0, 0, 0, 110));
        if (!m_selection.isNull()) {
            p.drawPixmap(m_selection, m_background, m_selection);
            p.setPen(QPen(QColor(0, 160, 255), 2));
            p.drawRect(m_selection.adjusted(0, 0, -1, -1));
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_origin = e->pos();
            m_selection = QRect(m_origin, QSize());
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (e->buttons() & Qt::LeftButton) {
            m_selection = QRect(m_origin, e->pos()).normalized();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_selection = QRect(m_origin, e->pos()).normalized();
            m_loop.quit();
            close();
        }
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            m_selection = QRect();
            m_loop.quit();
            close();
        }
    }

private:
    QPixmap    m_background;
    QRect      m_virtualGeom;
    QPoint     m_origin;
    QRect      m_selection;
    QEventLoop m_loop;
};

} // namespace

ScreenCaptureResult captureScreenRegion(QWidget *parentWindow)
{
    const bool wasVisible = parentWindow && parentWindow->isVisible();
    if (parentWindow) {
        parentWindow->hide();
    }

    {
        QEventLoop delayLoop;
        QTimer::singleShot(200, &delayLoop, &QEventLoop::quit);
        delayLoop.exec();
    }

    QRect virtualGeom;
    for (QScreen *s : QGuiApplication::screens()) {
        virtualGeom = virtualGeom.united(s->geometry());
    }

    QPixmap combined(virtualGeom.size());
    combined.fill(Qt::black);
    {
        QPainter p(&combined);
        for (QScreen *s : QGuiApplication::screens()) {
            QPixmap shot = s->grabWindow(0);
            shot.setDevicePixelRatio(1.0);
            const QRect g = s->geometry();
            p.drawPixmap(QRect(g.topLeft() - virtualGeom.topLeft(), g.size()), shot);
        }
    }

    CaptureOverlay overlay(combined, virtualGeom);
    const QRect sel = overlay.run();

    if (parentWindow && wasVisible) {
        parentWindow->show();
        parentWindow->raise();
        parentWindow->activateWindow();
    }

    if (sel.width() < 2 || sel.height() < 2) {
        return {};
    }

    const QImage img = combined.copy(sel).toImage();
    if (img.isNull()) {
        return {};
    }

    const QString tempPath = makeScreenshotPath();
    if (!img.save(tempPath, "PNG")) {
        return { img, QString() };
    }
    const QFileInfo fi(tempPath);
    return { img, fi.canonicalFilePath() };
}

} // namespace pictureviewer

#endif // !__APPLE__
