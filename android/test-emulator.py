#!/usr/bin/env python3
"""Install and exercise the real release APK/UI on Android, preserving evidence."""
import json
import io
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import zipfile
from PIL import Image

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

def acknowledge_observed_fullscreen_tutorial():
    # Android36 sometimes exposes no accessibility root for its immersive
    # tutorial. The first RC3 run captured this exact 2340x1080 system panel.
    # Require its light body AND three blue button pixels before tapping the
    # observed Got it location; never issue a blind tap into the native game.
    data=adb('exec-out','screencap','-p',binary=True)
    picture=Image.open(io.BytesIO(data)).convert('RGB')
    if picture.size!=(2340,1080): return False
    def near(point,expected):
        actual=picture.getpixel((round(picture.width*point[0]),round(picture.height*point[1])))
        return all(abs(a-b)<=8 for a,b in zip(actual,expected))
    body=[(.24,.05),(.5,.05),(.75,.2)]
    button=[(.69,.45),(.72,.49),(.745,.46)]
    if not all(near(p,(238,237,243)) for p in body): return False
    if not all(near(p,(73,93,146)) for p in button): return False
    (out/'android-fullscreen-tutorial.png').write_bytes(data)
    adb('shell','input','tap','1673','499')
    print('Acknowledged screenshot-verified Android fullscreen tutorial with null accessibility root',flush=True)
    return True

def hierarchy():
    last_error=None
    for attempt in range(3):
        try:
            result=adb('shell','uiautomator','dump','--compressed','/sdcard/sa2-test-ui.xml')
            (out/'last-ui-dump.txt').write_text(result)
            text=adb('shell','cat','/sdcard/sa2-test-ui.xml')
            root=ET.fromstring(text)
            (out/'last-ui.xml').write_text(text)
            return root
        except (subprocess.CalledProcessError,subprocess.TimeoutExpired,ET.ParseError) as error:
            last_error=error
            acknowledge_observed_fullscreen_tutorial()
            time.sleep(.5)
    raise AssertionError('Android accessibility hierarchy unavailable after retries: '+str(last_error))

def find(prefix='', description=''):
    for attempt in range(5):
        nodes=list(hierarchy().iter('node'))
        # Android's own first-use immersive tutorial covers the app on a fresh
        # system image. Its overlay can expose the app's underlying nodes,
        # so dismiss it before returning a target that is actually covered.
        dismissed=False
        for node in nodes:
            if node.get('package')=='com.android.systemui' and node.get('text')=='Got it':
                x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
                adb('shell','input','tap',str((x1+x2)//2),str((y1+y2)//2))
                print('Acknowledged Android fullscreen tutorial',flush=True)
                dismissed=True
                break
        if dismissed:
            time.sleep(.4)
            continue
        for node in nodes:
            if ((prefix and node.get('text','').startswith(prefix)) or
                (description and node.get('content-desc','') == description)):
                x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
                if x2>x1 and y2>y1: return ((x1+x2)//2,(y1+y2)//2)
        time.sleep(.4)
    raise AssertionError('Android UI target unavailable: '+prefix+description)

def click(prefix='',description=''):
    print('Android UI: '+prefix+description, flush=True)
    x,y=find(prefix,description)
    adb('shell','input','tap',str(x),str(y))
    time.sleep(.3)

def select_setting(parent,choice):
    # A delayed system tutorial can consume the tap that should open the
    # choice dialog. Verify the resulting setting and repeat the observed
    # action if necessary; an unresponsive app still fails after three tries.
    for attempt in range(3):
        try:
            click(parent)
            click(choice)
            find(parent+' '+choice)
            return
        except AssertionError as error:
            if not str(error).startswith('Android UI target unavailable:') or attempt==2:
                raise
            assert adb('shell','pidof',package).strip(), 'native process must remain alive during settings selection'
            print('Retry observed settings action after system overlay: '+parent,flush=True)

def start():
    adb('shell','am','start','-n',package+'/org.libsdl.app.RomImportActivity')
    time.sleep(6)
    assert adb('shell','pidof',package).strip(), 'native process must remain alive'

def open_settings():
    for attempt in range(3):
        adb('shell','input','keyevent','4')
        try:
            find('SONIC ADVANCE 2')
            return
        except AssertionError:
            if attempt==2: raise

def assert_no_game_menu():
    # A bare SDL SurfaceView legitimately has no accessibility root on API36.
    # Check the actual Java view visibility and focused app window instead.
    # The positive pause test below still locates/clicks the real Button via
    # accessibility; a missing or stale visibility callback fails this check.
    acknowledge_observed_fullscreen_tutorial()
    assert adb('shell','pidof',package).strip(), 'native process must remain alive'
    window=adb('shell','dumpsys','window')
    (out/'game-window.txt').write_text(window)
    focus=[line for line in window.splitlines() if 'mCurrentFocus=' in line]
    assert any(package+'/org.libsdl.app.SA2Activity' in line for line in focus), 'the native game must own the focused window: '+str(focus)
    states=re.findall(r'SA Android pause settings visible=([01])',adb('logcat','-d'))
    assert states and states[-1]=='0', 'the actual Android settings button must be absent or GONE during gameplay'

def wait_native_stage(name,timeout=30):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        log=adb('logcat','-d').rsplit('SA Android stage: before_engine_init',1)[-1]
        if 'SA Android stage: '+name in log: return
        assert adb('shell','pidof',package).strip(), 'native process must remain alive while waiting for '+name
        time.sleep(.3)
    raise AssertionError('Native scene never became ready: '+name)

checks=[]
try:
    adb('logcat','-c')
    adb('shell','wm','size','1080x2340')
    with zipfile.ZipFile(apk) as archive:
        dex=archive.read('classes.dex')
        assert '⋮'.encode() not in dex, 'the permanent three-dot overlay must be absent from the actual APK'
        assert b'setNativePauseMenuVisible' in dex, 'the APK must include the real native pause callback'
    assert 'Success' in adb('install','-r',str(apk))
    checks.append('release APK installed')
    start()
    width,height=capture('01-original-startup')
    assert width>height, 'landscape orientation'
    assert (width,height)==(2340,1080), 'Galaxy A15 aspect ratio must be exercised'
    assert_no_game_menu()
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
    open_settings()
    find('SONIC ADVANCE 2')
    capture('02-settings')
    select_setting('Pantalla:','Píxeles exactos')
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
    open_settings()
    find('Pantalla: Píxeles exactos')
    capture('07-persisted-settings')
    select_setting('Pantalla:','Pantalla completa · encuadre')
    click('Continuar jugando')
    time.sleep(.7)
    # Complete the original first-run profile with normal direction/A/Start
    # input, then keep the original title's default single-player selection.
    # No test shortcuts or synthetic stage-entry code is present in this APK.
    ready=False
    start_button=(round(width*.5+base*.1),round(height-base*.07))
    right=(round(base*.22+base*.105*.85),round(height-base*.25))
    boot_log=adb('logcat','-d').rsplit('SA Android stage: before_engine_init',1)[-1]
    if 'SA Android stage: before_new_profile' in boot_log:
        wait_native_stage('profile_language_ready')
        adb('shell','input','tap',str(tx),str(ty))
        wait_native_stage('profile_name_ready')
        # The Latin matrix starts on a space. Choose the next, nonempty
        # character only after the name screen's actual fade has completed.
        adb('shell','input','swipe',str(right[0]),str(right[1]),str(right[0]+1),str(right[1]),'180')
        time.sleep(.3)
        for point,delay in [((tx,ty),.4),(start_button,.4),(start_button,.4)]:
            adb('shell','input','tap',str(point[0]),str(point[1]))
            time.sleep(delay)
        wait_native_stage('profile_name_confirmed')
        capture('07b-original-profile-created')
        checks.append('original first-run language/name profile completed through actual touch controls')
    inputs=[start_button,(tx,ty)]
    selector_checked=False
    for n in range(100):
        current_log=adb('logcat','-d')
        if not selector_checked and 'SA Android stage: character_select_ready' in current_log:
            time.sleep(1)
            capture('07c-original-selector-sonic')
            for name in ['cream','tails','knuckles','amy']:
                adb('shell','input','tap',str(right[0]),str(right[1]))
                time.sleep(.45)
                assert 'SA Android stage: character_preview_'+name in adb('logcat','-d'), 'normal touch navigation must reach '+name
                capture('07c-original-selector-'+name)
            selector_checked=True
            checks.append('five original portraits navigated from a fresh save; Amy confirmed through the normal selector')
        if 'SA Android gameplay ready' in current_log:
            ready=True
            break
        x,y=inputs[n%len(inputs)]
        adb('shell','input','tap',str(x),str(y))
        time.sleep(.3)
    assert ready, 'real Android touch input must reach the original game stage'
    assert selector_checked, 'original character selector must be exercised before starting the stage'
    time.sleep(8)
    dx,dy=round(base*.22+base*.105*.85),round(height-base*.25)
    adb('shell','input','swipe',str(dx),str(dy),str(dx+1),str(dy),'1000')
    adb('shell','input','tap',str(tx),str(ty))
    capture('08-native-gameplay')
    assert_no_game_menu()
    assert 'viewport gameplay=1 output=2340x1080 mode=2 inset=22' in adb('logcat','-d'), 'full-screen A15 framing and HUD inset must be active in the native game'
    adb('shell','input','tap',str(start_button[0]),str(start_button[1]))
    find(description='Ajustes de Android')
    assert re.findall(r'SA Android pause settings visible=([01])',adb('logcat','-d'))[-1]=='1', 'the actual Java settings button must become visible on pause'
    capture('08b-pause-settings-entry')
    click(description='Ajustes de Android')
    click('Continuar jugando')
    adb('shell','input','tap',str(start_button[0]),str(start_button[1]))
    time.sleep(.5)
    assert_no_game_menu()
    capture('08c-resumed-without-overlay')
    checks.append('A15 full-screen framing without stretching; settings button appears only while paused and disappears on resume')
    checks.append('original menus reach the real native level through touch; directional and jump targets exercised')
    adb('shell','am','force-stop',package)
    start()
    fresh_log=adb('logcat','-d').rsplit('SA Android stage: before_engine_init',1)[-1]
    assert 'SA Android stage: before_load_save' in fresh_log, 'original game profile must reload from real persisted save sectors'
    assert 'SA Android stage: before_new_profile' not in fresh_log, 'restart must not ask for a new profile again'
    capture('09-original-save-reloaded')
    checks.append('real original game profile survives force-stop and native save reload')
    log=adb('logcat','-d')
    assert 'SA Android options loaded screen=2' in log, 'native startup must reload persisted full-screen configuration'
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
