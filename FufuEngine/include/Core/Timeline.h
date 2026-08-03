#pragma once

#include "Core/Signal.h"

namespace Fufu
{

// Generic clock without domain semantics.
//
// The core does not know whether this Timeline represents a game loop,
// an animation clip, a musical tempo, or a simulation step — it is the
// module above that gives it meaning.
//
// Integration in the main loop:
//   timeline.tick(deltaTime);   // called every frame by the layer
//
// Example — Game module:
//   m_Timeline.setLoop(true);
//   m_Timeline.play();
//   m_Timeline.onTick.connect([](double t, double dt) { updateSystems(dt); });
//
// Example — Motion module:
//   m_Timeline.setLoop(true, clipDuration);
//   m_Timeline.setRate(0.5f);   // slow motion
//   m_Timeline.onTick.connect([](double t, double) { evaluateCurves(t); });
//   m_Timeline.onLoopEnd.connect([]{ advanceToNextClip(); });
class Timeline
{
public:
    // ── Playback control ─────────────────────────────────────────────────────
    void play();
    void pause();
    void stop();                      // pause + seek(0)
    void seek(double t);              // jump to instant t (in seconds)
    void setRate(float rate);         // 1=normal, 0=pause, -1=reverse, 2=double
    void setLoop(bool loop, double duration = -1.0);

    // ── Called every frame by the owning Layer or module ─────────────────────
    // dt is the real delta-time (seconds). The Timeline scales it by m_Rate.
    void tick(double dt);

    // ── State ─────────────────────────────────────────────────────────────────
    double time()      const { return m_Time; }
    double duration()  const { return m_Duration; }
    float  rate()      const { return m_Rate; }
    bool   isPlaying() const { return m_Playing; }
    bool   isLooping() const { return m_Loop; }
    bool   hasEnded()  const;   // true if duration is finite and not looping

    // ── Signals ───────────────────────────────────────────────────────────────
    Signal<double /*time*/, double /*dt_scaled*/> onTick;
    Signal<>                                       onPlay;
    Signal<>                                       onPause;
    Signal<>                                       onStop;
    Signal<>                                       onLoopEnd;   // emitted on each loop iteration
    Signal<>                                       onEnd;       // emitted once at the end

private:
    double m_Time     = 0.0;
    double m_Duration = -1.0;   // -1 = infinite
    float  m_Rate     = 1.0f;
    bool   m_Playing  = false;
    bool   m_Loop     = false;
    bool   m_Ended    = false;  // remembers that onEnd has already been emitted
};

} // namespace Fufu
