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
    // panel has a random offset into the 5-element target cycle. Per panel:
    //   shown = targetDigit[(startOffset[i] + globalTick) mod 5]
    // Panel d settles when (startOffset[d] + tick) mod 5 == d, after at
    // least `cycles` full passes through the target sequence.
    uint8_t globalTick = 0;
    uint8_t maxDoneTick = 0;
    uint8_t targetDigit[5];            // 0..9
    uint8_t startOffset[5];            // 0..4, position within the 5-cycle
    bool digitSettled[5];              // once true, that panel won't be repushed
    bool firstFrame = true;            // gate the one-time dot/blank panel paint
    bool useCachedDigits = false;      // true iff TFTs has a working scaled-BMP
                                       // cache for the target digits

    uint32_t dwellStartMs = 0;

    // Builds the unique-digit list (no duplicates) and writes it into out[].
    // Returns the count.
    uint8_t collectUniqueTargets(uint8_t out[5]) const;
};

#endif
