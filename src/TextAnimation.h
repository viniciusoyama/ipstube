#ifndef _TEXT_ANIMATION_H
#define _TEXT_ANIMATION_H

#include <Arduino.h>
#include <TFT_eSPI.h>

class TFTs;

class TextAnimation {
public:
    TextAnimation();

    bool loop();
    void animate(TFTs& tfts);
    void invalidate();

private:
    static uint16_t parseHexColor(const String& s);
    char tapeAt(const String& tape, int32_t idx) const;

    uint32_t lastDrawMs = 0;
    uint32_t tick = 0;
    bool dirty = true;
    String lastText;
    bool lastFixed = true;
    String lastFg;
    String lastBg;
};

#endif
