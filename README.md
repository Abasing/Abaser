# Abaser

**Abaser** — a Minecraft auto-clicker in the classic style: a **standalone
Windows executable** kept strictly outside the game. The repo builds two
things and releases them together, automatically, on every push to `develop`:

| Deliverable | What it is |
|---|---|
| **`Abaser-Clicker-<version>-win64.exe`** | The auto-clicker. Win32 + Dear ImGui + DirectX 9, one file, no runtimes, no install. |
| `Abaser-1.8.8-<version>.jar` | Companion mod — **carries the exe inside**: drop it in `mods/` and the mod unpacks, starts and manages the clicker by itself (zero manual setup). Feeds it live game state (GUIs, bow, spawn box, ally under crosshair), closes it with the game, and carries the **aim assist** (client-side camera steering, players only, with peace-list / manual / lock-in exceptions). **It does not click.** |

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
| Aim assist | **G** (in the mod, rebindable) | Toggle the aim assist — the mod's client-side camera nudge, see "Aim assist" |

Everything is in the UI: randomized CPS min/max per side, hit chance,
Block Hit (sword-block taps), the two-color theme, rebindable keys.
Settings persist in `abaser.ini` next to the exe. The window parks in the
**system tray** (X / INSERT / tray-click); the tray menu can arm/disarm
the clickers, toggle **Start with Windows** (a per-user Run key, launches
straight into the tray), and exit.

Improvements over the classic external clicker it descends from:

- **accurate CPS by default** — the classic's delay math ran everything at
  ~2× the label value; "Legacy timing" brings that feel back if you miss it
- **independent left/right timing** — both click channels run on one engine
  thread but carry their own click deadlines, so the left clicker's
  press/gap sleeps can no longer stall the right channel (a real defect of
  the sequential loop: each side's effective CPS dropped under the other's
  load). Presses are non-blocking DOWN-post + release-deadline pairs; the
  loop sleeps only until the nearest deadline. The legacy right-click gap
  also now reads the RIGHT channel's CPS range (it once read the left max),
  and the whole timing math lives in one simulator-tested function
- **race-free shared state** — the engine, the UI, the tray, the mod link
  and the status streamer all touch the same settings and gate flags; every
  access goes through one recursive state mutex (gates, config apply/save,
  UI frame, snapshots) and the lifetime counters are atomics
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
- **spawn + ally gates** — the mod's link can hold EVERY click while you
  stand inside your spawn box (`spawn_gate` setting, on by default) or while
  a team member is under your crosshair, so you never hit friends while
  spamming; the footer tells you which gate is holding ("inside spawn",
  "ally under crosshair")
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

- when a **server/world loads**, the mod installs and starts
  `AbaserClicker.exe --managed /tray` — release jars carry the exe inside,
  so the mod unpacks it to `.minecraft/Abaser/` on first use (SHA-1-checked:
  a newer jar replaces an older exe automatically, `abaser.ini` survives in
  the same folder) and it appears directly in the system tray; a manually
  installed copy (game folder, `mods/` folder, or `$ABASER_EXE`) still works
  and is preferred only if no bundled exe is present — and a stray running
  instance is retired first, so the managed process always owns the
  single-instance mutex and receives the safety state below;
- on every change (plus a 2 s heartbeat) the mod writes one JSON line to
  the exe's stdin:
  `{"gui":true,"bow":false,"held":"item.bow","spawn":false,"ally":false}`;
- the exe's click gates (shared code, regression-tested in the simulator):
  * **any open GUI** — inventory, chest, villager, enderchest, chat, pause
    menu, everything — blocks **all** clicks;
  * **inside the spawn box** (the mod measures an exact 3D box, editable
    with `/abaser spawn`) blocks **all** clicks while the exe's
    `spawn_gate` setting is on — toggle it in the panel's Clicker tab;
  * **an ally under the crosshair** — peace-list or manually excepted
    player — blocks **all** clicks so you never hit friends while
    spamming (the "Ally hold" toggle);
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

## Aim assist — the mod's camera nudge (client-side)

Abaser absorbed the classic MOG-AIMASSIST client mod and rebuilt it into the
panel. The aim assist is the one engine that lives **inside the mod**: each
client tick it finds the closest valid **player** and turns your own camera a
fraction of the way toward them. It steers your view and nothing else — it
never presses a button. Every click still comes from the exe, from its own
engine and its own hotkeys; the two never touch each other's work. (The mob
assist was removed — targeting is players-only.)

| Feature | What it does |
|---|---|
| **Target search** | Closest valid player within **Distance** blocks **and** inside the **FOV** cone (both measured around where you look) |
| **Target stickiness** | The current target is KEPT while it stays valid (hysteresis on distance + FOV); the assist only re-scans when it drops out — no flip-flopping between players |
| **Strength** | Overall pull toward the target (0–1) |
| **Vertical / Horizontal** | Extra multipliers so you can keep pitch calmer than yaw |
| **Smooth aim** | Turn only **Smooth** of the remaining angle per tick — a flick, not a snap |
| **Silent aim** | Apply the same turn through the mouse's own entry point (`setAngles`) instead of writing the rotation fields. The mode converts the wanted degrees into `setAngles` input space — 1.8.8's `setAngles` multiplies the input by its own `0.15` mouse scaling and flips the pitch sign, so feeding it raw degrees turned yaw at ~1/7 strength and pitched the WRONG way (fixed: `setAngles(stepYaw / 0.15F, -stepPitch / 0.15F)`) |
| **Require click** | Only assist while the attack key is held |
| **Peace/team exceptions** | The mod parses the server's `You have peace with N player(s)` chat block (`- Name` lines follow) and never targets those players — live in memory, refreshed each time the server re-announces |
| **Manual exceptions** | `/abaser except add|remove NAME` — players the assist (and the clicks) always skip; persisted |
| **Lock-in mode** | `/abaser lock on` + `lock add NAME` — ONLY those names are targeted, no one else; persisted, toggled in the panel too |
| **Spawn gates** | An exact 3D box (two corners): **no aim assist** inside it (Aim tab) and **no clicks** inside it (`spawn_gate`, Clicker tab) — both on by default |

Controls and storage:

- **G** toggles it (rebindable in Controls, `key.abaser.aimassist`);
  `/abaser aim` (or `aim on` / `aim off`) does the same from chat;
- the panel's **Aim Assist tab** (Right Shift) exposes every knob with the
  same glass sliders and toggles as the Clicker tab — plus a live
  `target: <name>` readout and a `peace N manual N lock N` counter line;
- the default spawn box is the measured SKYPVP spawn
  (`-23.52 57.00 23.67` to `23.65 58.00 -22.35`, exact 3D test); move it
  with `/abaser spawn here [halfwidth]` (re-centers around you),
  `/abaser spawn box X1 Y1 Z1 X2 Y2 Z2` (exact corners) or
  `/abaser spawn show`;
- `/abaser peace` dumps the currently parsed peace list;
- aim settings persist in the game's `config/abaser_aim.json` — mod-owned,
  clamped on load and on every edit, and never sent over the mod link
  (the exe has nothing to do with them); exception and lock-in lists ride
  along in the same file.

Guards: the assist is dormant while **any screen is open** (inventory,
chest, villager, enderchest, chat, the Abaser panel itself), while you are
dead, while no world is loaded, and inside the spawn box while the aim gate
is on. It runs purely client-side; the server only sees the resulting look
angles. Switching worlds clears the parsed peace list — a new server's
allies mean nothing on the next one.

## The companion mod (the jar)

The 1.8.8 Forge mod in this repository used to contain the auto-clicker.
It doesn't anymore — the clicking lives in the exe, and the mod's jobs are
the **mod link** above (start the exe with the server, feed it game state
— GUIs, bow, spawn box, ally under crosshair — over stdin, stop it when
the game closes), the **in-game panel** (Right Shift, or `/abaser panel`)
so you can manage and configure the clicker without leaving the game, and
the **aim assist** above — the one piece of Abaser that steers your camera
from inside the mod, client-side. `/abaser` in chat explains the wiring.

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
