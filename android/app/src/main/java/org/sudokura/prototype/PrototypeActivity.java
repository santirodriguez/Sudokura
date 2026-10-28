package org.sudokura.prototype;

import android.content.res.Configuration;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowInsets;
import org.libsdl.app.SDLActivity;

/** SDL remains the game owner. Java only supplies current Android surface metrics. */
public final class PrototypeActivity extends SDLActivity {
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
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        publishDensityAndSlop();
        if (mLayout != null) mLayout.requestApplyInsets();
    }
}
