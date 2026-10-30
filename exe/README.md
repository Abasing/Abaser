# Abaser Clicker — the standalone auto-clicker

**Abaser Clicker** is a single Windows executable that finds the Minecraft
window (LWJGL) and posts synthetic mouse clicks to it — the classic external
clicker approach, rebuilt and improved. Win32 + Dear ImGui + DirectX 9, no
runtimes, no installer, no drivers.

## How it clicks

`PostMessage` goes to the game window **only** — the clicker physically
cannot click anything else on your desktop. It targets the `LWJGL` window
class, so it works with any LWJGL2-based Minecraft (1.7.x through 1.12.2;
1.8.8 is the sweet spot).

## Controls

| Control | Default | Action |
|---|---|---|
| Left toggle | **F4** | Arm/disarm the left clicker |
| Right toggle | **F4** | Arm/disarm the right clicker (same key by default — F4 flips both, rebind to split) |
| Hide window | **INSERT** | Hide/show the Abaser window (the clickers keep running) |

All three are rebindable in the UI (press the **Bind** button, then a key;
gives up after 5 s).

## Tabs

- **Clicker** — left/right enable + bind, CPS min/max sliders (randomized
  every click), hit chance per side, activation mode (hold-to-click like the
  classic, or always), focus gate, legacy timing toggle. Live CPS in the
  status strip.
- **Misc** — Block Hit (right-click sword-block taps during left clicks,
  with its own chance), lifetime click counters.
- **Configs** — the classic said NOTHING YET; this one actually saves:
  `abaser.ini` next to the exe, loaded at startup, saved on exit.
- **Settings** — accent color (the whole theme derives from it), background
  shade, FPS, hide-window bind.

## Improvements over the classic

- **Accurate CPS by default** — the classic's delay math made everything run
  at ~2× the label value. Legacy timing mode reproduces the old feel exactly.
- **Right chance is real** — the classic's right-chance slider was read by
  nobody; the engine rolled the left chance for both sides.
- **Live CPS readout + click counters**.
- **Settings persist** (`abaser.ini`).
- **Focus gate** — optionally no clicks while the game window is unfocused.
- No `imgui.ini` litter, no console flash, no shell-outs on exit.

## Deliberately absent

Anything that needs game-internal state is impossible from outside the
process: detecting open inventories/GUIs, "don't click while holding a bow",
"sword only". Those were in-game mod features; an external exe cannot see
them, so they are not pretended here.

## Building

```
cmake -S exe -B build -DABASER_VERSION="2.1-dev.4"
cmake --build build --config Release          # MSVC
# or cross-compile from Linux:
cmake -S exe -B build -DCMAKE_TOOLCHAIN_FILE=exe/mingw-toolchain.cmake \
                   -DABASER_VERSION="2.1-dev.4" && cmake --build build
```

Output: `build/AbaserClicker.exe` (or `build/Release/AbaserClicker.exe`
with MSVC multi-config generators). Dear ImGui v1.91.5 is vendored in
`third_party/imgui` — no external dependencies at all.

The embedded Ubuntu font travels in `src/abaser_assets.h` inside the
classic's own "57BC" compressed container; to regenerate it from a changed
`assets/Ubuntu-Regular.ttf`, run `tools/pack_assets.py` (see `legacy/` for
the full recovery documentation).

## Notes

- Windows SmartScreen may warn on first run (unsigned binary): right-click
  the exe → Properties → **Unblock**, then run.
- Antivirus products are sometimes suspicious of clickers in general;
  allow-list it if you trust where you got it from.
