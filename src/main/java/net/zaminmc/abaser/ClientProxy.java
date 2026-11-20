package net.zaminmc.abaser;

import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.fml.common.FMLCommonHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;

/**
 * Client-side wiring.
 *
 * The clicker itself is the standalone Abaser Clicker executable; this mod
 * starts it when a server loads, feeds it game state (open GUIs, held item)
 * over stdin, and stops it when the game closes. See ClientTickHandler.
 */
public class ClientProxy extends CommonProxy {

    @Override
    public void init(FMLInitializationEvent event) {
        ClientCommandHandler.instance.registerCommand(new CommandAbaser());
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
