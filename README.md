# Abaser

**Vega - Clicker V2 | Coming Soon Edition** — reborn as a Minecraft 1.8.8
Forge mod.

The original was a standalone C++ tool (Dear ImGui + DirectX 9, MSVC) by
**RuneGaming** — "Made with <3". Its compiled binary was recovered and
reverse-engineered back into source (kept under [`legacy/`](legacy/) so it
can never be lost again), and this repo is that tool's second life: same
defaults, same accent color, same tabs, same click rhythm, same quirks —
now living *inside* the game.

## Why a mod

| The standalone exe | Abaser |
|---|---|
| Run it next to the game, hope it finds the window | Loads with Forge, always there |
| Console window hidden by force every frame | No terminal, ever |
| Settings in memory, `imgui.ini` deleted on exit | Preferences persist in `config/vegaclicker.cfg` |
| Fixed binds (F4 / INSERT) | Rebindable in vanilla Controls |
| Dead "Configs" tab saying NOTHING YET | Working Save / Reload / Defaults |

## Usage

| Control | Default | Action |
|---|---|---|
| Toggle clicker | **F4** | Arms/disarms left + right clicker (original bind, 150 ms debounce) |
| Vega menu | **INSERT** | Opens the in-game menu (original: hide window) |
| `/vega` | — | `on` / `off` / `toggle` / `status`, or bare to open the menu |

Hold your physical mouse button and the engine clicks *for* you, honoring
hit-chance and randomized CPS — it calls the game's own
`clickMouse()` / `rightClickMouse()` through reflection, so every click runs
the real vanilla attack/use code path. A small HUD shows live CPS while
armed.

## Defaults (recovered from the original binary)

| Setting | Default | Range |
|---|---|---|
| Left CPS min / max | 9.6 / 14.0 | 7.5–18 / 10–20 |
| Right CPS min / max | 13.0 / 20.0 | 7.5–18 / 10–20 |
| Hit chance | 87 % | 75–100 |
| Right chance | 80 % | 75–100 |
| Block Hit | on, 6 % | 1–25 % |
| Accent color | #FF23FF | RGB sliders |

## Original quirks — kept on purpose

These are the details that made the original *feel* like the original, so
they are preserved (and toggleable in the menu):

1. **Legacy Vega timing** (Misc tab, default ON): the original engine slept
   `100/cps` then `(300|400)/cps2` ms per click — a factor-of-ten "bug".
   Real click rate runs ~2x the label. That ~22 CPS rhythm at "10 CPS"?
   That's the original. Turn it off for mathematically-accurate CPS.
2. **F4 flips left AND right together** — the original bound both toggles
   to the same key.
3. **Right branch reads hit chance in legacy mode** — the original's
   right-chance slider wrote a variable the engine never read. The slider
   is honored only in accurate mode.
4. **Block Hit** = right-click tap right after an attack (the 1.8 classic).

Sane-mod differences: clicking pauses while any GUI is open, and the mod is
client-side only.

## Releases — the autopublish pipeline

Every push to `develop` cuts the next numbered dev build automatically
(the [`release`](.github/workflows/release.yml) workflow, in the spirit of
the ZaminTorch pipeline):

- the build number comes from the `abaser-1.8.8-N` tag ledger;
- the version is stamped into the source and synced back to `develop` as an
  Abasing-authored `[skip ci]` bump commit **before** building, so the jar
  always matches the tagged tree;
- the reobfuscated jar + sources are published as a **prerelease** with
  auto-generated notes (commits since the previous tag);
- commits carrying `[skip ci]` postpone the release to the next push;
- a `Release-Title:` trailer in a commit message (or a manual
  `workflow_dispatch` run) sets the release title.

`develop` is the working branch — releases are cut from it; merges to
`main` are done by hand.

## Building from source

Requirements: **JDK 8** — ForgeGradle 2.1's deobfuscation toolchain does
not run on newer JVMs. The Gradle wrapper (4.10.3) handles the rest.

```
./gradlew setupCIWorkspace build     # first run downloads Forge + MCP
```

The jar lands in `build/libs/Abaser-1.8.8-<version>.jar`. For an IDE run
config, use `./gradlew setupDecompWorkspace` first, then
`./gradlew eclipse` / `--continue idea` as usual for ForgeGradle 2 projects.

Toolchain notes (all verified by actually building this way):

- ForgeGradle `2.1-SNAPSHOT` (the 2.1.3 line Forge rebuilt in 2021 so 1.8.x
  modding survives the maven migration);
- Gradle **4.10.3** — FG 2.1 calls `TaskOutputs.dir(Object)`, which Gradle
  5 removed; 4.10.3 is the newest Gradle that still has it;
- MCP mappings `stable_20`, the final stable channel for 1.8.8;
- no access transformer — the engine reaches `clickMouse` /
  `rightClickMouse` via reflection with both MCP and SRG names, so the same
  source works in dev *and* in the reobfuscated production jar.

## Project layout

```
src/main/java/com/runegaming/vegaclicker/
├── VegaClicker.java        @Mod entry, config load
├── CommonProxy.java        server no-op
├── ClientProxy.java        client wiring (buses, command)
├── VegaConfig.java         preference persistence (config/vegaclicker.cfg)
├── VegaKeys.java           F4 / INSERT keybinds + debounce
├── ClickerEngine.java      the engine: timing math, chance rolls, clicks
├── VegaHud.java            in-game status overlay w/ live CPS
├── CommandVega.java        /vega chat command
└── gui/
    └── VegaGuiScreen.java  the in-game menu (4 tabs, draggable, sliders)
legacy/                     the recovered C++ source of the original exe
.github/workflows/release.yml   the autopublish pipeline
```

## Fair play

An auto-clicker is an unfair advantage on most PvP servers — this is a
reconstruction of a personal tool, for your own use (1.8.8 PvP practice,
private servers, nostalgia). Don't be that guy on servers that forbid it.
