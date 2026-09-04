# LeapUI - Minimal GBA-only Frontend for NocturnalRTOS (DartOS)

LeapUI là core **frontend** cho NocturnalRTOS/DartOS (HC32 B210 / SF2000, GB300) - lấy điều hướng từ **Slot** (carousel + insert/eject) và cách hoạt động/libretro wrapper từ **FrogUI**, nhưng chỉ **GBA (gpSP)** để tối giản.

```
FrogUI = libretro + RETRO_ENVIRONMENT_RUN_EMULATOR + filesystem
Slot   = carousel L/R + SlotChrome + theme housing/recess
LeapUI = FrogUI core_api + Slot shelf (GBA-only) + UI viền xanh
```

Cập nhật theo commit mới `2026-08`: `NocturnalRTOS`/`Argent-Loader`/`Argent-Cores` - output core giờ là **`.mars`** ở `system/Phobos/cores` (giữ `.hcrtos` cũ để tương thích).

## Tính năng (bản mới)

- **GBA-only**: scan `ROMS/Game Boy Advance/*.gba` (chuẩn `console_mappings.opt` `Game Boy Advance = gpSP`); thư mục trống thì fallback quét thẳng `ROMS/` (tối đa 512 ROM)
- **UI viền xanh**: nền đồng nhất màu tối, banner giữa `200x104` (lòng ảnh `196x100`, đúng tỉ lệ sticker GBA 43:22 = 508x260) viền nhấp nháy, hiện ảnh `.res/<tên rom>.rgb565` nếu có (nhận size chuẩn FrogUI 64/128/160/200/250x200 + size GBA label, **fit giữ tỉ lệ không bóp méo**), không có ảnh thì logo `GAMEBOY ADVANCED` + tên rom trong pill cao `10px`
- **Side preview**: 2 ô `32x64` trái/phải bob nhẹ, chỉ hiện ký tự đầu, đã xóa chữ `*game khac`
- **Animation**: viền banner nhấp nháy `60f`, trượt ngang `1px`, side bob `bob/10`, shelf spring `k=14 d=8`
- **Font 8x8 bitmap ascii tự làm** (96 chars, không dùng `GamePocket` nữa) - `font.c` tự chứa, <60KB
- **Điều hướng Slot**: `L/R` (hoặc `LEFT/RIGHT`) browse wrap; `A` (tap <500ms) = chọn game/resume, giữ `A` đủ 500ms = clean boot (xóa `Saves/<stem>.state` + `.state.auto`); `SELECT/START` = About (thoát bằng `A`/`B`); ở shelf thì `B` không có tác dụng
- **Theme (sườn)**: `theme_load()` đọc `system/assets/LeapUI/theme.txt` (assets dir do frontend cấp qua `GET_CORE_ASSETS_DIRECTORY`, fallback stat `system/assets/LeapUI` -> `HCRTOS/assets/LeapUI`), key kiểu Slot: `housing`, `recess`, `opening`, `edge` + màu hex. **Lưu ý hiện trạng**: `theme.c` đã parse nhưng render vẫn dùng palette cố định trong `render.h` (`C_BG*`/`C_BORDER`/`C_HOUSING`...) — hàm `theme_housing()/...` chưa được nối vào render.
- **Persist + log**: `last_cart.txt` + `leapui.log` ở `system/assets/LeapUI/` (fallback `HCRTOS/assets/LeapUI/`) ghi `roms/gba/assets`, `shelf_scan`, `queue_insert`, `ROM/CORE check`, `RUN_EMULATOR ret`

## SD Card Layout (mới `system`, giữ `HCRTOS` cũ để tương thích)

```
SD:/
  ROMS/Game Boy Advance/*.gba          <- scan chính (tối đa 512; trống -> fallback ROMS/)
  ROMS/Game Boy Advance/.res/*.rgb565  <- thumbnail raw RGB565 (64x64/128x128/160x160/200x200/250x200/196x100...), hiển thị fit giữ tỉ lệ
  ROMS/LeapUI/leap.ui                  <- copy của leapui.mars (theo yêu cầu /roms/LeapUI/leap.ui)
  system/Phobos/cores/leapui.mars      <- core mới (make gb300 -> build/dartos/leapui.mars, copy qua đây)
  system/Phobos/cores/gpSP.mars        <- core chạy game - lấy từ Argent-Cores (bản cũ: HCRTOS/cores/gpSP.hcrtos)
  system/bios/gba_bios.bin
  system/assets/LeapUI/theme.txt, leapui.log, last_cart.txt
  system/saves/*.srm, *.state
  system/configs/dartos.opt            <- make install tự ghi: hcrtos_core_path="leapui"
  system/logs/Phobos.log
  # cũ vẫn giữ (tương thích):
  HCRTOS/cores/leapui.mars + .hcrtos
  HCRTOS/assets/LeapUI/theme.txt, leapui.log
```

### Ảnh bìa (.res/.rgb565)

Chuyển ảnh PNG -> RGB565 (cần Pillow: `pip install pillow`); size khớp 1 trong các size `thumb_load` nhận (64/128/160/200 vuông, 250x200, 196x100...) — ảnh hiển thị fit giữ tỉ lệ:

```bash
python3 - <<'PY'
import struct, sys
from PIL import Image
im = Image.open('label.png').convert('RGB').resize((196, 100))
with open('label.rgb565', 'wb') as f:
    for r, g, b in im.getdata():
        f.write(struct.pack('<H', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
PY
# bo vao ROMS/Game Boy Advance/.res/<ten-rom>.rgb565 (ten trung ten .gba)
```

## Build

Cần `frog-toolchain` `mipsel-mti-elf` GCC 16.2 / binutils 2.47 / newlib 4.6 (bare-metal) — chạy `make toolchain` một lần để tải về `./x-tools` (~150 MB, toolchain không commit theo repo). Trên Windows phải chạy qua **WSL** (toolchain là ELF Linux, Git Bash Windows không chạy trực tiếp được):

```bash
# trong WSL Ubuntu, vào thư mục LeapUI:
cd <thư mục LeapUI>   # ví dụ: cd '/mnt/c/Users/<user>/.../LeapUI'

make pc -j4     # = make platform=pc / platform=unix  -> build/unix/leapui_libretro.so (test trên PC, cần gcc)
make gb300 -j4  # = make platform=gb300 / platform=dartos -> build/dartos/leapui.mars + leapui.hcrtos (cho GB300/SF2000)
```

- `make` (không tham số) tự chọn: có MIPS toolchain chạy được -> build device (`gb300`), không -> build PC.
- Tất cả artifact nằm trong `build/<platform>/` (không làm bẩn root): `.o` + `.so` ở `build/unix/`, `.o` + `.a` + `core.elf` + `leapui.mars/.hcrtos` ở `build/dartos/`. Copy lên SD: `make install SDROOT=/path/to/sd` (core vào `system/Phobos/cores/` + ghi `system/configs/dartos.opt`), hoặc copy tay `build/dartos/leapui.mars`.
- `gb300` = `dartos` = cùng một platform: NocturnalRTOS/DartOS chạy chung trên SF2000, GB300, DY19... (chỉ khác joypad/LCD init, OS tự detect lúc runtime nên **1 file core dùng cho cả 3**). `pc` = `unix` (desktop).
- Object tách riêng theo platform (`build/unix/`, `build/dartos/`) để không lẫn `.o` x86 với MIPS; `make clean` xóa `build/` + artifact cũ còn sót ở root.
- Test PC nhanh không cần RetroArch: `bash tools/smoke_run.sh` (rebuild rồi nạp `.so` qua frontend libretro tối giản, in log `shelf_scan` của core).

Build thủ công như `Argent-Cores` (ít khi cần — `make gb300` làm đúng các bước này, object nằm sẵn trong `build/dartos/`):
```bash
mipsel-mti-elf-g++ -EL -march=mips32 -msoft-float -e __core_entry__ -T src/core.ld \
  -Wl,--start-group build/dartos/core_api.o build/dartos/frontend_functions.o build/dartos/_libretro_dartos.a \
  -lc -Wl,--end-group -Wl,--gc-sections -z max-page-size=32 -o build/dartos/core.elf
mipsel-mti-elf-objcopy -O binary -R .MIPS.abiflags -R .note.gnu.build-id -R ".rel*" build/dartos/core.elf build/dartos/leapui.mars
cp build/dartos/leapui.mars build/dartos/leapui.hcrtos
```

`NocturnalRTOS`/`Argent-Loader`/`Argent-Cores` đều đã cập nhật: `phobos.h` mới dùng `SDCARD_DIRECTORY "/media/mmcblk0p2"` + `SYSTEM_DIRECTORY "/system/bios"` etc, `CORES_DIRECTORY` = `"/media/mmcblk0p2/system/Phobos/cores"` và `*.mars`.

## Cài lên SF2000/GB300/NocturnalRTOS

1. Flash NocturnalRTOS (branch `Nocturnal`).
2. Build xong, cắm thẻ SD và copy core lên thẻ: `make install SDROOT=/media/<user>/<SD>` (Linux/WSL) — tự copy `build/dartos/leapui.mars` vào `system/Phobos/cores/` + `ROMS/LeapUI/leap.ui` và ghi `system/configs/dartos.opt` (`hcrtos_core_path="leapui"`). Copy tay cũng được: 3 file/core đó.
3. Đảm bảo có core chạy game: `system/Phobos/cores/gpSP.mars` (lấy từ Argent-Cores) — **thiếu gpSP thì vào game sẽ fail ở `CORE check` trong log**, menu vẫn chạy được.
4. ROM GBA vào `ROMS/Game Boy Advance/`, thumb tuỳ chọn vào `ROMS/Game Boy Advance/.res/<tên rom>.rgb565`.
5. Boot — LeapUI quét tối đa 512 ROM, `L/R` (hoặc `LEFT/RIGHT`) duyệt, `A` chọn game qua `RETRO_ENVIRONMENT_RUN_EMULATOR` (`dartos.h` + `game_name_buf`/`core_name_buf` buffers).

## So sánh

|  | FrogUI | Slot | LeapUI (mới) |
|---|---|---|---|
| Scan | /ROMS/* mọi hệ | /Games/*.gba | /ROMS/Game Boy Advance/*.gba only |
| UI | list dọc | carousel SlotChrome | carousel viền xanh 200x104 + side 32x64 + 8x8 font + animation |
| Input | Up/Down, A/B, Select | L/R, Tap/Hold A, MENU | L/R, Tap/Hold A 500ms, SELECT About |
| Thumb | .res/*.rgb565 + WQW | Labels 196x86 | .res/*.rgb565 mọi size chuẩn, fit giữ tỉ lệ vào banner tỉ lệ sticker GBA 43:22 |
| Theme | theme.c | System/theme.txt | system/assets/LeapUI/theme.txt (mới) + HCRTOS fallback |
| Launch | RUN_EMULATOR | mGBA trực tiếp | RUN_EMULATOR core=gpSP (y hệt FrogUI, đã fix `dartos.h` include) |
| Output | .hcrtos | - | .mars (mới) + .hcrtos (compat) |

## Dev notes

- `src/leapui.c` chứa `retro_init` (build_paths + font_init sau), `shelf.c` scan, `render.c` backdrop/header/footer + center/side + thumb, `font.c` 8x8 bitmap tự làm (không dùng file font ngoài).
- `fallback_functions.c` `#include "dartos.h"` và gọi `RETRO_ENVIRONMENT_RUN_EMULATOR` (`0x20000|3`) với `core_name_buf`/`game_name_buf` để không rơi vào `SHUTDOWN` (bug cũ).
- Build: 2 platform chính là `pc` và `gb300` (alias `unix`/`dartos`); mọi output (`.o`, `.a`, `.so`, `core.elf`, `leapui.mars/.hcrtos`) đều trong `build/{unix,dartos}/`, root sạch để public; toolchain path đã quote nên chạy được cả khi đường dẫn có space. `make install SDROOT=...` ghi core + config vào `system/` (layout mới) — không có `DARTOS/` hay `configs/` cũ trong repo public.
- `core_api.c` / `frontend_functions.c` giữ stock `Argent-Cores` — **đừng sửa** (thay đổi phải kèm thay đổi tương ứng bên loader; mọi thứ tự sửa nên nằm trong core).
- `include/dirent.h` là shim cho device (`frontend_functions.c` tự implement `readdir` đúng layout shim); build `pc` trên Linux thì tự `#include_next` `dirent.h` thật (layout glibc khác, shim sẽ đọc sai `d_name`).
- Nhánh build zig/musl đã bỏ: `src/mmap_stub.c` còn nằm lại nhưng **không được build**; `include/reent.h` (stub `struct _reent` cho zig) đã xóa vì đè lên newlib làm `core_api.c` lỗi `redefinition of struct _reent`.
- Log `system/assets/LeapUI/leapui.log` ghi `roms/gba/assets`, `shelf_scan`, `queue_insert`, `ROM/CORE check`, `RUN_EMULATOR ret` để debug loop. Lưu ý `ROM/CORE check` hiện hardcode path `/media/mmcblk0p2/HCRTOS/cores/gpSP.hcrtos` (bản legacy) — chỉ là log, không chặn launch.
- Smoke test PC: `tools/smoke_libretro.c` (frontend tối giản) + `tools/smoke_run.sh` (rebuild + chạy, in log `shelf_scan`).

## License

ISC (như Argent-Cores) — xem [LICENSE](LICENSE).
