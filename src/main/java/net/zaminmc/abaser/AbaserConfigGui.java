package net.zaminmc.abaser;

import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import org.lwjgl.input.Keyboard;
import org.lwjgl.opengl.GL11;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.GuiTextField;
import net.minecraft.client.renderer.GlStateManager;

/**
 * The in-game Abaser panel — a small glass-styled settings screen with four
 * tabs, reached with Right Shift (or /abaser panel).
 *
 * TAB 1 — CLICKER: talks to the standalone Abaser Clicker exe over the mod
 * link. This screen is a COCKPIT, not the engine: every switch and slider
 * here becomes one {"config":{...}} line on the exe's stdin, the exe applies
 * it with the same clamps as its own UI, persists it to abaser.ini and
 * echoes the canonical snapshot back. Nothing here ever generates a click —
 * the exe keeps doing all of the clicking with its own engine, its own
 * hotkeys and its own PostMessage targeting.
 *
 * TAB 2 — AIM: the one engine that lives inside the mod (a client-side
 * camera nudge — see AimAssist). These knobs edit the mod's own config
 * (config/abaser_aim.json) directly; they never travel the link because the
 * exe has nothing to do with them.
 *
 * TAB 3 — FRIENDS: who the assist must never target and, in lock-in mode,
 * who it must target. Manual exceptions and lock names are edited right
 * here (text field + Add button, click the x to remove); the peace list is
 * shown read-only — it fills automatically from the server's chat.
 *
 * TAB 4 — SPAWN: the exact 3D spawn box (editable fields, an "around me"
 * button, radius) plus both gates — no aim inside (mod) and no clicks
 * inside (the exe's spawn_gate).
 *
 * While this screen is open the mod reports gui=true (it is a GUI like any
 * other), so the exe holds every click while you configure it — and the aim
 * assist freezes too, because no camera turns behind an open screen.
 *
 * Layout is hand-drawn (no vanilla Button widgets) to match the exe's
 * Liquid-Glass look: translucent cards, 1 px light borders, iOS toggles,
 * capsule sliders and theme swatches — all in Minecraft's scaled GUI space.
 */
public class AbaserConfigGui extends GuiScreen {

    // ---- panel geometry (scaled px) ---------------------------------------
    private static final int PW = 244, PH = 268;

    // ---- palette (matches the exe's glass) ---------------------------------
    private static final int C_BACKDROP   = 0x78000000;
    private static final int C_PANEL      = 0xEE0C111A;
    private static final int C_CARD       = 0x14FFFFFF;
    private static final int C_CARD_EDGE  = 0x2EFFFFFF;
    private static final int C_TEXT       = 0xFFEDEFF5;
    private static final int C_MUT        = 0xFF9AA3B2;
    private static final int C_DIM        = 0xFF5C6470;
    private static final int C_TRACK_OFF  = 0x46000000;
    private static final int C_TRACK      = 0x26FFFFFF;
    private static final int C_OK         = 0xFF30D158;   // iOS green
    private static final int C_WARN       = 0xFFFFC24B;
    private static final int C_BAD        = 0xFFFF453A;
    private static final int C_DONE_BG    = 0x22FF3899;

    // theme presets — the exact pairs the exe's Settings tab offers
    private static final int[][] PRESETS = {
        { 0x5865F2, 0x8EA1FF },   // blurple (Discord)
        { 0xFF3899, 0xFF3899 },   // pink (classic Abaser)
        { 0x2563EB, 0x60A5FA },   // blue
        { 0x8B5CF6, 0xC4B5FD },   // purple
        { 0xF97316, 0xFDBA74 },   // orange
        { 0x22C55E, 0x86EFAC },   // green
    };

    // ---- tabs ----------------------------------------------------------------
    private static final String[] TABS = { "Clicker", "Aim", "Friends", "Spawn" };
    // 0 = clicker (exe link), 1 = aim assist (mod), 2 = friends, 3 = spawn
    private int tab = 0;
    /** Remembers the open tab across re-opens in one session. */
    private static int lastTab = 0;

    // ---- slider specs -------------------------------------------------------
    private static class Slider {
        final String key, label;
        final float lo, hi;
        final boolean isInt;
        Slider(String key, String label, float lo, float hi, boolean isInt) {
            this.key = key; this.label = label; this.lo = lo; this.hi = hi; this.isInt = isInt;
        }
    }
    private static final Slider[] SIDE_SLIDERS = {
        new Slider("left_max_cps",  "CPS max", 1, 25, false),
        new Slider("left_min_cps",  "CPS min", 1, 25, false),
        new Slider("hit_chance",    "Chance",  1, 100, true),
    };
    private static final Slider[] RIGHT_SLIDERS = {
        new Slider("right_max_cps", "CPS max", 1, 25, false),
        new Slider("right_min_cps", "CPS min", 1, 25, false),
        new Slider("right_chance",  "Chance",  1, 100, true),
    };
    private static final Slider BLOCK_CHANCE = new Slider("blockhit_chance", "Block hit chance", 1, 25, true);

    // aim-assist sliders — read/write AimAssist.cfg, never the exe link
    private static class AimSlider {
        final String key, label;
        final float lo, hi;
        final boolean isInt;
        AimSlider(String key, String label, float lo, float hi, boolean isInt) {
            this.key = key; this.label = label; this.lo = lo; this.hi = hi; this.isInt = isInt;
        }
    }
    private static final AimSlider[] AIM_LEFT = {
        new AimSlider("aim_distance", "Distance", 1, 10, false),
        new AimSlider("aim_fov",      "FOV",     30, 180, true),
        new AimSlider("aim_strength", "Strength", 0, 1, false),
    };
    private static final AimSlider[] AIM_RIGHT = {
        new AimSlider("aim_smooth", "Smooth",  0.05F, 1, false),
        new AimSlider("aim_v",      "Vertical", 0, 2, false),
        new AimSlider("aim_h",      "Horizontal", 0, 2, false),
    };

    // ---- state --------------------------------------------------------------
    private final Map<String, String> work = new HashMap<String, String>();
    private String dragKey = null;          // slider currently being dragged (exe tab)
    private String aimDrag = null;          // slider currently being dragged (aim tab)
    private long   lastEditMs = 0L;         // guard against echo clobbering

    private int px, py;                     // panel origin
    private int accent = 0xFF3899;
    private boolean linked, haveCfg;

    public AbaserConfigGui() {}

    @Override
    public boolean doesGuiPauseGame() { return false; }

    @Override
    public void initGui() {
        px = (this.width - PW) / 2;
        py = Math.max(4, (this.height - PH) / 2);
        tab = lastTab;
        initNameFields();
        pullFromExe(true);
        // ask again in case the handshake echo has not landed yet
        if (linked && !haveCfg) ClientTickHandler.sendConfigLine("{\"get\":\"config\"}");
    }

    // ---- text fields (Friends + Spawn tabs) ----------------------------------
    // Geometry is fixed per tab layout; fields are built once in initGui with
    // their final on-screen positions (GuiTextField has no move setters).
    private GuiTextField exAdd, lockAdd;                    // name entry rows
    private final GuiTextField[] spawnField = new GuiTextField[6];   // X1 Y1 Z1 X2 Y2 Z2
    private float spawnRadius = 24.0F;                      // "box around me" radius
    private String spawnErr = null;                         // last parse error, if any
    private boolean radiusDrag = false;

    // field hit boxes {x,y,w,h} — 1.8.8's GuiTextField.mouseClicked returns
    // void, so the "was the click inside the field" test is ours to do
    private int[] exAddBox, lockAddBox;
    private final int[][] spawnBox = new int[6][];

    // friends-tab card geometry (shared by draw + hit tests)
    private int exCardX()   { return px + 8; }
    private int exCardY()   { return py + 38; }
    private int lockCardY() { return py + 118; }
    private int friendCardH() { return 76; }

    private void initNameFields() {
        final int ax = exCardX();
        exAdd  = makeField(1, ax + 8, exCardY() + 58, 168);
        exAddBox = new int[] { ax + 8, exCardY() + 58, 168, 13 };
        lockAdd = makeField(2, ax + 8, lockCardY() + 58, 168);
        lockAddBox = new int[] { ax + 8, lockCardY() + 58, 168, 13 };
        final float[] cur = {
            AimAssist.cfg.spawnX1, AimAssist.cfg.spawnY1, AimAssist.cfg.spawnZ1,
            AimAssist.cfg.spawnX2, AimAssist.cfg.spawnY2, AimAssist.cfg.spawnZ2
        };
        final int[] fy = { py + 58, py + 74, py + 90 };     // X, Y, Z rows
        final int[] fx = { px + 20, px + 124 };             // min / max columns
        for (int i = 0; i < 6; i++) {
            spawnField[i] = makeField(3 + i, fx[i % 2], fy[i / 2], i % 2 == 0 ? 88 : 92);
            spawnBox[i] = new int[] { fx[i % 2], fy[i / 2], i % 2 == 0 ? 88 : 92, 13 };
            spawnField[i].setText(fmt(cur[i]));
        }
    }

    private GuiTextField makeField(int id, int x, int y, int w) {
        final GuiTextField f = new GuiTextField(id, this.fontRendererObj, x, y, w, 13);
        f.setMaxStringLength(id >= 3 ? 12 : 16);
        return f;
    }

    /** Route a click to a field (focus/cursor handled internally by
     *  GuiTextField), then report whether the click landed inside it. */
    private boolean clickField(GuiTextField f, int[] box, int mx, int my, int btn) {
        if (f == null || box == null) return false;
        f.mouseClicked(mx, my, btn);
        return inRect(mx, my, box[0], box[1], box[2], box[3]);
    }

    /** Copy the exe's last canonical config into the working copy. */
    private void pullFromExe(boolean force) {
        linked = ClientTickHandler.isLinkUp();
        haveCfg = LinkState.haveConfig;
        if (haveCfg && LinkState.config != null && (force || mayRefresh())) {
            work.clear();
            work.putAll(LinkState.config);
        }
        if (force && !haveCfg) seedDefaults();
        String a = work.get("theme_accent");
        try { accent = a != null ? (int)Long.parseLong(a.trim()) & 0xFFFFFF : 0xFF3899; }
        catch (Exception e) { accent = 0xFF3899; }
    }

    private boolean mayRefresh() {
        return dragKey == null && System.currentTimeMillis() - lastEditMs > 750L;
    }

    private void seedDefaults() {
        put("left_min_cps", "9.6");  put("left_max_cps", "14");
        put("right_min_cps", "13");  put("right_max_cps", "20");
        put("hit_chance", "87");     put("right_chance", "80");
        put("blockhit", "false");    put("blockhit_chance", "6");
        put("hold_mode", "true");    put("require_focus", "true");
        put("legacy_timing", "false"); put("spawn_gate", "true");
        put("left_enabled", "false"); put("right_enabled", "false");
        put("theme_primary", "16726169"); put("theme_accent", "16726169");
    }

    private void put(String k, String v) { work.put(k, v); }

    private static float f(Map<String, String> m, String k, float def) {
        try { return Float.parseFloat(m.get(k).trim()); } catch (Exception e) { return def; }
    }

    private static boolean bool(Map<String, String> m, String k, boolean def) {
        String v = m == null ? null : m.get(k);
        if (v == null || v.trim().length() == 0) return def;
        v = v.trim();
        return v.equals("true") || v.equals("1");
    }

    private float getF(String k, float def) { return f(work, k, def); }
    private boolean getB(String k, boolean def) { return bool(work, k, def); }

    private void toggle(String k, boolean def) {
        put(k, getB(k, def) ? "false" : "true");
        sendWork();
    }

    // ---- JSON out (one line per change) ------------------------------------
    private static String num(float v) {
        if (Math.abs(v - Math.floor(v)) < 0.001f)
            return String.format(Locale.ROOT, "%d", (long)Math.floor(v));
        return String.format(Locale.ROOT, "%.1f", v);
    }

    private void sendWork() {
        final StringBuilder sb = new StringBuilder(192);
        sb.append("{\"config\":{");
        add(sb, "left_min_cps",  num(getF("left_min_cps", 9.6f)));
        add(sb, "left_max_cps",  num(getF("left_max_cps", 14.0f)));
        add(sb, "right_min_cps", num(getF("right_min_cps", 13.0f)));
        add(sb, "right_max_cps", num(getF("right_max_cps", 20.0f)));
        add(sb, "hit_chance",    String.format(Locale.ROOT, "%d", (int)getF("hit_chance", 87)));
        add(sb, "right_chance",  String.format(Locale.ROOT, "%d", (int)getF("right_chance", 80)));
        add(sb, "blockhit",         getB("blockhit", false) ? "true" : "false");
        add(sb, "blockhit_chance",  String.format(Locale.ROOT, "%d", (int)getF("blockhit_chance", 6)));
        add(sb, "hold_mode",        getB("hold_mode", true) ? "true" : "false");
        add(sb, "require_focus",    getB("require_focus", true) ? "true" : "false");
        add(sb, "legacy_timing",    getB("legacy_timing", false) ? "true" : "false");
        add(sb, "spawn_gate",       getB("spawn_gate", true) ? "true" : "false");
        add(sb, "left_enabled",     getB("left_enabled", false) ? "true" : "false");
        add(sb, "right_enabled",    getB("right_enabled", false) ? "true" : "false");
        add(sb, "theme_primary",    String.valueOf(getThemePrimary()));
        add(sb, "theme_accent",     String.valueOf(accent));
        sb.setCharAt(sb.length() - 1, '}');
        sb.append('}');
        ClientTickHandler.sendConfigLine(sb.toString());
        lastEditMs = System.currentTimeMillis();
    }

    private int getThemePrimary() {
        try { return (int)Long.parseLong(work.get("theme_primary").trim()) & 0xFFFFFF; }
        catch (Exception e) { return 0xFF3899; }
    }

    private static void add(StringBuilder sb, String k, String v) {
        sb.append('"').append(k).append("\":").append(v).append(',');
    }

    // ---- drawing helpers ----------------------------------------------------
    /** Rounded-ish rect: two stacked rects cut the 1 px corners. */
    private static void glassRect(int x, int y, int w, int h, int fill, int edge) {
        drawRect(x + 1, y, x + w - 1, y + h, fill);
        drawRect(x, y + 1, x + w, y + h - 1, fill);
        if (edge != 0) {
            // 1 px border in four strips
            drawRect(x, y + 1, x + 1, y + h - 1, edge);
            drawRect(x + w - 1, y + 1, x + w, y + h - 1, edge);
            drawRect(x + 1, y, x + w - 1, y + 1, edge);
            drawRect(x + 1, y + h - 1, x + w - 1, y + h, edge);
        }
    }

    private static void circle(float cx, float cy, float r, int argb) {
        final float a = ((argb >> 24) & 0xFF) / 255.0f;
        final float rC = ((argb >> 16) & 0xFF) / 255.0f;
        final float g = ((argb >> 8) & 0xFF) / 255.0f;
        final float b = (argb & 0xFF) / 255.0f;
        GlStateManager.enableBlend();
        GlStateManager.blendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
        GlStateManager.disableTexture2D();
        GlStateManager.color(1.0f, 1.0f, 1.0f, 1.0f);
        GL11.glColor4f(rC, g, b, a);
        GL11.glBegin(GL11.GL_TRIANGLE_FAN);
        GL11.glVertex2f(cx, cy);
        for (int i = 0; i <= 16; i++) {
            final double ang = (double)i / 16.0 * Math.PI * 2.0;
            GL11.glVertex2d(cx + Math.cos(ang) * r, cy + Math.sin(ang) * r);
        }
        GL11.glEnd();
        GlStateManager.enableTexture2D();
        GlStateManager.disableBlend();
    }

    private void drawSwitch(int x, int y, boolean on, boolean hovered) {
        final int w = 22, h = 11;
        glassRect(x, y, w, h, on ? accentFix(accent) : C_TRACK_OFF,
                  on ? lighten(accent, 0.45f) : 0x30FFFFFF);
        final float kn = on ? (w - h + 1.0f) : 1.0f;    // knob travel
        circle(x + kn + (h - 2.0f) / 2.0f, y + h / 2.0f, (h - 2.0f) / 2.0f, hovered ? 0xFFFFFFFF : 0xFFF2F3F7);
    }

    /** accent as a translucent ON-track fill (24-bit accent + alpha). */
    private static int accentFix(int a) { return (a & 0xFFFFFF) | 0xE8000000; }

    private static int lighten(int argb, float t) {
        final int r = (int)((((argb >> 16) & 0xFF) + (255 - ((argb >> 16) & 0xFF)) * t));
        final int g = (int)((((argb >> 8) & 0xFF) + (255 - ((argb >> 8) & 0xFF)) * t));
        final int b = (int)(((argb & 0xFF) + (255 - (argb & 0xFF)) * t));
        return 0xFF000000 | (Math.min(255, r) << 16) | (Math.min(255, g) << 8) | Math.min(255, b);
    }

    private boolean inRect(int mx, int my, int x, int y, int w, int h) {
        return mx >= x && mx < x + w && my >= y && my < y + h;
    }

    // ---- screen -------------------------------------------------------------
    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        pullFromExe(false);
        drawRect(0, 0, this.width, this.height, C_BACKDROP);

        glassRect(px, py, PW, PH, C_PANEL, C_CARD_EDGE);

        drawHeader();
        drawTabs(mouseX, mouseY);

        if (tab == 1) {                       // the aim tab works with or without the exe
            drawAimTab(mouseX, mouseY);
            return;
        }
        if (tab == 2) {                       // friends — mod-side, link not needed
            drawFriendsTab(mouseX, mouseY);
            return;
        }
        if (tab == 3) {                       // spawn — box mod-side, gates split
            drawSpawnTab(mouseX, mouseY);
            return;
        }
        if (!linked) {
            drawNotRunning(mouseX, mouseY);
            return;
        }
        if (!haveCfg) {
            drawCentered("Syncing settings from the clicker...", py + 110, C_MUT);
            doneButton(mouseX, mouseY);
            return;
        }

        drawSideCard(px + 8,  "LEFT CLICKER",  getB("left_enabled", false), "left_enabled", SIDE_SLIDERS, mouseX, mouseY);
        drawSideCard(px + 125, "RIGHT CLICKER", getB("right_enabled", false), "right_enabled", RIGHT_SLIDERS, mouseX, mouseY);
        drawActivationCard(mouseX, mouseY);
        drawThemeRow(mouseX, mouseY);
        drawFooter(mouseX, mouseY);
    }

    private void drawCentered(String s, int y, int color) {
        this.fontRendererObj.drawStringWithShadow(s, this.width / 2.0f - this.fontRendererObj.getStringWidth(s) / 2.0f, y, color);
    }

    private void drawNotRunning(int mouseX, int mouseY) {
        drawCentered("Abaser Clicker is not running", py + 96, C_TEXT);
        drawCentered("Join a world - the mod installs and starts it", py + 110, C_MUT);
        drawCentered("in your system tray (bundled with this jar).", py + 122, C_MUT);
        drawCentered("The Aim, Friends and Spawn tabs work without it.", py + 138, C_DIM);
        doneButton(mouseX, mouseY);
    }

    private void drawHeader() {
        this.fontRendererObj.drawStringWithShadow("Abaser Clicker", px + 10, py + 7, C_TEXT);
        // the exe's own build string, once the handshake has reported it
        if (tab == 0 && linked && LinkState.version != null) {
            this.fontRendererObj.drawStringWithShadow(LinkState.version,
                    px + 10 + this.fontRendererObj.getStringWidth("Abaser Clicker") + 6, py + 7, C_DIM);
        }
        // live status dot + word, right-aligned
        String st; int dot;
        if (tab >= 1) {
            st  = AimAssist.cfg.enabled ? "assist on" : "assist off";
            dot = AimAssist.cfg.enabled ? C_OK : 0x50FFFFFF;
        } else if (LinkState.guiOpen)       { st = "menu open"; dot = C_WARN; }
        else if (!LinkState.mcFound)        { st = "no game";   dot = C_BAD; }
        else                                { st = "in-game";   dot = C_OK; }
        final int tw = this.fontRendererObj.getStringWidth(st);
        this.fontRendererObj.drawStringWithShadow(st, px + PW - 10 - tw - 8, py + 7, C_MUT);
        circle(px + PW - 10 - 3, py + 11.5f, 2.5f, dot);
    }

    // ---- tab bar --------------------------------------------------------------
    private final Map<Integer, int[]> tabHit = new HashMap<Integer, int[]>();

    private void drawTabs(int mx, int my) {
        tabHit.clear();
        int x = px + 8;
        final int y = py + 20, h = 13;
        for (int i = 0; i < TABS.length; i++) {
            final int w = this.fontRendererObj.getStringWidth(TABS[i]) + 12;
            final boolean sel = tab == i;
            final boolean hov = inRect(mx, my, x, y, w, h);
            glassRect(x, y, w, h,
                      sel ? accentFix(accent) : (hov ? 0x24FFFFFF : C_TRACK),
                      sel ? lighten(accent, 0.45f) : 0x28FFFFFF);
            this.fontRendererObj.drawStringWithShadow(TABS[i], x + 6, y + 3,
                    sel ? 0xFFFFFFFF : (hov ? C_TEXT : C_MUT));
            tabHit.put(i, new int[] { x, y, w });
            x += w + 4;
        }
    }

    // ---- CLICKER tab (the exe link) ------------------------------------------
    private void drawSideCard(int x, String title, boolean on, String toggleKey, Slider[] sliders, int mx, int my) {
        final int y = py + 38, w = 111, h = 98;
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        this.fontRendererObj.drawStringWithShadow(title, x + 6, y + 5, C_DIM);

        // toggle row
        this.fontRendererObj.drawStringWithShadow(on ? "On" : "Off", x + 6, y + 16, C_MUT);
        final int sx = x + w - 6 - 22, sy = y + 15;
        drawSwitch(sx, sy, on, inRect(mx, my, sx, sy, 22, 11));

        // three sliders
        for (int i = 0; i < sliders.length; i++) {
            final int ry = y + 30 + i * 22;
            drawSliderRow(sliders[i], x + 6, ry, w - 12, mx, my);
        }
    }

    private void drawSliderRow(Slider s, int x, int y, int w, int mx, int my) {
        final float v = getF(s.key, s.lo);
        final String txt = s.isInt
            ? String.format(Locale.ROOT, "%d", (int)v)
            : String.format(Locale.ROOT, "%.1f", v);
        this.fontRendererObj.drawStringWithShadow(s.label, x, y, C_MUT);
        final int vw = this.fontRendererObj.getStringWidth(txt);
        this.fontRendererObj.drawStringWithShadow(txt, x + w - vw, y, (dragKey != null && dragKey.equals(s.key)) ? accent | 0xFF000000 : C_TEXT);

        // capsule track
        final int tx = x, ty = y + 11, tw = w, th = 6;
        drawRect(tx, ty, tx + tw, ty + th, C_TRACK);
        final float frac = Math.max(0.0f, Math.min(1.0f, (v - s.lo) / (s.hi - s.lo)));
        final int pad = 3;
        final int fillw = (int)(pad + frac * (tw - 2 * pad));
        if (fillw > 1) drawRect(tx, ty, tx + fillw, ty + th, (accent & 0xFFFFFF) | 0xE6000000);
        final float kx = tx + pad + frac * (tw - 2 * pad);
        circle(kx, ty + th / 2.0f, 4.0f, 0xFFF2F3F7);
        circle(kx, ty + th / 2.0f, 4.0f - 0.75f, (accent & 0xFFFFFF) | 0x66FFFFFF);
        circle(kx, ty + th / 2.0f, 4.0f - 1.5f, 0xFFF2F3F7);
    }

    /** Slider drag/click hit test — shared by mouseClicked / mouseClickMove. */
    private boolean sliderHit(Slider s, int x, int y, int w, int mx, int my, boolean apply) {
        final int ty = y + 11;
        if (my < ty - 2 || my > ty + 10) return false;
        if (mx < x - 2 || mx > x + w + 2) return false;
        if (apply) {
            final float frac = Math.max(0.0f, Math.min(1.0f, (mx - x) / (float)w));
            float v = s.lo + frac * (s.hi - s.lo);
            if (s.isInt) v = Math.round(v);
            if (v != getF(s.key, s.lo)) {
                put(s.key, s.isInt ? String.format(Locale.ROOT, "%d", (int)v) : num(v));
            }
        }
        return true;
    }

    private void drawActivationCard(int mx, int my) {
        final int x = px + 8, y = py + 140, w = 228, h = 66;
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        final int ax0 = x + 6, ax1 = x + w - 6;
        final int col2 = ax0 + 110;

        // row 1
        toggleRow("Hold to click", "hold_mode", true, ax0, y + 4, ax0 + 104, mx, my);
        toggleRow("Focus gate", "require_focus", true, col2, y + 4, ax1, mx, my);
        // row 2
        toggleRow("Legacy timing", "legacy_timing", false, ax0, y + 18, ax0 + 104, mx, my);
        toggleRow("Block hit", "blockhit", false, col2, y + 18, ax1, mx, my);
        // row 3 — the two new click gates
        toggleRow("Ally hold", "allyHold", true, ax0, y + 32, ax0 + 104, mx, my);
        toggleRow("Spawn gate", "spawn_gate", true, col2, y + 32, ax1, mx, my);
        // row 4 — block-hit chance, only meaningful while block hit is on
        final int ry = y + 46;
        if (getB("blockhit", false)) {
            drawSliderRow(BLOCK_CHANCE, bhX(), bhY(), bhW(), mx, my);
        } else {
            this.fontRendererObj.drawStringWithShadow("No clicks inside spawn or with an ally under the crosshair.", ax0 + 6, ry, C_DIM);
        }
    }

    // block-hit chance slider geometry — shared by draw / click / drag
    private int bhX() { return px + 20; }
    private int bhY() { return py + 186; }
    private int bhW() { return 210; }

    private void toggleRow(String label, String key, boolean def, int x, int y, int rowRight, int mx, int my) {
        this.fontRendererObj.drawStringWithShadow(label, x, y + 1, C_TEXT);
        final int sx = rowRight - 22;
        drawSwitch(sx, y, getB(key, def), inRect(mx, my, sx, y, 22, 11));
        rowHit.put(key, new int[] { sx, y });
    }

    private final Map<String, int[]> rowHit = new HashMap<String, int[]>();

    private void drawThemeRow(int mx, int my) {
        final int y = py + 196;
        this.fontRendererObj.drawStringWithShadow("Theme", px + 14, y + 2, C_MUT);
        final int sw = 14, sh = 12, gap = 3;
        int x = px + PW - 10 - (PRESETS.length * sw + (PRESETS.length - 1) * gap);
        for (int i = 0; i < PRESETS.length; i++) {
            final int pr = PRESETS[i][0], ac = PRESETS[i][1];
            final boolean sel = getThemePrimary() == pr && accent == ac;
            final boolean hov = inRect(mx, my, x, y, sw, sh);
            // mini profile-banner: vertical gradient of the pair (Discord style)
            drawRect(x, y, x + sw, y + sh, 0x40FFFFFF);
            // 1.8.8's drawGradientRect has no zLevel parameter
            this.drawGradientRect(x + 1, y + 1, x + sw - 1, y + sh - 1,
                                  (pr & 0xFFFFFF) | 0xFF000000, shade(pr, 0.42f));
            if (sel) {
                drawRect(x - 1, y - 1, x + sw + 1, y, 0xFFFFFFFF);
                drawRect(x - 1, y + sh, x + sw + 1, y + sh + 1, 0xFFFFFFFF);
                drawRect(x - 1, y, x, y + sh, 0xFFFFFFFF);
                drawRect(x + sw, y, x + sw + 1, y + sh, 0xFFFFFFFF);
            } else if (hov) {
                drawRect(x - 1, y - 1, x + sw + 1, y, 0x66FFFFFF);
                drawRect(x - 1, y + sh, x + sw + 1, y + sh + 1, 0x66FFFFFF);
                drawRect(x - 1, y, x, y + sh, 0x66FFFFFF);
                drawRect(x + sw, y, x + sw + 1, y + sh, 0x66FFFFFF);
            }
            themeHit.put(i, new int[] { x, y });
            x += sw + gap;
        }
    }

    private final Map<Integer, int[]> themeHit = new HashMap<Integer, int[]>();

    private static int shade(int argb, float f) {
        final int r = (int)(((argb >> 16) & 0xFF) * f);
        final int g = (int)(((argb >> 8) & 0xFF) * f);
        final int b = (int)((argb & 0xFF) * f);
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }

    private void drawFooter(int mx, int my) {
        final int y = py + 232;
        // live line, dot + "x.x L / y.y R cps"
        final boolean armed = getB("left_enabled", false) || getB("right_enabled", false);
        final String live = String.format(Locale.ROOT, "%.1f L / %.1f R cps live",
                LinkState.cpsLeft, LinkState.cpsRight);
        circle(px + 16, y + 5, 2.5f, armed ? C_OK : 0x50FFFFFF);
        this.fontRendererObj.drawStringWithShadow(live, px + 22, y + 1, armed ? C_TEXT : C_MUT);

        // the same honesty the exe's footer has: say WHY nothing clicks
        String why = null; int whyCol = C_DIM;
        if (LinkState.guiOpen)            { why = "paused - a menu is open"; whyCol = C_WARN; }
        else if (getB("spawn_gate", true) && AimAssist.inSpawn)
                                          { why = "paused - inside spawn";  whyCol = C_WARN; }
        else if (AimAssist.allyCross)     { why = "paused - ally in crosshair"; whyCol = C_WARN; }
        else if (LinkState.holdingBow)    { why = "paused - bow drawn";      whyCol = C_WARN; }
        else {
            why = "RSHIFT opens this panel";
        }
        final int ww = this.fontRendererObj.getStringWidth(why);
        this.fontRendererObj.drawStringWithShadow(why, px + PW - 10 - 44 - 8 - ww, y + 1, whyCol);

        doneButton(mx, my);
    }

    private int doneRect() { return px + PW - 10 - 44; }

    private void doneButton(int mx, int my) {
        final int x = doneRect(), y = py + PH - 24, w = 44, h = 14;
        final boolean hov = inRect(mx, my, x, y, w, h);
        glassRect(x, y, w, h, hov ? 0x30FFFFFF : C_DONE_BG, hov ? 0x55FFFFFF : lighten(accent, 0.35f));
        this.fontRendererObj.drawStringWithShadow("Done", x + w / 2 - this.fontRendererObj.getStringWidth("Done") / 2, y + 3, C_TEXT);
    }

    // ---- AIM ASSIST tab (the mod's own engine) --------------------------------

    private float aimGet(String key) {
        final AimAssistConfig c = AimAssist.cfg;
        if (key.equals("aim_distance")) return c.maxDistance;
        if (key.equals("aim_fov"))      return c.fov;
        if (key.equals("aim_strength")) return c.strength;
        if (key.equals("aim_smooth"))   return c.smoothFactor;
        if (key.equals("aim_v"))        return c.verticalStrength;
        if (key.equals("aim_h"))        return c.horizontalStrength;
        return 0.0f;
    }

    private void aimSet(String key, float v) {
        final AimAssistConfig c = AimAssist.cfg;
        if      (key.equals("aim_distance")) c.maxDistance = v;
        else if (key.equals("aim_fov"))      c.fov = v;
        else if (key.equals("aim_strength")) c.strength = v;
        else if (key.equals("aim_smooth"))   c.smoothFactor = v;
        else if (key.equals("aim_v"))        c.verticalStrength = v;
        else if (key.equals("aim_h"))        c.horizontalStrength = v;
        c.clamp();
    }

    private boolean aimToggleState(String key) {
        final AimAssistConfig c = AimAssist.cfg;
        if      (key.equals("aim_smooth_on")) return c.smoothAim;
        else if (key.equals("aim_silent"))    return c.silent;
        else if (key.equals("aim_require"))   return c.requireClick;
        else if (key.equals("aim_spawn"))     return c.spawnBlockAim;
        else if (key.equals("aim_lock"))      return c.lockEnabled;
        else if (key.equals("aim_ally"))      return c.allyHold;
        return false;
    }

    private void aimToggle(String key) {
        final AimAssistConfig c = AimAssist.cfg;
        if      (key.equals("aim_smooth_on")) c.smoothAim = !c.smoothAim;
        else if (key.equals("aim_silent"))    c.silent = !c.silent;
        else if (key.equals("aim_require"))   c.requireClick = !c.requireClick;
        else if (key.equals("aim_spawn"))     c.spawnBlockAim = !c.spawnBlockAim;
        else if (key.equals("aim_lock"))      c.lockEnabled = !c.lockEnabled;
        else if (key.equals("aim_ally"))      c.allyHold = !c.allyHold;
        c.save();
    }

    /** 90.0 -> "90", 0.3 -> "0.3", 0.05 -> "0.05". */
    private static String fmt(float v) {
        if (Math.abs(v - Math.floor(v)) < 0.005f)
            return String.format(Locale.ROOT, "%d", (long)Math.floor(v));
        String s = String.format(Locale.ROOT, "%.2f", v);
        while (s.endsWith("0")) s = s.substring(0, s.length() - 1);
        if (s.endsWith(".")) s = s.substring(0, s.length() - 1);
        return s;
    }

    private void drawAimTab(int mx, int my) {
        // master switch row (live target name when the assist is on)
        final int my0 = py + 38;
        this.fontRendererObj.drawStringWithShadow("Aim assist", px + 10, my0 + 1, C_TEXT);
        String sub;
        if (!AimAssist.cfg.enabled) {
            sub = "camera untouched";
        } else {
            final String t = AimAssist.targetName();
            sub = t != null ? ("target: " + t) : "searching...";
        }
        this.fontRendererObj.drawStringWithShadow(sub, px + 72, my0 + 1, C_DIM);
        final int msx = px + PW - 10 - 22;
        drawSwitch(msx, my0, AimAssist.cfg.enabled, inRect(mx, my, msx, my0, 22, 11));
        masterHit = new int[] { msx, my0 };

        // two cards of sliders
        drawAimCard(px + 8,   "TARGETING", AIM_LEFT,  mx, my);
        drawAimCard(px + 125, "TURNING",   AIM_RIGHT, mx, my);

        drawAimToggles(mx, my);
        drawAimFooter(mx, my);
    }

    private int[] masterHit = null;

    private void drawAimCard(int x, String title, AimSlider[] sliders, int mx, int my) {
        final int y = py + 54, w = 111, h = 90;
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        this.fontRendererObj.drawStringWithShadow(title, x + 6, y + 5, C_DIM);
        for (int i = 0; i < sliders.length; i++) {
            drawAimSliderRow(sliders[i], x + 6, y + 28 + i * 21, w - 12, mx, my);
        }
    }

    private void drawAimSliderRow(AimSlider s, int x, int y, int w, int mx, int my) {
        final float v = aimGet(s.key);
        final String txt = fmt(v);
        this.fontRendererObj.drawStringWithShadow(s.label, x, y, C_MUT);
        final int vw = this.fontRendererObj.getStringWidth(txt);
        this.fontRendererObj.drawStringWithShadow(txt, x + w - vw, y,
                (aimDrag != null && aimDrag.equals(s.key)) ? accent | 0xFF000000 : C_TEXT);

        final int ty = y + 11, th = 6;
        drawRect(x, ty, x + w, ty + th, C_TRACK);
        final float frac = Math.max(0.0f, Math.min(1.0f, (v - s.lo) / (s.hi - s.lo)));
        final int pad = 3;
        final int fillw = (int)(pad + frac * (w - 2 * pad));
        if (fillw > 1) drawRect(x, ty, x + fillw, ty + th, (accent & 0xFFFFFF) | 0xE6000000);
        final float kx = x + pad + frac * (w - 2 * pad);
        circle(kx, ty + th / 2.0f, 4.0f, 0xFFF2F3F7);
        circle(kx, ty + th / 2.0f, 4.0f - 0.75f, (accent & 0xFFFFFF) | 0x66FFFFFF);
        circle(kx, ty + th / 2.0f, 4.0f - 1.5f, 0xFFF2F3F7);
    }

    private boolean aimSliderHit(AimSlider s, int x, int y, int w, int mx, int my, boolean apply) {
        final int ty = y + 11;
        if (my < ty - 2 || my > ty + 10) return false;
        if (mx < x - 2 || mx > x + w + 2) return false;
        if (apply) {
            final float frac = Math.max(0.0f, Math.min(1.0f, (mx - x) / (float)w));
            float v = s.lo + frac * (s.hi - s.lo);
            if (s.isInt) v = Math.round(v);
            if (v != aimGet(s.key)) aimSet(s.key, v);
        }
        return true;
    }

    private static final String[][] AIM_TOGGLES = {
        { "aim_smooth_on", "Smooth aim" },
        { "aim_silent",    "Silent" },
        { "aim_require",   "Require click" },
        { "aim_spawn",     "No aim in spawn" },
        { "aim_lock",      "Lock-in mode" },
        { "aim_ally",      "Ally hold" },
    };

    private final Map<String, int[]> aimRowHit = new HashMap<String, int[]>();

    private void drawAimToggles(int mx, int my) {
        aimRowHit.clear();
        final int y0 = py + 150;
        for (int i = 0; i < AIM_TOGGLES.length; i++) {
            final int col = i % 2, row = i / 2;
            final int x = px + 8 + col * 117, y = y0 + row * 16;
            final String key = AIM_TOGGLES[i][0];
            final boolean val = aimToggleState(key);
            this.fontRendererObj.drawStringWithShadow(AIM_TOGGLES[i][1], x, y + 1, C_TEXT);
            final int sx = x + 89;
            drawSwitch(sx, y, val, inRect(mx, my, sx, y, 22, 11));
            aimRowHit.put(key, new int[] { sx, y });
        }
    }

    private void drawAimFooter(int mx, int my) {
        final int y = py + 232;
        // live exception bookkeeping: what the assist refuses to target
        final String live = String.format(Locale.ROOT,
                "peace %d manual %d lock %s",
                PeaceList.size(),
                AimAssist.cfg.manualExceptions.size(),
                AimAssist.cfg.lockEnabled
                        ? (AimAssist.cfg.lockNames.isEmpty() ? "empty"
                           : String.valueOf(AimAssist.cfg.lockNames.size()))
                        : "off");
        this.fontRendererObj.drawStringWithShadow(live, px + 14, y + 1, C_MUT);
        doneButton(mx, my);
    }

    // ---- FRIENDS tab (exceptions + lock-in + peace list) ----------------------
    // The mod's own lists, edited right here instead of via chat commands.
    // Never touches the exe link — targeting decisions are made by the mod's
    // aim assist; the exe only ever receives the resulting ally=true/false.

    private final Map<Integer, int[]> exRemoveHit   = new HashMap<Integer, int[]>();
    private final Map<Integer, int[]> lockRemoveHit = new HashMap<Integer, int[]>();
    private int[] exClearHit = null;
    private int[] lockSwitchHit = null;
    private int[] exAddHit = null;
    private int[] lockAddHit = null;

    private void drawFriendsTab(int mx, int my) {
        drawFriendCard(exCardY(), "Exceptions", AimAssist.cfg.manualExceptions, false, mx, my);
        drawFriendCard(lockCardY(), "Lock-in", AimAssist.cfg.lockNames, true, mx, my);

        // peace list (read-only — the server's chat fills it automatically)
        final Set<String> peace = PeaceList.snapshot();
        String ptxt;
        if (peace.isEmpty()) {
            ptxt = "peace list is empty - it fills when the server prints its /peace list";
        } else {
            final StringBuilder sb = new StringBuilder(192);
            for (String s : peace) { if (sb.length() > 0) sb.append(", "); sb.append(s); }
            ptxt = "peace list (" + peace.size() + "): " + sb;
        }
        this.fontRendererObj.drawStringWithShadow(
                this.fontRendererObj.trimStringToWidth(ptxt, PW - 28), px + 14, py + 204, C_MUT);
        this.fontRendererObj.drawStringWithShadow(
                "parsed from the server's chat automatically - read-only", px + 14, py + 216, C_DIM);
        doneButton(mx, my);
    }

    /** One list card: header (title + clear|lock switch), 2x3 name rows with
     *  x-buttons, and the add field + button. */
    private void drawFriendCard(int y, String title, List<String> names,
                                boolean lockCard, int mx, int my) {
        final int x = exCardX(), w = 228, h = friendCardH();
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        this.fontRendererObj.drawStringWithShadow(
                title + " (" + names.size() + ")", x + 8, y + 4, C_DIM);

        if (lockCard) {
            final int swx = x + w - 8 - 22, swy = y + 2;
            drawSwitch(swx, swy, AimAssist.cfg.lockEnabled, inRect(mx, my, swx, swy, 22, 11));
            lockSwitchHit = new int[] { swx, swy };
        } else if (!names.isEmpty()) {
            final int cbx = x + w - 8 - 34, cby = y + 2;
            final boolean hov = inRect(mx, my, cbx, cby, 34, 12);
            glassRect(cbx, cby, 34, 12, hov ? 0x30FFFFFF : 0x14FFFFFF, 0x2EFFFFFF);
            this.fontRendererObj.drawStringWithShadow("clear", cbx + 8, cby + 2,
                    hov ? C_TEXT : C_MUT);
            exClearHit = new int[] { cbx, cby };
        } else {
            exClearHit = null;
        }

        final Map<Integer, int[]> removeHit = lockCard ? lockRemoveHit : exRemoveHit;
        removeHit.clear();
        final int n = names.size();
        final int shown = n > 6 ? 5 : n;                 // keep slot 6 for "+N more"
        for (int i = 0; i < shown; i++) {
            final int col = i / 3, row = i % 3;
            final int nx = x + 8 + col * 112, ny = y + 17 + row * 13;
            this.fontRendererObj.drawStringWithShadow(
                    this.fontRendererObj.trimStringToWidth(names.get(i), 80), nx, ny, C_TEXT);
            final int bx = nx + 86, by = ny - 2;
            final boolean hov = inRect(mx, my, bx, by, 10, 10);
            glassRect(bx, by, 10, 10, hov ? 0x34FFFFFF : 0x14FFFFFF, 0x2EFFFFFF);
            this.fontRendererObj.drawStringWithShadow("x", bx + 3, by + 1, hov ? C_BAD : C_MUT);
            removeHit.put(i, new int[] { bx, by });
        }
        if (n > 6) {
            this.fontRendererObj.drawStringWithShadow("+" + (n - 5) + " more",
                    x + 8 + 112, y + 17 + 2 * 13, C_DIM);
        } else if (n == 0) {
            this.fontRendererObj.drawStringWithShadow(
                    lockCard
                        ? (AimAssist.cfg.lockEnabled ? "ON but empty - add the only targets"
                                                     : "when on, ONLY these names are targeted")
                        : "names here are never targeted",
                    x + 8, y + 17, C_DIM);
        }

        // the add row (field built in initGui with its final position)
        final GuiTextField f = lockCard ? lockAdd : exAdd;
        if (f != null) f.drawTextBox();
        final int bax = x + w - 48, bay = y + 58;
        final boolean bhov = inRect(mx, my, bax, bay, 40, 13);
        glassRect(bax, bay, 40, 13, bhov ? 0x30FFFFFF : C_DONE_BG, 0x2EFFFFFF);
        this.fontRendererObj.drawStringWithShadow("Add", bax + 13, bay + 3, C_TEXT);
        if (lockCard) lockAddHit = new int[] { bax, bay }; else exAddHit = new int[] { bax, bay };
    }

    private void addFromField(GuiTextField f, boolean exception) {
        if (f == null) return;
        final String s = f.getText().trim();
        if (s.length() == 0) return;
        if (exception) AimAssist.cfg.addException(s); else AimAssist.cfg.addLock(s);
        f.setText("");
    }

    // ---- SPAWN tab (the exact 3D box + both gates) ----------------------------

    private int spawnCardX()  { return px + 8; }
    private int spawnCardY()  { return py + 38; }
    private int spawnGatesY() { return py + 144; }
    private int[] aroundHit = null, applyHit = null, spawnAimHit = null, spawnClickHit = null;

    private void drawSpawnTab(int mx, int my) {
        final int x = spawnCardX(), y = spawnCardY(), w = 228, h = 100;
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        final boolean inside = AimAssist.inSpawn;
        this.fontRendererObj.drawStringWithShadow("Spawn box - exact 3D corners", x + 8, y + 4, C_DIM);
        final String you = inside ? "you are INSIDE" : "you are outside";
        final int yw = this.fontRendererObj.getStringWidth(you);
        this.fontRendererObj.drawStringWithShadow(you, x + w - 8 - yw, y + 4, inside ? C_WARN : C_DIM);

        // coordinate rows (fields built in initGui with their final positions)
        final String[] axis = { "X", "Y", "Z" };
        for (int r = 0; r < 3; r++) {
            final int ry = y + 20 + r * 16;
            this.fontRendererObj.drawStringWithShadow(axis[r], x + 8, ry + 3, C_MUT);
            this.fontRendererObj.drawStringWithShadow("..", x + 110, ry + 3, C_DIM);
            if (spawnField[r * 2]     != null) spawnField[r * 2].drawTextBox();
            if (spawnField[r * 2 + 1] != null) spawnField[r * 2 + 1].drawTextBox();
        }

        // radius slider — feeds "Box around me"
        final int ry2 = y + 72;
        this.fontRendererObj.drawStringWithShadow("Radius", x + 8, ry2, C_MUT);
        this.fontRendererObj.drawStringWithShadow(
                String.format(Locale.ROOT, "%.0f", spawnRadius), x + 196, ry2, C_TEXT);
        final int tx = x + 56, tw = 132, ty = ry2 + 10, th = 6;
        drawRect(tx, ty, tx + tw, ty + th, C_TRACK);
        final float frac = Math.max(0.0f, Math.min(1.0f, (spawnRadius - 2.0f) / 46.0f));
        final int pad = 3;
        final int fillw = (int)(pad + frac * (tw - 2 * pad));
        if (fillw > 1) drawRect(tx, ty, tx + fillw, ty + th, (accent & 0xFFFFFF) | 0xE6000000);
        final float kx = tx + pad + frac * (tw - 2 * pad);
        circle(kx, ty + th / 2.0f, 4.0f, 0xFFF2F3F7);
        circle(kx, ty + th / 2.0f, 4.0f - 0.75f, (accent & 0xFFFFFF) | 0x66FFFFFF);
        circle(kx, ty + th / 2.0f, 4.0f - 1.5f, 0xFFF2F3F7);

        // buttons
        final int by = y + 84;
        final boolean ahov = inRect(mx, my, x + 8, by, 104, 13);
        glassRect(x + 8, by, 104, 13, ahov ? 0x30FFFFFF : C_DONE_BG, 0x2EFFFFFF);
        this.fontRendererObj.drawStringWithShadow("Box around me", x + 26, by + 3, C_TEXT);
        aroundHit = new int[] { x + 8, by };
        final boolean phov = inRect(mx, my, x + 120, by, 100, 13);
        glassRect(x + 120, by, 100, 13, phov ? 0x30FFFFFF : C_DONE_BG, 0x2EFFFFFF);
        this.fontRendererObj.drawStringWithShadow("Apply fields", x + 148, by + 3, C_TEXT);
        applyHit = new int[] { x + 120, by };

        // the two gates
        final int gy = spawnGatesY();
        glassRect(x, gy, w, 44, C_CARD, C_CARD_EDGE);
        this.fontRendererObj.drawStringWithShadow("Gates", x + 8, gy + 4, C_DIM);
        // aim gate — mod-owned, always editable
        this.fontRendererObj.drawStringWithShadow("No aim inside spawn", x + 8, gy + 17, C_TEXT);
        final int s1x = x + w - 8 - 22;
        drawSwitch(s1x, gy + 16, AimAssist.cfg.spawnBlockAim,
                inRect(mx, my, s1x, gy + 16, 22, 11));
        spawnAimHit = new int[] { s1x, gy + 16 };
        // click gate — exe-owned (spawn_gate), editable only with the link up
        final boolean gate = getB("spawn_gate", true);
        this.fontRendererObj.drawStringWithShadow("No clicks inside spawn", x + 8, gy + 33,
                linked ? C_TEXT : C_DIM);
        final int s2x = x + w - 8 - 22;
        drawSwitch(s2x, gy + 32, gate, linked && inRect(mx, my, s2x, gy + 32, 22, 11));
        spawnClickHit = new int[] { s2x, gy + 32 };
        if (!linked) this.fontRendererObj.drawStringWithShadow("(needs the clicker)",
                x + 120, gy + 33, C_DIM);

        // error / status line
        final String line = spawnErr != null ? spawnErr
                : inside ? "inside the box - aim freezes and clicks hold"
                         : "outside the box - nothing is held";
        this.fontRendererObj.drawStringWithShadow(line, px + 14, py + 200,
                spawnErr != null ? C_BAD : (inside ? C_WARN : C_DIM));
        doneButton(mx, my);
    }

    private void applySpawnFields() {
        try {
            final AimAssistConfig c = AimAssist.cfg;
            c.spawnX1 = Float.parseFloat(spawnField[0].getText().trim());
            c.spawnY1 = Float.parseFloat(spawnField[1].getText().trim());
            c.spawnZ1 = Float.parseFloat(spawnField[2].getText().trim());
            c.spawnX2 = Float.parseFloat(spawnField[3].getText().trim());
            c.spawnY2 = Float.parseFloat(spawnField[4].getText().trim());
            c.spawnZ2 = Float.parseFloat(spawnField[5].getText().trim());
            c.save();
            spawnErr = null;
        } catch (NumberFormatException e) {
            spawnErr = "bad number - use plain decimals, e.g. 57.00";
        }
    }

    private void spawnAroundMe() {
        final Minecraft mc0 = Minecraft.getMinecraft();
        if (mc0.thePlayer == null) { spawnErr = "join a world first"; return; }
        final double x0 = mc0.thePlayer.posX, y0 = mc0.thePlayer.posY, z0 = mc0.thePlayer.posZ;
        final float r = Math.max(2.0F, spawnRadius);
        final AimAssistConfig c = AimAssist.cfg;
        c.spawnX1 = (float) (x0 - r);  c.spawnY1 = (float) y0;       c.spawnZ1 = (float) (z0 - r);
        c.spawnX2 = (float) (x0 + r);  c.spawnY2 = (float) (y0 + 2); c.spawnZ2 = (float) (z0 + r);
        c.save();
        final float[] cur = {
            c.spawnX1, c.spawnY1, c.spawnZ1, c.spawnX2, c.spawnY2, c.spawnZ2
        };
        for (int i = 0; i < 6; i++) if (spawnField[i] != null) spawnField[i].setText(fmt(cur[i]));
        spawnErr = null;
    }

    /** Radius track geometry (draw + click + drag). */
    private boolean radiusHit(int mx, int my) {
        final int tx = spawnCardX() + 56, ty = spawnCardY() + 72 + 10;
        return my >= ty - 2 && my <= ty + 10 && mx >= tx - 2 && mx <= tx + 132 + 2;
    }

    private void radiusApply(int mx) {
        final int tx = spawnCardX() + 56;
        final float frac = Math.max(0.0f, Math.min(1.0f, (mx - tx) / 132.0f));
        spawnRadius = Math.round(2.0f + frac * 46.0f);
    }

    private void blurFields() {
        if (exAdd != null) exAdd.setFocused(false);
        if (lockAdd != null) lockAdd.setFocused(false);
        for (GuiTextField f : spawnField) if (f != null) f.setFocused(false);
    }

    // ---- input --------------------------------------------------------------
    @Override
    protected void mouseClicked(int mx, int my, int btn) {
        if (btn != 0) return;

        // tab pills — always available
        for (Map.Entry<Integer, int[]> e : tabHit.entrySet()) {
            final int[] r = e.getValue();
            if (inRect(mx, my, r[0], r[1], r[2], 13)) {
                if (tab != e.getKey()) {
                    tab = e.getKey();
                    lastTab = tab;
                    dragKey = aimDrag = null;   // never carry a drag across tabs
                    radiusDrag = false; spawnErr = null;
                    blurFields();
                }
                return;
            }
        }
        // Done — always available
        if (inRect(mx, my, doneRect(), py + PH - 24, 44, 14)) { close(); return; }

        // friends tab: edit the mod's own lists
        if (tab == 2) {
            if (clickField(exAdd, exAddBox, mx, my, btn)) return;
            if (clickField(lockAdd, lockAddBox, mx, my, btn)) return;
            final List<String> exL = AimAssist.cfg.manualExceptions;
            for (Map.Entry<Integer, int[]> e : exRemoveHit.entrySet()) {
                final int[] r = e.getValue();
                if (inRect(mx, my, r[0], r[1], 10, 10)) {
                    if (e.getKey() < exL.size()) AimAssist.cfg.removeException(exL.get(e.getKey()));
                    return;
                }
            }
            final List<String> lkL = AimAssist.cfg.lockNames;
            for (Map.Entry<Integer, int[]> e : lockRemoveHit.entrySet()) {
                final int[] r = e.getValue();
                if (inRect(mx, my, r[0], r[1], 10, 10)) {
                    if (e.getKey() < lkL.size()) AimAssist.cfg.removeLock(lkL.get(e.getKey()));
                    return;
                }
            }
            if (exClearHit != null && inRect(mx, my, exClearHit[0], exClearHit[1], 34, 12)) {
                AimAssist.cfg.clearExceptions();
                return;
            }
            if (lockSwitchHit != null && inRect(mx, my, lockSwitchHit[0], lockSwitchHit[1], 22, 11)) {
                AimAssist.cfg.lockEnabled = !AimAssist.cfg.lockEnabled;
                AimAssist.cfg.save();
                return;
            }
            if (exAddHit != null && inRect(mx, my, exAddHit[0], exAddHit[1], 40, 13)) {
                addFromField(exAdd, true); return;
            }
            if (lockAddHit != null && inRect(mx, my, lockAddHit[0], lockAddHit[1], 40, 13)) {
                addFromField(lockAdd, false); return;
            }
            return;
        }

        // spawn tab: mod-side box + gates (click gate needs the link)
        if (tab == 3) {
            boolean fieldHit = false;
            for (int i = 0; i < 6; i++)
                if (clickField(spawnField[i], spawnBox[i], mx, my, btn)) fieldHit = true;
            if (fieldHit) return;
            if (radiusHit(mx, my)) { radiusDrag = true; radiusApply(mx); return; }
            if (aroundHit != null && inRect(mx, my, aroundHit[0], aroundHit[1], 104, 13)) {
                spawnAroundMe(); return;
            }
            if (applyHit != null && inRect(mx, my, applyHit[0], applyHit[1], 100, 13)) {
                applySpawnFields(); return;
            }
            if (spawnAimHit != null && inRect(mx, my, spawnAimHit[0], spawnAimHit[1], 22, 11)) {
                AimAssist.cfg.spawnBlockAim = !AimAssist.cfg.spawnBlockAim;
                AimAssist.cfg.save();
                return;
            }
            if (spawnClickHit != null && inRect(mx, my, spawnClickHit[0], spawnClickHit[1], 22, 11)) {
                if (linked && haveCfg) toggle("spawn_gate", true);
                return;
            }
            return;
        }

        // aim tab: edit the mod's own config
        if (tab == 1) {
            if (masterHit != null && inRect(mx, my, masterHit[0], masterHit[1], 22, 11)) {
                AimAssist.toggle();             // flips + saves + announces
                return;
            }
            if (aimCardClick(AIM_LEFT,  px + 8,   mx, my)) return;
            if (aimCardClick(AIM_RIGHT, px + 125, mx, my)) return;
            for (Map.Entry<String, int[]> e : aimRowHit.entrySet()) {
                final int[] r = e.getValue();
                if (inRect(mx, my, r[0], r[1], 22, 11)) {
                    aimToggle(e.getKey());
                    return;
                }
            }
            return;
        }

        if (!linked || !haveCfg) return;

        // side cards
        handleSideClick(px + 8,   "left_enabled",  SIDE_SLIDERS,  mx, my);
        handleSideClick(px + 125, "right_enabled", RIGHT_SLIDERS, mx, my);

        // activation card
        for (Map.Entry<String, int[]> e : rowHit.entrySet()) {
            final int[] r = e.getValue();
            if (inRect(mx, my, r[0], r[1], 22, 11)) {
                if (e.getKey().equals("hold_mode")) toggle("hold_mode", true);
                else if (e.getKey().equals("require_focus")) toggle("require_focus", true);
                else if (e.getKey().equals("legacy_timing")) toggle("legacy_timing", false);
                else if (e.getKey().equals("blockhit")) toggle("blockhit", false);
                else if (e.getKey().equals("spawn_gate")) toggle("spawn_gate", true);
                else if (e.getKey().equals("allyHold")) {
                    // mod-owned: lives in config/abaser_aim.json, not the exe
                    AimAssist.cfg.allyHold = !AimAssist.cfg.allyHold;
                    AimAssist.cfg.save();
                }
                return;
            }
        }
        // block-hit chance slider (row 3 of the activation card)
        if (getB("blockhit", false)) {
            if (sliderHit(BLOCK_CHANCE, bhX(), bhY(), bhW(), mx, my, true)) {
                sendWork();
                dragKey = BLOCK_CHANCE.key;
                return;
            }
        }

        // theme swatches
        for (Map.Entry<Integer, int[]> e : themeHit.entrySet()) {
            final int[] r = e.getValue();
            if (inRect(mx, my, r[0] - 1, r[1] - 1, 16, 14)) {
                put("theme_primary", String.valueOf(PRESETS[e.getKey()][0]));
                accent = PRESETS[e.getKey()][1];
                sendWork();
                return;
            }
        }
    }

    private boolean aimCardClick(AimSlider[] sliders, int x, int mx, int my) {
        final int y = py + 54, w = 111;
        for (int i = 0; i < sliders.length; i++) {
            final int ry = y + 28 + i * 21;
            if (aimSliderHit(sliders[i], x + 6, ry, w - 12, mx, my, true)) {
                aimDrag = sliders[i].key;       // saved on release, like the exe sliders
                return true;
            }
        }
        return false;
    }

    private void handleSideClick(int x, String toggleKey, Slider[] sliders, int mx, int my) {
        final int y = py + 38, w = 111;
        // toggle
        final int sx = x + w - 6 - 22, sy = y + 15;
        if (inRect(mx, my, sx, sy, 22, 11)) { toggle(toggleKey, false); return; }
        // sliders
        for (int i = 0; i < sliders.length; i++) {
            final int ry = y + 30 + i * 22;
            if (sliderHit(sliders[i], x + 6, ry, w - 12, mx, my, true)) {
                sendWork();
                dragKey = sliders[i].key;
                return;
            }
        }
    }

    @Override
    protected void mouseClickMove(int mx, int my, int btn, long timeSinceLast) {
        if (btn != 0) return;
        if (radiusDrag) { radiusApply(mx); return; }
        if (aimDrag != null) {
            for (int a = 0; a < 2; a++) {
                final AimSlider[] arr = a == 0 ? AIM_LEFT : AIM_RIGHT;
                final int x = a == 0 ? px + 8 : px + 125;
                for (int i = 0; i < arr.length; i++) {
                    if (arr[i].key.equals(aimDrag)) {
                        aimSliderHit(arr[i], x + 6, py + 54 + 28 + i * 21, 99, mx, my, true);
                        return;
                    }
                }
            }
            return;
        }
        if (dragKey == null) return;
        if (dragKey.equals("left_max_cps") || dragKey.equals("left_min_cps") || dragKey.equals("hit_chance"))
            dragSide(SIDE_SLIDERS, px + 8, mx, my);
        else if (dragKey.equals("right_max_cps") || dragKey.equals("right_min_cps") || dragKey.equals("right_chance"))
            dragSide(RIGHT_SLIDERS, px + 125, mx, my);
        else if (dragKey.equals("blockhit_chance"))
            sliderHit(BLOCK_CHANCE, bhX(), bhY(), bhW(), mx, my, true);
    }

    private void dragSide(Slider[] sliders, int x, int mx, int my) {
        for (int i = 0; i < sliders.length; i++) {
            if (sliders[i].key.equals(dragKey)) {
                sliderHit(sliders[i], x + 6, py + 38 + 30 + i * 22, 99, mx, my, true);
                return;
            }
        }
    }

    @Override
    protected void mouseReleased(int mx, int my, int state) {
        radiusDrag = false;
        if (aimDrag != null) {
            aimDrag = null;
            AimAssist.cfg.clamp();
            AimAssist.cfg.save();
        }
        if (dragKey != null) { dragKey = null; sendWork(); }
    }

    @Override
    public void updateScreen() {
        if (exAdd != null) exAdd.updateCursorCounter();
        if (lockAdd != null) lockAdd.updateCursorCounter();
        for (GuiTextField f : spawnField) if (f != null) f.updateCursorCounter();
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) {
        // text fields first — they must see every key while focused
        if (exAdd != null) exAdd.textboxKeyTyped(typedChar, keyCode);
        if (lockAdd != null) lockAdd.textboxKeyTyped(typedChar, keyCode);
        for (GuiTextField f : spawnField) if (f != null) f.textboxKeyTyped(typedChar, keyCode);
        // Enter in a name field = Add
        if (keyCode == Keyboard.KEY_RETURN) {
            if (exAdd != null && exAdd.isFocused()) { addFromField(exAdd, true); return; }
            if (lockAdd != null && lockAdd.isFocused()) { addFromField(lockAdd, false); return; }
        }
        if (keyCode == Keyboard.KEY_ESCAPE) { rememberTab(); close(); return; }
        if (ClientProxy.panelKey != null && keyCode == ClientProxy.panelKey.getKeyCode()) {
            rememberTab(); close();
        }
    }

    private void rememberTab() { lastTab = tab; }

    private void close() {
        rememberTab();
        this.mc.displayGuiScreen(null);
    }
}
