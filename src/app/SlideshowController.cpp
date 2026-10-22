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
    if (m_timer->isActive()) {
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
    // Voláno jen z UI vlákna (QTimer je s ním svázaný) — zámek by nic nechránil.
    if (m_timer->isActive()) {
        m_timer->stop();
    } else {
        m_timer->start(m_intervalMs);
    }
}

} // namespace pictureviewer
