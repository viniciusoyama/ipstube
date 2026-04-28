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

    // Single global tick counter — all panels advance together, but each
    // panel starts at a random digit. Per panel:
    //   shown = (startOffset[i] + globalTick) mod 10
    // Each panel settles on its target after enough cycles to wrap around to
    // it, so different panels finish at slightly different ticks.
    uint8_t globalTick = 0;
    uint8_t maxDoneTick = 0;
    uint8_t targetDigit[5];            // 0..9
    uint8_t startOffset[5];            // random initial digit shown
    bool digitSettled[5];              // once true, that panel won't be repushed
    bool firstFrame = true;            // gate the one-time dot/blank panel paint

    uint32_t dwellStartMs = 0;
};

#endif
