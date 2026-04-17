#include "app/Application.hpp"
#include "app/MainWindow.hpp"

#include <QEvent>
#include <QFileOpenEvent>
#include <QProcess>
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
    // Only spawn a new instance if this instance wasn't launched with a file
    if (event->type() == QEvent::FileOpen) {
        auto fileEvent = dynamic_cast<QFileOpenEvent *>(event);
        if (fileEvent) {
            const QString filePath = fileEvent->file();

            // If this instance was launched with a file, just open it here
            if (m_launchedWithFile) {
                qDebug() << "File open event ignored - instance launched with file:" << filePath;
                return true;
            }

            qDebug() << "File open event received - spawning new instance:" << filePath;

            // Spawn new instance with file path as command-line argument
            // The new instance will receive the file path via args in Application::run()
            QProcess::startDetached(qApp->applicationFilePath(), QStringList() << filePath);

            return true;  // Event handled
        }
    }

    return QApplication::event(event);
}


// Application implementation
Application::Application(int &argc, char **argv)
    : m_qtApplication(std::make_unique<PictureViewerApplication>(argc, argv))
{
    m_qtApplication->setApplicationName("PictureViewer");
    m_qtApplication->setApplicationVersion("0.2");
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
        // Skip the first argument which is the program name
        const QString filePath = args.at(1);
        qDebug() << "Opening file from command line:" << filePath;
        m_mainWindow->openFile(filePath);
        // Mark that this instance was launched with a file
        // so it won't spawn a new instance on FileOpen events
        m_qtApplication->setLaunchedWithFile(true);
    }

    m_mainWindow->show();
    return m_qtApplication->exec();
}

} // namespace pictureviewer
