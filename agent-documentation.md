# Agent documentation — standing rules for this codebase

Rules for agents (and humans) working on the TamAIgotchi UI. These come from
the decisions made in the UI restructure (#29) and are the contract that keeps
the display correct. **Do not break them** — each one fixes a real bug that
re-appeared when a rule was relaxed.

## Single-render-pass rule (pitfall 5)

One `renderScreen()` per frame = **status bar + alien + bubble**, with exactly
**one `clearDisplay()` + one `display()`**.

- `renderScreen()` (display.cpp) is the ONLY place that calls
  `display.clearDisplay()` / `display.display()`. Every state change (status
  line, bubble text, animation phase, scroll) ends in that one call.
- **Do not split it back up.** If each region clears/pushes on its own, the
  three regions can be drawn in different frames → flicker / half-states
  (a status line without its alien, a bubble without its text).
- The status bar draws its two stored lines itself (`statusShow()`), the alien
  and the bubble keep their own current-content state; `renderScreen()` just
  composes them. Keep that division.

## Status rule: 21 chars × 2 (pitfall 10)

Status messages must fit **21 chars/line × 2 lines** (font size 1, the top two
lines of the 128×64 panel — `STATUS_CHARS_PER_LINE`).

- Longer text goes to the **bubble** or **Serial** — **never rely on the
  SSD1306 driver clipping**. `statusShow()` / `statusError()` truncate to 21
  chars, so an over-long message can no longer spill into the alien/bubble area,
  but the *correct* fix is to shorten the message, not to lean on the cut.
- Three messages overflowed before this restructure and had to be shortened:
  `Initializing I2S bus...`, `Failed to initialize I2S bus!`,
  `Record buffer alloc failed`.
- All user-facing status strings live in `TamAIgotchi/messages.h` (one place to
  review all on-screen text). Add new ones there, not inline.

## Bubble rule

- The bubble is **always the same size (15×5)** and **always drawn**, even
  empty — no popping rectangle (issue #29 Q8).
- The bubble hosts **all content**: prompt, response, and the idle `hello`
  (`MSG_ALIEN_BUBBLE`).
- The **alien is always present** at **x 4..27 / y 42..63** (the lower-left
  quadrant), the stand frame when idle, the current animation frame while the
  idle animation runs.
- The bubble rectangle is x 30..126 / y 18..62 (`BUBBLE_X/Y/W/H` in config.h).
- The idle animation draws its bubble content **without touching the bubble's
  line table** (`bubbleRenderText()`), so the stored response survives the
  animation (issue #35 step 5 follow-up, Q4). Keep that.

## Button semantics (must not drift)

| Button | Short press | Double press | Hold 5 s |
|---|---|---|---|
| **GPIO3** (main) | — (it is the hold button) | — | **hold-to-record**: hold as long as you record (max 10 s), release = send |
| **GPIO9** | scroll **down** one line | jump to **end** | **reset WiFi settings** & reboot into the setup AP |
| **GPIO11** | scroll **up** one line | jump to **start** | **exit the response view** back to IDLE |

- The main button (GPIO3) is **hold-to-record** — the hold is not a 5 s
  threshold action, it records for as long as it is held (capped at
  `MAX_REC_SECONDS` = 10 s) and sends the take on release.
- The two scroll buttons are only active while the response is on screen
  (RESPONSE state); during recording / sending they are ignored.
- The 5 s holds keep their meaning in **every** state (the GPIO9 WiFi reset is
  checked before the state machine branches).
- Debounce 50 ms, long-press threshold 5000 ms (`BUTTON_DEBOUNCE_MS` /
  `BUTTON_LONG_PRESS_MS`).

## Build

- Build with **`arduino-cli`** (not the IDE), FQBN **`esp32:esp32:lolin_s2_mini`**,
  core **`esp32:esp32` 3.3.11**, from the `TamAIgotchi/` subdir:

  ```
  arduino-cli compile --fqbn esp32:esp32:lolin_s2_mini TamAIgotchi/
  ```

- **Zero warnings in project code** (library warnings are acceptable but should
  be noted).
- **Report flash/RAM sizes** vs. the previous step's baseline (expect no change,
  or a few bytes less). The step-5 baseline is **1 224 839 B (93%) flash /
  82 716 B (25%) RAM**.

## Test build

- **Every PR** uploads a `lolin-s2-mini` test build to the esp-webflasher so a
  human can flash it from the browser.
- Version naming: **`tamai-<slug>-lolin-s2-mini-<shortsha>`** (e.g.
  `tamai-cleanup-docs-lolin-s2-mini-<shortsha>`).
- Upload the **app** image (`*.ino.bin`, not `*.merged.bin`) as the firmware
  file, plus the bootloader and partition table so the addresses are inferred
  correctly.

## Layout quick reference (128×64, font size 1)

```
+--------------------------------------------------+  y=0
|  status line 1 (<=21 chars)                       |
+--------------------------------------------------+  y=8
|  status line 2 (<=21 chars)                       |
+--------------------------------------------------+  y=16
|            |  +--------------------------------+  |
|            |  |        speech bubble           |  |  x=30..126
|   alien    |  |   (prompt / response / hello)  |  |  y=18..62
|  (4..27    |  |         15 chars x 5 lines     |  |
|   42..63)  |  +--------------------------------+  |
+--------------------------------------------------+  y=64
```
