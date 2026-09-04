#!/usr/bin/env bash
# smoke_run.sh - chay trong WSL: rebuild pc+gb300 roi smoke-test core PC
#   wsl bash -lc 'cd "<repo>" && bash tools/smoke_run.sh'
set -e
cd "$(dirname "$0")/.."

echo "== REBUILD =="
make clean >/dev/null 2>&1
make pc -j4 >/tmp/bp.log 2>&1 || { echo "PC BUILD FAIL"; tail -30 /tmp/bp.log; exit 1; }
make gb300 -j4 >/tmp/bg.log 2>&1 || { echo "GB300 BUILD FAIL"; tail -30 /tmp/bg.log; exit 1; }
echo "REBUILD-OK pc+gb300 (leapui.mars $(wc -c < build/dartos/leapui.mars) bytes)"

echo "== SANDBOX =="
S=/tmp/leapui_smoke
rm -rf "$S"
mkdir -p "$S/ROMS/Game Boy Advance" "$S/system/assets/LeapUI"
head -c 4096 /dev/urandom > "$S/ROMS/Game Boy Advance/Pokemon Fire Red.gba"
head -c 2048 /dev/urandom > "$S/ROMS/Game Boy Advance/zelda minish.gba"
mkdir -p "$S/ROMS/Game Boy Advance/.res"
python3 tools/make_test_thumb.py "$S/ROMS/Game Boy Advance/.res/Pokemon Fire Red.rgb565" 196 100
ls "$S/ROMS/Game Boy Advance/"

echo "== BUILD HARNESS =="
gcc -O0 -g -Wall -I"$(pwd)/include" tools/smoke_libretro.c -o /tmp/smoke_libretro -ldl
echo "HARNESS-BUILT"

echo "== RUN =="
DUMP="$(pwd)/tools/frame_ui.rgb565"
/tmp/smoke_libretro "$(pwd)/build/unix/leapui_libretro.so" "$S" "$DUMP"
echo "HARNESS_EXIT=$?"
if [ -f "$DUMP" ]; then
  python3 tools/rgb565_to_png.py "$DUMP" "$(pwd)/tools/frame_ui.png" 3
fi
