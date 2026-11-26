# imgui_draw.cpp modifications in the original binary

Static analysis shows the original executable **patched Dear ImGui itself** in
two places, rather than doing the decompression in `main.cpp` (which is what
the reconstruction in `main.cpp` does instead — same result, no library fork).

If you want a bit-perfect rebuild of the *original* design, apply these two
patches to `imgui_draw.cpp` (ImGui ~1.80–1.83 era, the version the binary was
compiled against):

---

## Patch 1 — `ImFontAtlas::AddFont` transparently decompresses "57BC" blobs

At `0x140025340` the binary's `ImFontAtlas::AddFont` starts by checking the
font data for the magic header `57 BC 00 00` (big-endian read of the first
four bytes). If present it:

1. reads the **decompressed size** from bytes 8–11 (big-endian u32),
2. allocates that many bytes,
3. runs the custom LZ77 decoder (opcodes listed in `vega_compression.h`)
   over bytes 16..end until the `05 FA` end-marker,
4. and continues with the *decompressed* buffer as the font data.

With this patch, a plain call works:

```cpp
io.Fonts->AddFontFromMemoryTTF((void*)UBUNTU_57BC_BLOB, 0x3A1E8, 18.0f);
```

The blob used by the original for this call lives at file offset
`0x524E0` (`.rdata`) and decodes to **Ubuntu-Regular.ttf, 298 928 bytes**.

> The custom LZ decoder in the binary keeps the stb naming style
> (`stb__dout`, `stb__barrier_out_e`) — it was written as a replacement of
> stb's zlib output stage, using four module-level pointers stored at
> `0x140092F88..0x140092FA0`.

## Patch 2 — `ImFontAtlas::AddFontDefault` embeds the Base85 font

At `0x140024D40` the binary's `AddFontDefault` still formats the classic
label `ImFormatString(cfg.Name, 40, "ProggyClean.ttf, %dpx", (int)px)` but
**feeds it the Base85-encoded, LZ-compressed ProggyCleanTT.ttf instead of
ImGui's stock binary blob**, through a helper at `0x140025790` that:

1. `strlen`s the string (11 980 chars → 9 568 decoded bytes),
2. decodes the custom Base85 variant (alphabet `#..x` minus `\`, first char
   of each 5-group is the *least* significant digit, bytes little-endian),
3. hands the resulting 57BC container to the patched `AddFont`.

The Base85 string lives at file offset `0x4F560` (`.rdata`, VA `0x140050160`)
and decodes to **ProggyCleanTT.ttf, 41 208 bytes** — re-encoding it with
`tools/pack_assets.py` reproduces the original string byte-for-byte.

Because `main()` adds the Ubuntu font *before* the atlas is first built, this
default-font path only ever runs as a fallback (no fonts yet) — in practice
**Ubuntu Regular 18 px is the only font that renders**, and the ProggyClean
blob is dead data, exactly as shipped.

---

## Custom font-texture creation

`0x140042C60` (called when the font texture global `0x140092FA8` is null, i.e.
on first frame and after device resets) replaces
`ImGui_ImplDX9_CreateFontTexture`:

```cpp
unsigned char* alpha = atlas->TexPixelsAlpha8;      // after atlas->Build()
// rgba[i] = (alpha[i] << 24) | 0x00FFFFFF;         // white, alpha from glyph
device->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT, &g_FontTexture, nullptr);
// LockRect + row-by-row memcpy with pitch
```

## Clipboard functions

`io.SetClipboardTextFn` / `io.GetClipboardTextFn` point at the standard
`imgui_impl_win32.cpp` handlers (`0x140087B20` / `0x140087B10` in the binary's
layout) — i.e. the Win32 backend was initialized as usual.
