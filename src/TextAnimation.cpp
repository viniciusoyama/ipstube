#include "TextAnimation.h"
#include "TFTs.h"
#include "IPSClock.h"
#include "GLOBAL_DEFINES.h"

// Reading-order mapping: index 0 is leftmost panel (hours-tens),
// index 5 is rightmost (seconds-ones). Works for both panel orderings.
static const uint8_t kLeftToRightPanel[NUM_DIGITS] = {
    HOURS_TENS, HOURS_ONES, MINUTES_TENS, MINUTES_ONES, SECONDS_TENS, SECONDS_ONES
};

TextAnimation::TextAnimation() {}

uint16_t TextAnimation::parseHexColor(const String& s) {
    if (s.length() != 7 || s.charAt(0) != '#') return 0x0000;
    long v = strtol(s.c_str() + 1, nullptr, 16);
    uint8_t r = (v >> 16) & 0xFF;
    uint8_t g = (v >> 8) & 0xFF;
    uint8_t b = v & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void TextAnimation::invalidate() {
    dirty = true;
    tick = 0;
    prevLeftmostIdx = -1;
}

void TextAnimation::resetCycleCount() {
    if (IPSClock::getTextCycleLimitEnabled() && IPSClock::getTextCycleLimit().value > 0) {
        cyclesRemaining = IPSClock::getTextCycleLimit().value;
    } else {
        cyclesRemaining = -1;
    }
    finished = false;
    prevLeftmostIdx = -1;
}

bool TextAnimation::loop() {
    bool fixed = IPSClock::getTextFixed();

    if (fixed) {
        if (dirty) {
            dirty = false;
            return true;
        }
        return false;
    }

    uint32_t interval = (uint32_t)IPSClock::getTextInterval().value;
    if (interval < 50) interval = 50;
    uint32_t now = millis();
    if (dirty || (now - lastDrawMs) >= interval) {
        if (!dirty) tick++;
        dirty = false;
        lastDrawMs = now;
        return true;
    }
    return false;
}

void TextAnimation::animate(TFTs& tfts) {
    String text = IPSClock::getTextContent();
    bool fixed = IPSClock::getTextFixed();
    uint8_t padding = IPSClock::getTextPadding();
    uint16_t fg565 = parseHexColor(IPSClock::getTextFgColor());
    uint16_t bg565 = parseHexColor(IPSClock::getTextBgColor());

    int32_t textLen = (int32_t)text.length();

    // Cycle phase model (slide mode):
    //   tick = 0           -> initial activation frame, all panels blank
    //   tick = 1           -> first text char enters rightmost panel
    //   phase 0..scrollLen-1 -> text scrolling across (last char on leftmost at phase = scrollLen-1)
    //   phase scrollLen..cyclePeriod-1 -> trailing all-blank padding
    //
    // panel charIdx (0=leftmost..5=rightmost) shows text[stripPos] when
    //   stripPos = phase - (NUM_DIGITS - 1 - charIdx)  is in [0, textLen).
    // Otherwise the panel is blank.
    int32_t scrollLen = (textLen > 0) ? (textLen + (int32_t)NUM_DIGITS - 1) : 0;
    int32_t cyclePeriod = scrollLen + (int32_t)padding;
    int32_t phase = -1;
    if (!fixed && textLen > 0 && cyclePeriod > 0 && tick > 0) {
        phase = (int32_t)((tick - 1) % (uint32_t)cyclePeriod);
    }

    // Cycle counting (slide mode only). Compares positions, not characters,
    // so repeated characters in the text don't trigger false counts.
    int32_t currLeftmostStripPos = (phase >= 0) ? (phase - (int32_t)(NUM_DIGITS - 1)) : -1;
    if (!fixed && textLen > 0 && cyclesRemaining > 0) {
        // A cycle completes when the leftmost panel transitions from
        // showing the last text character (stripPos == textLen-1) to
        // showing anything else.
        if (prevLeftmostIdx == textLen - 1 && currLeftmostStripPos != textLen - 1) {
            cyclesRemaining--;
            if (cyclesRemaining == 0) {
                finished = true;
            }
        }
    }
    prevLeftmostIdx = currLeftmostStripPos;

    StaticSprite& sprite = tfts.getSprite();
    int16_t cx = sprite.width() / 2;
    int16_t cy = sprite.height() / 2;

    for (uint8_t charIdx = 0; charIdx < NUM_DIGITS; charIdx++) {
        char ch = ' ';
        if (fixed) {
            if (charIdx < (uint8_t)textLen) ch = text.charAt(charIdx);
        } else if (textLen > 0 && phase >= 0) {
            int32_t stripPos = phase - (int32_t)(NUM_DIGITS - 1 - charIdx);
            if (stripPos >= 0 && stripPos < textLen) {
                ch = text.charAt(stripPos);
            }
        }

        uint8_t panel = kLeftToRightPanel[charIdx];
        tfts.chip_select.setDigit(panel);

        sprite.fillSprite(bg565);
        if (ch != ' ') {
            sprite.setTextDatum(MC_DATUM);
            sprite.setTextColor(fg565, bg565);
            sprite.setTextFont(4);
            sprite.setTextSize(3);
            char buf[2] = { ch, 0 };
            sprite.drawString(buf, cx, cy);
        }
        sprite.pushSprite(0, 0);
    }

    // Restore sprite state so other faces (weather, status, meter) don't inherit
    // our font size and datum.
    sprite.setTextSize(1);
    sprite.setTextDatum(TL_DATUM);
}
