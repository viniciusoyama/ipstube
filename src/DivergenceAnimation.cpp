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

    maxDoneTick = 0;
    for (uint8_t i = 0; i < 5; i++) {
        targetDigit[i] = (uint8_t)(padded[i] - '0');
        startOffset[i] = (uint8_t)random(10);
        digitSettled[i] = false;
        // Earliest tick at which (startOffset + tick) % 10 == target
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
    //   - Rolling: render with font (fast) every frame.
    //   - Just-settled this frame: render with the BMP from the active clock
    //     face (one-time).
    //   - Already-settled: skip — no SPI work, leaves the previous BMP push
    //     intact on that panel.
    static const uint8_t digitCharIdx[5] = { 0, 2, 3, 4, 5 };
    for (uint8_t d = 0; d < 5; d++) {
        if (digitSettled[d]) continue;

        uint8_t toTarget = (uint8_t)(((int)targetDigit[d] - (int)startOffset[d] + 10) % 10);
        uint8_t doneTick = (uint8_t)(cycles * 10 + toTarget);
        uint8_t physicalPanel = kLeftToRightPanel[digitCharIdx[d]];

        if (globalTick >= doneTick) {
            // Just settled — switch to clock-face BMP for the final look.
            tfts.drawDivergenceDigit(physicalPanel, (char)('0' + targetDigit[d]), false);
            digitSettled[d] = true;
        } else {
            // Still rolling — fast font render.
            uint8_t shown = (uint8_t)((startOffset[d] + globalTick) % 10);
            tfts.drawDivergenceDigit(physicalPanel, (char)('0' + shown), true);
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
