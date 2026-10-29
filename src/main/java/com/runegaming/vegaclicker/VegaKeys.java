package com.runegaming.vegaclicker;

import java.util.Locale;

import org.lwjgl.input.Keyboard;

import net.minecraft.client.Minecraft;
import net.minecraft.client.settings.KeyBinding;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;

import net.minecraftforge.fml.client.registry.ClientRegistry;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.InputEvent;

import com.runegaming.vegaclicker.gui.VegaGuiScreen;

/**
 * Keybinds, straight from the original binary:
 *
 *   F4      -- toggle the clicker (the original bound BOTH the left and the
 *              right toggle to F4, so one press flips both; 150 ms debounce
 *              like the original watcher at 0x1400461B0)
 *   INSERT  -- the original hid the overlay window; in the mod it opens the
 *              in-game menu instead
 *
 * Both are rebindable through Options > Controls > Vega Clicker.
 */
public class VegaKeys {

    public static KeyBinding toggleClicker;
    public static KeyBinding openGui;

    public static void register() {
        toggleClicker = new KeyBinding("key.vegaclicker.toggle",
                Keyboard.KEY_F4, "key.categories.vegaclicker");
        openGui = new KeyBinding("key.vegaclicker.gui",
                Keyboard.KEY_INSERT, "key.categories.vegaclicker");
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
                VegaConfig.leftEnabled = !VegaConfig.leftEnabled;
                VegaConfig.rightEnabled = !VegaConfig.rightEnabled;

                if (VegaConfig.toggleChatFeedback && mc.thePlayer != null) {
                    boolean on = VegaConfig.leftEnabled || VegaConfig.rightEnabled;
                    String msg = EnumChatFormatting.LIGHT_PURPLE + "[Vega] "
                            + EnumChatFormatting.GRAY + "Clicker " + (on ? "ON" : "OFF")
                            + "  L " + fmt(VegaConfig.leftMinCps) + "-" + fmt(VegaConfig.leftMaxCps)
                            + "  R " + fmt(VegaConfig.rightMinCps) + "-" + fmt(VegaConfig.rightMaxCps)
                            + "  " + VegaConfig.hitChance + "%";
                    mc.thePlayer.addChatMessage(new ChatComponentText(msg));
                }
            }
        }

        if (openGui.isPressed()) {
            if (mc.currentScreen == null) {
                mc.displayGuiScreen(new VegaGuiScreen());
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
