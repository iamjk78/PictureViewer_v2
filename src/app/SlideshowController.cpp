#include "app/SlideshowController.hpp"

#include <QMutexLocker>
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
    QMutexLocker locker(&m_mutex);
    return m_timer->isActive();
}

int SlideshowController::intervalMs() const
{
    QMutexLocker locker(&m_mutex);
    return m_intervalMs;
}

void SlideshowController::setInterval(int milliseconds)
{
    QMutexLocker locker(&m_mutex);
    m_intervalMs = std::max(milliseconds, MinimumIntervalMs);
    if (m_timer->isActive()) {
        m_timer->start(m_intervalMs);
    }
}

void SlideshowController::start()
{
    QMutexLocker locker(&m_mutex);
    m_timer->start(m_intervalMs);
}

void SlideshowController::stop()
{
    QMutexLocker locker(&m_mutex);
    m_timer->stop();
}

void SlideshowController::toggle()
{
    // Celá sekvence pod jedním zámkem — jinak by dvě souběžná toggle()
    // mohla obě vidět stejný výchozí stav a skončit v nekonzistenci.
    QMutexLocker locker(&m_mutex);
    if (m_timer->isActive()) {
        m_timer->stop();
    } else {
        m_timer->start(m_intervalMs);
    }
}

} // namespace pictureviewer
