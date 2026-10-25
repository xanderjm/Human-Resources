#!/usr/bin/env python3
"""Check every shipped ELF64 library's 16 KiB LOAD and RELRO layout."""
from pathlib import Path
import argparse
import struct
import zipfile

PAGE = 16384


def audit_elf(data: bytes, name: str) -> None:
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01':
        raise ValueError(f'Expected little-endian ELF64: {name}')
    offset = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    if size < 56 or count == 0 or count > 256 or offset + size * count > len(data):
        raise ValueError(f'Invalid ELF program headers: {name}')
    headers = [struct.unpack_from('<IIQQQQQQ', data, offset + index * size) for index in range(count)]
    loads = [record for record in headers if record[0] == 1]
    if not loads:
        raise ValueError(f'ELF has no LOAD segments: {name}')
    for _, _, file_offset, address, _, file_size, memory_size, alignment in loads:
        if alignment < PAGE or alignment & (alignment - 1) or file_offset % alignment != address % alignment:
            raise ValueError(f'ELF LOAD is not 16 KiB aligned: {name}')
        if file_size > memory_size or file_offset + file_size > len(data):
            raise ValueError(f'Invalid ELF LOAD bounds: {name}')
    for record in headers:
        if record[0] != 0x6474e552:  # PT_GNU_RELRO
            continue
        start, end = record[3], record[3] + record[6]
        if end % PAGE == 0:
            continue
        rounded_end = (end + PAGE - 1) // PAGE * PAGE
        # A non-aligned RELRO end is valid when its LOAD contains only RELRO
        # and following writable LOADs begin beyond its page protection range.
        for load in loads:
            writable = load[1] & 2
            writable_start = max(load[3], end)
            writable_end = load[3] + load[6]
            if writable and writable_start < min(writable_end, rounded_end):
                raise ValueError(f'ELF RELRO page overlaps writable data: {name}')
        if not any(load[3] <= start and end <= load[3] + load[6] for load in loads):
            raise ValueError(f'ELF RELRO is outside LOAD segments: {name}')


def audit_archive(path: Path, prefix: str = '') -> int:
    with zipfile.ZipFile(path) as archive:
        names = [entry.filename for entry in archive.infolist() if not entry.is_dir()]
        native = [name for name in names if name.startswith(prefix + 'lib/') and name.endswith('.so')]
        expected = {prefix + f'lib/{abi}/{library}' for abi in ('arm64-v8a', 'x86_64')
                    for library in ('libmain.so', 'libSDL3.so', 'libc++_shared.so')}
        if set(native) != expected or len(native) != len(expected):
            raise ValueError('Native ABI/library inventory differs from explicit product set')
        for name in sorted(native):
            audit_elf(archive.read(name), name)
            print(f'16 KiB ELF LOAD/RELRO passed: {name}')
        return len(native)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--prefix', default='')
    arguments = parser.parse_args()
    audit_archive(arguments.archive, arguments.prefix)
