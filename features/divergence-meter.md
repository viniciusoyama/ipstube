# Divergence Meter Feature

> **Future agent: if your change touches any code, config, web UI, or
> behaviour described below, update this document in the same commit.**

## What it does

A one-shot face that displays a 5-digit number across the 6 LCD panels in
"Steins;Gate divergence meter" style. When activated, every digit panel
rolls through clock-face digits at reduced quality before settling on
its target digit at full quality. After a configurable dwell, the device
automatically returns to clock TIME mode.

**Panel layout** (reading order, left → right):

```
[ digit0 ] [ . ] [ digit1 ] [ digit2 ] [ digit3 ] [ digit4 ]
   p0       p1       p2        p3        p4        p5
```

Reading-order index 1 is a **dedicated dot panel** — it never shows a
digit. The dot is loaded from `/ips/cache/dot.bmp` at full resolution
once, on the first frame of activation. After that, the dot panel never
needs to be repushed (it's static).

Reading-order index 5 is a permanent blank panel. Indices 0 and 2..4
hold the five user-supplied digits. Panel indices map to physical
hardware via `kLeftToRightPanel[]` so the layout works on both standard
and PunkCyber wiring.

## User-facing config (web UI page "Divergence Meter")

| field | type | default | meaning |
|---|---|---|---|
| `divergence_number` | 5-char string | `"00000"` | Digits to display, padded with leading `0` if shorter |
| `divergence_roll_interval` | int (ms) | 50 | Wait between rolling steps |
| `divergence_cycles` | byte (1–24) | 3 | Full passes through 0..9 per panel |
| `divergence_dwell_seconds` | byte (1–255) | 10 | How long the settled number stays visible before the device returns to TIME |

There is no font/dot color setting — the rolling visual is the user's
active clock face, and the dot uses `dot.bmp` from the same face. A
small hardcoded orange (RGB 255,115,0) is used only as a fallback if
`dot.bmp` isn't present.

The page also has an **Activate Meter** button. Each click restarts the
animation from scratch ("last click wins"). The button does NOT
persist — it sends a non-config WebSocket key `activate_meter:true`.

## End-to-end flow

1. User clicks **Activate Meter** on `web/src/divergence.html`.
2. Browser sends `9:<pageId>:activate_meter:true` over the WebSocket.
3. `updateValue()` in `src/main.cpp` matches the non-config key and
   posts `ACTIVATE_DIVERGENCE` to `mainQueue`.
4. `clockTaskFn` dequeues it, calls
   `tfts->restartDivergenceAnimation()`, then sets
   `time_or_date = DIVERGENCE` and broadcasts.
5. `DivergenceAnimation::restart()`:
   - parses the 5 target digits from `divergence_number`
   - asks `TFTs::buildDivergenceDigitCache()` to load all 10 digits
     (`0..9`) at **third resolution** (~7 KB each, ~72 KB total)
   - assigns each panel a random `startOffset` in `[0, 10)`
   - computes per-panel
     `doneTick = cycles*10 + (targetDigit - startOffset + 10) % 10`
6. The dispatch switch in `clockTaskFn` routes `IPSClock::DIVERGENCE`
   to `tfts->animateDivergence()` each tick. That calls
   `animator.loop()` (interval gating) and `animator.animate()`.
7. `animate()` per tick:
   - **first frame only:** load and push `/ips/cache/dot.bmp` to the
     dot panel, paint the blank panel.
   - **each rolling panel:** compute
     `shown = (startOffset[d] + globalTick) % 10`. Call
     `pushCachedDivergenceDigit()` which scales the cached
     third-resolution buffer up to 135×240 (nearest-neighbour) and
     pushes the panel.
   - **on the frame `globalTick == doneTick[d]`:** push the panel
     using the **full-resolution BMP** via the standard
     `setDigit/showDigit` path. Mark `digitSettled[d] = true`. Skip
     the panel from then on.
8. Once all panels are settled (`globalTick >= maxDoneTick`), phase
   transitions to `DWELLING`. After `divergence_dwell_seconds` of
   holding the final number, phase becomes `DONE` and `isFinished()`
   returns true.
9. Back in `clockTaskFn`, the `isDivergenceAnimationFinished()` check
   fires and writes `time_or_date = TIME`, broadcasting + notifying so
   the UI resets and the clock returns.

## Files in this feature

| file | role |
|---|---|
| `src/IPSClock.h` | `DIVERGENCE = 5` enum value; the four heap-allocated config getters |
| `src/main.cpp` | `divergenceConfig`, `wsDivergenceHandler`, dispatch case, `ACTIVATE_DIVERGENCE` queue handler, `activate_meter` WS key |
| `src/WSMenuHandler.{h,cpp}` | `divergenceMenu` JSON entry (key `"10"`) |
| `src/DivergenceAnimation.{h,cpp}` | Animation state machine (`ROLLING` → `DWELLING` → `DONE`), per-panel start offsets, settle logic |
| `src/TFTs.{h,cpp}` | `animateDivergence`, `restartDivergenceAnimation`, `isDivergenceAnimationFinished`, `buildDivergenceDigitCache`, `pushCachedDivergenceDigit`, `drawDivergenceDigit` |
| `web/src/divergence.html` | Number input, three numeric configs, Activate Meter button |
| `web/src/app.html` | `sv.init.divergence` case in the WS message switch, `onActivateMeter()` helper |
| `web/server.js` | Mock state and dispatcher case for local browser testing |

## Architectural decisions

### Why quarter-resolution cache for all 10 digits
Per-frame disk I/O for digit BMPs is the rendering bottleneck (~25 ms
per panel × 5 panels ≈ 125 ms/frame, very visibly sequential). We
cache the 10 digit BMPs once at activation, downsampled to **quarter
resolution** (~4 KB each, ~40 KB total). Per push we
nearest-neighbour upsample directly into the working sprite (~1 ms)
before pushing (~12 ms). Per-panel cost is ~13 ms × 5 ≈ 65 ms/frame
during rolling.

### Why a quarter — not half or third — resolution
Free heap on this board (`esp32dev`, no PSRAM) at runtime is much
tighter than the chip's nominal 320 KB SRAM suggests. Empirically
the `malloc` budget for the divergence cache is around 40–50 KB
after WiFi, MQTT, the web server and the 64 KB sprite buffer have
taken their share. We observed:

- Half-res × 5 unique digits (~80 KB) failed unless ≤ 3 unique;
- Third-res × 10 (~72 KB) failed silently during the build;
- Quarter-res × 10 (~40 KB) reliably succeeds.

The cache build also frees the 64 KB `divergenceBg` (lazy
clock-face background) before allocating, freeing more room when
`divergenceBg` happened to be allocated.

The visual cost: a 34×60 RGB565 sprite scaled 4× nearest-neighbour
is very blocky during rolling. Acceptable because:
- rolling is brief (a few seconds max);
- the panel switches to the **full-resolution BMP** the moment it
  settles, so the final number is always sharp;
- the dot panel uses **full-resolution `dot.bmp`** since it's static
  and requires only one disk load.

If your board has more free heap (e.g. PSRAM), bumping
`DIV_CACHE_W/H` back to half resolution (`67/120`, ~16 KB each,
~160 KB total) gives a much sharper rolling visual.

### Why incremental order (not random per panel)
Each panel rolls `0,1,2,3,…,9,0,1,…` starting from a random
`startOffset 0..9`. The order is fixed-and-incremental, the
randomness lives in the per-panel start. This is robust against
"shown digit not in cache" failure modes — every digit 0..9 is
cached, so `pushCachedDivergenceDigit()` always succeeds.

We tried per-panel random Fisher–Yates permutations earlier. That
broke when the cache failed to populate (heap pressure) and the
permutation referenced uncached digits — `pushCachedDivergenceDigit`
returned false, the rolling code treated that as "settle now", and
panels froze. Incremental cycle keeps the cycle source identical to
the cache contents.

### Why heap-allocated ConfigItems
ESP32 `dram0_0_seg` is link-time-fixed and was already nearly full
from the existing project + the text face. Each `ConfigItem`
static-local costs ~32–44 bytes BSS as a static instance, but only
~12 bytes (pointer + init guard) when written as
`static T* p = new T(...);`. All four divergence ConfigItems use the
heap pattern — see the comment in `IPSClock.h` near the
text/cycle/divergence getters.

## Gotchas

### `LoadImageIntoBuffer` always returns `false`
There's a bug in the upstream codebase (`src/TFTs.cpp` around line
1197): the function is declared `bool` and ends with `return false;`
unconditionally, ignoring the local `loaded` flag. To detect whether
a file was actually loaded, **check `loadedFilename` after the call**:
the function only writes the filename into that buffer on a
successful file open. We use `loadedFilename[0] = 0;` before the call
and `loadedFilename[0] != 0` after to test for success. The cache
build and the dot.bmp branch in `TFTs.cpp` both rely on this.

If you fix the upstream bug, also revert the `loadedFilename` checks
to direct `bool` checks for clarity.

### Digit BMPs are not panel-sized
Clock-face digit BMPs are typically 119×205 pixels, not 135×240 (the
panel size). `LoadImageIntoBuffer` writes them centred into the
135×240 sprite buffer with whatever margin colour the BMP defines.
Our cache snapshots the full 135×240 sprite (which already includes
the margin) at one-third resolution, so the upsampled push reproduces
the look of a normal clock-face digit push (margin + digit) — just
blockier.

### The dispatcher's `9:` opcode collides with our menu key
The web protocol uses `9:` as the leading opcode for "config update"
messages. The Text page uses menu key `"9"`; the Divergence page uses
key `"10"`. The dispatcher in `handleWSMsg` (in `src/main.cpp`)
disambiguates by colon count: a single-colon `9:` is a page init
request; multi-colon `9:<pageId>:<key>:<value>` is a config update.

### `cycles` is capped at 24
`globalTick` is `uint8_t`. With `cycleLength = 10`, max value is
`cycles * 10 + 9`. With `cycles = 24`, that's 249 — fits. The cap is
enforced in both `restart()` and `animate()`.

### Activate-while-running really resets
`restart()` rebuilds the entire cache (10 fresh `malloc`s + 10 BMP
disk loads + 10 downsamples), which takes a measurable amount of
time at activation (~100–200 ms). That's a one-time cost per
activation; rolling ticks afterwards are heap-only.

### Memory accounting
- Each cached digit: `34 × 60 × sizeof(uint16_t)` = 4 080 bytes.
- All 10: ~40 KB heap.
- Working sprite (`StaticSprite::output_buffer`): 64 KB BSS (always
  reserved, not specific to this feature).
- BSS additions for the four ConfigItems (heap-allocated pattern):
  ~48 bytes total.

If a future feature consumes more heap, the cache may stop fitting.
The cache build returns `false` on any allocation failure, in which
case the rolling phase is skipped and panels go straight to settled
(no font fallback).

## Testing checklist

Quick browser-only test (no flashing):
1. `cd web && node server.js`
2. Browse to `http://localhost:8080`, click "Divergence Meter".
3. Confirm form fields render and the Activate button is present.

End-to-end on hardware:
1. `pio run -e ipstube --target upload`
2. `cd web && npx gulp && cd .. && pio run -e ipstube --target uploadfs`
3. On the device: open the web UI, set a number, click Activate.
4. Verify all 5 digit panels roll through 0..9 starting from
   different random offsets per panel (so they show different digits
   at the same tick).
5. Verify each panel settles smoothly to its target digit at **full**
   resolution (sharper than the rolling frames).
6. Verify the dot panel shows `dot.bmp` at full resolution
   throughout — no blockiness.
7. Verify the panel at reading-order index 5 stays blank.
8. Verify the device returns to clock TIME mode after
   `divergence_dwell_seconds`.
9. Activate again — confirm the start offsets are different (random
   per activation).

---

**Reminder to future LLM agents:** if your change touches the
divergence meter — config schema, wire protocol, animation logic, web
UI, panel layout, BMP loading, the cache, the resolution, or anything
in the gotchas section — update this document in the same commit so
the next agent doesn't have to reverse-engineer the change.
