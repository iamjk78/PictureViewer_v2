#include "app/Application.hpp"
#include "app/MainWindow.hpp"

#include <QEvent>
#include <QFileOpenEvent>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QProxyStyle>
#include <QTimer>
#include <QDebug>

// On macOS 26 (Tahoe) the SF Symbol used for the QToolBarExtension (">>")
// button crashes inside NSImageSymbolRepProvider when QStyleSheetStyle is
// active. Override standardIcon so both extension-button variants return a
// plain drawn pixmap and never go through QAppleIconEngine.
namespace {
class ToolbarExtensionStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QIcon standardIcon(StandardPixmap sp,
                       const QStyleOption *opt = nullptr,
                       const QWidget *widget = nullptr) const override
    {
        if (sp == QStyle::SP_ToolBarHorizontalExtensionButton ||
            sp == QStyle::SP_ToolBarVerticalExtensionButton) {
            QPixmap pm(12, 12);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setPen(Qt::black);
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.drawText(pm.rect(), Qt::AlignCenter, ">>");
            return QIcon(pm);
        }
        return QProxyStyle::standardIcon(sp, opt, widget);
    }
};
} // namespace

namespace pictureviewer {

// PictureViewerApplication implementation
PictureViewerApplication::PictureViewerApplication(int &argc, char **argv)
    : QApplication(argc, argv)
{
    // Note: setApplicationName and setOrganizationName are called in Application::Application
}

void PictureViewerApplication::setMainWindow(MainWindow *window)
{
    m_mainWindow = window;
}

bool PictureViewerApplication::event(QEvent *event)
{
    // Handle file open events from macOS Finder
    if (event->type() == QEvent::FileOpen) {
        auto fileEvent = dynamic_cast<QFileOpenEvent *>(event);
        if (fileEvent) {
            const QString filePath = fileEvent->file();
            qDebug() << "FileOpen event:" << filePath << "- starting:" << m_isStarting;

            // During startup (right after launch), open in this instance
            if (m_isStarting) {
                qDebug() << "Opening in startup instance";
                m_mainWindow->openFile(filePath);
                return true;
            }

            // App already running, spawn new instance
            qDebug() << "App running - spawning new instance";
            QProcess::startDetached(qApp->applicationFilePath(), QStringList() << filePath);
            return true;
        }
    }

    return QApplication::event(event);
}


// Application implementation
Application::Application(int &argc, char **argv)
    : m_qtApplication(std::make_unique<PictureViewerApplication>(argc, argv))
{
    m_qtApplication->setApplicationName("PictureViewer");
    m_qtApplication->setApplicationVersion(QStringLiteral(PV_APP_VERSION));
    m_qtApplication->setOrganizationName("JiriKrejci");
    m_qtApplication->setOrganizationDomain("com.jk78");

    // Ochrana proti dekompresním bombám: malý soubor, který se rozbalí do
    // obřího rastru, dekodér odmítne. Globální limit pro všechna QImageReader
    // použití (ImageLoader, ThumbnailWorker, ImageView). 512 MB ≈ 128 Mpx
    // ARGB32 — pokryje i velká panoramata.
    QImageReader::setAllocationLimit(512);

    // Workaround for macOS 26 crash: NSImageSymbolRepProvider crashes when
    // QStyleSheetStyle requests the SF Symbol for the toolbar extension button.
    m_qtApplication->setStyle(new ToolbarExtensionStyle(m_qtApplication->style()));

    m_mainWindow = std::make_unique<MainWindow>();
    m_qtApplication->setMainWindow(m_mainWindow.get());
}

Application::~Application() = default;

int Application::run()
{
    // Handle command-line arguments (e.g., open image passed from command line)
    const QStringList args = m_qtApplication->arguments();
    if (args.size() > 1) {
        const QString filePath = args.at(1);
        qDebug() << "Opening file from command line:" << filePath;
        m_mainWindow->openFile(filePath);
    }

    m_mainWindow->show();
    // Mark that startup is complete - now FileOpen events should spawn new instances
    QTimer::singleShot(100, [this]() { m_qtApplication->setStarting(false); });
    return m_qtApplication->exec();
}

} // namespace pictureviewer
