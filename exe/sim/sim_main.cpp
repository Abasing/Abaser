// ============================================================================
//  sim_main.cpp — headless UI simulator & functional test harness
// ----------------------------------------------------------------------------
//  Compiles the REAL ui.cpp (the exact code the Windows exe draws) without
//  any Win32/DX9 dependency, feeds it synthetic input events, rasterizes the
//  draw data with the software rasterizer (sim_raster.h) and saves BMP
//  screenshots. Because the UI layer is shared verbatim, a click that works
//  here works in the exe — this is how the "clicks do nothing" bug was
//  reproduced and how its fix is regression-tested.
//
//  Usage:  ./sim <output_dir>
//  Prints PASS/FAIL per test, exits non-zero if any test fails.
// ============================================================================
#include "imgui.h"
#include "../src/ui.h"
#include "../src/ui_state.h"
#include "../src/abaser_compression.h"
#include "../src/abaser_assets.h"
#include "sim_raster.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cmath>

// ---------------------------------------------------------------------------
// BMP writer (24bpp, bottom-up) — no external deps
// ---------------------------------------------------------------------------
static void SaveBMP(const SimFB& fb, const std::string& path)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); return; }
    const int w = fb.w, h = fb.h;
    const int rowBytes = (w * 3 + 3) & ~3;
    const int dataSize = rowBytes * h;
    const int fileSize = 14 + 40 + dataSize;

    uint8_t hdr[14] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    *(uint32_t*)&hdr[2] = (uint32_t)fileSize;
    *(uint32_t*)&hdr[10] = (uint32_t)(14 + 40);
    fwrite(hdr, 1, 14, f);

    uint8_t dib[40] = {};
    *(uint32_t*)&dib[0] = (uint32_t)40;
    *(int32_t*)&dib[4] = (int32_t)w;
    *(int32_t*)&dib[8] = (int32_t)h;
    *(uint16_t*)&dib[12] = 1;
    *(uint16_t*)&dib[14] = 24;
    *(uint32_t*)&dib[20] = (uint32_t)dataSize;
    fwrite(dib, 1, 40, f);

    std::vector<uint8_t> row((size_t)rowBytes, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            const uint32_t c = fb.px[(size_t)y * (size_t)w + (size_t)x];
            row[x * 3 + 0] = (uint8_t)((c >> IM_COL32_B_SHIFT) & 0xFFu);
            row[x * 3 + 1] = (uint8_t)((c >> IM_COL32_G_SHIFT) & 0xFFu);
            row[x * 3 + 2] = (uint8_t)((c >> IM_COL32_R_SHIFT) & 0xFFu);
        }
        fwrite(row.data(), 1, (size_t)rowBytes, f);
    }
    fclose(f);
}

// ---------------------------------------------------------------------------
// Fake platform host (scripted)
// ---------------------------------------------------------------------------
static bool  g_sim_mc_found  = true;
static bool  g_sim_autostart = false;
static int   g_sim_bind_key  = 0;      // what PollBindKey reports this frame
static float g_sim_cps_l = 0.0f, g_sim_cps_r = 0.0f;
static int   g_min_calls = 0, g_close_calls = 0;

static bool  SimMcFound(void*)            { return g_sim_mc_found; }
static bool  SimAutostart(void*)          { return g_sim_autostart; }
static bool  SimSetAutostart(void*, bool on) { g_sim_autostart = on; return true; }
static const char* SimConfigPath(void*)   { return "/tmp/abaser_sim.ini"; }
static int   SimPollBindKey(void*)        { return g_sim_bind_key; }
static void  SimMinimize(void*)           { g_min_calls++; }
static void  SimCloseToTray(void*)        { g_close_calls++; }

// ---------------------------------------------------------------------------
// Test plumbing
// ---------------------------------------------------------------------------
static int g_failures = 0;
static void Check(bool ok, const char* what)
{
    printf("%-4s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_failures++;
}

static SimFB        g_fb;
static ImFont*      g_font_title = nullptr, *g_font_body = nullptr, *g_font_small = nullptr;
static AbaserUIHost g_host;
static const char*  g_version = "1.8.8-sim";

static void RunFrames(int n, const char* shot = nullptr)
{
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < n; i++) {
        io.DisplaySize = ImVec2(lay::CLIENT_W, lay::CLIENT_H);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();
        DrawUI(g_host, g_version);
        ImGui::Render();
        SimRenderDrawData(ImGui::GetDrawData(), g_fb,
                          (const uint32_t*)ImGui::GetIO().Fonts->TexPixelsRGBA32,
                          ImGui::GetIO().Fonts->TexWidth, ImGui::GetIO().Fonts->TexHeight);
    }
    if (shot) SaveBMP(g_fb, std::string(shot));
}

static void MouseMove(float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); }
static void MouseDown(int b = 0)        { ImGui::GetIO().AddMouseButtonEvent(b, true); }
static void MouseUp(int b = 0)          { ImGui::GetIO().AddMouseButtonEvent(b, false); }

static void ClickAt(float x, float y, int settle_frames = 4)
{
    MouseMove(x, y);
    RunFrames(2);
    MouseDown();
    RunFrames(2);
    MouseUp();
    RunFrames(settle_frames);
}

static void DragFromTo(float x0, float y0, float x1, float y1)
{
    MouseMove(x0, y0);
    RunFrames(2);
    MouseDown();
    RunFrames(2);
    const int steps = 10;
    for (int s = 1; s <= steps; s++) {
        MouseMove(x0 + (x1 - x0) * (float)s / (float)steps,
                  y0 + (y1 - y0) * (float)s / (float)steps);
        RunFrames(1);
    }
    MouseUp();
    RunFrames(3);
}

static ImVec2 Center(const ImRect& r) { return r.GetCenter(); }

int main(int argc, char** argv)
{
    const std::string outdir = argc > 1 ? argv[1] : ".";
    const std::string dir = outdir.back() == '/' ? outdir : outdir + "/";

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // fonts — same TTF, same sizes as the exe
    static std::vector<uint8_t> ttf;
    ImFontConfig fc;
    fc.FontDataOwnedByAtlas = false;
    if (abaser::decode_binary_asset(ABASER_FONT_UBUNTU, sizeof(ABASER_FONT_UBUNTU), ttf) && ttf.size() > 4) {
        g_font_title = io.Fonts->AddFontFromMemoryTTF(ttf.data(), (int)ttf.size(), 16.0f, &fc);
        g_font_body  = io.Fonts->AddFontFromMemoryTTF(ttf.data(), (int)ttf.size(), 14.0f, &fc);
        g_font_small = io.Fonts->AddFontFromMemoryTTF(ttf.data(), (int)ttf.size(), 12.0f, &fc);
    } else {
        g_font_title = io.Fonts->AddFontDefault();
        g_font_body  = io.Fonts->AddFontDefault();
        g_font_small = io.Fonts->AddFontDefault();
    }
    g_fonts.title = g_font_title;
    g_fonts.body  = g_font_body;
    g_fonts.small = g_font_small;

    unsigned char* tex_px = nullptr;
    int tw = 0, th = 0;
    io.Fonts->GetTexDataAsRGBA32(&tex_px, &tw, &th);

    g_fb.w = (int)lay::CLIENT_W;
    g_fb.h = (int)lay::CLIENT_H;
    g_fb.px.assign((size_t)g_fb.w * (size_t)g_fb.h, 0xFF000000u);

    g_host.mc_found          = &SimMcFound;
    g_host.autostart_enabled = &SimAutostart;
    g_host.set_autostart     = &SimSetAutostart;
    g_host.config_path       = &SimConfigPath;
    g_host.poll_bind_key     = &SimPollBindKey;
    g_host.minimize          = &SimMinimize;
    g_host.close_to_tray     = &SimCloseToTray;
    g_host.user              = nullptr;

    // ------------------------------------------------------------------
    // WARM-UP + FIRST SCREENSHOT (Clicker tab, default state)
    // ------------------------------------------------------------------
    RunFrames(5, (dir + "01_clicker_default.bmp").c_str());

    // ------------------------------------------------------------------
    // FRAMELESS TITLEBAR: the drawn min / X buttons must be clickable
    // (in the exe the rest of the band is the OS drag region).
    // ------------------------------------------------------------------
    ClickAt(Center(lay::TBMinBtn()).x, Center(lay::TBMinBtn()).y);
    Check(g_min_calls == 1, "click drawn minimize button -> host minimize");
    ClickAt(Center(lay::TBCloseBtn()).x, Center(lay::TBCloseBtn()).y);
    Check(g_close_calls == 1, "click drawn close button -> host close-to-tray");

    // ------------------------------------------------------------------
    // CLICK GATES (ui_state.h): the single policy the engine and this sim
    // share — an open GUI blocks everything, spawn box + ally crosshair block
    // everything (when the gate is on), a drawn bow blocks right only.
    // ------------------------------------------------------------------
    {
        hold_mode = true;
        mod_gui_open = false; mod_holding_bow = false;
        mod_in_spawn = false; mod_ally_crosshair = false;
        Check(GateLeft(true, true) == true,  "gate: left fires in-game with key held");
        Check(GateRight(true, true) == true, "gate: right fires in-game with key held");
        mod_gui_open = true;
        Check(GateLeft(true, true) == false,  "gate: open GUI blocks left");
        Check(GateRight(true, true) == false, "gate: open GUI blocks right (chest, inv, villager, ...)");
        mod_gui_open = false; mod_holding_bow = true;
        Check(GateLeft(true, true) == true,   "gate: drawn bow — left keeps clicking");
        Check(GateRight(true, true) == false, "gate: drawn bow blocks the right autoclicker");
        Check(GateBlockHit() == false,        "gate: drawn bow blocks block-hit taps");
        mod_holding_bow = false;
        Check(GateLeft(false, true) == false, "gate: no game window blocks everything");
        Check(GateRight(false, true) == false, "gate: no game window blocks right too");
        hold_mode = false;
        Check(GateLeft(true, false) == true,  "gate: non-hold mode clicks nonstop");
        hold_mode = true;
        // spawn gate: the mod's spawn box holds every click while enabled
        spawn_gate = true; mod_in_spawn = true;
        Check(GateLeft(true, true) == false,  "gate: inside spawn blocks left (spawn_gate on)");
        Check(GateRight(true, true) == false, "gate: inside spawn blocks right");
        Check(GateBlockHit() == false,        "gate: inside spawn blocks block-hit taps");
        spawn_gate = false;
        Check(GateLeft(true, true) == true,   "gate: spawn gate off — spawn box alone does not block");
        spawn_gate = true; mod_in_spawn = false;
        // ally crosshair: team member / excepted player under the sights
        mod_ally_crosshair = true;
        Check(GateLeft(true, true) == false,  "gate: ally under crosshair blocks left");
        Check(GateRight(true, true) == false, "gate: ally under crosshair blocks right");
        Check(GateBlockHit() == false,        "gate: ally under crosshair blocks block-hit taps");
        mod_ally_crosshair = false;
        Check(GateLeft(true, true) == true,   "gate: ally flag clear — gates open again");
    }

    // ------------------------------------------------------------------
    // CLICK TIMING, regression-tested (shared engine math):
    // the legacy right-click gap once read left_max_cps — the right channel
    // used the wrong range for its 300/400 divisor. The math now lives in
    // one shared function and both channels must pass it.
    // ------------------------------------------------------------------
    {
        long long p = 0, g = 0;
        // legacy, cold draw (cps2 = 10): press 100/10, gap 400/10
        ClickTiming(true, 10.0f, 10.0f, 0.5f, 0.5f, &p, &g);
        Check(p == 10 && g == 40, "timing: legacy cold draw -> 400 divisor, own range");
        // legacy, hot draw (cps2 = 20): press 100/20, gap 300/20 — the case
        // that broke when the right channel read the LEFT max cps
        ClickTiming(true, 20.0f, 20.0f, 0.5f, 0.5f, &p, &g);
        Check(p == 5 && g == 15, "timing: legacy hot draw -> 300 divisor, own range");
        // legacy with a spread: same range for press and gap draws
        ClickTiming(true, 8.0f, 16.0f, 0.0f, 1.0f, &p, &g);
        Check(p == 12 && g == 18, "timing: legacy spread draw -> press 100/8, gap 300/16");
        // normal mode: 2 ms press, remainder of 1000/cps as the gap
        ClickTiming(false, 10.0f, 10.0f, 0.5f, 0.5f, &p, &g);
        Check(p == 2 && g == 98, "timing: normal mode -> 2 ms press, 1000/cps cycle");
        ClickTiming(false, 25.0f, 25.0f, 0.5f, 0.5f, &p, &g);
        Check(p == 2 && g == 38, "timing: normal mode at 25 cps -> 40 ms cycle");
    }

    // ------------------------------------------------------------------
    // THE CLICK BUG, regression-tested:
    // every one of these used to land on the 800x800 SkipItems overlay
    // window and do nothing. Now they must all work.
    // ------------------------------------------------------------------
    const ImRect lsw = lay::SwitchIn(lay::SideToggle(lay::LeftCard()));
    ClickAt(Center(lsw).x, Center(lsw).y);
    Check(left_enabled == true, "click left toggle -> left clicker arms");

    const ImRect rsw = lay::SwitchIn(lay::SideToggle(lay::RightCard()));
    ClickAt(Center(rsw).x, Center(rsw).y);
    Check(right_enabled == true, "click right toggle -> right clicker arms");

    // segmented tabs
    const ImRect seg1 = lay::SegSeg(1);
    ClickAt(Center(seg1).x, Center(seg1).y);
    Check(g_tab == 1, "click Misc tab -> tab switches");
    RunFrames(2, (dir + "02_misc.bmp").c_str());

    const ImRect seg2 = lay::SegSeg(2);
    ClickAt(Center(seg2).x, Center(seg2).y);
    Check(g_tab == 2, "click Configs tab -> tab switches");
    RunFrames(2, (dir + "03_configs.bmp").c_str());

    const ImRect seg3 = lay::SegSeg(3);
    ClickAt(Center(seg3).x, Center(seg3).y);
    Check(g_tab == 3, "click Settings tab -> tab switches");
    RunFrames(2, (dir + "04_settings.bmp").c_str());

    const ImRect seg0 = lay::SegSeg(0);
    ClickAt(Center(seg0).x, Center(seg0).y);
    Check(g_tab == 0, "click Clicker tab -> back to tab 0");

    // back on Clicker tab: toggle off again (flip back check)
    ClickAt(Center(lsw).x, Center(lsw).y);
    Check(left_enabled == false, "click left toggle again -> disarms");

    // ------------------------------------------------------------------
    // SLIDER DRAG (left CPS max)
    // ------------------------------------------------------------------
    const ImRect sl0 = lay::SideSlider(lay::LeftCard(), 0);
    const float fy = sl0.Min.y + 24.0f;                 // inside the grab strip
    const float left_before = left_max_cps;
    DragFromTo(sl0.Min.x + 30.0f, fy, sl0.Max.x - 6.0f, fy);
    Check(left_max_cps > left_before, "drag CPS max slider -> value increases");

    // mid-drag screenshot for visual QA
    MouseMove(sl0.Min.x + 30.0f, fy);
    RunFrames(2);
    MouseDown();
    RunFrames(1);
    MouseMove(sl0.Min.x + (sl0.Max.x - sl0.Min.x) * 0.72f, fy);
    RunFrames(2, (dir + "05_slider_drag.bmp").c_str());
    MouseUp();
    RunFrames(3);

    // int slider (left chance)
    const ImRect sl2 = lay::SideSlider(lay::LeftCard(), 2);
    const int chance_before = hit_chance;
    DragFromTo(sl2.Min.x + (sl2.Max.x - sl2.Min.x) * 0.8f, sl2.Min.y + 24.0f, sl2.Min.x + 8.0f, sl2.Min.y + 24.0f);
    Check(hit_chance < chance_before, "drag Chance slider left -> value decreases");

    // ------------------------------------------------------------------
    // ACTIVATION TOGGLES
    // ------------------------------------------------------------------
    const ImRect act0 = lay::SwitchIn(lay::ActToggle(lay::ActivationCard(), 0));
    ClickAt(Center(act0).x, Center(act0).y);
    Check(hold_mode == false, "click Hold-to-click switch -> flips off");

    const ImRect act2 = lay::SwitchIn(lay::ActToggle(lay::ActivationCard(), 2));
    ClickAt(Center(act2).x, Center(act2).y);
    Check(legacy_timing == true, "click Legacy timing switch -> flips on");

    // hover screenshot on an activation toggle (tooltip appears)
    MouseMove(Center(lay::SwitchIn(lay::ActToggle(lay::ActivationCard(), 1))).x,
              Center(lay::SwitchIn(lay::ActToggle(lay::ActivationCard(), 1))).y);
    RunFrames(25, (dir + "06_hover_tooltip.bmp").c_str());

    // ------------------------------------------------------------------
    // NON-BLOCKING BIND CAPTURE
    // ------------------------------------------------------------------
    const ImRect pillL = lay::PillIn(lay::SideBind(lay::LeftCard()));
    ClickAt(Center(pillL).x, Center(pillL).y);
    Check(g_bind_capture == BIND_LEFT, "click bind pill -> capture starts");
    RunFrames(2, (dir + "07_bind_capture.bmp").c_str());
    RunFrames(2);                       // a clean "no key" frame (arms capture)
    g_sim_bind_key = 70;                // 'F' held
    RunFrames(2);
    Check(left_bind == 70, "press F while capturing -> bind becomes F");
    g_sim_bind_key = 0;
    RunFrames(2, (dir + "08_bind_set.bmp").c_str());

    // cancel capture by clicking the pill again
    ClickAt(Center(pillL).x, Center(pillL).y);
    Check(g_bind_capture == BIND_LEFT, "click bind pill -> capture starts again");
    ClickAt(Center(pillL).x, Center(pillL).y);
    Check(g_bind_capture == BIND_NONE, "click bind pill while capturing -> cancels");
    Check(left_bind == 70, "cancelled capture keeps the old bind");

    // ------------------------------------------------------------------
    // THEME PRESETS (Settings tab) — two ints per preset, Discord-style
    // ------------------------------------------------------------------
    const ImRect seg3b = lay::SegSeg(3);
    ClickAt(Center(seg3b).x, Center(seg3b).y);
    RunFrames(2);
    {
        const uint32_t bg_before = g_fb.px[(size_t)(int)(lay::CLIENT_H - 15) * (size_t)(int)(lay::CLIENT_W - 20)];
        const ImRect blurple = lay::ThemePreset(lay::AccentCard(), 0);
        ClickAt(Center(blurple).x, Center(blurple).y);
        Check(theme_primary == 0x5865F2u && theme_accent == 0x8EA1FFu,
              "click blurple preset -> theme ints set");
        RunFrames(2, (dir + "09_theme_blurple.bmp").c_str());
        const uint32_t bg_after = g_fb.px[(size_t)(int)(lay::CLIENT_H - 15) * (size_t)(int)(lay::CLIENT_W - 20)];
        Check(bg_before != bg_after, "blurple preset re-themes the background gradient");
    }
    {
        const ImRect pinkp = lay::ThemePreset(lay::AccentCard(), 1);
        ClickAt(Center(pinkp).x, Center(pinkp).y);
        Check(theme_primary == 0xFF3899u, "click pink preset -> back to classic");
        RunFrames(2, (dir + "10_theme_pink.bmp").c_str());
    }

    // backdrop toggle (Settings tab row 0)
    const ImRect bd = lay::SwitchIn(lay::PrefsRow(lay::PrefsCard(), 0));
    ClickAt(Center(bd).x, Center(bd).y);
    Check(other_background == false, "click Dim backdrop switch -> flips off");
    RunFrames(2, (dir + "11_backdrop_bright.bmp").c_str());
    ClickAt(Center(bd).x, Center(bd).y);
    Check(other_background == true, "click Dim backdrop switch -> flips back");

    // ------------------------------------------------------------------
    // CONFIG BUTTONS
    // ------------------------------------------------------------------
    ClickAt(Center(seg2).x, Center(seg2).y);
    RunFrames(2);
    const ImRect row = lay::ConfigBtnRow();
    ClickAt(row.Min.x + 75.0f, Center(row).y);   // Save now
    RunFrames(2, (dir + "12_configs_saved.bmp").c_str());

    // ------------------------------------------------------------------
    // MISC TAB (stats + reset)
    // ------------------------------------------------------------------
    ClickAt(Center(seg1).x, Center(seg1).y);
    RunFrames(2);
    g_left_total = 12345; g_right_total = 678;
    RunFrames(2, (dir + "13_misc_stats.bmp").c_str());
    ClickAt(Center(lay::StatsResetBtn()).x, Center(lay::StatsResetBtn()).y);
    RunFrames(2);
    Check(g_left_total == 0 && g_right_total == 0, "Reset counters clears totals");

    // no-game state screenshot
    g_sim_mc_found = false;
    RunFrames(2, (dir + "14_no_game.bmp").c_str());
    g_sim_mc_found = true;

    // ------------------------------------------------------------------
    // MOD LINK UI: linked chips + the paused footer states
    // ------------------------------------------------------------------
    ClickAt(Center(seg0).x, Center(seg0).y);
    RunFrames(2);
    g_host.mod_linked = true;
    g_host.mod_gui_open = true;
    RunFrames(2, (dir + "15_mod_menu_open.bmp").c_str());
    g_host.mod_gui_open = false;
    g_host.mod_holding_bow = true;
    RunFrames(2, (dir + "16_mod_bow_held.bmp").c_str());
    g_host.mod_linked = false;
    g_host.mod_holding_bow = false;

    // ------------------------------------------------------------------
    // REMOTE CONFIG (the in-game panel protocol): the mod sends one JSON
    // line, the exe applies it with the same clamps as the UI sliders,
    // persists it and echoes the canonical snapshot back.
    // ------------------------------------------------------------------
    {
        ResetDefaults();
        mod_gui_open = false; mod_holding_bow = false;

        const std::string panel_line =
            "{\"gui\":false,\"bow\":false,\"held\":\"item.bow\","
            "\"config\":{\"left_max_cps\":18.5,\"left_min_cps\":2.0,"
            "\"hit_chance\":55,\"blockhit\":true,\"blockhit_chance\":9,"
            "\"theme_primary\":5793266,\"theme_accent\":9347583,"
            "\"right_enabled\":true,\"left_bind\":116,"
            "\"hold_mode\":false,\"legacy_timing\":true}}";
        Check(ApplyJsonConfig(panel_line) == 11, "config line applies every key it carries");
        Check(left_max_cps == 18.5f && left_min_cps == 2.0f, "config line sets the CPS pair");
        Check(hit_chance == 55 && blockhit && blockhit_chance == 9, "config line sets chance + block hit");
        Check(right_enabled && left_bind == 116, "config line arms + rebinds");
        Check(!hold_mode && legacy_timing, "config line flips activation toggles");
        Check(theme_primary == 0x5865F2u && theme_accent == 0x8EA1FFu, "config line themes (Discord ints)");
        Check(mod_gui_open == false, "state flags on a config line are NOT eaten");

        // the config never fights the click gates: armed side, open GUI
        mod_gui_open = true;
        Check(GateRight(true, true) == false, "panel-armed right clicker still gated by an open GUI");
        mod_gui_open = false;
        Check(GateRight(true, true) == true,  "panel-armed right clicker fires in-game");

        // clamping — the panel cannot set what the sliders cannot
        ApplyJsonConfig("{\"config\":{\"left_max_cps\":99,\"hit_chance\":500,\"left_min_cps\":-3}}");
        Check(left_max_cps == CPS_HI && hit_chance == CHANCE_HI && left_min_cps == CPS_LO,
              "config values clamp exactly like the UI sliders");

        // round-trip: dump -> apply onto defaults restores every value
        left_min_cps = 9.5f;                    // 9.5 is exact in binary
        const std::string dump = BuildConfigJson();
        const float  d_max = left_max_cps, d_min = left_min_cps;
        const int    d_ch  = hit_chance,  d_bnd = left_bind;
        const bool   d_bh  = blockhit,    d_hm  = hold_mode;
        const unsigned int d_tp = theme_primary, d_ta = theme_accent;
        ResetDefaults();
        Check(ApplyJsonConfig(dump) == ABASER_CONFIG_KEY_COUNT, "config dump re-applies every key");
        Check(left_max_cps == d_max && left_min_cps == d_min && hit_chance == d_ch && left_bind == d_bnd &&
              blockhit == d_bh && hold_mode == d_hm && theme_primary == d_tp && theme_accent == d_ta,
              "config dump round-trips losslessly");
        Check(dump.find("\"config\":{") == 1, "dump is a config object");

        // status line shape
        left_enabled = true;
        right_enabled = false;
        const std::string st = BuildStatusJson(true, 11.5f, 0.0f);
        Check(st.find("\"status\":{") == 1 && st.find("\"left_on\":true") != std::string::npos &&
              st.find("\"right_on\":false") != std::string::npos && st.find("\"cps_l\":11.5") != std::string::npos &&
              st.find("\"mc\":true") != std::string::npos &&
              st.find("\"ver\":\"") != std::string::npos,
              "status line reports armed side, live CPS, the game window and the build");

        // ini round-trip survives a remote apply
        SaveConfig(SimConfigPath(nullptr));
        ResetDefaults();
        Check(LoadConfig(SimConfigPath(nullptr)) && left_min_cps == d_min && hit_chance == d_ch,
              "remote-applied settings persist through abaser.ini");
        ResetDefaults();
        left_enabled = false;
    }

    // ------------------------------------------------------------------
    printf("\n%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
