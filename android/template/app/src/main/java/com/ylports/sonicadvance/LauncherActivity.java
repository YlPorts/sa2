package com.ylports.sonicadvance;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.libsdl.app.SA2Activity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public class LauncherActivity extends Activity {
    private static final String STAGE_FILE = "sa_startup_stage.txt";

    private TextView statusView;
    private boolean launchedGame;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState != null) {
            launchedGame = savedInstanceState.getBoolean("launchedGame", false);
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        int pad = (int)(24 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText(getApplicationInfo().loadLabel(getPackageManager()));
        title.setTextSize(24);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        statusView = new TextView(this);
        statusView.setTextSize(16);
        statusView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        statusParams.topMargin = pad;
        root.addView(statusView, statusParams);

        Button start = new Button(this);
        start.setText("Iniciar / reintentar");
        start.setOnClickListener(v -> startGame());
        LinearLayout.LayoutParams buttonParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        buttonParams.topMargin = pad;
        root.addView(start, buttonParams);

        setContentView(root);
        refreshStatus();

        if (!launchedGame) {
            new Handler(Looper.getMainLooper()).postDelayed(this::startGame, 350);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (launchedGame) {
            new Handler(Looper.getMainLooper()).postDelayed(this::refreshStatus, 300);
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putBoolean("launchedGame", launchedGame);
        super.onSaveInstanceState(outState);
    }

    private void startGame() {
        launchedGame = true;
        writeStage("launcher_start_game");
        statusView.setText("Iniciando motor nativo...");
        Intent intent = new Intent(this, SA2Activity.class);
        startActivity(intent);
    }

    private File stageFile() {
        return new File(getFilesDir(), STAGE_FILE);
    }

    private void writeStage(String value) {
        try (FileOutputStream out = new FileOutputStream(stageFile(), false)) {
            out.write(value.getBytes(StandardCharsets.UTF_8));
            out.flush();
        } catch (Exception ignored) {
        }
    }

    private String readStage() {
        File file = stageFile();
        if (!file.exists()) {
            return "sin diagnóstico todavía";
        }

        try (FileInputStream in = new FileInputStream(file)) {
            byte[] data = new byte[(int)Math.min(file.length(), 4096)];
            int read = in.read(data);
            if (read <= 0) {
                return "diagnóstico vacío";
            }
            return new String(data, 0, read, StandardCharsets.UTF_8).trim();
        } catch (Exception e) {
            return "no se pudo leer: " + e.getClass().getSimpleName();
        }
    }

    private void refreshStatus() {
        String stage = readStage();
        if (launchedGame) {
            statusView.setText("El proceso del juego volvió o se cerró.\nÚltima etapa: " + stage
                    + "\n\nDime exactamente esta etapa si el juego no abre.");
        } else {
            statusView.setText("Diagnóstico de arranque listo.\nÚltima etapa: " + stage);
        }
    }
}
