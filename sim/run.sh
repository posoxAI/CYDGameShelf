#!/bin/sh
# Builds the games for this computer and runs the tests. Screen pictures land in sim/out as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p sim/out
g++ -std=gnu++11 -O2 -Wall -Wextra -Werror -Wformat=2 -DCYD_SIM -o sim/out/sim src/*.cpp sim/*.cpp
./sim/out/sim sim/out
python3 - <<'PY'
import pathlib
from PIL import Image
out = pathlib.Path('sim/out')
for p in sorted(out.glob('*.ppm')):
    Image.open(p).save(p.with_suffix('.png')); p.unlink()
PY
