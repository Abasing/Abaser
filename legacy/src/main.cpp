// ============================================================================
//  Vega - Clicker V2 | Coming Soon Edition
// ----------------------------------------------------------------------------
//  Source reconstruction, reverse-engineered from the original compiled
//  executable (MSVC x64, Dear ImGui + DirectX 9 + Win32).
//
//  Original author credit found inside the binary:
//      "Made with <3\nRuneGaming"
//  Original project path:
//      C:\Users\miss_\Documents\GitHub\Vega-Clicker
//
//  Everything below was recovered from static analysis of the binary:
//    * window/class names, sizes, styles and positions
//    * every UI window, tab, widget, label and default value
//    * the auto-clicker engine thread and its exact delay math
//    * the string-obfuscation key ("the key" the file name talks about)
//    * the custom LZ77 + Base85 font compression ("57BC" format)
//
//  Build (MSVC, x64, Debug or Release):
//      cl /std:c++17 /EHsc main.cpp imgui*.cpp imgui_impl_dx9.cpp
//         imgui_impl_win32.cpp d3d9.lib
//  (or drop these files into the standard imgui DX9 example project)
// ============================================================================

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "vega_compression.h"
#include "vega_assets.h"
#include <d3d9.h>
#include <windows.h>
#include <shellapi.h>     // system()
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstdio>

#pragma comment(lib, "d3d9.lib")

// ---------------------------------------------------------------------------
// String obfuscation ("the key is in the source code")
// ---------------------------------------------------------------------------
// The original binary stored every user-visible string XOR-encrypted with a
// 48-byte key and decoded them on the stack right before use. The key:
static constexpr uint64_t VEGA_XOR_KEY[6] = {
    0x8118b5b2183014a7ull, 0x9ab7bae35936a050ull, 0x9ca7d1fc536407d9ull,
    0x3201677d57919edaull, 0x5e3342ae6df551fbull, 0x190c352f4c1ed6d4ull,
};

// Compile-time obfuscated string: only the XOR'd words are emitted into the
// binary; the plaintext is rebuilt in a stack-lifetime buffer on each call —
// mirroring the original's movabs/pxor/stack pattern.
template<size_t N>
struct VegaStr {
    uint64_t data[(N + 7) / 8] {};   // encrypted storage only
    char     buf[N + 1] {};          // zero-init, filled at runtime
    constexpr VegaStr(const char (&s)[N]) {
        for (size_t q = 0; q < (N + 7) / 8; q++) {
            uint64_t v = 0;
            for (int b = 7; b >= 0; b--) {
                size_t idx = q * 8 + (size_t)b;
                v = (v << 8) | (uint8_t)(idx < N ? s[idx] : 0);
            }
            data[q] = v ^ VEGA_XOR_KEY[q % 6];
        }
    }
    const char* operator()() {
        for (size_t q = 0; q < (N + 7) / 8; q++) {
            uint64_t v = data[q] ^ VEGA_XOR_KEY[q % 6];
            for (int b = 0; b < 8; b++)
                if (q * 8 + (size_t)b < N)
                    buf[q * 8 + b] = (char)((v >> (8 * b)) & 0xFF);
        }
        buf[N] = 0;
        return buf;
    }
};

#define VSTR(s) ([]() { static VegaStr<sizeof(s)> _v(s); return _v(); })()

// ---------------------------------------------------------------------------
// Virtual-key name table (exact table recovered from the binary's .rdata)
// ---------------------------------------------------------------------------
static const char* EMPTY_KEY_NAME = "";
// Name lookup for a virtual-key code — exact table recovered from the
// binary's .rdata (103 named keys; everything else maps to "").
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
        default: return EMPTY_KEY_NAME;
    }
}

// ---------------------------------------------------------------------------
// Global state (defaults recovered from the .data section of the binary)
// ---------------------------------------------------------------------------
static LPDIRECT3D9           g_pD3D        = nullptr;   // 0x140092fe0
static LPDIRECT3DDEVICE9     g_pd3dDevice  = nullptr;   // 0x140092fe8
static D3DPRESENT_PARAMETERS g_d3dpp       = {};        // 0x140092ff0
static IDirect3DTexture9*    g_FontTexture = nullptr;   // 0x140092fa8
static HWND                  g_hWnd        = nullptr;   // 0x140092fc8

static int    left_bind        = 115;      // 0x140092050 — VK_F4
static int    right_bind       = 115;      // 0x140092054 — VK_F4
static float  right_max_cps    = 20.0f;    // 0x140092058
static float  left_min_cps     = 9.6f;     // 0x14009205c
static float  left_max_cps     = 14.0f;    // 0x140092060
static int    blockhit_chance  = 6;        // 0x140092064
static float  right_min_cps    = 13.0f;    // 0x140092068
static int    hit_chance       = 87;       // 0x14009206c
static int    right_chance     = 80;       // 0x140092070 — UI-only in the original
static bool   other_background = true;     // 0x140092074
static ImVec4 accent           = ImVec4(1.00f, 0.137f, 1.00f, 1.00f); // 0x140092078
static int    hide_bind        = 45;       // 0x140092088 — VK_INSERT
static int    g_tab            = 0;        // 0x140092f6c

static bool   blockhit       = false;      // 0x140092f69
static bool   right_enabled  = false;      // 0x140092f6a
static bool   left_enabled   = false;      // 0x140092f6b
static bool   window_hidden  = false;      // 0x140092f80

static LARGE_INTEGER g_Time = {}, g_TicksPerSecond = {};  // 0x140092fd0/fd8

// ---------------------------------------------------------------------------
// Precise millisecond sleep (the original implemented sleep_until() with
// QueryPerformanceCounter + _Thrd_sleep; std::this_thread::sleep_for is close
// enough for a clicker, but we keep the precise variant for fidelity).
// ---------------------------------------------------------------------------
static void precise_sleep(long long ms)
{
    if (ms <= 0) return;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    std::this_thread::sleep_until(deadline);
}

// ---------------------------------------------------------------------------
// Key helpers
// ---------------------------------------------------------------------------
// True when the watched key (or any key sharing its display name, e.g. the
// numpad twin) is currently held — matches fn 0x140046120 in the binary.
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

// Toggle-watch: called every frame; toggles the flag when the bind goes down,
// then debounces for 150 ms (fn 0x1400461b0 in the binary).
static void WatchBindToggle(int key, bool* flag)
{
    if (IsBindKeyDown(key)) {
        *flag = !*flag;
        precise_sleep(150);
    }
}

// Interactive rebind used by the "Bind" buttons: scan VK 3..255 for a pressed
// key that has a display name; retry every 1 ms until one is found.
static int ScanForNewBind()
{
    for (;;) {
        for (int k = 3; k < 256; k++) {
            if (GetAsyncKeyState(k)) {
                if (GetKeyName(k)[0])
                    return k;
            }
        }
        precise_sleep(1);
    }
}

// ---------------------------------------------------------------------------
// The clicker engine thread (fn 0x1400439d0 in the binary).
// Targets the Minecraft (LWJGL) window and posts synthetic mouse clicks while
// the physical left/right button is held, honouring a hit-chance and
// randomised CPS between min and max. "Block Hit" additionally right-clicks
// (sword block) during left clicks with its own probability.
// ---------------------------------------------------------------------------
static void ClickerThread()
{
    const float RAND_MAX_F = 32767.0f;    // 0x14008863c — rand() range
    for (;;) {
        HWND target = FindWindowA("LWJGL", nullptr);

        // ---- left button ----
        if (GetAsyncKeyState(VK_LBUTTON) && left_enabled) {
            if (rand() % 100 < hit_chance) {
                PostMessageA(target, WM_LBUTTONDOWN, MK_LBUTTON, 0);
                // randomized CPS between left_min_cps and left_max_cps
                float cps = left_min_cps +
                            (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                precise_sleep((long long)(100.0f / cps));       // note: 100, as in the original
                PostMessageA(target, WM_LBUTTONUP, MK_LBUTTON, 0);

                if (blockhit && rand() % 100 < blockhit_chance) {
                    PostMessageA(target, WM_RBUTTONDOWN, MK_RBUTTON, 0);
                    precise_sleep(20);
                    PostMessageA(target, WM_RBUTTONUP, MK_RBUTTON, 0);
                }

                float cps2 = left_min_cps +
                             (left_max_cps - left_min_cps) * (rand() / RAND_MAX_F);
                int gap = (cps2 > 14.0f) ? 300 : 400;           // 14.0f constant, as original
                precise_sleep((long long)(gap / cps2));
            }
        }

        // ---- right button ----
        if (GetAsyncKeyState(VK_RBUTTON) && right_enabled) {
            if (rand() % 100 < hit_chance) {                   // original quirk: reads
                PostMessageA(target, WM_RBUTTONDOWN, MK_RBUTTON, 0); // hit_chance here,
                float cps = right_min_cps +                     // not right_chance
                            (right_max_cps - right_min_cps) * (rand() / RAND_MAX_F);
                precise_sleep((long long)(100.0f / cps));
                PostMessageA(target, WM_RBUTTONUP, MK_RBUTTON, 0);

                float cps2 = right_min_cps +
                             (right_max_cps - right_min_cps) * (rand() / RAND_MAX_F);
                int gap = (left_max_cps > 14.0f) ? 300 : 400;
                precise_sleep((long long)(gap / cps2));
            }
        }

        precise_sleep(1);
    }
}

// ---------------------------------------------------------------------------
// Font texture creation (custom variant of ImGui_ImplDX9_CreateFontTexture —
// the original converted the grayscale atlas to white-with-alpha RGBA itself).
// ---------------------------------------------------------------------------
static bool CreateFontsTexture()
{
    ImGuiIO& io = ImGui::GetIO();

    // Build atlas and get the texture as alpha8
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    // Convert to white RGBA (in the binary: rgba[i] = (alpha[i] << 24) | 0x00FFFFFF)
    std::vector<unsigned int> rgba((size_t)width * height);
    for (int i = 0; i < width * height; i++)
        rgba[(size_t)i] = ((unsigned int)pixels[i] << 24) | 0x00FFFFFF;

    if (g_pd3dDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC,
                                    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                    &g_FontTexture, nullptr) < 0)
        return false;

    D3DLOCKED_RECT locked;
    if (g_FontTexture->LockRect(0, &locked, nullptr, 0) != D3D_OK)
        return false;
    for (int y = 0; y < height; y++)
        memcpy((unsigned char*)locked.pBits + (size_t)y * locked.Pitch,
               rgba.data() + (size_t)y * width, (size_t)width * 4);
    g_FontTexture->UnlockRect(0);

    io.Fonts->SetTexID((ImTextureID)g_FontTexture);
    return true;
}

static void InvalidateDeviceObjects()
{
    if (g_FontTexture) { g_FontTexture->Release(); g_FontTexture = nullptr; ImGui::GetIO().Fonts->SetTexID(nullptr); }
}

static bool CreateDeviceObjects()
{
    if (!CreateFontsTexture())
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// Device / window plumbing (classic imgui_impl_dx9 example flow)
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
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Custom theme (recovered constants: dark gray surfaces, magenta accent)
// ---------------------------------------------------------------------------
static void StyleVega()
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
    // accent-derived tones — the binary computed these from the accent by
    // shuffling components and nudging by 0.1/0.2:
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
// Slider limits (named constants — C++ has no compound literals)
// ---------------------------------------------------------------------------
static const float  CPS_MIN_LO = 7.5f,  CPS_MIN_HI = 18.0f;
static const float  CPS_MAX_LO = 10.0f, CPS_MAX_HI = 20.0f;
static const uint8_t CHANCE_LO = 75,   CHANCE_HI = 100;
static const uint8_t BH_LO = 1,       BH_HI = 25;

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    // --- window -----------------------------------------------------------
    // Class and title are both a single space — the app hides its identity.
    WNDCLASSEXW wc = { sizeof(wc), 0, WndProc, 0, 0, GetModuleHandleW(nullptr), nullptr,
                       nullptr, nullptr, nullptr, L" ", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(0, L" ", L" ",
                                  WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  100, 100, 500, 335,
                                  nullptr, nullptr, wc.hInstance, nullptr);
    g_hWnd = hwnd;
    if (!CreateDeviceD3D(hwnd)) {
        if (g_pD3D) g_pD3D->Release();
        ::UnregisterClassW(L" ", wc.hInstance);
        return 1;
    }

    // --- ImGui + fonts ------------------------------------------------------
    // The original patched imgui_draw.cpp so ImFontAtlas::AddFont* would
    // transparently LZ-decompress its proprietary "57BC" container; the two
    // blobs were compiled straight into the executable. Here we decode them
    // at startup and hand ImGui plain TTF data — identical result, no fork
    // of the library required. (See imgui_mods_notes.md for the original.)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::vector<uint8_t> ubuntu_ttf;
    if (!vega::decode_binary_asset(VEGA_FONT_UBUNTU, sizeof(VEGA_FONT_UBUNTU), ubuntu_ttf))
        return 1;
    io.Fonts->AddFontFromMemoryTTF(ubuntu_ttf.data(), (int)ubuntu_ttf.size(), 18.0f);

    // (The ProggyClean Base85 blob was the atlas' *fallback* font — only used
    // when no other font had been added yet. Since Ubuntu is loaded above it
    // never renders, exactly like in the original binary. It is decoded here
    // purely to document/verify the pipeline:)
    std::vector<uint8_t> proggyclean_ttf;
    bool pc_ok = vega::decode_b85_asset(VEGA_FONT_PROGGYCLEAN_B85, proggyclean_ttf);
    (void)pc_ok;

    // --- backends -----------------------------------------------------------
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);
    CreateDeviceObjects();          // custom font texture (white RGBA variant)
    StyleVega();                    // custom dark + accent theme

    // --- clicker thread -----------------------------------------------------
    std::thread clicker(ClickerThread);
    clicker.detach();

    // --- main loop ------------------------------------------------------------
    MSG msg = {};
    bool running = true;
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
        if (g_FontTexture == nullptr)
            CreateDeviceObjects();

        // Start the frame
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();

        // Bind watchers (F4 / F4 / INSERT by default, 150 ms debounce)
        WatchBindToggle(left_bind,  &left_enabled);
        WatchBindToggle(right_bind, &right_enabled);
        WatchBindToggle(hide_bind,  &window_hidden);

        // Window visibility toggle + keep the console hidden
        ::ShowWindow(hwnd, window_hidden ? SW_HIDE : SW_SHOWDEFAULT);
        if (!window_hidden) ::UpdateWindow(hwnd);
        ::ShowWindow(::GetConsoleWindow(), SW_HIDE);

        // ---- window 1: title shell ----------------------------------------
        ImGui::Begin(VSTR("Vega - Clicker V2 | Coming Soon Edition"), nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoDocking);
        ImGui::SetWindowSize(ImVec2(500.0f, 0.0f));
        ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::End();

        // ---- window 2: side panel (tab buttons + credit) -------------------
        ImGui::Begin(VSTR("SidePanel"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowPos(ImVec2(-5.0f, 20.0f));
        ImGui::SetWindowSize(ImVec2(125.0f, 530.0f));
        if (ImGui::Button(VSTR("Clicker"), ImVec2(100.0f, 50.0f)))   g_tab = 0;
        if (ImGui::Button(VSTR("Misc"),    ImVec2(100.0f, 50.0f)))   g_tab = 1;
        if (ImGui::Button(VSTR("Settings"),ImVec2(100.0f, 50.0f)))   g_tab = 3;
        ImGui::TextUnformatted(VSTR("Made with <3\nRuneGaming"));
        ImGui::End();

        // ---- window 3: canvas (tab content) ---------------------------------
        ImGui::Begin(VSTR("Canvas"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowPos(ImVec2(107.0f, 25.0f));
        ImGui::SetWindowSize(ImVec2(529.9f, 350.0f));

        if (g_tab == 0) {
            // keep min below max (integer compare, as in the original)
            if ((int)left_min_cps >= (int)left_max_cps)
                left_min_cps = left_max_cps - 1.0f;

            ImGui::Checkbox(VSTR("Left Toggle"), &left_enabled);
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Left Bind")))
                left_bind = ScanForNewBind();
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(left_bind));
            ImGui::SliderScalar(VSTR("  "), ImGuiDataType_Float, &left_max_cps,
                                &CPS_MAX_LO, &CPS_MAX_HI, VSTR("%.1f LCPS"));
            ImGui::SliderScalar(VSTR("   "), ImGuiDataType_Float, &left_min_cps,
                                &CPS_MIN_LO, &CPS_MIN_HI, VSTR("%.1f LCPS"));
            ImGui::SliderScalar(VSTR("    "), ImGuiDataType_U8, (uint8_t*)&hit_chance,
                                &CHANCE_LO, &CHANCE_HI, "%d Chance");
            ImGui::Separator();

            ImGui::Checkbox(VSTR("Right Toggle"), &right_enabled);
            ImGui::SameLine();
            if (ImGui::Button(VSTR("Right Bind")))
                right_bind = ScanForNewBind();
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(right_bind));
            ImGui::SliderScalar(VSTR("     "), ImGuiDataType_Float, &right_max_cps,
                                &CPS_MAX_LO, &CPS_MAX_HI, VSTR("%.1f RCPS"));
            ImGui::SliderScalar(VSTR("      "), ImGuiDataType_Float, &right_min_cps,
                                &CPS_MIN_LO, &CPS_MIN_HI, VSTR("%.1f RCPS"));
            ImGui::SliderScalar(VSTR("        "), ImGuiDataType_U8, (uint8_t*)&right_chance,
                                &CHANCE_LO, &CHANCE_HI, "%d Chance");
        }
        else if (g_tab == 1) {
            ImGui::Checkbox(VSTR("Block Hit"), &blockhit);
            ImGui::SameLine();
            ImGui::TextUnformatted(VSTR("Note: This is only for Left Click"));
            ImGui::SliderScalar(VSTR("          "), ImGuiDataType_U8, (uint8_t*)&blockhit_chance,
                                &BH_LO, &BH_HI, "%d Chance");
        }
        else if (g_tab == 2) {
            ImGui::TextUnformatted(VSTR("NOTHING YET - Configs"));
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
                hide_bind = ScanForNewBind();
            ImGui::SameLine();
            ImGui::TextUnformatted(GetKeyName(hide_bind));
        }
        ImGui::End();

        // ---- window 4: drawlist overlay (sidebar separator line) -----------
        ImGui::Begin(VSTR("DrawList"), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking);
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
    // The original deleted imgui.ini on exit via system() — kept for fidelity.
    system(VSTR("del imgui.ini"));

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D)       { g_pD3D->Release();       g_pD3D = nullptr; }
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(L" ", wc.hInstance);
    return 0;
}
