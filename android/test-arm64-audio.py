#!/usr/bin/env python3
"""Execute the shipped ARM64 audio code, independently of the x86 Android UI.

Unicorn runs the real NDK instructions and relocated tables. Only imported
libc memory helpers are provided by the host. Android/SDL audio output and
physical-device performance are outside this test's scope.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

from native_audio_check import NativeElf, verify_audio_tables
from unicorn import Uc, UcError, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm64_const import (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2,
                               UC_ARM64_REG_X30, UC_ARM64_REG_SP, UC_ARM64_REG_PC,
                               UC_ARM64_REG_CPACR_EL1)

BASE = 0x10000000
STACK = 0x50000000
STUBS = 0x60000000
RETURN = 0x70000000


class ArmAudio:
    def __init__(self, data):
        self.elf = NativeElf(data)
        assert self.elf.machine == 183 and self.elf.pointer_size == 8, 'ARM64 library required'
        self.cpu = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.cpu.reg_write(UC_ARM64_REG_CPACR_EL1, 3 << 20)
        end = max(address + memsz for address, memsz, _, _ in self.elf.loads)
        self.cpu.mem_map(BASE, (end + 4095) & ~4095)
        for address, _, offset, size in self.elf.loads:
            self.cpu.mem_write(BASE + address, data[offset:offset + size])
        self.cpu.mem_map(STACK, 0x100000)
        self.cpu.mem_map(STUBS, 0x100000)
        self.cpu.mem_map(RETURN, 4096)
        self.imports = {}
        self.names = {}
        for address, relocation in self.elf.relocations.items():
            value = self.elf.relocated_value(relocation, BASE, self.external)
            self.cpu.mem_write(BASE + address, struct.pack('<Q', value))
        self.cpu.hook_add(UC_HOOK_CODE, self.import_call, begin=STUBS, end=STUBS + 0xfffff)
        self.cpu.hook_add(UC_HOOK_MEM_INVALID, self.invalid_memory)
        self.fault = None
        self.memory_helpers = set()

    def external(self, name):
        if name not in self.imports:
            address = STUBS + 16 * len(self.imports)
            self.imports[name] = address
            self.names[address] = name
            self.cpu.mem_write(address, bytes.fromhex('c0035fd6'))  # ret
            if name == '__stack_chk_guard':
                self.cpu.mem_write(address, struct.pack('<Q', 0x123456789abcdef0))
        return self.imports[name]

    def import_call(self, cpu, address, size, _):
        name = self.names.get(address)
        dst, source, count = (cpu.reg_read(r) for r in (UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2))
        if name in ('memcpy', 'memmove', '__memcpy_chk', '__memmove_chk'):
            cpu.mem_write(dst, bytes(cpu.mem_read(source, count)))
        elif name in ('memset', '__memset_chk'):
            cpu.mem_write(dst, bytes([source & 255]) * count)
        else:
            raise AssertionError('Audio unexpectedly called host import: ' + str(name))
        self.memory_helpers.add(name)
        cpu.reg_write(UC_ARM64_REG_PC, cpu.reg_read(UC_ARM64_REG_X30))

    def invalid_memory(self, cpu, access, address, size, value, _):
        self.fault = {'pc': hex(cpu.reg_read(UC_ARM64_REG_PC) - BASE),
                      'address': hex(address), 'access': access, 'size': size}
        return False

    def address(self, name):
        return BASE + self.elf.address(name)

    def call(self, name, *args):
        self.cpu.reg_write(UC_ARM64_REG_SP, STACK + 0x100000 - 16)
        self.cpu.reg_write(UC_ARM64_REG_X30, RETURN)
        for register, value in zip((UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2), args):
            self.cpu.reg_write(register, value)
        self.cpu.emu_start(self.address(name), RETURN, timeout=5_000_000, count=10_000_000)
        assert self.cpu.reg_read(UC_ARM64_REG_PC) == RETURN, name + ': instruction/time budget exhausted'
        return self.cpu.reg_read(UC_ARM64_REG_X0)

    def u32(self, address):
        return struct.unpack('<I', self.cpu.mem_read(address, 4))[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path, help='unstripped ELF, libmain.so or the actual ARM64 APK')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-rc1-crash', action='store_true', help='negative control: require the exact reported PC/address')
    args = parser.parse_args()
    if zipfile.is_zipfile(args.binary):
        with zipfile.ZipFile(args.binary) as z:
            data = z.read('lib/arm64-v8a/libmain.so')
    else:
        data = args.binary.read_bytes()
    runner = ArmAudio(data)
    report = {'binary': args.binary.name, 'arm64_lib_sha256': hashlib.sha256(data).hexdigest(),
              'physical_device_tested': False, 'android_audio_output_tested': False}
    try:
        runner.call('cgb_audio_init', 48000)
        runner.call('m4aSoundInit')
    except UcError:
        report['fault'] = runner.fault
        if not args.expect_rc1_crash:
            args.output.write_text(json.dumps(report, indent=2) + '\n')
            raise
        assert runner.fault and runner.fault['pc'] == '0x6a4340' and runner.fault['address'] == '0x20', runner.fault
        report['status'] = 'expected_rc1_crash_reproduced'
        args.output.write_text(json.dumps(report, indent=2) + '\n')
        print('PASS: original ARM64 RC1 reproduces m4aSoundInit+572, PC 0x6a4340, null write at 0x20')
        return
    assert not args.expect_rc1_crash, 'negative control must reproduce the original crash'
    report['audio_tables'] = verify_audio_tables(runner.elf)
    # The same four player states used by the game must all be initialized.
    for name in ('BGM', 'SE1', 'SE2', 'SE3'):
        at = runner.address('gMPlayInfo_' + name)
        assert runner.u32(at + 72) == 0x68736d53, name + ': player lock/ABI incorrect'
        assert bytes(runner.cpu.mem_read(at + 56, 8)) == struct.pack('<Q', runner.address('gMPlayTrack_' + name))
    mixer = runner.address('gSoundInfo')
    size = runner.elf.symbols['gSoundInfo'].size
    pcm_bytes = 4907 * 2 * 4
    pcm = mixer + size - pcm_bytes
    cgb = runner.call('cgb_get_buffer')
    cycles = 1
    checks = []
    # Original intro, level, special stage and effect songs exercise different
    # headers, sequencers, voicegroups and player slots, using real mixer code.
    for song in (1, 13, 51, 102):
        runner.call('m4aMPlayAllStop')
        runner.call('m4aSoundInit')
        cycles += 1
        runner.cpu.mem_write(cgb, bytes(1600 * 4))
        runner.call('m4aSongNumStart', song)
        entry = runner.elf.address('gSongTable') + song * 16
        player = struct.unpack('<H', runner.elf.read(entry + 8, 2))[0]
        name = ('BGM', 'SE1', 'SE2', 'SE3')[player]
        state = runner.address('gMPlayInfo_' + name)
        assert bytes(runner.cpu.mem_read(state, 8)) == struct.pack('<Q', BASE + runner.elf.pointer(entry))
        audible = False
        for _ in range(90):
            runner.call('m4aSoundMain')
            # SoundMain selects a DMA block from dmaCounter; it need not be
            # block zero. Include all freshly cleared PCM blocks and the real
            # PSG buffer, so a PSG-only passage also generates a valid signal.
            audible |= any(runner.cpu.mem_read(pcm, pcm_bytes)) or any(runner.cpu.mem_read(cgb, 1600 * 4))
        assert audible, f'song {song}: real mixer produced no PCM samples'
        clock = runner.u32(state + 16)
        assert clock > 0, f'song {song}: native sequencer did not advance'
        checks.append({'song': song, 'ticks': 90, 'nonzero_pcm': audible, 'sequencer_clock': clock})
    runner.call('m4aMPlayAllStop')
    runner.call('m4aSoundInit')
    cycles += 1
    report.update(status='passed', checks=checks, initialization_cycles=cycles,
                  memory_helpers=sorted(runner.memory_helpers))
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: packaged ARM64 m4aSoundInit six times, four real players and 360 mixer ticks with nonzero PCM')


if __name__ == '__main__':
    main()
