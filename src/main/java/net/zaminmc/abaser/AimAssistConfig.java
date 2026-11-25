package net.zaminmc.abaser;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import net.minecraft.client.Minecraft;

/**
 * The aim assist's own settings. These belong to the MOD, not to the exe:
 * the aim assist steers this player's camera entirely client-side, so its
 * knobs are stored in the game's own config folder
 * ({@code config/abaser_aim.json}) and never travel over the mod link.
 *
 * The field set and defaults mirror the classic MOG-AIMASSIST config that
 * Abaser absorbed: strength, max distance, a FOV cone, mob/player filters,
 * smooth aim, require-click, silent aim and separate horizontal/vertical
 * strength. Everything is clamped on load and on every panel edit so a bad
 * number in the JSON can never throw the camera off.
 */
public final class AimAssistConfig {

    // master switch (also toggled by the G keybind and /abaser aim)
    public boolean enabled = false;

    // targeting
    public float maxDistance    = 4.0F;     // blocks around the player
    public float fov            = 90.0F;    // degrees, full cone
    public boolean onlyMobs     = true;     // hostile mobs pass the filter
    public boolean onlyPlayers  = false;    // players pass the filter

    // aiming
    public float  strength          = 0.5F; // overall pull, 0..1
    public float  horizontalStrength = 1.0F; // extra yaw multiplier, 0..2
    public float  verticalStrength   = 0.8F; // extra pitch multiplier, 0..2
    public boolean smoothAim        = true;
    public float  smoothFactor      = 0.3F; // fraction of the turn per tick, 0.05..1
    public boolean requireClick     = false; // only assist while attacking
    public boolean silent           = false; // move the camera without touching rotation fields

    /** Keep every knob inside the panel's slider range. */
    public void clamp() {
        maxDistance = clampF(maxDistance, 1.0F, 10.0F);
        fov         = clampF(fov, 30.0F, 180.0F);
        strength    = clampF(strength, 0.0F, 1.0F);
        horizontalStrength = clampF(horizontalStrength, 0.0F, 2.0F);
        verticalStrength   = clampF(verticalStrength, 0.0F, 2.0F);
        smoothFactor       = clampF(smoothFactor, 0.05F, 1.0F);
    }

    private static float clampF(float v, float lo, float hi) {
        if (Float.isNaN(v)) return lo;
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // ---- persistence --------------------------------------------------------

    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();

    private static File file() {
        return new File(Minecraft.getMinecraft().mcDataDir, "config/abaser_aim.json");
    }

    /** Load from config/abaser_aim.json (or fall back to the defaults). */
    public static AimAssistConfig load() {
        final File f = file();
        if (!f.isFile()) return new AimAssistConfig();
        try {
            final InputStreamReader r = new InputStreamReader(new FileInputStream(f), StandardCharsets.UTF_8);
            final AimAssistConfig cfg = GSON.fromJson(r, AimAssistConfig.class);
            r.close();
            if (cfg != null) {
                cfg.clamp();
                return cfg;
            }
        } catch (Exception ignored) {
            // a broken file must never stop the game from loading
        }
        return new AimAssistConfig();
    }

    /** Write the current values back to config/abaser_aim.json. */
    public void save() {
        try {
            final File f = file();
            final File dir = f.getParentFile();
            if (dir != null && !dir.isDirectory()) dir.mkdirs();
            final OutputStreamWriter w = new OutputStreamWriter(new FileOutputStream(f), StandardCharsets.UTF_8);
            GSON.toJson(this, w);
            w.close();
        } catch (Exception ignored) {
            // losing one settings write is not worth crashing a client tick
        }
    }
}
