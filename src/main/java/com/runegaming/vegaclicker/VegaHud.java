package com.runegaming.vegaclicker;

import java.util.Locale;

import net.minecraft.client.Minecraft;

import net.minecraftforge.client.event.RenderGameOverlayEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

/**
 * Small in-game status overlay (the mod answer to "is it even running?").
 * Drawn in the accent color while the clicker is armed; stays out of the
 * way of the F3 debug screen.
 */
public class VegaHud {

    @SubscribeEvent
    public void onRenderOverlay(RenderGameOverlayEvent.Post event) {
        if (event.type != RenderGameOverlayEvent.ElementType.TEXT) return;
        if (!VegaConfig.hudEnabled) return;
        if (!VegaConfig.leftEnabled && !VegaConfig.rightEnabled) return;

        Minecraft mc = Minecraft.getMinecraft();
        if (mc.thePlayer == null || mc.theWorld == null) return;

        int x = 4;
        int y = 4;
        if (mc.gameSettings.showDebugInfo) y += 48; // keep clear of F3

        int accent = VegaConfig.accentColor();

        mc.fontRendererObj.drawStringWithShadow("Vega", x, y, accent);
        int wx = x + mc.fontRendererObj.getStringWidth("Vega") + 5;

        StringBuilder sb = new StringBuilder();
        if (VegaConfig.leftEnabled) {
            sb.append("L:ON ").append(fmt(ClickerEngine.leftCps)).append("cps ");
        } else {
            sb.append("L:off ");
        }
        if (VegaConfig.rightEnabled) {
            sb.append("R:ON ").append(fmt(ClickerEngine.rightCps)).append("cps");
        } else {
            sb.append("R:off");
        }
        mc.fontRendererObj.drawStringWithShadow(sb.toString(), wx, y, 0xFFD0D0D0);
    }

    private static String fmt(float v) {
        return String.format(Locale.US, "%.1f", v);
    }
}
