package net.zaminmc.abaser;

import java.util.Locale;

import net.minecraft.client.Minecraft;
import net.minecraft.client.entity.EntityPlayerSP;
import net.minecraft.entity.player.EntityPlayer;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;
import net.minecraft.util.MathHelper;
import net.minecraft.util.MovingObjectPosition;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;
import net.minecraftforge.fml.common.gameevent.TickEvent;

/**
 * Abaser's aim assist — the one piece of Abaser that lives entirely inside
 * the mod. It is a camera nudge, nothing more: each client tick it looks
 * for the closest valid PLAYER inside a FOV cone and turns the player's own
 * view a fraction of the way toward them.
 *
 * What it never does is click. Every click still comes from the standalone
 * Abaser Clicker exe through its own engine, hotkeys and PostMessage
 * targeting — the aim assist cannot press a single button, it only steers
 * this player's camera, exactly like a hand on the mouse would.
 *
 * Targeting (players only — the mob assist was removed):
 *
 *   - everyone is valid EXCEPT the parsed peace/team list (PeaceList),
 *     manual exceptions (/abaser except) and, while lock-in mode is on,
 *     anyone not in the locked names (/abaser lock);
 *   - target STICKINESS: the current target is kept while it stays valid
 *     (alive, within distance + FOV hysteresis, still allowed) — the assist
 *     only re-scans for a new closest player when the target drops out, so
 *     it never flip-flops between two players;
 *   - the assist freezes while ANY screen is open (inventory, chest,
 *     villager, enderchest, chat, the panel itself), while dead, while not
 *     in a world, and inside the spawn box when the aim gate is on;
 *   - require click mode assists only while the attack key is held.
 *
 * The same tick also produces the two flags the exe needs for its click
 * gates (sent over the mod link by ClientTickHandler — they are STATE, not
 * clicks):
 *
 *   - inSpawn  — the player stands inside the exact 3D spawn box; the exe
 *     holds every click while its spawn_gate setting is on;
 *   - allyCross — an excepted player (peace list or manual) is under the
 *     crosshair right now; the exe holds every click so you never hit
 *     friends while spamming. Runs regardless of whether the assist is
 *     enabled — it is a clicks safety, not an aim feature.
 */
public final class AimAssist {

    /** The mod-owned settings (config/abaser_aim.json). Replaced by load(). */
    public static AimAssistConfig cfg = new AimAssistConfig();

    /** Live state the exe link reports (spawn gate + ally hold). */
    public static volatile boolean inSpawn    = false;
    public static volatile boolean allyCross  = false;

    private static final AimAssist INSTANCE = new AimAssist();

    public static AimAssist instance() { return INSTANCE; }

    private final Minecraft mc = Minecraft.getMinecraft();
    private EntityPlayer target;
    private Object lastWorld;               // identity marker for peace-list resets

    // keep-target hysteresis: hold a target a bit beyond the acquire limits so
    // two players at similar distance never cause a flicker war
    private static final float KEEP_DISTANCE_PAD = 1.5F;   // blocks beyond maxDistance
    private static final float KEEP_FOV_SCALE    = 1.5F;   // cone widened by this much

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

    /** Current target's name for the panel ("target: <name>"), or null. */
    public static String targetName() {
        final EntityPlayer t = INSTANCE.target;
        return t == null ? null : t.getName();
    }

    // ---- the engine ----------------------------------------------------------

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;

        if (mc.theWorld == null || mc.thePlayer == null) {
            if (lastWorld != null) { lastWorld = null; PeaceList.reset(); }
            target = null;
            inSpawn = false;
            allyCross = false;
            return;
        }
        // a new world = a new server: the old peace list means nothing now
        if (lastWorld != mc.theWorld) {
            lastWorld = mc.theWorld;
            PeaceList.reset();
            target = null;
        }

        // spawn flag: exact 3D box around the player's feet, every tick
        inSpawn = cfg.insideSpawn(mc.thePlayer.posX, mc.thePlayer.posY, mc.thePlayer.posZ);

        // ally flag: an excepted player under the crosshair (clicks safety —
        // independent of whether the aim assist itself is enabled)
        allyCross = cfg.allyHold && crosshairAlly();

        if (mc.currentScreen != null) { target = null; return; }
        if (!cfg.enabled) { target = null; return; }
        if (mc.thePlayer.isDead) { target = null; return; }
        if (cfg.spawnBlockAim && inSpawn) { target = null; return; }
        if (cfg.requireClick && !mc.gameSettings.keyBindAttack.isKeyDown()) { target = null; return; }

        updateTarget();
        if (target != null) aimAtTarget();
    }

    /** True when the player under the crosshair is an excepted (ally) player. */
    private boolean crosshairAlly() {
        if (mc.currentScreen != null || mc.objectMouseOver == null) return false;
        if (mc.objectMouseOver.typeOfHit != MovingObjectPosition.MovingObjectType.ENTITY) return false;
        if (!(mc.objectMouseOver.entityHit instanceof EntityPlayer)) return false;
        final String name = ((EntityPlayer) mc.objectMouseOver.entityHit).getName();
        return PeaceList.contains(name) || cfg.hasException(name);
    }

    /**
     * Sticky targeting: keep the current target while it stays valid, only
     * re-scan when it drops out. This is what keeps the assist from
     * flip-flopping between two players standing close together.
     */
    private void updateTarget() {
        if (target != null && !keepValid(target)) target = null;
        if (target == null) target = acquireTarget();
    }

    /** The looser "keep" test — hysteresis around the acquire limits. */
    private boolean keepValid(EntityPlayer p) {
        if (p.isDead || p.worldObj != mc.theWorld) return false;
        if (p == mc.thePlayer) return false;
        final String name = p.getName();
        if (!lockPass(name) || isExcepted(name)) return false;
        final double distance = mc.thePlayer.getDistanceToEntity(p);
        if (distance > cfg.maxDistance + KEEP_DISTANCE_PAD) return false;
        return isInCone(p, cfg.fov * KEEP_FOV_SCALE / 2.0F);
    }

    /** Closest allowed player inside the distance + FOV limits. */
    private EntityPlayer acquireTarget() {
        double closest = cfg.maxDistance;
        EntityPlayer best = null;

        for (Object o : mc.theWorld.loadedEntityList) {
            if (!(o instanceof EntityPlayer)) continue;
            final EntityPlayer p = (EntityPlayer) o;
            if (p == mc.thePlayer || p.isDead) continue;
            final String name = p.getName();
            if (!lockPass(name) || isExcepted(name)) continue;

            final double distance = mc.thePlayer.getDistanceToEntity(p);
            if (distance > cfg.maxDistance) continue;
            if (distance >= closest) continue;
            if (!isInCone(p, cfg.fov / 2.0F)) continue;
            closest = distance;
            best = p;
        }
        return best;
    }

    /** Peace/team list + manual exceptions decide who is off-limits. */
    private static boolean isExcepted(String name) {
        return PeaceList.contains(name) || cfg.hasException(name);
    }

    /** Lock-in mode: only the locked names may be targeted. */
    private static boolean lockPass(String name) {
        if (!cfg.lockActive()) return true;
        final String k = name.trim().toLowerCase(Locale.ROOT);
        for (String s : cfg.lockNames) if (s.toLowerCase(Locale.ROOT).equals(k)) return true;
        return false;
    }

    /** Is the entity inside a per-axis cone of {@code halfFov} degrees? */
    private boolean isInCone(EntityPlayer entity, float halfFov) {
        final double dx = entity.posX - mc.thePlayer.posX;
        final double dz = entity.posZ - mc.thePlayer.posZ;
        final double dy = (entity.posY + entity.getEyeHeight())
                - (mc.thePlayer.posY + mc.thePlayer.getEyeHeight());
        final double flat = Math.sqrt(dx * dx + dz * dz);
        final float yaw   = (float) Math.toDegrees(Math.atan2(-dx, dz));
        final float pitch = (float) Math.toDegrees(Math.atan2(-dy, flat));
        final float yawDiff   = MathHelper.wrapAngleTo180_float(mc.thePlayer.rotationYaw - yaw);
        final float pitchDiff = MathHelper.wrapAngleTo180_float(mc.thePlayer.rotationPitch - pitch);
        return Math.abs(yawDiff) <= halfFov && Math.abs(pitchDiff) <= halfFov;
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
            // silent: feed the turn through the same entry point the mouse
            // uses (EntityPlayerSP.setAngles). Verified against the 1.8.8
            // implementation: setAngles applies rotationYaw += input * 0.15
            // and rotationPitch -= input * 0.15 (the mouse convention, where
            // a positive input pitches the view DOWN). stepYaw/stepPitch are
            // already the exact degrees we want to turn, so convert them into
            // setAngles input space: divide by the 0.15 mouse scaling and
            // negate the pitch — without this the silent mode turned yaw at
            // ~1/7 strength and pitched the WRONG way.
            final EntityPlayerSP self = (EntityPlayerSP) mc.thePlayer;
            self.setAngles(stepYaw / 0.15F, -stepPitch / 0.15F);
        }
    }
}
