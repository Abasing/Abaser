package net.zaminmc.abaser;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Random;

import org.lwjgl.input.Mouse;

import net.minecraft.client.Minecraft;
import net.minecraft.item.ItemBow;
import net.minecraft.item.ItemSword;
import net.minecraft.item.ItemStack;

import net.minecraftforge.fml.common.gameevent.TickEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

/**
 * The clicker engine, ported from the original binary's thread at
 * 0x1400439D0. The original posted WM_LBUTTONDOWN/UP to the LWJGL window;
 * the mod equivalent is calling the game's own click handlers
 * (Minecraft.clickMouse / rightClickMouse) via reflection, which runs the
 * exact vanilla attack/use code path.
 *
 * Faithfully preserved quirks (all optional, see AbaserConfig.legacyTiming):
 *  - legacy delay math: 100/cps + (300|400)/cps2 ms per cycle
 *    (real rate runs ~2x the label -- this IS the original feel)
 *  - in legacy mode the right branch reads hitChance, not rightChance
 *    (the original's right-chance slider was dead data)
 *  - hit chance + block-hit chance rolls per click
 *  - randomized CPS drawn per click between min/max
 *  - one F4 press flips left AND right (the original bound both to F4)
 *
 * Safety gating (see the Safety tab of the menu):
 *  - clicking always stops while any GUI / inventory / chat is open
 *  - optional idle while the window is unfocused
 *  - optional: no right clicks while holding a bow (draw and aim in peace)
 *  - optional: no right clicks while using an item (eat / drink in peace)
 *  - optional: left clicks only while holding a sword
 */
public class AbaserEngine {

    private static final Random RNG = new Random();

    // --- reflection caches (MCP name for dev, SRG name for production) ---
    private static Method clickMouseMethod;
    private static Method rightClickMouseMethod;
    private static Field  leftClickCounterField;
    private static Field  rightClickDelayField;

    private long nextLeftNs = 0L;
    private long nextRightNs = 0L;

    // live CPS meters for the HUD overlay
    public static volatile float leftCps = 0f;
    public static volatile float rightCps = 0f;
    private static long lastLeftMs = 0L;
    private static long lastRightMs = 0L;

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent event) {
        if (event.phase != TickEvent.Phase.END) return;

        Minecraft mc = Minecraft.getMinecraft();
        if (mc.thePlayer == null || mc.theWorld == null) return;

        // While ANY screen is open (inventory, chest, chat, pause, menu) we
        // idle and reset the schedule, so arming the clicker resumes cleanly
        // the moment the screen closes.
        if (mc.currentScreen != null) {
            long n = System.nanoTime();
            nextLeftNs = n;
            nextRightNs = n;
            return;
        }

        // No clicks into an unfocused window either (a real player can't).
        if (AbaserConfig.pauseUnfocused && !mc.inGameHasFocus) {
            long n = System.nanoTime();
            nextLeftNs = n;
            nextRightNs = n;
            return;
        }

        long now = System.nanoTime();

        // ---- LEFT branch --------------------------------------------------
        // Original: GetAsyncKeyState(VK_LBUTTON) while left toggle is on.
        if (AbaserConfig.leftEnabled && Mouse.isButtonDown(0)) {
            if (now >= nextLeftNs) {
                if (canLeftClick(mc) && RNG.nextInt(100) < AbaserConfig.hitChance) {
                    leftClick(mc);
                    // Block Hit: RMB tap right after the attack
                    // (original: WM_RBUTTONDOWN, 20 ms, WM_RBUTTONUP)
                    if (AbaserConfig.blockHit
                            && canRightClick(mc)
                            && RNG.nextInt(100) < AbaserConfig.blockHitChance) {
                        rightClick(mc);
                    }
                }
                nextLeftNs = now + leftIntervalNs();
            }
        } else {
            nextLeftNs = now;
        }

        // ---- RIGHT branch -------------------------------------------------
        if (AbaserConfig.rightEnabled && Mouse.isButtonDown(1)) {
            if (now >= nextRightNs) {
                // Original quirk: the right branch read hit_chance and never
                // touched the right-chance slider. Kept in legacy mode only.
                int chance = AbaserConfig.legacyTiming ? AbaserConfig.hitChance : AbaserConfig.rightChance;
                if (canRightClick(mc) && RNG.nextInt(100) < chance) {
                    rightClick(mc);
                }
                nextRightNs = now + rightIntervalNs();
            }
        } else {
            nextRightNs = now;
        }
    }

    // ---------------------------------------------------------------- gates

    /** Sword-only mode: only attack while actually holding a sword. */
    private static boolean canLeftClick(Minecraft mc) {
        if (!AbaserConfig.swordOnly) return true;
        ItemStack held = mc.thePlayer.getHeldItem();
        return held != null && held.getItem() instanceof ItemSword;
    }

    /** Bow guard + use-item guard: right-click only when it makes sense. */
    private static boolean canRightClick(Minecraft mc) {
        ItemStack held = mc.thePlayer.getHeldItem();
        if (AbaserConfig.noBowRight && held != null && held.getItem() instanceof ItemBow) {
            return false;
        }
        if (AbaserConfig.noUseInterrupt && mc.thePlayer.isUsingItem()) {
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------- timing

    /** Original lerp(min, max, rand()/32767) */
    private static float randBetween(float min, float max) {
        return min + (max - min) * RNG.nextFloat();
    }

    /**
     * Legacy mode reproduces the original cycle:
     *   sleep(100/cps); [click]; sleep((cps2 > 14 ? 300 : 400)/cps2)
     * Accurate mode is the "correct" 1000/cps math.
     */
    private static long leftIntervalNs() {
        float cps = randBetween(AbaserConfig.leftMinCps, AbaserConfig.leftMaxCps);
        if (AbaserConfig.legacyTiming) {
            float cps2 = randBetween(AbaserConfig.leftMinCps, AbaserConfig.leftMaxCps);
            long ms = (long) (100f / cps) + (long) ((cps2 > 14.0f ? 300f : 400f) / cps2);
            return Math.max(ms, 1L) * 1000000L;
        }
        return (long) (1000f / Math.max(cps, 0.1f) * 1000000f);
    }

    private static long rightIntervalNs() {
        float cps = randBetween(AbaserConfig.rightMinCps, AbaserConfig.rightMaxCps);
        if (AbaserConfig.legacyTiming) {
            float cps2 = randBetween(AbaserConfig.rightMinCps, AbaserConfig.rightMaxCps);
            long ms = (long) (100f / cps) + (long) ((cps2 > 14.0f ? 300f : 400f) / cps2);
            return Math.max(ms, 1L) * 1000000L;
        }
        return (long) (1000f / Math.max(cps, 0.1f) * 1000000f);
    }

    // ---------------------------------------------------------------- clicks

    /** Full vanilla left click (attack/miss swing), via the game's own code. */
    public static void leftClick(Minecraft mc) {
        try {
            Field lc = leftClickCounter();
            if (lc != null) lc.setInt(mc, 0); // clear the vanilla click cooldown
            Method m = clickMouse();
            if (m != null) m.invoke(mc);
            long now = System.currentTimeMillis();
            if (lastLeftMs != 0L) {
                float dt = (now - lastLeftMs) / 1000f;
                if (dt > 0f && dt < 2f) leftCps = leftCps * 0.5f + (1f / dt) * 0.5f;
            }
            lastLeftMs = now;
        } catch (Exception ignored) {
            // never let the engine kill the game
        }
    }

    /** Full vanilla right click (use/place/block), via the game's own code. */
    public static void rightClick(Minecraft mc) {
        try {
            Field rc = rightClickDelay();
            if (rc != null) rc.setInt(mc, 0); // clear the vanilla use cooldown
            Method m = rightClickMouse();
            if (m != null) m.invoke(mc);
            long now = System.currentTimeMillis();
            if (lastRightMs != 0L) {
                float dt = (now - lastRightMs) / 1000f;
                if (dt > 0f && dt < 2f) rightCps = rightCps * 0.5f + (1f / dt) * 0.5f;
            }
            lastRightMs = now;
        } catch (Exception ignored) {
            // never let the engine kill the game
        }
    }

    // ------------------------------------------------------------ reflection

    private static Method clickMouse() {
        if (clickMouseMethod == null)
            clickMouseMethod = findMethod("clickMouse", "func_147116_af");
        return clickMouseMethod;
    }

    private static Method rightClickMouse() {
        if (rightClickMouseMethod == null)
            rightClickMouseMethod = findMethod("rightClickMouse", "func_147121_ag");
        return rightClickMouseMethod;
    }

    private static Field leftClickCounter() {
        if (leftClickCounterField == null)
            leftClickCounterField = findField("leftClickCounter", "field_71429_W");
        return leftClickCounterField;
    }

    private static Field rightClickDelay() {
        if (rightClickDelayField == null)
            rightClickDelayField = findField("rightClickDelayTimer", "field_71467_ac");
        return rightClickDelayField;
    }

    /** MCP name works in dev, SRG name works in a production (obfuscated) jar. */
    private static Method findMethod(String... names) {
        for (String n : names) {
            try {
                Method m = Minecraft.class.getDeclaredMethod(n);
                m.setAccessible(true);
                return m;
            } catch (NoSuchMethodException ignored) {}
        }
        return null;
    }

    private static Field findField(String... names) {
        for (String n : names) {
            try {
                Field f = Minecraft.class.getDeclaredField(n);
                f.setAccessible(true);
                return f;
            } catch (NoSuchFieldException ignored) {}
        }
        return null;
    }
}
