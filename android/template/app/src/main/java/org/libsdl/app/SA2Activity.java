package org.libsdl.app;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Build;
import android.view.View;
import android.view.Display;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class SA2Activity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        super.onCreate(savedInstanceState);
        applyImmersiveMode();
        request60Hz();
    }

    @Override
    protected void onResume() {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        super.onResume();
        applyImmersiveMode();
        request60Hz();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveMode();
            request60Hz();
        }
    }

    private void applyImmersiveMode() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attrs = window.getAttributes();
            attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(attrs);
        }

        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private void request60Hz() {
        Window window = getWindow();
        WindowManager.LayoutParams attrs = window.getAttributes();
        attrs.preferredRefreshRate = 60.0f;

        if (Build.VERSION.SDK_INT >= 23) {
            Display display = getWindowManager().getDefaultDisplay();
            Display.Mode current = display.getMode();
            Display.Mode best = current;
            float bestDiff = Math.abs(current.getRefreshRate() - 60.0f);

            for (Display.Mode mode : display.getSupportedModes()) {
                if (mode.getPhysicalWidth() != current.getPhysicalWidth()
                        || mode.getPhysicalHeight() != current.getPhysicalHeight()) {
                    continue;
                }

                float diff = Math.abs(mode.getRefreshRate() - 60.0f);
                if (diff < bestDiff) {
                    best = mode;
                    bestDiff = diff;
                }
            }

            if (bestDiff < 1.0f) {
                attrs.preferredDisplayModeId = best.getModeId();
            }
        }

        window.setAttributes(attrs);
    }

    @Override
    protected String[] getArguments() {
        return new String[] { getFilesDir().getAbsolutePath() };
    }

}