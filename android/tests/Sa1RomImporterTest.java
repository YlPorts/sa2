package org.libsdl.app;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.Arrays;

public final class Sa1RomImporterTest {
    private static void require(boolean value) {
        if (!value) throw new AssertionError();
    }

    private static String sha1(byte[] value) throws Exception {
        StringBuilder hex = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-1").digest(value))
            hex.append(String.format("%02x", b & 255));
        return hex.toString();
    }

    private static void reject(InputStream input, File target, int size, String hash, byte[] original) throws Exception {
        try {
            Sa1RomImporter.copyValidated(input, target, size, hash);
            throw new AssertionError("Invalid import accepted");
        } catch (IOException expected) {
            require(Arrays.equals(original, Files.readAllBytes(target.toPath())));
            require(!new File(target.getPath() + ".tmp").exists());
        }
    }

    public static void main(String[] args) throws Exception {
        File directory = new File(args[0]);
        File target = new File(directory, "assets.gba");
        byte[] valid = new byte[150003];
        for (int i = 0; i < valid.length; i++) valid[i] = (byte)(i * 13);
        String hash = sha1(valid);
        Sa1RomImporter.copyValidated(new ByteArrayInputStream(valid), target, valid.length, hash);
        require(Arrays.equals(valid, Files.readAllBytes(target.toPath())));
        reject(new ByteArrayInputStream(Arrays.copyOf(valid, valid.length - 1)), target, valid.length, hash, valid);
        reject(new ByteArrayInputStream(Arrays.copyOf(valid, valid.length + 1)), target, valid.length, hash, valid);
        byte[] wrong = valid.clone();
        wrong[70000] ^= 1;
        reject(new ByteArrayInputStream(wrong), target, valid.length, hash, valid);
        reject(new InputStream() {
            public int read() throws IOException { throw new IOException("Provider disconnected"); }
        }, target, valid.length, hash, valid);
        Thread.currentThread().interrupt();
        reject(new ByteArrayInputStream(valid), target, valid.length, hash, valid);
        Thread.interrupted();
        require(!Sa1RomImporter.isValid(target));
        File synthetic = new File(directory, "synthetic.gba");
        Files.write(synthetic.toPath(), new byte[Sa1RomImporter.ROM_SIZE]);
        require(!Sa1RomImporter.isValid(synthetic));
        try (InputStream input = Files.newInputStream(synthetic.toPath())) {
            try {
                Sa1RomImporter.importRom(input, target);
                throw new AssertionError("Synthetic ROM accepted");
            } catch (IOException expected) {
                require(Arrays.equals(valid, Files.readAllBytes(target.toPath())));
            }
        }
        System.out.println("Import: valid stream, checksum, exact size, failed reads, cancellation and existing data preservation passed");
    }
}
