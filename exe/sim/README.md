# The headless UI simulator

`sim_main.cpp` compiles the **real** `src/ui.cpp` — the exact code the
Windows exe draws — on Linux without any Win32/DirectX dependency, feeds it
synthetic mouse/key events through Dear ImGui's own event queue, rasterizes
the draw data with a software rasterizer (`sim_raster.h`, top-left fill
rule + bilinear font atlas sampling, same blending math as the GPU
backends) and saves BMP screenshots.

## Why it exists

The exe's UI used to have a bug where **clicks did nothing**: an 800x800
overlay window with `SkipItems = true` sat on top of everything, and Dear
ImGui's hover search does not skip `SkipItems` windows — so *it* was always
the hovered window and no widget anywhere else was ever hoverable. A
bug like that is invisible to screenshot-only QA and invisible to a Windows
box you don't have — but trivially observable here, because the simulator
asserts that interacting **changes the underlying state**.

## What it checks (all PASS)

toggles arm/disarm both clickers · segmented tabs switch · slider drags
change float and int values · activation switches flip · bind capture
starts, accepts a key, and cancels · theme presets set the two color ints
and re-theme the background gradient · the full click-gate truth table
(open GUI blocks everything, drawn bow blocks right only, no game window
blocks everything) · the drawn minimize / close buttons fire the host
callbacks · the Dim backdrop toggle flips · counters reset · the MC /
no-game chip switches · a `{"config":{...}}` line from the in-game panel
applies every key with the UI's clamps, arms/rebinds/themes without
weakening the click gates, dumps round-trip losslessly, persists through
abaser.ini, and the status line reports armed sides + live CPS.

## Run

```
./build_sim.sh
```

PASS/FAIL lines print to stdout; BMP screenshots land in `shots/`
(`01_clicker_default` … `14_no_game`). The committed PNGs are those
screenshots converted. Exit code is non-zero if any test fails — CI-friendly.

## Trustworthiness notes

* same Dear ImGui sources, same fonts, same layout constants (`ui.h` is the
  single source of truth for both drawing and click targets);
* `io.AddMousePosEvent` / `AddMouseButtonEvent` — the same input path the
  Win32 backend uses;
* the rasterizer reproduces straight-alpha `SrcAlpha / 1-SrcAlpha`
  blending; verified against hand-computed values for solid, gradient and
  textured primitives.

If a click works here, it works in the exe.
