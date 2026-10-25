package com.xanderjm.humanresources;

import android.app.Activity;
import android.os.Build;
import android.os.SystemClock;
import android.os.Vibrator;
import android.view.HapticFeedbackConstants;
import android.view.View;

/** Optional system feedback. There is at most one queued UI task and no raw vibration. */
final class HapticsLayer {
    private final Activity activity;
    private boolean active;
    private boolean foreground;
    private boolean pending;
    private int pendingCue;
    private long epoch;
    private long requestedAt;

    HapticsLayer(Activity owner) { activity = owner; }

    boolean available() {
        try {
            Vibrator vibrator = activity.getSystemService(Vibrator.class);
            return vibrator != null && vibrator.hasVibrator();
        } catch (RuntimeException unavailable) {
            return false;
        }
    }

    synchronized void setActive(boolean value) {
        active = value;
        if (!value) {
            ++epoch;
            pending = false;
        }
    }

    synchronized void setForeground(boolean value) {
        foreground = value;
        if (!value) {
            ++epoch;
            pending = false;
        }
    }

    synchronized void perform(int cue) {
        if (!active || !foreground || cue < 0 || cue > 4) return;
        if (pending) {
            pendingCue = Math.max(pendingCue, cue);
            return;
        }
        pending = true;
        pendingCue = cue;
        requestedAt = SystemClock.uptimeMillis();
        final long ticket = epoch;
        try {
            activity.runOnUiThread(() -> drain(ticket));
        } catch (RuntimeException unavailable) {
            pending = false;
        }
    }

    private synchronized void drain(long ticket) {
        if (ticket != epoch) return;
        pending = false;
        if (!active || !foreground || SystemClock.uptimeMillis() - requestedAt > 150) return;
        try {
            performOnView();
        } catch (RuntimeException unavailable) {
            // Optional feedback failure must not interrupt gameplay or lifecycle.
        }
    }

    private void performOnView() {
        if (activity.isFinishing() || activity.isDestroyed()
                || !activity.hasWindowFocus() || activity.getWindow() == null) return;
        View view = activity.getWindow().getDecorView();
        if (!view.isAttachedToWindow()) return;
        int effect;
        switch (pendingCue) {
            case 0:
                effect = Build.VERSION.SDK_INT >= 27
                        ? HapticFeedbackConstants.TEXT_HANDLE_MOVE : HapticFeedbackConstants.CLOCK_TICK;
                break;
            case 1: effect = HapticFeedbackConstants.CLOCK_TICK; break;
            case 2: effect = HapticFeedbackConstants.LONG_PRESS; break;
            case 3:
                effect = Build.VERSION.SDK_INT >= 30
                        ? HapticFeedbackConstants.CONFIRM : HapticFeedbackConstants.VIRTUAL_KEY;
                break;
            default:
                effect = Build.VERSION.SDK_INT >= 30
                        ? HapticFeedbackConstants.REJECT : HapticFeedbackConstants.LONG_PRESS;
                break;
        }
        // No IGNORE_* flags: the OS's touch-feedback setting remains authoritative.
        view.performHapticFeedback(effect);
    }
}
