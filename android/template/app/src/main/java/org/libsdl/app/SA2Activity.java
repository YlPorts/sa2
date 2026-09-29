package org.libsdl.app;

import android.os.Bundle;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public class SA2Activity extends SDLActivity {
    private static final String STAGE_FILE = "sa_startup_stage.txt";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        writeStage("java_activity_onCreate");
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onResume() {
        writeStage("java_activity_onResume");
        super.onResume();
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
