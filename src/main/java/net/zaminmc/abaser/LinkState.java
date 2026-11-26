package net.zaminmc.abaser;

import java.util.Map;

/**
 * The latest picture of the Abaser Clicker exe, as the exe itself reports it
 * on stdout (see ClientTickHandler's reader thread).
 *
 * Two streams come back from the exe:
 *
 *   {"config":{...}}  — the canonical settings snapshot, sent after every
 *                       apply and on request; keys are abaser.ini key names;
 *   {"status":{...}}  — a 2 s live line: armed sides, live CPS, the gates.
 *
 * The exe owns the truth; this class only mirrors what it last said. The
 * in-game panel (AbaserConfigGui) reads from here and writes THROUGH the
 * exe — it never edits these values as if it owned them.
 */
final class LinkState {

    /** Last canonical config the exe echoed (abaser.ini key names -> strings). */
    static volatile Map<String, String> config = null;
    static volatile boolean haveConfig = false;

    // live status (every 2 s, from BuildStatusJson on the exe side)
    static volatile boolean leftOn    = false;
    static volatile boolean rightOn   = false;
    static volatile boolean mcFound   = false;
    static volatile boolean guiOpen   = false;
    static volatile boolean holdingBow = false;
    static volatile float   cpsLeft   = 0.0f;
    static volatile float   cpsRight  = 0.0f;

    /** The clicker exe's own build string (from the status line's "ver").
     *  Null while the exe predates the version handshake — i.e. an
     *  outdated exe. The panel shows it; ClientTickHandler's watchdog
     *  replaces outdated exes with the bundled build. */
    static volatile String  version   = null;

    /** Any stdout line has been seen since the exe started. */
    static volatile boolean linkUp = false;

    private LinkState() {}

    static void applyConfigLine(final Map<String, String> cfg) {
        config = cfg;
        haveConfig = true;
    }

    static void reset() {
        config = null;
        haveConfig = false;
        linkUp = false;
        leftOn = rightOn = mcFound = guiOpen = holdingBow = false;
        cpsLeft = cpsRight = 0.0f;
        version = null;
    }
}
