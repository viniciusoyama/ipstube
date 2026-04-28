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

    // Single global tick counter. Each panel rolls 0,1,2,...,9,0,1,...
    // starting from a random startOffset 0..9, in incremental order.
    //   shown = (startOffset[d] + globalTick) mod 10
    // Panel d settles on its target after enough cycles to wrap to it:
    //   doneTick[d] = cycles*10 + (targetDigit[d] - startOffset[d] + 10) % 10
    uint8_t globalTick = 0;
    uint8_t maxDoneTick = 0;
    uint8_t targetDigit[5];            // 0..9
    uint8_t startOffset[5];            // 0..9, panel start position in 0..9 cycle
    bool digitSettled[5];              // once true, that panel won't be repushed
    bool firstFrame = true;            // gate the one-time dot/blank panel paint
    bool useCachedDigits = false;      // true iff TFTs has a working scaled-BMP
                                       // cache covering all 10 digits

    uint32_t dwellStartMs = 0;
};

#endif
