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

The window itself is **frameless**: the top band is the app's own glass
canvas — drag it anywhere, or use the drawn **— / ✕** buttons in the top
right corner (— = taskbar minimize, ✕ = park to the tray, exactly like the
old native X). No native title bar anywhere.

## System tray

Abaser lives in the tray like a proper background tool:

- **X** (or **INSERT**, or a left-click on the tray icon) parks the window
  by the clock — the clickers keep running.
- The tray tooltip shows the live armed state (`L:ON R:off`).
- **Right-click the tray icon** for the menu: Show/Hide, arm/disarm the
  left and right clickers, Start with Windows, and Exit (the real quit —
  X never kills the app).
- A one-time balloon tip explains "X = tray, not exit" the first time the
  window is parked. If Explorer restarts, the icon re-adds itself.
- The icon (a magenta cursor-arrow, the accent color) is drawn at runtime
  at the tray's own icon size — no `.ico` file ships, the exe stays one
  self-contained file, and it stays crisp at any DPI.
- Launching a second copy doesn't stack a duplicate: the new process just
  tells the running one to show its window and exits.

## Start with Windows

The **Settings** tab (and the tray menu) have a **Start with Windows**
checkbox. It writes a plain per-user Run entry
(`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, no admin rights,
no service) that launches Abaser with the `/tray` argument — so at login
it starts **directly in the tray**, window hidden, clickers disarmed until
you press your toggle key. Unchecking removes the entry again; deleting
the registry value yourself is detected at the next launch. On Windows
shutdown/logoff the settings are still saved on the way out.

## The UI

Apple **Liquid Glass** style: one translucent canvas under frosted glass
cards — 1 px light borders, top specular highlights, 10 px corner radii —
with iOS-style controls: toggle switches (dark OFF trough so "off" always
reads), one-row sliders, a segmented tab control whose active pill is the
solid accent color, status chips and theme swatches. Cards are sized to
their content (no hollow bottoms), every label is vertically centered in
its row, and the type scale is 16/14/12 px so the settings actually fit.
Everything renders through `src/ui.cpp`, shared verbatim with the headless
simulator (see `sim/README.md`). The window is frameless, so the canvas —
gradient, glows and all — runs all the way under the top band. There is no
native title bar.

Tabs:

- **Clicker** — left/right glass cards (enable switch, hotkey pill, CPS
  max/min, chance) + an Activation card (hold-to-click, focus gate, legacy
  timing). Live CPS in the footer.
- **Misc** — Block Hit (sword-block taps during left clicks, with its own
  chance) and lifetime click counters.
- **Configs** — the classic said NOTHING YET; this one actually saves:
  `abaser.ini` next to the exe, loaded at startup, saved on exit.
- **Settings** — theme presets + custom pickers, backdrop dim, hide-window
  bind, Start with Windows, about/FPS.

The **theme** is Discord's profile-color model: two plain integers,
`theme_primary` and `theme_accent`, each stored as 24-bit RGB
(`(R<<16)+(G<<8)+B`, the exact shape of Discord's `theme_colors` array).
Everything else is derived at render time:

- the **background** is a vertical gradient of the primary (top ≈ 30 %
luminance, bottom ≈ 12 %) with two soft glows tinted by the accent and the
primary — pick the Discord blurple preset and the whole app re-themes
itself;
- the **accent** drives switches, sliders, the active tab pill and status
chips;
- **knob and glyph colors** are picked by relative luminance (`OnColor`):
dark knobs on light accents, white on dark — text stays readable on any
color you throw at it;
- six preset pairs (Discord blurple, the classic pink, blue, purple,
orange, green) plus labeled **Primary** / **Accent** custom pickers in the
Settings tab; `abaser.ini` stores the two ints (old configs with the
legacy `accent = r g b a` line still load fine).


## The mod link (managed mode)

Anything that needs game-internal state is impossible from outside the
process — so the companion Forge mod (`src/` in the repo) hands it over
the room. When the mod starts the exe it passes two arguments:

```
AbaserClicker.exe --managed /tray
```

`/tray` starts it directly in the system tray; `--managed` enables the
link. In managed mode a reader thread ingests the mod's JSON lines from
stdin (`{"gui":true,"bow":false,"held":"item.bow"}`) and drives the
click gates in `ui_state.h`:

- **`gui` true — any open in-game screen** (inventory, chest, villager,
  enderchest, chat, pause menu, everything) → *no* clicks at all;
- **`bow` true — the player holds a bow** → every right-click emission is
  held back (the right autoclicker and Block Hit's sword-block taps), so
  charging the bow works normally. Left clicks are untouched;
- the footer turns amber and tells you why: `paused - a menu is open` /
  `paused - bow drawn`; the titlebar chips show `Mod` and the in-game
  state while the link is up.

Closing the game closes the clicker — the mod sends `{"exit":true}`, and
if that never arrives (crash, kill), stdin EOF triggers the same graceful
exit (settings saved, tray icon removed).

The mod never clicks. All clicks still come from this exe — its own
engine, its own hotkeys, its own `PostMessage` targeting; the state line
only decides *whether* the engine fires, never *how*.

Run standalone (no arguments, no mod) and everything behaves exactly as
before — the gates just stay open.

## The old click bug

Before this redesign the UI stacked four overlapping Dear ImGui root
windows, the topmost being an 800x800 "DrawList" overlay used to draw the
sidebar separator. Dear ImGui's hover search does not skip `SkipItems`
windows — so the overlay was *always* the hovered window, `ItemHoverable()`
failed for every widget in every other window, and **no click anywhere in
the UI did anything**. The overlay is gone (the UI is one root window now)
and the behavior is regression-tested in the simulator: 20 interactions
(toggles, tabs, slider drags, bind capture, swatches, counters) all assert
the state actually changes.

## Improvements over the classic

- **Accurate CPS by default** — the classic's delay math made everything run
  at ~2× the label value. Legacy timing mode reproduces the old feel exactly.
- **Right chance is real** — the classic's right-chance slider was read by
  nobody; the engine rolled the left chance for both sides.
- **Live CPS readout + click counters**.
- **Settings persist** (`abaser.ini`).
- **Focus gate** — optionally no clicks while the game window is unfocused.
- **System tray** — X/INSERT park it by the clock; arm/disarm, autostart and
  Exit from the tray menu; one-instance guard; runtime-drawn icon.
- **Start with Windows** — per-user Run key, launches straight into the tray.
- **Frameless seamless window** — the glass canvas replaces the native bar.
- **Discord-style two-int theming** with luminance-safe derived colors.
- **Mod link** — game state (open GUIs, held bow) over stdin, click gates
  shared with the regression-tested simulator.
- No `imgui.ini` litter, no console flash, no shell-outs on exit.

## Deliberately absent

Nothing on the state front anymore — the mod link covers open GUIs and
held items. Still impossible from outside the process (and not pretended):
"sword only" targeting or any deep inventory scanning. The mod reports one
thing: is *a* screen open, and what's in the player's hand.

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

## Testing the UI (no Windows needed)

`sim/` builds the real `src/ui.cpp` on Linux with a software rasterizer and
runs 34 scripted interactions against it, saving screenshots of every tab
and state:

```
exe/sim/build_sim.sh
```

If a click works there, it works in the exe — same code, same Dear ImGui
version, same layout constants (shared in `ui.h` so clicks and pixels can
never drift apart). The gate policy (`GateLeft` / `GateRight` /
`GateBlockHit` in `ui_state.h`) is the exact code the engine calls, and
the sim asserts its whole truth table: open GUI blocks everything, drawn
bow blocks right only, no game window blocks everything.

The embedded Ubuntu font travels in `src/abaser_assets.h` inside the
classic's own "57BC" compressed container; to regenerate it from a changed
`assets/Ubuntu-Regular.ttf`, run `tools/pack_assets.py` (see `legacy/` for
the full recovery documentation).

## Notes

- Windows SmartScreen may warn on first run (unsigned binary): right-click
  the exe → Properties → **Unblock**, then run.
- Antivirus products are sometimes suspicious of clickers in general;
  allow-list it if you trust where you got it from.
