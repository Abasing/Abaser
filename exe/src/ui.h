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
// `version` is the build version string shown in the header.
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

inline ImVec2 Client()            { return ImVec2(CLIENT_W, CLIENT_H); }
inline float  ContentRight()      { return CLIENT_W - MARGIN; }          // 544
inline float  ContentW()          { return CLIENT_W - 2 * MARGIN; }      // 528
inline float  ContentBottom()     { return CLIENT_H - 46.0f; }           // 459
inline float  CardGap()           { return 10.0f; }

// header
inline ImRect HeaderRect()        { return ImRect(MARGIN, 12.0f, ContentRight(), 58.0f); }

// segmented control
inline ImRect SegRect()           { return ImRect(MARGIN, 64.0f, ContentRight(), 100.0f); }
inline ImRect SegSeg(int i)       { const float w = ContentW() / 4.0f;
                                    return ImRect(MARGIN + i * w, 64.0f, MARGIN + (i + 1) * w, 100.0f); }

// content origin
inline float  ContentY()          { return 112.0f; }

// ---- Clicker tab -----------------------------------------------------------
inline ImRect LeftCard()          { return ImRect(MARGIN, 112.0f, MARGIN + 259.0f, 330.0f); }
inline ImRect RightCard()         { return ImRect(285.0f, 112.0f, ContentRight(), 330.0f); }
inline ImRect ActivationCard()    { return ImRect(MARGIN, 340.0f, ContentRight(), 458.0f); }

// inside a side card: content box is the card inset by 14 px
inline ImRect CardContent(const ImRect& c) { return ImRect(c.Min.x + 14, c.Min.y + 12, c.Max.x - 14, c.Max.y - 12); }

inline ImRect SideToggle(const ImRect& card)   { ImRect cc = CardContent(card);
                                                 return ImRect(cc.Min.x, cc.Min.y + 22, cc.Max.x, cc.Min.y + 48); }
inline ImRect SideBind(const ImRect& card)     { ImRect cc = CardContent(card);
                                                 return ImRect(cc.Min.x, cc.Min.y + 56, cc.Max.x, cc.Min.y + 80); }
inline ImRect SideSlider(const ImRect& card, int i) { ImRect cc = CardContent(card);
                                                 return ImRect(cc.Min.x, cc.Min.y + 88 + i * 38, cc.Max.x, cc.Min.y + 88 + i * 38 + 32); }

inline ImRect ActToggle(const ImRect& card, int i) { return ImRect(card.Min.x + 14, card.Min.y + 32 + i * 28,
                                                                    card.Max.x - 14, card.Min.y + 32 + i * 28 + 26); }

// ---- Misc tab ---------------------------------------------------------------
inline ImRect BlockCard()         { return ImRect(MARGIN, 112.0f, ContentRight(), 238.0f); }
inline ImRect BlockToggle()       { return ImRect(MARGIN + 14, 146.0f, ContentRight() - 14, 172.0f); }
inline ImRect BlockSlider()       { return ImRect(MARGIN + 14, 182.0f, ContentRight() - 14, 214.0f); }
inline ImRect StatsCard()         { return ImRect(MARGIN, 248.0f, ContentRight(), 420.0f); }
inline ImRect StatsResetBtn()     { return ImRect(ContentRight() - 14 - 150.0f, 358.0f, ContentRight() - 14, 390.0f); }

// ---- Configs tab ------------------------------------------------------------
inline ImRect ConfigCard()        { return ImRect(MARGIN, 112.0f, ContentRight(), 330.0f); }
inline ImRect ConfigBtnRow()      { return ImRect(MARGIN + 14, 188.0f, ContentRight() - 14, 222.0f); }

// ---- Settings tab -----------------------------------------------------------
inline ImRect AccentCard()        { return ImRect(MARGIN, 112.0f, ContentRight(), 200.0f); }
inline ImRect PrefsCard()         { return ImRect(MARGIN, 210.0f, ContentRight(), 344.0f); }
inline ImRect PrefsRow(const ImRect& card, int i) { return ImRect(card.Min.x + 14, card.Min.y + 30 + i * 34,
                                                                  card.Max.x - 14, card.Min.y + 30 + i * 34 + 26); }
inline ImRect AboutCard()         { return ImRect(MARGIN, 354.0f, ContentRight(), 442.0f); }

// ---- footer ------------------------------------------------------------------
inline ImRect FooterRect()        { return ImRect(MARGIN, CLIENT_H - 44.0f, ContentRight(), CLIENT_H - 8.0f); }

// toggle switch metrics (iOS style)
constexpr float SW_W = 44.0f, SW_H = 26.0f;
inline ImRect SwitchIn(const ImRect& row) { return ImRect(row.Max.x - SW_W, row.Min.y, row.Max.x, row.Min.y + SW_H); }

// bind pill metrics
constexpr float PILL_W = 86.0f, PILL_H = 24.0f;
inline ImRect PillIn(const ImRect& row)   { return ImRect(row.Max.x - PILL_W, (row.Min.y + row.Max.y) * 0.5f - PILL_H * 0.5f,
                                                          row.Max.x, (row.Min.y + row.Max.y) * 0.5f + PILL_H * 0.5f); }
} // namespace lay
