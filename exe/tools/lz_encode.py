#!/usr/bin/env python3
"""LZ compressor matching the custom format found in the binary.
Token format (from disassembly):
  0x80-0xFF b1            : match dist=b1+1,         len=b0-0x7F          (2B)
  0x40-0x7F b1 b2         : match dist=((b0-0x40)<<8|b1)+1, len=b2+1      (3B)
  0x20-0x3F <lit>         : literal run len=b0-0x1F                        (1B+len)
  0x18-0x1F b1 b2 b3      : match dist=((b0-0x18)<<16|b1<<8|b2)+1, len=b3+1  (4B)
  0x10-0x17 b1 b2 b3 b4   : match dist=((b0-0x10)<<16|b1<<8|b2)+1, len=(b3<<8|b4)+1 (5B)
  0x08-0x0F b1 <lit>      : literal len=((b0-8)<<8|b1)+1                   (2B+len)
  0x07 b1 b2 <lit>        : literal len=(b1<<8|b2)+1                       (3B+len)
  0x06 b1 b2 b3 b4        : match dist=(b1<<16|b2<<8|b3)+1, len=b4+1       (5B)
  0x05 b1 b2 b3 b4 b5     : match dist=(b1<<16|b2<<8|b3)+1, len=(b4<<8|b5)+1 (6B)
  0x05 0xFA               : END marker
Greedy encoder with hash-chain match search. Verifies round-trip.
"""
import struct, sys

MAX_DIST = 0x1000000
MAX_LEN = 0x10000

def compress(data):
    n = len(data)
    out = bytearray()
    # hash table of 3-byte sequences -> list of positions
    from collections import defaultdict
    table = defaultdict(list)
    i = 0
    def emit_literal(chunk):
        L = len(chunk)
        p = 0
        while L > 0:
            if L <= 32:
                out.append(0x1F + L); out.extend(chunk[p:p+L]); L = 0
            elif L <= 2048:
                b0 = 8 + ((L - 1) >> 8); b1 = (L - 1) & 0xFF
                out.append(b0); out.append(b1); out.extend(chunk[p:p+L]); L = 0
            else:
                take = 2048
                b0 = 8 + ((take - 1) >> 8); b1 = (take - 1) & 0xFF
                out.append(b0); out.append(b1); out.extend(chunk[p:p+take])
                p += take; L -= take
    def emit_match(dist, length):
        while length > 0:
            if dist <= 256 and length <= 128:
                out.append(0x7F + length); out.append(dist - 1); length = 0
            elif dist <= 16384 and length <= 256:
                d = dist - 1
                out.append(0x40 + (d >> 8)); out.append(d & 0xFF); out.append(length - 1); length = 0
            elif dist <= 524288 and length <= 256:
                d = dist - 1
                out.append(0x18 + (d >> 16)); out.append((d >> 8) & 0xFF); out.append(d & 0xFF); out.append(length - 1); length = 0
            elif dist <= 524288:
                # len up to 65536 via 0x10-0x17
                take = min(length, 0x10000)
                d = dist - 1
                out.append(0x10 + (d >> 16)); out.append((d >> 8) & 0xFF); out.append(d & 0xFF)
                out.append((take - 1) >> 8); out.append((take - 1) & 0xFF)
                length -= take
            else:
                take = min(length, 0x10000)
                d = dist - 1
                out.append(0x05); out.append((d >> 16) & 0xFF); out.append((d >> 8) & 0xFF); out.append(d & 0xFF)
                out.append((take - 1) >> 8); out.append((take - 1) & 0xFF)
                length -= take
    lit_start = 0
    while i < n:
        best_len = 0; best_dist = 0
        if i + 3 <= n:
            key = data[i:i+3]
            for j in reversed(table[key]):
                if i - j > MAX_DIST: break
                # find match length
                l = 0
                maxl = min(n - i, MAX_LEN)
                while l < maxl and data[j + l] == data[i + l]:
                    l += 1
                if l > best_len:
                    best_len = l; best_dist = i - j
                    if l >= 1024: break
            table[key].append(i)
        if best_len >= 4:
            if lit_start < i:
                emit_literal(data[lit_start:i])
            emit_match(best_dist, best_len)
            # register hashes inside match
            for k in range(i + 1, min(i + best_len, n - 2)):
                table[data[k:k+3]].append(k)
            i += best_len
            lit_start = i
        else:
            i += 1
    if lit_start < n:
        emit_literal(data[lit_start:n])
    out.append(0x05); out.append(0xFA)  # END marker
    return bytes(out)

def decompress(stream, out_size):
    out = bytearray(out_size)
    wp = 0
    p = 0
    nn = len(stream)
    while p < nn:
        b0 = stream[p]
        if b0 == 5:
            assert stream[p+1] == 0xFA, "bad END"
            return bytes(out[:wp]), p + 2
        elif b0 == 6:
            dist = ((stream[p+1] << 16) | (stream[p+2] << 8) | stream[p+3]) + 1
            length = stream[p+4] + 1; p += 5
            for k in range(length): out[wp] = out[wp - dist]; wp += 1
        elif b0 == 7:
            length = ((stream[p+1] << 8) | stream[p+2]) + 1
            out[wp:wp+length] = stream[p+3:p+3+length]; wp += length; p += 3 + length
        elif 8 <= b0 <= 0xF:
            length = (((b0 - 8) << 8) | stream[p+1]) + 1
            out[wp:wp+length] = stream[p+2:p+2+length]; wp += length; p += 2 + length
        elif 0x10 <= b0 <= 0x17:
            dist = (((b0 - 0x10) << 16) | (stream[p+1] << 8) | stream[p+2]) + 1
            length = ((stream[p+3] << 8) | stream[p+4]) + 1; p += 5
            for k in range(length): out[wp] = out[wp - dist]; wp += 1
        elif 0x18 <= b0 <= 0x1F:
            dist = (((b0 - 0x18) << 16) | (stream[p+1] << 8) | stream[p+2]) + 1
            length = stream[p+3] + 1; p += 4
            for k in range(length): out[wp] = out[wp - dist]; wp += 1
        elif 0x20 <= b0 <= 0x3F:
            length = b0 - 0x1F
            out[wp:wp+length] = stream[p+1:p+1+length]; wp += length; p += 1 + length
        elif 0x40 <= b0 <= 0x7F:
            dist = (((b0 - 0x40) << 8) | stream[p+1]) + 1
            length = stream[p+2] + 1; p += 3
            for k in range(length): out[wp] = out[wp - dist]; wp += 1
        else:
            dist = stream[p+1] + 1
            length = b0 - 0x7F; p += 2
            for k in range(length): out[wp] = out[wp - dist]; wp += 1
    raise ValueError("no END marker")

def make_57bc(raw):
    comp = compress(raw)
    hdr = struct.pack(">IIII", 0x57BC0000, 0, len(raw), 0x40000)
    return hdr + comp

if __name__ == "__main__":
    # verify round-trip against both fonts + original binary blobs
    exe = open("/home/z/my-project/upload/They_Key_is_in_source_code_you_have_to_decompile_it.exe", "rb").read()
    ok = True
    for name, raw in [("Ubuntu", open("/home/z/my-project/asset_main.bin","rb").read()),
                      ("ProggyClean", open("/home/z/my-project/asset_b85.bin","rb").read())]:
        blob = make_57bc(raw)
        dec, _ = decompress(blob[16:], len(raw))
        same = dec == raw
        ok &= same
        print(f"{name}: {len(raw)} raw -> {len(blob)} packed (ratio {len(blob)/len(raw):.2%}), roundtrip={'OK' if same else 'FAIL'}")
    # also verify decoder against the ORIGINAL binary streams
    off = 0x140053ce0 - 0x14004c000 + 0x4b400
    orig_stream = exe[off+16:]
    dec, end = decompress(orig_stream, 298928)
    same = dec == open("/home/z/my-project/asset_main.bin","rb").read()
    print(f"Original binary main-font stream: decodes {len(dec)} bytes, END at {end}, matches extraction: {same}")
    b85 = open("/home/z/my-project/blob_decoded.bin","rb").read()
    dec2, end2 = decompress(b85[16:], 41208)
    same2 = dec2 == open("/home/z/my-project/asset_b85.bin","rb").read()
    print(f"Original binary b85-font stream: decodes {len(dec2)} bytes, END at {end2}, matches extraction: {same2}")
    print("\nALL VERIFIED" if (ok and same and same2) else "SOME CHECKS FAILED")
