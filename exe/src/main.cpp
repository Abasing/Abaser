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
//  This file is now only the PLATFORM: Win32 window + DX9 device, tray,
//  autostart, single-instance guard, and the clicker engine. Everything you
//  see on screen lives in src/ui.cpp (the glass UI), shared verbatim with
//  the headless simulator in sim/ so the two can never drift apart.
//
//  Recent fixes / changes:
//    * CLICK BUG FIXED: the UI used to layer a 800x800 "DrawList" overlay
//      window above everything; Dear ImGui hit-testing picked that window
//      (SkipItems windows are not skipped by the hover search), so NO widget
//      in any other window was ever hovered — clicks visibly did nothing.
//      The overlay is gone; the whole UI is one window now.
//    * Rebinding no longer freezes the UI (the old ScanForNewBind blocked
//      the message pump for up to 5 s). It is a small non-blocking state
//      machine across frames now.
//    * Toggle hotkeys use key-down EDGES (high bit of GetAsyncKeyState) —
//      the old code treated the unreliable "pressed since last call" bit as
//      "down", and blocked the UI thread 150 ms per hit.
//    * New Apple "Liquid Glass" look: one translucent canvas, glass cards,
//      iOS-style switches / segmented tabs / sliders, all in ui.cpp.
//    * FRAMELESS WINDOW: WS_POPUP + a custom drawn titlebar band — the app
//      is one seamless glass canvas, no native bar. Dragging works through
//      WM_NCHITTEST (HTCAPTION over the band, minus the min/X corner).
//    * MOD LINK: launched with --managed, a stdin reader thread ingests the
//      companion mod's state lines (GUI open? bow held?) and drives the
//      click gates; stdin EOF (game closed) shuts the clicker down.
// ============================================================================
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "ui.h"
#include "ui_state.h"
#include "abaser_compression.h"
#include "abaser_assets.h"
#include <windows.h>
#include <windowsx.h>             // GET_X_LPARAM / GET_Y_LPARAM
#include <shellapi.h>             // Shell_NotifyIcon — the tray
#include <d3d9.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#ifndef ABASER_VERSION
#define ABASER_VERSION "dev"
#endif

// ---------------------------------------------------------------------------
// Globals (window / device / tray plumbing)
// ---------------------------------------------------------------------------
static LPDIRECT3D9           g_pD3D        = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice  = nullptr;
static D3DPRESENT_PARAMETERS g_d3dpp       = {};
static HWND                  g_hWnd        = nullptr;

enum {
    WM_APP_TRAY    = WM_APP + 1,
    IDM_TRAY_SHOW  = 2001,
    IDM_TRAY_LEFT  = 2002,
    IDM_TRAY_RIGHT = 2003,
    IDM_TRAY_AUTO  = 2004,
    IDM_TRAY_EXIT  = 2005,
};
static const UINT   TRAY_ICON_ID  = 1;
static const wchar_t RUN_KEY[]    = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t RUN_VALUE[]  = L"Abaser Clicker";

static HWND   g_trayWnd          = nullptr;
static HICON  g_appIcon          = nullptr;
static UINT   g_msgTaskbarCreated = 0;
static UINT   g_msgShowInstance   = 0;
static HANDLE g_singleMutex      = nullptr;
static bool   g_trayClass        = false;
static bool   start_with_windows = false;
static bool   g_balloonShown     = false;

static std::mutex g_stats_mutex;
static std::deque<std::chrono::steady_clock::time_point> g_left_times, g_right_times;

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

static float LiveCps(bool left)
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(g_stats_mutex);
    std::deque<std::chrono::steady_clock::time_point>& q = left ? g_left_times : g_right_times;
    while (!q.empty() && now - q.front() > std::chrono::seconds(1))
        q.pop_front();
    return (float)q.size();
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
// Key helpers. Only the HIGH bit of GetAsyncKeyState counts (the key is down
// right now). The low "pressed since last call" bit made the old toggle
// watcher fire randomly and double-flip.
// ---------------------------------------------------------------------------
static bool IsBindKeyDown(int key)
{
    if (key < 0 || key > 255) return false;
    const char* myname = GetKeyName(key);
    if (!myname[0]) return false;
    for (int i = 3; i < 256; i++) {
        if ((GetAsyncKeyState(i) & 0x8000) == 0) continue;
        const char* name = GetKeyName(i);
        if (name[0] && strcmp(name, myname) == 0)
            return true;
    }
    return false;
}

// Edge-triggered toggle watch — called every frame, no blocking, no debounce
// sleep. Fires exactly once per key-down transition.
static void WatchBindToggle(int key, bool* flag, bool* prev_down)
{
    const bool down = IsBindKeyDown(key);
    if (down && !*prev_down)
        *flag = !*flag;
    *prev_down = down;
}

// Non-blocking bind capture: report the named key currently held (0 if none).
// Mouse buttons 1/2 are excluded (scan starts at 3), so clicking "Bind" can
// never capture itself.
static int PollBindKeyPlatform(void*)
{
    for (int k = 3; k < 256; k++)
        if (GetAsyncKeyState(k) & 0x8000)
            if (GetKeyName(k)[0])
                return k;
    return 0;
}

// ---------------------------------------------------------------------------
// The clicker engine thread.
//
// Targets the Minecraft (LWJGL) window and posts synthetic WM_LBUTTONDOWN /
// WM_LBUTTONUP pairs while active, honouring hit chance and randomised CPS
// between min and max. "Block Hit" additionally right-clicks (sword block)
// during left clicks with its own probability.
//
// The click GATES live in ui_state.h (shared with the simulator):
//   * any open in-game GUI (mod state) blocks everything,
//   * a drawn bow blocks every right-click emission — left keeps clicking.
// ---------------------------------------------------------------------------
static void ClickerThread()
{
    const float RAND_MAX_F = 32767.0f;
    for (;;) {
        HWND target = FindWindowA("LWJGL", nullptr);

        const bool focus_ok = !require_focus
            || (target != nullptr && GetForegroundWindow() == target);
        const bool base_ok  = (target != nullptr) && focus_ok;

        // ---- left button ----
        const bool left_go = left_enabled && GateLeft(base_ok,
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        if (left_go) {
            if (rand() % 100 < hit_chance) {
                PostMessageA(target, WM_LBUTTONDOWN, MK_LBUTTON, 0);
                if (legacy_timing) {
                    float cps = left_min_cps + (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                    precise_sleep((long long)(100.0f / cps));
                } else {
                    precise_sleep(2);     // believable press duration
                }
                PostMessageA(target, WM_LBUTTONUP, MK_LBUTTON, 0);
                RecordClick(true);

                if (blockhit && GateBlockHit() && rand() % 100 < blockhit_chance) {
                    PostMessageA(target, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                    precise_sleep(20);
                    PostMessageA(target, WM_RBUTTONUP, MK_RBUTTON, 0);
                    RecordClick(false);
                }

                if (legacy_timing) {
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
        const bool right_go = right_enabled && GateRight(base_ok,
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
        if (right_go) {
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
// The mod link (managed mode).
//
// The companion Forge mod starts this exe with "--managed" when a server
// loads and feeds one JSON line per state change over stdin:
//
//     {"gui":true,"bow":false,"held":"item.bow"}
//
// The mod NEVER clicks — this exe still does all of its own clicking, the
// mod only reports what it can see inside the game:
//   * "gui"  — any in-game GUI/screen is open (inventory, chest, villager,
//              enderchest, chat, pause menu, ...) -> block every click;
//   * "bow"  — the player is holding a bow -> hold all right-click output
//              (the right autoclicker and Block Hit) so charging stays
//              normal; left clicks are untouched;
//   * "exit" — the game is closing -> shut down gracefully.
// stdin EOF (game process gone, even after a crash) is a fallback exit.
//
// The link also carries SETTINGS (the in-game panel): the mod may send
//     {"config":{...abaser.ini keys...}}   — apply, persist, echo back
//     {"get":"config"}                     — dump the current settings
// Answers and a 2 s live-status line go out on stdout (ModOut below). The
// mod only turns knobs; every click is still produced by this engine.
// ---------------------------------------------------------------------------
static std::atomic<bool> g_exit_requested{false};
static std::atomic<bool> g_mod_linked{false};
static std::mutex g_mod_out_mutex;
static std::string ConfigPath();                 // defined below (abaser.ini path)

// One JSON line back to whoever spawned us (the mod reads it). Standalone
// runs have no usable stdout handle — nothing is written, nothing changes.
static void ModOut(const std::string& s)
{
    HANDLE out = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == nullptr || out == INVALID_HANDLE_VALUE) return;
    const std::string line = s + "\n";
    std::lock_guard<std::mutex> lock(g_mod_out_mutex);
    DWORD written = 0;
    if (::WriteFile(out, line.c_str(), (DWORD)line.size(), &written, nullptr))
        ::FlushFileBuffers(out);
}

// Look for "key" : true|false in a JSON-ish line; -1 when absent.
static int ModJsonFlag(const std::string& line, const char* key)
{
    std::string needle = std::string("\"") + key + "\"";
    const size_t p = line.find(needle);
    if (p == std::string::npos) return -1;
    const size_t colon = line.find(':', p + needle.size());
    if (colon == std::string::npos) return -1;
    size_t q = colon + 1;
    while (q < line.size() && (line[q] == ' ' || line[q] == '\t')) q++;
    if (line.compare(q, 4, "true") == 0)  return 1;
    if (line.compare(q, 5, "false") == 0) return 0;
    return -1;
}

static void ModLinkThread()
{
    HANDLE in = ::GetStdHandle(STD_INPUT_HANDLE);
    if (in == nullptr || in == INVALID_HANDLE_VALUE) return;   // standalone start

    std::string buf;
    char chunk[512];
    DWORD n = 0;
    for (;;) {
        if (!::ReadFile(in, chunk, sizeof(chunk), &n, nullptr) || n == 0)
            break;                                   // EOF — the game went away
        buf.append(chunk, chunk + (size_t)n);
        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            const std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (line.empty()) continue;

            const bool first_contact = !g_mod_linked.load();

            // ---- in-game panel settings (the mod turns the same knobs) ----
            if (ApplyJsonConfig(line) > 0) {
                SaveConfig(ConfigPath());            // persist like the UI does
                snprintf(g_config_status, sizeof(g_config_status), "%s",
                         VSTR("settings applied from the in-game panel"));
                ModOut(BuildConfigJson());           // canonical echo
            }
            if (line.find("\"get\"") != std::string::npos)
                ModOut(BuildConfigJson());           // panel (re)opened: dump

            const int gui = ModJsonFlag(line, "gui");
            const int bow = ModJsonFlag(line, "bow");
            if (gui >= 0) mod_gui_open    = gui != 0;
            if (bow >= 0) mod_holding_bow = bow != 0;
            g_mod_linked = true;

            if (first_contact)
                ModOut(BuildConfigJson());           // handshake: current settings

            const bool exit_cmd = (line == "exit") || (line.find("\"exit\"") != std::string::npos);
            if (exit_cmd) {
                g_mod_linked = false;
                g_exit_requested = true;
                return;
            }
        }
    }
    // EOF: the game closed (or crashed). Follow it down.
    g_mod_linked = false;
    g_exit_requested = true;
}

// 2 s live status for the in-game panel: armed sides, live CPS, the gates.
static void ModStatusThread()
{
    for (;;) {
        precise_sleep(2000);
        if (!g_mod_linked.load()) continue;
        ModOut(BuildStatusJson(FindWindowA("LWJGL", nullptr) != nullptr,
                               LiveCps(true), LiveCps(false)));
    }
}

// ---------------------------------------------------------------------------
// Config file path (abaser.ini next to the exe).
// ---------------------------------------------------------------------------
static std::string ConfigPath()
{
    char buf[MAX_PATH + 1] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return "abaser.ini";
    std::string path(buf, (size_t)n);
    const size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return "abaser.ini";
    return path.substr(0, slash + 1) + "abaser.ini";
}

// ---------------------------------------------------------------------------
// The tray icon — a magenta cursor-arrow drawn with GDI at the tray's own
// icon metric, so it stays crisp at any DPI and no .ico file has to ship.
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
    bi.bmiHeader.biHeight      = s;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    uint32_t* px     = nullptr;
    HBITMAP   color  = ::CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void**)&px, nullptr, 0);
    HICON     icon   = nullptr;
    if (screen && dc && color && px) {
        HGDIOBJ oldBmp = ::SelectObject(dc, color);
        const POINT base[7] = { {0,0},{0,11},{3,8},{5,12},{7,11},{5,7},{9,7} };
        const float k  = (s - 2.0f) / 12.0f;
        const float ox = (s - 9.0f * k) / 2.0f;
        const float oy = (s - 12.0f * k) / 2.0f;
        POINT pts[7];
        for (int i = 0; i < 7; i++) {
            pts[i].x = (LONG)(ox + base[i].x * k);
            pts[i].y = (LONG)(oy + base[i].y * k);
        }
        HBRUSH   brush    = ::CreateSolidBrush(RGB(255, 35, 255));
        HGDIOBJ  oldBrush = ::SelectObject(dc, brush);
        HGDIOBJ  oldPen   = ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        ::Polygon(dc, pts, 7);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(brush);
        ::SelectObject(dc, oldBmp);

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
// Tray plumbing.
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
// Start with Windows — a plain per-user Run entry in HKCU (no admin).
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
        ok = (d == ERROR_SUCCESS || d == ERROR_FILE_NOT_FOUND);
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

// Managed mode: a game-side mod (or launcher) owns this process's lifetime.
static bool StartManaged(LPSTR cmdLine)
{
    char buf[512] = {};
    lstrcpynA(buf, cmdLine ? cmdLine : "", 512);
    CharLowerBuffA(buf, lstrlenA(buf));
    return strstr(buf, "managed") != nullptr;
}

// ---------------------------------------------------------------------------
// The tray window — hidden message sink owning the icon.
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
    ::SetForegroundWindow(g_trayWnd);
    ::TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_trayWnd, nullptr);
    ::PostMessageW(g_trayWnd, WM_NULL, 0, 0);
    ::DestroyMenu(m);
}

static LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_msgTaskbarCreated != 0 && msg == g_msgTaskbarCreated) {
        TrayAddIcon();
        return 0;
    }
    if (g_msgShowInstance != 0 && msg == g_msgShowInstance) {
        window_hidden = false;
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
    case WM_NCHITTEST:
        // Seamless frameless titlebar: the top band drags the window like a
        // caption, except the reserved corner where the drawn min / X live.
        // Everything else is client area — the glass canvas is the window.
        if (ImGui::GetCurrentContext() != nullptr) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ::ScreenToClient(hWnd, &pt);
            if (pt.y >= 0 && pt.y < (LONG)lay::TB_H && pt.x < (LONG)(lay::CLIENT_W - lay::TB_CTRLS_W))
                return HTCAPTION;
        }
        return HTCLIENT;
    case WM_ENTERSIZEMOVE:
    case WM_EXITSIZEMOVE:
        // a caption drag swallowed the mouse — drop stale state so no ghost
        // click/drag fires inside the UI when the cursor comes back
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGuiIO& mio = ImGui::GetIO();
            mio.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            mio.AddMouseButtonEvent(0, false);
            mio.AddMouseButtonEvent(1, false);
        }
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
        if (wParam) SaveConfig(ConfigPath());
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Dark title bar + rounded window corners, where the OS offers them.
// Resolved at runtime so the exe still runs on old Windows untouched.
static void ApplyWindowDressing(HWND hwnd)
{
    typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE dwm = ::LoadLibraryW(L"dwmapi.dll");
    if (dwm == nullptr) return;
    DwmSetWindowAttributeFn fn =
        (DwmSetWindowAttributeFn)::GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (fn != nullptr) {
        BOOL dark = TRUE;
        fn(hwnd, 20, &dark, sizeof(dark));              // DWMWA_USE_IMMERSIVE_DARK_MODE
        fn(hwnd, 19, &dark, sizeof(dark));              // (older build number)
        DWORD round = 2;                                 // DWMWCP_ROUND
        fn(hwnd, 33, &round, sizeof(round));            // DWMWA_WINDOW_CORNER_PREFERENCE
    }
    ::FreeLibrary(dwm);
}

// ---------------------------------------------------------------------------
// UI host — the platform's answer to the few questions ui.cpp asks.
// ---------------------------------------------------------------------------
static bool  HostMcFound(void*)           { return FindWindowA("LWJGL", nullptr) != nullptr; }
static bool  HostAutostartEnabled(void*)  { return AutostartEnabled(); }
static bool  HostSetAutostart(void*, bool on) { return SetAutostart(on); }
static const char* HostConfigPath(void*)  { static std::string p = ConfigPath(); return p.c_str(); }
static void  HostMinimize(void*)          { if (g_hWnd) ::ShowWindow(g_hWnd, SW_MINIMIZE); }
static void  HostCloseToTray(void*)       { window_hidden = true; }

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance; (void)nCmdShow;
    srand((unsigned int)GetTickCount());

    // --- one instance at a time --------------------------------------------
    g_msgShowInstance = ::RegisterWindowMessageW(L"AbaserClicker_ShowInstance");
    g_singleMutex     = ::CreateMutexW(nullptr, FALSE, L"Local\\AbaserClicker_SingleInstance");
    if (g_singleMutex != nullptr && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_msgShowInstance != 0)
            ::PostMessageW(HWND_BROADCAST, g_msgShowInstance, 0, 0);
        ::CloseHandle(g_singleMutex);
        g_singleMutex = nullptr;
        return 0;
    }

    LoadConfig(ConfigPath());
    const bool managed = StartManaged(lpCmdLine);
    if (StartInTray(lpCmdLine)) window_hidden = true;
    start_with_windows = AutostartEnabled();

    // --- window: frameless, client area exactly the layout the UI was
    // --- designed for (the UI draws its own titlebar band)
    g_appIcon = CreateAppIcon();
    WNDCLASSEXW wc = { sizeof(wc), 0, WndProc, 0, 0, GetModuleHandleW(nullptr), g_appIcon,
                       nullptr, nullptr, nullptr, L" ", g_appIcon };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(0, L" ", L"Abaser Clicker",
                                  WS_POPUP | WS_MINIMIZEBOX,
                                  100, 100, (int)lay::CLIENT_W, (int)lay::CLIENT_H,
                                  nullptr, nullptr, wc.hInstance, nullptr);
    g_hWnd = hwnd;
    if (!CreateDeviceD3D(hwnd)) {
        if (g_pD3D) g_pD3D->Release();
        ::UnregisterClassW(L" ", wc.hInstance);
        return 1;
    }
    ApplyWindowDressing(hwnd);
    ::ShowWindow(hwnd, window_hidden ? SW_HIDE : SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // --- ImGui + fonts ------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    static std::vector<uint8_t> ubuntu_ttf;   // must outlive the font atlas
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    if (abaser::decode_binary_asset(ABASER_FONT_UBUNTU, sizeof(ABASER_FONT_UBUNTU), ubuntu_ttf)
        && ubuntu_ttf.size() > 4) {
        g_fonts.title = io.Fonts->AddFontFromMemoryTTF(ubuntu_ttf.data(), (int)ubuntu_ttf.size(), 16.0f, &font_cfg);
        g_fonts.body  = io.Fonts->AddFontFromMemoryTTF(ubuntu_ttf.data(), (int)ubuntu_ttf.size(), 14.0f, &font_cfg);
        g_fonts.small = io.Fonts->AddFontFromMemoryTTF(ubuntu_ttf.data(), (int)ubuntu_ttf.size(), 12.0f, &font_cfg);
    } else {
        g_fonts.title = io.Fonts->AddFontDefault();
        g_fonts.body  = io.Fonts->AddFontDefault();
        g_fonts.small = io.Fonts->AddFontDefault();
    }

    // --- backends -----------------------------------------------------------
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);
    StyleAbaserGlass();

    // --- clicker thread -----------------------------------------------------
    std::thread clicker(ClickerThread);
    clicker.detach();

    // --- mod link (managed mode only) ----------------------------------------
    if (managed) {
        std::thread(ModLinkThread).detach();
        std::thread(ModStatusThread).detach();
    }

    // --- tray ---------------------------------------------------------------
    if (CreateTrayWindow(wc.hInstance))
        TrayAddIcon();

    // --- main loop ------------------------------------------------------------
    MSG msg = {};
    bool running = true;
    bool last_hidden = window_hidden;
    bool prev_left_key = false, prev_right_key = false, prev_hide_key = false;
    bool tip_l = left_enabled, tip_r = right_enabled;

    AbaserUIHost host = {};
    host.mc_found          = &HostMcFound;
    host.autostart_enabled = &HostAutostartEnabled;
    host.set_autostart     = &HostSetAutostart;
    host.config_path       = &HostConfigPath;
    host.poll_bind_key     = &PollBindKeyPlatform;
    host.minimize          = &HostMinimize;
    host.close_to_tray     = &HostCloseToTray;
    host.user              = nullptr;

    while (running) {
        if (g_exit_requested) {          // the mod said exit, or the game closed
            running = false;
            break;
        }
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

        // Bind watchers (F4 / F4 / INSERT by default, edge-triggered)
        WatchBindToggle(left_bind,  &left_enabled,  &prev_left_key);
        WatchBindToggle(right_bind, &right_enabled, &prev_right_key);
        WatchBindToggle(hide_bind,  &window_hidden, &prev_hide_key);

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

        // tray tooltip tracks the armed state (one API call per change)
        if (tip_l != left_enabled || tip_r != right_enabled) {
            tip_l = left_enabled;
            tip_r = right_enabled;
            RefreshTrayTip();
        }

        host.live_cps_left  = LiveCps(true);
        host.live_cps_right = LiveCps(false);
        host.mod_linked      = g_mod_linked.load();
        host.mod_gui_open    = mod_gui_open;
        host.mod_holding_bow = mod_holding_bow;

        DrawUI(host, ABASER_VERSION);

        // ---- finish frame + present -----------------------------------------
        ImGui::EndFrame();

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        D3DCOLOR bg = 0xFF0C111A;
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
    SaveConfig(ConfigPath());

    TrayRemoveIcon();
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
