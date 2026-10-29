package net.zaminmc.abaser;

import java.util.Locale;

import net.minecraft.client.Minecraft;

import net.minecraftforge.client.event.RenderGameOverlayEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

/**
 * Small in-game status overlay (the mod answer to "is it even running?").
 * Drawn in the accent color while the clicker is armed; stays out of the
 * way of the F3 debug screen.
 */
public class AbaserHud {

    @SubscribeEvent
    public void onRenderOverlay(RenderGameOverlayEvent.Post event) {
        if (event.type != RenderGameOverlayEvent.ElementType.TEXT) return;
        if (!AbaserConfig.hudEnabled) return;
        if (!AbaserConfig.leftEnabled && !AbaserConfig.rightEnabled) return;

        Minecraft mc = Minecraft.getMinecraft();
        if (mc.thePlayer == null || mc.theWorld == null) return;

        int x = 4;
        int y = 4;
        if (mc.gameSettings.showDebugInfo) y += 48; // keep clear of F3

        int accent = AbaserConfig.accentColor();

        mc.fontRendererObj.drawStringWithShadow("Abaser", x, y, accent);
        int wx = x + mc.fontRendererObj.getStringWidth("Abaser") + 5;

        StringBuilder sb = new StringBuilder();
        if (AbaserConfig.leftEnabled) {
            sb.append("L:ON ").append(fmt(AbaserEngine.leftCps)).append("cps ");
        } else {
            sb.append("L:off ");
        }
        if (AbaserConfig.rightEnabled) {
            sb.append("R:ON ").append(fmt(AbaserEngine.rightCps)).append("cps");
        } else {
            sb.append("R:off");
        }
        mc.fontRendererObj.drawStringWithShadow(sb.toString(), wx, y, 0xFFD0D0D0);

        // Which zone is holding the clicker off, if any.
        AbaserZones.Zone z = AbaserZones.suppressingZone(mc);
        if (z != null) {
            String hold = "held: " + z.name + " (" + z.modeLabel() + ")";
            mc.fontRendererObj.drawStringWithShadow(hold, x, y + 11, 0xFF9C9C9C);
        }
    }

    private static String fmt(float v) {
        return String.format(Locale.US, "%.1f", v);
    }
}
