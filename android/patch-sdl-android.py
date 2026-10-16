#!/usr/bin/env python3
"""Apply narrowly scoped Android compatibility fixes to pinned SDL 2.30.3."""
from pathlib import Path
import sys

root=Path(sys.argv[1])
hid=root/'android-project/app/src/main/java/org/libsdl/app/HIDDeviceManager.java'
text=hid.read_text()
# Backport SDL2's registerReceiverCompat for target 34+ without replacing the
# rendering/input library. See libsdl-org/SDL, SDL2 branch, HIDDeviceManager.java.
for receiver in ['mUsbBroadcast','mBluetoothBroadcast']:
    old=f'mContext.registerReceiver({receiver}, filter);'
    new=('if (android.os.Build.VERSION.SDK_INT >= 33) {\n'
         f'            mContext.registerReceiver({receiver}, filter, Context.RECEIVER_NOT_EXPORTED);\n'
         '        } else {\n'
         f'            {old}\n'
         '        }')
    marker=f'mContext.registerReceiver({receiver}, filter, Context.RECEIVER_NOT_EXPORTED);'
    if marker not in text:
        assert text.count(old)==1, 'unexpected pinned SDL receiver source'
        text=text.replace(old,new)
hid.write_text(text)

# AGP >=8.5.1 aligns uncompressed native libraries for 16 KB Android pages.
# AGP 8.5.x requires Gradle 8.7 and JDK 17, both pinned by the build workflow.
for relative,old,new in [
    ('android-project/build.gradle','com.android.tools.build:gradle:8.1.1','com.android.tools.build:gradle:8.5.2'),
    ('android-project/gradle/wrapper/gradle-wrapper.properties','gradle-8.1.1-bin.zip','gradle-8.7-bin.zip')]:
    path=root/relative
    text=path.read_text()
    if new not in text:
        assert text.count(old)==1, 'unexpected pinned SDL Gradle source'
        path.write_text(text.replace(old,new))
