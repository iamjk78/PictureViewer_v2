#include "app/SlideshowController.hpp"

#include <QTimer>

#include <algorithm>

namespace pictureviewer {

SlideshowController::SlideshowController(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_intervalMs(DefaultIntervalMs)
{
    connect(m_timer, &QTimer::timeout, this, &SlideshowController::nextImageRequested);
}

bool SlideshowController::isRunning() const
{
    return m_timer->isActive();
}

int SlideshowController::intervalMs() const
{
    return m_intervalMs;
}

void SlideshowController::setInterval(int milliseconds)
{
    m_intervalMs = std::max(milliseconds, MinimumIntervalMs);
    if (isRunning()) {
        m_timer->start(m_intervalMs);
    }
}

void SlideshowController::start()
{
    m_timer->start(m_intervalMs);
}

void SlideshowController::stop()
{
    m_timer->stop();
}

void SlideshowController::toggle()
{
    if (isRunning()) {
        stop();
        return;
    }

    start();
}

} // namespace pictureviewer
