package net.zaminmc.abaser;

import java.io.File;

import net.minecraftforge.common.config.Configuration;
import net.minecraftforge.fml.client.event.ConfigChangedEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

/**
 * Preference saving. Everything the menu exposes lives here and is
 * persisted to config/abaser.cfg -- preferences survive reboots.
 *
 * Clicker defaults are the values recovered from the original binary
 * (see legacy/analysis/REVERSE_ENGINEERING_NOTES.md); the safety options
 * are sane mod-side additions.
 */
public class AbaserConfig {

    public static final String CATEGORY_CLICKER = "clicker";
    public static final String CATEGORY_MISC    = "misc";
    public static final String CATEGORY_GUI     = "gui";
    public static final String CATEGORY_SAFETY  = "safety";

    // ---- clicker (defaults recovered from the original .data section) ----
    public static boolean leftEnabled  = false;         // toggled at runtime with F4
    public static boolean rightEnabled = false;
    public static float   leftMinCps   = 9.6f;          // slider range 7.5 - 18
    public static float   leftMaxCps   = 14.0f;         // slider range 10 - 20
    public static float   rightMinCps  = 13.0f;         // slider range 7.5 - 18
    public static float   rightMaxCps  = 20.0f;         // slider range 10 - 20
    public static int     hitChance    = 87;            // slider range 75 - 100
    public static int     rightChance  = 80;            // original quirk: engine never read this in legacy mode

    // ---- misc ----
    public static boolean blockHit          = true;
    public static int     blockHitChance    = 6;        // slider range 1 - 25
    public static boolean legacyTiming      = true;     // reproduce the original 100/cps delay math
    public static boolean toggleChatFeedback = true;

    // ---- safety ----
    public static boolean noBowRight     = true;   // no right clicks while holding a bow
    public static boolean noUseInterrupt = true;   // no right clicks while eating / drinking / using an item
    public static boolean swordOnly      = false;  // left clicks only while holding a sword
    public static boolean pauseUnfocused = true;   // idle while the game window is unfocused

    // ---- gui (accent color: 1.00, 0.137, 1.00 in the original) ----
    public static float    accentR         = 1.000f;
    public static float    accentG         = 0.137f;
    public static float    accentB         = 1.000f;
    public static boolean otherBackground  = true;      // 0C161C instead of 1B1616
    public static boolean hudEnabled       = true;

    public static Configuration configuration;
    public static File configFile;

    // ------------------------------------------------------------------ load

    public static void load(File file) {
        configFile = file;
        configuration = new Configuration(file);
        read();
    }

    /** Re-read the file from disk (Reload button / config-changed event). */
    public static void reload() {
        if (configuration == null) return;
        configuration.load();
        read();
    }

    private static void read() {
        if (configuration == null) return;

        leftEnabled  = configuration.getBoolean("leftEnabled",  CATEGORY_CLICKER, leftEnabled,  "Left auto-clicker enabled");
        rightEnabled = configuration.getBoolean("rightEnabled", CATEGORY_CLICKER, rightEnabled, "Right auto-clicker enabled");
        leftMinCps   = configuration.getFloat("leftMinCps",  CATEGORY_CLICKER, leftMinCps,  7.5f, 18.0f, "Left CPS minimum (original default 9.6)");
        leftMaxCps   = configuration.getFloat("leftMaxCps",  CATEGORY_CLICKER, leftMaxCps,  10.0f, 20.0f, "Left CPS maximum (original default 14.0)");
        rightMinCps  = configuration.getFloat("rightMinCps", CATEGORY_CLICKER, rightMinCps, 7.5f, 18.0f, "Right CPS minimum (original default 13.0)");
        rightMaxCps  = configuration.getFloat("rightMaxCps", CATEGORY_CLICKER, rightMaxCps, 10.0f, 20.0f, "Right CPS maximum (original default 20.0)");
        hitChance    = configuration.getInt("hitChance",   CATEGORY_CLICKER, hitChance,   75, 100, "Hit chance percent (original default 87)");
        rightChance  = configuration.getInt("rightChance", CATEGORY_CLICKER, rightChance, 75, 100, "Right chance percent (original quirk: unused by the engine in legacy mode, original default 80)");

        blockHit         = configuration.getBoolean("blockHit",       CATEGORY_MISC, blockHit,        "Tap right-click right after left clicks (1.8 block-hit)");
        blockHitChance   = configuration.getInt("blockHitChance",     CATEGORY_MISC, blockHitChance,  1, 25, "Block-hit chance percent (original default 6)");
        legacyTiming     = configuration.getBoolean("legacyTiming",       CATEGORY_MISC, legacyTiming,       "Reproduce the original engine's 100/cps delay math -- real rate runs ~2x the label, exactly like the original exe");
        toggleChatFeedback = configuration.getBoolean("toggleChatFeedback", CATEGORY_MISC, toggleChatFeedback, "Chat message when the clicker is toggled");

        noBowRight     = configuration.getBoolean("noBowRight",     CATEGORY_SAFETY, noBowRight,     "No right clicks while holding a bow, so you can draw and aim normally");
        noUseInterrupt = configuration.getBoolean("noUseInterrupt", CATEGORY_SAFETY, noUseInterrupt, "No right clicks while using an item (eating, drinking, drawing)");
        swordOnly      = configuration.getBoolean("swordOnly",      CATEGORY_SAFETY, swordOnly,      "Left clicks only while holding a sword");
        pauseUnfocused = configuration.getBoolean("pauseUnfocused", CATEGORY_SAFETY, pauseUnfocused, "Idle while the game window is unfocused");

        accentR        = configuration.getFloat("accentR", CATEGORY_GUI, accentR, 0.0f, 1.0f, "Accent red (original 1.000)");
        accentG        = configuration.getFloat("accentG", CATEGORY_GUI, accentG, 0.0f, 1.0f, "Accent green (original 0.137)");
        accentB        = configuration.getFloat("accentB", CATEGORY_GUI, accentB, 0.0f, 1.0f, "Accent blue (original 1.000)");
        otherBackground = configuration.getBoolean("otherBackground", CATEGORY_GUI, otherBackground, "GUI background 0C161C instead of 1B1616");
        hudEnabled      = configuration.getBoolean("hudEnabled",      CATEGORY_GUI, hudEnabled,      "Show the in-game status overlay");

        if (configuration.hasChanged()) configuration.save();
    }

    // ------------------------------------------------------------------ save

    /** Push every current field value into the file and save it. */
    public static void save() {
        if (configuration == null) return;
        putBool (CATEGORY_CLICKER, "leftEnabled",  leftEnabled,  "");
        putBool (CATEGORY_CLICKER, "rightEnabled", rightEnabled, "");
        putFloat(CATEGORY_CLICKER, "leftMinCps",   leftMinCps,   "");
        putFloat(CATEGORY_CLICKER, "leftMaxCps",   leftMaxCps,   "");
        putFloat(CATEGORY_CLICKER, "rightMinCps",  rightMinCps,  "");
        putFloat(CATEGORY_CLICKER, "rightMaxCps",  rightMaxCps,  "");
        putInt  (CATEGORY_CLICKER, "hitChance",    hitChance,    "");
        putInt  (CATEGORY_CLICKER, "rightChance",  rightChance,  "");
        putBool (CATEGORY_MISC, "blockHit",          blockHit,          "");
        putInt  (CATEGORY_MISC, "blockHitChance",    blockHitChance,    "");
        putBool (CATEGORY_MISC, "legacyTiming",      legacyTiming,      "");
        putBool (CATEGORY_MISC, "toggleChatFeedback", toggleChatFeedback, "");
        putBool (CATEGORY_SAFETY, "noBowRight",     noBowRight,     "");
        putBool (CATEGORY_SAFETY, "noUseInterrupt", noUseInterrupt, "");
        putBool (CATEGORY_SAFETY, "swordOnly",      swordOnly,      "");
        putBool (CATEGORY_SAFETY, "pauseUnfocused", pauseUnfocused, "");
        putFloat(CATEGORY_GUI, "accentR",         accentR,         "");
        putFloat(CATEGORY_GUI, "accentG",         accentG,         "");
        putFloat(CATEGORY_GUI, "accentB",         accentB,         "");
        putBool (CATEGORY_GUI, "otherBackground", otherBackground, "");
        putBool (CATEGORY_GUI, "hudEnabled",      hudEnabled,      "");
        configuration.save();
    }

    private static void putBool(String cat, String name, boolean v, String c) {
        configuration.get(cat, name, v, c).set(v);
    }

    private static void putInt(String cat, String name, int v, String c) {
        configuration.get(cat, name, v, c).set(v);
    }

    private static void putFloat(String cat, String name, float v, String c) {
        configuration.get(cat, name, (double) v, c).set((double) v);
    }

    // ---------------------------------------------------------------- helpers

    /** Restore the default values. */
    public static void resetDefaults() {
        leftEnabled = false;  rightEnabled = false;
        leftMinCps = 9.6f;    leftMaxCps = 14.0f;
        rightMinCps = 13.0f;  rightMaxCps = 20.0f;
        hitChance = 87;       rightChance = 80;
        blockHit = true;      blockHitChance = 6;
        legacyTiming = true;  toggleChatFeedback = true;
        noBowRight = true;    noUseInterrupt = true;
        swordOnly = false;    pauseUnfocused = true;
        accentR = 1.000f;     accentG = 0.137f;    accentB = 1.000f;
        otherBackground = true; hudEnabled = true;
    }

    /** Packed ARGB of the accent color. */
    public static int accentColor() {
        return 0xFF000000 | (clamp255(accentR) << 16) | (clamp255(accentG) << 8) | clamp255(accentB);
    }

    /** "#RRGGBB" form, for the Settings tab preview. */
    public static String accentHex() {
        return String.format(java.util.Locale.US, "#%02X%02X%02X",
                clamp255(accentR), clamp255(accentG), clamp255(accentB));
    }

    private static int clamp255(float v) {
        int i = (int) (v * 255f + 0.5f);
        return i < 0 ? 0 : (i > 255 ? 255 : i);
    }

    /** Registered on the FML event bus (client side). */
    public static class EventHandler {
        @SubscribeEvent
        public void onConfigChanged(ConfigChangedEvent.OnConfigChangedEvent event) {
            if (Abaser.MODID.equals(event.modID)) reload();
        }
    }
}
