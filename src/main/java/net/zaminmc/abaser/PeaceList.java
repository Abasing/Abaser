package net.zaminmc.abaser;

import java.util.LinkedHashSet;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import net.minecraft.client.Minecraft;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.EnumChatFormatting;
import net.minecraftforge.client.event.ClientChatReceivedEvent;
import net.minecraftforge.fml.common.eventhandler.SubscribeEvent;

/**
 * The peace/team list, parsed straight out of the server chat.
 *
 * SKYPVP (and servers like it) announce the player's allies like this:
 *
 *     SKYPVP » You have peace with 47 player(s).
 *     - Mr_Criminal_001
 *     - ninejax
 *     - LeDamage
 *     ...
 *
 * (color codes and rank colors ride along in the real component — this
 * parser works on the plain text, which reads exactly like the block
 * above). The trigger line starts a capture with the announced count;
 * every following "- Name" line adds one name; the capture ends when the
 * count is reached OR the first line that is not a list entry arrives
 * (other chat interleaving) — whatever was collected gets committed.
 *
 * The names form an exception set: the aim assist never targets them, and
 * while one of them is under the crosshair the mod reports ally=true so
 * the exe holds every click. The list lives in memory only — the server
 * re-announces it (relog, or the server's own peace command) and the next
 * parse replaces it; switching worlds clears it.
 */
public final class PeaceList {

    /** Trigger: "You have peace with 47 player(s)." (wording from SKYPVP). */
    private static final Pattern TRIGGER =
            Pattern.compile("You have peace with\\s+(\\d+)\\s+player", Pattern.CASE_INSENSITIVE);

    /** A list entry: "- Mr_Criminal_001" (plain text after formatting is gone). */
    private static final Pattern LIST_LINE = Pattern.compile("^-\\s*(\\S.*)$");

    private static final Set<String> names   = new LinkedHashSet<String>();  // committed, original case
    private static final Set<String> pending = new LinkedHashSet<String>();  // capture in progress
    private static boolean capturing = false;
    private static int remaining = 0;

    /** Registered by ClientProxy on the forge bus. */
    public PeaceList() {}

    // ---- the event (registered on the forge bus by ClientProxy) --------------

    @SubscribeEvent
    public void onChat(ClientChatReceivedEvent event) {
        try {
            final String raw = event.message.getUnformattedText();
            if (raw == null || raw.length() == 0) return;
            final String line = EnumChatFormatting.getTextWithoutFormattingCodes(raw).trim();
            if (line.length() == 0) return;

            // a fresh trigger always (re)starts the capture
            final Matcher m = TRIGGER.matcher(line);
            if (m.find()) {
                int n;
                try { n = Integer.parseInt(m.group(1)); } catch (Exception e) { n = 100; }
                pending.clear();
                capturing = true;
                remaining = Math.max(1, Math.min(100, n));
                return;
            }
            if (!capturing) return;

            final Matcher entry = LIST_LINE.matcher(line);
            if (entry.matches()) {
                final String name = entry.group(1).trim();
                if (!name.isEmpty() && !isSelf(name)) pending.add(name);
                if (--remaining <= 0) commit();
                return;
            }
            // any other chat line ends the capture — keep what we got
            commit();
        } catch (Exception ignored) {
            // a weird chat line must never break the game
        }
    }

    private static boolean isSelf(String name) {
        try {
            final Minecraft mc = Minecraft.getMinecraft();
            return mc.thePlayer != null
                    && name.equalsIgnoreCase(mc.thePlayer.getName());
        } catch (Exception e) { return false; }
    }

    private static void commit() {
        capturing = false;
        final boolean changed = !pending.equals(names);
        names.clear();
        names.addAll(pending);
        pending.clear();
        if (changed && !names.isEmpty()) {
            try {
                Minecraft.getMinecraft().thePlayer.addChatMessage(new ChatComponentText(
                        EnumChatFormatting.LIGHT_PURPLE + "[Abaser] " + EnumChatFormatting.GRAY
                                + "peace list parsed: " + EnumChatFormatting.WHITE + names.size()
                                + EnumChatFormatting.GRAY + " player(s) - aim assist and clicks will skip them"));
            } catch (Exception ignored) {}
        }
    }

    // ---- queries -------------------------------------------------------------

    /** Case-insensitive membership. */
    public static boolean contains(String name) {
        if (name == null || names.isEmpty()) return false;
        final String k = name.trim().toLowerCase(java.util.Locale.ROOT);
        for (String s : names) if (s.toLowerCase(java.util.Locale.ROOT).equals(k)) return true;
        return false;
    }

    public static int size() { return names.size(); }

    /** Snapshot for display (/abaser peace, the panel). */
    public static Set<String> snapshot() { return new LinkedHashSet<String>(names); }

    /** World changed or left: the old server's allies mean nothing now. */
    public static void reset() {
        capturing = false;
        remaining = 0;
        pending.clear();
        names.clear();
    }
}
