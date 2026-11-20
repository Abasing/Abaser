package net.zaminmc.abaser;

import org.apache.logging.log4j.Logger;

import net.minecraftforge.fml.common.Mod;
import net.minecraftforge.fml.common.SidedProxy;
import net.minecraftforge.fml.common.event.FMLInitializationEvent;
import net.minecraftforge.fml.common.event.FMLPreInitializationEvent;

/**
 * Abaser -- companion mod for Minecraft 1.8.8.
 *
 * The auto-clicker itself lives in the standalone Abaser Clicker executable
 * (see exe/ in this repository): a normal Windows program that finds the
 * game window and posts synthetic clicks to it, exactly like the classic
 * external clickers. This mod carries no clicking logic of its own -- it
 * starts the exe (in the system tray) when a server loads, feeds it live
 * game state over stdin (is a GUI open? is a bow held?) so the exe can hold
 * its clicks in menus and while a bow is drawn, and stops it when the game
 * closes.
 */
@Mod(modid = Abaser.MODID, name = Abaser.NAME,
     version = Abaser.VERSION, acceptedMinecraftVersions = "[1.8.8]")
public class Abaser {

    public static final String MODID   = "abaser";
    public static final String NAME    = "Abaser";
    public static final String VERSION = "2.1-dev.8";

    public static Logger logger;

    @SidedProxy(clientSide = "net.zaminmc.abaser.ClientProxy",
                serverSide = "net.zaminmc.abaser.CommonProxy")
    public static CommonProxy proxy;

    @Mod.EventHandler
    public void preInit(FMLPreInitializationEvent event) {
        logger = event.getModLog();
    }

    @Mod.EventHandler
    public void init(FMLInitializationEvent event) {
        proxy.init(event);
    }
}
