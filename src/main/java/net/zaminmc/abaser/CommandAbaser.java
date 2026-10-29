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
 *   /abaser            open the menu
 *   /abaser on|off     arm/disarm the clicker
 *   /abaser toggle     same as pressing F4
 *   /abaser status     current settings dump
 */
public class CommandAbaser extends CommandBase {

    @Override
    public String getCommandName() {
        return "abaser";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/abaser [on|off|toggle|status]";
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
        } else {
            throw new WrongUsageException(getCommandUsage(sender));
        }

        AbaserConfig.save();
        sender.addChatMessage(new ChatComponentText(
                EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                + EnumChatFormatting.GRAY + "Clicker " + (on ? "ON" : "OFF")));
    }

    private String describe() {
        return String.format(Locale.US,
                "Clicker %s | L %.1f-%.1f cps | R %.1f-%.1f cps | hit %d%% | blockhit %s (%d%%) | legacy %s | sword-only %s | bow-guard %s",
                (AbaserConfig.leftEnabled || AbaserConfig.rightEnabled) ? "ON" : "OFF",
                AbaserConfig.leftMinCps, AbaserConfig.leftMaxCps,
                AbaserConfig.rightMinCps, AbaserConfig.rightMaxCps,
                AbaserConfig.hitChance,
                AbaserConfig.blockHit ? "on" : "off", AbaserConfig.blockHitChance,
                AbaserConfig.legacyTiming ? "on" : "off",
                AbaserConfig.swordOnly ? "on" : "off",
                AbaserConfig.noBowRight ? "on" : "off");
    }

    @Override
    public List<String> addTabCompletionOptions(ICommandSender sender, String[] args, BlockPos pos) {
        if (args.length == 1) {
            List<String> out = new ArrayList<String>();
            for (String s : new String[] {"on", "off", "toggle", "status"}) {
                if (s.startsWith(args[0].toLowerCase(Locale.US))) out.add(s);
            }
            return out;
        }
        return null;
    }
}
