package org.libsdl.app;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

final class Sa1RomImporter {
    static final String FILENAME = "sa1-assets.gba";
    static final int ROM_SIZE = 8 * 1024 * 1024;
    static final String SHA1 = "eb00f101af23d728075ac2117e27ecd8a4b4c3e9";

    static boolean isValid(File file) {
        if (file.length() != ROM_SIZE) return false;
        try (InputStream stream = new FileInputStream(file)) {
            return SHA1.equals(digest(stream, null, ROM_SIZE));
        } catch (IOException error) {
            return false;
        }
    }

    static void importRom(InputStream input, File destination) throws IOException {
        copyValidated(input, destination, ROM_SIZE, SHA1);
    }

    // Shared by import and regression tests. The Android UI supplies only the
    // fixed revision above; no intent or preference can override its checksum.
    static void copyValidated(InputStream input, File destination, int size, String expected) throws IOException {
        File temporary = new File(destination.getPath() + ".tmp");
        try {
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                if (!expected.equals(digest(input, output, size))) {
                    throw new IOException("Esta ROM no corresponde a Sonic Advance (Europe). Selecciona la versión europea original, sin modificar.");
                }
                output.flush();
                output.getFD().sync();
            }
            if (!temporary.renameTo(destination)) {
                throw new IOException("No se pudieron guardar los datos del juego.");
            }
        } finally {
            if (temporary.exists()) temporary.delete();
        }
    }

    private static String digest(InputStream input, FileOutputStream output, int expectedSize) throws IOException {
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA-1");
        } catch (NoSuchAlgorithmException error) {
            throw new IOException("No se pudo verificar el juego.", error);
        }
        byte[] buffer = new byte[64 * 1024];
        int total = 0;
        int count;
        while ((count = input.read(buffer)) != -1) {
            if (Thread.currentThread().isInterrupted()) throw new IOException("Importación cancelada.");
            if (count > expectedSize - total) throw new IOException("El archivo seleccionado es demasiado grande.");
            total += count;
            sha1.update(buffer, 0, count);
            if (output != null) output.write(buffer, 0, count);
        }
        if (total != expectedSize) throw new IOException("El archivo está incompleto. Selecciona una ROM .gba de Sonic Advance (Europe).");
        StringBuilder hex = new StringBuilder(40);
        for (byte value : sha1.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", value & 0xff));
        return hex.toString();
    }
}
