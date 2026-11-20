// ============================================================================
//  Abaser Clicker — the UI layer (platform-neutral)
// ----------------------------------------------------------------------------
//  DrawUI() renders the whole window. The platform (Win32 exe / headless sim)
//  provides an AbaserUIHost vtable with the few bits that genuinely belong to
//  the platform. No Win32 types appear here, so the exact pixels you see in
//  the simulator are the pixels the exe draws.
//
//  The layout constants below are the single source of truth: ui.cpp draws
//  from them and the headless simulator clicks on them, so a screenshot and
//  a click test can never disagree.
//
//  The window is FRAMELESS: the exe uses a WS_POPUP window and drags it by
//  the titlebar band (TB_H) defined here; the min/close buttons live in the
//  reserved top-right corner. WM_NCHITTEST in main.cpp reads TB_H and
//  TB_CTRLS_W so the drag region and the clickable buttons can never drift.
// ============================================================================
#pragma once
#include "imgui.h"
#include "imgui_internal.h"     // ImRect

struct AbaserUIHost
{
    // Minecraft (LWJGL) window present?
    bool (*mc_found)(void* user);

    // Autostart (per-user Run entry) — the registry is the source of truth.
    bool (*autostart_enabled)(void* user);
    bool (*set_autostart)(void* user, bool enable);

    // Config file (abaser.ini next to the exe).
    const char* (*config_path)(void* user);

    // Poll the currently pressed named virtual key (3..255), 0 if none.
    // Used by the non-blocking bind-capture flow; never reports mouse 1/2.
    int (*poll_bind_key)(void* user);

    // Custom frameless-window controls (the drawn min / X buttons).
    void (*minimize)(void* user);        // taskbar minimize
    void (*close_to_tray)(void* user);   // X parks the window in the tray

    // Live state fed by the companion mod (see "The mod link" in README).
    // All false/zero when the exe runs standalone without the mod.
    bool  mod_linked;        // the game-side mod is feeding us state
    bool  mod_gui_open;      // any in-game GUI is open (inventory, chest, chat, ...)
    bool  mod_holding_bow;   // the player is holding a bow

    // Live CPS readouts (engine-side rolling 1s window); 0.0 when idle.
    float live_cps_left;
    float live_cps_right;

    void* user;
};

// Fonts — set once by the platform after loading the shared TTF.
struct AbaserUIFonts { ImFont* body; ImFont* small; ImFont* title; };
inline AbaserUIFonts g_fonts = { nullptr, nullptr, nullptr };

// The glass theme — also applied once at startup by the platform.
void StyleAbaserGlass();

// One frame of UI (called between ImGui::NewFrame() and EndFrame).
// `version` is the build version string shown in the About card.
void DrawUI(const AbaserUIHost& host, const char* version);

// Bind-capture state (non-blocking rebind flow). which: BIND_* below.
enum { BIND_NONE = 0, BIND_LEFT = 1, BIND_RIGHT = 2, BIND_HIDE = 3 };
extern int g_bind_capture;

// ---------------------------------------------------------------------------
// Layout — client size 560 x 505 (the OS window is sized to exactly this
// client area by both platforms).
// ---------------------------------------------------------------------------
namespace lay
{
constexpr float CLIENT_W = 560.0f;
constexpr float CLIENT_H = 505.0f;
constexpr float MARGIN   = 16.0f;

// ---- custom titlebar --------------------------------------------------------
// The OS draws nothing — this band is part of the same glass canvas as the
// rest of the app, so there is no ugly native bar stuck on top of it.
constexpr float TB_H       = 40.0f;                       // draggable band height
constexpr float TB_BTN_W   = 38.0f;                       // min / close button width
constexpr float TB_CTRLS_W = 2.0f * TB_BTN_W;            // reserved right corner

inline ImRect TBMinBtn()    { return ImRect(CLIENT_W - TB_CTRLS_W, 0.0f, CLIENT_W - TB_BTN_W, TB_H); }
inline ImRect TBCloseBtn()  { return ImRect(CLIENT_W - TB_BTN_W, 0.0f, CLIENT_W, TB_H); }
inline float  TBChipRight() { return CLIENT_W - TB_CTRLS_W - 10.0f; }   // chips end here

inline ImVec2 Client()            { return ImVec2(CLIENT_W, CLIENT_H); }
inline float  ContentRight()      { return CLIENT_W - MARGIN; }          // 544
inline float  ContentW()          { return CLIENT_W - 2 * MARGIN; }      // 528
inline float  CardGap()           { return 10.0f; }

// segmented control
inline ImRect SegRect()           { return ImRect(MARGIN, 50.0f, ContentRight(), 86.0f); }
inline ImRect SegSeg(int i)       { const float w = ContentW() / 4.0f;
                                    return ImRect(MARGIN + i * w, 50.0f, MARGIN + (i + 1) * w, 86.0f); }

// ---- Clicker tab -----------------------------------------------------------
inline ImRect LeftCard()          { return ImRect(MARGIN, 96.0f, MARGIN + 259.0f, 314.0f); }
inline ImRect RightCard()         { return ImRect(285.0f, 96.0f, ContentRight(), 314.0f); }
inline ImRect ActivationCard()    { return ImRect(MARGIN, 324.0f, ContentRight(), 450.0f); }

// inside a card: content box is the card inset by 14 px (12 top)
inline ImRect CardContent(const ImRect& c) { return ImRect(c.Min.x + 14, c.Min.y + 12, c.Max.x - 14, c.Max.y - 12); }

// side card rows (card height 218): title+switch, hotkey, then three sliders
inline ImRect SideToggle(const ImRect& card)   { return ImRect(card.Min.x + 14, card.Min.y + 16,
                                                               card.Max.x - 14, card.Min.y + 44); }
inline ImRect SideBind(const ImRect& card)     { return ImRect(card.Min.x + 14, card.Min.y + 52,
                                                               card.Max.x - 14, card.Min.y + 76); }
inline ImRect SideSlider(const ImRect& card, int i) { return ImRect(card.Min.x + 14, card.Min.y + 86 + i * 38,
                                                               card.Max.x - 14, card.Min.y + 86 + i * 38 + 32); }

inline ImRect ActToggle(const ImRect& card, int i) { return ImRect(card.Min.x + 14, card.Min.y + 30 + i * 30,
                                                                   card.Max.x - 14, card.Min.y + 30 + i * 30 + 26); }

// ---- Misc tab ---------------------------------------------------------------
inline ImRect BlockCard()         { return ImRect(MARGIN, 96.0f, ContentRight(), 224.0f); }
inline ImRect BlockToggle()       { return ImRect(MARGIN + 14, 130.0f, ContentRight() - 14, 156.0f); }
inline ImRect BlockSlider()       { return ImRect(MARGIN + 14, 164.0f, ContentRight() - 14, 196.0f); }
inline float  BlockNoteY()        { return 206.0f; }
inline ImRect StatsCard()         { return ImRect(MARGIN, 234.0f, ContentRight(), 416.0f); }
inline ImRect StatsResetBtn()     { return ImRect(ContentRight() - 14 - 150.0f, 374.0f, ContentRight() - 14.0f, 406.0f); }

// ---- Configs tab ------------------------------------------------------------
inline ImRect ConfigCard()        { return ImRect(MARGIN, 96.0f, ContentRight(), 320.0f); }
inline float  ConfigFileY()       { return 140.0f; }
inline ImRect ConfigBtnRow()      { return ImRect(MARGIN + 14, 170.0f, ContentRight() - 14, 204.0f); }
inline float  ConfigStatusY()     { return 224.0f; }

// ---- Settings tab -----------------------------------------------------------
// "Theme" card: preset gradient chips on the first row, the two custom
// pickers (Primary + Accent — Discord-style 24-bit ints) on the second.
inline ImRect AccentCard()        { return ImRect(MARGIN, 96.0f, ContentRight(), 204.0f); }
inline ImRect ThemePreset(const ImRect& card, int i) { return ImRect(card.Min.x + 22 + i * 40.0f, card.Min.y + 35.0f,
                                                                     card.Min.x + 22 + i * 40.0f + 26.0f, card.Min.y + 57.0f); }
inline ImRect ThemePrimarySw(const ImRect& card)     { return ImRect(card.Min.x + 22.0f, card.Min.y + 70.0f,
                                                                     card.Min.x + 48.0f, card.Min.y + 94.0f); }
inline ImRect ThemeAccentSw(const ImRect& card)      { return ImRect(card.Min.x + 150.0f, card.Min.y + 70.0f,
                                                                     card.Min.x + 176.0f, card.Min.y + 94.0f); }
inline ImRect PrefsCard()         { return ImRect(MARGIN, 214.0f, ContentRight(), 348.0f); }
inline ImRect PrefsRow(const ImRect& card, int i) { return ImRect(card.Min.x + 14, card.Min.y + 30 + i * 34,
                                                                  card.Max.x - 14, card.Min.y + 30 + i * 34 + 26); }
inline ImRect AboutCard()         { return ImRect(MARGIN, 358.0f, ContentRight(), 442.0f); }

// ---- footer ------------------------------------------------------------------
inline ImRect FooterRect()        { return ImRect(MARGIN, CLIENT_H - 44.0f, ContentRight(), CLIENT_H - 8.0f); }

// toggle switch metrics (iOS style)
constexpr float SW_W = 44.0f, SW_H = 26.0f;
inline ImRect SwitchIn(const ImRect& row) { return ImRect(row.Max.x - SW_W, (row.Min.y + row.Max.y) * 0.5f - SW_H * 0.5f,
                                                          row.Max.x, (row.Min.y + row.Max.y) * 0.5f + SW_H * 0.5f); }

// bind pill metrics
constexpr float PILL_W = 96.0f, PILL_H = 24.0f;
inline ImRect PillIn(const ImRect& row)   { return ImRect(row.Max.x - PILL_W, (row.Min.y + row.Max.y) * 0.5f - PILL_H * 0.5f,
                                                          row.Max.x, (row.Min.y + row.Max.y) * 0.5f + PILL_H * 0.5f); }
} // namespace lay
