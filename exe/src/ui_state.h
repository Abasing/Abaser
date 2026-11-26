// ============================================================================
//  Abaser Clicker — shared UI state & settings
// ----------------------------------------------------------------------------
//  Platform-neutral core shared by the Win32 exe (src/main.cpp) and the
//  headless UI simulator (sim/sim_main.cpp). Nothing here touches Win32.
//
//  Contents:
//    * every user-facing setting (the same keys/values as abaser.ini has
//      always used — old config files keep working)
//    * config load/save (plain key=value text)
//    * the virtual-key name table
//    * small string/number helpers
// ============================================================================
#pragma once

#include "imgui.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <fstream>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// String obfuscation ("the key is in the source code")
// ---------------------------------------------------------------------------
// Every user-visible string is XOR-encrypted at compile time and rebuilt on
// the stack right before use, so the plaintext never sits in the binary.
// (The 48-byte key is the classic's own — kept as a little homage.)
static constexpr uint64_t ABASER_XOR_KEY[6] = {
    0x8118b5b2183014a7ull, 0x9ab7bae35936a050ull, 0x9ca7d1fc536407d9ull,
    0x3201677d57919edaull, 0x5e3342ae6df551fbull, 0x190c352f4c1ed6d4ull,
};

template<size_t N>
struct AbaserStr {
    uint64_t data[(N + 7) / 8] {};   // encrypted storage only
    char     buf[N + 1] {};          // zero-init, filled at runtime
    constexpr AbaserStr(const char (&s)[N]) {
        for (size_t q = 0; q < (N + 7) / 8; q++) {
            uint64_t v = 0;
            for (int b = 7; b >= 0; b--) {
                size_t idx = q * 8 + (size_t)b;
                v = (v << 8) | (uint8_t)(idx < N ? s[idx] : 0);
            }
            data[q] = v ^ ABASER_XOR_KEY[q % 6];
        }
    }
    const char* operator()() {
        for (size_t q = 0; q < (N + 7) / 8; q++) {
            uint64_t v = data[q] ^ ABASER_XOR_KEY[q % 6];
            for (int b = 0; b < 8; b++)
                if (q * 8 + (size_t)b < N)
                    buf[q * 8 + b] = (char)((v >> (8 * b)) & 0xFF);
        }
        buf[N] = 0;
        return buf;
    }
};

#define VSTR(s) ([]() { static AbaserStr<sizeof(s)> _v(s); return _v(); })()

// --- theme colors (Discord-style) -------------------------------------------
// Two colors, each stored as a plain 24-bit RGB integer ((R<<16)+(G<<8)+B),
// exactly the way Discord stores profile theme_colors [primary, accent].
// Everything else on screen derives from these two ints:
//   * the window background is a vertical gradient of the primary
//     (Discord's --profile-gradient-primary/secondary-color),
//   * the interactive accent drives switches, sliders and highlights,
//   * knob/glyph colors are picked by relative luminance so they stay
//     readable on any color the user picks (Discord's luminance clamp).
inline unsigned int theme_primary = 0xFF3899u;   // profile gradient base
inline unsigned int theme_accent  = 0xFF3899u;   // interactive accent

// legacy ImVec4 mirror of theme_accent (the old config key / engine colors)
inline ImVec4 accent = ImVec4(1.00f, 0.22f, 0.55f, 1.00f);

inline unsigned int RgbFromIm4(const ImVec4& c)
{
    const unsigned r = (unsigned)(ImClamp(c.x, 0.0f, 1.0f) * 255.0f + 0.5f);
    const unsigned g = (unsigned)(ImClamp(c.y, 0.0f, 1.0f) * 255.0f + 0.5f);
    const unsigned b = (unsigned)(ImClamp(c.z, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (r << 16) | (g << 8) | b;
}
inline ImVec4 Im4FromRgb(unsigned int rgb)
{
    return ImVec4(((rgb >> 16) & 0xFFu) / 255.0f,
                  ((rgb >> 8)  & 0xFFu) / 255.0f,
                  ( rgb        & 0xFFu) / 255.0f, 1.0f);
}
inline void SyncAccentFromTheme() { accent = Im4FromRgb(theme_accent); }

inline ImVec4 ShadeColor(const ImVec4& c, float f)
{
    return ImVec4(ImClamp(c.x * f, 0.0f, 1.0f), ImClamp(c.y * f, 0.0f, 1.0f), ImClamp(c.z * f, 0.0f, 1.0f), c.w);
}
inline ImVec4 MixColor(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}
// relative luminance (WCAG-ish) + the Discord trick: pick dark or light text
// for anything drawn on a user color so it is readable on any theme
inline float Luma01(const ImVec4& c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }
inline ImVec4 OnColor(const ImVec4& bg) { return Luma01(bg) > 0.55f ? ImVec4(0.10f, 0.11f, 0.14f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); }

// --- live state from the companion mod ---------------------------------------
// The mod feeds this over the stdin link (see main.cpp ModLinkThread). When
// the exe runs standalone, everything stays false — the gates are wide open.
//
// THREAD MODEL: these globals are touched by four threads (mod link writes
// them, the clicker engine evaluates them, the UI displays them, the status
// streamer reads them). Plain accesses across threads are a C++ data race,
// so every read/write below goes through g_state_mutex. The mutex is
// RECURSIVE on purpose — the UI frame holds it for its whole pass and calls
// LoadConfig/SaveConfig, which lock it again.
inline std::recursive_mutex g_state_mutex;

inline bool mod_gui_open    = false;   // any in-game GUI/screen is open
inline bool mod_holding_bow = false;   // the player is holding a bow
inline bool mod_in_spawn    = false;   // the player stands inside the spawn box
inline bool mod_ally_crosshair = false; // a team member / excepted player is under the crosshair

// --- slider limits ----------------------------------------------------------
static const float CPS_LO = 1.0f,  CPS_HI = 25.0f;
static const int   CHANCE_LO = 1,  CHANCE_HI = 100;
static const int   BH_LO = 1,      BH_HI = 25;

// --- every user-facing setting ---------------------------------------------
inline int    left_bind        = 115;    // VK_F4 — both sides on one key, classic default
inline int    right_bind       = 115;    // VK_F4
inline float  left_min_cps     = 9.6f;
inline float  left_max_cps     = 14.0f;
inline float  right_min_cps    = 13.0f;
inline float  right_max_cps    = 20.0f;
inline int    hit_chance       = 87;     // left connect chance, %
inline int    right_chance     = 80;     // right connect chance, %
inline bool   blockhit         = false;
inline int    blockhit_chance  = 6;
inline bool   hold_mode        = true;   // click only while the physical button is held
inline bool   require_focus    = true;   // no clicks into an unfocused game window
inline bool   legacy_timing    = false;  // reproduce the classic's ~2x CPS math
inline bool   spawn_gate       = true;   // hold all clicks while the mod reports "inside spawn"
inline bool   other_background = true;   // "dim backdrop" — darkens the gradient
inline int    hide_bind        = 45;     // VK_INSERT

inline bool   left_enabled     = false;
inline bool   right_enabled    = false;
inline bool   window_hidden    = false;
inline int    g_tab            = 0;

// The click gates — the single policy both the engine and the simulator test:
//   * an open GUI blocks EVERYTHING (inventory, chest, villager, enderchest,
//     chat, pause menu — any screen at all),
//   * inside the spawn box (when the spawn_gate setting is on) blocks
//     EVERYTHING — the mod measures the box and sends "spawn":true,
//   * an ally under the crosshair (team member, peace list or a manual
//     exception — the mod decides) blocks EVERYTHING so you never hit friends
//     while spamming,
//   * a drawn bow blocks every RIGHT-click emission (the right autoclicker
//     and Block Hit's sword-block taps) so charging feels normal; left clicks
//     are untouched.
// Each gate takes the state mutex — the policy is unchanged, but now the
// clicker thread can evaluate it safely while the mod link writes the flags.
inline bool GateLeft(bool base_ok, bool hold_key_down)
{
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    return base_ok && !mod_gui_open && !(spawn_gate && mod_in_spawn)
        && !mod_ally_crosshair && (!hold_mode || hold_key_down);
}
inline bool GateRight(bool base_ok, bool hold_key_down)
{
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    return base_ok && !mod_gui_open && !(spawn_gate && mod_in_spawn)
        && !mod_ally_crosshair && !mod_holding_bow && (!hold_mode || hold_key_down);
}
inline bool GateBlockHit()
{
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    return !mod_gui_open && !(spawn_gate && mod_in_spawn)
        && !mod_ally_crosshair && !mod_holding_bow;
}

// --- click timing (engine + simulator share this) ----------------------------
// One click's timing from two random draws in [0,1): how long the button is
// held (press) and the gap from release to the next click. legacy_timing
// reproduces the classic's ~2x CPS math: the press is 100/cps long and the
// gap takes a FRESH draw — 300/cps2 when that draw is hot (cps2 > 14) else
// 400/cps2. Normal mode: 2 ms press, the remainder of 1000/cps as the gap.
//
// min_cps/max_cps MUST come from the same channel as the caller: the legacy
// right-click gap used to read left_max_cps here (a one-word bug that
// silently changed hot right-clickers' rhythm), which is why the math now
// lives in one tested function instead of two copies in the engine.
inline void ClickTiming(bool legacy, float min_cps, float max_cps,
                        float rnd_press, float rnd_gap,
                        long long* press_ms, long long* gap_ms)
{
    const float cps = min_cps + (max_cps - min_cps) * rnd_press;
    *press_ms = legacy ? (long long)(100.0f / cps) : 2;
    if (legacy) {
        const float cps2 = min_cps + (max_cps - min_cps) * rnd_gap;
        const int gap = (cps2 > 14.0f) ? 300 : 400;
        *gap_ms = (long long)(gap / cps2);
    } else {
        const long long total = (long long)(1000.0f / cps);
        *gap_ms = total > 2 ? total - 2 : 1;
    }
}

// Lifetime counters — the engine increments them, the UI resets and prints
// them: atomic so neither side can tear a 64-bit write.
inline std::atomic<unsigned long long> g_left_total  = 0;   // lifetime counters (engine-side)
inline std::atomic<unsigned long long> g_right_total = 0;

// small shared status line (save/reload/autostart feedback), UI-shown
inline char   g_config_status[192] = "settings load at startup and save on exit";

// --- helpers ----------------------------------------------------------------
inline int   ClampI(int v, int lo, int hi)       { return v < lo ? lo : (v > hi ? hi : v); }
inline float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline std::string TrimStr(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// --- virtual-key name table (recovered from the classic's .rdata) -----------
inline const char* GetKeyName(int vk)
{
    switch (vk) {
        case 5: return "MOUSE4";
        case 6: return "MOUSE5";
        case 8: return "BACKSPACE";
        case 9: return "TAB";
        case 12: return "CLEAR";
        case 13: return "ENTER";
        case 32: return "SPACE";
        case 33: return "PGUP";
        case 34: return "PGDOWN";
        case 35: return "END";
        case 45: return "INSERT";
        case 46: return "DELETE";
        case 48: return "0";
        case 49: return "1";
        case 50: return "2";
        case 51: return "3";
        case 52: return "4";
        case 53: return "5";
        case 54: return "6";
        case 55: return "7";
        case 56: return "8";
        case 57: return "9";
        case 65: return "A";
        case 66: return "B";
        case 67: return "C";
        case 68: return "D";
        case 69: return "E";
        case 70: return "F";
        case 71: return "G";
        case 72: return "H";
        case 73: return "I";
        case 74: return "J";
        case 75: return "K";
        case 76: return "L";
        case 77: return "M";
        case 78: return "N";
        case 79: return "O";
        case 80: return "P";
        case 81: return "Q";
        case 82: return "R";
        case 83: return "S";
        case 84: return "T";
        case 85: return "U";
        case 86: return "V";
        case 87: return "W";
        case 88: return "X";
        case 89: return "Y";
        case 90: return "Z";
        case 96: return "NUM0";
        case 97: return "NUM1";
        case 98: return "NUM2";
        case 99: return "NUM3";
        case 100: return "NUM4";
        case 101: return "NUM5";
        case 102: return "NUM6";
        case 103: return "NUM7";
        case 104: return "NUM8";
        case 105: return "NUM9";
        case 106: return "MULTIPLY";
        case 107: return "ADD";
        case 108: return "SEPARATOR";
        case 109: return "SUBTRACT";
        case 110: return "DECIMAL";
        case 111: return "DIVIDE";
        case 112: return "F1";
        case 113: return "F2";
        case 114: return "F3";
        case 115: return "F4";
        case 116: return "F5";
        case 117: return "F6";
        case 118: return "F7";
        case 119: return "F8";
        case 120: return "F9";
        case 121: return "F10";
        case 122: return "F11";
        case 123: return "F12";
        case 124: return "F13";
        case 125: return "F14";
        case 126: return "F15";
        case 127: return "F16";
        case 128: return "F17";
        case 129: return "F18";
        case 130: return "F19";
        case 131: return "F20";
        case 132: return "F21";
        case 133: return "F22";
        case 134: return "F23";
        case 135: return "F24";
        case 160: return "LSHIFT";
        case 161: return "RSHIFT";
        case 162: return "LCONTROL";
        case 163: return "RCONTROL";
        case 164: return "LMENU";
        case 165: return "RMENU";
        case 188: return "COMMA";
        case 189: return "MINUS";
        case 190: return "PERIOD";
        case 191: return "SLASH";
        case 192: return "TILDA";
        case 219: return "[";
        case 220: return "|";
        case 221: return "]";
        case 222: return "QUOTE";
        default: return "";
    }
}

// --- settings ---------------------------------------------------------------
inline void ResetDefaults()
{
    left_bind        = 115;
    right_bind       = 115;
    hide_bind        = 45;
    left_min_cps     = 9.6f;
    left_max_cps     = 14.0f;
    right_min_cps    = 13.0f;
    right_max_cps    = 20.0f;
    hit_chance       = 87;
    right_chance     = 80;
    blockhit         = false;
    blockhit_chance  = 6;
    hold_mode        = true;
    require_focus    = true;
    legacy_timing    = false;
    spawn_gate       = true;
    other_background = true;
    theme_primary    = 0xFF3899u;
    theme_accent     = 0xFF3899u;
    SyncAccentFromTheme();
    left_enabled     = false;
    right_enabled    = false;
}

// Callers must hold g_state_mutex (LoadConfig, ApplyJsonConfig, the UI frame
// all do). Keeping the lock out of here lets callers batch several writes
// into one critical section.
inline void ApplyConfigKey(const std::string& key, const std::string& val)
{
    const char* v = val.c_str();
    if      (key == "left_bind")        left_bind        = ClampI(atoi(v), 0, 255);
    else if (key == "right_bind")       right_bind       = ClampI(atoi(v), 0, 255);
    else if (key == "hide_bind")        hide_bind        = ClampI(atoi(v), 0, 255);
    else if (key == "left_min_cps")     left_min_cps     = ClampF((float)atof(v), CPS_LO, CPS_HI);
    else if (key == "left_max_cps")     left_max_cps     = ClampF((float)atof(v), CPS_LO, CPS_HI);
    else if (key == "right_min_cps")    right_min_cps    = ClampF((float)atof(v), CPS_LO, CPS_HI);
    else if (key == "right_max_cps")    right_max_cps    = ClampF((float)atof(v), CPS_LO, CPS_HI);
    else if (key == "hit_chance")       hit_chance       = ClampI(atoi(v), CHANCE_LO, CHANCE_HI);
    else if (key == "right_chance")     right_chance     = ClampI(atoi(v), CHANCE_LO, CHANCE_HI);
    else if (key == "blockhit")         blockhit         = (val == "true" || val == "1");
    else if (key == "blockhit_chance")  blockhit_chance  = ClampI(atoi(v), BH_LO, BH_HI);
    else if (key == "hold_mode")        hold_mode        = (val == "true" || val == "1");
    else if (key == "require_focus")    require_focus    = (val == "true" || val == "1");
    else if (key == "legacy_timing")    legacy_timing    = (val == "true" || val == "1");
    else if (key == "spawn_gate")       spawn_gate       = (val == "true" || val == "1");
    else if (key == "other_background") other_background = (val == "true" || val == "1");
    else if (key == "left_enabled")     left_enabled     = (val == "true" || val == "1");
    else if (key == "right_enabled")    right_enabled    = (val == "true" || val == "1");
    else if (key == "theme_primary")    { theme_primary = (unsigned int)strtoul(v, nullptr, 10); SyncAccentFromTheme(); }
    else if (key == "theme_accent")     { theme_accent  = (unsigned int)strtoul(v, nullptr, 10); SyncAccentFromTheme(); }
    else if (key == "accent") {
        // legacy per-float key (pre-theme builds) — feeds the accent int
        float r = 1.0f, g = 0.22f, b = 0.55f, a = 1.0f;
        if (sscanf(v, "%f %f %f %f", &r, &g, &b, &a) == 4) {
            theme_accent = RgbFromIm4(ImVec4(r, g, b, 1.0f));
            accent = ImVec4(r, g, b, a);
        }
    }
}

inline bool LoadConfig(const std::string& path)
{
    static std::mutex s_cfg_mutex;   // the file itself: UI + mod link + startup
    std::lock_guard<std::mutex> file_lock(s_cfg_mutex);
    std::lock_guard<std::recursive_mutex> state_lock(g_state_mutex);
    std::ifstream in(path.c_str());
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = TrimStr(line.substr(0, eq));
        const std::string val = TrimStr(line.substr(eq + 1));
        if (key.empty() || key[0] == '#') continue;
        ApplyConfigKey(key, val);
    }
    return true;
}

inline bool SaveConfig(const std::string& path)
{
    static std::mutex s_cfg_mutex;   // same file can be saved by UI + mod link
    std::lock_guard<std::mutex> file_lock(s_cfg_mutex);
    std::lock_guard<std::recursive_mutex> state_lock(g_state_mutex);
    std::ofstream out(path.c_str());
    if (!out) return false;
    out << "# Abaser Clicker settings (loaded at startup, saved on exit)\n";
    out << "left_bind = "        << left_bind        << "\n";
    out << "right_bind = "       << right_bind       << "\n";
    out << "hide_bind = "        << hide_bind        << "\n";
    out << "left_min_cps = "     << left_min_cps     << "\n";
    out << "left_max_cps = "     << left_max_cps     << "\n";
    out << "right_min_cps = "    << right_min_cps    << "\n";
    out << "right_max_cps = "    << right_max_cps    << "\n";
    out << "hit_chance = "       << hit_chance       << "\n";
    out << "right_chance = "     << right_chance     << "\n";
    out << "blockhit = "         << (blockhit ? "true" : "false")        << "\n";
    out << "blockhit_chance = "  << blockhit_chance  << "\n";
    out << "hold_mode = "        << (hold_mode ? "true" : "false")       << "\n";
    out << "require_focus = "    << (require_focus ? "true" : "false")   << "\n";
    out << "legacy_timing = "    << (legacy_timing ? "true" : "false")   << "\n";
    out << "spawn_gate = "       << (spawn_gate ? "true" : "false")      << "\n";
    out << "other_background = " << (other_background ? "true" : "false") << "\n";
    out << "left_enabled = "     << (left_enabled ? "true" : "false")    << "\n";
    out << "right_enabled = "    << (right_enabled ? "true" : "false")   << "\n";
    out << "# theme colors, stored like Discord: ((R<<16)+(G<<8)+B)\n";
    out << "theme_primary = "    << theme_primary    << "\n";
    out << "theme_accent = "     << theme_accent     << "\n";
    return true;
}

// --- the in-game panel (remote config over the mod link) ---------------------
//
// The companion mod can open a small panel in-game and edit the SAME settings
// this exe edits. Every change arrives as one JSON line on stdin:
//
//     {"config":{"left_max_cps":13.5,"hit_chance":90,"theme_accent":9332727}}
//
// Keys use the exact abaser.ini names, so ApplyConfigKey applies them with the
// same clamps as the on-screen sliders. The exe answers on stdout with the
// canonical snapshot (BuildConfigJson) after every apply, and streams a
// status line every 2 s (BuildStatusJson) so the panel can show live state.
// The mod never sends clicks and never touches the engine — it only turns the
// same knobs the exe's own UI turns.
inline bool JsonExtractValue(const std::string& line, const char* key, std::string* out)
{
    const std::string needle = std::string("\"") + key + "\"";
    const size_t p = line.find(needle);
    if (p == std::string::npos) return false;
    size_t c = line.find(':', p + needle.size());
    if (c == std::string::npos) return false;
    c++;
    while (c < line.size() && (line[c] == ' ' || line[c] == '\t')) c++;
    if (c >= line.size()) return false;
    if (line[c] == '"') {                       // string value
        size_t e = c + 1;
        while (e < line.size() && line[e] != '"') e++;
        if (e >= line.size()) return false;
        *out = line.substr(c + 1, e - c - 1);
        return true;
    }
    size_t e = c;                               // number / true / false
    while (e < line.size() && line[e] != ',' && line[e] != '}' && line[e] != ' ' && line[e] != '\t') e++;
    if (e == c) return false;
    *out = line.substr(c, e - c);
    return true;
}

// Every setting the panel may set — exactly the abaser.ini key set.
static const char* const ABASER_CONFIG_KEYS[] = {
    "left_bind", "right_bind", "hide_bind",
    "left_min_cps", "left_max_cps", "right_min_cps", "right_max_cps",
    "hit_chance", "right_chance",
    "blockhit", "blockhit_chance",
    "hold_mode", "require_focus", "legacy_timing", "spawn_gate", "other_background",
    "left_enabled", "right_enabled",
    "theme_primary", "theme_accent",
};
static const int ABASER_CONFIG_KEY_COUNT =
    (int)(sizeof(ABASER_CONFIG_KEYS) / sizeof(ABASER_CONFIG_KEYS[0]));

// Apply a "config" JSON line. Returns how many keys it carried.
inline int ApplyJsonConfig(const std::string& line)
{
    int applied = 0;
    std::string val;
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    for (const char* k : ABASER_CONFIG_KEYS)
        if (JsonExtractValue(line, k, &val)) { ApplyConfigKey(k, val); applied++; }
    return applied;
}

// The canonical settings snapshot the exe sends back after every apply.
inline std::string BuildConfigJson()
{
    char num[64];
    std::string o = "{\"config\":{";
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    for (int i = 0; i < ABASER_CONFIG_KEY_COUNT; i++) {
        const char* k = ABASER_CONFIG_KEYS[i];
        if      (!strcmp(k, "left_bind"))       snprintf(num, sizeof(num), "%d", left_bind);
        else if (!strcmp(k, "right_bind"))      snprintf(num, sizeof(num), "%d", right_bind);
        else if (!strcmp(k, "hide_bind"))       snprintf(num, sizeof(num), "%d", hide_bind);
        else if (!strcmp(k, "left_min_cps"))    snprintf(num, sizeof(num), "%g", (double)left_min_cps);
        else if (!strcmp(k, "left_max_cps"))    snprintf(num, sizeof(num), "%g", (double)left_max_cps);
        else if (!strcmp(k, "right_min_cps"))   snprintf(num, sizeof(num), "%g", (double)right_min_cps);
        else if (!strcmp(k, "right_max_cps"))   snprintf(num, sizeof(num), "%g", (double)right_max_cps);
        else if (!strcmp(k, "hit_chance"))      snprintf(num, sizeof(num), "%d", hit_chance);
        else if (!strcmp(k, "right_chance"))    snprintf(num, sizeof(num), "%d", right_chance);
        else if (!strcmp(k, "blockhit"))        snprintf(num, sizeof(num), "%s", blockhit ? "true" : "false");
        else if (!strcmp(k, "blockhit_chance")) snprintf(num, sizeof(num), "%d", blockhit_chance);
        else if (!strcmp(k, "hold_mode"))       snprintf(num, sizeof(num), "%s", hold_mode ? "true" : "false");
        else if (!strcmp(k, "require_focus"))   snprintf(num, sizeof(num), "%s", require_focus ? "true" : "false");
        else if (!strcmp(k, "legacy_timing"))   snprintf(num, sizeof(num), "%s", legacy_timing ? "true" : "false");
        else if (!strcmp(k, "spawn_gate"))      snprintf(num, sizeof(num), "%s", spawn_gate ? "true" : "false");
        else if (!strcmp(k, "other_background")) snprintf(num, sizeof(num), "%s", other_background ? "true" : "false");
        else if (!strcmp(k, "left_enabled"))    snprintf(num, sizeof(num), "%s", left_enabled ? "true" : "false");
        else if (!strcmp(k, "right_enabled"))   snprintf(num, sizeof(num), "%s", right_enabled ? "true" : "false");
        else if (!strcmp(k, "theme_primary"))   snprintf(num, sizeof(num), "%u", theme_primary);
        else if (!strcmp(k, "theme_accent"))    snprintf(num, sizeof(num), "%u", theme_accent);
        else continue;
        o += std::string("\"") + k + "\":" + num;
        if (i + 1 < ABASER_CONFIG_KEY_COUNT) o += ',';
    }
    o += "}}";
    return o;
}

// Live-state line, streamed every 2 s while the link is up.
inline std::string BuildStatusJson(bool mc_found, float cps_l, float cps_r)
{
    char buf[256];
    std::lock_guard<std::recursive_mutex> lk(g_state_mutex);
    snprintf(buf, sizeof(buf),
        "{\"status\":{\"left_on\":%s,\"right_on\":%s,\"cps_l\":%.1f,\"cps_r\":%.1f,"
        "\"gui\":%s,\"spawn\":%s,\"ally\":%s,\"bow\":%s,\"mc\":%s}}",
        left_enabled ? "true" : "false", right_enabled ? "true" : "false",
        (double)cps_l, (double)cps_r,
        mod_gui_open ? "true" : "false",
        mod_in_spawn ? "true" : "false",
        mod_ally_crosshair ? "true" : "false",
        mod_holding_bow ? "true" : "false",
        mc_found ? "true" : "false");
    return buf;
}
