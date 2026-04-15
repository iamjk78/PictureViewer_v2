#include "app/Application.hpp"
#include "app/MainWindow.hpp"

#include <QEvent>
#include <QFileOpenEvent>
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
        if (fileEvent && m_mainWindow) {
            const QString filePath = fileEvent->file();
            qDebug() << "File open event received:" << filePath;

            // Tell MainWindow to open this file
            // The file will be loaded and displayed
            m_mainWindow->openFile(filePath);

            m_mainWindow->show();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();

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
    m_qtApplication->setApplicationVersion("0.1.0");
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
    }

    m_mainWindow->show();
    return m_qtApplication->exec();
}

} // namespace pictureviewer
