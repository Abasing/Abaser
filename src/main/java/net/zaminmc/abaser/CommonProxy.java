package net.zaminmc.abaser;

import net.minecraftforge.fml.common.event.FMLInitializationEvent;
import net.minecraftforge.fml.common.event.FMLPreInitializationEvent;

/**
 * Server-side no-op. Abaser is a client-side mod; everything interesting
 * happens in ClientProxy.
 */
public class CommonProxy {
    public void preInit(FMLPreInitializationEvent event) {}
    public void init(FMLInitializationEvent event) {}
}
