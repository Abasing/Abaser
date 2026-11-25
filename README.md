# Abaser

**Abaser** — a Minecraft auto-clicker in the classic style: a **standalone
Windows executable** that lives next to your game. The repo builds two things
and releases them together, automatically, on every push to `develop`:

| Deliverable | What it is |
|---|---|
| **`Abaser-Clicker-<version>-win64.exe`** | The auto-clicker. Win32 + Dear ImGui + DirectX 9, one file, no runtimes, no install. |
| `Abaser-1.8.8-<version>.jar` | Companion mod — loads with Forge, starts the exe with your server, feeds it live game state, closes it with the game. **It does not click.** |

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
| Open in-game panel | **Right Shift** (in the mod) | The companion mod's in-game panel — see "The mod link" |

Everything is in the UI: randomized CPS min/max per side, hit chance,
Block Hit (sword-block taps), the two-color theme, rebindable keys.
Settings persist in `abaser.ini` next to the exe. The window parks in the
**system tray** (X / INSERT / tray-click); the tray menu can arm/disarm
the clickers, toggle **Start with Windows** (a per-user Run key, launches
straight into the tray), and exit.

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
- **system tray** — X/INSERT park it by the clock; the tray tooltip tracks
  the armed state, its menu arms/disarms, toggles start-with-Windows and
  exits; a second launch just summons the first
- **start with Windows** — one checkbox (Settings tab or tray menu) writes
  a per-user Run entry that launches it straight into the tray
- **seamless glass UI** — a frameless window (no native title bar): the
  same glass canvas runs under the whole top band, with drawn minimize /
  close buttons and a drag-anywhere band
- **Discord-style theming** — a theme is two plain integers
  (`theme_primary`, `theme_accent`, stored as `(R<<16)+(G<<8)+B`, exactly
  like Discord profile `theme_colors`); the background is a vertical
  gradient of the primary, controls take the accent, and knob/glyph colors
  are picked by relative luminance so any color stays readable

## The mod link — game state the exe cannot see alone

An external program can never see inside the game process on its own. The
companion mod closes that gap with a stdin link:

- when a **server/world loads**, the mod starts
  `AbaserClicker.exe --managed /tray` — it appears directly in the system
  tray (game folder or `mods/` folder, or `$ABASER_EXE`);
- on every change (plus a 2 s heartbeat) the mod writes one JSON line to
  the exe's stdin: `{"gui":true,"bow":false,"held":"item.bow"}`;
- the exe's click gates (shared code, regression-tested in the simulator):
  * **any open GUI** — inventory, chest, villager, enderchest, chat, pause
    menu, everything — blocks **all** clicks;
  * **holding a bow** blocks every right-click emission (the right
    autoclicker *and* Block Hit taps) so charging works normally; left
    clicks keep going;
- closing the game closes the clicker: the mod says `{"exit":true}` and
  destroys the process, and the exe also exits on stdin EOF — a crashed
  game cannot orphan it.

The clicks still come from the exe itself, from its own engine and its own
hotkeys, exactly as before — the mod never generates, translates or
forwards a single click; it only reports what it can see.

### The in-game panel — configure it from inside the game

The mod link carries SETTINGS as well as state. Press **Right Shift**
(rebindable in Minecraft's Controls, `key.categories.abaser`) or run
`/abaser panel` and the mod opens a small glass-styled panel in-game:

- the **left and right cards** (arm switch, CPS max/min, chance),
- **activation** toggles (hold-to-click, focus gate, legacy timing),
- **block hit** with its own chance slider,
- the six **theme preset pairs** (the same Discord-style two-int colors),
- a footer with the **live CPS** and the pause reason ("a menu is open",
  "bow drawn") — the same honesty the exe's own footer has.

Every change leaves the mod as one `{"config":{...}}` line on the exe's
stdin using the exact `abaser.ini` key names; the exe applies it through
the same code path as its own sliders (same clamps), **persists it to
abaser.ini**, and echoes the canonical snapshot back on stdout. A 2 s
status line keeps the panel's live numbers fresh. The exe also dumps its
current settings on the first stdin line, so a freshly opened panel shows
what the exe is actually doing.

The panel is a cockpit, not the engine. It sends numbers for the knobs the
exe already owns — never a click. And because the panel is a GUI like any
other, the mod reports `gui=true` while it is open, so the exe holds every
click while you configure it.

Without the mod, the exe behaves exactly as it always has (standalone,
tray, hotkeys); without the exe, the mod just sits there — it cannot click
anything.

See `exe/README.md` for the full details.

## The companion mod (the jar)

The 1.8.8 Forge mod in this repository used to contain the auto-clicker.
It doesn't anymore — the clicking lives in the exe, and the mod's whole job
is the **mod link** above: start the exe with the server, feed it game
state over stdin, stop it when the game closes, and give you the
**in-game panel** (Right Shift, or `/abaser panel`) so you can manage and
configure the clicker without leaving the game. `/abaser` in chat explains
the wiring.

## Layout

```
exe/            the standalone clicker (C++: Win32 + Dear ImGui + DX9)
src/            the companion mod (Java: Forge 1.8.8) — the mod link
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
