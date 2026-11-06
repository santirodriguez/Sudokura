package org.sudokura.prototype;

import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/** Semantic companion to SDL's surface. It never owns gameplay or touch input. */
final class SudokuraAccessibilityView extends View {
    static final int LIMIT = 192;
    static final int STRIDE = 8;
    private static final int NONE = Integer.MIN_VALUE;

    /** Complete immutable publication, copied before crossing onto the UI thread. */
    static final class Snapshot {
        final long generation, attemptHi, attemptLo;
        final int screen, width, height;
        final int[] nodes;
        final String[] labels;
        final String announcement;

        private Snapshot(long generation, long hi, long lo, int screen,
                int width, int height, int[] nodes, String[] labels, String announcement) {
            this.generation = generation;
            attemptHi = hi;
            attemptLo = lo;
            this.screen = screen;
            this.width = width;
            this.height = height;
            this.nodes = nodes;
            this.labels = labels;
            this.announcement = announcement;
        }

        static Snapshot copy(long generation, long hi, long lo, int screen,
                int width, int height, int[] source, String[] sourceLabels,
                String announcement) {
            if (generation <= 0 || width <= 0 || height <= 0 || source == null ||
                    sourceLabels == null || sourceLabels.length > LIMIT ||
                    source.length != sourceLabels.length * STRIDE) return null;
            int[] nodes = source.clone();
            String[] labels = sourceLabels.clone();
            for (int i = 0; i < labels.length; ++i) {
                int p = i * STRIDE;
                if (nodes[p] <= 0 || nodes[p + 1] < 0 || nodes[p + 2] < 0 ||
                        nodes[p + 3] <= 0 || nodes[p + 4] <= 0 ||
                        (long)nodes[p + 1] + nodes[p + 3] > width ||
                        (long)nodes[p + 2] + nodes[p + 4] > height ||
                        (nodes[p + 5] & ~15) != 0 ||
                        (nodes[p + 6] != 0 && nodes[p + 6] != 1) ||
                        (nodes[p + 7] != 0 && nodes[p + 7] != 1) ||
                        labels[i] == null || labels[i].isEmpty() ||
                        labels[i].getBytes(StandardCharsets.UTF_8).length >= 512) return null;
                for (int j = 0; j < i; ++j)
                    if (nodes[j * STRIDE] == nodes[p]) return null;
            }
            String notice = announcement == null ? "" : announcement;
            if (notice.getBytes(StandardCharsets.UTF_8).length >= 512) return null;
            return new Snapshot(generation, hi, lo, screen, width, height,
                    nodes, labels, notice);
        }

        int index(int id) {
            for (int i = 0; i < labels.length; ++i)
                if (nodes[i * STRIDE] == id) return i;
            return -1;
        }
    }

    private final PrototypeActivity activity;
    private final AccessibilityManager manager;
    private final Provider provider = new Provider();
    private Snapshot snapshot;
    private long lastGeneration;
    private int focused = NONE;
    private int hovered = NONE;
    private boolean available = true;

    SudokuraAccessibilityView(PrototypeActivity activity) {
        super(activity);
        this.activity = activity;
        manager = (AccessibilityManager)activity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        setFocusable(false);
        setClickable(false);
        setWillNotDraw(true);
    }

    void setAvailable(boolean value) {
        if (available == value) return;
        clearFocusAndHover();
        available = value;
        if (!value) snapshot = null;
        sendEvent(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, null);
    }

    void dispose() {
        clearFocusAndHover();
        available = false;
        snapshot = null;
    }

    void publish(Snapshot next) {
        if (next.generation <= lastGeneration) return;
        lastGeneration = next.generation;
        // Publications received while paused cannot resurrect concealed cells.
        if (!available) return;
        boolean contextChanged = snapshot == null || snapshot.screen != next.screen ||
                snapshot.attemptHi != next.attemptHi || snapshot.attemptLo != next.attemptLo ||
                snapshot.width != next.width || snapshot.height != next.height;
        boolean contentChanged = contextChanged || !Arrays.equals(snapshot.nodes, next.nodes) ||
                !Arrays.equals(snapshot.labels, next.labels);
        if (contextChanged || (focused != NONE && next.index(focused) < 0)) clearFocusAndHover();
        String previousNotice = snapshot == null ? "" : snapshot.announcement;
        snapshot = next;
        // A generation-only rebind updates command identity silently.
        if (contentChanged) sendEvent(AccessibilityNodeProvider.HOST_VIEW_ID,
                contextChanged ? AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED :
                        AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, null);
        if (!next.announcement.isEmpty() && !next.announcement.equals(previousNotice))
            sendEvent(AccessibilityNodeProvider.HOST_VIEW_ID,
                    AccessibilityEvent.TYPE_ANNOUNCEMENT, next.announcement);
    }

    private boolean visible() {
        return available && snapshot != null && isShown() && getWindowVisibility() == VISIBLE;
    }

    private void clearFocusAndHover() {
        if (focused != NONE)
            sendEvent(focused, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED, null);
        if (hovered != NONE) sendEvent(hovered, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT, null);
        focused = hovered = NONE;
    }

    private void sendEvent(int id, int type, String announcement) {
        if (manager == null || !manager.isEnabled() || getParent() == null) return;
        AccessibilityEvent event = AccessibilityEvent.obtain(type);
        event.setPackageName(getContext().getPackageName());
        event.setClassName(View.class.getName());
        event.setSource(this, id);
        if (type == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
        int index = snapshot == null ? -1 : snapshot.index(id);
        if (announcement != null) event.getText().add(announcement);
        else if (index >= 0) event.setContentDescription(snapshot.labels[index]);
        else if (type == AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED &&
                snapshot != null && snapshot.labels.length > 0)
            event.setContentDescription(snapshot.labels[0]);
        getParent().requestSendAccessibilityEvent(this, event);
    }

    private Rect bounds(int index) {
        int p = index * STRIDE;
        return new Rect(
                (int)((long)snapshot.nodes[p + 1] * getWidth() / snapshot.width),
                (int)((long)snapshot.nodes[p + 2] * getHeight() / snapshot.height),
                (int)(((long)snapshot.nodes[p + 1] + snapshot.nodes[p + 3]) * getWidth() / snapshot.width),
                (int)(((long)snapshot.nodes[p + 2] + snapshot.nodes[p + 4]) * getHeight() / snapshot.height));
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        clearFocusAndHover();
        sendEvent(AccessibilityNodeProvider.HOST_VIEW_ID,
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED, null);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return provider;
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        if (!visible() || manager == null || !manager.isEnabled() ||
                !manager.isTouchExplorationEnabled()) return false;
        int next = NONE;
        if (event.getActionMasked() != MotionEvent.ACTION_HOVER_EXIT) {
            for (int i = snapshot.labels.length - 1; i >= 0; --i)
                if (bounds(i).contains((int)event.getX(), (int)event.getY())) {
                    next = snapshot.nodes[i * STRIDE];
                    break;
                }
        }
        if (next != hovered) {
            if (hovered != NONE) sendEvent(hovered, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT, null);
            hovered = next;
            if (next != NONE) sendEvent(next, AccessibilityEvent.TYPE_VIEW_HOVER_ENTER, null);
        }
        return next != NONE || event.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT;
    }

    private final class Provider extends AccessibilityNodeProvider {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int id) {
            if (id == HOST_VIEW_ID) {
                AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(SudokuraAccessibilityView.this);
                onInitializeAccessibilityNodeInfo(info);
                info.setClassName("android.view.ViewGroup");
                if (visible()) for (int i = 0; i < snapshot.labels.length; ++i)
                    info.addChild(SudokuraAccessibilityView.this, snapshot.nodes[i * STRIDE]);
                return info;
            }
            if (!visible()) return null;
            int i = snapshot.index(id);
            if (i < 0) return null;
            int p = i * STRIDE;
            int actions = snapshot.nodes[p + 5];
            AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
            info.setSource(SudokuraAccessibilityView.this, id);
            info.setParent(SudokuraAccessibilityView.this);
            info.setPackageName(getContext().getPackageName());
            info.setClassName((actions & 1) != 0 ? "android.widget.Button" : "android.widget.TextView");
            info.setContentDescription(snapshot.labels[i]);
            info.setEnabled(snapshot.nodes[p + 6] != 0);
            info.setSelected(snapshot.nodes[p + 7] != 0);
            info.setFocusable(true);
            info.setAccessibilityFocused(id == focused);
            Rect parent = bounds(i);
            info.setBoundsInParent(parent);
            int[] location = new int[2];
            getLocationOnScreen(location);
            Rect screen = new Rect(parent);
            screen.offset(location[0], location[1]);
            Rect actual = new Rect();
            boolean onScreen = getLocalVisibleRect(actual);
            actual.offset(location[0], location[1]);
            onScreen = onScreen && screen.intersect(actual);
            info.setBoundsInScreen(screen);
            info.setVisibleToUser(onScreen);
            if (i > 0) info.setTraversalAfter(SudokuraAccessibilityView.this, snapshot.nodes[(i - 1) * STRIDE]);
            if (i + 1 < snapshot.labels.length)
                info.setTraversalBefore(SudokuraAccessibilityView.this, snapshot.nodes[(i + 1) * STRIDE]);
            info.addAction(id == focused ? AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS :
                    AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
            if (snapshot.nodes[p + 6] != 0) {
                if ((actions & 1) != 0) {
                    info.setClickable(true);
                    info.addAction(AccessibilityNodeInfo.ACTION_CLICK);
                }
                if ((actions & 8) != 0) {
                    info.setLongClickable(true);
                    info.addAction(AccessibilityNodeInfo.ACTION_LONG_CLICK);
                }
                if ((actions & 2) != 0) info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
                if ((actions & 4) != 0) info.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
                info.setScrollable((actions & 6) != 0);
            }
            return info;
        }

        @Override
        public AccessibilityNodeInfo findFocus(int focus) {
            return focus == AccessibilityNodeInfo.FOCUS_ACCESSIBILITY && focused != NONE ?
                    createAccessibilityNodeInfo(focused) : null;
        }

        @Override
        public List<AccessibilityNodeInfo> findAccessibilityNodeInfosByText(String text, int id) {
            List<AccessibilityNodeInfo> found = new ArrayList<>();
            if (!visible() || text == null) return found;
            String query = text.toLowerCase(Locale.ROOT);
            for (int i = 0; i < snapshot.labels.length; ++i)
                if ((id == HOST_VIEW_ID || snapshot.nodes[i * STRIDE] == id) &&
                        snapshot.labels[i].toLowerCase(Locale.ROOT).contains(query))
                    found.add(createAccessibilityNodeInfo(snapshot.nodes[i * STRIDE]));
            return found;
        }

        @Override
        public boolean performAction(int id, int action, Bundle arguments) {
            if (!visible()) return false;
            int i = snapshot.index(id);
            if (i < 0) return false;
            if (action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS) {
                if (focused == id) return false;
                if (focused != NONE)
                    sendEvent(focused, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED, null);
                focused = id;
                sendEvent(id, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED, null);
                return true;
            }
            if (action == AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS) {
                if (focused != id) return false;
                focused = NONE;
                sendEvent(id, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED, null);
                return true;
            }
            int semantic = action == AccessibilityNodeInfo.ACTION_CLICK ? 1 :
                    action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD ? 2 :
                    action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD ? 4 :
                    action == AccessibilityNodeInfo.ACTION_LONG_CLICK ? 8 : 0;
            int p = i * STRIDE;
            if (semantic == 0 || snapshot.nodes[p + 6] == 0 ||
                    (snapshot.nodes[p + 5] & semantic) == 0) return false;
            return activity.submitAccessibilityCommand(snapshot.generation, snapshot.attemptHi,
                    snapshot.attemptLo, snapshot.screen, id, semantic);
        }
    }
}
