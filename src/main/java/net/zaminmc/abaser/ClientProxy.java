package net.zaminmc.abaser;

import net.minecraftforge.client.ClientCommandHandler;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;

/**
 * Client-side wiring. These days that is just the /abaser info command --
 * the clicker itself is the standalone Abaser Clicker executable, so there
 * is nothing left to hook into the game loop.
 */
public class ClientProxy extends CommonProxy {

    @Override
    public void init(FMLInitializationEvent event) {
        ClientCommandHandler.instance.registerCommand(new CommandAbaser());
        Abaser.logger.info("Abaser ready -- the clicker lives in the standalone Abaser Clicker exe");
    }
}
