#include "DivergenceAnimation.h"
#include "TFTs.h"
#include "IPSClock.h"

DivergenceAnimation::DivergenceAnimation() {
    for (uint8_t i = 0; i < NUM_DIGITS; i++) {
        targetDigit[i] = 0;
        panelTicks[i] = 0;
    }
}

void DivergenceAnimation::restart() {
    String number = IPSClock::getDivergenceNumber();
    char padded[6] = { '0', '0', '0', '0', '0', 0 };
    for (uint8_t i = 0; i < 5 && i < number.length(); i++) {
        char c = number.charAt(i);
        if (c >= '0' && c <= '9') padded[i] = c;
    }

    for (uint8_t i = 0; i < 5; i++) {
        targetDigit[i] = (uint8_t)(padded[i] - '0');
        panelTicks[i] = 0;
    }
    targetDigit[5] = 0;
    panelTicks[5] = 0;

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

    bool allSettled = true;
    for (uint8_t i = 0; i < 5; i++) {
        uint16_t totalTicks = (uint16_t)cycles * 10 + targetDigit[i];
        if (panelTicks[i] < totalTicks) {
            allSettled = false;
        }
    }

    for (uint8_t i = 0; i < NUM_DIGITS; i++) {
        char digitChar = ' ';
        bool drawDot = false;

        if (i == 5) {
            digitChar = ' ';
        } else {
            uint16_t totalTicks = (uint16_t)cycles * 10 + targetDigit[i];
            uint8_t shown = (panelTicks[i] >= totalTicks)
                ? targetDigit[i]
                : (uint8_t)(panelTicks[i] % 10);
            digitChar = (char)('0' + shown);
            if (i == 0) drawDot = true;
        }

        tfts.drawDivergenceDigit(i, digitChar, drawDot);
    }

    for (uint8_t i = 0; i < 5; i++) {
        uint16_t totalTicks = (uint16_t)cycles * 10 + targetDigit[i];
        if (panelTicks[i] < totalTicks) panelTicks[i]++;
    }

    if (allSettled) {
        phase = DWELLING;
        dwellStartMs = millis();
    }
}
