package net.zaminmc.abaser;

import net.minecraft.client.Minecraft;
import net.minecraft.command.CommandBase;
import net.minecraft.command.ICommandSender;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

/**
 * /abaser [panel] -- how the pieces are wired, plus the in-game panel.
 *
 * The mod never clicks: the auto-clicker is the standalone Abaser Clicker
 * executable. The mod starts it when a server loads, feeds it live game
 * state, lets you configure it from the in-game panel, and closes it when
 * the game closes.
 */
public class CommandAbaser extends CommandBase {

    @Override
    public String getCommandName() {
        return "abaser";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/abaser [panel]";
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
        if (args.length > 0) {
            final String a = args[0].toLowerCase();
            if (a.equals("panel") || a.equals("gui") || a.equals("open") || a.equals("config")) {
                // client command — we are already on the client thread
                Minecraft.getMinecraft().displayGuiScreen(new AbaserConfigGui());
                return;
            }
        }
        send(sender, EnumChatFormatting.LIGHT_PURPLE + "Abaser "
                + EnumChatFormatting.GRAY + Abaser.VERSION);
        send(sender, EnumChatFormatting.GRAY + "The auto-clicker is the standalone "
                + EnumChatFormatting.WHITE + "Abaser Clicker"
                + EnumChatFormatting.GRAY + " exe - this mod starts it in your"
                + " system tray when a world loads and closes it when the game closes.");
        send(sender, EnumChatFormatting.GRAY + "Put "
                + EnumChatFormatting.WHITE + "AbaserClicker.exe"
                + EnumChatFormatting.GRAY + " in the game folder or the mods folder.");
        send(sender, EnumChatFormatting.GRAY + "Configure it from inside the game: press "
                + EnumChatFormatting.WHITE + "Right Shift"
                + EnumChatFormatting.GRAY + " (or run " + EnumChatFormatting.WHITE + "/abaser panel"
                + EnumChatFormatting.GRAY + ") for the in-game panel.");
        send(sender, EnumChatFormatting.GRAY + "The exe clicks only inside Minecraft:"
                + " nothing is clicked while any menu is open or while you hold a bow.");
    }

    private static void send(ICommandSender sender, String text) {
        sender.addChatMessage(new ChatComponentText(text));
    }
}
