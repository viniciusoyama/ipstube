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

char TextAnimation::tapeAt(const String& tape, int32_t idx) const {
    int32_t L = (int32_t)tape.length();
    if (L == 0) return ' ';
    int32_t m = ((idx % L) + L) % L;
    return tape.charAt(m);
}

void TextAnimation::invalidate() { dirty = true; }

bool TextAnimation::loop() {
    String text = IPSClock::getTextContent();
    bool fixed = IPSClock::getTextFixed();
    String fg = IPSClock::getTextFgColor();
    String bg = IPSClock::getTextBgColor();

    if (text != lastText || fixed != lastFixed || fg != lastFg || bg != lastBg) {
        lastText = text;
        lastFixed = fixed;
        lastFg = fg;
        lastBg = bg;
        dirty = true;
        tick = 0;
    }

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

    String tape = text;
    if (!fixed) {
        for (uint8_t k = 0; k < padding; k++) tape += ' ';
    }

    StaticSprite& sprite = tfts.getSprite();
    int16_t cx = sprite.width() / 2;
    int16_t cy = sprite.height() / 2;

    for (uint8_t charIdx = 0; charIdx < NUM_DIGITS; charIdx++) {
        char ch;
        if (fixed) {
            ch = (charIdx < text.length()) ? text.charAt(charIdx) : ' ';
        } else {
            if (tape.length() == 0) {
                ch = ' ';
            } else {
                // Right-to-left marquee: letters enter from the rightmost panel
                // and travel left, so the text reads in natural order across panels.
                int32_t fromRight = (int32_t)(NUM_DIGITS - 1 - charIdx);
                ch = tapeAt(tape, (int32_t)tick - fromRight);
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
