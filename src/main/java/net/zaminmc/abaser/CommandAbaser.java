package net.zaminmc.abaser;

import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

/**
 * /abaser -- tells you how the pieces are wired.
 *
 * The mod never clicks: the auto-clicker is the standalone Abaser Clicker
 * executable. The mod starts it when a server loads, feeds it live game
 * state, and closes it when the game closes.
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
                + EnumChatFormatting.GRAY + " exe - this mod starts it in your"
                + " system tray when a world loads and closes it when the game closes.");
        send(sender, EnumChatFormatting.GRAY + "Put "
                + EnumChatFormatting.WHITE + "AbaserClicker.exe"
                + EnumChatFormatting.GRAY + " in the game folder or the mods folder.");
        send(sender, EnumChatFormatting.GRAY + "The exe clicks only inside Minecraft:"
                + " nothing is clicked while any menu is open or while you hold a bow.");
    }

    private static void send(ICommandSender sender, String text) {
        sender.addChatMessage(new ChatComponentText(text));
    }
}
