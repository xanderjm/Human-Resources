package com.xanderjm.humanresources;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.os.Bundle;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

final class AccessibilityLayer {
    private final Activity activity;
    private final long handle;
    private final PassthroughLayout container;
    private Map<String, AccessibilityView> retainedViews = new LinkedHashMap<>();

    private static native boolean nativeAccessibilityAction(long handle, String identityHex, int action);

    AccessibilityLayer(Activity activity, long handle) {
        this.handle = handle;
        this.activity = activity;
        container = new PassthroughLayout(activity);
        container.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        activity.addContentView(container, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    void update(String serializedNodes) {
        activity.runOnUiThread(() -> updateOnUiThread(serializedNodes));
    }

    private void updateOnUiThread(String serializedNodes) {
        Map<String, AccessibilityView> nextViews = new LinkedHashMap<>();
        int previousId = View.NO_ID;
        for (String line : serializedNodes.split("\n")) {
            if (line.isEmpty()) {
                continue;
            }
            String[] fields = line.split("\t", -1);
            if (fields.length != 13) {
                continue;
            }
            try {
                int nodeId = Integer.parseInt(fields[0]);
                int role = Integer.parseInt(fields[1]);
                boolean focusable = fields[2].equals("1");
                boolean disabled = fields[3].equals("1");
                float x = Float.parseFloat(fields[4]);
                float y = Float.parseFloat(fields[5]);
                float width = Float.parseFloat(fields[6]);
                float height = Float.parseFloat(fields[7]);
                String label = decodeHex(fields[8]);
                String value = decodeHex(fields[9]);
                if (nodeId <= 0 || !Float.isFinite(x) || !Float.isFinite(y) || !Float.isFinite(width) || !Float.isFinite(height)
                        || width <= 0 || height <= 0 || label.isEmpty()) {
                    continue;
                }
                String key = fields[10];
                if (key.isEmpty()) continue;
                AccessibilityView view = retainedViews.get(key);
                if (view == null) {
                    view = new AccessibilityView(activity, handle, key);
                    view.setId(View.generateViewId());
                }
                view.role = role;
                view.checked = fields[11].equals("1");
                view.setSelected(fields[12].equals("1"));
                nextViews.put(key, view);
                String description = value.isEmpty() ? label : label + ", " + value;
                if (!Objects.equals(view.getContentDescription(), description)) {
                    view.setContentDescription(description);
                }
                view.setAccessibilityTraversalAfter(previousId);
                previousId = view.getId();
                view.setEnabled(!disabled);
                view.setFocusable(focusable);
                view.setClickable(focusable && !disabled && (role == 2 || role == 6));
                view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
                view.setBackgroundColor(Color.TRANSPARENT);
                FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                        Math.max(1, Math.round(width)), Math.max(1, Math.round(height)));
                params.leftMargin = Math.round(x);
                params.topMargin = Math.round(y);
                if (view.getParent() == null) container.addView(view, params);
                else view.setLayoutParams(params);
            } catch (NumberFormatException ignored) {
                // Invalid native data is omitted rather than exposed as a broken element.
            }
        }
        for (AccessibilityView old : retainedViews.values()) {
            if (!nextViews.containsValue(old)) container.removeView(old);
        }
        retainedViews = nextViews;
        // Retained views keep their accessibility focus. When an element is
        // removed, TalkBack owns selection of the next element in traversal order.
    }

    private static String decodeHex(String encoded) {
        if ((encoded.length() & 1) != 0) {
            return "";
        }
        byte[] bytes = new byte[encoded.length() / 2];
        for (int index = 0; index < encoded.length(); index += 2) {
            int upper = Character.digit(encoded.charAt(index), 16);
            int lower = Character.digit(encoded.charAt(index + 1), 16);
            if (upper < 0 || lower < 0) {
                return "";
            }
            bytes[index / 2] = (byte) ((upper << 4) | lower);
        }
        return new String(bytes, StandardCharsets.UTF_8);
    }

    private static final class PassthroughLayout extends FrameLayout {
        PassthroughLayout(Context context) {
            super(context);
            setWillNotDraw(true);
        }

        @Override
        public boolean dispatchTouchEvent(MotionEvent event) {
            return false;
        }
    }

    void close() {
        if (container.getParent() instanceof ViewGroup) {
            ((ViewGroup) container.getParent()).removeView(container);
        }
        retainedViews.clear();
    }

    private static final class AccessibilityView extends View {
        private final long handle;
        private final String identityHex;
        private int role;
        private boolean checked;

        AccessibilityView(Context context, long handle, String identityHex) {
            super(context);
            this.handle = handle;
            this.identityHex = identityHex;
        }

        @Override
        public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(info);
            if (role == 2) info.setClassName("android.widget.Button");
            else if (role == 6) {
                info.setClassName("android.widget.Switch");
                info.setCheckable(true);
                info.setChecked(checked);
            } else if (role == 5) {
                info.setClassName("android.widget.SeekBar");
                if (isEnabled()) {
                    info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD);
                    info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD);
                }
            } else if (role == 4) info.setClassName("android.widget.ImageView");
            else info.setClassName("android.widget.TextView");
        }

        @Override
        public boolean performAccessibilityAction(int action, Bundle arguments) {
            if (role == 5 && isEnabled() && isFocusable()) {
                if (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD)
                    return nativeAccessibilityAction(handle, identityHex, 1);
                if (action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
                    return nativeAccessibilityAction(handle, identityHex, 2);
            }
            return super.performAccessibilityAction(action, arguments);
        }

        @Override
        public boolean performClick() {
            if (!isEnabled() || !isClickable()) return false;
            super.performClick();
            return nativeAccessibilityAction(handle, identityHex, 0);
        }
    }
}
