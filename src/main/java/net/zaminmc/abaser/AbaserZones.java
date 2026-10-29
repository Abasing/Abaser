package net.zaminmc.abaser;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import net.minecraft.client.Minecraft;
import net.minecraft.util.BlockPos;

/**
 * World zones -- regions where the clicker automatically holds off.
 *
 * A zone is the box between corner 1 and corner 2 (either corner may be
 * the min or the max; they are normalized on every check). Two dials:
 *
 *   - mode: "off inside" (the zone is an exception -- the clicker holds
 *     off while you are in it) or "only inside" (the zone is the ONLY
 *     place the clicker works -- it holds off everywhere else);
 *   - vertical: the Y coordinates are ignored and the zone becomes the
 *     whole column, top to bottom -- an "except column" / "only column".
 *
 * Zones are dimension-bound: a zone captured in the Overworld never
 * fires in the Nether. Everything is checked against the local player's
 * own position -- fully client side, no server involvement of any kind.
 *
 * If several zones overlap, any zone that says "hold off" wins, so an
 * "only inside" zone carves the allowed area and "off inside" zones
 * subtract from it.
 */
public class AbaserZones {

    /** The zone list, in config order. */
    public static final List<Zone> zones = new ArrayList<Zone>();

    /** Index of the zone the GUI and the /abaser pos1|pos2 commands edit. */
    public static int selected = 0;

    // ---------------------------------------------------------------- model

    public static class Zone {
        public String name = "Zone";
        public int dim = 0;
        public int x1, y1, z1;                      // corner 1 (any order)
        public int x2, y2, z2;                      // corner 2 (any order)
        public boolean vertical = false;            // ignore Y: whole column
        public boolean offInside = true;            // true: off INSIDE (except zone); false: off OUTSIDE (only zone)

        public int minX() { return Math.min(x1, x2); }
        public int maxX() { return Math.max(x1, x2); }
        public int minY() { return Math.min(y1, y2); }
        public int maxY() { return Math.max(y1, y2); }
        public int minZ() { return Math.min(z1, z2); }
        public int maxZ() { return Math.max(z1, z2); }

        public boolean contains(int x, int y, int z) {
            if (x < minX() || x > maxX()) return false;
            if (z < minZ() || z > maxZ()) return false;
            if (!vertical && (y < minY() || y > maxY())) return false;
            return true;
        }

        /** Does THIS zone demand a hold-off at these coordinates? */
        public boolean suppresses(int x, int y, int z) {
            boolean inside = contains(x, y, z);
            return offInside ? inside : !inside;
        }

        public String modeLabel() {
            return offInside ? "off inside" : "only inside";
        }

        public String coordsLabel() {
            if (vertical) {
                return String.format(Locale.US, "%d, %d .. %d, %d  (any Y)",
                        minX(), minZ(), maxX(), maxZ());
            }
            return String.format(Locale.US, "%d, %d, %d .. %d, %d, %d",
                    minX(), minY(), minZ(), maxX(), maxY(), maxZ());
        }
    }

    // ---------------------------------------------------------------- state

    /** True = the clicker must hold off right now (player standing in a zone). */
    public static boolean isSuppressed(Minecraft mc) {
        if (!AbaserConfig.zonesEnabled) return false;
        if (zones.isEmpty()) return false;
        if (mc.thePlayer == null || mc.theWorld == null) return false;

        int dim = mc.theWorld.provider.getDimensionId();
        BlockPos p = mc.thePlayer.getPosition();
        for (Zone z : zones) {
            if (z.dim != dim) continue;
            if (z.suppresses(p.getX(), p.getY(), p.getZ())) return true;
        }
        return false;
    }

    /** The zone currently holding the clicker off (for the HUD), or null. */
    public static Zone suppressingZone(Minecraft mc) {
        if (!AbaserConfig.zonesEnabled) return null;
        if (mc.thePlayer == null || mc.theWorld == null) return null;

        int dim = mc.theWorld.provider.getDimensionId();
        BlockPos p = mc.thePlayer.getPosition();
        for (Zone z : zones) {
            if (z.dim != dim) continue;
            if (z.suppresses(p.getX(), p.getY(), p.getZ())) return z;
        }
        return null;
    }

    /** The zone the GUI/commands operate on, creating one if needed. */
    public static Zone ensureSelected(Minecraft mc) {
        if (zones.isEmpty()) {
            Zone z = new Zone();
            z.name = "Zone 1";
            if (mc != null && mc.thePlayer != null && mc.theWorld != null) {
                BlockPos p = mc.thePlayer.getPosition();
                z.dim = mc.theWorld.provider.getDimensionId();
                z.x1 = z.x2 = p.getX();
                z.y1 = z.y2 = p.getY();
                z.z1 = z.z2 = p.getZ();
            }
            zones.add(z);
            selected = 0;
        }
        if (selected < 0 || selected >= zones.size()) selected = zones.size() - 1;
        return zones.get(selected);
    }

    /** Capture a corner at the player's feet ("stand there, run the command"). */
    public static Zone captureCorner(Minecraft mc, boolean corner2) {
        Zone z = ensureSelected(mc);
        if (mc.thePlayer == null) return z;
        BlockPos p = mc.thePlayer.getPosition();
        if (corner2) {
            z.x2 = p.getX(); z.y2 = p.getY(); z.z2 = p.getZ();
        } else {
            z.x1 = p.getX(); z.y1 = p.getY(); z.z1 = p.getZ();
        }
        // a freshly captured zone belongs to where you stand
        if (mc.theWorld != null) z.dim = mc.theWorld.provider.getDimensionId();
        return z;
    }

    /** Add a zone (copying the selected one's shape), centered on the player if it is the first. */
    public static void addNew(Minecraft mc) {
        Zone z = new Zone();
        z.name = "Zone " + (zones.size() + 1);
        Zone ref = zones.isEmpty() ? null : zones.get(Math.max(0, Math.min(selected, zones.size() - 1)));
        if (ref != null) {
            z.dim = ref.dim;
            z.x1 = ref.x1; z.y1 = ref.y1; z.z1 = ref.z1;
            z.x2 = ref.x2; z.y2 = ref.y2; z.z2 = ref.z2;
            z.vertical = ref.vertical;
            z.offInside = ref.offInside;
        } else if (mc != null && mc.thePlayer != null && mc.theWorld != null) {
            BlockPos p = mc.thePlayer.getPosition();
            z.dim = mc.theWorld.provider.getDimensionId();
            z.x1 = z.x2 = p.getX();
            z.y1 = z.y2 = p.getY();
            z.z1 = z.z2 = p.getZ();
        }
        zones.add(z);
        selected = zones.size() - 1;
    }

    public static void deleteSelected() {
        if (zones.isEmpty()) return;
        zones.remove(Math.max(0, Math.min(selected, zones.size() - 1)));
        if (selected >= zones.size()) selected = zones.size() - 1;
        if (selected < 0) selected = 0;
    }

    // ------------------------------------------------------------ persistence

    /**
     * One string per zone:
     *   name;dim;x1,y1,z1;x2,y2,z2;vertical;offInside
     * (';' and ',' are stripped from names on load; everything else is
     * machine-written, so the format stays stable.)
     */
    public static String[] toArray() {
        List<String> out = new ArrayList<String>(zones.size());
        for (Zone z : zones) {
            out.add(String.format(Locale.US, "%s;%d;%d,%d,%d;%d,%d,%d;%b;%b",
                    z.name.replace(";", "").replace(",", ""),
                    z.dim,
                    z.x1, z.y1, z.z1,
                    z.x2, z.y2, z.z2,
                    z.vertical, z.offInside));
        }
        return out.toArray(new String[out.size()]);
    }

    public static void fromArray(String[] arr) {
        zones.clear();
        selected = 0;
        if (arr == null) return;
        for (String s : arr) {
            if (s == null) continue;
            String[] f = s.split(";", 6);
            if (f.length < 6) continue;
            try {
                Zone z = new Zone();
                z.name = clampName(f[0]);
                z.dim = Integer.parseInt(f[1].trim());
                int[] a = parseInts(f[2]);
                int[] b = parseInts(f[3]);
                if (a == null || b == null) continue;
                z.x1 = a[0]; z.y1 = a[1]; z.z1 = a[2];
                z.x2 = b[0]; z.y2 = b[1]; z.z2 = b[2];
                z.vertical = Boolean.parseBoolean(f[4].trim());
                z.offInside = Boolean.parseBoolean(f[5].trim());
                zones.add(z);
            } catch (Exception ignored) {
                // one malformed line never takes the whole list down
            }
        }
    }

    private static String clampName(String raw) {
        String n = raw == null ? "" : raw.replace(";", "").replace(",", "").trim();
        return n.isEmpty() ? "Zone" : n;
    }

    private static int[] parseInts(String csv) {
        if (csv == null) return null;
        String[] f = csv.split(",");
        if (f.length != 3) return null;
        return new int[] {
                Integer.parseInt(f[0].trim()),
                Integer.parseInt(f[1].trim()),
                Integer.parseInt(f[2].trim())
        };
    }
}
