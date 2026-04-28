#ifndef _DIVERGENCE_ANIMATION_H
#define _DIVERGENCE_ANIMATION_H

#include <Arduino.h>
#include "GLOBAL_DEFINES.h"

class TFTs;

class DivergenceAnimation {
public:
    DivergenceAnimation();

    // Reset state and re-read config. Called on activation (last-click-wins).
    void restart();

    // Returns true if a redraw is needed (interval elapsed or dirty).
    bool loop();

    // Renders one frame onto the panels. Caller has the TFT mutex.
    void animate(TFTs& tfts);

    // True when the dwell phase has ended; caller should switch back to TIME.
    bool isFinished() const { return finished; }

private:
    enum Phase { ROLLING, DWELLING, DONE };

    Phase phase = DONE;
    bool finished = false;
    uint32_t lastDrawMs = 0;
    bool dirty = true;

    // Per-panel rolling state. Panels 0..4 (reading order) take the 5 input
    // digits; panel 5 stays blank.
    uint8_t targetDigit[NUM_DIGITS];   // 0..9; index 5 unused
    uint16_t panelTicks[NUM_DIGITS];   // ticks consumed by panel; once it
                                       // reaches N*10 + targetDigit the panel
                                       // is settled.

    uint32_t dwellStartMs = 0;
};

#endif
