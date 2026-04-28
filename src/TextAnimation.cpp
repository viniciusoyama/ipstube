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
    int32_t tapeLen = textLen + (int32_t)padding;

    // Marquee model: the visible window slides over an infinite virtual strip
    //   (NUM_DIGITS leading blanks) + tape + tape + tape + ...
    // where tape = text + padding spaces.
    //
    // Panel charIdx (0=leftmost, 5=rightmost) at tick T shows strip[T + charIdx].
    // The leading blanks make activation start clean: at T=0 all panels are
    // blank, at T=1 the rightmost panel just received the first text char,
    // etc. After T grows past the leading prefix the marquee wraps the tape
    // continuously and subsequent text instances may overlap the previous on
    // screen (that's the user's intent — controlled via padding).
    auto stripAt = [&](int32_t i) -> char {
        if (i < (int32_t)NUM_DIGITS) return ' ';        // leading lead-in
        if (textLen == 0 || tapeLen == 0) return ' ';
        int32_t tapeIdx = (((i - (int32_t)NUM_DIGITS) % tapeLen) + tapeLen) % tapeLen;
        if (tapeIdx < textLen) return text.charAt(tapeIdx);
        return ' ';                                     // padding region
    };

    // Returns the tape position for the panel at charIdx, or -1 if the panel
    // is currently in the lead-in or in the padding region.
    auto stripPosAt = [&](int32_t charIdx) -> int32_t {
        int32_t i = (int32_t)tick + charIdx;
        if (i < (int32_t)NUM_DIGITS) return -1;
        if (tapeLen <= 0) return -1;
        int32_t tapeIdx = (((i - (int32_t)NUM_DIGITS) % tapeLen) + tapeLen) % tapeLen;
        return (tapeIdx < textLen) ? tapeIdx : -1;
    };

    // Cycle counting (slide mode only). A cycle completes when the leftmost
    // panel transitions from showing the last text character (position
    // textLen - 1) to showing anything else. We compare positions, not chars,
    // so repeated characters in the text don't cause false counts.
    int32_t currLeftmostStripPos = (!fixed) ? stripPosAt(0) : -1;
    if (!fixed && textLen > 0 && cyclesRemaining > 0) {
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
        } else {
            ch = stripAt((int32_t)tick + (int32_t)charIdx);
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
