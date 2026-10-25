"""Compile the real Activity against a deterministic UI queue when JDK 17 exists.

These stubs test publication/lifetime ownership, not Android rendering or JNI.
Android SDK compilation and lint remain separate qualification gates.
"""
from pathlib import Path
import shutil
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from game_support.toolchains import java_17_executable


STUBS = {
    "android/animation/ValueAnimator.java": """
package android.animation;
public final class ValueAnimator { public static boolean areAnimatorsEnabled() { return true; } }
""",
    "android/content/pm/ApplicationInfo.java": """
package android.content.pm;
public final class ApplicationInfo { public static final int FLAG_DEBUGGABLE = 2; public int flags; }
""",
    "org/libsdl/app/SDLActivity.java": """
package org.libsdl.app;
import java.util.ArrayDeque;
import android.content.pm.ApplicationInfo;
public class SDLActivity {
    private final ArrayDeque<Runnable> queue = new ArrayDeque<>();
    private boolean uiThread;
    public int peakQueued;
    public void runOnUiThread(Runnable action) {
        if (uiThread) action.run();
        else { queue.addLast(action); peakQueued = Math.max(peakQueued, queue.size()); }
    }
    public int queued() { return queue.size(); }
    public boolean isUiThread() { return uiThread; }
    public void drainOne() {
        Runnable action = queue.removeFirst();
        onUiThreadNow(action);
    }
    public void drainAll() {
        int count = 0;
        while (!queue.isEmpty()) {
            if (++count > 100) throw new AssertionError("publication queue never settles");
            drainOne();
        }
    }
    public void onUiThreadNow(Runnable action) {
        boolean previous = uiThread;
        uiThread = true;
        try { action.run(); } finally { uiThread = previous; }
    }
    public void fromProducer(Runnable action) {
        boolean previous = uiThread;
        uiThread = false;
        try { action.run(); } finally { uiThread = previous; }
    }
    public ApplicationInfo getApplicationInfo() { return new ApplicationInfo(); }
    public Intent getIntent() { return new Intent(); }
    public Resources getResources() { return new Resources(); }
    public static final class Intent {
        public String getStringExtra(String key) { return null; }
        public boolean getBooleanExtra(String key, boolean fallback) { return fallback; }
    }
    public static final class Resources { public Configuration getConfiguration() { return new Configuration(); } }
    public static final class Configuration { public float fontScale = 1; }
    protected String[] getArguments() { return new String[0]; }
    protected void onResume() {}
    protected void onPause() {}
    protected void onDestroy() {}
}
""",
    "com/xanderjm/humanresources/HapticsLayer.java": """
package com.xanderjm.humanresources;
final class HapticsLayer {
    HapticsLayer(HumanResourcesActivity owner) {}
    boolean available() { return false; }
    void setActive(boolean active) {}
    void perform(int cue) {}
    void setForeground(boolean foreground) {}
}
""",
    "com/xanderjm/humanresources/AccessibilityLayer.java": """
package com.xanderjm.humanresources;
final class AccessibilityLayer {
    static AccessibilityLayer current;
    static int creations, updates, closes;
    static Runnable duringUpdate;
    final HumanResourcesActivity owner;
    final long handle;
    String nodes;
    AccessibilityLayer(HumanResourcesActivity owner, long handle) {
        if (!owner.isUiThread()) throw new AssertionError("layer constructed off UI thread");
        this.owner = owner; this.handle = handle; current = this; ++creations;
    }
    void update(String value) {
        // The real Layer calls runOnUiThread again, which executes inline here.
        owner.runOnUiThread(() -> {
            nodes = value; ++updates;
            Runnable callback = duringUpdate; duringUpdate = null;
            if (callback != null) callback.run();
        });
    }
    void close() {
        if (!owner.isUiThread()) throw new AssertionError("layer closed off UI thread");
        ++closes;
        if (current == this) current = null;
    }
}
""",
    "com/xanderjm/humanresources/PublicationHarness.java": r"""
package com.xanderjm.humanresources;
public final class PublicationHarness {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    private static String nodes(String label, int x, boolean disabled) {
        return "100\t2\t1\t" + (disabled ? "1" : "0") + "\t" + x
            + "\t0\t44\t44\t" + label + "\t\tstable-identity\t0\t0\n";
    }
    private static void burst() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        for (int i = 0; i < 10000; ++i) a.accessibilityUpdate(1, nodes("label" + i, i, false));
        check(a.queued() == 1 && a.peakQueued == 1, "burst retains one pending UI runnable");
        a.drainAll();
        check(AccessibilityLayer.updates == 1 && AccessibilityLayer.current.nodes.equals(nodes("label9999",9999,false)), "only latest snapshot is applied");
        a.accessibilityUpdate(1, nodes("label9999",9999,false));
        check(a.queued() == 0, "identical applied snapshot is deduplicated");
    }
    private static void changes() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        a.accessibilityUpdate(2, nodes("A",0,false)); a.drainAll();
        AccessibilityLayer retained = AccessibilityLayer.current;
        for (String value : new String[] {nodes("B",0,false), nodes("B",50,false), nodes("B",50,true)}) {
            a.accessibilityUpdate(2,value); check(a.queued() == 1,"changed label/bounds/flags are published"); a.drainAll();
            check(AccessibilityLayer.current == retained,"same handle retains element owner/focus identity");
        }
        check(AccessibilityLayer.updates == 4 && AccessibilityLayer.creations == 1,"no changed snapshot is incorrectly deduplicated");
    }
    private static void duringDrain() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        AccessibilityLayer.duringUpdate = () -> a.fromProducer(() -> {
            a.accessibilityUpdate(3,nodes("B",1,false));
            a.accessibilityUpdate(3,nodes("C",2,false));
            check(a.queued() == 1,"producer during drain schedules exactly one next drain");
        });
        a.accessibilityUpdate(3,nodes("A",0,false)); a.drainOne();
        check(a.queued() == 1 && AccessibilityLayer.updates == 1,"in-flight drain leaves only one pending update");
        a.drainAll();
        check(a.peakQueued == 1 && AccessibilityLayer.updates == 2 && AccessibilityLayer.current.nodes.equals(nodes("C",2,false)),"reposted drain publishes latest concurrent state");
    }
    private static void closeOrdering() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        a.accessibilityUpdate(5,nodes("old",0,false)); a.drainAll();
        a.accessibilityUpdate(6,nodes("new",0,false)); a.accessibilityClose(5);
        a.accessibilityUpdate(5,nodes("stale",0,false));
        check(a.queued() == 1,"close shares the publication drain"); a.drainAll();
        check(AccessibilityLayer.current.handle == 6 && AccessibilityLayer.closes == 1,"old close preserves newer handle");
        a.accessibilityClose(4); check(a.queued() == 0,"older duplicate close does no work");
        a.accessibilityClose(6); a.accessibilityUpdate(6,nodes("resurrection",0,false)); a.drainAll();
        check(AccessibilityLayer.current == null && AccessibilityLayer.creations == 2,"same handle cannot recreate a closed layer");
        a.accessibilityUpdate(7,nodes("next",0,false)); a.drainAll();
        check(AccessibilityLayer.current.handle == 7,"newer handle can replace closed older session");
        a.accessibilityUpdate(8,nodes("next",0,false)); a.drainAll();
        check(AccessibilityLayer.current.handle == 8 && AccessibilityLayer.creations == 4,"identical payload for a newer handle still replaces the old owner");
    }
    private static void closeBeforeFirstDrain() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        a.accessibilityUpdate(10,nodes("pending",0,false)); a.accessibilityClose(10);
        a.accessibilityUpdate(9,nodes("older",0,false)); a.accessibilityUpdate(10,nodes("closed",0,false));
        a.drainAll(); check(AccessibilityLayer.creations == 0,"queued closed or older snapshot never creates a layer");
        a.accessibilityUpdate(11,nodes("new",0,false)); a.drainAll();
        check(AccessibilityLayer.current.handle == 11,"valid newer state remains available after closure");
    }
    private static void destroy() {
        HumanResourcesActivity a = new HumanResourcesActivity();
        a.accessibilityUpdate(12,nodes("live",0,false)); a.drainAll();
        a.accessibilityUpdate(12,nodes("pending",1,false));
        a.onUiThreadNow(a::onDestroy); a.drainAll();
        check(AccessibilityLayer.current == null && AccessibilityLayer.updates == 1 && AccessibilityLayer.closes == 1,"destroy invalidates queued state and closes live layer");
        a.accessibilityUpdate(13,nodes("dead activity",0,false)); a.accessibilityClose(13);
        check(a.queued() == 0,"post-destroy update and close cannot enqueue or resurrect");
        HumanResourcesActivity recreated = new HumanResourcesActivity();
        recreated.accessibilityUpdate(12,nodes("live",0,false)); recreated.drainAll();
        check(AccessibilityLayer.current.handle == 12,"new Activity accepts same native session/payload without cross-Activity cache loss");
    }
    public static void main(String[] args) {
        switch (args[0]) {
            case "burst": burst(); break;
            case "changes": changes(); break;
            case "during-drain": duringDrain(); break;
            case "close-ordering": closeOrdering(); break;
            case "close-before-drain": closeBeforeFirstDrain(); break;
            case "destroy": destroy(); break;
            default: throw new AssertionError("unknown test");
        }
        System.out.println("PASS " + args[0]);
    }
}
""",
}


class AndroidAccessibilityPublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Baseline desktop tooling does not require the Android toolchain.
        # Android qualification must execute these contracts with zero skips.
        try:
            cls.java = java_17_executable()
        except RuntimeError as error:
            raise unittest.SkipTest("Android JDK 17 behavior remains unqualified: " + str(error)) from error
        cls.javac = cls.java.with_name("javac.exe" if sys.platform == "win32" else "javac")
        if not cls.javac.is_file() or not os.access(cls.javac, os.X_OK):
            raise unittest.SkipTest("Android JDK 17 behavior remains unqualified: javac is unavailable")
        cls.temporary = tempfile.TemporaryDirectory(prefix="hr accessibility publication ")
        cls.addClassCleanup(cls.temporary.cleanup)
        root = Path(cls.temporary.name)
        for relative, content in STUBS.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
        source = ROOT / "android/app/src/main/java/com/xanderjm/humanresources/HumanResourcesActivity.java"
        shutil.copyfile(source, root / "com/xanderjm/humanresources/HumanResourcesActivity.java")
        cls.classes = root / "classes"
        cls.classes.mkdir()
        compiled = subprocess.run([str(cls.javac), "--release", "17", "-Xlint:all", "-Werror", "-d", str(cls.classes),
                                   *map(str, sorted(root.rglob("*.java")))], capture_output=True, text=True)
        if compiled.returncode:
            raise AssertionError("Actual Activity/stub compilation failed:\n" + compiled.stdout + compiled.stderr)

    def run_contract(self, name):
        result = subprocess.run([str(self.java), "-cp", str(self.classes),
                                 "com.xanderjm.humanresources.PublicationHarness", name],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.strip(), "PASS " + name)

    def test_burst_keeps_only_latest_and_deduplicates(self):
        self.run_contract("burst")

    def test_changed_labels_bounds_flags_preserve_layer_identity(self):
        self.run_contract("changes")

    def test_update_during_drain_reposts_one_latest_snapshot(self):
        self.run_contract("during-drain")

    def test_close_watermark_preserves_newer_and_rejects_resurrection(self):
        self.run_contract("close-ordering")

    def test_close_before_first_drain_cancels_stale_snapshot(self):
        self.run_contract("close-before-drain")

    def test_destroy_cancels_pending_and_activity_recreation_has_no_stale_cache(self):
        self.run_contract("destroy")


if __name__ == "__main__":
    unittest.main()
