#!/usr/bin/env python3
"""Reserve SA1 ROM assets and register their source ranges after C preprocessing.

Only active .incbin directives reach this filter, so Android's disabled demo and
multiboot data stay disabled. No ROM is read at build time. Existing labels and
portable pointer relocations are retained exactly as in the normal assembly.
"""
import argparse
import re
import sys

ROM_SIZE = 8 * 1024 * 1024
INCBIN = re.compile(
    r'^\s*\.incbin\s+"baserom_sa1\.gba"\s*,\s*(0x[\da-fA-F]+|\d+)\s*,\s*(0x[\da-fA-F]+|\d+)\s*(?:[@;].*)?$',
    re.MULTILINE,
)


def transform(source: str, pointer_size: int) -> str:
    matches = list(INCBIN.finditer(source))
    if not matches:
        if '"baserom_sa1.gba"' in source:
            raise ValueError("unhandled SA1 ROM directive")
        return source
    # RELRO is read-only once Android's dynamic linker finishes. Imported data
    # must instead live in writable .data, including the labels preceding it.
    source = re.sub(r'\bmSectionRodata\b', '.data', source)
    pointer = '.quad' if pointer_size == 8 else '.long'
    index = 0

    def replace(match):
        nonlocal index
        offset, size = (int(value, 0) for value in match.groups())
        if size <= 0 or offset < 0 or offset + size > ROM_SIZE:
            raise ValueError("SA1 ROM asset range outside the supported ROM")
        label = f'.Lsa1_import_{index}'
        index += 1
        return (
            f'{label}:\n    .space {size}, 0\n'
            '    .pushsection sa1_rom_assets,"aw",%progbits\n'
            f'    .balign {pointer_size}\n    {pointer} {label}\n'
            f'    .long {offset}\n    .long {size}\n    .popsection'
        )

    result = INCBIN.sub(replace, source)
    if '"baserom_sa1.gba"' in result:
        raise ValueError("unhandled SA1 ROM directive")
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pointer-size', type=int, choices=(4, 8), default=4)
    args = parser.parse_args()
    try:
        sys.stdout.write(transform(sys.stdin.read(), args.pointer_size))
    except ValueError as error:
        sys.exit(str(error))
