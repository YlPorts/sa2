package org.libsdl.app;

import android.os.Bundle;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public class SA2Activity extends SDLActivity {
    private static final String STAGE_FILE = "sa_startup_stage.txt";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        writeStage("java_activity_onCreate");
        applyImmersiveMode();
        super.onCreate(savedInstanceState);
        applyImmersiveMode();
    }

    @Override
    protected void onResume() {
        writeStage("java_activity_onResume");
        super.onResume();
        applyImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveMode();
        }
    }

    private void applyImmersiveMode() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

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

    @Override
    protected String[] getArguments() {
        return new String[] { getFilesDir().getAbsolutePath() };
    }

    private void writeStage(String value) {
        try {
            File file = new File(getFilesDir(), STAGE_FILE);
            try (FileOutputStream out = new FileOutputStream(file, false)) {
                out.write(value.getBytes(StandardCharsets.UTF_8));
                out.flush();
            }
        } catch (Exception ignored) {
        }
    }
}
