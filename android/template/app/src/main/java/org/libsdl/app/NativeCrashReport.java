package org.libsdl.app;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Reads only crash fields from Android's public tombstone protobuf schema.
 * https://github.com/aosp-mirror/platform_system_core/blob/main/debuggerd/proto/tombstone.proto
 * No native signal handlers or third-party reporting services are installed.
 */
public final class NativeCrashReport {
    private static final class Field {
        int number;
        long value;
        byte[] data;
        String text() { return data == null ? "" : new String(data, StandardCharsets.UTF_8); }
    }

    private static final class Reader {
        final byte[] data;
        int cursor;
        Reader(byte[] data) { this.data = data; }
        long integer() throws IOException {
            long value = 0;
            for (int shift = 0; shift < 64; shift += 7) {
                if (cursor >= data.length) throw new IOException("Truncated protobuf");
                int next = data[cursor++] & 255;
                if (shift == 63 && (next & 254) != 0) throw new IOException("Invalid protobuf integer");
                value |= (long)(next & 127) << shift;
                if ((next & 128) == 0) return value;
            }
            throw new IOException("Invalid protobuf integer");
        }
        Field next() throws IOException {
            if (cursor == data.length) return null;
            long tag = integer();
            Field field = new Field();
            field.number = (int)(tag >>> 3);
            if (field.number == 0 || tag >>> 3 > 0x1fffffff) throw new IOException("Invalid protobuf tag");
            int wire = (int)tag & 7;
            if (wire == 0) field.value = integer();
            else if (wire == 2) {
                long length = integer();
                if (length < 0 || length > data.length - cursor) throw new IOException("Truncated protobuf message");
                field.data = java.util.Arrays.copyOfRange(data, cursor, cursor + (int)length);
                cursor += (int)length;
            } else if (wire == 1 || wire == 5) {
                int length = wire == 1 ? 8 : 4;
                if (length > data.length - cursor) throw new IOException("Truncated protobuf field");
                cursor += length;
            } else throw new IOException("Unsupported protobuf wire type");
            return field;
        }
    }

    private static List<Field> fields(byte[] data) throws IOException {
        List<Field> result = new ArrayList<>();
        Reader reader = new Reader(data == null ? new byte[0] : data);
        Field field;
        while ((field = reader.next()) != null) result.add(field);
        return result;
    }

    private static Field find(List<Field> fields, int number) {
        for (Field field : fields) if (field.number == number) return field;
        return new Field();
    }

    public static String describe(byte[] data) throws IOException {
        if (data.length > 8 * 1024 * 1024) throw new IOException("Crash report too large");
        List<Field> tombstone = fields(data);
        StringBuilder report = new StringBuilder();
        List<Field> signal = fields(find(tombstone, 10).data);
        report.append("Signal: ").append(find(signal, 2).text())
            .append(" (").append(find(signal, 1).value).append(") ")
            .append(find(signal, 4).text()).append('\n');
        report.append("Fault address: 0x").append(Long.toHexString(find(signal, 9).value)).append('\n');
        String abort = find(tombstone, 14).text();
        if (!abort.isEmpty()) report.append("Abort: ").append(abort).append('\n');
        long crashingThread = find(tombstone, 6).value;
        for (Field field : tombstone) {
            if (field.number == 15) report.append("Cause: ").append(find(fields(field.data), 1).text()).append('\n');
            if (field.number != 16) continue;
            List<Field> entry = fields(field.data);
            if (find(entry, 1).value != crashingThread) continue;
            List<Field> thread = fields(find(entry, 2).data);
            report.append("Thread: ").append(find(thread, 2).text()).append('\n');
            int index = 0;
            for (Field item : thread) {
                if (item.number != 4 || index >= 48) continue;
                List<Field> frame = fields(item.data);
                report.append('#').append(index++).append(" pc ")
                    .append(Long.toHexString(find(frame, 1).value)).append(' ')
                    .append(find(frame, 6).text()).append(' ')
                    .append(find(frame, 4).text()).append('+').append(find(frame, 5).value)
                    .append(" build_id=").append(find(frame, 8).text()).append('\n');
            }
        }
        return report.toString();
    }
}
