package org.libsdl.app;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.graphics.Color;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class RomImportActivity extends Activity {
    private static final int PICK_ROM = 1;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private Button select;
    private ProgressBar progress;
    private boolean busy;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        if (!getPackageName().startsWith("com.ylports.sonicadvance1")) {
            if (CrashReportUi.showPreviousCrash(this, this::startGame)) return;
            startGame();
            return;
        }
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(dp(32), dp(16), dp(32), dp(16));
        layout.setBackgroundColor(Color.rgb(14, 24, 45));
        TextView title = new TextView(this);
        title.setText("Sonic Advance");
        title.setTextSize(30);
        title.setTextColor(Color.WHITE);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);
        status = new TextView(this);
        status.setTextSize(16);
        status.setTextColor(Color.rgb(211, 225, 246));
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, dp(16), 0, dp(16));
        layout.addView(status);
        progress = new ProgressBar(this);
        layout.addView(progress);
        select = new Button(this);
        select.setText("Seleccionar ROM .gba");
        select.setOnClickListener(view -> pickRom());
        layout.addView(select);
        setContentView(layout);
        setBusy(true, "Comprobando datos del juego…");
        worker.execute(() -> {
            boolean valid = Sa1RomImporter.isValid(dataFile());
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                if (valid) startGame();
                else setBusy(false, "Selecciona Sonic Advance (Europe).gba una vez para preparar el juego.");
            });
        });
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private File dataFile() {
        return new File(getFilesDir(), Sa1RomImporter.FILENAME);
    }

    private void startGame() {
        startActivity(new Intent(this, SA2Activity.class));
        finish();
    }

    private void setBusy(boolean value, String text) {
        busy = value;
        select.setEnabled(!value);
        progress.setVisibility(value ? android.view.View.VISIBLE : android.view.View.GONE);
        status.setText(text);
    }

    private void pickRom() {
        if (busy) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        try {
            startActivityForResult(intent, PICK_ROM);
        } catch (android.content.ActivityNotFoundException error) {
            setBusy(false, "No se encontró un selector de archivos en este dispositivo.");
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_ROM || result != RESULT_OK || data == null || data.getData() == null) return;
        setBusy(true, "Preparando el juego…");
        android.net.Uri uri = data.getData();
        worker.execute(() -> {
            String failure = null;
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                if (input == null) throw new IOException("No se pudo abrir el archivo seleccionado.");
                Sa1RomImporter.importRom(input, dataFile());
            } catch (IOException | SecurityException error) {
                failure = error.getMessage();
                if (failure == null) failure = "No se pudieron importar los datos del juego.";
            }
            final String message = failure;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                if (message == null) startGame();
                else setBusy(false, message);
            });
        });
    }

    @Override
    protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }
}
