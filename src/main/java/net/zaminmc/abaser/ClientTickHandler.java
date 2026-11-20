package net.zaminmc.abaser;

import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

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
 * whole job is three small things:
 *
 *   1. start the exe (hidden in the system tray) when a server/world loads,
 *   2. feed it live game state over stdin, one JSON line per change:
 *          {"gui":true,"bow":false,"held":"item.bow"}
 *      "gui" — ANY in-game screen is open (inventory, chest, villager,
 *              enderchest, chat, pause menu, ...) -> the exe blocks all
 *              clicks while that is true;
 *      "bow" — the player is holding a bow -> the exe holds its right-click
 *              output (right autoclicker and Block Hit taps) so charging the
 *              bow behaves normally; left clicks are untouched;
 *   3. stop the exe when the game closes.
 *
 * The exe also exits on its own when stdin hits EOF, so even a hard game
 * crash cannot orphan it.
 */
public class ClientTickHandler {

    private static Process clicker;
    private static BufferedWriter toClicker;

    private static boolean worldJoined;     // saw a world/server load this session
    private static boolean launchFailed;    // complained once about a missing exe
    private static boolean lastGui;
    private static boolean lastBow;
    private static String  lastHeld = "";
    private static boolean haveLast;
    private static int     ticksSinceSend;

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;
        final Minecraft mc = Minecraft.getMinecraft();
        final boolean inWorld = mc.theWorld != null && mc.thePlayer != null;

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
        }

        final boolean changed = !haveLast || gui != lastGui || bow != lastBow || !held.equals(lastHeld);
        if (changed || ++ticksSinceSend >= 40) {   // send on change + a 2 s heartbeat
            sendState(gui, bow, held);
            lastGui = gui;
            lastBow = bow;
            lastHeld = held;
            haveLast = true;
            ticksSinceSend = 0;
        }
    }

    /** Start AbaserClicker.exe in managed tray mode. Never blocks the client tick. */
    private static void tryLaunch(final Minecraft mc) {
        if (clicker != null) return;
        final File exe = findExe(mc);
        if (exe == null) {
            if (!launchFailed) {
                launchFailed = true;
                chat("AbaserClicker.exe not found - drop it in the game folder or the mods folder.");
            }
            return;
        }
        // launch off-thread: ProcessBuilder.start can hit disk / AV scanning
        new Thread(new Runnable() {
            public void run() {
                try {
                    Process p = new ProcessBuilder(exe.getAbsolutePath(), "--managed", "/tray").start();
                    clicker = p;
                    toClicker = new BufferedWriter(new OutputStreamWriter(p.getOutputStream(), StandardCharsets.US_ASCII));
                    haveLast = false;
                    chat("Abaser Clicker is running in your system tray - it clicks only inside Minecraft.");
                } catch (IOException e) {
                    Abaser.logger.warn("Abaser: could not start AbaserClicker.exe", e);
                    chat("Found AbaserClicker.exe but could not start it (see log).");
                }
            }
        }, "Abaser-Launch").start();
    }

    /**
     * Where the exe may live. The Run-key autostart copy and a manually
     * started standalone instance are both fine too — the exe's own
     * single-instance guard keeps the two from stacking.
     */
    private static File findExe(Minecraft mc) {
        String env = System.getenv("ABASER_EXE");
        if (env != null && env.trim().length() > 0) {
            File f = new File(env.trim());
            if (f.isFile()) return f;
        }
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

    private static void sendState(boolean gui, boolean bow, String held) {
        StringBuilder sb = new StringBuilder(96);
        sb.append("{\"gui\":").append(gui).append(",\"bow\":").append(bow);
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
    }

    private static void chat(String msg) {
        try {
            Minecraft.getMinecraft().thePlayer.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] " + EnumChatFormatting.GRAY + msg));
        } catch (Exception ignored) {}                 // world may be gone already
    }
}
