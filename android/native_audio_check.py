"""Inspect the C/assembler audio ABI in the actual linked Android library.

Use dynamic relocations as Android's linker does; pointer bytes in a shared
library alone do not necessarily contain their final values. No target code
or third-party Python packages are needed for this structural check.
"""
from dataclasses import dataclass
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Symbol:
    name: str
    value: int
    size: int
    section: int


class NativeElf:
    def __init__(self, data):
        self.data = data
        assert data[:4] == b'\x7fELF' and data[5] == 1, 'little-endian ELF required'
        self.pointer_size = 4 if data[4] == 1 else 8
        self.machine = self.unpack('H', 18)[0]
        bits32 = self.pointer_size == 4
        phoff = self.unpack('I' if bits32 else 'Q', 28 if bits32 else 32)[0]
        phsize, phcount = self.unpack('HH', 42 if bits32 else 54)
        self.loads = []
        for i in range(phcount):
            at = phoff + i * phsize
            if self.unpack('I', at)[0] != 1:
                continue
            if bits32:
                offset, address, _, filesz, memsz = self.unpack('IIIII', at + 4)
            else:
                offset, address, _, filesz, memsz = self.unpack('QQQQQ', at + 8)
            self.loads.append((address, memsz, offset, filesz))
        shoff = self.unpack('I' if bits32 else 'Q', 32 if bits32 else 40)[0]
        shsize, shcount = self.unpack('HH', 46 if bits32 else 58)
        self.sections = []
        for i in range(shcount):
            at = shoff + i * shsize
            typ = self.unpack('I', at + 4)[0]
            if bits32:
                offset, size, link = self.unpack('III', at + 16)
                entry = self.unpack('I', at + 36)[0]
            else:
                offset, size = self.unpack('QQ', at + 24)
                link = self.unpack('I', at + 40)[0]
                entry = self.unpack('Q', at + 56)[0]
            self.sections.append((typ, offset, size, link, entry))
        self.symbols = {}
        self.symbol_tables = {}
        for index, (typ, offset, size, link, entry) in enumerate(self.sections):
            if typ != 11:  # SHT_DYNSYM survives release stripping.
                continue
            _, string_offset, string_size, _, _ = self.sections[link]
            names = data[string_offset:string_offset + string_size]
            table = []
            for at in range(offset, offset + size, entry):
                if bits32:
                    name, value, length, _, _, section = self.unpack('IIIBBH', at)
                else:
                    name, _, _, section, value, length = self.unpack('IBBHQQ', at)
                name = names[name:names.index(b'\0', name)].decode()
                symbol = Symbol(name, value, length, section)
                table.append(symbol)
                if section:
                    self.symbols[name] = symbol
            self.symbol_tables[index] = table
        self.relocations = {}
        for typ, offset, size, link, entry in self.sections:
            if typ not in (4, 9):  # SHT_RELA / SHT_REL
                continue
            table = self.symbol_tables[link]
            for at in range(offset, offset + size, entry):
                address, info = self.unpack('II' if bits32 else 'QQ', at)
                if typ == 4:
                    addend = self.unpack('i' if bits32 else 'q', at + 2 * self.pointer_size)[0]
                else:
                    addend = int.from_bytes(self.read(address, self.pointer_size), 'little')
                number, symbol_index = ((info & 255, info >> 8) if bits32 else (info & 0xffffffff, info >> 32))
                self.relocations[address] = (number, table[symbol_index], addend)

    def unpack(self, fmt, offset):
        return struct.unpack_from('<' + fmt, self.data, offset)

    def read(self, address, size):
        for start, memsz, offset, filesz in self.loads:
            if start <= address and address + size <= start + memsz:
                file_bytes = max(0, min(size, start + filesz - address))
                begin = offset + address - start
                return self.data[begin:begin + file_bytes] + bytes(size - file_bytes)
        raise AssertionError(f'ELF address outside loadable memory: {address:#x}+{size}')

    def address(self, name):
        assert name in self.symbols, 'missing native symbol: ' + name
        return self.symbols[name].value

    def relocated_value(self, relocation, base=0, external=None):
        number, symbol, addend = relocation
        types = {183: (257, 1025, 1026, 1027), 62: (1, 6, 7, 8), 40: (2, 21, 22, 23)}[self.machine]
        assert number in types, f'unsupported pointer relocation {number}'
        if number == types[3]:
            return base + addend
        if symbol.section == 0:
            assert external is not None, 'unresolved audio pointer: ' + symbol.name
            target = external(symbol.name)
        else:
            target = symbol.value + (0 if symbol.section == 0xfff1 else base)
        return target + (addend if number == types[0] else 0)

    def pointer(self, address):
        if address in self.relocations:
            return self.relocated_value(self.relocations[address])
        return int.from_bytes(self.read(address, self.pointer_size), 'little')


def aligned(value, alignment):
    return (value + alignment - 1) // alignment * alignment


def verify_audio_tables(elf):
    p = elf.pointer_size
    labels = {name: symbol.value for name, symbol in elf.symbols.items()}

    def expect_pointer(at, name, message):
        value = elf.pointer(at)
        name = re.sub(r'C_DECL\((\w+)\)', r'\1', name)
        if re.fullmatch(r'(?:0x[0-9a-fA-F]+|\d+)', name):
            expected_value = int(name, 0)
        else:
            # Local assembler labels are stripped from release dynsym. Their
            # relocation must still resolve to readable, non-null data, and
            # every later use of that label must resolve identically.
            assert value, message + ': null pointer'
            elf.read(value, 1)
            expected_value = labels.setdefault(name, value)
        assert value == expected_value, message
        return value

    table = elf.address('gMPlayTable')
    stride = aligned(2 * p + 4, p)
    for i, name in enumerate(['BGM', 'SE1', 'SE2', 'SE3']):
        at = table + i * stride
        assert elf.pointer(at) == elf.address('gMPlayInfo_' + name), f'audio player {i}: assembler/C stride mismatch'
        assert elf.pointer(at + p) == elf.address('gMPlayTrack_' + name), f'audio player {i}: incorrect track array'
        assert elf.read(at + 2 * p, 1) == b'\x10', f'audio player {i}: incorrect track count'
    expected = re.findall(r'^\s*song C_DECL\((\w+)\),\s*(\d+),\s*(\d+)',
                          (ROOT / 'sound/sa2/song_table.inc').read_text(), re.M)
    assert expected
    table = elf.address('gSongTable')
    stride = aligned(p + 4, p)
    headers = set()
    for i, (name, player, other) in enumerate(expected):
        at = table + i * stride
        header = expect_pointer(at, name, f'song {i}: assembler/C stride mismatch ({name})')
        assert elf.read(at + p, 4) == struct.pack('<HH', int(player), int(other)), f'song {i}: incorrect player'
        headers.add(header)
    tracks = 0
    for header in headers:
        count = elf.read(header, 1)[0]
        assert count <= 16, f'invalid song track count: {count}'
        if count:
            voice = elf.pointer(header + aligned(4, p))
            assert voice, 'song voicegroup must not be null'
            elf.read(voice, 1)
            for i in range(count):
                part = elf.pointer(header + aligned(4, p) + p * (i + 1))
                assert part, 'song track pointer must not be null'
                elf.read(part, 1)
                tracks += 1
    types = {'voice_directsound': 0, 'voice_directsound_no_resample': 8, 'voice_directsound_alt': 16,
             'voice_square_1': 1, 'voice_square_1_alt': 9, 'voice_square_2': 2, 'voice_square_2_alt': 10,
             'voice_programmable_wave': 3, 'voice_programmable_wave_alt': 11,
             'voice_noise': 4, 'voice_noise_alt': 12, 'voice_keysplit': 64, 'voice_keysplit_all': 128}
    voices = 0
    stride = aligned(aligned(4, p) + 2 * p, p)
    for file in sorted((ROOT / 'sound/sa2/voicegroups').glob('voicegroup*.inc')):
        entries = re.findall(r'^\s*(voice_\w+)\s+([^\n@]+)', file.read_text(), re.M)
        start = elf.address(file.stem)
        for i, (kind, arguments) in enumerate(entries):
            at = start + i * stride
            assert elf.read(at, 1)[0] == types[kind], f'{file.stem} voice {i}: C/assembler layout mismatch'
            args = [v.strip() for v in arguments.split(',')]
            pointers = ([args[2]] if kind.startswith('voice_directsound') else
                        [args[0]] if kind.startswith('voice_programmable_wave') else
                        args if kind == 'voice_keysplit' else [args[0]] if kind == 'voice_keysplit_all' else [])
            for j, name in enumerate(pointers):
                expect_pointer(at + aligned(4, p) + j * p, name, f'{file.stem} voice {i}: incorrect sample/keysplit pointer')
            voices += 1
    return {'players': 4, 'songs': len(expected), 'song_tracks': tracks, 'voices': voices,
            'native_pointer_bytes': p, 'assembler_matches_c_layout': True}
