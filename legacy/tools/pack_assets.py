#!/usr/bin/env python3
"""
pack_assets.py — regenerates src/vega_assets.h for the Vega-Clicker reconstruction.

Pipeline (identical to the original binary, verified byte-for-byte):
  Ubuntu-Regular.ttf  -> 57BC header + custom LZ77 stream      -> C byte array
  ProggyCleanTT.ttf   -> 57BC header + custom LZ77 stream      -> custom Base85 -> C string

Usage:
  python3 tools/pack_assets.py            # packs assets/*.ttf into src/vega_assets.h
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---------------------------- custom LZ77 encoder ---------------------------
MAX_DIST = 0x1000000
MAX_LEN  = 0x10000

def lz_compress(data):
    from collections import defaultdict
    n = len(data)
    out = bytearray()
    table = defaultdict(list)

    def emit_literal(chunk):
        L, p = len(chunk), 0
        while L > 0:
            if L <= 32:
                out.append(0x1F + L); out.extend(chunk[p:p+L]); L = 0
            elif L <= 2048:
                out.append(8 + ((L - 1) >> 8)); out.append((L - 1) & 0xFF)
                out.extend(chunk[p:p+L]); L = 0
            else:
                take = 2048
                out.append(8 + ((take - 1) >> 8)); out.append((take - 1) & 0xFF)
                out.extend(chunk[p:p+take]); p += take; L -= take

    def emit_match(dist, length):
        while length > 0:
            if dist <= 256 and length <= 128:
                out.append(0x7F + length); out.append(dist - 1); length = 0
            elif dist <= 16384 and length <= 256:
                d = dist - 1
                out.append(0x40 + (d >> 8)); out.append(d & 0xFF); out.append(length - 1); length = 0
            elif dist <= 524288 and length <= 256:
                d = dist - 1
                out.append(0x18 + (d >> 16)); out.append((d >> 8) & 0xFF); out.append(d & 0xFF)
                out.append(length - 1); length = 0
            elif dist <= 524288:
                take = min(length, 0x10000); d = dist - 1
                out.append(0x10 + (d >> 16)); out.append((d >> 8) & 0xFF); out.append(d & 0xFF)
                out.append((take - 1) >> 8); out.append((take - 1) & 0xFF); length -= take
            else:
                take = min(length, 0x10000); d = dist - 1
                out.append(0x05); out.append((d >> 16) & 0xFF); out.append((d >> 8) & 0xFF); out.append(d & 0xFF)
                out.append((take - 1) >> 8); out.append((take - 1) & 0xFF); length -= take

    lit_start = i = 0
    while i < n:
        best_len, best_dist = 0, 0
        if i + 3 <= n:
            key = bytes(data[i:i+3])
            for j in reversed(table[key]):
                if i - j > MAX_DIST: break
                l, maxl = 0, min(n - i, MAX_LEN)
                while l < maxl and data[j + l] == data[i + l]: l += 1
                if l > best_len:
                    best_len, best_dist = l, i - j
                    if l >= 1024: break
            table[key].append(i)
        if best_len >= 4:
            if lit_start < i: emit_literal(data[lit_start:i])
            emit_match(best_dist, best_len)
            for k in range(i + 1, min(i + best_len, n - 2)):
                table[bytes(data[k:k+3])].append(k)
            i += best_len; lit_start = i
        else:
            i += 1
    if lit_start < n: emit_literal(data[lit_start:n])
    out.append(0x05); out.append(0xFA)
    return bytes(out)

# ---------------------------- custom Base85 encoder -------------------------
ALPHA = bytes(c for c in range(0x23, 0x79) if c != 0x5C)   # 85 chars, no backslash

def b85_encode(data):
    out = bytearray()
    for i in range(0, len(data) - len(data) % 4, 4):
        v = int.from_bytes(data[i:i+4], "little")
        digits = []
        for _ in range(5):
            digits.append(v % 85); v //= 85
        out += bytes(ALPHA[d] for d in digits)               # least significant first
    return bytes(out)

def make_57bc(raw):
    return struct.pack(">IIII", 0x57BC0000, 0, len(raw), 0x40000) + lz_compress(raw)

# ---------------------------- code generation --------------------------------
def c_string_literal(b):
    # split into C string literal chunks of 100 chars
    parts = []
    s = b.decode("ascii")
    for i in range(0, len(s), 100):
        parts.append('    "%s"' % s[i:i+100])
    return "\n".join(parts)

def c_array_literal(b):
    lines = []
    for i in range(0, len(b), 16):
        chunk = ", ".join("0x%02X" % c for c in b[i:i+16])
        lines.append("    " + chunk + ",")
    return "\n".join(lines)

def main():
    ubuntu = open(os.path.join(ROOT, "assets", "Ubuntu-Regular.ttf"), "rb").read()
    proggyclean = open(os.path.join(ROOT, "assets", "ProggyCleanTT.ttf"), "rb").read()

    ubuntu_blob = make_57bc(ubuntu)
    pc_blob = make_57bc(proggyclean)
    pc_b85 = b85_encode(pc_blob)

    print(f"Ubuntu-Regular.ttf : {len(ubuntu):>7} -> {len(ubuntu_blob):>7} bytes (57BC+LZ)")
    print(f"ProggyCleanTT.ttf  : {len(proggyclean):>7} -> {len(pc_blob):>7} bytes (57BC+LZ) -> {len(pc_b85)} chars (Base85)")

    # sanity: round-trip
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    from lz_encode import decompress  # decoder proven against the original binary
    assert decompress(ubuntu_blob[16:], len(ubuntu))[0] == ubuntu
    assert decompress(pc_blob[16:], len(proggyclean))[0] == proggyclean
    print("round-trip: OK")

    hdr = """// vega_assets.h — generated by tools/pack_assets.py — do not edit by hand.
// Compressed font blobs in the original "57BC" container format.
// Ubuntu is stored as a raw byte array; ProggyClean is Base85-encoded
// (exactly like the original executable stored it).
#pragma once
#include <stddef.h>
#include <stdint.h>

"""
    body = (f"// Ubuntu-Regular.ttf, {len(ubuntu)} bytes -> {len(ubuntu_blob)} compressed\n"
            f"static const uint8_t VEGA_FONT_UBUNTU[{len(ubuntu_blob)}] = {{\n"
            + c_array_literal(ubuntu_blob) +
            "\n};\n\n"
            f"// ProggyCleanTT.ttf, {len(proggyclean)} bytes -> {len(pc_blob)} compressed -> {len(pc_b85)} Base85 chars\n"
            f"static const char VEGA_FONT_PROGGYCLEAN_B85[{len(pc_b85)} + 1] =\n"
            + c_string_literal(pc_b85) +
            ";\n")

    out_path = os.path.join(ROOT, "src", "vega_assets.h")
    with open(out_path, "w") as f:
        f.write(hdr + body)
    print(f"wrote {out_path}")

if __name__ == "__main__":
    main()
