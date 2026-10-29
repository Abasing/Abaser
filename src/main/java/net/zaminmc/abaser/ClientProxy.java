package net.zaminmc.abaser;

import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.common.MinecraftForge;
import net.minecraftforge.fml.common.FMLCommonHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;

/**
 * Client-side wiring: keybinds, engine, HUD, /abaser command.
 * Everything starts with the game -- no terminal, no extra windows,
 * nothing in your face until you press F4 or INSERT.
 */
public class ClientProxy extends CommonProxy {

    @Override
    public void init(FMLInitializationEvent event) {
        AbaserKeys.register();

        // Tick + overlay events live on the Forge bus in 1.8.8
        MinecraftForge.EVENT_BUS.register(new AbaserEngine());
        MinecraftForge.EVENT_BUS.register(new AbaserHud());

        // Key input + config-changed events live on the FML bus in 1.8.8
        FMLCommonHandler.instance().bus().register(new AbaserKeys());
        FMLCommonHandler.instance().bus().register(new AbaserConfig.EventHandler());

        ClientCommandHandler.instance.registerCommand(new CommandAbaser());

        Abaser.logger.info("Abaser ready -- F4 toggles, INSERT opens the menu, /abaser for chat control");
    }
}
