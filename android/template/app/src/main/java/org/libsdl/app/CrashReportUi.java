package org.libsdl.app;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ApplicationExitInfo;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;

final class CrashReportUi {
    private static byte[] read(InputStream input, int maximum) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (output.size() + count > maximum) throw new IOException("Report exceeds size limit");
            output.write(buffer, 0, count);
        }
        return output.toByteArray();
    }

    static boolean showPreviousCrash(Activity activity, Runnable continueGame) {
        if (Build.VERSION.SDK_INT < 30) return false;
        try {
            ActivityManager manager = (ActivityManager)activity.getSystemService(Context.ACTIVITY_SERVICE);
            List<ApplicationExitInfo> history = manager.getHistoricalProcessExitReasons(activity.getPackageName(), 0, 5);
            ApplicationExitInfo exit = null;
            for (ApplicationExitInfo item : history) {
                if (activity.getPackageName().equals(item.getProcessName())) { exit = item; break; }
            }
            if (exit == null) return false;
            int reason = exit.getReason();
            if (reason != ApplicationExitInfo.REASON_CRASH_NATIVE && reason != ApplicationExitInfo.REASON_CRASH
                && reason != ApplicationExitInfo.REASON_ANR && reason != ApplicationExitInfo.REASON_LOW_MEMORY
                && reason != ApplicationExitInfo.REASON_SIGNALED
                && !(reason == ApplicationExitInfo.REASON_EXIT_SELF && exit.getStatus() != 0)) return false;
            SharedPreferences preferences = activity.getSharedPreferences("crash-report", Context.MODE_PRIVATE);
            if (exit.getTimestamp() <= preferences.getLong("last-shown", 0)) return false;
            StringBuilder report = new StringBuilder();
            report.append(activity.getPackageName()).append(' ')
                .append(activity.getPackageManager().getPackageInfo(activity.getPackageName(), 0).versionName)
                .append(" (installed version; previous crash may be from the previous APK)\n")
                .append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append(" / Android ").append(Build.VERSION.RELEASE)
                .append(" API ").append(Build.VERSION.SDK_INT).append('\n')
                .append("Timestamp: ").append(exit.getTimestamp()).append(" reason=").append(reason)
                .append(" status=").append(exit.getStatus()).append(" RSS=").append(exit.getRss()).append(" KB\n")
                .append("Description: ").append(exit.getDescription()).append('\n');
            File stage = new File(activity.getFilesDir(), "sa_startup_stage.txt");
            if (stage.isFile()) {
                try (InputStream input = new FileInputStream(stage)) {
                    report.append("Last engine stage: ").append(new String(read(input, 1024), StandardCharsets.UTF_8)).append('\n');
                } catch (IOException ignored) { }
            }
            try (InputStream input = exit.getTraceInputStream()) {
                if (input != null) {
                    byte[] trace = read(input, 8 * 1024 * 1024);
                    report.append(reason == ApplicationExitInfo.REASON_CRASH_NATIVE && Build.VERSION.SDK_INT >= 31
                        ? NativeCrashReport.describe(trace) : new String(trace, StandardCharsets.UTF_8));
                } else report.append("System trace unavailable\n");
            } catch (IOException | RuntimeException error) {
                report.append("Trace unavailable: ").append(error.getMessage()).append('\n');
            }
            final String details = report.substring(0, Math.min(report.length(), 65536));
            preferences.edit().putLong("last-shown", exit.getTimestamp()).apply();
            new AlertDialog.Builder(activity).setTitle("El juego se cerró")
                .setMessage("Android registró un cierre inesperado. Puedes copiar el informe para revisar la causa; tus partidas se conservan.")
                .setCancelable(false)
                .setPositiveButton("Continuar", (dialog, which) -> continueGame.run())
                .setNeutralButton("Copiar informe", (dialog, which) -> { copy(activity, details); continueGame.run(); })
                .setNegativeButton("Ver informe", (dialog, which) -> showDetails(activity, details, continueGame))
                .show();
            return true;
        } catch (Exception ignored) {
            // Reporting must never prevent a normal launch on older OEM APIs.
            return false;
        }
    }

    private static void copy(Activity activity, String report) {
        ClipboardManager clipboard = (ClipboardManager)activity.getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("Sonic Advance - cierre", report));
    }

    private static void showDetails(Activity activity, String report, Runnable continueGame) {
        TextView text = new TextView(activity);
        text.setText(report);
        text.setTextIsSelectable(true);
        text.setTextSize(12);
        text.setPadding(24, 16, 24, 16);
        ScrollView scroll = new ScrollView(activity);
        scroll.addView(text);
        new AlertDialog.Builder(activity).setTitle("Informe del cierre").setView(scroll).setCancelable(false)
            .setPositiveButton("Continuar", (dialog, which) -> continueGame.run())
            .setNeutralButton("Copiar informe", (dialog, which) -> { copy(activity, report); continueGame.run(); }).show();
    }
}
