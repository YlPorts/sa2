#!/usr/bin/env python3
"""Install and exercise the real release APK/UI on Android, preserving evidence."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

source, out = map(Path, sys.argv[1:3])
out.mkdir(parents=True, exist_ok=True)
apk = next(source.rglob('*emulator.apk'))
metadata = next(source.rglob('package.txt')).read_text()
package = re.search(r"package: name='([a-zA-Z0-9_.]+)'", metadata).group(1)
assert package.startswith('com.ylports.sonicadvance2')

def adb(*args, binary=False):
    return subprocess.check_output(['adb', *args], text=not binary, stderr=subprocess.STDOUT, timeout=20)

def capture(name):
    data = adb('exec-out','screencap','-p',binary=True)
    (out / (name+'.png')).write_bytes(data)
    print('Captured '+name, flush=True)
    return struct.unpack_from('>II',data,16)

def hierarchy():
    adb('shell','uiautomator','dump','/sdcard/sa2-test-ui.xml')
    text = adb('shell','cat','/sdcard/sa2-test-ui.xml')
    (out/'last-ui.xml').write_text(text)
    return ET.fromstring(text)

def find(prefix='', description=''):
    for attempt in range(5):
        nodes=list(hierarchy().iter('node'))
        for node in nodes:
            if ((prefix and node.get('text','').startswith(prefix)) or
                (description and node.get('content-desc','') == description)):
                x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
                if x2>x1 and y2>y1: return ((x1+x2)//2,(y1+y2)//2)
        # Android's own first-use immersive tutorial covers the app on a fresh
        # system image. Acknowledge the observed system dialog like a user.
        for node in nodes:
            if node.get('package')=='com.android.systemui' and node.get('text')=='Got it':
                x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
                adb('shell','input','tap',str((x1+x2)//2),str((y1+y2)//2))
                print('Acknowledged Android fullscreen tutorial',flush=True)
                break
        time.sleep(.4)
    raise AssertionError('Android UI target unavailable: '+prefix+description)

def click(prefix='',description=''):
    print('Android UI: '+prefix+description, flush=True)
    x,y=find(prefix,description)
    adb('shell','input','tap',str(x),str(y))
    time.sleep(.3)

def start():
    adb('shell','am','start','-n',package+'/org.libsdl.app.RomImportActivity')
    time.sleep(6)
    assert adb('shell','pidof',package).strip(), 'native process must remain alive'

checks=[]
try:
    adb('logcat','-c')
    assert 'Success' in adb('install','-r',str(apk))
    checks.append('release APK installed')
    start()
    width,height=capture('01-original-startup')
    assert width>height, 'landscape orientation'
    checks.append('Android36 native x86_64 boot and landscape')
    original_pid=adb('shell','pidof',package).strip()
    adb('shell','settings','put','system','font_scale','1.1')
    time.sleep(4)
    assert adb('shell','pidof',package).strip()==original_pid, 'configuration recreation must not exit the Android process'
    adb('shell','settings','put','system','font_scale','1.0')
    time.sleep(4)
    assert adb('shell','pidof',package).strip()==original_pid, 'activity must recreate and restart native SDL cleanly'
    recreated_width,recreated_height=capture('01b-recreated-activity')
    assert recreated_width>recreated_height, 'recreated game activity stays visible in landscape'
    checks.append('Android configuration recreation keeps the same process and restarts the real native engine')
    click(description='Ajustes de Android')
    find('SONIC ADVANCE 2')
    capture('02-settings')
    click('Pantalla:')
    click('Píxeles exactos')
    find('Pantalla: Píxeles exactos')
    click('Editar posición')
    width,height=capture('03-control-editor')
    base=min(width,height)
    ax,ay=round(width-base*.19),round(height-base*.295)
    tx,ty=round(width*.78),round(height*.55)
    adb('shell','input','swipe',str(ax),str(ay),str(tx),str(ty),'450')
    capture('04-moved-control')
    click('Guardar')
    click('Continuar jugando')
    time.sleep(1)
    checks.append('settings overlay, integer scaling, real control drag/save and native JNI apply')
    capture('05-game-and-controls')
    adb('shell','input','keyevent','3')
    time.sleep(.5)
    start()
    capture('06-resumed')
    checks.append('background and resume without native crash')
    adb('shell','am','force-stop',package)
    start()
    click(description='Ajustes de Android')
    find('Pantalla: Píxeles exactos')
    capture('07-persisted-settings')
    click('Continuar jugando')
    time.sleep(.7)
    # Advance the original screens through actual Android touch targets. No
    # test shortcuts or synthetic stage-entry code is present in this APK.
    ready=False
    # The original first-run profile needs a nonblank name, then Start twice.
    # Leave END and its blank bottom grid row before choosing a valid letter;
    # recover when a preceding Start selected END while the name was empty.
    left=(round(base*.22-base*.105*.85),round(height-base*.25))
    start_button=(round(width*.5+base*.1),round(height-base*.07))
    up=(round(base*.22),round(height-base*.25-base*.105*.85))
    inputs=[start_button,up,up,left,left,(tx,ty),start_button,start_button]
    for n in range(100):
        if 'SA Android gameplay ready' in adb('logcat','-d'):
            ready=True
            break
        x,y=inputs[n%len(inputs)]
        adb('shell','input','tap',str(x),str(y))
        time.sleep(.3)
    assert ready, 'real Android touch input must reach the original game stage'
    time.sleep(8)
    dx,dy=round(base*.22+base*.105*.85),round(height-base*.25)
    adb('shell','input','swipe',str(dx),str(dy),str(dx+1),str(dy),'1000')
    adb('shell','input','tap',str(tx),str(ty))
    capture('08-native-gameplay')
    checks.append('original menus reach the real native level through touch; directional and jump targets exercised')
    log=adb('logcat','-d')
    assert 'SA Android options loaded screen=1' in log, 'native startup must reload persisted configuration'
    assert 'SA Android options applied open=1 screen=1' in log, 'Java settings reach the native game thread'
    assert 'SA Android options applied open=0 screen=1' in log, 'closing settings resumes the native game thread'
    assert 'SA Android audio ready driver=' in log, 'a real Android audio backend must open successfully'
    assert not re.search(r'Process: '+re.escape(package)+r', PID:',log), 'Android Java exception'
    assert ('>>> '+package+' <<<') not in log, 'Android native crash'
    checks.append('configuration survives force-stop/relaunch and native startup reload')
    (out/'logcat.txt').write_text(log)
    report={'status':'passed','package':package,'android_api':36,'abi':'x86_64','checks':checks,
            'physical_samsung_a15_tested':False,'arm32_arm64_gameplay_executed':False}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: installed Android36 APK, original native startup, settings/JNI, edit/save, suspend/resume and persistence')
except Exception:
    try:
        capture('failure')
        time.sleep(4)
        (out/'logcat.txt').write_text(adb('logcat','-d'))
        (out/'crash-log.txt').write_text(adb('logcat','-b','crash','-d'))
        try:
            adb('root')
            adb('wait-for-device')
            stage=adb('shell','cat','/data/data/'+package+'/files/sa_startup_stage.txt')
            (out/'native-startup-stage.txt').write_text(stage)
            subprocess.run(['adb','pull','/data/tombstones',str(out/'tombstones')],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        except subprocess.CalledProcessError:
            pass
    finally:
        (out/'report.json').write_text(json.dumps({'status':'failed','checks_completed':checks},indent=2)+'\n')
    raise
