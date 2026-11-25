package net.zaminmc.abaser;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

import org.lwjgl.input.Keyboard;
import org.lwjgl.opengl.GL11;

import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.renderer.GlStateManager;

/**
 * The in-game Abaser panel — a small glass-styled settings screen that talks
 * to the standalone Abaser Clicker exe over the mod link.
 *
 * This screen is a COCKPIT, not the engine: every switch and slider here
 * becomes one {"config":{...}} line on the exe's stdin, the exe applies it
 * with the same clamps as its own UI, persists it to abaser.ini and echoes
 * the canonical snapshot back. Nothing here ever generates a click — the
 * exe keeps doing all of the clicking with its own engine, its own hotkeys
 * and its own PostMessage targeting.
 *
 * While this screen is open the mod reports gui=true (it is a GUI like any
 * other), so the exe holds every click while you configure it.
 *
 * Layout is hand-drawn (no vanilla Button widgets) to match the exe's
 * Liquid-Glass look: translucent cards, 1 px light borders, iOS toggles,
 * capsule sliders and theme swatches — all in Minecraft's scaled GUI space.
 */
public class AbaserConfigGui extends GuiScreen {

    // ---- panel geometry (scaled px) ---------------------------------------
    private static final int PW = 244, PH = 236;

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

    // ---- state --------------------------------------------------------------
    private final Map<String, String> work = new HashMap<String, String>();
    private String dragKey = null;          // slider currently being dragged
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
        pullFromExe(true);
        // ask again in case the handshake echo has not landed yet
        if (linked && !haveCfg) ClientTickHandler.sendConfigLine("{\"get\":\"config\"}");
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
        put("legacy_timing", "false");
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

        if (!linked) {
            drawNotRunning(mouseX, mouseY);
            return;
        }
        if (!haveCfg) {
            drawCentered("Syncing settings from the clicker...", py + 100, C_MUT);
            doneButton(mouseX, mouseY);
            return;
        }

        drawHeader();
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
        drawCentered("Abaser Clicker is not running", py + 92, C_TEXT);
        drawCentered("Join a world - the mod starts it in your tray.", py + 106, C_MUT);
        doneButton(mouseX, mouseY);
    }

    private void drawHeader() {
        this.fontRendererObj.drawStringWithShadow("Abaser Clicker", px + 10, py + 7, C_TEXT);
        // live status dot + word, right-aligned
        String st; int dot;
        if (LinkState.guiOpen)       { st = "menu open"; dot = C_WARN; }
        else if (!LinkState.mcFound) { st = "no game";   dot = C_BAD; }
        else                         { st = "in-game";   dot = C_OK; }
        final int tw = this.fontRendererObj.getStringWidth(st);
        this.fontRendererObj.drawStringWithShadow(st, px + PW - 10 - tw - 8, py + 7, C_MUT);
        circle(px + PW - 10 - 3, py + 11.5f, 2.5f, dot);
    }

    private void drawSideCard(int x, String title, boolean on, String toggleKey, Slider[] sliders, int mx, int my) {
        final int y = py + 22, w = 111, h = 98;
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
        final int x = px + 8, y = py + 128, w = 228, h = 52;
        glassRect(x, y, w, h, C_CARD, C_CARD_EDGE);
        final int ax0 = x + 6, ax1 = x + w - 6;
        final int col2 = ax0 + 110;

        // row 1
        toggleRow("Hold to click", "hold_mode", true, ax0, y + 6, ax0 + 104, mx, my);
        toggleRow("Focus gate", "require_focus", true, col2, y + 6, ax1, mx, my);
        // row 2
        toggleRow("Legacy timing", "legacy_timing", false, ax0, y + 24, ax0 + 104, mx, my);
        toggleRow("Block hit", "blockhit", false, col2, y + 24, ax1, mx, my);
        // row 3 — block-hit chance, only meaningful while block hit is on
        final int ry = y + 40;
        if (getB("blockhit", false)) {
            drawSliderRow(BLOCK_CHANCE, bhX(), ry, bhW(), mx, my);
        } else {
            this.fontRendererObj.drawStringWithShadow("Sword-block taps ride along with left clicks.", ax0 + 6, ry, C_DIM);
        }
    }

    // block-hit chance slider geometry — shared by draw / click / drag
    private int bhX() { return px + 20; }
    private int bhY() { return py + 168; }
    private int bhW() { return 210; }

    private void toggleRow(String label, String key, boolean def, int x, int y, int rowRight, int mx, int my) {
        this.fontRendererObj.drawStringWithShadow(label, x, y + 1, C_TEXT);
        final int sx = rowRight - 22;
        drawSwitch(sx, y, getB(key, def), inRect(mx, my, sx, y, 22, 11));
        rowHit.put(key, new int[] { sx, y });
    }

    private final Map<String, int[]> rowHit = new HashMap<String, int[]>();

    private void drawThemeRow(int mx, int my) {
        final int y = py + 186;
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
        final int y = py + 204;
        // live line, dot + "x.x L / y.y R cps"
        final boolean armed = getB("left_enabled", false) || getB("right_enabled", false);
        final String live = String.format(Locale.ROOT, "%.1f L / %.1f R cps live",
                LinkState.cpsLeft, LinkState.cpsRight);
        circle(px + 16, y + 5, 2.5f, armed ? C_OK : 0x50FFFFFF);
        this.fontRendererObj.drawStringWithShadow(live, px + 22, y + 1, armed ? C_TEXT : C_MUT);

        // the same honesty the exe's footer has: say WHY nothing clicks
        String why = null; int whyCol = C_DIM;
        if (LinkState.guiOpen)            { why = "paused - a menu is open"; whyCol = C_WARN; }
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

    // ---- input --------------------------------------------------------------
    @Override
    protected void mouseClicked(int mx, int my, int btn) {
        if (btn != 0) return;
        if (!linked || !haveCfg) {
            if (inRect(mx, my, doneRect(), py + PH - 24, 44, 14)) close();
            return;
        }
        // Done
        if (inRect(mx, my, doneRect(), py + PH - 24, 44, 14)) { close(); return; }

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

    private void handleSideClick(int x, String toggleKey, Slider[] sliders, int mx, int my) {
        final int y = py + 22, w = 111;
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
        if (btn != 0 || dragKey == null) return;
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
                sliderHit(sliders[i], x + 6, py + 52 + i * 22, 99, mx, my, true);
                return;
            }
        }
    }

    @Override
    protected void mouseReleased(int mx, int my, int state) {
        if (dragKey != null) { dragKey = null; sendWork(); }
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) {
        if (keyCode == Keyboard.KEY_ESCAPE) { close(); return; }
        if (ClientProxy.panelKey != null && keyCode == ClientProxy.panelKey.getKeyCode()) close();
    }

    private void close() {
        this.mc.displayGuiScreen(null);
    }
}
