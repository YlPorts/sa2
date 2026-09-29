package org.libsdl.app;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public final class NativeCrashReportTest {
    private static byte[] combine(byte[]... pieces) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        for (byte[] piece : pieces) output.write(piece, 0, piece.length);
        return output.toByteArray();
    }
    private static byte[] integer(long value) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        do {
            int part = (int)(value & 127);
            value >>>= 7;
            output.write(part | (value == 0 ? 0 : 128));
        } while (value != 0);
        return output.toByteArray();
    }
    private static byte[] scalar(int field, long value) { return combine(integer(field << 3), integer(value)); }
    private static byte[] message(int field, byte[] value) { return combine(integer((field << 3) | 2), integer(value.length), value); }
    private static byte[] text(int field, String value) { return message(field, value.getBytes(StandardCharsets.UTF_8)); }
    private static void check(boolean valid) { if (!valid) throw new AssertionError(); }
    private static void reject(byte[] value) {
        try { NativeCrashReport.describe(value); throw new AssertionError("Malformed trace accepted"); }
        catch (IOException expected) { }
    }
    public static void main(String[] args) throws Exception {
        byte[] frame = combine(scalar(1, 0x123456), text(4, "GameStageStart"), scalar(5, 20), text(6, "libmain.so"), text(8, "abcdef"));
        byte[] worker = combine(scalar(1, 7), message(2, combine(text(2, "worker"), message(4, frame))));
        byte[] crashed = combine(scalar(1, 42), message(2, combine(text(2, "SDLThread"), message(4, frame))));
        byte[] tombstone = combine(scalar(6, 42), message(10, combine(scalar(1, 11), text(2, "SIGSEGV"), text(4, "SEGV_MAPERR"), scalar(9, 0xdeadbeefL))),
            text(14, "test abort"), message(16, worker), message(16, crashed), scalar(999, 234));
        String report = NativeCrashReport.describe(tombstone);
        check(report.contains("SIGSEGV (11)") && report.contains("Fault address: 0xdeadbeef"));
        check(report.contains("Thread: SDLThread") && !report.contains("worker"));
        check(report.contains("pc 123456 libmain.so GameStageStart+20 build_id=abcdef"));
        check(report.contains("Abort: test abort"));
        reject(new byte[] { 0 });
        reject(new byte[] { (byte)0x80 });
        reject(new byte[] { 10, 4, 0 });
        reject(new byte[] { 9, 1 });
        reject(new byte[] { 13, 1 });
        reject(new byte[] { 11 });
        reject(new byte[] { 8, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, (byte)0x80, 2 });
        System.out.println("Passed: native crash signal, fault, correct thread, addresses, build ID and malformed traces");
    }
}
