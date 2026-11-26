package net.zaminmc.abaser;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import net.minecraft.client.Minecraft;
import net.minecraft.init.Items;
import net.minecraft.item.Item;
import net.minecraft.item.ItemStack;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.TickEvent;

/**
 * The bridge between the game and the standalone Abaser Clicker executable.
 *
 * This class NEVER clicks anything and NEVER touches the mouse. Clicks come
 * from the exe itself, exactly as they always have (PostMessage against the
 * LWJGL window, driven by the exe's own hotkeys and CPS engine). The mod's
 * whole job is five small things:
 *
 *   1. install and start the exe (hidden in the system tray) when a
 *      server/world loads — release jars carry the exe INSIDE, so there is
 *      nothing to download or run by hand (see resolveExe/bundleExe);
 *   2. feed it live game state over stdin, one JSON line per change:
 *          {"gui":true,"bow":false,"held":"item.bow","spawn":false,"ally":false}
 *      "gui"   — ANY in-game screen is open (inventory, chest, villager,
 *                enderchest, chat, pause menu, ...) -> the exe blocks all
 *                clicks while that is true;
 *      "spawn" — the player stands inside the configured spawn box -> the
 *                exe blocks all clicks while its spawn_gate setting is on;
 *      "ally"  — a team member (peace list or manual exception) is under
 *                the crosshair -> the exe blocks all clicks so you never
 *                hit friends while spamming;
 *      "bow"   — the player is holding a bow -> the exe holds its right-click
 *                output (right autoclicker and Block Hit taps) so charging the
 *                bow behaves normally; left clicks are untouched;
 *   3. read what the exe answers on stdout (its settings snapshot and a 2 s
 *      live status) so the in-game panel can show and edit the SAME
 *      settings the exe edits — see AbaserConfigGui;
 *   4. forward the panel's {"config":{...}} lines to the exe. The panel is
 *      a cockpit, not the engine: it only turns the same knobs the exe's
 *      own UI turns, and the exe still produces every click itself;
 *   5. stop the exe when the game closes.
 *
 * The exe also exits on its own when stdin hits EOF, so even a hard game
 * crash cannot orphan it.
 */
public class ClientTickHandler {

    private static Process clicker;
    private static BufferedWriter toClicker;

    /** The clicker exe shipped INSIDE this jar (release builds; dev jars omit it). */
    private static final String BUNDLED_EXE_RESOURCE = "/abaser/AbaserClicker.exe";
    /** Set when this launch actually installed or updated the managed copy. */
    private static boolean exeInstalledHere;

    private static boolean worldJoined;     // saw a world/server load this session
    private static boolean launchFailed;    // complained once about a missing exe
    private static boolean lastGui;
    private static boolean lastBow;
    private static boolean lastSpawn;
    private static boolean lastAlly;
    private static String  lastHeld = "";
    private static boolean haveLast;
    private static int     ticksSinceSend;

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;
        final Minecraft mc = Minecraft.getMinecraft();
        final boolean inWorld = mc.theWorld != null && mc.thePlayer != null;

        // ---- 0. the panel keybind (Right Shift by default) --------------------
        if (inWorld && ClientProxy.panelKey != null && ClientProxy.panelKey.isPressed()
                && mc.currentScreen == null) {
            mc.displayGuiScreen(new AbaserConfigGui());
            return;
        }

        // ---- 0b. the aim assist toggle (G by default, rebindable) -------------
        // G flips the mod's own camera-assist engine; like everything else in
        // this class it never presses a button — it only flips a boolean that
        // AimAssist's tick reads.
        if (inWorld && ClientProxy.aimKey != null && ClientProxy.aimKey.isPressed()
                && mc.currentScreen == null) {
            AimAssist.toggle();
        }

        // ---- 1. the server just loaded: bring the clicker up in the tray ----
        if (inWorld && !worldJoined) {
            worldJoined = true;
            tryLaunch(mc);
        } else if (!inWorld && worldJoined) {
            worldJoined = false;
        }

        if (clicker == null) return;

        // ---- 2. feed state: any open GUI + what the player is holding ----
        boolean gui;
        boolean bow = false;
        boolean spawn = false;
        boolean ally = false;
        String held = "";
        if (!inWorld) {
            gui = true;                       // menus / loading screens: suppress
        } else {
            gui = mc.currentScreen != null;   // inventory, chest, villager, enderchest, chat, pause, ...
            ItemStack stack = mc.thePlayer.getHeldItem();
            if (stack != null) {
                Item item = stack.getItem();
                bow = (item == Items.bow);
                held = item.getUnlocalizedName();
            }
            // measured by the aim assist's tick (one tick of latency is fine):
            // spawn — the player stands inside the exact 3D spawn box;
            // ally  — a peace/manual-exception player is under the crosshair.
            // The exe holds EVERY click while either is true (spawn only while
            // its spawn_gate setting is on). STATE ONLY — never a click.
            spawn = AimAssist.inSpawn;
            ally  = AimAssist.allyCross;
        }

        final boolean changed = !haveLast || gui != lastGui || bow != lastBow
                || spawn != lastSpawn || ally != lastAlly || !held.equals(lastHeld);
        if (changed || ++ticksSinceSend >= 40) {   // send on change + a 2 s heartbeat
            sendState(gui, bow, held, spawn, ally);
            lastGui = gui;
            lastBow = bow;
            lastHeld = held;
            lastSpawn = spawn;
            lastAlly = ally;
            haveLast = true;
            ticksSinceSend = 0;
        }
    }

    /**
     * Start AbaserClicker.exe in managed tray mode. Never blocks the client
     * tick: resolving the exe (first-run extraction) and launching it touch
     * the disk and the process table, so all of it runs off-thread and only
     * comes back through chat + the link threads.
     */
    private static void tryLaunch(final Minecraft mc) {
        if (clicker != null) return;
        new Thread(new Runnable() {
            public void run() {
                exeInstalledHere = false;
                final File exe = resolveExe(mc);
                if (exe == null) {
                    if (!launchFailed) {
                        launchFailed = true;
                        chat("AbaserClicker.exe not found - drop it in the game folder or the mods folder.");
                    }
                    return;
                }
                // retire any stray instance FIRST: the child we start must win
                // the single-instance mutex (see killStrayClickers)
                killStrayClickers();
                try {
                    Process p = new ProcessBuilder(exe.getAbsolutePath(), "--managed", "/tray")
                            .start();
                    clicker = p;
                    toClicker = new BufferedWriter(new OutputStreamWriter(p.getOutputStream(), StandardCharsets.US_ASCII));
                    haveLast = false;
                    LinkState.reset();
                    startStdoutReader(p);
                    chat(exeInstalledHere
                            ? "Bundled clicker installed to .minecraft/Abaser - Abaser Clicker is running in your system tray."
                            : "Abaser Clicker is running in your system tray - it clicks only inside Minecraft.");
                    // the exe dumps its current settings on the first line it
                    // sees; asking explicitly too keeps the panel in sync even
                    // if a heartbeat raced the handshake
                    sendConfigLine("{\"get\":\"config\"}");
                } catch (IOException e) {
                    Abaser.logger.warn("Abaser: could not start AbaserClicker.exe", e);
                    chat("Found AbaserClicker.exe but could not start it (see log).");
                }
            }
        }, "Abaser-Launch").start();
    }

    // ------------------------------------------------------------------
    //  where the exe comes from, in priority order:
    //
    //    1. the ABASER_EXE environment variable — explicit developer
    //       override, used verbatim;
    //    2. the copy THIS MOD manages under <game dir>/Abaser/, extracted
    //       from the exe bundled inside this jar. This is the zero-install
    //       path: the mod carries its own clicker, installs it on first use
    //       and replaces it with the jar's current build on every update;
    //    3. legacy manual installs (game folder, mods folder, next to the
    //       jar) — for dev builds whose jar carries no exe.
    // ------------------------------------------------------------------

    /** Pick the exe to run. Runs OFF the client tick (may extract files). */
    private static File resolveExe(final Minecraft mc) {
        String env = System.getenv("ABASER_EXE");
        if (env != null && env.trim().length() > 0) {
            File f = new File(env.trim());
            if (f.isFile()) return f;
        }
        File managed = bundleExe(mc);
        if (managed != null) return managed;
        return findInstalledExe(mc);
    }

    /**
     * The bundled clicker. Extracts {@link #BUNDLED_EXE_RESOURCE} to
     * <game dir>/Abaser/AbaserClicker.exe unless a byte-identical copy is
     * already there. The SHA-1 check is what makes updates work: a newer
     * jar simply overwrites the older exe in place, and abaser.ini (which
     * lives next to it) survives untouched. Returns null when this jar
     * carries no exe, or the disk refuses the write.
     */
    private static File bundleExe(final Minecraft mc) {
        final byte[] bundled = readBundledExe();
        if (bundled == null) return null;                    // dev jar: no exe inside
        final File dir = new File(mc.mcDataDir, "Abaser");
        final File exe = new File(dir, "AbaserClicker.exe");
        final String want = sha1(bundled);
        final String have = fileSha1(exe);
        if (have != null && have.equals(want)) return exe;   // installed and already current
        exeInstalledHere = true;
        if (exe.exists()) killStrayClickers();               // a live instance locks the file on Windows
        if (!dir.isDirectory() && !dir.mkdirs()) return null;
        final File tmp = new File(dir, "AbaserClicker.exe.new");
        try {
            final FileOutputStream out = new FileOutputStream(tmp);
            try { out.write(bundled); } finally { out.close(); }
        } catch (IOException e) {
            Abaser.logger.warn("Abaser: could not write the bundled clicker", e);
            return null;
        }
        if (!tmp.renameTo(exe)) {                            // still locked? one more nudge
            killStrayClickers();
            try { Thread.sleep(500); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            if (exe.exists() && !exe.delete()) return null;
            if (!tmp.renameTo(exe)) return null;
        }
        return exe;
    }

    private static byte[] readBundledExe() {
        final InputStream in = ClientTickHandler.class.getResourceAsStream(BUNDLED_EXE_RESOURCE);
        if (in == null) return null;
        try {
            final ByteArrayOutputStream out = new ByteArrayOutputStream(1 << 20);
            final byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return out.toByteArray();
        } catch (IOException e) {
            return null;
        } finally {
            try { in.close(); } catch (IOException ignored) {}
        }
    }

    private static String sha1(final byte[] data) {
        try { return hex(MessageDigest.getInstance("SHA-1").digest(data)); }
        catch (Exception e) { return ""; }
    }

    /** SHA-1 of a file on disk, or null when it cannot be read (missing/locked). */
    private static String fileSha1(final File f) {
        final InputStream in;
        try { in = new FileInputStream(f); } catch (IOException e) { return null; }
        try {
            final MessageDigest md = MessageDigest.getInstance("SHA-1");
            final byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) md.update(buf, 0, n);
            return hex(md.digest());
        } catch (Exception e) {
            return null;
        } finally {
            try { in.close(); } catch (IOException ignored) {}
        }
    }

    private static String hex(final byte[] d) {
        final StringBuilder sb = new StringBuilder(d.length * 2);
        for (final byte b : d) {
            sb.append(Character.forDigit((b >> 4) & 0xF, 16));
            sb.append(Character.forDigit(b & 0xF, 16));
        }
        return sb.toString();
    }

    /**
     * Retire any AbaserClicker.exe that is already running — an orphan of a
     * crashed session, a leftover from an older build, or an instance the
     * user started by hand. The managed child must OWN the single-instance
     * mutex, so every click flows through a process that actually receives
     * the mod's safety state (GUI, spawn, ally, bow); a stray without that
     * state would click ungated. Windows only: anywhere else, and on any
     * failure, this is a harmless no-op.
     */
    private static void killStrayClickers() {
        try {
            final Process p = new ProcessBuilder("taskkill", "/F", "/T", "/IM", "AbaserClicker.exe")
                    .redirectErrorStream(true).start();
            if (!p.waitFor(5, TimeUnit.SECONDS)) p.destroy();
        } catch (Exception ignored) {}   // no taskkill (not Windows) or nothing to kill
    }

    /**
     * The exe answers on stdout: {"config":{...}} snapshots and 2 s status
     * lines. This thread mirrors them into LinkState for the in-game panel.
     * Without it the pipe would fill and block the exe, so it always runs.
     */
    private static void startStdoutReader(final Process p) {
        final Thread reader = new Thread(new Runnable() {
            public void run() {
                try {
                    final BufferedReader r = new BufferedReader(
                            new InputStreamReader(p.getInputStream(), StandardCharsets.US_ASCII));
                    String line;
                    while ((line = r.readLine()) != null)
                        handleExeLine(line);
                } catch (IOException ignored) {
                    // the exe died — the tick handler notices via the writer
                } finally {
                    LinkState.linkUp = false;
                }
            }
        }, "Abaser-LinkRead");
        reader.setDaemon(true);
        reader.start();
    }

    private static void handleExeLine(String line) {
        LinkState.linkUp = true;
        try {
            final JsonObject o = new JsonParser().parse(line).getAsJsonObject();
            if (o.has("config") && o.get("config").isJsonObject()) {
                final JsonObject cfg = o.getAsJsonObject("config");
                final Map<String, String> m = new HashMap<String, String>();
                for (Map.Entry<String, JsonElement> e : cfg.entrySet())
                    if (e.getValue().isJsonPrimitive())
                        m.put(e.getKey(), e.getValue().getAsString());
                LinkState.applyConfigLine(m);
            }
            if (o.has("status") && o.get("status").isJsonObject()) {
                final JsonObject s = o.getAsJsonObject("status");
                LinkState.leftOn     = flag(s, "left_on");
                LinkState.rightOn    = flag(s, "right_on");
                LinkState.mcFound    = flag(s, "mc");
                LinkState.guiOpen    = flag(s, "gui");
                LinkState.holdingBow = flag(s, "bow");
                LinkState.cpsLeft    = num(s, "cps_l");
                LinkState.cpsRight   = num(s, "cps_r");
            }
        } catch (Exception ignored) {
            // tolerate partial/garbage lines — never kill the reader
        }
    }

    private static boolean flag(JsonObject o, String k) {
        return o.has(k) && o.get(k).isJsonPrimitive() && o.get(k).getAsBoolean();
    }

    private static float num(JsonObject o, String k) {
        try { return o.has(k) && o.get(k).isJsonPrimitive() ? o.get(k).getAsFloat() : 0.0f; }
        catch (Exception e) { return 0.0f; }
    }

    /** Is the panel allowed to edit settings right now? */
    public static boolean isLinkUp() {
        return clicker != null && LinkState.linkUp;
    }

    /**
     * One settings line from the in-game panel to the exe. THIS IS NOT A
     * CLICK and never becomes one — it is numbers for the exe's own knobs;
     * the exe applies, clamps and persists them exactly like its own UI.
     */
    public static void sendConfigLine(String json) {
        try {
            synchronized (ClientTickHandler.class) {
                if (toClicker == null) return;
                toClicker.write(json);
                toClicker.write("\n");
                toClicker.flush();
            }
            LinkState.linkUp = true;
        } catch (IOException e) {
            Abaser.logger.info("Abaser: the clicker exe is gone (config send failed)");
            stopClicker();
        }
    }

    /**
     * Legacy fallback: a manually installed copy (dev builds whose jar
     * carries no bundled exe, or a user's own arrangement). Checked AFTER
     * the bundled copy — {@link #resolveExe} owns the priority order.
     */
    private static File findInstalledExe(Minecraft mc) {
        String[] names = { "AbaserClicker.exe", "abaserclicker.exe" };
        File base = mc.mcDataDir;
        String[] dirs = { "", "mods", "mods/Abaser", "mods/abaser", "Abaser" };
        for (String d : dirs) {
            for (String n : names) {
                File f = new File(base, (d.length() > 0 ? d + "/" : "") + n);
                if (f.isFile()) return f;
            }
        }
        // next to (or above) the mod jar itself
        try {
            File jar = new File(ClientTickHandler.class.getProtectionDomain()
                    .getCodeSource().getLocation().toURI());
            File dir = jar.getParentFile();
            for (int i = 0; i < 3 && dir != null; i++) {
                for (String n : names) {
                    File f = new File(dir, n);
                    if (f.isFile()) return f;
                }
                dir = dir.getParentFile();
            }
        } catch (Exception ignored) {}
        return null;
    }

    private static void sendState(boolean gui, boolean bow, String held, boolean spawn, boolean ally) {
        StringBuilder sb = new StringBuilder(112);
        sb.append("{\"gui\":").append(gui).append(",\"bow\":").append(bow);
        sb.append(",\"spawn\":").append(spawn).append(",\"ally\":").append(ally);
        if (held != null && held.length() > 0) sb.append(",\"held\":\"").append(held).append('"');
        sb.append("}\n");
        try {
            synchronized (ClientTickHandler.class) {
                if (toClicker == null) return;
                toClicker.write(sb.toString());
                toClicker.flush();
            }
        } catch (IOException e) {
            // the exe died (user exited it from the tray, or it crashed):
            // stop feeding and allow a fresh launch on the next world load
            Abaser.logger.info("Abaser: the clicker exe is gone - will try again next world load");
            stopClicker();
        }
    }

    /** The game is closing: say goodbye, then make sure the process is gone. */
    public static void shutdownClicker() {
        try {
            synchronized (ClientTickHandler.class) {
                if (toClicker != null) {
                    toClicker.write("{\"gui\":true,\"bow\":false,\"exit\":true}\n");
                    toClicker.flush();
                }
            }
            Thread.sleep(250);                          // give the exe a moment to exit cleanly
        } catch (Exception ignored) {}
        stopClicker();
    }

    private static void stopClicker() {
        synchronized (ClientTickHandler.class) {
            if (clicker != null) {
                clicker.destroy();
                clicker = null;
            }
            if (toClicker != null) {
                try { toClicker.close(); } catch (IOException ignored) {}
                toClicker = null;
            }
        }
        haveLast = false;
        LinkState.reset();
    }

    private static void chat(String msg) {
        try {
            Minecraft.getMinecraft().thePlayer.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] " + EnumChatFormatting.GRAY + msg));
        } catch (Exception ignored) {}                 // world may be gone already
    }
}
