package com.runegaming.vegaclicker;

import org.apache.logging.log4j.Logger;

import net.minecraftforge.fml.common.Mod;
import net.minecraftforge.fml.common.SidedProxy;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;
import net.minecraftforge.fml.common.event.FMLPreInitializationEvent;

/**
 * Vega - Clicker V2 | Coming Soon Edition -- as a Minecraft 1.8.8 Forge mod.
 *
 * The original was a standalone ImGui (DX9) tool by RuneGaming
 * ("Made with <3"). This mod is a faithful port: it loads with the game,
 * runs silently in the background (no console window), toggles with F4 and
 * is customized through an in-game menu. All preferences persist in
 * config/vegaclicker.cfg.
 */
@Mod(modid = VegaClicker.MODID, name = VegaClicker.NAME,
     version = VegaClicker.VERSION, acceptedMinecraftVersions = "[1.8.8]")
public class VegaClicker {

    public static final String MODID   = "vegaclicker";
    public static final String NAME    = "Vega Clicker";
    public static final String VERSION = "2.0";

    public static Logger logger;

    @SidedProxy(clientSide = "com.runegaming.vegaclicker.ClientProxy",
                serverSide = "com.runegaming.vegaclicker.CommonProxy")
    public static CommonProxy proxy;

    @Mod.EventHandler
    public void preInit(FMLPreInitializationEvent event) {
        logger = event.getModLog();
        VegaConfig.load(event.getSuggestedConfigurationFile());
        logger.info("Vega - Clicker V2 | Coming Soon Edition -- Made with <3, RuneGaming");
    }

    @Mod.EventHandler
    public void init(FMLInitializationEvent event) {
        proxy.init(event);
    }
}
