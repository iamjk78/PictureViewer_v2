#pragma once

#include <QObject>

class QTimer;

namespace pictureviewer {

class SlideshowController : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultIntervalMs = 3000;
    static constexpr int MinimumIntervalMs = 500;

    explicit SlideshowController(QObject *parent = nullptr);

    bool isRunning() const;
    int intervalMs() const;

public slots:
    void setInterval(int milliseconds);
    void start();
    void stop();
    void toggle();

signals:
    void nextImageRequested();

private:
    QTimer *m_timer;
    int m_intervalMs;
};

} // namespace pictureviewer
