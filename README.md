![Banner](/tools/frame_ui.png)
# LeapUI

A minimal **GBA-only frontend core** (libretro) for **NocturnalRTOS / DartOS** handhelds
(SF2000, GB300, DY19, ...). It borrows the libretro wrapper + `core_api` from
**FrogUI**, the carousel/slot UX from **Slot**, and stays deliberately small:
scan GBA ROMs, show a carousel with box art, launch the game through **gpSP**.

```
FrogUI = libretro + RETRO_ENVIRONMENT_RUN_EMULATOR + filesystem
Slot   = carousel L/R + slot chrome + theme housing/recess
LeapUI = FrogUI core_api + Slot-style shelf (GBA only) + green-accent UI
```

Built against the 2026-08 `NocturnalRTOS`/`Argent-Loader`/`Argent-Cores` line:
the device core is now **`.mars`** at `system/Phobos/cores` (`.hcrtos` kept for
backward compatibility).

## Features

- **GBA only**: scans `ROMS/Game Boy Advance/*.gba` (matches the
  `console_mappings.opt` mapping `Game Boy Advance = gpSP`); falls back to
  scanning `ROMS/` directly when the folder is empty (up to 512 ROMs).
- **Green-accent UI**: uniform dark background, centered banner `200x104`
  (image area `196x100`, matching the GBA cart sticker ratio 43:22 = 508x260)
  with a blinking border; shows `ROMS/Game Boy Advance/.res/<rom>.rgb565` box
  art when present (any standard FrogUI size 64/128/160/200/250x200 plus the
  GBA label size, **fit without distortion**); otherwise a `GAMEBOY ADVANCED`
  logo + ROM name pill (`10px` tall).
- **Side preview**: two `32x64` tiles (left/right) gently bobbing, showing the
  first character of the neighbor ROM.
- **Animation**: blinking banner border (`60f`), `1px` horizontal slide, side
  bob (`bob/10`), springy shelf (`k=14 d=8`).
- **Bundled 8x8 bitmap font** (96 ASCII chars, no external font file) in
  `font.c`, <60 KB.
- **Slot-style navigation**: `L/R` (or `LEFT/RIGHT`) wraps around the shelf;
  `A` tap (<500 ms) = start/resume game; `A` hold ≥500 ms = clean boot
  (removes `Saves/<stem>.state` + `.state.auto`); `SELECT/START` = About
  (exit with `A`/`B`); `B` is a no-op on the shelf.
- **Theme (scaffold)**: `theme_load()` reads `system/assets/LeapUI/theme.txt`
  (assets dir from `GET_CORE_ASSETS_DIRECTORY`, fallback
  `system/assets/LeapUI` → `HCRTOS/assets/LeapUI`), Slot-style keys
  `housing`, `recess`, `opening`, `edge` + hex colors. **Current state**:
  `theme.c` parses the file but rendering still uses the fixed palette in
  `render.h` (`C_BG*`/`C_BORDER`/`C_HOUSING`...) — `theme_housing()/...` are
  not wired into the renderer yet.
- **Persistence + logs**: `last_cart.txt` (last game for resume) lives under
  `system/assets/LeapUI/`; `leapui.log` follows the firmware convention and is
  written to `system/logs/` (next to `Phobos.log`), recording
  `roms/gba/assets`, `shelf_scan`, `queue_insert`, `ROM/CORE check` and
  `RUN_EMULATOR ret`.

## Repository layout

```
src/leapui.c            retro_init/build_paths + ROM/CORE check + run-game queue
src/shelf.c             ROM scanning + carousel state
src/render.c            backdrop/header/footer, banner, box art (thumb_load)
src/font.c              8x8 bitmap ASCII font (self-contained)
src/theme.c             theme.txt parser (not yet wired to renderer)
src/core_api.c          stock Argent-Cores API glue — DO NOT EDIT
src/frontend_functions.c  stock Argent-Cores loader glue — DO NOT EDIT
src/fallback_functions.c  RUN_EMULATOR path (dartos.h + core/rom buffers)
include/                headers incl. bare-metal dirent.h shim + libretro.h
tools/                  smoke test harness + PNG tools (see "Testing on PC")
build/                  generated artifacts (gitignored): build/{unix,dartos}
.github/workflows/      CI: builds dartos + unix, uploads on push/release
```

## SD card layout

```
SD:/
  ROMS/Game Boy Advance/*.gba          <- main scan (max 512; empty -> fallback ROMS/)
  ROMS/Game Boy Advance/.res/*.rgb565  <- raw RGB565 box art (64x64/128x128/160x160/
                                          200x200/250x200/196x100...), fit, no distortion
  ROMS/LeapUI/leap.ui                  <- copy of leapui.mars (per /roms/LeapUI/leap.ui)
  system/Phobos/cores/leapui.mars      <- core (make gb300 -> build/dartos/leapui.mars, copy here)
  system/Phobos/cores/gpSP.mars        <- game core - from Argent-Cores (legacy: HCRTOS/cores/gpSP.hcrtos)
  system/bios/gba_bios.bin
  system/assets/LeapUI/theme.txt, last_cart.txt
  system/saves/*.srm, *.state
  system/configs/dartos.opt            <- make install writes: hcrtos_core_path="leapui"
  system/logs/Phobos.log, leapui.log   <- leapui.log next to the firmware log
  # legacy kept for compatibility:
  HCRTOS/cores/leapui.mars + .hcrtos
  HCRTOS/assets/LeapUI/theme.txt
```

### Box art (.res/.rgb565)

Convert a PNG to RGB565 (needs Pillow: `pip install pillow`); any size from the
`thumb_load` list works (64/128/160/200 square, 250x200, 196x100, ...) — art is
fit into the banner without distortion:

```bash
python3 - <<'PY'
import struct
from PIL import Image
im = Image.open('label.png').convert('RGB').resize((196, 100))
with open('label.rgb565', 'wb') as f:
    for r, g, b in im.getdata():
        f.write(struct.pack('<H', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
PY
# drop it at ROMS/Game Boy Advance/.res/<rom-name>.rgb565 (same name as the .gba)
```

## Building

Requires the `frog-toolchain` `mipsel-mti-elf` GCC 16.2 / binutils 2.47 /
newlib 4.6 (bare-metal). Run `make toolchain` once to download it into
`./x-tools` (~150 MB; the toolchain is **not** committed). On Windows run
everything through **WSL** (the toolchain binaries are Linux ELF):

```bash
# inside the LeapUI folder (WSL example: cd '/mnt/c/Users/<user>/.../LeapUI')
make pc -j4     # = platform=pc / unix      -> build/unix/leapui_libretro.so   (PC test, needs gcc)
make gb300 -j4  # = platform=gb300 / dartos -> build/dartos/leapui.mars + .hcrtos (device core)
```

- `make` (no arguments) auto-detects: MIPS toolchain available → device build
  (`gb300`), otherwise → PC build.
- All artifacts live under `build/<platform>/` (root stays clean): `build/unix/`
  for `.o` + `.so`, `build/dartos/` for `.o` + `.a` + `core.elf` +
  `leapui.mars/.hcrtos`.
- `gb300` = `dartos` = one platform: NocturnalRTOS/DartOS runs on SF2000,
  GB300 and DY19 (only joypad/LCD init differ and the OS detects that at
  runtime, so **one core file works for all three**). `pc` = `unix` (desktop).
- Per-platform objects avoid mixing x86 and MIPS `.o` files; `make clean`
  removes `build/` plus any leftover root artifacts from older Makefiles.
- No MIPS toolchain handy? `make docker` builds in an alpine container.

Manual link (rarely needed — `make gb300` does exactly this):
```bash
mipsel-mti-elf-g++ -EL -march=mips32 -msoft-float -e __core_entry__ -T src/core.ld \
  -Wl,--start-group build/dartos/core_api.o build/dartos/frontend_functions.o build/dartos/_libretro_dartos.a \
  -lc -Wl,--end-group -Wl,--gc-sections -z max-page-size=32 -o build/dartos/core.elf
mipsel-mti-elf-objcopy -O binary -R .MIPS.abiflags -R .note.gnu.build-id -R ".rel*" \
  build/dartos/core.elf build/dartos/leapui.mars
cp build/dartos/leapui.mars build/dartos/leapui.hcrtos
```

## Installing on a device

1. Flash NocturnalRTOS (branch `Nocturnal`).
2. Mount the SD card and run `make install SDROOT=/media/<user>/<SD>` — it
   copies `build/dartos/leapui.mars` to `system/Phobos/cores/` and
   `ROMS/LeapUI/leap.ui`, and writes `system/configs/dartos.opt`
   (`hcrtos_core_path="leapui"`). Copying those three files manually works too.
3. Make sure a game core is present: `system/Phobos/cores/gpSP.mars` (from
   Argent-Cores). **Without gpSP the game launch fails at the `CORE check`
   log line** — the menu itself still runs.
4. Put GBA ROMs in `ROMS/Game Boy Advance/`; box art (optional) in
   `ROMS/Game Boy Advance/.res/<rom-name>.rgb565`.
5. Boot — LeapUI scans up to 512 ROMs; `L/R` (or `LEFT/RIGHT`) browses, `A`
   launches through `RETRO_ENVIRONMENT_RUN_EMULATOR` (`dartos.h` +
   `game_name_buf`/`core_name_buf` buffers).

## Testing on PC

No RetroArch needed:

```bash
bash tools/smoke_run.sh
```

This rebuilds both platforms, feeds a few fake ROMs to a minimal libretro
frontend (`tools/smoke_libretro.c`), exercises a full scan + one `A` press
(`queue_insert` → `ROM/CORE check` → `WRAP_RUN_GAME`) and renders the final
frame to `tools/frame_ui.png` (via `tools/rgb565_to_png.py`) so you can eyeball
the UI. `tools/make_test_thumb.py` generates a fake `.res` box-art file.

## Development notes

- `src/leapui.c` holds `retro_init` (build paths + font init), the run-game
  queue, and the `ROM/CORE check` logs (it checks both
  `system/Phobos/cores/gpSP.mars` and the legacy `HCRTOS/cores/gpSP.hcrtos`;
  log only, never blocks launch). The device-side paths default to
  `/media/mmcblk0p2` when the frontend does not provide directories.
- `fallback_functions.c` includes `dartos.h` and calls
  `RETRO_ENVIRONMENT_RUN_EMULATOR` (`0x20000|3`) with
  `core_name_buf`/`game_name_buf` so the core does not fall into `SHUTDOWN`
  (old bug).
- `core_api.c` / `frontend_functions.c` are **stock Argent-Cores** — do not
  edit them: any API change must be mirrored in the loader, so custom work
  belongs inside the core itself.
- `include/dirent.h` is a shim for the device build (`frontend_functions.c`
  implements `readdir` with that layout); the PC build (`unix`) instead does
  `#include_next` on the host `<dirent.h>` (glibc layout differs — using the
  shim on Linux would misread `d_name`).
- `src/mmap_stub.c` is a leftover from an abandoned zig/musl build path and is
  **not compiled** by the Makefile.
- `leapui.log` is written to `system/logs/` (firmware convention, next to
  `Phobos.log`) for debugging boot loops.

## Credits

- **FrogUI** (Data-Frog-Central) — libretro wrapper + `core_api` + filesystem
  glue that LeapUI builds on.
- **Slot** — carousel/slot UX (L/R shelf, insert/eject, theme housing/recess)
  that inspired the LeapUI shelf.
- **Desoxyn** — author of **NocturnalRTOS**; also helped debug the boot loop:
  tracing `R31` (`$ra`) to locate the faulting function in `core.elf` and
  identifying the core-side cause.
- **AxelGarciaK (axgdev)** — pointing out `RETRO_ENVIRONMENT_RUN_EMULATOR` +
  `dartos.h` (why the core fell into `SHUTDOWN`), and steering the build away
  from the experimental zig/musl path.
- **Freebuff (AI)** — the coding agent used to build and debug this project
  (source edits, smoke tests, and this repo's tooling).

## License

ISC — see [LICENSE](LICENSE).
