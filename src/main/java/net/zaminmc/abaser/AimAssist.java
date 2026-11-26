package net.zaminmc.abaser;

import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityLivingBase;
import net.minecraft.entity.monster.EntityMob;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;
import net.minecraft.util.MathHelper;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.TickEvent;

/**
 * Abaser's aim assist — the one piece of Abaser that lives entirely inside
 * the mod. It is a camera nudge, nothing more: each client tick it looks
 * for the closest valid target inside a FOV cone and turns the player's own
 * view a fraction of the way toward it.
 *
 * What it never does is click. Every click still comes from the standalone
 * Abaser Clicker exe through its own engine, hotkeys and PostMessage
 * targeting — the aim assist cannot press a single button, it only steers
 * this player's camera, exactly like a hand on the mouse would.
 *
 * Features (absorbed from the classic MOG-AIMASSIST client mod):
 *
 *   - target search: closest living entity within {@code maxDistance}
 *     blocks AND inside a {@code fov} degree cone, filtered by the
 *     Only-mobs / Only-players switches (both off = everything living);
 *   - assist pull: overall {@code strength}, split into separate
 *     horizontal/vertical multipliers;
 *   - smooth aim: turn only {@code smoothFactor} of the remaining angle
 *     per tick so the move reads as a flick, not a snap;
 *   - silent aim: apply the same turn through {@code setAngles}, the way
 *     the mouse would, instead of writing the rotation fields directly;
 *   - require click: only assist while the attack key is held down;
 *
 * Guards: the assist is dormant while any GUI/screen is open (inventory,
 * chest, villager, enderchest, chat, the Abaser panel itself), while the
 * player is dead and while no world is loaded. It also runs on the client
 * thread only — it is a purely client-side effect, invisible to the server
 * beyond the resulting look angles.
 */
public final class AimAssist {

    /** The mod-owned settings (config/abaser_aim.json). Replaced by load(). */
    public static AimAssistConfig cfg = new AimAssistConfig();

    private static final AimAssist INSTANCE = new AimAssist();

    public static AimAssist instance() { return INSTANCE; }

    private final Minecraft mc = Minecraft.getMinecraft();
    private EntityLivingBase target;

    // ---- lifecycle -----------------------------------------------------------

    /** Flip the master switch, persist it and tell the player. */
    public static void toggle() {
        cfg.enabled = !cfg.enabled;
        cfg.save();
        chat(cfg.enabled
                ? EnumChatFormatting.GREEN + "Aim assist ENABLED"
                : EnumChatFormatting.RED + "Aim assist DISABLED");
    }

    public static void setEnabled(boolean on, boolean announce) {
        if (cfg.enabled == on) return;
        cfg.enabled = on;
        cfg.save();
        if (announce) chat(on
                ? EnumChatFormatting.GREEN + "Aim assist ENABLED"
                : EnumChatFormatting.RED + "Aim assist DISABLED");
    }

    private static void chat(String msg) {
        try {
            Minecraft.getMinecraft().thePlayer.addChatMessage(new ChatComponentText(
                    EnumChatFormatting.LIGHT_PURPLE + "[Abaser] " + EnumChatFormatting.GRAY + msg));
        } catch (Exception ignored) {
            // world may be gone already
        }
    }

    // ---- the engine ----------------------------------------------------------

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;
        target = null;
        if (!cfg.enabled) return;
        if (mc.theWorld == null || mc.thePlayer == null) return;
        // any open screen (inventory, chest, villager, enderchest, chat,
        // the Abaser panel) freezes the assist — the camera belongs to the GUI
        if (mc.currentScreen != null) return;
        if (mc.thePlayer.isDead) return;
        if (cfg.requireClick && !mc.gameSettings.keyBindAttack.isKeyDown()) return;

        findTarget();
        if (target != null) aimAtTarget();
    }

    /** Closest living entity inside the distance and FOV limits. */
    private void findTarget() {
        double closest = cfg.maxDistance;
        EntityLivingBase best = null;

        for (Object o : mc.theWorld.loadedEntityList) {
            if (!(o instanceof EntityLivingBase)) continue;
            final EntityLivingBase living = (EntityLivingBase) o;
            if (living == mc.thePlayer || living.isDead) continue;

            // both switches off = everything living; either on = that group only
            if (cfg.onlyMobs && cfg.onlyPlayers) {
                // keep going: both groups allowed
            } else if (cfg.onlyMobs && !(living instanceof EntityMob)) {
                continue;
            } else if (cfg.onlyPlayers && !(living instanceof EntityPlayer)) {
                continue;
            }

            final double distance = mc.thePlayer.getDistanceToEntity(living);
            if (distance > cfg.maxDistance) continue;
            if (distance >= closest) continue;
            if (!isInFOV(living)) continue;
            closest = distance;
            best = living;
        }
        target = best;
    }

    /** Is the entity inside the {@code fov} cone around where we look? */
    private boolean isInFOV(EntityLivingBase entity) {
        final double dx = entity.posX - mc.thePlayer.posX;
        final double dz = entity.posZ - mc.thePlayer.posZ;
        final double dy = (entity.posY + entity.getEyeHeight())
                - (mc.thePlayer.posY + mc.thePlayer.getEyeHeight());
        final double flat = Math.sqrt(dx * dx + dz * dz);
        final float yaw   = (float) Math.toDegrees(Math.atan2(-dx, dz));
        final float pitch = (float) Math.toDegrees(Math.atan2(-dy, flat));
        final float yawDiff   = MathHelper.wrapAngleTo180_float(mc.thePlayer.rotationYaw - yaw);
        final float pitchDiff = MathHelper.wrapAngleTo180_float(mc.thePlayer.rotationPitch - pitch);
        return Math.abs(yawDiff) <= cfg.fov / 2.0F && Math.abs(pitchDiff) <= cfg.fov / 2.0F;
    }

    /** Turn the camera a fraction of the way toward the target. */
    private void aimAtTarget() {
        if (target == null) return;

        final double dx = target.posX - mc.thePlayer.posX;
        final double dz = target.posZ - mc.thePlayer.posZ;
        final double dy = (target.posY + target.getEyeHeight())
                - (mc.thePlayer.posY + mc.thePlayer.getEyeHeight());
        final double flat = Math.sqrt(dx * dx + dz * dz);
        final float wantYaw   = (float) Math.toDegrees(Math.atan2(-dx, dz));
        final float wantPitch = (float) Math.toDegrees(Math.atan2(-dy, flat));

        float yawDiff   = MathHelper.wrapAngleTo180_float(wantYaw - mc.thePlayer.rotationYaw);
        float pitchDiff = MathHelper.wrapAngleTo180_float(wantPitch - mc.thePlayer.rotationPitch);

        if (cfg.smoothAim) {
            yawDiff   *= cfg.smoothFactor;
            pitchDiff *= cfg.smoothFactor;
        }

        final float stepYaw   = yawDiff   * cfg.strength * cfg.horizontalStrength;
        final float stepPitch = pitchDiff * cfg.strength * cfg.verticalStrength;

        if (!cfg.silent) {
            mc.thePlayer.rotationYaw   += stepYaw;
            mc.thePlayer.rotationPitch += stepPitch;
        } else {
            // silent: feed the turn through the same entry point the mouse uses
            final EntityPlayerSP self = (EntityPlayerSP) mc.thePlayer;
            self.setAngles(stepYaw, stepPitch);
        }
    }
}
