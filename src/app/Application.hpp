#pragma once

#include <memory>
#include <QApplication>

class QFileOpenEvent;

namespace pictureviewer {

class MainWindow;

// Custom QApplication that handles file open events (macOS, drag-drop)
class PictureViewerApplication : public QApplication
{
    Q_OBJECT

public:
    explicit PictureViewerApplication(int &argc, char **argv);

    void setMainWindow(MainWindow *window);
    void setLaunchedWithFile(bool launched) { m_launchedWithFile = launched; }

protected:
    // Handle file open events (macOS: double-click, Open With, drag-drop)
    bool event(QEvent *event) override;

private:
    MainWindow *m_mainWindow = nullptr;
    bool m_launchedWithFile = false;
};


class Application
{
public:
    Application(int &argc, char **argv);
    ~Application();

    int run();

private:
    std::unique_ptr<PictureViewerApplication> m_qtApplication;
    std::unique_ptr<MainWindow> m_mainWindow;
};

} // namespace pictureviewer
