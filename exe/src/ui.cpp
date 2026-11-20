// ============================================================================
//  Abaser Clicker — the glass UI
// ----------------------------------------------------------------------------
//  Apple "Liquid Glass" design language, realized inside Dear ImGui:
//
//    * one translucent-material canvas: ambient gradient + two soft light
//      glows behind everything, glass panels (white @ ~6% alpha, 1px light
//      borders, top specular highlight) floating on it
//    * big continuous radii everywhere (16-18px cards, pill controls)
//    * iOS system colors for status; the user accent stays configurable
//    * real controls: toggle switches, one-row sliders, segmented tabs,
//      pill buttons — all drawn by hand, all mouse-first
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

static void AddTextS(ImDrawList* dl, const ImVec2& pos, ImU32 col, const char* txt)   // small font
{
    dl->AddText(g_fonts.small, g_fonts.small->FontSize, pos, col, txt);
}
static ImVec2 TextSizeS(const char* txt)
{
    return g_fonts.small->CalcTextSizeA(g_fonts.small->FontSize, FLT_MAX, 0.0f, txt);
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
    s.FrameRounding     = 10.0f;
    s.PopupRounding     = 14.0f;
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
// at the right edge of `row`. Returns true when the value flipped.
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

    // track
    const ImVec4 track = *v ? accent : ImVec4(1, 1, 1, hovered ? 0.14f : 0.09f);
    const float round_t = lay::SW_H * 0.5f;
    dl->AddRectFilled(sw.Min, sw.Max, A(track), round_t);
    if (!*v)
        dl->AddRect(ImVec2(sw.Min.x + 0.5f, sw.Min.y + 0.5f), ImVec2(sw.Max.x - 0.5f, sw.Max.y - 0.5f),
                    AA(WHITE, hovered ? 0.22f : 0.14f), round_t, 0, 1.0f);
    // knob with a soft drop shadow
    const float kd = lay::SW_H - 6.0f;
    const float kx = sw.Min.x + 3.0f + kn * (lay::SW_W - kd - 6.0f);
    const ImVec2 kc(kx + kd * 0.5f, sw.Min.y + lay::SW_H * 0.5f);
    AddRadialGlow(dl, ImVec2(kc.x, kc.y + 1.0f), kd * 0.75f, ImVec4(0, 0, 0, 1), 0.35f);
    dl->AddCircleFilled(kc, kd * 0.5f, A(WHITE), 24);
    dl->AddCircle(kc, kd * 0.5f - 0.5f, AA(WHITE, 0.35f), 24, 1.0f);

    // label on the left, vertically centered
    ImFont* f = g_fonts.body;
    const float lh = f->FontSize;
    dl->AddText(ImVec2(row.Min.x, (row.Min.y + row.Max.y) * 0.5f - lh * 0.5f), A(TEXT_HI), label);

    if (tip && hovered) {
        ImGui::PushFont(g_fonts.small);
        ImGui::SetTooltip("%s", tip);
        ImGui::PopFont();
    }
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
    const bool dragging = ImGui::GetStateStorage()->GetBoolRef(id, false);
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
                      AA(WHITE, 0.10f), th * 0.5f);
    const float fillw = pad + frac * ((track.Max.x - track.Min.x) - 2 * pad);
    if (fillw > 2.0f)
        dl->AddRectFilled(ImVec2(track.Min.x, cy - th * 0.5f), ImVec2(track.Min.x + fillw, cy + th * 0.5f),
                          AA(accent, 0.85f), th * 0.5f);
    const float kx = track.Min.x + pad + frac * ((track.Max.x - track.Min.x) - 2 * pad);
    const float kr = hovered || active ? 8.0f : 7.0f;
    AddRadialGlow(dl, ImVec2(kx, cy + 1.0f), kr * 1.5f, ImVec4(0, 0, 0, 1), 0.30f);
    dl->AddCircleFilled(ImVec2(kx, cy), kr, A(WHITE), 24);
    dl->AddCircle(ImVec2(kx, cy), kr - 0.5f, AA(accent, active ? 0.9f : 0.45f), 24, 1.5f);

    if (tip && hovered) {
        ImGui::PushFont(g_fonts.small);
        ImGui::SetTooltip("%s", tip);
        ImGui::PopFont();
    }
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
    dl->AddText(ImVec2((r.Min.x + r.Max.x) * 0.5f - ts.x * 0.5f, (r.Min.y + r.Max.y) * 0.5f - ts.y * 0.5f),
                A(hovered ? TEXT_HI : ImVec4(0.85f, 0.88f, 0.94f, 1.0f)), label);
    return ImGui::IsItemClicked(ImGuiMouseButton_Left);
}

// Bind pill: shows the current key; click to start/stop capture.
static bool BindPill(const char* label, int* bind, int which, const ImRect& row)
{
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    ImDrawList* dl = win->DrawList;
    const ImRect r = lay::PillIn(row);
    char idbuf[64];
    snprintf(idbuf, sizeof(idbuf), "##bind_%s", label);
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
                            : ImVec4(1, 1, 1, hovered ? 0.22f : 0.13f)),
                r.GetHeight() * 0.5f, 0, 1.0f);

    char txt[24];
    if (capturing) snprintf(txt, sizeof(txt), "%s", VSTR("press a key"));
    else           snprintf(txt, sizeof(txt), "%s", GetKeyName(*bind)[0] ? GetKeyName(*bind) : VSTR("unbound"));
    ImFont* f = g_fonts.small;
    const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, txt);
    dl->AddText(ImVec2((r.Min.x + r.Max.x) * 0.5f - ts.x * 0.5f, (r.Min.y + r.Max.y) * 0.5f - ts.y * 0.5f),
                A(capturing ? accent : TEXT_HI), txt);
    return clicked;
}

// Status chip (non-interactive): dot + text on a small glass pill.
static void StatusChip(ImDrawList* dl, float right_x, float y, const ImVec4& dot, const char* txt, ImVec2* cursor_out)
{
    const ImVec2 ts = TextSizeS(txt);
    const float h = 22.0f;
    const float w = ts.x + 8.0f + 8.0f + 8.0f;
    const float x = right_x - w;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), AA(WHITE, 0.075f), h * 0.5f);
    dl->AddRect(ImVec2(x + 0.5f, y + 0.5f), ImVec2(x + w - 0.5f, y + h - 0.5f), AA(WHITE, 0.12f), h * 0.5f, 0, 1.0f);
    dl->AddCircleFilled(ImVec2(x + 13.0f, y + h * 0.5f), 3.0f, A(dot), 12);
    AddTextS(dl, ImVec2(x + 22.0f, y + (h - ts.y) * 0.5f), A(ImVec4(0.85f, 0.88f, 0.94f, 1.0f)), txt);
    if (cursor_out) *cursor_out = ImVec2(x - 6.0f, y);
}

// Card with small uppercase title; returns content box.
static ImRect Card(ImDrawList* dl, const ImRect& r, const char* title)
{
    AddGlassPanel(dl, r, 16.0f, CARD_FILL);
    char up[64];
    size_t n = 0;
    for (; n < 63 && title[n]; n++)
        up[n] = (char)((title[n] >= 'a' && title[n] <= 'z') ? title[n] - 32 : title[n]);
    up[n] = 0;
    AddTextS(dl, ImVec2(r.Min.x + 14.0f, r.Min.y + 11.0f), AA(TEXT_MUT, 0.85f), up);
    return lay::CardContent(r);
}

// --- background -----------------------------------------------------------------
static void PaintBackground(ImDrawList* dl)
{
    const ImVec2 sz = lay::Client();
    const bool cool = other_background;
    const ImVec4 top = cool ? ImVec4(0.043f, 0.055f, 0.090f, 1) : ImVec4(0.075f, 0.052f, 0.060f, 1);
    const ImVec4 bot = cool ? ImVec4(0.098f, 0.110f, 0.155f, 1) : ImVec4(0.133f, 0.098f, 0.102f, 1);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), sz, A(top), A(top), A(bot), A(bot));

    // ambient light glows — the "liquid" in liquid glass
    AddRadialGlow(dl, ImVec2(sz.x * 0.95f, 26.0f), 250.0f, ImVec4(accent.x, accent.y, accent.z, 1), 0.11f);
    AddRadialGlow(dl, ImVec2(sz.x * 0.05f, sz.y + 20.0f), 230.0f,
                  cool ? ImVec4(0.25f, 0.45f, 1.00f, 1) : ImVec4(0.95f, 0.45f, 0.35f, 1), 0.10f);
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

    // ---- header ------------------------------------------------------------
    {
        ImFont* tf = g_fonts.title;
        dl->AddText(g_fonts.title, tf->FontSize, ImVec2(lay::MARGIN, 14.0f), A(TEXT_HI), VSTR("Abaser Clicker"));
        AddTextS(dl, ImVec2(lay::MARGIN, 42.0f), AA(TEXT_DIM, 1.0f), version);

        // status chips, right-aligned
        ImVec2 cur(lay::ContentRight(), 14.0f);
        StatusChip(dl, cur.x, 14.0f,
                   host.mc_found(host.user) ? OK_GRN : BAD_RED,
                   host.mc_found(host.user) ? VSTR("Minecraft") : VSTR("No game"), &cur);
        StatusChip(dl, cur.x, 14.0f, left_enabled ? accent : ImVec4(1, 1, 1, 0.25f),
                   left_enabled ? VSTR("L on") : VSTR("L off"), &cur);
        StatusChip(dl, cur.x, 14.0f, right_enabled ? accent : ImVec4(1, 1, 1, 0.25f),
                   right_enabled ? VSTR("R on") : VSTR("R off"), &cur);
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
                dl->AddRectFilled(pill.Min, pill.Max, AA(WHITE, 0.14f), pill.GetHeight() * 0.5f);
                dl->AddRect(ImVec2(pill.Min.x + 0.5f, pill.Min.y + 0.5f), ImVec2(pill.Max.x - 0.5f, pill.Max.y - 0.5f),
                            AA(WHITE, 0.22f), pill.GetHeight() * 0.5f, 0, 1.0f);
                dl->AddLine(ImVec2(pill.Min.x + pill.GetWidth() * 0.25f, pill.Min.y + 1.5f),
                            ImVec2(pill.Max.x - pill.GetWidth() * 0.25f, pill.Min.y + 1.5f),
                            AA(WHITE, 0.35f), 1.0f);
            } else if (hovered) {
                dl->AddRectFilled(sr.Min, sr.Max, AA(WHITE, 0.05f), sr.GetHeight() * 0.5f - 2.0f);
            }
            ImFont* f = g_fonts.body;
            const ImVec2 ts = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, names[i]);
            dl->AddText(ImVec2((sr.Min.x + sr.Max.x) * 0.5f - ts.x * 0.5f, (sr.Min.y + sr.Max.y) * 0.5f - ts.y * 0.5f),
                        A(active ? TEXT_HI : (hovered ? ImVec4(0.82f, 0.85f, 0.92f, 1.0f) : TEXT_MUT)), names[i]);
        }
    }

    // ---- tab content ---------------------------------------------------------
    if (g_tab == 0) {
        // keep min below max
        if (left_min_cps >= left_max_cps)  left_min_cps  = left_max_cps - 0.5f;
        if (right_min_cps >= right_max_cps) right_min_cps = right_max_cps - 0.5f;

        // two glass cards, left & right clicker
        const ImRect lc = lay::LeftCard(), rc = lay::RightCard();
        {
            const ImRect row = lay::SideToggle(lc);
            char lbl[32]; snprintf(lbl, sizeof(lbl), "%s", VSTR("Left clicker"));
            GlassToggle(lbl, &left_enabled, row);
            BindPill("left", &left_bind, BIND_LEFT, lay::SideBind(lc));
            AddTextS(dl, ImVec2(lay::CardContent(lc).Min.x,
                                (lay::SideBind(lc).Min.y + lay::SideBind(lc).Max.y) * 0.5f - TextSizeS(VSTR("Hotkey")).y * 0.5f),
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
                                (lay::SideBind(rc).Min.y + lay::SideBind(rc).Max.y) * 0.5f - TextSizeS(VSTR("Hotkey")).y * 0.5f),
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
        GlassToggle(VSTR("Block Hit"), &blockhit, lay::BlockToggle(),
                    VSTR("Sword-block taps ride along with left clicks."));
        GlassSliderI("bh_ch", VSTR("Chance"), &blockhit_chance, BH_LO, BH_HI, "%d%%", lay::BlockSlider());
        AddTextS(dl, ImVec2(lay::MARGIN + 14, 224.0f), A(TEXT_DIM), VSTR("Note: applies to the left clicker only."));

        const ImRect sc = lay::StatsCard();
        Card(dl, sc, VSTR("Statistics"));
        {
            std::string l = FormatCount(g_left_total);
            std::string r = FormatCount(g_right_total);
            ImFont* tf = g_fonts.title;
            dl->AddText(g_fonts.title, tf->FontSize, ImVec2(sc.Min.x + 14, 288.0f), A(TEXT_HI), l.c_str());
            AddTextS(dl, ImVec2(sc.Min.x + 14, 288.0f + tf->FontSize + 4.0f), A(TEXT_MUT), VSTR("left clicks"));
            dl->AddText(g_fonts.title, tf->FontSize, ImVec2(sc.Min.x + 150, 288.0f), A(TEXT_HI), r.c_str());
            AddTextS(dl, ImVec2(sc.Min.x + 150, 288.0f + tf->FontSize + 4.0f), A(TEXT_MUT), VSTR("right clicks"));
            if (GlassButtonPill(VSTR("Reset counters"), lay::StatsResetBtn())) {
                g_left_total = 0;
                g_right_total = 0;
            }
        }
    }
    else if (g_tab == 2) {
        const ImRect cc = lay::ConfigCard();
        Card(dl, cc, VSTR("Config file"));
        AddTextS(dl, ImVec2(cc.Min.x, 148.0f), A(TEXT_MUT), VSTR("File"));
        AddTextS(dl, ImVec2(cc.Min.x + 44.0f, 148.0f),
                 A(ImVec4(0.80f, 0.83f, 0.90f, 1.0f)),
                 Ellipsize(host.config_path(host.user), cc.Max.x - cc.Min.x - 44.0f).c_str());

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
        ImGui::SetCursorScreenPos(ImVec2(cc.Min.x, 238.0f));
        ImGui::PushTextWrapPos(cc.Max.x);
        ImGui::TextDisabled("%s", g_config_status);
        ImGui::PopTextWrapPos();
    }
    else if (g_tab == 3) {
        const ImRect ac = lay::AccentCard();
        Card(dl, ac, VSTR("Accent"));
        {
            static const ImVec4 presets[6] = {
                ImVec4(1.00f, 0.22f, 0.55f, 1.0f),   // pink
                ImVec4(1.00f, 0.14f, 1.00f, 1.0f),   // magenta (classic)
                ImVec4(0.20f, 0.52f, 1.00f, 1.0f),   // blue
                ImVec4(0.72f, 0.40f, 0.98f, 1.0f),   // purple
                ImVec4(1.00f, 0.58f, 0.20f, 1.0f),   // orange
                ImVec4(0.35f, 0.85f, 0.45f, 1.0f),   // green
            };
            const float r = 13.0f;
            const float cy = ac.Min.y + 46.0f;
            for (int i = 0; i < 6; i++) {
                const float cx = ac.Min.x + 22.0f + i * 38.0f;
                char idbuf[24];
                snprintf(idbuf, sizeof(idbuf), "##preset%d", i);
                ImGui::SetCursorScreenPos(ImVec2(cx - r - 2, cy - r - 2));
                ImGui::InvisibleButton(idbuf, ImVec2(2 * r + 4, 2 * r + 4));
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    accent = presets[i];
                const bool selected = fabsf(accent.x - presets[i].x) < 0.01f && fabsf(accent.y - presets[i].y) < 0.01f
                                   && fabsf(accent.z - presets[i].z) < 0.01f;
                dl->AddCircleFilled(ImVec2(cx, cy), r, A(presets[i]), 28);
                if (selected)
                    dl->AddCircle(ImVec2(cx, cy), r + 3.5f, A(WHITE), 28, 2.0f);
                else if (ImGui::IsItemHovered())
                    dl->AddCircle(ImVec2(cx, cy), r + 2.0f, AA(WHITE, 0.35f), 28, 1.5f);
            }
            // custom color pill (small ColorEdit)
            ImGui::SetCursorScreenPos(ImVec2(ac.Max.x - 14.0f - 92.0f, ac.Min.y + 33.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.0f);
            ImGui::ColorEdit3("##custom", (float*)&accent,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoBorder);
            ImGui::PopStyleVar();
            AddTextS(dl, ImVec2(ac.Max.x - 14.0f - 52.0f, ac.Min.y + 40.0f), A(TEXT_MUT), VSTR("Custom"));
        }

        const ImRect pc = lay::PrefsCard();
        Card(dl, pc, VSTR("Preferences"));
        GlassToggle(VSTR("Cool backdrop"), &other_background, lay::PrefsRow(pc, 0),
                    VSTR("Bluish slate vs warm charcoal behind the glass."));
        BindPill("hide", &hide_bind, BIND_HIDE, lay::PrefsRow(pc, 1));
        AddTextS(dl, ImVec2(pc.Min.x + 14,
                            (lay::PrefsRow(pc, 1).Min.y + lay::PrefsRow(pc, 1).Max.y) * 0.5f - TextSizeS(VSTR("Hide window")).y * 0.5f),
                 A(TEXT_MUT), VSTR("Hide window"));
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
        Card(dl, ab, VSTR("About"));
        dl->AddText(ImVec2(ab.Min.x + 14, ab.Min.y + 30.0f), A(TEXT_HI), VSTR("Abaser Clicker"));
        AddTextS(dl, ImVec2(ab.Min.x + 14, ab.Min.y + 52.0f), A(TEXT_MUT), version);
        char fps[64];
        snprintf(fps, sizeof(fps), "%.1f FPS", io.Framerate);
        AddTextS(dl, ImVec2(ab.Max.x - 14.0f - TextSizeS(fps).x, ab.Min.y + 52.0f), A(TEXT_MUT), fps);
    }

    // ---- footer ---------------------------------------------------------------
    {
        const ImRect fr = lay::FooterRect();
        dl->AddLine(ImVec2(fr.Min.x, fr.Min.y - 6.0f), ImVec2(fr.Max.x, fr.Min.y - 6.0f), AA(WHITE, 0.08f), 1.0f);
        const bool armed = left_enabled || right_enabled;
        char live[64];
        snprintf(live, sizeof(live), VSTR("%.1f L / %.1f R CPS live"), host.live_cps_left, host.live_cps_right);
        dl->AddCircleFilled(ImVec2(fr.Min.x + 3.5f, fr.Min.y + 9.0f), 3.0f,
                            A(armed ? OK_GRN : ImVec4(1, 1, 1, 0.25f)), 12);
        AddTextS(dl, ImVec2(fr.Min.x + 12.0f, fr.Min.y + 4.0f), A(armed ? TEXT_HI : TEXT_MUT), live);
        const char* hint = VSTR("F4 arm / disarm - INSERT hides to tray");
        const ImVec2 hs = TextSizeS(hint);
        AddTextS(dl, ImVec2(fr.Max.x - hs.x, fr.Min.y + 4.0f), A(TEXT_DIM), hint);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
