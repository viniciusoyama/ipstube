#ifndef _TEXT_ANIMATION_H
#define _TEXT_ANIMATION_H

#include <Arduino.h>
#include <TFT_eSPI.h>

class TFTs;

class TextAnimation {
public:
    TextAnimation();

    bool loop();
    void animate(TFTs& tfts);
    void invalidate();

    // Cycle limiting: caller resets the count when entering text mode.
    // animate() decrements once per completed slide pass (last text char
    // leaving the leftmost panel). isFinished() flips true when remaining
    // hits 0 so the caller can switch back to clock mode.
    void resetCycleCount();
    bool isFinished() const { return finished; }

private:
    static uint16_t parseHexColor(const String& s);
    char tapeAt(const String& tape, int32_t idx) const;

    uint32_t lastDrawMs = 0;
    uint32_t tick = 0;
    bool dirty = true;
    String lastText;
    bool lastFixed = true;
    String lastFg;
    String lastBg;

    int32_t cyclesRemaining = -1;     // -1 = unlimited
    int32_t prevLeftmostIdx = -1;     // tape index shown on leftmost last frame; -1 = uninitialized
    bool finished = false;
};

#endif
