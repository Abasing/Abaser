package com.runegaming.vegaclicker.gui;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.Consumer;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.ScaledResolution;

import com.runegaming.vegaclicker.VegaConfig;
import com.runegaming.vegaclicker.VegaKeys;

/**
 * In-game recreation of the original Vega-Clicker ImGui interface:
 *
 *   +-----------------------------------------------------------+
 *   | Vega - Clicker V2 | Coming Soon Edition   <- accent bar   |
 *   | Clicker   |  tab content (sliders, toggles, notes)        |
 *   | Misc      |                                                |
 *   | Configs   |  (finally reachable - it was dead code in     |
 *   | Settings  |   the original binary)                         |
 *   | Made      |                                                |
 *   | with <3   |  vertical accent separator, just like the      |
 *   | RuneGaming|  original "DrawList" overlay line              |
 *   +-----------------------------------------------------------+
 *
 * The panel is draggable by its title bar (ImGui windows moved like that),
 * preferences autosave when the menu closes, and INSERT or ESC closes it.
 */
public class VegaGuiScreen extends GuiScreen {

    private static final int PANEL_W = 268;
    private static final int PANEL_H = 186;
    private static final int SIDE_W  = 56;   // 107px in the original at 2x scale
    private static final int TITLE_H = 13;
    private static final int ROW     = 12;

    private static final String[] TABS = { "Clicker", "Misc", "Configs", "Settings" };

    private int tab = 0;                     // 0 Clicker | 1 Misc | 2 Configs | 3 Settings
    private int panelX = 0, panelY = 0;
    private boolean dragging = false;
    private int dragOffX = 0, dragOffY = 0;
    private String status = "";

    private int mouseX = 0, mouseY = 0;

    // ------------------------------------------------------------------
    // Minimal immediate-mode-ish widget system: every frame the draw code
    // registers clickable rects + slider rects; input events hit-test them.
    // ------------------------------------------------------------------

    private static final class Clickable {
        final int x, y, w, h;
        final Runnable action;
        Clickable(int x, int y, int w, int h, Runnable action) {
            this.x = x; this.y = y; this.w = w; this.h = h; this.action = action;
        }
    }

    private static final class SliderW {
        final int x, y, w, h;
        final float min, max;
        final Consumer<Float> set;
        SliderW(int x, int y, int w, int h, float min, float max, Consumer<Float> set) {
            this.x = x; this.y = y; this.w = w; this.h = h;
            this.min = min; this.max = max; this.set = set;
        }
    }

    private final List<Clickable> clickables = new ArrayList<Clickable>();
    private final List<SliderW> sliders = new ArrayList<SliderW>();
    private SliderW activeSlider = null;

    // ------------------------------------------------------------------

    @Override
    public void initGui() {
        ScaledResolution sr = new ScaledResolution(mc);
        panelX = (sr.getScaledWidth() - PANEL_W) / 2;
        panelY = (sr.getScaledHeight() - PANEL_H) / 2;
    }

    @Override
    public boolean doesGuiPauseGame() {
        return false; // the world keeps running behind the menu
    }

    @Override
    public void drawScreen(int mouseX, int mouseY, float partialTicks) {
        this.mouseX = mouseX;
        this.mouseY = mouseY;
        clickables.clear();
        sliders.clear();

        drawDefaultBackground();

        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();
        int panelBg = VegaConfig.otherBackground ? 0xE60C161C : 0xE61B1616;

        // panel frame
        drawRect(panelX, panelY, panelX + PANEL_W, panelY + PANEL_H, 0xFF000000);
        drawRect(panelX + 1, panelY + 1, panelX + PANEL_W - 1, panelY + PANEL_H - 1, panelBg);

        // title bar: accent fill + white text (ImGui default title styling)
        drawRect(panelX + 1, panelY + 1, panelX + PANEL_W - 1, panelY + TITLE_H, accent);
        fr.drawStringWithShadow("Vega - Clicker V2 | Coming Soon Edition", panelX + 5, panelY + 3, 0xFFFFFFFF);

        // side panel + the accent separator from the original "DrawList" window
        drawRect(panelX + 1, panelY + TITLE_H, panelX + SIDE_W, panelY + PANEL_H - 1, 0x60101010);
        drawRect(panelX + SIDE_W, panelY + 1, panelX + SIDE_W + 2, panelY + PANEL_H - 1, accent);

        // tab buttons
        for (int i = 0; i < TABS.length; i++) {
            int ty = panelY + TITLE_H + 6 + i * ROW;
            boolean active = (i == tab);
            boolean hover = in(panelX + 2, ty - 2, SIDE_W - 8, ROW - 1, mouseX, mouseY);
            if (active) {
                drawRect(panelX + 2, ty + fr.FONT_HEIGHT,
                         panelX + 2 + fr.getStringWidth(TABS[i]), ty + fr.FONT_HEIGHT + 1, accent);
            }
            int color = active ? accent : (hover ? 0xFFFFFFFF : 0xFF9C9C9C);
            fr.drawStringWithShadow(TABS[i], panelX + 5, ty, color);
            final int idx = i;
            clickables.add(new Clickable(panelX + 2, ty - 2, SIDE_W - 8, ROW - 1,
                    new Runnable() { public void run() { tab = idx; } }));
        }

        // author credit, exactly as obfuscated inside the original binary
        fr.drawStringWithShadow("Made with <3", panelX + 5, panelY + PANEL_H - 27, 0xFF9C9C9C);
        fr.drawStringWithShadow("RuneGaming", panelX + 5, panelY + PANEL_H - 16, 0xFF9C9C9C);

        // tab content
        int cx = panelX + SIDE_W + 8;
        int cy = panelY + TITLE_H + 6;
        switch (tab) {
            case 0: drawClickerTab(cx, cy);  break;
            case 1: drawMiscTab(cx, cy);     break;
            case 2: drawConfigsTab(cx, cy);  break;
            case 3: drawSettingsTab(cx, cy); break;
        }

        super.drawScreen(mouseX, mouseY, partialTicks);
    }

    // ---------------------------------------------------------------- tabs

    private void drawClickerTab(int x, int y) {
        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();

        toggle(x, y,          "Left clicker",  VegaConfig.leftEnabled,  new ToggleSet() { public void set(boolean v) { VegaConfig.leftEnabled = v; } });
        toggle(x, y + ROW,    "Right clicker", VegaConfig.rightEnabled, new ToggleSet() { public void set(boolean v) { VegaConfig.rightEnabled = v; } });

        int sy = y + ROW * 2 + 3;
        slider(x, sy + ROW * 0, "Left CPS min",  VegaConfig.leftMinCps,  7.5f, 18.0f, "%.1f", new SliderSet() { public void set(float v) { VegaConfig.leftMinCps = Math.min(v, VegaConfig.leftMaxCps); } });
        slider(x, sy + ROW * 1, "Left CPS max",  VegaConfig.leftMaxCps,  10.0f, 20.0f, "%.1f", new SliderSet() { public void set(float v) { VegaConfig.leftMaxCps = Math.max(v, VegaConfig.leftMinCps); } });
        slider(x, sy + ROW * 2, "Right CPS min", VegaConfig.rightMinCps, 7.5f, 18.0f, "%.1f", new SliderSet() { public void set(float v) { VegaConfig.rightMinCps = Math.min(v, VegaConfig.rightMaxCps); } });
        slider(x, sy + ROW * 3, "Right CPS max", VegaConfig.rightMaxCps, 10.0f, 20.0f, "%.1f", new SliderSet() { public void set(float v) { VegaConfig.rightMaxCps = Math.max(v, VegaConfig.rightMinCps); } });
        slider(x, sy + ROW * 4, "Hit chance",    VegaConfig.hitChance,   75, 100, "%.0f%%", new SliderSet() { public void set(float v) { VegaConfig.hitChance = Math.round(v); } });
        slider(x, sy + ROW * 5, "Right chance",  VegaConfig.rightChance, 75, 100, "%.0f%%", new SliderSet() { public void set(float v) { VegaConfig.rightChance = Math.round(v); } });

        int by = sy + ROW * 6 + 4;
        String binds = "Bind: " + VegaKeys.keyName(VegaKeys.toggleClicker)
                + " toggle | " + VegaKeys.keyName(VegaKeys.openGui) + " menu";
        fr.drawStringWithShadow(binds, x, by, 0xFF9C9C9C);
        fr.drawStringWithShadow("Rebind in Options > Controls >", x, by + 10, 0xFF6E6E6E);
        fr.drawStringWithShadow("the Vega Clicker category", x, by + 20, 0xFF6E6E6E);
        if (VegaConfig.legacyTiming) {
            fr.drawStringWithShadow("Legacy timing ON - rate ~2x label", x, by + 32, accent);
        }
    }

    private void drawMiscTab(int x, int y) {
        FontRenderer fr = mc.fontRendererObj;

        toggle(x, y, "Block Hit", VegaConfig.blockHit, new ToggleSet() { public void set(boolean v) { VegaConfig.blockHit = v; } });
        slider(x, y + ROW, "Block Hit chance", VegaConfig.blockHitChance, 1, 25, "%.0f%%", new SliderSet() { public void set(float v) { VegaConfig.blockHitChance = Math.round(v); } });

        int ny = y + ROW * 2 + 4;
        fr.drawStringWithShadow("Taps right-click right after a left", x, ny, 0xFF8A8A8A);
        fr.drawStringWithShadow("click - the classic 1.8 block-hit.", x, ny + 10, 0xFF8A8A8A);

        int my = ny + 26;
        toggle(x, my, "Legacy Vega timing", VegaConfig.legacyTiming, new ToggleSet() { public void set(boolean v) { VegaConfig.legacyTiming = v; } });
        fr.drawStringWithShadow("Original engine slept 100/cps per click.", x, my + 11, 0xFF6E6E6E);

        toggle(x, my + 24, "Chat feedback on toggle", VegaConfig.toggleChatFeedback, new ToggleSet() { public void set(boolean v) { VegaConfig.toggleChatFeedback = v; } });
    }

    private void drawConfigsTab(int x, int y) {
        FontRenderer fr = mc.fontRendererObj;

        button(x,       y, 60, "Save",     new Runnable() { public void run() { VegaConfig.save();          status = "Saved to config/vegaclicker.cfg"; } });
        button(x + 66,  y, 60, "Reload",   new Runnable() { public void run() { VegaConfig.reload();        status = "Reloaded from disk"; } });
        button(x + 132, y, 60, "Defaults", new Runnable() { public void run() { VegaConfig.resetDefaults(); VegaConfig.save(); status = "Defaults restored + saved"; } });

        int ny = y + 18;
        if (!status.isEmpty()) {
            fr.drawStringWithShadow(status, x, ny, VegaConfig.accentColor());
        }
        fr.drawStringWithShadow("Preferences also autosave when the", x, ny + 12, 0xFF8A8A8A);
        fr.drawStringWithShadow("menu closes, so tweaks just stick.", x, ny + 22, 0xFF8A8A8A);
        fr.drawStringWithShadow("The original's Configs tab said", x, ny + 36, 0xFF6E6E6E);
        fr.drawStringWithShadow("NOTHING YET - this one ships :)", x, ny + 46, 0xFF6E6E6E);
    }

    private void drawSettingsTab(int x, int y) {
        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();

        slider(x, y + ROW * 0, "Accent red",   VegaConfig.accentR * 255f, 0, 255, "%.0f", new SliderSet() { public void set(float v) { VegaConfig.accentR = v / 255f; } });
        slider(x, y + ROW * 1, "Accent green", VegaConfig.accentG * 255f, 0, 255, "%.0f", new SliderSet() { public void set(float v) { VegaConfig.accentG = v / 255f; } });
        slider(x, y + ROW * 2, "Accent blue",  VegaConfig.accentB * 255f, 0, 255, "%.0f", new SliderSet() { public void set(float v) { VegaConfig.accentB = v / 255f; } });

        // live preview
        drawRect(x + 84, y + ROW * 3 + 2, x + 144, y + ROW * 3 + 10, accent);
        fr.drawStringWithShadow(VegaConfig.accentHex(), x + 150, y + ROW * 3 + 2, 0xFFE8E8E8);

        toggle(x, y + ROW * 4 + 4, "Other Background", VegaConfig.otherBackground, new ToggleSet() { public void set(boolean v) { VegaConfig.otherBackground = v; } });
        toggle(x, y + ROW * 5 + 4, "HUD overlay",      VegaConfig.hudEnabled,      new ToggleSet() { public void set(boolean v) { VegaConfig.hudEnabled = v; } });

        int fy = y + ROW * 6 + 8;
        int fps = Minecraft.getDebugFPS();
        // the classic ImGui demo metrics line the original window showed
        String fpsLine = String.format(Locale.US,
                "Application average %.3f ms/frame (%.1f FPS)",
                fps > 0 ? 1000.0 / fps : 0.0, (double) fps);
        fr.drawStringWithShadow(fpsLine, x, fy, 0xFF8A8A8A);
        fr.drawStringWithShadow("F4 toggles - INSERT opens this menu", x, fy + 11, 0xFF6E6E6E);
    }

    // ------------------------------------------------------------- widgets

    /** avoid java.util.function (works everywhere, even old IDE setups). */
    private interface ToggleSet { void set(boolean v); }
    private interface SliderSet { void set(float v); }

    private void toggle(int x, int y, String label, boolean value, final ToggleSet set) {
        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();

        fr.drawStringWithShadow(label, x, y, 0xFFC8C8C8);

        int bx = x + 152, by = y;
        drawRect(bx, by, bx + 9, by + 9, 0xFF5A5A5A);
        drawRect(bx + 1, by + 1, bx + 8, by + 8, value ? accent : 0xFF161616);
        fr.drawStringWithShadow(value ? "ON" : "OFF", bx + 14, y, value ? accent : 0xFF8A8A8A);

        clickables.add(new Clickable(bx - 2, by - 2, 44, 12, new Runnable() {
            public void run() { set.set(!value); }
        }));
    }

    private void slider(int x, int y, String label, float value, float min, float max,
                        String fmt, final SliderSet set) {
        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();

        int labelW = 84;
        int tx = x + labelW, tw = 80, ty = y + 3;

        fr.drawStringWithShadow(label, x, y, 0xFFC8C8C8);
        drawRect(tx, ty, tx + tw, ty + 4, 0xFF3A3A3A);

        float t = max > min ? (value - min) / (max - min) : 0f;
        if (t < 0f) t = 0f;
        if (t > 1f) t = 1f;
        int fw = (int) (tw * t);
        drawRect(tx, ty, tx + fw, ty + 4, accent);
        drawRect(tx + fw - 1, ty - 2, tx + fw + 1, ty + 6, 0xFFFFFFFF);

        String vtxt = String.format(Locale.US, fmt, value);
        fr.drawStringWithShadow(vtxt, tx + tw + 5, y, 0xFFE8E8E8);

        sliders.add(new SliderW(tx, y - 3, tw, 12, min, max, new Consumer<Float>() {
            public void accept(Float v) { set.set(v); }
        }));
    }

    private void button(int x, int y, int w, String label, final Runnable action) {
        FontRenderer fr = mc.fontRendererObj;
        int accent = VegaConfig.accentColor();

        boolean hover = in(x, y, w, 12, mouseX, mouseY);
        drawRect(x, y, x + w, y + 12, 0xFF5A5A5A);
        drawRect(x + 1, y + 1, x + w - 1, y + 11, hover ? 0xFF464646 : 0xFF242424);
        drawRect(x + 1, y + 10, x + w - 1, y + 11, accent);
        fr.drawStringWithShadow(label, x + (w - fr.getStringWidth(label)) / 2, y + 2,
                hover ? 0xFFFFFFFF : 0xFFC8C8C8);

        clickables.add(new Clickable(x, y, w, 12, action));
    }

    // --------------------------------------------------------------- input

    @Override
    protected void mouseClicked(int mouseX, int mouseY, int mouseButton) throws IOException {
        if (mouseButton == 0) {
            // title bar = drag handle, like an ImGui window
            if (in(panelX, panelY, PANEL_W, TITLE_H, mouseX, mouseY)) {
                dragging = true;
                dragOffX = mouseX - panelX;
                dragOffY = mouseY - panelY;
                return;
            }
            for (int i = sliders.size() - 1; i >= 0; i--) {
                SliderW s = sliders.get(i);
                if (in(s.x, s.y, s.w, s.h, mouseX, mouseY)) {
                    activeSlider = s;
                    applySlider(s, mouseX);
                    return;
                }
            }
            for (int i = clickables.size() - 1; i >= 0; i--) {
                Clickable c = clickables.get(i);
                if (in(c.x, c.y, c.w, c.h, mouseX, mouseY)) {
                    c.action.run();
                    return;
                }
            }
        }
        super.mouseClicked(mouseX, mouseY, mouseButton);
    }

    @Override
    protected void mouseClickMove(int mouseX, int mouseY, int clickedButton, long timeSinceLastClick) {
        if (dragging) {
            panelX = mouseX - dragOffX;
            panelY = mouseY - dragOffY;
        }
        if (activeSlider != null) {
            applySlider(activeSlider, mouseX);
        }
    }

    @Override
    protected void mouseReleased(int mouseX, int mouseY, int state) {
        dragging = false;
        activeSlider = null;
    }

    private void applySlider(SliderW s, int mouseX) {
        float t = (mouseX - s.x) / (float) s.w;
        if (t < 0f) t = 0f;
        if (t > 1f) t = 1f;
        s.set.accept(s.min + t * (s.max - s.min));
    }

    @Override
    protected void keyTyped(char typedChar, int keyCode) throws IOException {
        if (VegaKeys.openGui != null && keyCode == VegaKeys.openGui.getKeyCode()) {
            mc.displayGuiScreen(null);
            return;
        }
        super.keyTyped(typedChar, keyCode); // ESC and friends
    }

    @Override
    public void onGuiClosed() {
        VegaConfig.save(); // preference saving
        super.onGuiClosed();
    }

    // -------------------------------------------------------------- helpers

    private static boolean in(int x, int y, int w, int h, int mx, int my) {
        return mx >= x && my >= y && mx < x + w && my < y + h;
    }
}
