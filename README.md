# Abaser

**Abaser** — a Minecraft auto-clicker in the classic style: a **standalone
Windows executable** that lives next to your game. The repo builds two things
and releases them together, automatically, on every push to `develop`:

| Deliverable | What it is |
|---|---|
| **`Abaser-Clicker-<version>-win64.exe`** | The auto-clicker. Win32 + Dear ImGui + DirectX 9, one file, no runtimes, no install. |
| `Abaser-1.8.8-<version>.jar` | Companion mod shell — loads silently with Forge, keeps the Abaser name in-game, answers `/abaser`. **It does not click.** |

## Abaser Clicker (the exe)

The clicker finds the Minecraft window (LWJGL — anything from 1.7.x through
1.12.2; 1.8.8 is the sweet spot) and posts synthetic mouse clicks to **that
window only**. `PostMessage` cannot click anything else on your desktop —
no focus games, no misclicks into your browser.

| Control | Default | Action |
|---|---|---|
| Left toggle | **F4** | Arm/disarm the left clicker |
| Right toggle | **F4** | Arm/disarm the right clicker (same key by default — rebind to split) |
| Hide window | **INSERT** | Hide/show the Abaser window (clickers keep running) |

Everything is in the UI: randomized CPS min/max per side, hit chance,
Block Hit (sword-block taps), accent color, rebindable keys. Settings
persist in `abaser.ini` next to the exe.

Improvements over the classic external clicker it descends from:

- **accurate CPS by default** — the classic's delay math ran everything at
  ~2× the label value; "Legacy timing" brings that feel back if you miss it
- **hold-to-click (classic) or always-click** activation
- optional **focus gate** — no clicks while the game window is unfocused
- the **right-chance slider is real** (the classic rolled the left chance
  for both sides)
- **live CPS readout** and lifetime click counters
- **Configs actually save** — the classic's Configs tab said `NOTHING YET`
- live Minecraft-window detection status in the UI

**Deliberately absent:** anything that needs game-internal state — detecting
open inventories/GUIs, "don't click while holding a bow", "sword only".
An external executable cannot see inside the game process, so those features
are not pretended. (They previously existed in the in-game mod; that mod no
longer clicks — see below.)

See `exe/README.md` for the full details.

## The companion mod (the jar)

The 1.8.8 Forge mod in this repository used to contain the auto-clicker.
It doesn't anymore — the clicking moved to the exe, and the mod is now a
deliberately featureless shell: it loads with the game, keeps the name
loadable, and `/abaser` tells you where the clicker went. Install it or
don't; the exe works either way.

## Layout

```
exe/            the standalone clicker (C++: Win32 + Dear ImGui + DX9)
src/            the companion mod shell (Java: Forge 1.8.8)
legacy/         the recovered classic clicker this descends from
                (frozen reference: original source reconstruction + notes)
.github/        the release pipeline
```

## Building

Both artifacts build in CI on every push to `develop`:

- the **exe** cross-compiles with MinGW-w64 (`cmake -S exe -B build && cmake --build build`)
- the **mod** builds with JDK 8 + Gradle 4.10.3 + ForgeGradle 2.1

Manual builds: see `exe/README.md` and the header of `build.gradle`.

## Notes

- Windows SmartScreen may warn on first run (unsigned binary): right-click
  the exe → Properties → **Unblock**, then run.
- Dear ImGui v1.91.5 is vendored in `exe/third_party/imgui` (MIT license) —
  no external dependencies.
