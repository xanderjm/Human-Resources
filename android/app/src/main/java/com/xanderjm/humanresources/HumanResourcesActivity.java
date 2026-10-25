package com.xanderjm.humanresources;

import android.animation.ValueAnimator;
import android.content.pm.ApplicationInfo;
import java.util.Objects;
import org.libsdl.app.SDLActivity;

public final class HumanResourcesActivity extends SDLActivity {
    private AccessibilityLayer accessibility;
    private long accessibilityHandle;
    private String appliedAccessibilityNodes;
    private final Object accessibilityPublicationLock = new Object();
    private long latestAccessibilityHandle;
    private String latestAccessibilityNodes;
    private long accessibilityClosedThrough;
    private boolean accessibilityPublicationPosted;
    private boolean accessibilityDestroyed;
    private final HapticsLayer haptics = new HapticsLayer(this);

    public boolean hapticsAvailable() { return haptics.available(); }
    public void hapticsSetActive(boolean active) { haptics.setActive(active); }
    public void hapticsPerform(int cue) { haptics.perform(cue); }

    @Override
    protected String[] getArguments() {
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            String mode = getIntent().getStringExtra("hr_qa_continuation");
            String slot = getIntent().getStringExtra("hr_qa_continuation_slot");
            if (mode != null && slot != null
                    && (mode.equals("prepare-choice") || mode.equals("inspect")
                        || mode.equals("resume") || mode.equals("bank"))
                    && slot.matches("[A-Za-z0-9_-]{1,32}")) {
                return new String[] {"--qa-continuation", mode, "--qa-continuation-slot", slot};
            }
            if (getIntent().getBooleanExtra("hr_playthrough_smoke", false)) {
                return new String[] {"--playthrough-smoke"};
            }
        }
        return super.getArguments();
    }

    public void accessibilityUpdate(long handle, String nodes) {
        final boolean post;
        synchronized (accessibilityPublicationLock) {
            if (accessibilityDestroyed || handle <= accessibilityClosedThrough
                    || handle < latestAccessibilityHandle) return;
            if (handle == latestAccessibilityHandle && Objects.equals(nodes, latestAccessibilityNodes)) return;
            latestAccessibilityHandle = handle;
            latestAccessibilityNodes = nodes;
            post = !accessibilityPublicationPosted;
            accessibilityPublicationPosted = true;
        }
        if (post) runOnUiThread(this::drainAccessibilityPublication);
    }

    public void accessibilityClose(long handle) {
        final boolean post;
        synchronized (accessibilityPublicationLock) {
            if (accessibilityDestroyed || handle <= accessibilityClosedThrough) return;
            accessibilityClosedThrough = handle;
            if (latestAccessibilityHandle <= handle) latestAccessibilityNodes = null;
            post = !accessibilityPublicationPosted;
            accessibilityPublicationPosted = true;
        }
        if (post) runOnUiThread(this::drainAccessibilityPublication);
    }

    private void drainAccessibilityPublication() {
        final long handle;
        final long closedThrough;
        final String nodes;
        synchronized (accessibilityPublicationLock) {
            // A producer may queue one newer drain while this one is applying.
            accessibilityPublicationPosted = false;
            if (accessibilityDestroyed) return;
            handle = latestAccessibilityHandle;
            nodes = latestAccessibilityNodes;
            closedThrough = accessibilityClosedThrough;
        }
        if (accessibility != null && accessibilityHandle <= closedThrough) {
            accessibility.close();
            accessibility = null;
            appliedAccessibilityNodes = null;
        }
        if (nodes == null || handle <= closedThrough || handle < accessibilityHandle) return;
        if (accessibility == null || handle != accessibilityHandle) {
            if (accessibility != null) accessibility.close();
            accessibility = new AccessibilityLayer(this, handle);
            accessibilityHandle = handle;
            appliedAccessibilityNodes = null;
        }
        if (!Objects.equals(nodes, appliedAccessibilityNodes)) {
            accessibility.update(nodes);
            appliedAccessibilityNodes = nodes;
        }
    }

    public float accessibilityTextScale() {
        return getResources().getConfiguration().fontScale;
    }

    public boolean accessibilityReducedMotion() {
        return !ValueAnimator.areAnimatorsEnabled();
    }

    @Override
    protected void onResume() {
        super.onResume();
        haptics.setForeground(true);
    }

    @Override
    protected void onPause() {
        haptics.setForeground(false);
        haptics.setActive(false);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        synchronized (accessibilityPublicationLock) {
            accessibilityDestroyed = true;
            latestAccessibilityNodes = null;
        }
        haptics.setForeground(false);
        haptics.setActive(false);
        if (accessibility != null) {
            accessibility.close();
            accessibility = null;
        }
        appliedAccessibilityNodes = null;
        super.onDestroy();
    }
}
