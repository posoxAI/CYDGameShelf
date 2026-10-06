#!/bin/sh
# Builds the games for this computer and runs the tests. Screen pictures land in sim/out as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p sim/out
g++ -std=gnu++11 -O2 -Wall -Wextra -Werror -Wformat=2 -DCYD_SIM -o sim/out/sim src/*.cpp sim/*.cpp
./sim/out/sim sim/out
python3 - <<'PY'
# The screens come out as PPM; turn them into PNG with nothing but the standard library.
import pathlib, struct, zlib

def chunk(tag, data):
    return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

def to_png(ppm):
    if not ppm.startswith(b'P6'):
        raise ValueError('not a P6 file')
    fields, at = [], 2
    while len(fields) < 3:                       # width, height, maximum value
        while ppm[at:at + 1].isspace(): at += 1
        if ppm[at:at + 1] == b'#':
            while ppm[at:at + 1] not in (b'\n', b''): at += 1
            continue
        start = at
        while not ppm[at:at + 1].isspace(): at += 1
        fields.append(int(ppm[start:at]))
    w, h, _ = fields
    px = ppm[at + 1:]
    rows = b''.join(b'\x00' + px[y * w * 3:(y + 1) * w * 3] for y in range(h))
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 9))
            + chunk(b'IEND', b''))

out = pathlib.Path('sim/out')
for p in sorted(out.glob('*.ppm')):
    p.with_suffix('.png').write_bytes(to_png(p.read_bytes()))
    p.unlink()
PY
