package net.zaminmc.abaser;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import net.minecraft.client.Minecraft;
import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.BlockPos;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

/**
 * /abaser — how the pieces are wired, the in-game panel, and the aim
 * assist's exceptions.
 *
 * The mod never clicks: the auto-clicker is the standalone Abaser Clicker
 * executable. The mod starts it when a server loads, feeds it live game
 * state (open GUIs, held item, spawn box, ally under crosshair), lets you
 * configure it from the in-game panel, and closes it when the game closes.
 * The aim assist is the mod's own client-side camera nudge.
 *
 * Commands still exist for keyboard-lovers, but EVERYTHING they do is also
 * in the panel's tabs: Clicker (the exe), Aim (the assist), Friends
 * (exceptions + lock-in + peace list) and Spawn (the box + gates).
 *
 * Subcommands:
 *   /abaser panel                 open the in-game panel
 *   /abaser aim [on|off]          toggle/force the aim assist
 *   /abaser except add|remove NAME   manual exceptions (never targeted)
 *   /abaser except list|clear     view/clear the manual exceptions
 *   /abaser lock on|off|add|remove NAME / list|clear   lock-in mode
 *   /abaser spawn here [halfwidth]  set the spawn box around you
 *   /abaser spawn box X1 Y1 Z1 X2 Y2 Z2   exact corners
 *   /abaser spawn show            print the current box
 *   /abaser peace                 show the parsed peace/team list
 */
public class CommandAbaser extends CommandBase {

    @Override
    public String getCommandName() {
        return "abaser";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/abaser [panel|aim|except|lock|spawn|peace]";
    }

    @Override
    public int getRequiredPermissionLevel() {
        return 0;
    }

    @Override
    public boolean canCommandSenderUseCommand(ICommandSender sender) {
        return true;
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        if (args.length == 1) {
            return getListOfStringsMatchingLastWord(args,
                    "panel", "aim", "except", "lock", "spawn", "peace");
        }
        if (args.length == 2) {
            if (args[0].equalsIgnoreCase("except") || args[0].equalsIgnoreCase("lock")) {
                final List<String> opts = new ArrayList<String>();
                opts.add("add"); opts.add("remove"); opts.add("list"); opts.add("clear");
                if (args[0].equalsIgnoreCase("lock")) { opts.add(0, "off"); opts.add(0, "on"); }
                return getListOfStringsMatchingLastWord(args, opts);
            }
            if (args[0].equalsIgnoreCase("spawn")) {
                return getListOfStringsMatchingLastWord(args, "here", "box", "show");
            }
            if (args[0].equalsIgnoreCase("aim")) {
                return getListOfStringsMatchingLastWord(args, "on", "off");
            }
        }
        // third arg: a name — suggest the players currently in the world
        if (args.length == 3) {
            final String sub = args[0].toLowerCase(Locale.ROOT);
            final String act = args[1].toLowerCase(Locale.ROOT);
            if ((sub.equals("except") || sub.equals("lock"))
                    && (act.equals("add") || act.equals("remove"))) {
                final List<String> names = new ArrayList<String>();
                try {
                    for (Object o : Minecraft.getMinecraft().theWorld.playerEntities) {
                        if (o instanceof EntityPlayer) {
                            names.add(((EntityPlayer) o).getName());
                        }
                    }
                } catch (Exception ignored) {}
                return getListOfStringsMatchingLastWord(args, names);
            }
        }
        return null;
    }

    @Override
    public void processCommand(ICommandSender sender, String[] args) {
        if (args.length > 0) {
            final String a = args[0].toLowerCase(Locale.ROOT);
            if (a.equals("panel") || a.equals("gui") || a.equals("open") || a.equals("config")) {
                // client command — but NOT displayGuiScreen directly: 1.8.8's
                // GuiChat nulls the screen right after sendChatMessage
                // returns, which used to close the panel the same frame.
                // Queue it; the next client tick opens it for real.
                ClientTickHandler.panelOpenRequested = true;
                return;
            }
            if (a.equals("aim") || a.equals("aimassist")) {
                // /abaser aim [on|off] — flips the mod's own camera assist
                if (args.length > 1) {
                    final String s = args[1].toLowerCase(Locale.ROOT);
                    if (s.equals("on") || s.equals("true") || s.equals("1"))
                        AimAssist.setEnabled(true, true);
                    else if (s.equals("off") || s.equals("false") || s.equals("0"))
                        AimAssist.setEnabled(false, true);
                    else
                        AimAssist.toggle();
                } else {
                    AimAssist.toggle();
                }
                return;
            }
            if (a.equals("except") || a.equals("exception") || a.equals("exceptions")) {
                handleExcept(sender, args);
                return;
            }
            if (a.equals("lock")) {
                handleLock(sender, args);
                return;
            }
            if (a.equals("spawn")) {
                handleSpawn(sender, args);
                return;
            }
            if (a.equals("peace") || a.equals("team")) {
                handlePeace(sender);
                return;
            }
        }
        send(sender, EnumChatFormatting.LIGHT_PURPLE + "Abaser "
                + EnumChatFormatting.GRAY + Abaser.VERSION);
        send(sender, EnumChatFormatting.GRAY + "The auto-clicker is the standalone "
                + EnumChatFormatting.WHITE + "Abaser Clicker"
                + EnumChatFormatting.GRAY + " exe - this mod starts it in your"
                + " system tray when a world loads and closes it when the game closes.");
        send(sender, EnumChatFormatting.GRAY + "Everything below is also in the panel's tabs"
                + " (Clicker / Aim / Friends / Spawn):" + EnumChatFormatting.WHITE + " Right Shift"
                + EnumChatFormatting.GRAY + " or " + EnumChatFormatting.WHITE + "/abaser panel"
                + EnumChatFormatting.GRAY + ".");
        send(sender, EnumChatFormatting.GRAY + "The aim assist is built into this mod (client-side):"
                + " press " + EnumChatFormatting.WHITE + "G" + EnumChatFormatting.GRAY
                + " or run " + EnumChatFormatting.WHITE + "/abaser aim"
                + EnumChatFormatting.GRAY + "; tune it in the panel's Aim tab.");
        send(sender, EnumChatFormatting.GRAY + "Exceptions (never targeted): add them in the"
                + EnumChatFormatting.WHITE + " Friends tab" + EnumChatFormatting.GRAY + " or with "
                + EnumChatFormatting.WHITE + "/abaser except add NAME" + EnumChatFormatting.GRAY + ";"
                + " the peace list is parsed from the server's chat automatically");
        send(sender, EnumChatFormatting.GRAY + "lock-in (only these targets): "
                + EnumChatFormatting.WHITE + "/abaser lock on" + EnumChatFormatting.GRAY + " + "
                + EnumChatFormatting.WHITE + "/abaser lock add NAME" + EnumChatFormatting.GRAY
                + " - or the Friends tab.");
        send(sender, EnumChatFormatting.GRAY + "Spawn gates: "
                + EnumChatFormatting.WHITE + "/abaser spawn here"
                + EnumChatFormatting.GRAY + " (or /abaser spawn box X1 Y1 Z1 X2 Y2 Z2)"
                + " - or set the box in the Spawn tab. No clicks and no aim inside it.");
        send(sender, EnumChatFormatting.GRAY + "The exe clicks only inside Minecraft:"
                + " nothing is clicked while any menu is open, while you hold a bow,"
                + " inside the spawn box or with an ally under the crosshair.");
    }

    // ---- /abaser except -------------------------------------------------------

    private void handleExcept(ICommandSender sender, String[] args) {
        final AimAssistConfig cfg = AimAssist.cfg;
        if (args.length >= 3 && args[1].equalsIgnoreCase("add")) {
            final boolean ok = cfg.addException(args[2]);
            msg(sender, ok ? "exception added: " + args[2]
                           : args[2] + " is already excepted");
            return;
        }
        if (args.length >= 3 && args[1].equalsIgnoreCase("remove")) {
            final boolean ok = cfg.removeException(args[2]);
            msg(sender, ok ? "exception removed: " + args[2]
                           : args[2] + " was not on the list");
            return;
        }
        if (args.length >= 2 && args[1].equalsIgnoreCase("clear")) {
            cfg.clearExceptions();
            msg(sender, "manual exceptions cleared");
            return;
        }
        if (args.length >= 2 && args[1].equalsIgnoreCase("list")
                || (args.length == 1 && sender != null)) {
            final List<String> l = cfg.manualExceptions;
            if (l.isEmpty()) { msg(sender, "no manual exceptions - everyone but the peace list is fair game"); return; }
            msg(sender, "manual exceptions (" + l.size() + "): " + joinNames(l));
            return;
        }
        msg(sender, "usage: /abaser except add|remove NAME - list|clear");
    }

    // ---- /abaser lock ---------------------------------------------------------

    private void handleLock(ICommandSender sender, String[] args) {
        final AimAssistConfig cfg = AimAssist.cfg;
        if (args.length >= 2) {
            final String s = args[1].toLowerCase(Locale.ROOT);
            if (s.equals("on")) {
                cfg.lockEnabled = true; cfg.save();
                msg(sender, "lock-in ON - only the locked names are targeted ("
                        + cfg.lockNames.size() + " set)");
                return;
            }
            if (s.equals("off")) {
                cfg.lockEnabled = false; cfg.save();
                msg(sender, "lock-in OFF");
                return;
            }
            if (s.equals("clear")) {
                cfg.clearLock();
                msg(sender, "lock list cleared");
                return;
            }
            if (args.length >= 3 && s.equals("add")) {
                final boolean ok = cfg.addLock(args[2]);
                msg(sender, ok ? "locked: " + args[2] + " (" + cfg.lockNames.size() + " in list)"
                               : args[2] + " is already locked");
                return;
            }
            if (args.length >= 3 && s.equals("remove")) {
                final boolean ok = cfg.removeLock(args[2]);
                msg(sender, ok ? "unlocked: " + args[2] : args[2] + " was not locked");
                return;
            }
            if (s.equals("list")) {
                if (cfg.lockNames.isEmpty()) { msg(sender, "lock list is empty"); return; }
                msg(sender, "locked targets (" + cfg.lockNames.size() + "): " + joinNames(cfg.lockNames)
                        + (cfg.lockEnabled ? "" : " - lock is OFF"));
                return;
            }
        }
        msg(sender, "usage: /abaser lock on|off - add|remove NAME - list|clear");
    }

    // ---- /abaser spawn --------------------------------------------------------

    private void handleSpawn(ICommandSender sender, String[] args) {
        final AimAssistConfig cfg = AimAssist.cfg;
        final Minecraft mc = Minecraft.getMinecraft();
        if (args.length >= 2 && args[1].equalsIgnoreCase("here")) {
            // box around the player: halfwidth blocks in X/Z, feet..feet+2 in Y
            float hw = 24.0F;
            if (args.length >= 3) {
                try { hw = Math.max(2.0F, Float.parseFloat(args[2])); }
                catch (NumberFormatException e) { msg(sender, "bad halfwidth: " + args[2]); return; }
            }
            final double x = mc.thePlayer.posX, y = mc.thePlayer.posY, z = mc.thePlayer.posZ;
            cfg.spawnX1 = (float) (x - hw); cfg.spawnY1 = (float) y; cfg.spawnZ1 = (float) (z - hw);
            cfg.spawnX2 = (float) (x + hw); cfg.spawnY2 = (float) (y + 2); cfg.spawnZ2 = (float) (z + hw);
            cfg.save();
            msg(sender, String.format(Locale.ROOT,
                    "spawn box set around you (halfwidth %.0f): X %.1f..%.1f  Y %.1f..%.1f  Z %.1f..%.1f",
                    hw, cfg.spawnX1, cfg.spawnX2, cfg.spawnY1, cfg.spawnY2, cfg.spawnZ1, cfg.spawnZ2));
            return;
        }
        if (args.length >= 8 && args[1].equalsIgnoreCase("box")) {
            try {
                cfg.spawnX1 = Float.parseFloat(args[2]);
                cfg.spawnY1 = Float.parseFloat(args[3]);
                cfg.spawnZ1 = Float.parseFloat(args[4]);
                cfg.spawnX2 = Float.parseFloat(args[5]);
                cfg.spawnY2 = Float.parseFloat(args[6]);
                cfg.spawnZ2 = Float.parseFloat(args[7]);
            } catch (NumberFormatException e) {
                msg(sender, "usage: /abaser spawn box X1 Y1 Z1 X2 Y2 Z2");
                return;
            }
            cfg.save();
            msg(sender, "spawn box corners set - gates use the exact 3D box");
            return;
        }
        if (args.length >= 2 && args[1].equalsIgnoreCase("show")) {
            showSpawn(sender, cfg);
            return;
        }
        msg(sender, "usage: /abaser spawn here [halfwidth] - box X1 Y1 Z1 X2 Y2 Z2 - show");
    }

    private void showSpawn(ICommandSender sender, AimAssistConfig cfg) {
        msg(sender, String.format(Locale.ROOT,
                "spawn box: (%.2f %.2f %.2f) .. (%.2f %.2f %.2f) - aim gate %s, click gate %s, you are %s",
                cfg.spawnX1, cfg.spawnY1, cfg.spawnZ1, cfg.spawnX2, cfg.spawnY2, cfg.spawnZ2,
                cfg.spawnBlockAim ? "ON" : "off",
                "(exe spawn_gate)",
                cfg.insideSpawn(Minecraft.getMinecraft().thePlayer.posX,
                                Minecraft.getMinecraft().thePlayer.posY,
                                Minecraft.getMinecraft().thePlayer.posZ) ? "INSIDE" : "outside"));
    }

    // ---- /abaser peace --------------------------------------------------------

    private void handlePeace(ICommandSender sender) {
        final Set<String> names = PeaceList.snapshot();
        if (names.isEmpty()) {
            msg(sender, "peace list is empty - it fills when the server prints"
                    + " \"You have peace with N player(s)\"");
            return;
        }
        msg(sender, "peace list (" + names.size() + "): " + joinNames(new ArrayList<String>(names)));
    }

    // ---- helpers ---------------------------------------------------------------

    private static String joinNames(List<String> l) {
        final StringBuilder sb = new StringBuilder(128);
        for (int i = 0; i < l.size(); i++) {
            if (i > 0) sb.append(", ");
            sb.append(l.get(i));
        }
        return sb.toString();
    }

    private void msg(ICommandSender sender, String text) {
        send(sender, EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                + EnumChatFormatting.GRAY + text);
    }

    private static void send(ICommandSender sender, String text) {
        sender.addChatMessage(new ChatComponentText(text));
    }
}
