# Vega-Clicker V2 — Reverse-Engineering Report

Target: `They_Key_is_in_source_code_you_have_to_decompile_it.exe`
SHA-256: `6cfe6fac4c62d54e6ed35a12607c561beced186069801b20e6eebede85940fa0`

## 1. Binary fingerprint

| Property | Value |
|---|---|
| Format | PE32+ console, x86-64, 5 sections, 613 888 bytes |
| Compiler | MSVC (imports `MSVCP140.dll`, `VCRUNTIME140.dll`, `__CxxFrameHandler4`) |
| Link timestamp | 2021-03-06 10:58:30 UTC |
| GUI stack | Dear ImGui (~1.80–1.83) + `imgui_impl_dx9` + `imgui_impl_win32` |
| Project path | `C:\Users\miss_\Documents\GitHub\Vega-Clicker` |
| Author credit | `"Made with <3\nRuneGaming"` (XOR-obfuscated in `.rdata`) |
| C++ features | `std::thread` (via `_beginthreadex`), PPL/`std::future`, `std::condition_variable` |

## 2. What the program is

An **auto-clicker for Minecraft** with an ImGui skin:

* creates a borderless-ish window titled `" "` (a single space),
* hides the console window every frame (`GetConsoleWindow` → `ShowWindow(SW_HIDE)`),
* finds the Minecraft window by **class `"LWJGL"`**,
* while the physical LMB/RMB is held (and the corresponding toggle is on),
  posts `WM_LBUTTONDOWN/UP` / `WM_RBUTTONDOWN/UP` via `PostMessageA` to the
  game window, honouring a **hit chance** and **randomised CPS** between
  configurable min/max, plus a **Block Hit** mode (RMB tap during LMB clicks),
* on exit runs `system("del imgui.ini")` (deletes the layout file).

Default settings (read from the `.data` section):

| Setting | Default | Range in UI |
|---|---|---|
| Left toggle bind | `VK_F4` (115) | rebindable |
| Right toggle bind | `VK_F4` (115) | rebindable |
| Hide-window bind | `VK_INSERT` (45) | rebindable |
| Left CPS min / max | 9.6 / 14.0 | 7.5–18 / 10–20 |
| Right CPS min / max | 13.0 / 20.0 | 7.5–18 / 10–20 |
| Hit chance | 87 % | 75–100 |
| Right chance (UI only) | 80 % | 75–100 |
| Block-hit chance | 6 % | 1–25 |
| Accent color | (1.00, 0.137, 1.00, 1.00) | color picker |
| Other Background | on | checkbox |

## 3. UI layout (4 ImGui windows, rebuilt in `src/main.cpp`)

```
+-----------------------------------------------------------+
| Vega - Clicker V2 | Coming Soon Edition   <- window 1     |
| (empty "title shell", 500px wide at (0,0))               |
| +--------+  +---------------------------------------+    |
| |Clicker |  |  Canvas (107,25 size 529.9x350)       |    |
| |Misc    |  |  tab content:                         |    |
| |Settings|  |   0 Clicker  - toggles, binds,        |    |
| |        |  |                CPS+chance sliders     |    |
| |Made    |  |   1 Misc     - Block Hit + note       |    |
| |with <3 |  |   2 Configs  - "NOTHING YET"          |    |
| |Rune-   |  |   3 Settings - color, background,     |    |
| |Gaming  |  |                FPS, hide-bind         |    |
| +--------+  +---------------------------------------+    |
|        ^ vertical accent separator (107,0)-(107,400),    |
|          thickness 3, drawn in window 4 "DrawList"       |
+-----------------------------------------------------------+
```

Notes:
* Tab IDs are 0/1/**3** — the "Configs" tab (2) has content but **no button
  ever selects it** (dead code in the shipped build).
* Slider labels are runs of spaces of differing length (`"  "`, `"   "`, …) —
  the author's trick to get invisible-but-unique widget IDs.
* The `"Application average %.3f ms/frame (%.1f FPS)"` line is the classic
  ImGui demo metrics string.
* Background clear color: `0xFF1B1616`, or `0xFF0C161C` when
  "Other Background" is checked.

## 4. Clicker engine (thread at `0x1400439D0`)

```
loop:
    hwnd = FindWindowA("LWJGL", NULL)
    if GetAsyncKeyState(VK_LBUTTON) and left_enabled:
        if rand()%100 < hit_chance:                 # 87 default
            PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0)
            cps  = lerp(left_min_cps,  left_max_cps,  rand()/32767.0)
            sleep(100 / cps)                        # ms  (note: 100, not 1000 — original quirk)
            PostMessageA(hwnd, WM_LBUTTONUP, MK_LBUTTON, 0)
            if blockhit and rand()%100 < blockhit_chance:      # 6 default
                PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, 0)
                sleep(20)
                PostMessageA(hwnd, WM_RBUTTONUP, MK_RBUTTON, 0)
            cps2 = lerp(left_min_cps, left_max_cps, rand()/32767.0)
            sleep((cps2 > 14.0 ? 300 : 400) / cps2)
    if GetAsyncKeyState(VK_RBUTTON) and right_enabled:
        ... same shape with right_min_cps/right_max_cps ...
        (reads hit_chance, not right_chance — original quirk)
    sleep(1)
```

The `sleep` is a precise `sleep_until`-style helper built on
`QueryPerformanceCounter` (fn `0x140044420`).

Bind toggles are watched every frame by fn `0x1400461B0`: if the bind key
(or any VK sharing its display name) is down → flip the flag, debounce
150 ms. The scan used by the "Bind" buttons walks VK 3..255 requiring a
non-empty entry in the 103-entry key-name table (which, faithful to the
original, has no HOME and no arrow-key names).

## 5. Asset pipeline (fully verified, round-trip proven)

```
Ubuntu-Regular.ttf ──57BC+LZ──> 196 592-byte blob @ .rdata 0x524E0
ProggyCleanTT.ttf  ──57BC+LZ──>──Base85──> 11 980-char string @ .rdata 0x4F560
```

* **57BC container**: magic `57 BC 00 00`, flags `0`, BE32 raw size,
  BE32 `0x00040000`, then the LZ77 stream.
* **LZ77**: 10 token families (see `vega_compression.h`), literal runs 1–65536,
  matches with 1–3 byte distances, explicit `05 FA` end marker.
* **Base85**: alphabet `0x23..0x78` minus `\` (85 chars); groups of 5 chars →
  4 bytes; **first char is the least-significant digit** and bytes are
  emitted **little-endian** (both differ from standard Ascii85).
* My decoder reproduces both embedded blobs **byte-for-byte**, and my encoder
  re-encodes the extracted ProggyClean blob to the **identical 11 980-char
  string** found in the executable.

## 6. String obfuscation

All user-visible strings XOR'd with the 48-byte key in `analysis/XOR_KEY.txt`,
decoded on the stack before each use (classic movabs + `pxor` pattern, 32
strings recovered — see the key file for the list).

## 7. Notable quirks (preserved in the reconstruction on purpose)

1. `sleep(100 / cps)` — a factor of 10 off the "true" CPS math.
2. Right-click branch reads `hit_chance`; the right-chance slider writes a
   variable the engine never reads.
3. The "Configs" tab is unreachable (no button sets tab = 2).
4. `SliderScalar` with `ImGuiDataType_U8` on `int`-sized globals.
5. `ImGui::EndFrame()` is called explicitly before the D3D state changes, and
   `ImGui::Render()` runs *inside* `BeginScene()` — harmless but unusual.
6. `system("del imgui.ini")` on exit (instead of `remove()`).
7. The embedded ProggyClean font is dead data (fallback path never taken).

## 8. Function map (main artifacts)

| VA | Role |
|---|---|
| `0x140046320` | `main()` — window, device, fonts, thread, UI loop |
| `0x1400439D0` | clicker thread body |
| `0x140048FB0` | `std::thread` trampoline |
| `0x1400461B0` / `0x140046120` | bind toggle watcher / key-down check |
| `0x140044420` | precise millisecond sleep |
| `0x140042C60` | custom create-font-texture (device objects) |
| `0x140042DF0` | invalidate device objects (reset path) |
| `0x140048110` | `WndProc` |
| `0x140043330` | `ImGui_ImplWin32_NewFrame` |
| `0x1400435B0` | `ImGui_ImplWin32_WndProcHandler` |
| `0x140025340` | `ImFontAtlas::AddFont` — patched with 57BC/LZ decode |
| `0x140025790` | Base85 decode + AddFont helper |
| `0x140024D40` | `ImFontAtlas::AddFontDefault` — patched with Base85 font |
| `0x14002A0F0` | LZ77 token decoder |
| `0x140029F80` / `0x14002A040` | LZ77 history-copy / literal-emit helpers |
