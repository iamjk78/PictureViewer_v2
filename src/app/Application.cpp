#include "app/Application.hpp"

#include "app/MainWindow.hpp"

#include <QApplication>

namespace pictureviewer {

Application::Application(int &argc, char **argv)
    : m_qtApplication(std::make_unique<QApplication>(argc, argv))
{
    m_qtApplication->setApplicationName("PictureViewer");
    m_qtApplication->setOrganizationName("JiriKrejci");
    m_mainWindow = std::make_unique<MainWindow>();
}

Application::~Application() = default;

int Application::run()
{
    m_mainWindow->show();
    return m_qtApplication->exec();
}

} // namespace pictureviewer
