#include "depch.h"
#include "Core/Timeline.h"

#include <algorithm>
#include <cmath>

namespace Fufu
{

// ── Playback Control ─────────────────────────────────────────────────────────

void Timeline::play()
{
    if (m_Playing) return;
    m_Playing = true;
    m_Ended   = false;
    onPlay.emit();
}

void Timeline::pause()
{
    if (!m_Playing) return;
    m_Playing = false;
    onPause.emit();
}

void Timeline::stop()
{
    m_Playing = false;
    m_Ended   = false;
    seek(0.0);
    onStop.emit();
}

void Timeline::seek(double t)
{
    if (m_Duration >= 0.0)
        t = std::clamp(t, 0.0, m_Duration);
    m_Time  = t;
    m_Ended = false;
}

void Timeline::setRate(float rate)
{
    m_Rate = rate;
}

void Timeline::setLoop(bool loop, double duration)
{
    m_Loop     = loop;
    m_Duration = duration;
}

// ── tick ──────────────────────────────────────────────────────────────────────

void Timeline::tick(double dt)
{
    if (!m_Playing) return;

    double scaled = dt * static_cast<double>(m_Rate);
    m_Time += scaled;

    // Infinite case — direct tick
    if (m_Duration < 0.0)
    {
        onTick.emit(m_Time, scaled);
        return;
    }

    // Finite duration case
    if (m_Rate >= 0.f)
    {
        // Advancing towards the end
        if (m_Time >= m_Duration)
        {
            if (m_Loop)
            {
                m_Time = std::fmod(m_Time, m_Duration);
                onTick.emit(m_Time, scaled);
                onLoopEnd.emit();
            }
            else
            {
                m_Time    = m_Duration;
                m_Playing = false;
                onTick.emit(m_Time, scaled);
                if (!m_Ended)
                {
                    m_Ended = true;
                    onEnd.emit();
                }
            }
        }
        else
        {
            onTick.emit(m_Time, scaled);
        }
    }
    else
    {
        // Reverse playback
        if (m_Time <= 0.0)
        {
            if (m_Loop)
            {
                m_Time = m_Duration + std::fmod(m_Time, m_Duration);
                onTick.emit(m_Time, scaled);
                onLoopEnd.emit();
            }
            else
            {
                m_Time    = 0.0;
                m_Playing = false;
                onTick.emit(m_Time, scaled);
                if (!m_Ended)
                {
                    m_Ended = true;
                    onEnd.emit();
                }
            }
        }
        else
        {
            onTick.emit(m_Time, scaled);
        }
    }
}

// ── State ─────────────────────────────────────────────────────────────────────

bool Timeline::hasEnded() const
{
    return m_Ended;
}

} // namespace Fufu
