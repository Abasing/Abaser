package net.zaminmc.abaser;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import net.minecraft.client.Minecraft;
import net.minecraft.command.CommandBase;
import net.minecraft.command.CommandException;
import net.minecraft.command.ICommandSender;
import net.minecraft.command.WrongUsageException;
import net.minecraft.util.BlockPos;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

import net.zaminmc.abaser.gui.AbaserGuiScreen;

/**
 * /abaser -- chat control, because sometimes your hands are busy.
 *
 *   /abaser                    open the menu
 *   /abaser on|off             arm/disarm the clicker
 *   /abaser toggle             same as pressing F4
 *   /abaser status             current settings dump
 *   /abaser pos1 / pos2        set a zone corner where you stand
 *   /abaser except <name>      add a name to the never-target list
 *   /abaser unexcept <name>    remove one again
 *
 * Client-side only (registered through ClientCommandHandler): nothing
 * here ever reaches the server.
 */
public class CommandAbaser extends CommandBase {

    @Override
    public String getCommandName() {
        return "abaser";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/abaser [on|off|toggle|status|pos1|pos2|except|unexcept]";
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
    public void processCommand(ICommandSender sender, String[] args) throws CommandException {
        if (args.length == 0) {
            Minecraft.getMinecraft().displayGuiScreen(new AbaserGuiScreen());
            return;
        }

        String sub = args[0].toLowerCase(Locale.US);
        boolean on;

        if ("on".equals(sub)) {
            AbaserConfig.leftEnabled = true;
            AbaserConfig.rightEnabled = true;
            on = true;
        } else if ("off".equals(sub)) {
            AbaserConfig.leftEnabled = false;
            AbaserConfig.rightEnabled = false;
            on = false;
        } else if ("toggle".equals(sub)) {
            AbaserConfig.leftEnabled = !AbaserConfig.leftEnabled;
            AbaserConfig.rightEnabled = !AbaserConfig.rightEnabled;
            on = AbaserConfig.leftEnabled || AbaserConfig.rightEnabled;
        } else if ("status".equals(sub)) {
            sender.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                    + EnumChatFormatting.GRAY + describe()));
            return;
        } else if ("pos1".equals(sub) || "pos2".equals(sub)) {
            Minecraft mc = Minecraft.getMinecraft();
            if (mc.thePlayer == null || mc.theWorld == null) {
                throw new CommandException("Not in a world right now.");
            }
            boolean second = "pos2".equals(sub);
            AbaserZones.Zone z = AbaserZones.captureCorner(mc, second);
            AbaserConfig.save();
            sender.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                    + EnumChatFormatting.GRAY + z.name + " corner " + (second ? "2" : "1")
                    + " set to " + String.format(Locale.US, "%d, %d, %d",
                            second ? z.x2 : z.x1, second ? z.y2 : z.y1, second ? z.z2 : z.z1)
                    + " (dim " + z.dim + ") -- INSERT menu to finish it"));
            return;
        } else if ("except".equals(sub)) {
            if (args.length < 2) {
                String list = AbaserTarget.joined();
                sender.addChatMessage(new ChatComponentText(
                        EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                        + EnumChatFormatting.GRAY + "Except list: "
                        + (list.isEmpty() ? "(empty)" : list)));
                return;
            }
            String name = joinArgs(args);
            AbaserTarget.add(name);
            AbaserConfig.save();
            sender.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                    + EnumChatFormatting.GRAY + "Never targeting: " + name));
            return;
        } else if ("unexcept".equals(sub)) {
            if (args.length < 2) {
                throw new WrongUsageException("/abaser unexcept <name>");
            }
            String name = joinArgs(args);
            boolean removed = AbaserTarget.remove(name);
            AbaserConfig.save();
            sender.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                    + EnumChatFormatting.GRAY + (removed
                    ? "Removed: " + name
                    : "Not on the except list: " + name)));
            return;
        } else {
            throw new WrongUsageException(getCommandUsage(sender));
        }

        AbaserConfig.save();
        sender.addChatMessage(new ChatComponentText(
                EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                + EnumChatFormatting.GRAY + "Clicker " + (on ? "ON" : "OFF")));
    }

    private String describe() {
        Minecraft mc = Minecraft.getMinecraft();
        AbaserZones.Zone hold = AbaserZones.suppressingZone(mc);
        return String.format(Locale.US,
                "Clicker %s | L %.1f-%.1f cps | R %.1f-%.1f cps | hit %d%% | blockhit %s (%d%%) | legacy %s | sword-only %s | bow-hold %s | except %s (%d) | zones %s (%d)%s",
                (AbaserConfig.leftEnabled || AbaserConfig.rightEnabled) ? "ON" : "OFF",
                AbaserConfig.leftMinCps, AbaserConfig.leftMaxCps,
                AbaserConfig.rightMinCps, AbaserConfig.rightMaxCps,
                AbaserConfig.hitChance,
                AbaserConfig.blockHit ? "on" : "off", AbaserConfig.blockHitChance,
                AbaserConfig.legacyTiming ? "on" : "off",
                AbaserConfig.swordOnly ? "on" : "off",
                AbaserConfig.noBowRight ? "on" : "off",
                AbaserConfig.exceptEnabled ? "on" : "off", AbaserTarget.except.size(),
                AbaserConfig.zonesEnabled ? "on" : "off", AbaserZones.zones.size(),
                hold != null ? " | HOLD: " + hold.name + " (" + hold.modeLabel() + ")" : "");
    }

    private static String joinArgs(String[] args) {
        StringBuilder sb = new StringBuilder();
        for (int i = 1; i < args.length; i++) {
            if (sb.length() > 0) sb.append(' ');
            sb.append(args[i]);
        }
        return sb.toString().trim();
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        if (args.length == 1) {
            List<String> out = new ArrayList<String>();
            for (String s : new String[] {"on", "off", "toggle", "status", "pos1", "pos2", "except", "unexcept"}) {
                if (s.startsWith(args[0].toLowerCase(Locale.US))) out.add(s);
            }
            return out;
        }
        return null;
    }
}
