package net.zaminmc.abaser;

import java.util.Locale;

import org.lwjgl.input.Keyboard;

import net.minecraft.client.Minecraft;
import net.minecraft.client.settings.KeyBinding;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

import net.minecraftforge.fml.client.registry.ClientRegistry;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.InputEvent;

import net.zaminmc.abaser.gui.AbaserGuiScreen;

/**
 * Keybinds:
 *
 *   F4      -- toggle the clicker (both the left and the right toggle, one
 *              press flips both, exactly like the original bind; 150 ms
 *              debounce like the original watcher at 0x1400461B0)
 *   INSERT  -- opens the in-game menu
 *
 * Both are rebindable through Options > Controls > Abaser.
 */
public class AbaserKeys {

    public static KeyBinding toggleClicker;
    public static KeyBinding openGui;

    public static void register() {
        toggleClicker = new KeyBinding("key.abaser.toggle",
                Keyboard.KEY_F4, "key.categories.abaser");
        openGui = new KeyBinding("key.abaser.gui",
                Keyboard.KEY_INSERT, "key.categories.abaser");
        ClientRegistry.registerKeyBinding(toggleClicker);
        ClientRegistry.registerKeyBinding(openGui);
    }

    private static long lastToggleMs = 0L;

    @SubscribeEvent
    public void onKeyInput(InputEvent.KeyInputEvent event) {
        Minecraft mc = Minecraft.getMinecraft();
        long now = System.currentTimeMillis();

        if (toggleClicker.isPressed()) {
            if (now - lastToggleMs >= 150L) { // original debounce window
                lastToggleMs = now;
                AbaserConfig.leftEnabled = !AbaserConfig.leftEnabled;
                AbaserConfig.rightEnabled = !AbaserConfig.rightEnabled;

                if (AbaserConfig.toggleChatFeedback && mc.thePlayer != null) {
                    boolean on = AbaserConfig.leftEnabled || AbaserConfig.rightEnabled;
                    String msg = EnumChatFormatting.LIGHT_PURPLE + "[Abaser] "
                            + EnumChatFormatting.GRAY + "Clicker " + (on ? "ON" : "OFF")
                            + "  L " + fmt(AbaserConfig.leftMinCps) + "-" + fmt(AbaserConfig.leftMaxCps)
                            + "  R " + fmt(AbaserConfig.rightMinCps) + "-" + fmt(AbaserConfig.rightMaxCps)
                            + "  " + AbaserConfig.hitChance + "%";
                    mc.thePlayer.addChatMessage(new ChatComponentText(msg));
                }
            }
        }

        if (openGui.isPressed()) {
            if (mc.currentScreen == null) {
                mc.displayGuiScreen(new AbaserGuiScreen());
            }
        }
    }

    private static String fmt(float v) {
        return String.format(Locale.US, "%.1f", v);
    }

    /** Human-readable bind name ("F4", "INSERT", "Mouse 3", ...). */
    public static String keyName(KeyBinding kb) {
        if (kb == null) return "?";
        int code = kb.getKeyCode();
        if (code >= 0) return Keyboard.getKeyName(code);
        return "Mouse " + (code + 100);
    }
}
