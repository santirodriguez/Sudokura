package org.sudokura.prototype;

import android.annotation.TargetApi;
import android.content.res.Configuration;
import android.view.KeyEvent;
import android.view.HapticFeedbackConstants;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowInsets;
import android.view.ViewGroup;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.libsdl.app.SDLActivity;

/** SDL owns the game and persistence; Java supplies Android surface and system UI. */
public final class PrototypeActivity extends SDLActivity {
    private Object backCallback;
    private ScrollView rendererRecovery;
    private volatile int recoveryGeneration;
    private SudokuraAccessibilityView accessibilityView;
    private final Object accessibilityPublicationLock = new Object();
    private SudokuraAccessibilityView.Snapshot pendingAccessibility;
    private boolean accessibilityPublicationPosted;
    private boolean accessibilityDestroyed;
    private boolean accessibilityResumed;

    private static native boolean nativeAccessibilityCommand(long generation,
            long attemptHi, long attemptLo, int screen, int nodeId, int action);

    boolean submitAccessibilityCommand(long generation, long attemptHi, long attemptLo,
            int screen, int nodeId, int action) {
        return !mBrokenLibraries && !accessibilityDestroyed && accessibilityResumed &&
                rendererRecovery == null && nativeAccessibilityCommand(generation,
                        attemptHi, attemptLo, screen, nodeId, action);
    }

    /* One pending immutable publication and one UI runnable, even if native
       produces multiple changes while the UI thread is occupied. */
    public void publishSudokuraAccessibility(long generation, long attemptHi,
            long attemptLo, int screen, int logicalWidth, int logicalHeight,
            int[] nodes, String[] labels, String announcement) {
        SudokuraAccessibilityView.Snapshot publication = SudokuraAccessibilityView.Snapshot.copy(
                generation, attemptHi, attemptLo, screen, logicalWidth, logicalHeight,
                nodes, labels, announcement);
        if (publication == null) return;
        synchronized (accessibilityPublicationLock) {
            if (accessibilityDestroyed || (pendingAccessibility != null &&
                    generation <= pendingAccessibility.generation)) return;
            pendingAccessibility = publication;
            if (accessibilityPublicationPosted) return;
            accessibilityPublicationPosted = true;
        }
        runOnUiThread(this::drainAccessibilityPublication);
    }

    private void drainAccessibilityPublication() {
        SudokuraAccessibilityView.Snapshot publication;
        synchronized (accessibilityPublicationLock) {
            publication = pendingAccessibility;
            pendingAccessibility = null;
            accessibilityPublicationPosted = false;
            if (accessibilityDestroyed) return;
        }
        if (accessibilityView != null && publication != null)
            accessibilityView.publish(publication);
    }

    private void updateAccessibilityAvailability() {
        if (accessibilityView != null)
            accessibilityView.setAvailable(accessibilityResumed && rendererRecovery == null);
    }

    @Override
    protected void onPause() {
        accessibilityResumed = false;
        updateAccessibilityAvailability();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        accessibilityResumed = true;
        updateAccessibilityAvailability();
    }

    private static native void nativeRendererRecovery(int generation, boolean exit);

    /* A bounded fallback when SDL cannot draw. Buttons enqueue commands; the
       SDL owner still validates the generation and owns retry/save/exit. */
    public void showSudokuraRendererRecovery(int generation, boolean visible,
            String message, String retryLabel, String exitLabel) {
        recoveryGeneration = generation;
        runOnUiThread(() -> {
            if (generation != recoveryGeneration || isFinishing() || isDestroyed()) return;
            if (rendererRecovery != null && mLayout != null) {
                mLayout.removeView(rendererRecovery);
                rendererRecovery = null;
            }
            updateAccessibilityAvailability();
            if (!visible || mLayout == null) return;
            final int padding = Math.round(24 * getResources().getDisplayMetrics().density);
            LinearLayout content = new LinearLayout(this);
            content.setOrientation(LinearLayout.VERTICAL);
            content.setGravity(Gravity.CENTER);
            content.setPadding(padding, padding, padding, padding);
            TextView text = new TextView(this);
            text.setText(message);
            text.setTextSize(20);
            text.setTextColor(0xffeeeeee);
            text.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
            content.addView(text);
            Button retry = new Button(this);
            Button exit = new Button(this);
            retry.setText(retryLabel);
            exit.setText(exitLabel);
            int minimum = Math.round(48 * getResources().getDisplayMetrics().density);
            for (Button button : new Button[] {retry, exit}) {
                button.setMinHeight(minimum);
                content.addView(button, new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            }
            retry.setOnClickListener(view -> {
                if (recoveryGeneration != generation) return;
                retry.setEnabled(false);
                exit.setEnabled(false);
                nativeRendererRecovery(generation, false);
            });
            exit.setOnClickListener(view -> {
                if (recoveryGeneration != generation) return;
                retry.setEnabled(false);
                exit.setEnabled(false);
                nativeRendererRecovery(generation, true);
            });
            rendererRecovery = new ScrollView(this);
            rendererRecovery.setBackgroundColor(0xff181818);
            rendererRecovery.setFillViewport(true);
            rendererRecovery.setClickable(true);
            rendererRecovery.addView(content);
            updateAccessibilityAvailability();
            mLayout.addView(rendererRecovery, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            retry.requestFocus();
        });
    }

    private void sendBackToNative() {
        if (mBrokenLibraries || mSDLThread == null) return;
        /* One key-down/up pair, not Activity.finish or a second UI dispatcher. */
        onNativeKeyDown(KeyEvent.KEYCODE_BACK);
        onNativeKeyUp(KeyEvent.KEYCODE_BACK);
    }

    @TargetApi(33)
    private static final class BackApi33 {
        static OnBackInvokedCallback install(PrototypeActivity activity) {
            final OnBackInvokedCallback callback = activity::sendBackToNative;
            activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            return callback;
        }

        static void remove(PrototypeActivity activity, Object callback) {
            activity.getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(
                    (OnBackInvokedCallback) callback);
        }
    }

    /* This is called on SDL's owner thread; View feedback runs on the UI
       thread and honors Android's accessibility/touch-feedback preferences. */
    public void performSudokuraHaptic() {
        runOnUiThread(() -> {
            if (mLayout != null && mLayout.isAttachedToWindow()) {
                mLayout.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
            }
        });
    }

    @Override
    public void onBackPressed() {
        sendBackToNative(); /* Fallback for three-button navigation on older OS. */
    }

    @Override
    protected void onDestroy() {
        recoveryGeneration = 0;
        synchronized (accessibilityPublicationLock) {
            accessibilityDestroyed = true;
            pendingAccessibility = null;
        }
        if (accessibilityView != null) {
            accessibilityView.dispose();
            if (mLayout != null) mLayout.removeView(accessibilityView);
            accessibilityView = null;
        }
        if (rendererRecovery != null && mLayout != null) {
            mLayout.removeView(rendererRecovery);
            rendererRecovery = null;
        }
        if (backCallback != null && Build.VERSION.SDK_INT >= 33) {
            BackApi33.remove(this, backCallback);
            backCallback = null;
        }
        super.onDestroy();
    }
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_ttf", "SDL2_mixer", "main"};
    }

    private void publishDensityAndSlop() {
        final float density = getResources().getDisplayMetrics().density;
        final int slopPx = ViewConfiguration.get(this).getScaledTouchSlop();
        if (!mBrokenLibraries) {
            nativeSetenv("SUDOKURA_ANDROID_DENSITY", Float.toString(density));
            nativeSetenv("SUDOKURA_ANDROID_TOUCH_SLOP_PX", Integer.toString(slopPx));
        }
    }

    private static int[] usableInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= 30) {
            final Insets safe = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            return new int[] {safe.left, safe.top, safe.right, safe.bottom};
        }
        int left = insets.getSystemWindowInsetLeft();
        int top = insets.getSystemWindowInsetTop();
        int right = insets.getSystemWindowInsetRight();
        int bottom = insets.getSystemWindowInsetBottom();
        final DisplayCutout cutout = insets.getDisplayCutout();
        if (cutout != null) {
            left = Math.max(left, cutout.getSafeInsetLeft());
            top = Math.max(top, cutout.getSafeInsetTop());
            right = Math.max(right, cutout.getSafeInsetRight());
            bottom = Math.max(bottom, cutout.getSafeInsetBottom());
        }
        return new int[] {left, top, right, bottom};
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        if (mBrokenLibraries || mLayout == null) return;
        publishDensityAndSlop();
        if (mSurface != null) mSurface.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        accessibilityView = new SudokuraAccessibilityView(this);
        mLayout.addView(accessibilityView, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        updateAccessibilityAvailability();

        /* SDL sees only its padded usable SurfaceView: never subtract these
           insets again when computing native board and pointer coordinates. */
        mLayout.setOnApplyWindowInsetsListener((View view, WindowInsets insets) -> {
            final int[] safe = usableInsets(insets);
            if (view.getPaddingLeft() != safe[0] ||
                view.getPaddingTop() != safe[1] ||
                view.getPaddingRight() != safe[2] ||
                view.getPaddingBottom() != safe[3]) {
                view.setPadding(safe[0], safe[1], safe[2], safe[3]);
            }
            return insets;
        });
        mLayout.requestApplyInsets();
        if (Build.VERSION.SDK_INT >= 33) backCallback = BackApi33.install(this);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        publishDensityAndSlop();
        if (mLayout != null) mLayout.requestApplyInsets();
    }
}
