#!/usr/bin/env python3
"""Verify SA1's runtime asset registry in the linked ELF before packaging."""
import argparse
import json
from pathlib import Path
import struct


def check_sa1_assets(path):
    elf = Path(path).read_bytes()
    if elf[:4] != b'\x7fELF' or elf[5] != 1 or elf[4] not in (1, 2):
        raise ValueError("Expected a little-endian ELF library")
    is64 = elf[4] == 2
    shoff = struct.unpack_from('<Q' if is64 else '<I', elf, 40 if is64 else 32)[0]
    shentsize, count, strings_index = struct.unpack_from('<HHH', elf, 58 if is64 else 46)
    sections = [struct.unpack_from('<IIQQQQIIQQ' if is64 else '<10I', elf, shoff + i * shentsize) for i in range(count)]
    string_section = sections[strings_index]
    strings = elf[string_section[4]:string_section[4] + string_section[5]]
    named = {strings[s[0]:strings.index(b'\0', s[0])].decode(): s for s in sections}
    registry = named.get('sa1_rom_assets')
    stride = 16 if is64 else 12
    if registry is None or not registry[5] or registry[5] % stride or registry[2] & 3 != 3:
        raise ValueError("Missing or invalid writable SA1 asset registry")
    phoff = struct.unpack_from('<Q' if is64 else '<I', elf, 32 if is64 else 28)[0]
    phentsize, phcount = struct.unpack_from('<HH', elf, 54 if is64 else 42)
    headers = [struct.unpack_from('<IIQQQQQQ' if is64 else '<8I', elf, phoff + i * phentsize) for i in range(phcount)]
    segments = [(h[0], h[3] if is64 else h[2], h[6] if is64 else h[5], h[1] if is64 else h[6]) for h in headers]
    total = 0
    for at in range(registry[4], registry[4] + registry[5], stride):
        destination, offset, size = struct.unpack_from('<QII' if is64 else '<III', elf, at)
        if size == 0 or offset + size > 8 * 1024 * 1024:
            raise ValueError("SA1 source range exceeds the supported ROM")
        target = next((s for s in sections if s[1] == 1 and s[2] & 3 == 3
                       and s[3] <= destination and destination + size <= s[3] + s[5]), None)
        if target is None:
            raise ValueError("SA1 destination is outside writable data")
        if not any(kind == 1 and flags & 2 and start <= destination
                   and destination + size <= start + length for kind, start, length, flags in segments):
            raise ValueError("SA1 destination is outside a writable load segment")
        if any(kind == 0x6474E552 and destination < start + length
               and destination + size > start for kind, start, length, flags in segments):
            raise ValueError("SA1 destination is in linker-protected RELRO memory")
        start = target[4] + destination - target[3]
        if elf[start:start + size] != bytes(size):
            raise ValueError("SA1 reserved data must be empty until the user's ROM is imported")
        total += size
    return {'asset_ranges': registry[5] // stride, 'asset_bytes': total, 'elf_bits': 64 if is64 else 32}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('library')
    args = parser.parse_args()
    try:
        print(json.dumps(check_sa1_assets(args.library)))
    except (ValueError, IndexError, struct.error) as error:
        parser.exit(1, f'SA1 native asset validation failed: {error}\n')
