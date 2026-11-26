package net.zaminmc.abaser;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;

import net.minecraft.client.Minecraft;

/**
 * The aim assist's own settings. These belong to the MOD, not to the exe:
 * the aim assist steers this player's camera entirely client-side, so its
 * knobs are stored in the game's own config folder
 * ({@code config/abaser_aim.json}) and never travel over the mod link.
 *
 * Targeting is PLAYERS ONLY (the mob assist was removed) — everyone is a
 * valid target except:
 *
 *   - the parsed peace/team list (the mod reads the server's
 *     "You have peace with N player(s)" chat block, see {@link PeaceList}),
 *   - manual exceptions you add yourself ({@code /abaser except add NAME}),
 *   - and when lock-in mode is on, ONLY the locked names are targeted at
 *     all ({@code /abaser lock on} + {@code /abaser lock add NAME}).
 *
 * Two spawn gates round it off, measured against an exact 3D box
 * (corners below default to the SKYPVP spawn the user measured):
 *
 *   - spawnBlockAim — the camera assist freezes inside the box,
 *   - allyHold      — clicks hold whenever an excepted player is under
 *                     the crosshair (the exe holds both sides when the
 *                     mod reports ally=true; the click-side spawn gate is
 *                     the exe's own spawn_gate setting).
 */
public final class AimAssistConfig {

    // master switch (also toggled by the G keybind and /abaser aim)
    public boolean enabled = false;

    // targeting (players only)
    public float maxDistance    = 4.0F;     // blocks around the player
    public float fov            = 90.0F;    // degrees, full cone
    public float  strength           = 0.5F; // overall pull, 0..1
    public float  horizontalStrength = 1.0F; // extra yaw multiplier, 0..2
    public float  verticalStrength   = 0.8F; // extra pitch multiplier, 0..2
    public boolean smoothAim         = true;
    public float  smoothFactor       = 0.3F; // fraction of the turn per tick, 0.05..1
    public boolean requireClick      = false; // only assist while attacking
    public boolean silent            = false; // move the camera without touching rotation fields

    // spawn box — exact 3D corners (default: the measured SKYPVP spawn)
    public float spawnX1 = -23.52F;
    public float spawnY1 = 57.00F;
    public float spawnZ1 = 23.67F;
    public float spawnX2 = 23.65F;
    public float spawnY2 = 58.00F;
    public float spawnZ2 = -22.35F;
    public boolean spawnBlockAim = true;    // freeze the assist inside the box

    // exceptions + lock-in
    public List<String> manualExceptions = new ArrayList<String>();
    public boolean lockEnabled = false;
    public List<String> lockNames = new ArrayList<String>();

    // crosshair ally hold — the exe holds BOTH click sides when the mod
    // reports ally=true (an excepted player under the crosshair)
    public boolean allyHold = true;

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

    // ---- names: exceptions + lock-in ----------------------------------------

    private static String norm(String s) {
        return s == null ? "" : s.trim().toLowerCase(Locale.ROOT);
    }

    /** Add a name if not present (case-insensitive). True when added.
     *  Stores the trimmed ORIGINAL case — every lookup normalizes anyway,
     *  and the Friends tab reads these lists for display, so storing the
     *  pre-lowered string made it show "mr_criminal" instead of
     *  "Mr_Criminal". */
    private static boolean addName(List<String> list, String name) {
        final String k = norm(name);
        if (k.isEmpty() || k.length() > 16) return false;
        for (String s : list) if (norm(s).equals(k)) return false;
        list.add(name.trim());
        return true;
    }

    /** Remove a name (case-insensitive). True when removed. */
    private static boolean removeName(List<String> list, String name) {
        final String k = norm(name);
        for (java.util.Iterator<String> it = list.iterator(); it.hasNext();) {
            if (norm(it.next()).equals(k)) { it.remove(); return true; }
        }
        return false;
    }

    public boolean hasException(String name) {
        final String k = norm(name);
        for (String s : manualExceptions) if (norm(s).equals(k)) return true;
        return false;
    }

    public boolean addException(String name) { final boolean r = addName(manualExceptions, name); if (r) save(); return r; }
    public boolean removeException(String name) { final boolean r = removeName(manualExceptions, name); if (r) save(); return r; }
    public void clearExceptions() { if (!manualExceptions.isEmpty()) { manualExceptions.clear(); save(); } }

    public boolean hasLock(String name) {
        final String k = norm(name);
        for (String s : lockNames) if (norm(s).equals(k)) return true;
        return false;
    }

    public boolean addLock(String name) { final boolean r = addName(lockNames, name); if (r) save(); return r; }
    public boolean removeLock(String name) { final boolean r = removeName(lockNames, name); if (r) save(); return r; }
    public void clearLock() { if (!lockNames.isEmpty()) { lockNames.clear(); save(); } }

    /** Lock-in is active only when switched on AND at least one name is set. */
    public boolean lockActive() { return lockEnabled && !lockNames.isEmpty(); }

    // ---- spawn box -----------------------------------------------------------

    /** Exact 3D box test (min/max of the two corners per axis). */
    public boolean insideSpawn(double x, double y, double z) {
        final double minX = Math.min(spawnX1, spawnX2), maxX = Math.max(spawnX1, spawnX2);
        final double minY = Math.min(spawnY1, spawnY2), maxY = Math.max(spawnY1, spawnY2);
        final double minZ = Math.min(spawnZ1, spawnZ2), maxZ = Math.max(spawnZ1, spawnZ2);
        return x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ;
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
                if (cfg.manualExceptions == null) cfg.manualExceptions = new ArrayList<String>();
                if (cfg.lockNames == null) cfg.lockNames = new ArrayList<String>();
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
