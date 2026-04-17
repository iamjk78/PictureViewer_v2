#include "app/Application.hpp"
#include "app/MainWindow.hpp"

#include <QEvent>
#include <QFileOpenEvent>
#include <QProcess>
#include <QTimer>
#include <QDebug>

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
    m_qtApplication->setApplicationVersion("0.4");
    m_qtApplication->setOrganizationName("JiriKrejci");
    m_qtApplication->setOrganizationDomain("com.jk78");

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
