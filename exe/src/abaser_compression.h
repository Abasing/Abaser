// vega_compression.h — Vega-Clicker custom asset compression
// ----------------------------------------------------------------------------
// Reverse-engineered from "They_Key_is_in_source_code_you_have_to_decompile_it.exe"
//
// The original binary embedded its fonts in a proprietary two-layer format:
//
//   Layer 1: "57BC" container
//            u32 BE  magic   = 0x57BC0000
//            u32 BE  flags   = 0
//            u32 BE  rawSize = decompressed byte count (allocation size)
//            u32 BE  field2  = 0x00040000 (constant)
//            then the custom LZ77 token stream.
//
//   Layer 2: custom Base85 (ASCII85 variant) for the ProggyClean blob.
//            Alphabet: 0x23..0x78 excluding 0x5C ('\'), value = c - 0x23 - (c >= 0x5C).
//            Groups of 5 chars -> 4 bytes. NOTE: the first character of a group is
//            the LEAST significant digit (reverse of standard Ascii85) and the 4
//            bytes are emitted little-endian.
//
// LZ77 token stream opcodes (first byte b0):
//   0x05 0xFA                       END marker
//   0x06 b1 b2 b3 b4                match  dist = (b1<<16|b2<<8|b3)+1        len = b4+1
//   0x07 lenhi lenlo <bytes>        literal len = (b1<<8|b2)+1
//   0x08..0x0F b1 <bytes>           literal len = ((b0-8)<<8|b1)+1
//   0x10..0x17 b1 b2 b3 b4          match  dist = ((b0-0x10)<<16|b1<<8|b2)+1  len = (b3<<8|b4)+1
//   0x18..0x1F b1 b2 b3             match  dist = ((b0-0x18)<<16|b1<<8|b2)+1  len = b3+1
//   0x20..0x3F <bytes>              literal len = b0-0x1F   (1..32)
//   0x40..0x7F b1 b2                match  dist = ((b0-0x40)<<8|b1)+1         len = b2+1
//   0x80..0xFF b1                   match  dist = b1+1                        len = b0-0x7F
//
// This decoder was verified byte-for-byte against both blobs embedded in the
// original executable (Ubuntu-Regular.ttf and ProggyCleanTT.ttf).
// ----------------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>

namespace abaser {

// ---------------------------------------------------------------------------
// Custom Base85 decode (see header comment for the exact variant).
// ---------------------------------------------------------------------------
inline bool b85_decode(const char* src, std::vector<uint8_t>& out)
{
    size_t n = strlen(src);
    if (n % 5 != 0) return false;              // original assumes multiples of 5
    out.clear();
    out.reserve((n / 5) * 4);
    for (size_t i = 0; i < n; i += 5) {
        uint32_t v = 0;
        for (int j = 4; j >= 0; j--) {         // first char = least significant
            uint8_t c = (uint8_t)src[i + j];
            uint32_t d;
            if (c < 0x23) return false;
            d = c - 0x23 - (c >= 0x5C ? 1 : 0);
            if (d > 84) return false;
            v = v * 85 + d;
        }
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Custom LZ77 decode.
// ---------------------------------------------------------------------------
inline bool lz_decode(const uint8_t* in, size_t in_size, uint8_t* out, size_t out_size, size_t& produced)
{
    size_t wp = 0, p = 0;
    while (p < in_size) {
        uint8_t b0 = in[p];
        if (b0 == 0x05) {                       // END marker
            if (p + 1 >= in_size || in[p + 1] != 0xFA) return false;
            produced = wp;
            return true;
        }
        if (b0 == 0x06) {
            if (p + 5 > in_size || wp < 1) return false;
            size_t dist = ((size_t)in[p+1] << 16 | in[p+2] << 8 | in[p+3]) + 1;
            size_t len  = in[p+4] + 1;
            if (wp < dist || wp + len > out_size) return false;
            for (size_t k = 0; k < len; k++) { out[wp] = out[wp - dist]; wp++; }
            p += 5;
        } else if (b0 == 0x07) {
            if (p + 3 > in_size) return false;
            size_t len = ((size_t)in[p+1] << 8 | in[p+2]) + 1;
            if (p + 3 + len > in_size || wp + len > out_size) return false;
            memcpy(out + wp, in + p + 3, len); wp += len; p += 3 + len;
        } else if (b0 >= 0x08 && b0 <= 0x0F) {
            if (p + 2 > in_size) return false;
            size_t len = (((size_t)(b0 - 8)) << 8 | in[p+1]) + 1;
            if (p + 2 + len > in_size || wp + len > out_size) return false;
            memcpy(out + wp, in + p + 2, len); wp += len; p += 2 + len;
        } else if (b0 >= 0x10 && b0 <= 0x17) {
            if (p + 5 > in_size) return false;
            size_t dist = (((size_t)(b0 - 0x10)) << 16 | in[p+1] << 8 | in[p+2]) + 1;
            size_t len  = ((size_t)in[p+3] << 8 | in[p+4]) + 1;
            if (wp < dist || wp + len > out_size) return false;
            for (size_t k = 0; k < len; k++) { out[wp] = out[wp - dist]; wp++; }
            p += 5;
        } else if (b0 >= 0x18 && b0 <= 0x1F) {
            if (p + 4 > in_size) return false;
            size_t dist = (((size_t)(b0 - 0x18)) << 16 | in[p+1] << 8 | in[p+2]) + 1;
            size_t len  = in[p+3] + 1;
            if (wp < dist || wp + len > out_size) return false;
            for (size_t k = 0; k < len; k++) { out[wp] = out[wp - dist]; wp++; }
            p += 4;
        } else if (b0 >= 0x20 && b0 <= 0x3F) {
            size_t len = b0 - 0x1F;
            if (p + 1 + len > in_size || wp + len > out_size) return false;
            memcpy(out + wp, in + p + 1, len); wp += len; p += 1 + len;
        } else if (b0 >= 0x40 && b0 <= 0x7F) {
            if (p + 3 > in_size) return false;
            size_t dist = (((size_t)(b0 - 0x40)) << 8 | in[p+1]) + 1;
            size_t len  = in[p+2] + 1;
            if (wp < dist || wp + len > out_size) return false;
            for (size_t k = 0; k < len; k++) { out[wp] = out[wp - dist]; wp++; }
            p += 3;
        } else {                                // 0x80..0xFF
            if (p + 2 > in_size) return false;
            size_t dist = in[p+1] + 1;
            size_t len  = b0 - 0x7F;
            if (wp < dist || wp + len > out_size) return false;
            for (size_t k = 0; k < len; k++) { out[wp] = out[wp - dist]; wp++; }
            p += 2;
        }
    }
    return false;                                // stream ended without END marker
}

// ---------------------------------------------------------------------------
// "57BC" container decode.
// ---------------------------------------------------------------------------
inline bool decode_57bc(const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    if (size < 16) return false;
    uint32_t magic = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
    if (magic != 0x57BC0000u) return false;
    uint32_t raw_size = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | data[11];
    out.resize(raw_size);
    size_t produced = 0;
    if (!lz_decode(data + 16, size - 16, out.data(), raw_size, produced)) return false;
    out.resize(produced);
    return true;
}

// Decode from an embedded C string (base85 layer first, then 57BC/LZ).
inline bool decode_b85_asset(const char* b85, std::vector<uint8_t>& out)
{
    std::vector<uint8_t> blob;
    if (!b85_decode(b85, blob)) return false;
    return decode_57bc(blob.data(), blob.size(), out);
}

// Decode from an embedded binary array (57BC/LZ only).
inline bool decode_binary_asset(const uint8_t* data, size_t size, std::vector<uint8_t>& out)
{
    return decode_57bc(data, size, out);
}

} // namespace abaser
