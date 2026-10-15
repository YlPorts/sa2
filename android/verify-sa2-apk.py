#!/usr/bin/env python3
"""Verify the packaged native payload, including every OC atlas byte."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parent.parent

def symbols(data):
    assert data[:4] == b'\x7fELF' and data[5] == 1, 'little-endian ELF required'
    bits = data[4]
    shoff = struct.unpack_from('<I' if bits == 1 else '<Q', data, 32 if bits == 1 else 40)[0]
    entsize, count = struct.unpack_from('<HH', data, 46 if bits == 1 else 58)
    sections = []
    for i in range(count):
        s = shoff + i * entsize
        typ = struct.unpack_from('<I', data, s + 4)[0]
        if bits == 1:
            address, offset, size, link = struct.unpack_from('<IIII', data, s + 12)
            entry = struct.unpack_from('<I', data, s + 36)[0]
        else:
            address, offset, size = struct.unpack_from('<QQQ', data, s + 16)
            link = struct.unpack_from('<I', data, s + 40)[0]
            entry = struct.unpack_from('<Q', data, s + 56)[0]
        sections.append((typ, address, offset, size, link, entry))
    result = {}
    for typ, _, offset, size, link, entry in sections:
        if typ != 11:
            continue
        strings = sections[link]
        names = data[strings[2]:strings[2] + strings[3]]
        for p in range(offset, offset + size, entry):
            if bits == 1:
                name, value, length, _, _, index = struct.unpack_from('<IIIBBH', data, p)
            else:
                name, _, _, index, value, length = struct.unpack_from('<IBBHQQ', data, p)
            name = names[name:names.find(b'\0', name)].decode('utf-8')
            if 0 < index < len(sections):
                section = sections[index]
                start = section[2] + value - section[1]
                result[name] = data[start:start + length]
            elif index == 0:
                assert name != '__sF', 'removed Android stdio symbol'
    return result

def joined(paths):
    return b''.join((ROOT / p).read_bytes() for p in paths)

def expected_assets():
    base = 'graphics/sa2/ocs/'
    guests = base + 'guests/generated/'
    names = ['elizabeth', 'jude', 'kiro', 'yuliana', 'kura']
    result = {}
    for symbol, suffix, guest_suffix in [
        ('gOcFrameTiles', '', '_frame'), ('gOcSpecialFrameTiles', '_special', '_special'),
        ('gOcActionFrameTiles', '_actions', '_action')]:
        result[symbol] = joined([base + n + suffix + '.4bpp' for n in names]
            + [guests + n + guest_suffix + '.4bpp' for n in ['sans', 'spiderman']])
    result['gOcPalettes'] = joined([base + n + '.gbapal' for n in names]
        + [guests + n + '.gbapal' for n in ['sans', 'spiderman']])
    result['gOcRevisedRunTiles'] = joined([base + n + '_run_v3.4bpp' for n in ['jude','kiro','yuliana']]
        + [guests + n + '_revised_run.4bpp' for n in ['sans','spiderman']])
    result['gOcRevisedAttackTiles'] = joined([base + n + '_attack_v3.4bpp' for n in ['jude','kiro','yuliana','kura']]
        + [guests + n + '_revised_attack.4bpp' for n in ['sans','spiderman']])
    result['gOcGuestProjectileTiles'] = joined([guests + n + '_projectile.4bpp' for n in ['sans','spiderman']])
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('apk', type=Path)
    parser.add_argument('--abis', default='armeabi-v7a,arm64-v8a')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = {'apk': args.apk.name, 'sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              'size_bytes': args.apk.stat().st_size, 'abis': {}, 'physical_device_tested': False,
              'stable_release_approved': False}
    assets = expected_assets()
    with zipfile.ZipFile(args.apk) as z:
        assert z.testzip() is None
        for name in ['classes.dex','AndroidManifest.xml','resources.arsc']:
            assert z.getinfo(name).file_size > 0
        for abi in args.abis.split(','):
            data = z.read(f'lib/{abi}/libmain.so')
            assert z.getinfo(f'lib/{abi}/libSDL2.so').file_size > 0
            assert struct.unpack_from('<H',data,18)[0] == {'armeabi-v7a':40,'arm64-v8a':183,'x86_64':62}[abi]
            syms = symbols(data)
            assert b'Game decompiled by:' not in data, 'PC intro remains in Android payload'
            assert not any(n.startswith('__wrap_') for n in syms), 'test harness must not ship'
            assert 'Java_org_libsdl_app_SA2Activity_nativeOptions' in syms
            checks = {}
            for name, expected in assets.items():
                assert syms.get(name) == expected, f'{abi}: packaged atlas mismatch: {name}'
                checks[name] = {'bytes':len(expected),'sha256':hashlib.sha256(expected).hexdigest()}
            report['abis'][abi] = {'seven_characters_byte_verified':True,'pc_intro_absent':True,'assets':checks}
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: APK integrity, native architectures, no PC intro/harness, JNI settings and seven exact OC atlas/palette payloads')

if __name__ == '__main__':
    main()
