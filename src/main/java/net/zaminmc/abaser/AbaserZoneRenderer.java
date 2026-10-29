package net.zaminmc.abaser;

import org.lwjgl.opengl.GL11;

import net.minecraft.client.Minecraft;
import net.minecraft.client.renderer.GlStateManager;
import net.minecraft.client.renderer.Tessellator;
import net.minecraft.client.renderer.WorldRenderer;
import net.minecraft.client.renderer.vertex.DefaultVertexFormats;
import net.minecraft.entity.Entity;

import net.minecraftforge.client.event.RenderWorldLastEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

import net.zaminmc.abaser.gui.AbaserGuiScreen;

/**
 * Wireframe preview of the zones -- but ONLY while the Abaser menu is
 * open, so it never clutters normal gameplay. Stand back, open the menu
 * (INSERT), check that the box sits where you meant it to, close it.
 *
 * The selected zone is drawn in the accent color; the others dim gray.
 * Vertical zones are drawn as the full-height column they really are.
 * Depth test is off while drawing (WorldEdit style), so the box edges
 * shine through terrain and stay readable in caves and ravines.
 */
public class AbaserZoneRenderer {

    @SubscribeEvent
    public void onRenderWorldLast(RenderWorldLastEvent event) {
        Minecraft mc = Minecraft.getMinecraft();
        if (!(mc.currentScreen instanceof AbaserGuiScreen)) return;
        if (!AbaserConfig.zoneOverlay || !AbaserConfig.zonesEnabled) return;
        if (zonesToDraw(mc) == 0) return;

        Entity view = mc.getRenderViewEntity();
        if (view == null) view = mc.thePlayer;
        if (view == null) return;
        float pt = event.partialTicks;
        double ex = view.prevPosX + (view.posX - view.prevPosX) * pt;
        double ey = view.prevPosY + (view.posY - view.prevPosY) * pt;
        double ez = view.prevPosZ + (view.posZ - view.prevPosZ) * pt;

        GlStateManager.pushMatrix();
        GlStateManager.translate((float) -ex, (float) -ey, (float) -ez);
        GlStateManager.disableTexture2D();
        GlStateManager.disableDepth();
        GlStateManager.enableBlend();
        GlStateManager.blendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
        GL11.glLineWidth(2.0f);

        int dim = mc.theWorld.provider.getDimensionId();
        for (int i = 0; i < AbaserZones.zones.size(); i++) {
            AbaserZones.Zone z = AbaserZones.zones.get(i);
            if (z.dim != dim) continue;
            boolean sel = (i == AbaserZones.selected);
            // selected: accent, bright; others: dim gray
            float r = sel ? AbaserConfig.accentR : 0.6f;
            float g = sel ? AbaserConfig.accentG : 0.6f;
            float b = sel ? AbaserConfig.accentB : 0.6f;
            float a = sel ? 0.85f : 0.30f;
            drawBox(z, r, g, b, a);
        }

        GL11.glLineWidth(1.0f);
        GlStateManager.enableDepth();
        GlStateManager.disableBlend();
        GlStateManager.enableTexture2D();
        GlStateManager.popMatrix();
    }

    /** How many zones will actually be drawn in this dimension (0 = skip GL entirely). */
    private static int zonesToDraw(Minecraft mc) {
        if (AbaserZones.zones.isEmpty() || mc.theWorld == null) return 0;
        int dim = mc.theWorld.provider.getDimensionId();
        int n = 0;
        for (AbaserZones.Zone z : AbaserZones.zones) {
            if (z.dim == dim) n++;
        }
        return n;
    }

    private static void drawBox(AbaserZones.Zone z, float r, float g, float b, float a) {
        double x1 = z.minX();
        double x2 = z.maxX() + 1.0;   // block coords: edge at the block boundary
        double zmn = z.minZ();
        double zmx = z.maxZ() + 1.0;
        double y1;
        double y2;
        if (z.vertical) {
            y1 = 0.0;                 // the whole column, bedrock to sky
            y2 = 256.0;
        } else {
            y1 = z.minY();
            y2 = z.maxY() + 1.0;
        }

        Tessellator t = Tessellator.getInstance();
        WorldRenderer wr = t.getWorldRenderer();
        wr.begin(GL11.GL_LINES, DefaultVertexFormats.POSITION_COLOR);

        // bottom rectangle
        edge(wr, x1, y1, zmn, x2, y1, zmn, r, g, b, a);
        edge(wr, x2, y1, zmn, x2, y1, zmx, r, g, b, a);
        edge(wr, x2, y1, zmx, x1, y1, zmx, r, g, b, a);
        edge(wr, x1, y1, zmx, x1, y1, zmn, r, g, b, a);
        // top rectangle
        edge(wr, x1, y2, zmn, x2, y2, zmn, r, g, b, a);
        edge(wr, x2, y2, zmn, x2, y2, zmx, r, g, b, a);
        edge(wr, x2, y2, zmx, x1, y2, zmx, r, g, b, a);
        edge(wr, x1, y2, zmx, x1, y2, zmn, r, g, b, a);
        // verticals
        edge(wr, x1, y1, zmn, x1, y2, zmn, r, g, b, a);
        edge(wr, x2, y1, zmn, x2, y2, zmn, r, g, b, a);
        edge(wr, x2, y1, zmx, x2, y2, zmx, r, g, b, a);
        edge(wr, x1, y1, zmx, x1, y2, zmx, r, g, b, a);

        t.draw();
    }

    private static void edge(WorldRenderer wr, double x1, double y1, double z1,
                             double x2, double y2, double z2,
                             float r, float g, float b, float a) {
        wr.pos(x1, y1, z1).color(r, g, b, a).endVertex();
        wr.pos(x2, y2, z2).color(r, g, b, a).endVertex();
    }
}
