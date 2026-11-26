# Legacy — the original Vega-Clicker V2 source (recovered)

This is the C++ source of the original standalone tool, reconstructed by
static reverse-engineering of the lost compiled executable
(`They_Key_is_in_source_code_you_have_to_decompile_it.exe`, MSVC x64,
Dear ImGui + DirectX 9, built 2021-03-06, `Vega-Clicker` by RuneGaming —
"Made with <3").

It is kept here so the source can never be lost again, and as the spec the
Forge mod in `src/` was ported against: same defaults, same quirks, same
feel.

- `src/` — the full reconstructed C++ application
- `analysis/` — the reverse-engineering report + the 48-byte XOR key
- `assets/` — the exact TTFs the executable carried
- `tools/` — the codec reference (round-trip verified byte-for-byte)

Building it requires ImGui's `example_win32_directx9` project + MSVC —
see the report in `analysis/` for details.
