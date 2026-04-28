# Notes for LLM agents working on this codebase

## Feature documentation

Significant features have a dedicated reference under `features/`. Read the
relevant doc before touching the feature, and **update the doc in the same
commit if you change behaviour, config, layout, gotchas, or architecture**.

Existing feature docs:

- [Divergence Meter](features/divergence-meter.md) — `time_or_date = 5`,
  the rolling 5-digit meter face. Covers the third-resolution digit cache,
  the random per-panel start offset, the `LoadImageIntoBuffer` upstream
  bug, and the BSS / heap accounting.

## Build & flash quickrefs

- `pio run -e ipstube` — compile firmware (most common target).
- `pio run -e ipstube --target upload` — flash firmware over USB.
- `cd web && npx gulp && cd .. && pio run -e ipstube --target uploadfs` —
  rebuild the web bundle and flash the LittleFS image.
- `cd web && node server.js` — local mock server for UI work without
  flashing. State and dispatcher live in `web/server.js`; mirror real
  protocol changes there.

## DRAM is the constrained resource

`dram0_0_seg` is link-time fixed and tight. New globals end up in BSS by
default — every `static T x;` ConfigItem getter costs ~24–44 bytes BSS.
For new ConfigItems on this codebase, prefer the heap pattern:

```cpp
static IntConfigItem& getMyThing() {
    static IntConfigItem* p = new IntConfigItem("my_thing", 0);
    return *p;
}
```

Saves ~12–24 bytes BSS per item; the body lives on the heap where there's
plenty of room. See `IPSClock.h` for the existing examples (text and
divergence sections).

## Heap budget on `esp32dev` is small

The board (no PSRAM) has ~100–150 KB free heap at runtime after WiFi,
MQTT, web server and the 64 KB sprite buffer. Anything that allocates
>~80 KB is risky. Half-resolution caching is the standard pattern for
the divergence meter; full-quality caching of multiple panel-sized BMPs
will not fit.
