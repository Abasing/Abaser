# Abaser

**Abaser** — a lightweight background auto-clicker for Minecraft **1.8.8**
(Forge), with an in-game menu, safety options and saved preferences.

It loads with Forge and runs silently in the background (no terminal, no
extra windows, nothing in your face) until you press **F4** or **INSERT**.
Hold your physical mouse button and the engine clicks *for* you, honoring
hit-chance and randomized CPS — it calls the game's own
`clickMouse()` / `rightClickMouse()` through reflection, so every click
runs the real vanilla attack/use code path. A small HUD shows live CPS
while armed.

## Usage

| Control | Default | Action |
|---|---|---|
| Toggle clicker | **F4** | Arms/disarms left + right clicker together (150 ms debounce) |
| Abaser menu | **INSERT** | Opens the in-game menu (5 tabs, draggable panel) |
| `/abaser` | — | `on` / `off` / `toggle` / `status`, or bare to open the menu |

Everything is rebindable in vanilla **Options > Controls > Abaser**.
Preferences persist in `config/abaser.cfg` and autosave whenever the menu
closes.

## Clicker settings

| Setting | Default | Range |
|---|---|---|
| Left CPS min / max | 9.6 / 14.0 | 7.5–18 / 10–20 |
| Right CPS min / max | 13.0 / 20.0 | 7.5–18 / 10–20 |
| Hit chance | 87 % | 75–100 |
| Right chance | 80 % | 75–100 |
| Block Hit | on, 6 % | 1–25 % |
| Accent color | #FF23FF | RGB sliders |

## Safety options

The Safety tab keeps the clicker out of situations where spamming clicks
would hurt you:

| Option | Default | What it does |
|---|---|---|
| No bow right-click | **on** | While holding a bow, right clicks are held off — draw and aim normally instead of spam-firing |
| Sword-only attack | off | Left clicks only fire while you are actually holding a sword |
| No click while eating | **on** | Right clicks pause while using an item (eating, drinking potions, drawing a bow), so nothing gets interrupted |
| Pause when unfocused | **on** | No clicks go into an unfocused window — a real player can't either |

In addition, clicking **always stops while any GUI is open** — inventory,
chest, chat, pause menu, anything. That one is not optional; it is simply
how a sane mod behaves.

## Engine quirks — kept on purpose

The click rhythm reproduces the feel of the tool this mod grew out of
(the recovered source lives under [`legacy/`](legacy/)), quirks included:

1. **Legacy timing** (Misc tab, default ON): the engine sleeps `100/cps`
   then `(300|400)/cps2` ms per click — real click rate runs ~2x the
   label. That ~22 CPS rhythm at "10 CPS" is intentional. Turn it off for
   mathematically-accurate CPS.
2. **F4 flips left AND right together** — both toggles ride one key.
3. **Right branch reads hit chance in legacy mode** — the right-chance
   slider is honored only in accurate mode.
4. **Block Hit** = right-click tap right after an attack (the 1.8 classic).

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
src/main/java/net/zaminmc/abaser/
├── Abaser.java             @Mod entry, config load
├── CommonProxy.java        server no-op
├── ClientProxy.java        client wiring (buses, command)
├── AbaserConfig.java       preference persistence (config/abaser.cfg)
├── AbaserKeys.java         F4 / INSERT keybinds + debounce
├── AbaserEngine.java       the engine: timing math, chance rolls, safety gates, clicks
├── AbaserHud.java          in-game status overlay w/ live CPS
├── CommandAbaser.java      /abaser chat command
└── gui/
    └── AbaserGuiScreen.java  the in-game menu (5 tabs, draggable, sliders)
legacy/                     the recovered C++ source this mod grew out of
.github/workflows/release.yml   the autopublish pipeline
```

## History

The click rhythm and defaults were recovered by reverse-engineering a
lost standalone tool; its reconstructed source is preserved under
[`legacy/`](legacy/) so it can never be lost again. Abaser is that
engine's second life — now living inside the game.

## Fair play

An auto-clicker is an unfair advantage on most PvP servers — this is a
personal tool, for your own use (1.8.8 PvP practice, private servers,
nostalgia). Don't be that guy on servers that forbid it.
