package net.zaminmc.abaser;

import org.apache.logging.log4j.Logger;

import net.minecraftforge.fml.common.Mod;
import net.minecraftforge.fml.common.SidedProxy;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;
import net.minecraftforge.fml.common.event.FMLPreInitializationEvent;

/**
 * Abaser -- a Minecraft 1.8.8 Forge auto-clicker mod.
 *
 * It loads with the game and runs silently in the background (no terminal,
 * no extra windows), toggles with F4 and is customized through an in-game
 * menu. All preferences persist in config/abaser.cfg.
 */
@Mod(modid = Abaser.MODID, name = Abaser.NAME,
     version = Abaser.VERSION, acceptedMinecraftVersions = "[1.8.8]")
public class Abaser {

    public static final String MODID   = "abaser";
    public static final String NAME    = "Abaser";
    public static final String VERSION = "2.0-dev.2";

    public static Logger logger;

    @SidedProxy(clientSide = "net.zaminmc.abaser.ClientProxy",
                serverSide = "net.zaminmc.abaser.CommonProxy")
    public static CommonProxy proxy;

    @Mod.EventHandler
    public void preInit(FMLPreInitializationEvent event) {
        logger = event.getModLog();
        AbaserConfig.load(event.getSuggestedConfigurationFile());
    }

    @Mod.EventHandler
    public void init(FMLInitializationEvent event) {
        proxy.init(event);
    }
}
