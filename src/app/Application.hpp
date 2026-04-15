#pragma once

#include <memory>

class QApplication;

namespace pictureviewer {

class MainWindow;

class Application
{
public:
    Application(int &argc, char **argv);
    ~Application();

    int run();

private:
    std::unique_ptr<QApplication> m_qtApplication;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace pictureviewer
