package com.xanderjm.humanresources;

import android.animation.ValueAnimator;
import android.content.pm.ApplicationInfo;
import org.libsdl.app.SDLActivity;

public final class HumanResourcesActivity extends SDLActivity {
    private AccessibilityLayer accessibility;
    private long accessibilityHandle;
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
        runOnUiThread(() -> {
            if (handle < accessibilityHandle) return;
            if (accessibility == null || handle != accessibilityHandle) {
                if (accessibility != null) accessibility.close();
                accessibility = new AccessibilityLayer(this, handle);
                accessibilityHandle = handle;
            }
            accessibility.update(nodes);
        });
    }

    public void accessibilityClose(long handle) {
        runOnUiThread(() -> {
            if (handle == accessibilityHandle && accessibility != null) {
                accessibility.close();
                accessibility = null;
            }
        });
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
        haptics.setForeground(false);
        haptics.setActive(false);
        if (accessibility != null) {
            accessibility.close();
            accessibility = null;
        }
        super.onDestroy();
    }
}
