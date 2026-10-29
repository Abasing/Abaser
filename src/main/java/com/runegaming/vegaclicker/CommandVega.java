package com.runegaming.vegaclicker;

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

import com.runegaming.vegaclicker.gui.VegaGuiScreen;

/**
 * /vega -- chat control, because sometimes your hands are busy.
 *
 *   /vega            open the menu
 *   /vega on|off     arm/disarm the clicker
 *   /vega toggle     same as pressing F4
 *   /vega status     current settings dump
 */
public class CommandVega extends CommandBase {

    @Override
    public String getCommandName() {
        return "vega";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/vega [on|off|toggle|status]";
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
            Minecraft.getMinecraft().displayGuiScreen(new VegaGuiScreen());
            return;
        }

        String sub = args[0].toLowerCase(Locale.US);
        boolean on;

        if ("on".equals(sub)) {
            VegaConfig.leftEnabled = true;
            VegaConfig.rightEnabled = true;
            on = true;
        } else if ("off".equals(sub)) {
            VegaConfig.leftEnabled = false;
            VegaConfig.rightEnabled = false;
            on = false;
        } else if ("toggle".equals(sub)) {
            VegaConfig.leftEnabled = !VegaConfig.leftEnabled;
            VegaConfig.rightEnabled = !VegaConfig.rightEnabled;
            on = VegaConfig.leftEnabled || VegaConfig.rightEnabled;
        } else if ("status".equals(sub)) {
            sender.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Vega] "
                    + EnumChatFormatting.GRAY + describe()));
            return;
        } else {
            throw new WrongUsageException(getCommandUsage(sender));
        }

        VegaConfig.save();
        sender.addChatMessage(new ChatComponentText(
                EnumChatFormatting.LIGHT_PURPLE + "[Vega] "
                + EnumChatFormatting.GRAY + "Clicker " + (on ? "ON" : "OFF")));
    }

    private String describe() {
        return String.format(Locale.US,
                "Clicker %s | L %.1f-%.1f cps | R %.1f-%.1f cps | hit %d%% | blockhit %s (%d%%) | legacy %s",
                (VegaConfig.leftEnabled || VegaConfig.rightEnabled) ? "ON" : "OFF",
                VegaConfig.leftMinCps, VegaConfig.leftMaxCps,
                VegaConfig.rightMinCps, VegaConfig.rightMaxCps,
                VegaConfig.hitChance,
                VegaConfig.blockHit ? "on" : "off", VegaConfig.blockHitChance,
                VegaConfig.legacyTiming ? "on" : "off");
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
