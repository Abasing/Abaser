// ============================================================================
//  Abaser Clicker
// ----------------------------------------------------------------------------
//  The auto-clicker, back where it belongs: OUTSIDE the game.
//
//  A standalone Windows program (Win32 + Dear ImGui + DirectX 9) that finds
//  the Minecraft window and posts synthetic mouse clicks to it. Because it
//  uses PostMessage against the game window only, it can never click
//  anything else on your desktop — no focus games, no misclicks into your
//  browser, no drivers.
//
//  Lineage: this is an improved rebuild of the classic external clicker
//  recovered in legacy/ (same UI bones, same engine idea, same embedded
//  font pipeline). Improvements over the classic:
//
//    * accurate CPS — the number you set is the number you get (the
//      classic's sloppy delay math made everything run ~2x the label;
//      "Legacy timing" brings that feel back if you miss it)
//    * hold-to-click (classic) or always-click activation, per your taste
//    * optional "only while Minecraft is focused" safety gate
//    * the right-clicker now honors its own chance slider
//    * live CPS readout and click counters
//    * settings actually persist — abaser.ini next to the exe (the
//      classic's "Configs" tab said NOTHING YET; now it says Save)
//    * no imgui.ini litter, no console flash, no shell-outs
//    * system tray — X / INSERT park the window next to the clock while
//      the clickers keep running; the tray tooltip tracks the armed
//      state, left-click shows the window again, right-click opens the
//      menu (arm/disarm both sides, start-with-Windows, Exit)
//    * start with Windows — a plain per-user Run entry (no admin) that
//      launches straight into the tray; a second launch just summons the
//      first instance instead of stacking a duplicate
//
//  Deliberately NOT here (an external exe cannot know them): detecting
//  open inventories/GUIs, held-item checks (bow, sword), anything that
//  needs game-internal state.
// ============================================================================

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"       // ImGui::GetCurrentWindow() for the accent line
#include "abaser_compression.h"
#include "abaser_assets.h"
#include <windows.h>
#include <shellapi.h>             // Shell_NotifyIcon — the tray
#include <d3d9.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "shell32.lib")     // tray icon (MSVC links it by default; explicit for clarity)
#pragma comment(lib, "advapi32.lib")    // registry Run entry for autostart
#endif

#ifndef ABASER_VERSION
#define ABASER_VERSION "dev"
#endif

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

// ---------------------------------------------------------------------------
// Virtual-key name table (recovered from the classic's .rdata; 103 named
// keys, everything else maps to "").
// ---------------------------------------------------------------------------
static const char* GetKeyName(int vk)
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

// ---------------------------------------------------------------------------
// Settings (defaults recovered from the classic's .data section; the new
// options default to their safe/improved values).
// ---------------------------------------------------------------------------
static LPDIRECT3D9           g_pD3D        = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice  = nullptr;
static D3DPRESENT_PARAMETERS g_d3dpp       = {};
static HWND                  g_hWnd        = nullptr;

static int    left_bind        = 115;      // VK_F4 — both sides on one key, classic default
static int    right_bind       = 115;      // VK_F4
static float  left_min_cps     = 9.6f;
static float  left_max_cps     = 14.0f;
static float  right_min_cps    = 13.0f;
static float  right_max_cps    = 20.0f;
static int    hit_chance       = 87;       // left connect chance, %
static int    right_chance     = 80;       // right connect chance, % (now actually used — classic quirk fixed)
static bool   blockhit         = false;
static int    blockhit_chance  = 6;
static bool   hold_mode        = true;     // true = click only while you hold the button (classic)
static bool   require_focus    = true;     // true = no clicks into an unfocused Minecraft window
static bool   legacy_timing    = false;    // true = reproduce the classic's ~2x CPS math
static bool   other_background = true;
static ImVec4 accent           = ImVec4(1.00f, 0.137f, 1.00f, 1.00f);
static int    hide_bind        = 45;       // VK_INSERT

static bool   left_enabled     = false;
static bool   right_enabled    = false;
static bool   window_hidden    = false;
static int    g_tab            = 0;

// ---------------------------------------------------------------------------
// System tray + autostart state.
// ---------------------------------------------------------------------------
enum {
    WM_APP_TRAY    = WM_APP + 1,     // tray callback (classic style: lParam = mouse msg, wParam = icon id)
    IDM_TRAY_SHOW  = 2001,
    IDM_TRAY_LEFT  = 2002,
    IDM_TRAY_RIGHT = 2003,
    IDM_TRAY_AUTO  = 2004,
    IDM_TRAY_EXIT  = 2005,
};
static const UINT   TRAY_ICON_ID  = 1;
static const wchar_t RUN_KEY[]    = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t RUN_VALUE[]  = L"Abaser Clicker";

static HWND   g_trayWnd          = nullptr;  // hidden message sink that owns the tray icon
static HICON  g_appIcon          = nullptr;  // drawn at runtime — no .ico ships, the exe stays one file
static UINT   g_msgTaskbarCreated = 0;       // "TaskbarCreated": explorer restarted, re-add the icon
static UINT   g_msgShowInstance   = 0;       // a second launch asks the first to come back out
static HANDLE g_singleMutex      = nullptr;  // one-instance guard (tray + autostart invite double launches)
static bool   g_trayClass        = false;    // tray window class registered
static bool   start_with_windows = false;    // mirrors the registry Run entry
static bool   g_balloonShown     = false;    // "still in the tray" hint, once per run

// Slider limits
static const float CPS_LO = 1.0f,  CPS_HI = 25.0f;
static const int   CHANCE_LO = 1,  CHANCE_HI = 100;
static const int   BH_LO = 1,      BH_HI = 25;

// ---------------------------------------------------------------------------
// Live statistics (rolling one-second window per side + lifetime counters).
// ---------------------------------------------------------------------------
static std::mutex g_stats_mutex;
static std::deque<std::chrono::steady_clock::time_point> g_left_times, g_right_times;
static unsigned long long g_left_total = 0, g_right_total = 0;

static void RecordClick(bool left)
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(g_stats_mutex);
    std::deque<std::chrono::steady_clock::time_point>& q = left ? g_left_times : g_right_times;
    q.push_back(now);
    while (!q.empty() && now - q.front() > std::chrono::seconds(1))
        q.pop_front();
    if (left) g_left_total++; else g_right_total++;
}

static size_t RecentClicks(bool left)
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(g_stats_mutex);
    std::deque<std::chrono::steady_clock::time_point>& q = left ? g_left_times : g_right_times;
    while (!q.empty() && now - q.front() > std::chrono::seconds(1))
        q.pop_front();
    return q.size();
}

// ---------------------------------------------------------------------------
// Precise millisecond sleep.
// ---------------------------------------------------------------------------
static void precise_sleep(long long ms)
{
    if (ms <= 0) return;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    std::this_thread::sleep_until(deadline);
}

// ---------------------------------------------------------------------------
// Key helpers (name-matching so the numpad twin of a bind also counts,
// exactly like the classic did).
// ---------------------------------------------------------------------------
static bool IsBindKeyDown(int key)
{
    if (key < 0 || key > 255) return false;
    const char* myname = GetKeyName(key);
    for (int i = 3; i < 256; i++) {
        if (!GetAsyncKeyState(i)) continue;
        const char* name = GetKeyName(i);
        if (name[0] && myname[0] && strcmp(name, myname) == 0)
            return true;
    }
    return false;
}

// Toggle-watch: called every frame; toggles when the bind goes down, then
// debounces for 150 ms (classic behavior).
static void WatchBindToggle(int key, bool* flag)
{
    if (IsBindKeyDown(key)) {
        *flag = !*flag;
        precise_sleep(150);
    }
}

// Interactive rebind: scan VK 3..255 for a pressed key with a display name.
// Improvement over the classic: gives up after 5 s and keeps the current
// bind instead of freezing the UI forever.
static int ScanForNewBind(int current)
{
    for (int waited = 0; waited < 5000; waited += 1) {
        for (int k = 3; k < 256; k++) {
            if (GetAsyncKeyState(k)) {
                if (GetKeyName(k)[0])
                    return k;
            }
        }
        precise_sleep(1);
    }
    return current;
}

// ---------------------------------------------------------------------------
// The clicker engine thread.
//
// Targets the Minecraft (LWJGL) window and posts synthetic WM_LBUTTONDOWN /
// WM_LBUTTONUP pairs while active, honouring hit chance and randomised CPS
// between min and max. "Block Hit" additionally right-clicks (sword block)
// during left clicks with its own probability.
// ---------------------------------------------------------------------------
static void ClickerThread()
{
    const float RAND_MAX_F = 32767.0f;    // rand() range, as in the classic
    for (;;) {
        HWND target = FindWindowA("LWJGL", nullptr);

        const bool focus_ok = !require_focus
            || (target != nullptr && GetForegroundWindow() == target);
        const bool base_ok  = (target != nullptr) && focus_ok;

        // ---- left button ----
        const bool left_go = left_enabled && base_ok
            && (!hold_mode || GetAsyncKeyState(VK_LBUTTON) != 0);
        if (left_go) {
            if (rand() % 100 < hit_chance) {
                PostMessageA(target, WM_LBUTTONDOWN, MK_LBUTTON, 0);
                if (legacy_timing) {
                    // classic quirk: 100/cps, not 1000/cps (part of the 2x feel)
                    float cps = left_min_cps + (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                    precise_sleep((long long)(100.0f / cps));
                } else {
                    precise_sleep(2);     // believable press duration
                }
                PostMessageA(target, WM_LBUTTONUP, MK_LBUTTON, 0);
                RecordClick(true);

                if (blockhit && rand() % 100 < blockhit_chance) {
                    PostMessageA(target, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                    precise_sleep(20);
                    PostMessageA(target, WM_RBUTTONUP, MK_RBUTTON, 0);
                    RecordClick(false);
                }

                if (legacy_timing) {
                    // classic pacing: 300 or 400 "centi-CPS" after the click
                    float cps2 = left_min_cps + (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                    int gap = (cps2 > 14.0f) ? 300 : 400;
                    precise_sleep((long long)(gap / cps2));
                } else {
                    float cps = left_min_cps + (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                    long long total = (long long)(1000.0f / cps);
                    precise_sleep(total > 2 ? total - 2 : 1);
                }
            }
        }

        // ---- right button ----
        const bool right_go = right_enabled && base_ok
            && (!hold_mode || GetAsyncKeyState(VK_RBUTTON) != 0);
        if (right_go) {
            // improvement: the right side now rolls against right_chance
            // (the classic read hit_chance here — its slider was a placebo)
            if (rand() % 100 < right_chance) {
                PostMessageA(target, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                if (legacy_timing) {
                    float cps = right_min_cps + (right_max_cps - right_min_cps) * (rand() / RAND_MAX_F);
                    precise_sleep((long long)(100.0f / cps));
                } else {
                    precise_sleep(2);
                }
                PostMessageA(target, WM_RBUTTONUP, MK_RBUTTON, 0);
                RecordClick(false);

                if (legacy_timing) {
                    // classic quirk kept: the right-side gap scaled with the
                    // LEFT max slider. Only in legacy mode; nobody needs two
                    // copies of a bug.
                    float cps2 = right_min_cps + (right_max_cps - right_min_cps) * (rand() / RAND_MAX_F);
                    int gap = (left_max_cps > 14.0f) ? 300 : 400;
                    precise_sleep((long long)(gap / cps2));
                } else {
                    float cps = right_min_cps + (right_max_cps - right_min_cps) * (rand() / RAND_MAX_F);
                    long long total = (long long)(1000.0f / cps);
                    precise_sleep(total > 2 ? total - 2 : 1);
                }
            }
        }

        precise_sleep(1);
    }
}

// ---------------------------------------------------------------------------
// Settings persistence — abaser.ini, a plain key=value file next to the exe.
// ---------------------------------------------------------------------------
static const char* CONFIG_FILE = "abaser.ini";

static std::string ConfigPath()
{
    char buf[MAX_PATH + 1] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return CONFIG_FILE;
    std::string path(buf, (size_t)n);
    const size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return CONFIG_FILE;
    return path.substr(0, slash + 1) + CONFIG_FILE;
}

static std::string TrimStr(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static int ClampI(int v, int lo, int hi)   { return v < lo ? lo : (v > hi ? hi : v); }
static float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void ResetDefaults()
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
    other_background = true;
    accent           = ImVec4(1.00f, 0.137f, 1.00f, 1.00f);
    left_enabled     = false;
    right_enabled    = false;
}

static void ApplyConfigKey(const std::string& key, const std::string& val)
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
    else if (key == "other_background") other_background = (val == "true" || val == "1");
    else if (key == "left_enabled")     left_enabled     = (val == "true" || val == "1");
    else if (key == "right_enabled")    right_enabled    = (val == "true" || val == "1");
    else if (key == "accent") {
        float r = 1.0f, g = 0.137f, b = 1.0f, a = 1.0f;
        if (sscanf(v, "%f %f %f %f", &r, &g, &b, &a) == 4)
            accent = ImVec4(r, g, b, a);
    }
}

static bool LoadConfig(const std::string& path)
{
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

static bool SaveConfig(const std::string& path)
{
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
    out << "other_background = " << (other_background ? "true" : "false") << "\n";
    out << "left_enabled = "     << (left_enabled ? "true" : "false")    << "\n";
    out << "right_enabled = "    << (right_enabled ? "true" : "false")   << "\n";
    out << "accent = " << accent.x << " " << accent.y << " "
                       << accent.z << " " << accent.w << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// The tray icon — a magenta cursor-arrow drawn with GDI at the tray's own
// icon metric, so it stays crisp at any DPI and no .ico file has to ship
// (the executable remains a single self-contained file).
// ---------------------------------------------------------------------------
static HICON CreateAppIcon()
{
    const int s = ::GetSystemMetrics(SM_CXSMICON);
    if (s < 8)
        return (HICON)::LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);

    HDC     screen = ::GetDC(nullptr);
    HDC     dc     = ::CreateCompatibleDC(screen);
    BITMAPINFO bi  = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = s;
    bi.bmiHeader.biHeight      = s;                 // bottom-up, as icons like it
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    uint32_t* px     = nullptr;
    HBITMAP   color  = ::CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void**)&px, nullptr, 0);
    HICON     icon   = nullptr;
    if (screen && dc && color && px) {
        HGDIOBJ oldBmp = ::SelectObject(dc, color);
        // the familiar Windows arrow polygon (9x12 grid), scaled to fit
        const POINT base[7] = { {0,0},{0,11},{3,8},{5,12},{7,11},{5,7},{9,7} };
        const float k  = (s - 2.0f) / 12.0f;
        const float ox = (s - 9.0f * k) / 2.0f;
        const float oy = (s - 12.0f * k) / 2.0f;
        POINT pts[7];
        for (int i = 0; i < 7; i++) {
            pts[i].x = (LONG)(ox + base[i].x * k);
            pts[i].y = (LONG)(oy + base[i].y * k);
        }
        HBRUSH   brush    = ::CreateSolidBrush(RGB(255, 35, 255));   // the accent
        HGDIOBJ  oldBrush = ::SelectObject(dc, brush);
        HGDIOBJ  oldPen   = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        ::Polygon(dc, pts, 7);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(brush);
        ::SelectObject(dc, oldBmp);

        // GDI leaves the alpha channel at 0: make every drawn pixel fully
        // opaque and the untouched (black) corners fully transparent.
        for (int i = 0; i < s * s; i++)
            px[i] = (px[i] & 0x00FFFFFFu) ? (px[i] | 0xFF000000u) : 0u;

        ICONINFO ii   = {};
        ii.fIcon      = TRUE;
        ii.hbmColor   = color;
        ii.hbmMask    = ::CreateBitmap(s, s, 1, 1, nullptr);
        icon          = ::CreateIconIndirect(&ii);
        ::DeleteObject(ii.hbmMask);
    }
    if (color)  ::DeleteObject(color);
    if (dc)     ::DeleteDC(dc);
    if (screen) ::ReleaseDC(nullptr, screen);
    return icon ? icon : (HICON)::LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
}

// ---------------------------------------------------------------------------
// Tray plumbing. The tooltip carries the live armed state; the balloon
// explains the X-means-tray behavior exactly once per run.
// ---------------------------------------------------------------------------
static void RefreshTrayTip()
{
    if (g_trayWnd == nullptr) return;
    wchar_t ver[32] = {};
    for (int i = 0; i < 31 && ABASER_VERSION[i]; i++)
        ver[i] = (wchar_t)(unsigned char)ABASER_VERSION[i];
    wchar_t tip[128] = {};
    swprintf(tip, 128, L"Abaser Clicker %s | L:%s R:%s",
             ver, left_enabled ? L"ON" : L"off", right_enabled ? L"ON" : L"off");
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_trayWnd;
    nid.uID    = TRAY_ICON_ID;
    nid.uFlags = NIF_TIP;
    lstrcpynW(nid.szTip, tip, 128);
    ::Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static bool TrayAddIcon()
{
    if (g_trayWnd == nullptr || g_appIcon == nullptr) return false;
    NOTIFYICONDATAW nid = {};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = g_trayWnd;
    nid.uID              = TRAY_ICON_ID;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon            = g_appIcon;
    lstrcpynW(nid.szTip, L"Abaser Clicker", 128);
    if (!::Shell_NotifyIconW(NIM_ADD, &nid)) return false;
    RefreshTrayTip();
    return true;
}

static void TrayRemoveIcon()
{
    if (g_trayWnd == nullptr) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_trayWnd;
    nid.uID    = TRAY_ICON_ID;
    ::Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void TrayBalloon(const wchar_t* title, const wchar_t* text)
{
    if (g_trayWnd == nullptr) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize      = sizeof(nid);
    nid.hWnd        = g_trayWnd;
    nid.uID         = TRAY_ICON_ID;
    nid.uFlags      = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    lstrcpynW(nid.szInfoTitle, title, 64);
    lstrcpynW(nid.szInfo,      text,  256);
    ::Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ---------------------------------------------------------------------------
// Start with Windows — a plain per-user Run entry in HKCU (no admin, no
// service, no scheduled task). The command carries /tray so the autostarted
// copy launches straight into the tray; unchecking removes the value again.
// The registry is the source of truth: the checkbox merely mirrors it.
// ---------------------------------------------------------------------------
static bool AutostartEnabled()
{
    HKEY k = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;
    const bool on = ::RegQueryValueExW(k, RUN_VALUE, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    ::RegCloseKey(k);
    return on;
}

static bool SetAutostart(bool enable)
{
    HKEY k = nullptr;
    const LSTATUS r = enable
        ? ::RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr)
        : ::RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k);
    if (r != ERROR_SUCCESS) return false;

    bool ok = false;
    if (enable) {
        wchar_t exe[1024] = {};
        if (::GetModuleFileNameW(nullptr, exe, 1024) > 0) {
            const std::wstring cmd = std::wstring(L"\"") + exe + L"\" /tray";
            ok = ::RegSetValueExW(k, RUN_VALUE, 0, REG_SZ,
                                  (const BYTE*)cmd.c_str(),
                                  (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        }
    } else {
        const LSTATUS d = ::RegDeleteValueW(k, RUN_VALUE);
        ok = (d == ERROR_SUCCESS || d == ERROR_FILE_NOT_FOUND);   // gone is gone
    }
    ::RegCloseKey(k);
    return ok;
}

static bool StartInTray(LPSTR cmdLine)
{
    char buf[512] = {};
    lstrcpynA(buf, cmdLine ? cmdLine : "", 512);
    CharLowerBuffA(buf, lstrlenA(buf));
    return strstr(buf, "/tray") != nullptr || strstr(buf, "-tray") != nullptr;
}

// ---------------------------------------------------------------------------
// The tray window — a hidden message sink owning the icon, so the icon can
// survive independently of the (hideable) main window.
// ---------------------------------------------------------------------------
static void TrayContextMenu()
{
    HMENU m = ::CreatePopupMenu();
    if (m == nullptr) return;
    ::AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW,
                  window_hidden ? L"Show" : L"Hide to tray");
    ::AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(m, MF_STRING | (left_enabled  ? MF_CHECKED : 0), IDM_TRAY_LEFT,  L"Left clicker");
    ::AppendMenuW(m, MF_STRING | (right_enabled ? MF_CHECKED : 0), IDM_TRAY_RIGHT, L"Right clicker");
    ::AppendMenuW(m, MF_STRING | (start_with_windows ? MF_CHECKED : 0), IDM_TRAY_AUTO, L"Start with Windows");
    ::AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"Exit");

    POINT pt = {};
    ::GetCursorPos(&pt);
    ::SetForegroundWindow(g_trayWnd);                 // KB135908: the menu must
    ::TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_trayWnd, nullptr);
    ::PostMessageW(g_trayWnd, WM_NULL, 0, 0);         // ...see a message after, to dismiss
    ::DestroyMenu(m);
}

static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_msgTaskbarCreated != 0 && msg == g_msgTaskbarCreated) {
        TrayAddIcon();                                // explorer restarted: icon is gone, re-add
        return 0;
    }
    if (g_msgShowInstance != 0 && msg == g_msgShowInstance) {
        window_hidden = false;                        // a second launch summons us
        return 0;
    }
    if (msg == WM_APP_TRAY) {
        switch ((UINT)lParam) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            window_hidden = !window_hidden;
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            TrayContextMenu();
            return 0;
        }
        return 0;
    }
    if (msg == WM_COMMAND) {
        switch (LOWORD(wParam)) {
        case IDM_TRAY_SHOW:  window_hidden = !window_hidden;             break;
        case IDM_TRAY_LEFT:  left_enabled  = !left_enabled;              break;
        case IDM_TRAY_RIGHT: right_enabled = !right_enabled;             break;
        case IDM_TRAY_AUTO:
            start_with_windows = !start_with_windows;
            SetAutostart(start_with_windows);
            break;
        case IDM_TRAY_EXIT:  ::DestroyWindow(g_hWnd);                    break;
        }
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CreateTrayWindow(HINSTANCE hInstance)
{
    g_msgTaskbarCreated = ::RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW wc = { sizeof(wc), 0, TrayWndProc, 0, 0, hInstance, nullptr, nullptr,
                       nullptr, nullptr, L"AbaserTrayWnd", nullptr };
    if (!::RegisterClassExW(&wc)) return false;
    g_trayClass = true;
    g_trayWnd   = ::CreateWindowExW(0, L"AbaserTrayWnd", L"", WS_OVERLAPPED,
                                    0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    return g_trayWnd != nullptr;
}

// ---------------------------------------------------------------------------
// Device / window plumbing (the standard imgui DX9 example flow).
// ---------------------------------------------------------------------------
static bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed               = TRUE;
    g_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    return g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                &g_d3dpp, &g_pd3dDevice) >= 0;
}

static void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0 && "Device Reset Failed!");
    ImGui_ImplDX9_CreateDeviceObjects();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
            ResetDevice();
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_SCREENSAVE || (wParam & 0xfff0) == SC_MONITORPOWER)
            return 0;
        break;
    case WM_CLOSE:
        // X parks the window in the tray — the clickers keep running and
        // Exit lives on the tray menu. INSERT does the same via its bind.
        window_hidden = true;
        return 0;
    case WM_ENDSESSION:
        // shutting down / logging off while parked in the tray: still save
        if (wParam) SaveConfig(ConfigPath());
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Theme (recovered constants: dark gray surfaces, magenta accent).
// ---------------------------------------------------------------------------
static void StyleAbaser()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 4.0f;
    style.FramePadding      = ImVec2(4.0f, 3.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.ScrollbarSize     = 20.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabMinSize       = 8.0f;
    style.GrabRounding      = 1.0f;
    style.TabRounding       = 4.0f;
    style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;

    const ImVec4 bg        = ImVec4(0.094f, 0.094f, 0.094f, 1.00f);
    const ImVec4 bgHover   = ImVec4(0.194f, 0.194f, 0.194f, 1.10f);
    const ImVec4 bgActive  = ImVec4(0.244f, 0.244f, 0.244f, 1.10f);
    const ImVec4 textDim   = ImVec4(0.240f, 0.230f, 0.290f, 1.00f);
    // accent-derived tones (the classic computed these from the accent)
    const ImVec4 accentA   = ImVec4(accent.y + 0.10f, accent.x + 0.10f, accent.z, 1.00f);
    const ImVec4 accentB   = ImVec4(accent.x, accent.y + 0.20f, accent.z, 1.00f);

    colors[ImGuiCol_Text]                 = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = textDim;
    colors[ImGuiCol_WindowBg]             = bg;
    colors[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.094f, 0.094f, 0.094f, 1.05f);
    colors[ImGuiCol_Border]               = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    colors[ImGuiCol_BorderShadow]         = bgHover;
    colors[ImGuiCol_FrameBg]              = accentB;
    colors[ImGuiCol_FrameBgHovered]       = accentA;
    colors[ImGuiCol_FrameBgActive]        = accent;
    colors[ImGuiCol_TitleBg]              = accent;
    colors[ImGuiCol_TitleBgActive]        = accent;
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.094f, 0.094f, 0.094f, 0.20f);
    colors[ImGuiCol_ScrollbarBg]          = bgActive;
    colors[ImGuiCol_ScrollbarGrab]        = accentB;
    colors[ImGuiCol_ScrollbarGrabHovered] = accentA;
    colors[ImGuiCol_ScrollbarGrabActive]  = accent;
    colors[ImGuiCol_CheckMark]            = accentA;
    colors[ImGuiCol_SliderGrab]           = accentA;
    colors[ImGuiCol_SliderGrabActive]     = accent;
    colors[ImGuiCol_Button]               = accentB;
    colors[ImGuiCol_ButtonHovered]        = accentA;
    colors[ImGuiCol_ButtonActive]         = accent;
    colors[ImGuiCol_Header]               = accentB;
    colors[ImGuiCol_HeaderHovered]        = accentA;
    colors[ImGuiCol_HeaderActive]         = accent;
    colors[ImGuiCol_Separator]            = accentA;
    colors[ImGuiCol_SeparatorHovered]     = accentA;
    colors[ImGuiCol_SeparatorActive]      = accent;
    colors[ImGuiCol_ResizeGrip]           = accentB;
    colors[ImGuiCol_ResizeGripHovered]    = accentA;
    colors[ImGuiCol_ResizeGripActive]     = accent;
    colors[ImGuiCol_Tab]                  = accentB;
    colors[ImGuiCol_TabHovered]           = accentA;
    colors[ImGuiCol_TabActive]            = accent;
    colors[ImGuiCol_TabUnfocused]         = accentB;
    colors[ImGuiCol_TabUnfocusedActive]   = accentA;
    colors[ImGuiCol_NavHighlight]         = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance; (void)nCmdShow;
    srand((unsigned int)GetTickCount());

    // --- one instance at a time --------------------------------------------
    // Tray + autostart make a double launch easy (exe double-clicked while
    // the autostarted copy sits in the tray): the second copy just tells
    // the first to show its window and exits.
    g_msgShowInstance = ::RegisterWindowMessageW(L"AbaserClicker_ShowInstance");
    g_singleMutex     = ::CreateMutexW(nullptr, FALSE, L"Local\\AbaserClicker_SingleInstance");
    if (g_singleMutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_msgShowInstance != 0)
            ::PostMessageW(HWND_BROADCAST, g_msgShowInstance, 0, 0);
        ::CloseHandle(g_singleMutex);
        g_singleMutex = nullptr;
        return 0;
    }

    // settings first, so even the window state is what you left it as
    LoadConfig(ConfigPath());

    // launched by the autostart entry ("/tray")? start parked in the tray.
    // The autostart checkbox mirrors the registry — the Run key is the
    // source of truth, so manually deleting it is reflected here too.
    if (StartInTray(lpCmdLine)) window_hidden = true;
    start_with_windows = AutostartEnabled();

    // --- window -----------------------------------------------------------
    // Class and title are both a single space — the app keeps a low profile,
    // exactly like the classic did. The class icon is the tray arrow so the
    // taskbar button matches the tray when the window is out.
    g_appIcon = CreateAppIcon();
    WNDCLASSEXW wc = { sizeof(wc), 0, WndProc, 0, 0, GetModuleHandleW(nullptr), g_appIcon,
                       nullptr, nullptr, nullptr, L" ", g_appIcon };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(0, L" ", L" ",
                                  WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  100, 100, 500, 430,
                                  nullptr, nullptr, wc.hInstance, nullptr);
    g_hWnd = hwnd;
    if (!CreateDeviceD3D(hwnd)) {
        if (g_pD3D) g_pD3D->Release();
        ::UnregisterClassW(L" ", wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, window_hidden ? SW_HIDE : SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // --- ImGui + fonts ------------------------------------------------------
    // The font rides along in the classic's own "57BC" compressed container
    // (abaser_assets.h) and is decoded at startup; identical rendering to
    // the original executable, no library fork.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;              // our abaser.ini is the only config file

    static std::vector<uint8_t> ubuntu_ttf;   // must outlive the font atlas
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;    // ...so the atlas must not free our buffer
    if (abaser::decode_binary_asset(ABASER_FONT_UBUNTU, sizeof(ABASER_FONT_UBUNTU), ubuntu_ttf)
        && ubuntu_ttf.size() > 4)
        io.Fonts->AddFontFromMemoryTTF(ubuntu_ttf.data(), (int)ubuntu_ttf.size(), 18.0f, &font_cfg);
    else
        io.Fonts->AddFontDefault();           // fallback if the blob ever fails to decode

    // --- backends -----------------------------------------------------------
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);
    StyleAbaser();

    // --- clicker thread -----------------------------------------------------
    std::thread clicker(ClickerThread);
    clicker.detach();

    // --- tray ---------------------------------------------------------------
    // Hidden message sink owning the tray icon. Failure is not fatal — the
    // app simply runs without a tray icon and X stays a plain close.
    if (CreateTrayWindow(wc.hInstance))
        TrayAddIcon();

    // --- main loop ------------------------------------------------------------
    MSG msg = {};
    bool running = true;
    bool last_hidden = window_hidden;
    bool tip_l = left_enabled, tip_r = right_enabled;   // tooltip change detector
    static char config_status[192] = "settings load at startup and save on exit";

    while (running) {
        if (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                running = false;
            continue;
        }

        if (g_pd3dDevice == nullptr)
            CreateDeviceD3D(hwnd);

        // Start the frame
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();

        // Bind watchers (F4 / F4 / INSERT by default, 150 ms debounce)
        WatchBindToggle(left_bind,  &left_enabled);
        WatchBindToggle(right_bind, &right_enabled);
        WatchBindToggle(hide_bind,  &window_hidden);

        // Window visibility toggle: INSERT, the X button and the tray icon
        // all flip the same flag — the clickers never notice.
        if (window_hidden != last_hidden) {
            ::ShowWindow(hwnd, window_hidden ? SW_HIDE : SW_SHOWDEFAULT);
            if (window_hidden) {
                if (!g_balloonShown) {
                    g_balloonShown = true;
                    TrayBalloon(L"Abaser Clicker",
                                L"Still running in the tray. Left-click the icon to show it, "
                                L"right-click for the menu.");
                }
            } else {
                ::SetForegroundWindow(hwnd);
            }
            last_hidden = window_hidden;
        }
        if (!window_hidden) ::UpdateWindow(hwnd);

        // tray tooltip tracks the armed state (one API call per change)
        if (tip_l != left_enabled || tip_r != right_enabled) {
            tip_l = left_enabled;
            tip_r = right_enabled;
            RefreshTrayTip();
        }

        // ---- window 1: title shell ----------------------------------------
        ImGui::Begin(VSTR("Abaser Clicker"), nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
        ImGui::SetWindowSize(ImVec2(500.0f, 0.0f));
        ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::End();

        // ---- window 2: side panel (tab buttons + version) ------------------
        ImGui::Begin(VSTR("SidePanel"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowPos(ImVec2(-5.0f, 20.0f));
        ImGui::SetWindowSize(ImVec2(125.0f, 530.0f));
        if (ImGui::Button(VSTR("Clicker"),  ImVec2(100.0f, 50.0f)))   g_tab = 0;
        if (ImGui::Button(VSTR("Misc"),     ImVec2(100.0f, 50.0f)))   g_tab = 1;
        if (ImGui::Button(VSTR("Configs"),  ImVec2(100.0f, 50.0f)))   g_tab = 2;
        if (ImGui::Button(VSTR("Settings"), ImVec2(100.0f, 50.0f)))   g_tab = 3;
        ImGui::TextDisabled("Abaser\n%s", ABASER_VERSION);
        ImGui::End();

        // ---- window 3: canvas (status strip + tab content) ------------------
        ImGui::Begin(VSTR("Canvas"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowPos(ImVec2(107.0f, 25.0f));
        ImGui::SetWindowSize(ImVec2(370.0f, 360.0f));

        // shared status strip
        {
            const ImVec4 ok   = ImVec4(0.35f, 1.00f, 0.45f, 1.00f);
            const ImVec4 bad  = ImVec4(1.00f, 0.40f, 0.40f, 1.00f);
            HWND mc = FindWindowA("LWJGL", nullptr);
            if (mc != nullptr) ImGui::TextColored(ok, "MC: found");
            else               ImGui::TextColored(bad, "MC: not found");
            ImGui::SameLine();
            if (left_enabled)  ImGui::TextColored(ok, "L: ON");
            else               ImGui::TextDisabled("L: OFF");
            ImGui::SameLine();
            if (right_enabled) ImGui::TextColored(ok, "R: ON");
            else               ImGui::TextDisabled("R: OFF");
            ImGui::SameLine();
            ImGui::TextDisabled("live %.1f / %.1f cps",
                                (float)RecentClicks(true), (float)RecentClicks(false));
        }
        ImGui::Separator();

        if (g_tab == 0) {
            // keep min below max (integer compare, as in the classic)
            if ((int)left_min_cps >= (int)left_max_cps)
                left_min_cps = left_max_cps - 1.0f;
            if ((int)right_min_cps >= (int)right_max_cps)
                right_min_cps = right_max_cps - 1.0f;

            ImGui::Checkbox(VSTR("Left Toggle"), &left_enabled);
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Left Bind")))
                left_bind = ScanForNewBind(left_bind);
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(left_bind));
            ImGui::SliderScalar(VSTR("  "), ImGuiDataType_Float, &left_max_cps,
                                &CPS_LO, &CPS_HI, VSTR("%.1f LCPS"));
            ImGui::SliderScalar(VSTR("   "), ImGuiDataType_Float, &left_min_cps,
                                &CPS_LO, &CPS_HI, VSTR("%.1f LCPS"));
            ImGui::SliderScalar(VSTR("    "), ImGuiDataType_S32, &hit_chance,
                                &CHANCE_LO, &CHANCE_HI, "%d Chance");
            ImGui::Separator();

            ImGui::Checkbox(VSTR("Right Toggle"), &right_enabled);
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Right Bind")))
                right_bind = ScanForNewBind(right_bind);
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(right_bind));
            ImGui::SliderScalar(VSTR("     "), ImGuiDataType_Float, &right_max_cps,
                                &CPS_LO, &CPS_HI, VSTR("%.1f RCPS"));
            ImGui::SliderScalar(VSTR("      "), ImGuiDataType_Float, &right_min_cps,
                                &CPS_LO, &CPS_HI, VSTR("%.1f RCPS"));
            ImGui::SliderScalar(VSTR("       "), ImGuiDataType_S32, &right_chance,
                                &CHANCE_LO, &CHANCE_HI, "%d Chance");
            ImGui::Separator();

            ImGui::Checkbox(VSTR("Hold to click (classic)"), &hold_mode);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Click only while you physically hold the button\n(the classic behavior). Uncheck to click nonstop\nwhile the toggle is on.");
            ImGui::Checkbox(VSTR("Only click while Minecraft is focused"), &require_focus);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("No clicks go into an unfocused game window.\nUncheck to keep clicking while Minecraft sits\nin the background.");
            ImGui::Checkbox(VSTR("Legacy timing (classic 2x feel)"), &legacy_timing);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Accurate by default: the CPS you set is the CPS\nyou get. Legacy reproduces the classic's sloppy\nmath (~2x the label value).");
        }
        else if (g_tab == 1) {
            ImGui::Checkbox(VSTR("Block Hit"), &blockhit);
            ImGui::SameLine();
            ImGui::TextUnformatted(VSTR("Note: This is only for Left Click"));
            ImGui::SliderScalar(VSTR("        "), ImGuiDataType_S32, &blockhit_chance,
                                &BH_LO, &BH_HI, "%d Chance");
            ImGui::Separator();
            ImGui::TextDisabled("Total clicks: L %lu / R %lu",
                                (unsigned long)g_left_total, (unsigned long)g_right_total);
            if (ImGui::Button(VSTR("Reset counters"))) {
                g_left_total = 0;
                g_right_total = 0;
            }
        }
        else if (g_tab == 2) {
            ImGui::TextUnformatted(VSTR("Settings persist in abaser.ini next to the executable."));
            if (ImGui::Button(VSTR("Save now"))) {
                if (SaveConfig(ConfigPath()))
                    snprintf(config_status, sizeof(config_status), "saved to %s", ConfigPath().c_str());
                else
                    snprintf(config_status, sizeof(config_status), "save failed (read-only folder?)");
            }
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Reload"))) {
                if (LoadConfig(ConfigPath()))
                    snprintf(config_status, sizeof(config_status), "reloaded from disk");
                else
                    snprintf(config_status, sizeof(config_status), "no abaser.ini yet");
            }
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Reset defaults"))) {
                ResetDefaults();
                snprintf(config_status, sizeof(config_status), "defaults restored (not saved yet)");
            }
            ImGui::Separator();
            ImGui::TextWrapped("%s", config_status);
        }
        else if (g_tab == 3) {
            ImGui::ColorEdit4(VSTR("                       "), (float*)&accent,
                              ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoSidePreview |
                              ImGuiColorEditFlags_NoBorder);
            ImGui::Checkbox("Other Background", &other_background);
            ImGui::Text(VSTR("Application average %.3f ms/frame (%.1f FPS)"),
                        1000.0f / io.Framerate, io.Framerate);
            ImGui::TextUnformatted(VSTR("Hide Window: "));
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Bind")))
                hide_bind = ScanForNewBind(hide_bind);
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(hide_bind));
            ImGui::Separator();
            {
                const bool was_on = start_with_windows;
                ImGui::Checkbox(VSTR("Start with Windows"), &start_with_windows);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Adds a per-user Run entry (HKCU, no admin) that starts\nAbaser hidden in the tray at login. Unchecking removes it.\nAlso available on the tray icon's right-click menu.");
                if (start_with_windows != was_on) {
                    if (SetAutostart(start_with_windows))
                        snprintf(config_status, sizeof(config_status),
                                 start_with_windows ? "autostart on - starts in the tray at login"
                                                    : "autostart removed");
                    else {
                        start_with_windows = was_on;
                        snprintf(config_status, sizeof(config_status),
                                 "autostart change failed (registry write denied?)");
                    }
                }
            }
            ImGui::TextDisabled("X / INSERT hide to the tray; the tray menu (right-click) has Exit.");
        }
        ImGui::End();

        // ---- window 4: drawlist overlay (sidebar separator line) -----------
        ImGui::Begin(VSTR("DrawList"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNav);
        ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetWindowSize(ImVec2(800.0f, 800.0f));
        ImGui::GetCurrentWindow()->SkipItems = true;   // pure-drawing surface
        ImGui::GetCurrentWindow()->DrawList->AddLine(
            ImVec2(107.0f, 0.0f), ImVec2(107.0f, 400.0f),
            ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, accent.w * ImGui::GetStyle().Alpha)),
            3.0f);
        ImGui::End();

        // ---- finish frame + present -----------------------------------------
        ImGui::EndFrame();

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        D3DCOLOR bg = other_background ? 0xFF0C161C   // "Other Background" checked
                                       : 0xFF1B1616;  // default
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, bg, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT hr = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (hr == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    // ---- cleanup -----------------------------------------------------------
    SaveConfig(ConfigPath());             // autosave on exit

    TrayRemoveIcon();                     // take the icon down before its window
    if (g_trayWnd) {
        ::DestroyWindow(g_trayWnd);
        g_trayWnd = nullptr;
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D)       { g_pD3D->Release();       g_pD3D = nullptr; }
    if (g_appIcon)    { ::DestroyIcon(g_appIcon); g_appIcon = nullptr; }
    if (g_singleMutex) { ::CloseHandle(g_singleMutex); g_singleMutex = nullptr; }
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(L" ", wc.hInstance);
    if (g_trayClass)  ::UnregisterClassW(L"AbaserTrayWnd", wc.hInstance);
    return 0;
}
