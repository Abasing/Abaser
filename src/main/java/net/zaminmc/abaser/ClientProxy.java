package net.zaminmc.abaser;

import org.apache.commons.lang3.ArrayUtils;
import org.lwjgl.input.Keyboard;

import net.minecraft.client.Minecraft;
import net.minecraft.client.settings.KeyBinding;
import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.fml.common.FMLCommonHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;

/**
 * Client-side wiring.
 *
 * The clicker itself is the standalone Abaser Clicker executable; this mod
 * starts it when a server loads, feeds it game state (open GUIs, held item)
 * over stdin, reads the exe's settings + live status from stdout for the
 * in-game panel, and stops it when the game closes. See ClientTickHandler
 * and AbaserConfigGui.
 */
public class ClientProxy extends CommonProxy {

    /** Opens the in-game Abaser panel (default Right Shift, rebindable). */
    public static KeyBinding panelKey;

    @Override
    public void init(FMLInitializationEvent event) {
        ClientCommandHandler.instance.registerCommand(new CommandAbaser());
        // Forge 1.8.8's ClientRegistry.registerKeyBinding does not exist yet
        // in the 11.15.0.1655 line, so register the vanilla way: append to
        // gameSettings.keyBindings. The controls screen then lists it, can
        // rebind it and persists it, and KeyBinding.isPressed() ticks in the
        // vanilla input loop (only while no screen is open — exactly what
        // the panel wants).
        panelKey = new KeyBinding("key.abaser.panel", Keyboard.KEY_RSHIFT, "key.categories.abaser");
        Minecraft.getMinecraft().gameSettings.keyBindings =
                ArrayUtils.add(Minecraft.getMinecraft().gameSettings.keyBindings, panelKey);
        // TickEvents ride the FML bus in 1.8.8, not the forge bus
        FMLCommonHandler.instance().bus().register(new ClientTickHandler());
        // game closing -> clicker closes with it (the exe also exits on stdin
        // EOF, so this hook is the polite path, not the only path)
        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable() {
            public void run() { ClientTickHandler.shutdownClicker(); }
        }, "Abaser-Shutdown"));
        Abaser.logger.info("Abaser ready -- join a world and the Abaser Clicker exe starts in your tray");
    }
}
