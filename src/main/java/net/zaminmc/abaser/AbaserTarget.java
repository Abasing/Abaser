package net.zaminmc.abaser;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

import net.minecraft.client.Minecraft;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityList;

import net.minecraft.util.MovingObjectPosition;

/**
 * The except list -- names the clicker must never target.
 *
 * Everything here is pure client-side knowledge: the check runs against
 * {@code mc.objectMouseOver}, the ray trace the vanilla client already
 * computes every tick. Nothing is asked of the server, ever.
 *
 * An entry matches if it equals (case-insensitive) any of an entity's
 * names:
 *   - a player's profile name            ("Notch")
 *   - a mob's custom name, if it has one ("god cow")
 *   - a mob's entity type name           ("Cow", "Zombie", "PigZombie")
 * A single {@code *} works as a wildcard ("zombie*" also catches
 * "ZombieVillager", "*" catches literally everything).
 */
public class AbaserTarget {

    /** Entries as typed; matched case-insensitively. */
    public static final List<String> except = new ArrayList<String>();

    // ------------------------------------------------------------------ api

    /** True if the entity under the crosshair is on the except list. */
    public static boolean isCrosshairExcepted(Minecraft mc) {
        Entity e = getCrosshairEntity(mc);
        return e != null && isExcepted(e);
    }

    /** The entity currently under the crosshair (or null). */
    public static Entity getCrosshairEntity(Minecraft mc) {
        MovingObjectPosition mop = mc.objectMouseOver;
        if (mop == null) return null;
        if (mop.typeOfHit != MovingObjectPosition.MovingObjectType.ENTITY) return null;
        return mop.entityHit;
    }

    /** The entity's display name, for the GUI's "add crosshair" button. */
    public static String nameOf(Entity e) {
        if (e == null) return "";
        try {
            String n = e.getName();
            if (n != null) return n;
        } catch (Exception ignored) {}
        return "";
    }

    // ---------------------------------------------------------------- list

    public static void add(String name) {
        String n = name == null ? "" : name.trim();
        if (n.isEmpty()) return;
        for (String s : except) {
            if (s.equalsIgnoreCase(n)) return;
        }
        except.add(n);
    }

    public static boolean remove(String name) {
        String n = name == null ? "" : name.trim();
        Iterator<String> it = except.iterator();
        while (it.hasNext()) {
            if (it.next().equalsIgnoreCase(n)) {
                it.remove();
                return true;
            }
        }
        return false;
    }

    public static void clear() {
        except.clear();
    }

    /** Comma-joined form for the text field / config. */
    public static String joined() {
        StringBuilder sb = new StringBuilder();
        for (String s : except) {
            if (sb.length() > 0) sb.append(", ");
            sb.append(s);
        }
        return sb.toString();
    }

    /** Parse a comma (or semicolon) separated blob back into the list. */
    public static void setFromJoined(String blob) {
        except.clear();
        if (blob == null) return;
        for (String part : blob.split("[,;]")) {
            add(part);
        }
    }

    /** Plain String[] view for the config file. */
    public static String[] toArray() {
        return except.toArray(new String[except.size()]);
    }

    public static void fromArray(String[] arr) {
        except.clear();
        if (arr == null) return;
        for (String s : arr) add(s);
    }

    // ------------------------------------------------------------- matching

    /** Does any list entry suppress this entity? */
    public static boolean isExcepted(Entity e) {
        if (e == null || except.isEmpty()) return false;

        // candidate names: display name (player name / custom name /
        // localized type name) and the registry string ("Cow", "PigZombie")
        String display = null;
        String registry = null;
        try {
            display = e.getName();
        } catch (Exception ignored) {}
        try {
            registry = EntityList.getEntityString(e);
        } catch (Exception ignored) {}

        for (String pattern : except) {
            if (pattern == null || pattern.isEmpty()) continue;
            if (matches(pattern, display) || matches(pattern, registry)) return true;
        }
        return false;
    }

    /**
     * Tiny glob: '*' stands for any run of characters. No regex -- the
     * patterns come from a user-edited text field, keep it dead simple.
     */
    private static boolean matches(String pattern, String target) {
        if (target == null || target.isEmpty()) return false;
        String p = pattern.trim().toLowerCase(Locale.US);
        String t = target.toLowerCase(Locale.US);
        if (!p.contains("*")) return p.equals(t);

        // limit -1 keeps the trailing empty strings, so a trailing '*' in
        // the pattern stays visible ("zombie*" -> ["zombie", ""])
        String[] parts = p.split("\\*+", -1);
        if (parts.length == 0) return true; // pattern was only '*'s -> matches anything
        if (!t.startsWith(parts[0])) return false;
        String last = parts[parts.length - 1];
        if (t.length() - last.length() < parts[0].length()) return false; // not enough room
        if (!t.endsWith(last)) return false;
        int idx = parts[0].length();
        for (int i = 1; i < parts.length; i++) {
            int found = t.indexOf(parts[i], idx);
            if (found < 0) return false;
            idx = found + parts[i].length();
        }
        return true;
    }
}
