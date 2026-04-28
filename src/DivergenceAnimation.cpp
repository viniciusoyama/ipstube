#include "DivergenceAnimation.h"
#include "TFTs.h"
#include "IPSClock.h"

// Reading-order mapping: index 0 is leftmost panel, index 5 is rightmost.
// Works for both panel orderings (HARDWARE_PunkCyber_CLOCK and the rest).
static const uint8_t kLeftToRightPanel[NUM_DIGITS] = {
    HOURS_TENS, HOURS_ONES, MINUTES_TENS, MINUTES_ONES, SECONDS_TENS, SECONDS_ONES
};

DivergenceAnimation::DivergenceAnimation() {
    for (uint8_t i = 0; i < 5; i++) {
        targetDigit[i] = 0;
        startOffset[i] = 0;
        digitSettled[i] = false;
    }
}

void DivergenceAnimation::restart() {
    String number = IPSClock::getDivergenceNumber();
    char padded[6] = { '0', '0', '0', '0', '0', 0 };
    for (uint8_t i = 0; i < 5 && i < number.length(); i++) {
        char c = number.charAt(i);
        if (c >= '0' && c <= '9') padded[i] = c;
    }

    uint8_t cycles = IPSClock::getDivergenceCycles().value;
    if (cycles > 24) cycles = 24;

    for (uint8_t i = 0; i < 5; i++) {
        targetDigit[i] = (uint8_t)(padded[i] - '0');
        digitSettled[i] = false;
    }

    // Cache all 10 digits at third resolution (~7 KB each, ~72 KB total).
    // Allows every panel to roll through the full 0..9 sequence without
    // touching the disk. If the build fails (heap), useCachedDigits stays
    // false and panels skip the rolling phase straight to settled BMPs.
    uint8_t allDigits[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    extern TFTs* tfts;
    useCachedDigits = tfts->buildDivergenceDigitCache(allDigits, 10);

    // Per-panel random start position 0..9 and done-tick.
    maxDoneTick = 0;
    for (uint8_t i = 0; i < 5; i++) {
        startOffset[i] = (uint8_t)random(10);
        // Earliest tick at which (startOffset + tick) % 10 == targetDigit[i].
        uint8_t toTarget = (uint8_t)(((int)targetDigit[i] - (int)startOffset[i] + 10) % 10);
        uint8_t doneTick = (uint8_t)(cycles * 10 + toTarget);
        if (doneTick > maxDoneTick) maxDoneTick = doneTick;
    }

    globalTick = 0;
    firstFrame = true;
    phase = ROLLING;
    finished = false;
    dirty = true;
    lastDrawMs = 0;
    dwellStartMs = 0;
}

bool DivergenceAnimation::loop() {
    if (phase == DONE) return false;

    uint32_t now = millis();

    if (phase == DWELLING) {
        uint32_t dwellMs = (uint32_t)IPSClock::getDivergenceDwellSeconds().value * 1000;
        if (now - dwellStartMs >= dwellMs) {
            phase = DONE;
            finished = true;
        }
        return false;
    }

    uint32_t interval = (uint32_t)IPSClock::getDivergenceRollInterval().value;
    if (interval < 20) interval = 20;
    if (dirty || (now - lastDrawMs) >= interval) {
        dirty = false;
        lastDrawMs = now;
        return true;
    }
    return false;
}

void DivergenceAnimation::animate(TFTs& tfts) {
    if (phase != ROLLING) return;

    uint8_t cycles = IPSClock::getDivergenceCycles().value;
    if (cycles > 24) cycles = 24;

    // Layout (reading order, left to right):
    //   panel charIdx 0 -> digit 0  (e.g. '1' from "12345")
    //   panel charIdx 1 -> dedicated dot panel
    //   panel charIdx 2 -> digit 1  ('2')
    //   panel charIdx 3 -> digit 2  ('3')
    //   panel charIdx 4 -> digit 3  ('4')
    //   panel charIdx 5 -> digit 4  ('5')

    // First frame after activation: paint the dot panel and the blank panel
    // once. They don't change after that, so subsequent frames skip them.
    if (firstFrame) {
        tfts.drawDivergenceDigit(kLeftToRightPanel[1], '.', true);
        tfts.drawDivergenceDigit(kLeftToRightPanel[5], ' ', true);
        firstFrame = false;
    }

    // Per-frame: push only the digit panels that need it.
    //   - Rolling: scaled cached BMP, cycling 0..9 from a random offset.
    //   - Just-settled this frame: full-quality BMP from the active clock
    //     face (one-time).
    //   - Already-settled: skip — no SPI work.
    //   - No cache (cache build failed): settle immediately, no rolling.
    static const uint8_t digitCharIdx[5] = { 0, 2, 3, 4, 5 };
    for (uint8_t d = 0; d < 5; d++) {
        if (digitSettled[d]) continue;

        uint8_t physicalPanel = kLeftToRightPanel[digitCharIdx[d]];

        if (!useCachedDigits) {
            tfts.drawDivergenceDigit(physicalPanel, (char)('0' + targetDigit[d]), false);
            digitSettled[d] = true;
            continue;
        }

        uint8_t toTarget = (uint8_t)(((int)targetDigit[d] - (int)startOffset[d] + 10) % 10);
        uint8_t doneTick = (uint8_t)(cycles * 10 + toTarget);

        if (globalTick >= doneTick) {
            // Just settled — switch to full-quality BMP for the final look.
            tfts.drawDivergenceDigit(physicalPanel, (char)('0' + targetDigit[d]), false);
            digitSettled[d] = true;
        } else {
            // Rolling — push the cached third-res digit. The cache covers
            // every digit 0..9, so this always succeeds.
            uint8_t shown = (uint8_t)((startOffset[d] + globalTick) % 10);
            tfts.pushCachedDivergenceDigit(physicalPanel, shown);
        }
    }

    // The frame *at* globalTick == maxDoneTick is when the last-settling
    // panel renders its BMP. Only after that frame should we transition.
    if (globalTick >= maxDoneTick) {
        phase = DWELLING;
        dwellStartMs = millis();
    } else if (globalTick < 255) {
        globalTick++;
    }
}
