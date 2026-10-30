package net.zaminmc.abaser;

import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

/**
 * /abaser -- one line of chat that says where the clicker went.
 *
 * The mod no longer clicks: the auto-clicker is the standalone Abaser
 * Clicker executable. This command just makes that obvious in-game.
 */
public class CommandAbaser extends CommandBase {

    @Override
    public String getCommandName() {
        return "abaser";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/abaser";
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
    public void processCommand(ICommandSender sender, String[] args) {
        send(sender, EnumChatFormatting.LIGHT_PURPLE + "Abaser "
                + EnumChatFormatting.GRAY + Abaser.VERSION);
        send(sender, EnumChatFormatting.GRAY + "The auto-clicker is the standalone "
                + EnumChatFormatting.WHITE + "Abaser Clicker"
                + EnumChatFormatting.GRAY + " executable -- run it next to the game "
                + "and toggle it with its own hotkey (default F4).");
    }

    private static void send(ICommandSender sender, String text) {
        sender.addChatMessage(new ChatComponentText(text));
    }
}
