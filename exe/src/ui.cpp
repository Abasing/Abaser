// ============================================================================
//  Abaser Clicker — the glass UI
// ----------------------------------------------------------------------------
//  Apple "Liquid Glass" design language, realized inside Dear ImGui:
//
//    * one translucent-material canvas: a Discord-style vertical gradient of
//      the user's primary color under frosted glass cards — 1 px light
//      borders, top specular highlights, 16 px continuous radii
//    * a SEAMLESS frameless titlebar: the same canvas continues into the top
//      band, with drawn min / close buttons — no native bar anywhere
//    * big continuous radii everywhere (16-18px cards, pill controls)
//    * real controls: toggle switches, one-row sliders, segmented tabs,
//      pill buttons — all drawn by hand, all mouse-first
//    * theme = two 24-bit ints (primary, accent) like Discord's profile
//      theme_colors; every color on screen derives from them, and knob /
//      glyph colors are picked by relative luminance so any theme stays
//      readable
//
//  Everything renders into ONE root window, so there is exactly one hover
//  target — the old four-overlapping-windows layout (whose 800x800
//  SkipItems overlay swallowed every click) is gone.
// ============================================================================
#include "ui.h"
#include "ui_state.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

int g_bind_capture = BIND_NONE;
static bool g_bind_armed = false;   // require one clean "no key" frame first

// --- palette -----------------------------------------------------------------
static const ImVec4 WHITE    = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
static const ImVec4 TEXT_HI  = ImVec4(0.93f, 0.95f, 0.99f, 1.00f);
static const ImVec4 TEXT_MUT = ImVec4(0.55f, 0.59f, 0.68f, 1.00f);
static const ImVec4 TEXT_DIM = ImVec4(0.40f, 0.44f, 0.52f, 1.00f);
static const ImVec4 OK_GRN   = ImVec4(0.19f, 0.82f, 0.35f, 1.00f);   // iOS green
static const ImVec4 BAD_RED  = ImVec4(1.00f, 0.27f, 0.23f, 1.00f);   // iOS red
static const ImVec4 WARN_AMB = ImVec4(1.00f, 0.76f, 0.28f, 1.00f);   // pause state
static const ImVec4 GLASS_FILL      = ImVec4(1, 1, 1, 0.060f);
static const ImVec4 GLASS_FILL_HOV  = ImVec4(1, 1, 1, 0.115f);
static const ImVec4 GLASS_FILL_ACT  = ImVec4(1, 1, 1, 0.155f);
static const ImVec4 GLASS_BORDER    = ImVec4(1, 1, 1, 0.13f);
static const ImVec4 CARD_FILL       = ImVec4(1, 1, 1, 0.075f);

static ImU32 A(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
static ImU32 AA(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * a)); }

// --- small drawlist helpers ---------------------------------------------------
// Radial glow: one triangle fan, alpha fading from center to edge.
static void AddRadialGlow(ImDrawList* dl, ImVec2 center, float radius, const ImVec4& color, float alpha)
{
    if (alpha <= 0.004f) return;
    const int N = 36;
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(N * 3, N + 2);
    const ImDrawIdx base = (ImDrawIdx)dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(center, uv, ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha)));
    for (int i = 0; i <= N; i++) {
        const float ang = (float)i / (float)N * 6.2831853f;
        const ImVec2 p(center.x + cosf(ang) * radius, center.y + sinf(ang) * radius);
        dl->PrimWriteVtx(p, uv, ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.0f)));
    }
    for (int i = 0; i < N; i++) {
        dl->PrimWriteIdx(base);
        dl->PrimWriteIdx((ImDrawIdx)(base + 1 + i));
        dl->PrimWriteIdx((ImDrawIdx)(base + 2 + i));
    }
}

// Small drop shadow under a knob — deliberately soft and tight, so it reads
// as depth and not as the "dark bubble" the old build drew.
static void AddKnobShadow(ImDrawList* dl, ImVec2 center, float radius)
{
    AddRadialGlow(dl, ImVec2(center.x, center.y + 1.5f), radius * 1.15f, ImVec4(0, 0, 0, 1), 0.16f);
}

static void AddTextS(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* txt)   // small font
{
    dl->AddText(g_fonts.small, g_fonts.small->FontSize, pos, col, txt);
}
static ImVec2 TextSizeS(const char* txt)
{
    return g_fonts.small->CalcTextSizeA(g_fonts.small->FontSize, FLT_MAX, 0.0f, txt);
}
static float CenterTextY(const ImRect& r, float text_h)
{
    return (r.Min.y + r.Max.y) * 0.5f - text_h * 0.5f;
}

// Tooltip in the small font, clamped INSIDE the frameless window — a stock
// SetTooltip near the right edge would hang out of the glass canvas and get
// cut off by the OS window.
static void TooltipS(const char* txt)
{
    ImGui::PushFont(g_fonts.small);
    const ImVec2 ts = ImGui::CalcTextSize(txt);
    const ImVec2 mp = ImGui::GetIO().MousePos;
    const float x = ImClamp(mp.x + 14.0f, 8.0f, lay::CLIENT_W - ts.x - 26.0f);
    const float y = ImClamp(mp.y + 16.0f, 8.0f, lay::CLIENT_H - ts.y - 26.0f);
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetTooltip("%s", txt);
    ImGui::PopFont();
}

// Glass panel: translucent fill + 1px light border + top specular line.
static void AddGlassPanel(ImDrawList* dl, const ImRect& r, float rounding, const ImVec4& fill)
{
    dl->AddRectFilled(r.Min, r.Max, A(fill), rounding);
    dl->AddRect(ImVec2(r.Min.x + 0.5f, r.Min.y + 0.5f), ImVec2(r.Max.x - 0.5f, r.Max.y - 0.5f),
                A(GLASS_BORDER), rounding, 0, 1.0f);
    // specular highlight along the top edge
    dl->AddLine(ImVec2(r.Min.x + rounding * 0.8f, r.Min.y + 0.75f),
                ImVec2(r.Max.x - rounding * 0.8f, r.Min.y + 0.75f),
                AA(WHITE, 0.22f), 1.0f);
}

// Truncate a string to fit width, appending "..." when needed.
static std::string Ellipsize(const char* txt, float max_w)
{
    std::string s = txt;
    if (g_fonts.small->CalcTextSizeA(g_fonts.small->FontSize, FLT_MAX, 0.0f, s.c_str()).x <= max_w)
        return s;
    s += "...";
    while (s.size() > 3) {
        s.erase(s.size() - 4, 1);          // drop one char before the dots
        if (g_fonts.small->CalcTextSizeA(g_fonts.small->FontSize, FLT_MAX, 0.0f, s.c_str()).x <= max_w)
            break;
    }
    return s;
}

// Number formatting with thousands separators for the lifetime counters.
static std::string FormatCount(unsigned long long v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", v);
    std::string s = buf, out;
    int digits = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), s[i]);
        if (++digits % 3 == 0 && i > 0) out.insert(out.begin(), ' ');
    }
    return out;
}

// --- the theme -----------------------------------------------------------------
void StyleAbaserGlass()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    s.WindowPadding     = ImVec2(16, 16);
    s.FramePadding      = ImVec2(10, 6);
    s.ItemSpacing       = ImVec2(10, 8);
    s.ItemInnerSpacing  = ImVec2(6, 6);
    s.WindowRounding    = 0.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameRounding     = 8.0f;
    s.PopupRounding     = 10.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.GrabRounding      = 6.0f;
    s.GrabMinSize       = 10.0f;
    s.ScrollbarSize     = 14.0f;
    s.ScrollbarRounding = 9.0f;
    s.TabRounding       = 10.0f;
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    s.AntiAliasedLines  = true;
    s.AntiAliasedFill   = true;
    s.CircleTessellationMaxError = 0.20f;

    c[ImGuiCol_Text]              = TEXT_HI;
    c[ImGuiCol_TextDisabled]      = TEXT_MUT;
    c[ImGuiCol_WindowBg]          = ImVec4(0.055f, 0.065f, 0.100f, 1.00f);
    c[ImGuiCol_ChildBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]           = ImVec4(0.120f, 0.135f, 0.190f, 0.97f);
    c[ImGuiCol_Border]            = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_BorderShadow]      = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]           = GLASS_FILL;
    c[ImGuiCol_FrameBgHovered]    = GLASS_FILL_HOV;
    c[ImGuiCol_FrameBgActive]     = GLASS_FILL_ACT;
    c[ImGuiCol_TitleBg]           = ImVec4(0.055f, 0.065f, 0.100f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.055f, 0.065f, 0.100f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.055f, 0.065f, 0.100f, 0.60f);
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.055f, 0.065f, 0.100f, 1.00f);
    c[ImGuiCol_ScrollbarBg]       = ImVec4(1, 1, 1, 0.03f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(1, 1, 1, 0.16f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.24f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(1, 1, 1, 0.30f);
    c[ImGuiCol_CheckMark]         = accent;
    c[ImGuiCol_SliderGrab]        = accent;
    c[ImGuiCol_SliderGrabActive]  = accent;
    c[ImGuiCol_Button]            = GLASS_FILL;
    c[ImGuiCol_ButtonHovered]     = GLASS_FILL_HOV;
    c[ImGuiCol_ButtonActive]      = GLASS_FILL_ACT;
    c[ImGuiCol_Header]            = GLASS_FILL;
    c[ImGuiCol_HeaderHovered]     = GLASS_FILL_HOV;
    c[ImGuiCol_HeaderActive]      = GLASS_FILL_ACT;
    c[ImGuiCol_Separator]         = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(1, 1, 1, 0.14f);
    c[ImGuiCol_SeparatorActive]   = accent;
    c[ImGuiCol_ResizeGrip]        = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Tab]               = GLASS_FILL;
    c[ImGuiCol_TabHovered]        = GLASS_FILL_HOV;
    c[ImGuiCol_TabSelected]       = GLASS_FILL_ACT;
    c[ImGuiCol_NavCursor]         = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_TextSelectedBg]    = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_DragDropTarget]    = accent;
    c[ImGuiCol_NavHighlight]      = ImVec4(1, 1, 1, 0.10f);
    c[ImGuiCol_ModalWindowDimBg]  = ImVec4(0, 0, 0, 0.55f);
}

// ============================================================================
//  Custom controls
// ============================================================================

// iOS-style toggle switch. `label` is drawn left in the row; the switch sits
// vertically centered at the right edge of `row`. Returns true on flip.
static bool GlassToggle(const char* label, bool* v, const ImRect& row, const char* tip = nullptr)
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = win->DrawList;
    const ImRect sw = lay::SwitchIn(row);
    const ImGuiID id = win->GetID(label);

    ImGui::SetCursorScreenPos(sw.Min);
    ImGui::InvisibleButton(label, sw.GetSize());
    bool hovered = ImGui::IsItemHovered();
    bool pressed = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (pressed) *v = !*v;

    // knob animation state (fast settle — 90 ms at 60 fps, never bouncy)
    float* t = ImGui::GetStateStorage()->GetFloatRef(id, *v ? 1.0f : 0.0f);
    const float target = *v ? 1.0f : 0.0f;
    *t = target + (*t - target) * 0.55f;
    const float kn = *t;

    // track: OFF must be clearly visible — a dark trough with a light rim
    // (the old 13%-white track vanished on the glass card and left a lonely
    // floating knob, which read as a broken off-center button)
    const float round_t = lay::SW_H * 0.5f;
    if (*v) {
        dl->AddRectFilled(sw.Min, sw.Max, A(accent), round_t);
        dl->AddRect(ImVec2(sw.Min.x + 0.5f, sw.Min.y + 0.5f), ImVec2(sw.Max.x - 0.5f, sw.Max.y - 0.5f),
                    AA(WHITE, 0.30f), round_t, 0, 1.0f);
    } else {
        dl->AddRectFilled(sw.Min, sw.Max, A(ImVec4(0, 0, 0, hovered ? 0.38f : 0.30f)), round_t);
        dl->AddRect(ImVec2(sw.Min.x + 0.5f, sw.Min.y + 0.5f), ImVec2(sw.Max.x - 0.5f, sw.Max.y - 0.5f),
                    AA(WHITE, hovered ? 0.26f : 0.18f), round_t, 0, 1.0f);
    }
    // knob — dark or white by luminance, so it reads on light accents too
    const float kd = lay::SW_H - 6.0f;
    const float kx = sw.Min.x + 3.0f + kn * (lay::SW_W - kd - 6.0f);
    const ImVec2 kc(kx + kd * 0.5f, sw.Min.y + lay::SW_H * 0.5f);
    AddKnobShadow(dl, kc, kd * 0.5f);
    const ImVec4 knob_col = *v ? OnColor(accent) : WHITE;
    dl->AddCircleFilled(kc, kd * 0.5f, A(knob_col), 24);
    dl->AddCircle(kc, kd * 0.5f - 0.5f, AA(WHITE, 0.30f), 24, 1.0f);

    // label on the left, vertically centered
    ImFont* f = g_fonts.body;
    const float lh = f->FontSize;
    dl->AddText(ImVec2(row.Min.x, CenterTextY(row, lh)), A(TEXT_HI), label);

    if (tip && hovered) TooltipS(tip);
    return pressed;
}

// One-row slider: small label left, live value right, capsule track below.
// Type-erased value binding so int and float sliders share the same painting.
struct SliderBinding {
    void*    self;
    float  (*getf)(void*);
    void   (*setf)(void*, float);
};
static float SBind_GetF(void* s)          { return *(float*)s; }
static void  SBind_SetF(void* s, float v) { *(float*)s = v; }
static float SBind_GetI(void* s)          { return (float)*(int*)s; }
static void  SBind_SetI(void* s, float v) { *(int*)s = (int)(v + 0.5f); }

static bool GlassSliderCore(const char* id_salt, const char* label, const SliderBinding& b, bool is_int, float lo, float hi, const char* fmt,
                            const ImRect& row, const char* tip = nullptr)
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = win->DrawList;
    char idbuf[96];
    snprintf(idbuf, sizeof(idbuf), "##sl_%s", id_salt);   // salt must be unique per on-screen slider
    const ImGuiID id = win->GetID(idbuf);

    const float cur = b.getf(b.self);

    // label + value line
    AddTextS(dl, ImVec2(row.Min.x, row.Min.y), A(TEXT_MUT), label);
    char vbuf[40];
    if (is_int) snprintf(vbuf, sizeof(vbuf), fmt, (int)(cur + 0.5f));
    else        snprintf(vbuf, sizeof(vbuf), fmt, cur);
    const ImVec2 vs = TextSizeS(vbuf);
    // GetBoolRef returns bool* — dereference it, or the pointer-to-bool
    // conversion makes every frame read as "dragging" (accent-colored values)
    const bool dragging = ImGui::GetStateStorage()->GetBool(id, false);
    AddTextS(dl, ImVec2(row.Max.x - vs.x, row.Min.y), A(dragging ? accent : TEXT_HI), vbuf);

    // track row (grab area)
    const ImRect track(row.Min.x, row.Min.y + 16.0f, row.Max.x, row.Min.y + 16.0f + 16.0f);
    ImGui::SetCursorScreenPos(track.Min);
    ImGui::InvisibleButton(idbuf, track.GetSize());
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImGui::GetStateStorage()->SetBool(id, active);

    bool changed = false;
    if (active) {
        const float frac = ImClamp((ImGui::GetIO().MousePos.x - track.Min.x) / (track.Max.x - track.Min.x), 0.0f, 1.0f);
        const float nv = lo + frac * (hi - lo);
        if (nv != cur) { b.setf(b.self, nv); changed = true; }
    }
    const float frac = ImClamp((b.getf(b.self) - lo) / (hi - lo), 0.0f, 1.0f);

    // capsule track + accent fill + knob
    const float cy = (track.Min.y + track.Max.y) * 0.5f;
    const float th = 6.0f;
    const float pad = 7.0f;
    dl->AddRectFilled(ImVec2(track.Min.x, cy - th * 0.5f), ImVec2(track.Max.x, cy + th * 0.5f),
                      AA(WHITE, 0.13f), th * 0.5f);
    const float fillw = pad + frac * ((track.Max.x - track.Min.x) - 2 * pad);
    if (fillw > 2.0f)
        dl->AddRectFilled(ImVec2(track.Min.x, cy - th * 0.5f), ImVec2(track.Min.x + fillw, cy + th * 0.5f),
                          AA(accent, 0.90f), th * 0.5f);
    const float kx = track.Min.x + pad + frac * ((track.Max.x - track.Min.x) - 2 * pad);
    const float kr = hovered || active ? 8.0f : 7.0f;
    AddKnobShadow(dl, ImVec2(kx, cy), kr);
    dl->AddCircleFilled(ImVec2(kx, cy), kr, A(OnColor(accent)), 24);
    dl->AddCircle(ImVec2(kx, cy), kr - 0.5f, AA(accent, active ? 0.9f : 0.45f), 24, 1.5f);

    if (tip && hovered) TooltipS(tip);
    return changed;
}

// float / int convenience wrappers
static bool GlassSlider(const char* id_salt, const char* label, float* v, float lo, float hi, const char* fmt,
                        const ImRect& row, const char* tip = nullptr)
{
    SliderBinding b = { v, &SBind_GetF, &SBind_SetF };
    return GlassSliderCore(id_salt, label, b, false, lo, hi, fmt, row, tip);
}
static bool GlassSliderI(const char* id_salt, const char* label, int* v, int lo, int hi, const char* fmt,
                        const ImRect& row, const char* tip = nullptr)
{
    SliderBinding b = { v, &SBind_GetI, &SBind_SetI };
    return GlassSliderCore(id_salt, label, b, true, (float)lo, (float)hi, fmt, row, tip);
}

// Pill button. Returns true when clicked.
static bool GlassButtonPill(const char* label, const ImRect& r, bool emphasize = false)
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = win->DrawList;
    ImGui::SetCursorScreenPos(r.Min);
    ImGui::InvisibleButton(label, r.GetSize());
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    const ImVec4 fill = active ? GLASS_FILL_ACT : (hovered ? GLASS_FILL_HOV : GLASS_FILL);
    dl->AddRectFilled(r.Min, r.Max, A(fill), r.GetHeight() * 0.5f);
    dl->AddRect(ImVec2(r.Min.x + 0.5f, r.Min.y + 0.5f), ImVec2(r.Max.x - 0.5f, r.Max.y - 0.5f),
                A(emphasize ? ImVec4(accent.x, accent.y, accent.z, hovered ? 0.65f : 0.40f) : ImVec4(1, 1, 1, hovered ? 0.20f : 0.13f)),
                r.GetHeight() * 0.5f, 0, 1.0f);

    ImFont* f = g_fonts.body;
    const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, label);
    dl->AddText(ImVec2((r.Min.x + r.Max.x) * 0.5f - ts.x * 0.5f, CenterTextY(r, ts.y)),
                A(hovered ? TEXT_HI : ImVec4(0.85f, 0.88f, 0.94f, 1.0f)), label);
    return ImGui::IsItemClicked(ImGuiMouseButton_Left);
}

// Bind pill: shows the current key; click to start/stop capture. The pill
// grows leftward past its base width when the text needs it (the pulsing
// "press a key" capture label is wider than a key name) — it never overlaps
// the row label, which sits at the far left of the row.
static bool BindPill(const char* label, int* bind, int which, const ImRect& row)
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = win->DrawList;
    char idbuf[64];
    snprintf(idbuf, sizeof(idbuf), "##bind_%s", label);
    const bool capturing0 = (g_bind_capture == which);
    char txt0[24];
    if (capturing0) snprintf(txt0, sizeof(txt0), "%s", VSTR("press a key"));
    else            snprintf(txt0, sizeof(txt0), "%s", GetKeyName(*bind)[0] ? GetKeyName(*bind) : VSTR("unbound"));
    // key names render big (title font) — measure and draw with the SAME font
    ImFont* kf = g_fonts.title;
    const ImVec2 ts0 = kf->CalcTextSizeA(kf->FontSize, FLT_MAX, 0.0f, txt0);
    const float w = ImMax(lay::PILL_W, ts0.x + 18.0f);
    const ImRect r(row.Max.x - w, (row.Min.y + row.Max.y) * 0.5f - lay::PILL_H * 0.5f,
                   row.Max.x, (row.Min.y + row.Max.y) * 0.5f + lay::PILL_H * 0.5f);
    ImGui::SetCursorScreenPos(r.Min);
    ImGui::InvisibleButton(idbuf, r.GetSize());
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool capturing = (g_bind_capture == which);

    if (clicked) {
        g_bind_capture = capturing ? BIND_NONE : which;
        g_bind_armed = false;
    }

    const float pulse = capturing ? (0.55f + 0.45f * sinf((float)ImGui::GetTime() * 5.0f)) : 0.0f;
    const ImVec4 fill = capturing ? ImVec4(accent.x, accent.y, accent.z, 0.16f)
                        : (hovered ? GLASS_FILL_HOV : GLASS_FILL);
    dl->AddRectFilled(r.Min, r.Max, A(fill), r.GetHeight() * 0.5f);
    dl->AddRect(ImVec2(r.Min.x + 0.5f, r.Min.y + 0.5f), ImVec2(r.Max.x - 0.5f, r.Max.y - 0.5f),
                A(capturing ? ImVec4(accent.x, accent.y, accent.z, 0.35f + 0.5f * pulse)
                            : ImVec4(1, 1, 1, hovered ? 0.22f : 0.14f)),
                r.GetHeight() * 0.5f, 0, 1.0f);

    ImFont* f = kf;
    const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, txt0);
    dl->AddText(f, f->FontSize, ImVec2((r.Min.x + r.Max.x) * 0.5f - ts.x * 0.5f, CenterTextY(r, ts.y)),
                A(capturing ? accent : TEXT_HI), txt0);
    return clicked;
}

// Status chip (non-interactive): dot + text on a small glass pill.
static void StatusChip(ImDrawList* dl, float right_x, float y, const ImVec4& dot, const char* txt, ImVec2* cursor_out)
{
    const ImVec2 ts = TextSizeS(txt);
    const float h = lay::CHIP_H;
    const float w = ts.x + 30.0f;                 // 12 dot-inset + dot + 8 + text + 8 rim
    const float x = right_x - w;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), AA(WHITE, 0.075f), h * 0.5f);
    dl->AddRect(ImVec2(x + 0.5f, y + 0.5f), ImVec2(x + w - 0.5f, y + h - 0.5f), AA(WHITE, 0.12f), h * 0.5f, 0, 1.0f);
    dl->AddCircleFilled(ImVec2(x + 12.0f, y + h * 0.5f), 3.0f, A(dot), 12);
    AddTextS(dl, ImVec2(x + 21.0f, y + (h - ts.y) * 0.5f), A(ImVec4(0.85f, 0.88f, 0.94f, 1.0f)), txt);
    if (cursor_out) *cursor_out = ImVec2(x - 6.0f, y);
}

// Card with small uppercase title; returns content box.
static ImRect Card(ImDrawList* dl, const ImRect& r, const char* title)
{
    AddGlassPanel(dl, r, lay::CARD_R, CARD_FILL);
    char up[64];
    size_t n = 0;
    for (; n < 63 && title[n]; n++)
        up[n] = (char)((title[n] >= 'a' && title[n] <= 'z') ? title[n] - 32 : title[n]);
    up[n] = 0;
    AddTextS(dl, ImVec2(r.Min.x + 12.0f, r.Min.y + 9.0f), AA(TEXT_MUT, 0.85f), up);
    return lay::CardContent(r);
}

// --- background -----------------------------------------------------------------
// Discord-profile style: a vertical gradient of the user's primary color
// (top = lighter shade, bottom = deep shade), two soft glows tinted by the
// accent and the primary. No more gray slab; the whole canvas re-themes.
static void PaintBackground(ImDrawList* dl)
{
    const ImVec2 sz = lay::Client();
    const ImVec4 pr = Im4FromRgb(theme_primary);
    const float  k  = other_background ? 1.0f : 0.62f;    // "Dim backdrop"
    const ImVec4 top = ShadeColor(pr, 0.30f * k);
    const ImVec4 bot = ShadeColor(pr, 0.125f * k);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), sz, A(top), A(top), A(bot), A(bot));

    // ambient light glows — the "liquid" in liquid glass (kept subtle)
    AddRadialGlow(dl, ImVec2(sz.x * 0.94f, 30.0f), 250.0f, accent, 0.09f);
    AddRadialGlow(dl, ImVec2(sz.x * 0.06f, sz.y + 20.0f), 230.0f, pr, 0.08f);
}

// ============================================================================
//  DrawUI
// ============================================================================
void DrawUI(const AbaserUIHost& host, const char* version)
{
    ImGuiIO& io = ImGui::GetIO();

    // keep a couple of accent-derived theme colors fresh while the accent changes
    ImGuiStyle& st = ImGui::GetStyle();
    st.Colors[ImGuiCol_CheckMark]  = accent;
    st.Colors[ImGuiCol_SliderGrab] = accent;
    st.Colors[ImGuiCol_SliderGrabActive] = accent;

    // ---- non-blocking bind capture ----------------------------------------
    if (g_bind_capture != BIND_NONE) {
        const int vk = host.poll_bind_key(host.user);
        if (vk == 0) {
            g_bind_armed = true;                       // clean release seen
        } else if (g_bind_armed) {
            if      (g_bind_capture == BIND_LEFT) left_bind  = vk;
            else if (g_bind_capture == BIND_RIGHT) right_bind = vk;
            else if (g_bind_capture == BIND_HIDE)  hide_bind  = vk;
            g_bind_capture = BIND_NONE;
            g_bind_armed   = false;
        }
    }

    // ---- root window (the ONLY window) ------------------------------------
    const ImVec2 sz = lay::Client();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(sz);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##abaser", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    PaintBackground(dl);

    // ---- seamless titlebar --------------------------------------------------
    // The OS drags this band (WM_NCHITTEST -> HTCAPTION, main.cpp); we only
    // draw here. The two buttons sit in the reserved HTCLIENT corner.
    {
        ImFont* tf = g_fonts.title;
        dl->AddText(g_fonts.title, tf->FontSize, ImVec2(lay::MARGIN, (lay::TB_H - tf->FontSize) * 0.5f),
                    A(TEXT_HI), VSTR("Abaser Clicker"));

        // status chips, right-aligned in front of the window buttons
        ImVec2 cur(lay::TBChipRight(), (lay::TB_H - lay::CHIP_H) * 0.5f);
        const bool link_ok = host.mod_linked;
        if (link_ok) {
            StatusChip(dl, cur.x, cur.y, OK_GRN, VSTR("Mod"), &cur);
        }
        if (link_ok) {
            // in-game state, exactly what the mod reports
            const bool menu = host.mod_gui_open;
            StatusChip(dl, cur.x, cur.y, menu ? WARN_AMB : OK_GRN,
                       menu ? VSTR("Menu open") : VSTR("Minecraft"), &cur);
        } else {
            StatusChip(dl, cur.x, cur.y,
                       host.mc_found(host.user) ? OK_GRN : BAD_RED,
                       host.mc_found(host.user) ? VSTR("Minecraft") : VSTR("No game"), &cur);
        }
        StatusChip(dl, cur.x, cur.y, left_enabled ? accent : ImVec4(1, 1, 1, 0.25f),
                   left_enabled ? VSTR("L on") : VSTR("L off"), &cur);
        StatusChip(dl, cur.x, cur.y, right_enabled ? accent : ImVec4(1, 1, 1, 0.25f),
                   right_enabled ? VSTR("R on") : VSTR("R off"), &cur);

        // window controls: minimize + close, Windows-style, top right
        {
            const ImRect mr = lay::TBMinBtn();
            ImGui::SetCursorScreenPos(mr.Min);
            ImGui::InvisibleButton("##winmin", mr.GetSize());
            const bool hov = ImGui::IsItemHovered();
            const float cy = (mr.Min.y + mr.Max.y) * 0.5f;
            if (hov) dl->AddRectFilled(mr.Min, mr.Max, AA(WHITE, 0.10f), 0.0f);
            dl->AddLine(ImVec2(mr.GetCenter().x - 5.0f, cy), ImVec2(mr.GetCenter().x + 5.0f, cy),
                        AA(WHITE, hov ? 0.95f : 0.70f), 1.4f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && host.minimize)
                host.minimize(host.user);

            const ImRect cr = lay::TBCloseBtn();
            ImGui::SetCursorScreenPos(cr.Min);
            ImGui::InvisibleButton("##winclose", cr.GetSize());
            const bool hc = ImGui::IsItemHovered();
            const float cx = (cr.Min.x + cr.Max.x) * 0.5f;
            if (hc) dl->AddRectFilled(cr.Min, cr.Max, A(ImVec4(0.86f, 0.21f, 0.20f, 0.65f)), 0.0f);
            const ImU32 xc = hc ? IM_COL32(255, 255, 255, 235) : AA(WHITE, 0.70f);
            dl->AddLine(ImVec2(cx - 5.0f, cy - 5.0f), ImVec2(cx + 5.0f, cy + 5.0f), xc, 1.4f);
            dl->AddLine(ImVec2(cx - 5.0f, cy + 5.0f), ImVec2(cx + 5.0f, cy - 5.0f), xc, 1.4f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && host.close_to_tray)
                host.close_to_tray(host.user);
        }
    }

    // ---- segmented control --------------------------------------------------
    {
        const ImRect seg = lay::SegRect();
        dl->AddRectFilled(seg.Min, seg.Max, AA(WHITE, 0.07f), seg.GetHeight() * 0.5f);
        dl->AddRect(ImVec2(seg.Min.x + 0.5f, seg.Min.y + 0.5f), ImVec2(seg.Max.x - 0.5f, seg.Max.y - 0.5f),
                    AA(WHITE, 0.11f), seg.GetHeight() * 0.5f, 0, 1.0f);
        const char* names[4] = { VSTR("Clicker"), VSTR("Misc"), VSTR("Configs"), VSTR("Settings") };
        for (int i = 0; i < 4; i++) {
            const ImRect sr = lay::SegSeg(i);
            char idbuf[32];
            snprintf(idbuf, sizeof(idbuf), "##seg%d", i);
            ImGui::SetCursorScreenPos(sr.Min);
            ImGui::InvisibleButton(idbuf, sr.GetSize());
            const bool hovered = ImGui::IsItemHovered();
            const bool active = (g_tab == i);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) g_tab = i;
            if (active) {
                const ImRect pill(sr.Min.x + 3, sr.Min.y + 3, sr.Max.x - 3, sr.Max.y - 3);
                // active pill: SOLID accent with readable on-color text — the
                // clearest premium statement of "you are here" on any theme
                dl->AddRectFilled(pill.Min, pill.Max, AA(accent, 0.92f), pill.GetHeight() * 0.5f);
                dl->AddRect(ImVec2(pill.Min.x + 0.5f, pill.Min.y + 0.5f), ImVec2(pill.Max.x - 0.5f, pill.Max.y - 0.5f),
                            AA(WHITE, 0.30f), pill.GetHeight() * 0.5f, 0, 1.0f);
                dl->AddLine(ImVec2(pill.Min.x + pill.GetWidth() * 0.3f, pill.Min.y + 1.5f),
                            ImVec2(pill.Max.x - pill.GetWidth() * 0.3f, pill.Min.y + 1.5f),
                            AA(WHITE, 0.35f), 1.0f);
            } else if (hovered) {
                dl->AddRectFilled(sr.Min, sr.Max, AA(WHITE, 0.05f), sr.GetHeight() * 0.5f - 2.0f);
            }
            ImFont* f = g_fonts.body;
            const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, names[i]);
            dl->AddText(ImVec2((sr.Min.x + sr.Max.x) * 0.5f - ts.x * 0.5f, CenterTextY(sr, ts.y)),
                        A(active ? OnColor(accent) : (hovered ? ImVec4(0.82f, 0.85f, 0.92f, 1.0f) : TEXT_MUT)), names[i]);
        }
    }

    // ---- tab content ---------------------------------------------------------
    if (g_tab == 0) {
        // keep min below max
        if (left_min_cps >= left_max_cps)  left_min_cps  = left_max_cps - 0.5f;
        if (right_min_cps >= right_max_cps) right_min_cps = right_max_cps - 0.5f;

        // two glass cards, left & right clicker — sized to their content
        const ImRect lc = lay::LeftCard(), rc = lay::RightCard();
        AddGlassPanel(dl, lc, lay::CARD_R, CARD_FILL);
        AddGlassPanel(dl, rc, lay::CARD_R, CARD_FILL);
        {
            const ImRect row = lay::SideToggle(lc);
            GlassToggle(VSTR("Left clicker"), &left_enabled, row);
            BindPill("left", &left_bind, BIND_LEFT, lay::SideBind(lc));
            AddTextS(dl, ImVec2(lay::CardContent(lc).Min.x,
                                CenterTextY(lay::SideBind(lc), TextSizeS(VSTR("Hotkey")).y)),
                     A(TEXT_MUT), VSTR("Hotkey"));
            GlassSlider("l_max", VSTR("CPS max"), &left_max_cps, CPS_LO, CPS_HI, "%.1f", lay::SideSlider(lc, 0));
            GlassSlider("l_min", VSTR("CPS min"), &left_min_cps, CPS_LO, CPS_HI, "%.1f", lay::SideSlider(lc, 1));
            GlassSliderI("l_ch", VSTR("Chance"), &hit_chance, CHANCE_LO, CHANCE_HI, "%d%%", lay::SideSlider(lc, 2));
        }
        {
            const ImRect row = lay::SideToggle(rc);
            GlassToggle(VSTR("Right clicker"), &right_enabled, row);
            BindPill("right", &right_bind, BIND_RIGHT, lay::SideBind(rc));
            AddTextS(dl, ImVec2(lay::CardContent(rc).Min.x,
                                CenterTextY(lay::SideBind(rc), TextSizeS(VSTR("Hotkey")).y)),
                     A(TEXT_MUT), VSTR("Hotkey"));
            GlassSlider("r_max", VSTR("CPS max"), &right_max_cps, CPS_LO, CPS_HI, "%.1f", lay::SideSlider(rc, 0));
            GlassSlider("r_min", VSTR("CPS min"), &right_min_cps, CPS_LO, CPS_HI, "%.1f", lay::SideSlider(rc, 1));
            GlassSliderI("r_ch", VSTR("Chance"), &right_chance, CHANCE_LO, CHANCE_HI, "%d%%", lay::SideSlider(rc, 2));
        }

        // activation card
        const ImRect ac = lay::ActivationCard();
        Card(dl, ac, VSTR("Activation"));
        GlassToggle(VSTR("Hold to click"), &hold_mode, lay::ActToggle(ac, 0),
                    VSTR("Click only while you physically hold the button.\nUncheck to click nonstop while armed."));
        GlassToggle(VSTR("Only while Minecraft focused"), &require_focus, lay::ActToggle(ac, 1),
                    VSTR("No clicks go into an unfocused game window."));
        GlassToggle(VSTR("Legacy timing (2x feel)"), &legacy_timing, lay::ActToggle(ac, 2),
                    VSTR("Accurate by default: the CPS you set is the CPS you get.\nLegacy reproduces the classic's sloppy math (~2x the label)."));
    }
    else if (g_tab == 1) {
        const ImRect bc = lay::BlockCard();
        Card(dl, bc, VSTR("Block hit"));
        GlassToggle(VSTR("Enabled"), &blockhit, lay::BlockToggle(),
                    VSTR("Sword-block taps ride along with left clicks."));
        GlassSliderI("bh_ch", VSTR("Chance"), &blockhit_chance, BH_LO, BH_HI, "%d%%", lay::BlockSlider());
        AddTextS(dl, ImVec2(lay::MARGIN + 12, lay::BlockNoteY()), A(TEXT_DIM),
                 VSTR("Note: applies to the left clicker only."));

        const ImRect sc = lay::StatsCard();
        Card(dl, sc, VSTR("Statistics"));
        {
            std::string l = FormatCount(g_left_total);
            std::string r = FormatCount(g_right_total);
            ImFont* tf = g_fonts.title;
            // two columns, each number + caption pair centered in its half
            const float half = sc.GetWidth() * 0.5f;
            const ImVec2 ls = tf->CalcTextSizeA(tf->FontSize, FLT_MAX, 0.0f, l.c_str());
            const ImVec2 rs = tf->CalcTextSizeA(tf->FontSize, FLT_MAX, 0.0f, r.c_str());
            const float num_y = sc.Min.y + 34.0f;
            const float cap_y = num_y + tf->FontSize + 5.0f;
            dl->AddText(g_fonts.title, tf->FontSize,
                        ImVec2(sc.Min.x + half * 0.5f - ls.x * 0.5f, num_y), A(TEXT_HI), l.c_str());
            AddTextS(dl, ImVec2(sc.Min.x + half * 0.5f - TextSizeS(VSTR("left clicks")).x * 0.5f, cap_y),
                     A(TEXT_MUT), VSTR("left clicks"));
            dl->AddText(g_fonts.title, tf->FontSize,
                        ImVec2(sc.Min.x + half * 1.5f - rs.x * 0.5f, num_y), A(TEXT_HI), r.c_str());
            AddTextS(dl, ImVec2(sc.Min.x + half * 1.5f - TextSizeS(VSTR("right clicks")).x * 0.5f, cap_y),
                     A(TEXT_MUT), VSTR("right clicks"));
            if (GlassButtonPill(VSTR("Reset counters"), lay::StatsResetBtn(), true)) {
                g_left_total = 0;
                g_right_total = 0;
            }
        }
    }
    else if (g_tab == 2) {
        const ImRect cc = lay::ConfigCard();
        Card(dl, cc, VSTR("Config file"));
        const ImRect content = lay::CardContent(cc);
        // everything sits INSIDE the card's content box (the old build drew
        // the file row and the status line at the raw card edge — broken look)
        const char* file_lbl = VSTR("File");
        const ImVec2 fls = TextSizeS(file_lbl);
        const float file_cy = CenterTextY(ImRect(content.Min.x, lay::ConfigFileY(), content.Max.x,
                                                 lay::ConfigFileY() + 20.0f), fls.y);
        AddTextS(dl, ImVec2(content.Min.x, file_cy), A(TEXT_MUT), file_lbl);
        AddTextS(dl, ImVec2(content.Min.x + fls.x + 10.0f, file_cy),
                 A(ImVec4(0.80f, 0.83f, 0.90f, 1.0f)),
                 Ellipsize(host.config_path(host.user),
                           content.Max.x - content.Min.x - fls.x - 10.0f).c_str());

        const ImRect row = lay::ConfigBtnRow();
        const float bw = 150.0f, gap = 10.0f;
        if (GlassButtonPill(VSTR("Save now"), ImRect(row.Min.x, row.Min.y, row.Min.x + bw, row.Max.y))) {
            snprintf(g_config_status, sizeof(g_config_status),
                     SaveConfig(host.config_path(host.user)) ? "saved to %s" : "save failed (read-only folder?)",
                     host.config_path(host.user));
        }
        if (GlassButtonPill(VSTR("Reload"), ImRect(row.Min.x + bw + gap, row.Min.y, row.Min.x + 2 * bw + gap, row.Max.y))) {
            snprintf(g_config_status, sizeof(g_config_status),
                     LoadConfig(host.config_path(host.user)) ? "reloaded from disk" : "no abaser.ini yet");
        }
        if (GlassButtonPill(VSTR("Reset defaults"), ImRect(row.Min.x + 2 * (bw + gap), row.Min.y, row.Min.x + 3 * bw + 2 * gap, row.Max.y))) {
            ResetDefaults();
            snprintf(g_config_status, sizeof(g_config_status), "defaults restored (not saved yet)");
        }
        ImGui::SetCursorScreenPos(ImVec2(content.Min.x, lay::ConfigStatusY()));
        ImGui::PushTextWrapPos(content.Max.x);
        ImGui::TextDisabled("%s", g_config_status);
        ImGui::PopTextWrapPos();
    }
    else if (g_tab == 3) {
        // ---- theme card (Discord-style two-color theming) ----
        const ImRect tc = lay::AccentCard();
        Card(dl, tc, VSTR("Theme"));
        {
            // preset pairs: { primary, accent } — first is Discord's blurple
            static const unsigned presets[6][2] = {
                { 0x5865F2u, 0x8EA1FFu },   // blurple (Discord)
                { 0xFF3899u, 0xFF3899u },   // pink (classic Abaser)
                { 0x2563EBu, 0x60A5FAu },   // blue
                { 0x8B5CF6u, 0xC4B5FDu },   // purple
                { 0xF97316u, 0xFDBA74u },   // orange
                { 0x22C55Eu, 0x86EFACu },   // green
            };
            static const char* names[6] = { VSTR("Blurple"), VSTR("Pink"), VSTR("Blue"),
                                            VSTR("Purple"), VSTR("Orange"), VSTR("Green") };
            for (int i = 0; i < 6; i++) {
                const ImRect pr = lay::ThemePreset(tc, i);
                char idbuf[24];
                snprintf(idbuf, sizeof(idbuf), "##preset%d", i);
                ImGui::SetCursorScreenPos(pr.Min);
                ImGui::InvisibleButton(idbuf, pr.GetSize());
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    theme_primary = presets[i][0];
                    theme_accent  = presets[i][1];
                    SyncAccentFromTheme();
                }
                const bool selected = (theme_primary == presets[i][0] && theme_accent == presets[i][1]);
                // mini profile-banner preview: vertical gradient of the pair.
                // Borders here are layered FILLED rects, not stroked paths —
                // tiny stroked rounded rects come out dashed on some
                // rasterizers, layered fills never do.
                const ImVec4 prc = Im4FromRgb(presets[i][0]);
                if (selected)
                    dl->AddRectFilled(ImVec2(pr.Min.x - 2.5f, pr.Min.y - 2.5f), ImVec2(pr.Max.x + 2.5f, pr.Max.y + 2.5f),
                                      A(WHITE), 9.0f);
                else if (ImGui::IsItemHovered())
                    dl->AddRectFilled(ImVec2(pr.Min.x - 1.5f, pr.Min.y - 1.5f), ImVec2(pr.Max.x + 1.5f, pr.Max.y + 1.5f),
                                      AA(WHITE, 0.40f), 8.0f);
                dl->AddRectFilled(pr.Min, pr.Max, AA(WHITE, 0.30f), 7.0f);
                const ImVec2 gmin(pr.Min.x + 1.0f, pr.Min.y + 1.0f), gmax(pr.Max.x - 1.0f, pr.Max.y - 1.0f);
                dl->AddRectFilledMultiColor(gmin, gmax, A(prc), A(prc),
                                            A(ShadeColor(prc, 0.42f)), A(ShadeColor(prc, 0.42f)));
                if (ImGui::IsItemHovered()) TooltipS(names[i]);
            }

            // custom pickers: Primary (profile gradient) + Accent (controls)
            {
                const ImRect psw = lay::ThemePrimarySw(tc);
                const ImVec4 pc = Im4FromRgb(theme_primary);
                float prim[3] = { pc.x, pc.y, pc.z };
                ImGui::SetCursorScreenPos(psw.Min);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
                ImGui::PushStyleColor(ImGuiCol_Button, pc);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pc);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, pc);
                if (ImGui::ColorEdit3("##primary_c", prim,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                      ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoOptions))
                {
                    theme_primary = RgbFromIm4(ImVec4(prim[0], prim[1], prim[2], 1.0f));
                    SyncAccentFromTheme();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
                AddTextS(dl, ImVec2(psw.Max.x + 8.0f, CenterTextY(psw, TextSizeS(VSTR("Primary")).y)),
                         A(TEXT_MUT), VSTR("Primary"));
                if (ImGui::IsItemHovered()) TooltipS(VSTR("The background gradient color"));
            }
            {
                const ImRect asw = lay::ThemeAccentSw(tc);
                const ImVec4 ac = Im4FromRgb(theme_accent);
                float acc[3] = { ac.x, ac.y, ac.z };
                ImGui::SetCursorScreenPos(asw.Min);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
                ImGui::PushStyleColor(ImGuiCol_Button, ac);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ac);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ac);
                if (ImGui::ColorEdit3("##accent_c", acc,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                      ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoOptions))
                {
                    theme_accent = RgbFromIm4(ImVec4(acc[0], acc[1], acc[2], 1.0f));
                    SyncAccentFromTheme();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
                AddTextS(dl, ImVec2(asw.Max.x + 8.0f, CenterTextY(asw, TextSizeS(VSTR("Accent")).y)),
                         A(TEXT_MUT), VSTR("Accent"));
                if (ImGui::IsItemHovered()) TooltipS(VSTR("Switches, sliders and highlights"));
            }
        }

        const ImRect pc = lay::PrefsCard();
        Card(dl, pc, VSTR("Preferences"));
        GlassToggle(VSTR("Dim backdrop"), &other_background, lay::PrefsRow(pc, 0),
                    VSTR("Darkens the theme gradient behind the glass."));
        BindPill("hide", &hide_bind, BIND_HIDE, lay::PrefsRow(pc, 1));
        // body font + same color as the toggle labels — the three rows read
        // as one consistent list
        {
            ImFont* f = g_fonts.body;
            const ImRect row = lay::PrefsRow(pc, 1);
            dl->AddText(f, f->FontSize, ImVec2(row.Min.x, CenterTextY(row, f->FontSize)), A(TEXT_HI), VSTR("Hide window"));
        }
        {
            const ImRect row = lay::PrefsRow(pc, 2);
            const bool was_on = host.autostart_enabled(host.user);
            static bool mirror = false;
            mirror = was_on;
            GlassToggle(VSTR("Start with Windows"), &mirror, row,
                        VSTR("Adds a per-user Run entry (HKCU, no admin) that starts\nAbaser hidden in the tray at login."));
            if (mirror != was_on) {
                if (!host.set_autostart(host.user, mirror)) {
                    snprintf(g_config_status, sizeof(g_config_status),
                             "autostart change failed (registry write denied?)");
                } else {
                    snprintf(g_config_status, sizeof(g_config_status),
                             mirror ? "autostart on - starts in the tray at login" : "autostart removed");
                }
            }
        }

        const ImRect ab = lay::AboutCard();
        // no uppercase card title here — the card IS the about; name + version
        // centered as one block, FPS right-aligned on the version line
        AddGlassPanel(dl, ab, lay::CARD_R, CARD_FILL);
        const ImRect content = lay::CardContent(ab);
        ImFont* bf = g_fonts.body;
        const float name_h = bf->FontSize;
        const float block_h = name_h + 4.0f + g_fonts.small->FontSize;
        const float y0 = ab.Min.y + (ab.GetHeight() - block_h) * 0.5f;
        dl->AddText(bf, name_h, ImVec2(content.Min.x, y0), A(TEXT_HI), VSTR("Abaser Clicker"));
        AddTextS(dl, ImVec2(content.Min.x, y0 + name_h + 4.0f), A(TEXT_MUT), version);
        char fps[64];
        snprintf(fps, sizeof(fps), "%.1f FPS", io.Framerate);
        AddTextS(dl, ImVec2(content.Max.x - TextSizeS(fps).x, y0 + name_h + 4.0f), A(TEXT_MUT), fps);
    }

    // ---- footer ---------------------------------------------------------------
    {
        const ImRect fr = lay::FooterRect();
        dl->AddLine(ImVec2(fr.Min.x, fr.Min.y - 6.0f), ImVec2(fr.Max.x, fr.Min.y - 6.0f), AA(WHITE, 0.08f), 1.0f);
        const bool armed = left_enabled || right_enabled;
        char live[64];
        snprintf(live, sizeof(live), VSTR("%.1f L / %.1f R CPS live"), host.live_cps_left, host.live_cps_right);
        const float dot_cy = fr.Min.y + 4.0f + TextSizeS("0").y * 0.5f;
        dl->AddCircleFilled(ImVec2(fr.Min.x + 3.5f, dot_cy), 3.0f,
                            A(armed ? OK_GRN : ImVec4(1, 1, 1, 0.25f)), 12);
        AddTextS(dl, ImVec2(fr.Min.x + 12.0f, fr.Min.y + 4.0f), A(armed ? TEXT_HI : TEXT_MUT), live);

        // the right hint tells you WHY nothing is clicking when the mod pauses it
        char hint[128];
        const bool paused_menu = host.mod_linked && host.mod_gui_open;
        const bool paused_bow  = host.mod_linked && host.mod_holding_bow && !paused_menu;
        if (paused_menu) {
            snprintf(hint, sizeof(hint), "%s", VSTR("paused - a menu is open"));
        } else if (paused_bow) {
            snprintf(hint, sizeof(hint), "%s", VSTR("paused - bow drawn"));
        } else {
            snprintf(hint, sizeof(hint), VSTR("%s arm / disarm - %s hides to tray"),
                     GetKeyName(left_bind), GetKeyName(hide_bind));
        }
        const ImVec2 hs = TextSizeS(hint);
        AddTextS(dl, ImVec2(fr.Max.x - hs.x, fr.Min.y + 4.0f),
                 A(paused_menu || paused_bow ? WARN_AMB : TEXT_DIM), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
