package com.runegaming.vegaclicker;

import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.common.MinecraftForge;
import net.minecraftforge.fml.common.FMLCommonHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;

/**
 * Client-side wiring: keybinds, engine, HUD, /vega command.
 * Everything starts with the game -- no terminal, no extra windows,
 * nothing in your face until you press F4 or INSERT.
 */
public class ClientProxy extends CommonProxy {

    @Override
    public void init(FMLInitializationEvent event) {
        VegaKeys.register();

        // Tick + overlay events live on the Forge bus in 1.8.8
        MinecraftForge.EVENT_BUS.register(new ClickerEngine());
        MinecraftForge.EVENT_BUS.register(new VegaHud());

        // Key input + config-changed events live on the FML bus in 1.8.8
        FMLCommonHandler.instance().bus().register(new VegaKeys());
        FMLCommonHandler.instance().bus().register(new VegaConfig.EventHandler());

        ClientCommandHandler.instance.registerCommand(new CommandVega());

        VegaClicker.logger.info("Vega ready -- F4 toggles, INSERT opens the menu, /vega for chat control");
    }
}
