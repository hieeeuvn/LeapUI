# LeapUI Makefile - lazy edition
# All artifacts land in build/<platform>/ (no root clutter):
#   make pc          -> build/unix/leapui_libretro.so        (PC test)
#   make gb300       -> build/dartos/leapui.mars + leapui.hcrtos (GB300/SF2000/DY19)
#   make sf2000      -> build/sf2000/_libretro_sf2000.a      (legacy, kept for compatibility)
#   make toolchain   -> download frog-toolchain into ./x-tools (once)
#   make docker      -> build inside an alpine container (needs docker)
#   make install     -> copy the core + point the boot core at leapui in system/configs/{Phobos,dartos}.opt
#   make check       -> check the environment
#   make clean       -> remove build/

SHELL := /bin/bash
STATIC_LINKING := 0
AR := ar
TARGET_NAME := leapui
LIBM := -lm
Q ?= @

# Where 'make install' copies - examples:
#   make install SDROOT=/media/<user>/<SD>        (Linux)
#   make install SDROOT=/mnt/d                    (WSL)
# If a DARTOS/ folder exists next to the repo (private SD staging), make gb300 also copies into it.
SDROOT ?=

SOURCES_C := src/leapui.c src/font.c src/render.c src/shelf.c src/theme.c src/fallback_functions.c

# --- toolchain ---
FROG_TC_VER ?= v1.3.2
FROG_TC_BASE ?= https://github.com/axgdev/frog-toolchain/releases/download/$(FROG_TC_VER)
# two main archives; the script picks one
FROG_TC_EDGE_X64 ?= toolchain-edge-static-x86_64-gcc16.2.0-binutils2.47-newlib4.6.0.20260123.tar.xz
FROG_TC_STABLE_X64 ?= toolchain-stable-static-x86_64-gcc16.2.0-binutils2.47-newlib4.6.0.20260123.tar.xz
XTOOLS := $(CURDIR)/x-tools
# fallback if the user keeps the toolchain in toolchain/
ifeq ($(wildcard $(XTOOLS)/mipsel-mti-elf/bin/mipsel-mti-elf-gcc),)
  ifneq ($(wildcard $(CURDIR)/toolchain/mipsel-mti-elf/bin/mipsel-mti-elf-gcc),)
    XTOOLS := $(CURDIR)/toolchain
  endif
endif
MIPS_LOCAL := $(XTOOLS)/mipsel-mti-elf/bin/mipsel-mti-elf-
MIPS_SYS   := /opt/mips32-mti-elf/2019.09-03-2/bin/mips-mti-elf-
# MIPS: prefer local -> sys -> PATH
ifeq ($(wildcard $(MIPS_LOCAL)gcc),)
  ifneq ($(wildcard $(MIPS_SYS)gcc),)
    MIPS := $(MIPS_SYS)
  else
    MIPS := mips-mti-elf-
  endif
else
  MIPS := $(MIPS_LOCAL)
endif

# --- platform: auto detect + short aliases ---
# pc == unix (desktop), gb300 == dartos (NocturnalRTOS/DartOS running on SF2000, GB300, DY19...)
# No platform given -> if a working MIPS toolchain exists build the device core (gb300), else build PC.
ifeq ($(platform),)
  PLATFORM_TC_OK := $(shell "$(MIPS)gcc" --version >/dev/null 2>&1 && echo 1)
  ifeq ($(PLATFORM_TC_OK),1)
    platform = dartos
  else
    platform = unix
  endif
endif
ifneq ($(filter pc gb300,$(platform)),)
  override platform := $(if $(filter pc,$(platform)),unix,dartos)
endif

# per-platform objects (pc=x86 vs gb300=mips must not share .o files)
BUILD_DIR := build/$(platform)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES_C:.c=.o)))
CORE_OBJS := $(addprefix $(BUILD_DIR)/,core_api.o frontend_functions.o)

# --- flags ---
ifeq ($(platform),unix)
  TARGET := $(BUILD_DIR)/$(TARGET_NAME)_libretro.so
  CC := gcc
  CXX := g++
  AR := ar
  CFLAGS += -fPIC -Wall -O2 -g -DUNIX
  LDFLAGS += -shared -Wl,--no-undefined
  STATIC_LINKING = 0
else ifeq ($(platform),sf2000)
  TARGET := $(BUILD_DIR)/_libretro_$(platform).a
  CC := $(MIPS)gcc
  CXX := $(MIPS)g++
  AR := $(MIPS)ar
  CFLAGS = -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mno-abicalls -fno-pic
  CFLAGS += -ffast-math -fomit-frame-pointer -ffunction-sections -fdata-sections -O3 -DNDEBUG
  CFLAGS += -DSF2000 -I./include
  CXXFLAGS := $(CFLAGS)
  STATIC_LINKING = 1
else ifeq ($(platform),dartos)
  TARGET := $(BUILD_DIR)/_libretro_$(platform).a
  CC := $(MIPS)gcc
  CXX := $(MIPS)g++
  AR := $(MIPS)ar
  OBJCPY := $(MIPS)objcopy
  LD := $(MIPS)ld
  CFLAGS = -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mno-abicalls -fno-pic
  CFLAGS += -ffast-math -fomit-frame-pointer -ffunction-sections -fdata-sections -O3 -DNDEBUG
  CFLAGS += -DDARTOS -I./include
  CXXFLAGS := $(CFLAGS)
  STATIC_LINKING = 1
endif

LDFLAGS += $(LIBM)
ifeq ($(DEBUG),1)
  CFLAGS += -O0 -g -DDEBUG
  CXXFLAGS += -O0 -g -DDEBUG
else
  CFLAGS += -Os
  CXXFLAGS += -Os
endif

INCFLAGS := -I./include -I./src
CFLAGS += -Wall -D__LIBRETRO__ $(INCFLAGS)
CXXFLAGS += -Wall -D__LIBRETRO__ $(INCFLAGS)

# --- targets ---
.PHONY: all clean check toolchain docker help install sdcard unix dartos pc gb300 sf2000

all: $(TARGET)
	@$(if $(filter dartos,$(platform)),$(MAKE) hcrtos)

# platform short aliases (two main names: pc + gb300; unix/dartos kept for compatibility)
pc unix:
	@echo "  MAKE platform=$@ (PC -> $(BUILD_DIR)/leapui_libretro.so)"
	$(MAKE) platform=$@
gb300 dartos:
	@echo "  MAKE platform=$@ (GB300/device -> $(BUILD_DIR)/leapui.mars + .hcrtos)"
	$(MAKE) platform=$@

# auto-fetch the toolchain when missing and building dartos/sf2000
$(TARGET): check-toolchain $(OBJECTS)
ifeq ($(STATIC_LINKING),1)
	$(Q)"$(AR)" rcs $@ $(OBJECTS)
	@echo "  AR  $@"
else
	@echo "  LD  $@"
	$(Q)"$(CC)" -fPIC -shared -o $@ $(OBJECTS) $(LDFLAGS)
endif

# DartOS .mars (new) + .hcrtos (legacy) - only when platform=dartos
hcrtos: $(TARGET) $(CORE_OBJS)
	@[ "$(platform)" = dartos ] || { echo "  [!] hcrtos only runs with platform=dartos/gb300"; exit 1; }
	@echo "  LD  $(BUILD_DIR)/core.elf"
	$(Q)"$(MIPS)g++" -EL -march=mips32 -mtune=mips32 -msoft-float -Wl,-Map=$(BUILD_DIR)/core.elf.map \
	  -e __core_entry__ -Tsrc/core.ld -o $(BUILD_DIR)/core.elf -Wl,--start-group \
	  $(CORE_OBJS) $(TARGET) -lc -Wl,--end-group \
	  -Wl,--gc-sections -z max-page-size=32
	@echo "  OBJCOPY $(BUILD_DIR)/leapui.mars"
	$(Q)"$(MIPS)objcopy" -O binary -R .MIPS.abiflags -R .note.gnu.build-id -R ".rel*" \
	  $(BUILD_DIR)/core.elf $(BUILD_DIR)/leapui.mars
	@cp $(BUILD_DIR)/leapui.mars $(BUILD_DIR)/leapui.hcrtos
	@echo "  OK  $(BUILD_DIR)/leapui.mars ($$(wc -c < $(BUILD_DIR)/leapui.mars) bytes)"
	@echo "      to copy to an SD: make install SDROOT=/path/to/sd (or cp build/dartos/leapui.mars)"
	@if [ -d DARTOS ]; then \
	  mkdir -p DARTOS/system/Phobos/cores DARTOS/HCRTOS/cores DARTOS/ROMS/LeapUI; \
	  cp $(BUILD_DIR)/leapui.mars DARTOS/system/Phobos/cores/leapui.mars; \
	  cp $(BUILD_DIR)/leapui.mars DARTOS/HCRTOS/cores/leapui.mars; \
	  cp $(BUILD_DIR)/leapui.mars DARTOS/HCRTOS/cores/leapui.hcrtos; \
	  : > DARTOS/ROMS/LeapUI/leap.ui; \
	  echo "  INSTALLED -> DARTOS/ (private SD staging; ROMS/LeapUI/leap.ui = 0-byte boot stub)"; \
	fi

# each .c in src/ -> build/<platform>/<name>.o (never leave .o files inside src/)
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@echo "  CC  $<"
	$(Q)"$(CC)" $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

check-toolchain:
ifeq ($(filter dartos sf2000,$(platform)),$(platform))
	@if ! "$(CC)" --version >/dev/null 2>&1; then \
	  echo "  [!] $(CC) not found -> run 'make toolchain' (downloads frog-toolchain) or 'make docker'"; \
	  echo "      On Windows run via WSL: wsl bash -lc 'cd <path-to-LeapUI> && make gb300'"; \
	  exit 1; \
	fi
endif

toolchain:
	@echo "  GET frog-toolchain $(FROG_TC_VER) -> $(XTOOLS)"
	@mkdir -p "$(XTOOLS)"
	@if [ -x "$(XTOOLS)/mipsel-mti-elf/bin/mipsel-mti-elf-gcc" ]; then echo "  already exists, skip"; exit 0; fi
	@if command -v curl >/dev/null 2>&1; then \
	  echo "  curl $(FROG_TC_BASE)/$(FROG_TC_EDGE_X64)"; \
	  curl -L --progress-bar -o /tmp/frog-tc.tar.xz $(FROG_TC_BASE)/$(FROG_TC_EDGE_X64) || \
	  curl -L --progress-bar -o /tmp/frog-tc.tar.xz $(FROG_TC_BASE)/$(FROG_TC_STABLE_X64); \
	elif command -v wget >/dev/null 2>&1; then \
	  wget -O /tmp/frog-tc.tar.xz $(FROG_TC_BASE)/$(FROG_TC_EDGE_X64) || wget -O /tmp/frog-tc.tar.xz $(FROG_TC_BASE)/$(FROG_TC_STABLE_X64); \
	else echo "  [!] need curl or wget to download the toolchain (Windows: use WSL or 'make docker')"; exit 1; fi
	@tar -xf /tmp/frog-tc.tar.xz -C "$(XTOOLS)" --strip-components=1 2>/dev/null || tar -xf /tmp/frog-tc.tar.xz -C "$(XTOOLS)"
	@ls -lh "$(XTOOLS)/mipsel-mti-elf/bin/mipsel-mti-elf-gcc" && echo "  OK toolchain ready"

docker:
	@echo "  DOCKER build (needs docker)"
	docker run --rm -v $(CURDIR):/work -w /work alpine:3.23 sh -c "apk add --no-cache make bash curl gcc musl-dev && make gb300 -j$$(nproc)"

check:
	@echo "platform=$(platform) CC=$(CC)"
	@echo "MIPS=$(MIPS)"
	@ls -lh $(TARGET) 2>/dev/null || echo "  target not built yet"
	@echo "output: build/unix/leapui_libretro.so + build/dartos/leapui.mars (see 'make help')"

clean:
	rm -rf build
	rm -f _libretro_*.a leapui_libretro.so core.elf core.elf.map leapui.hcrtos leapui.mars
	rm -f src/*.o

# copy the core + config to the SD card.
# NocturnalRTOS boots the core named by hcrtos_core_path in system/configs/Phobos.opt,
# so install patches that key to "leapui" in both Phobos.opt (new) and dartos.opt (legacy),
# keeping every other setting; a missing file is created with stock defaults.
install sdcard: hcrtos
	@if [ -n "$(SDROOT)" ]; then ROOT="$(SDROOT)"; \
	elif [ -d DARTOS ]; then ROOT="DARTOS"; \
	else echo "  [?] SDROOT not set - e.g. make install SDROOT=/media/<user>/<SD>"; exit 0; fi; \
	  mkdir -p "$$ROOT/system/Phobos/cores" "$$ROOT/system/configs" "$$ROOT/ROMS/LeapUI"; \
	  cp $(BUILD_DIR)/leapui.mars "$$ROOT/system/Phobos/cores/leapui.mars"; \
	  : > "$$ROOT/ROMS/LeapUI/leap.ui"; \
	  for f in Phobos dartos; do cfg="$$ROOT/system/configs/$$f.opt"; \
	    if [ -f "$$cfg" ]; then \
	      { grep -v '^hcrtos_core_path' "$$cfg" || true; echo 'hcrtos_core_path = "leapui"'; } > "$$cfg.tmp"; \
	    else \
	      printf '%s\n' 'hcrtos_scaling_mode = "aspect float"' 'hcrtos_mono_audio_enabled = "true"' \
	                     'hcrtos_brightness_percentage = "100"' \
	                     'hcrtos_rom_path = "/media/mmcblk0p2/ROMS/menu/m"' \
	                     'hcrtos_core_path = "leapui"' 'hcrtos_audio_device = "SF2000"' \
	                     'hcrtos_joypad_device = "SF2000"' 'hcrtos_gfx_custom_x_enabled = "false"' \
	                     'hcrtos_gfx_custom_y_enabled = "false"' 'hcrtos_gfx_custom_x = "0"' \
	                     'hcrtos_gfx_custom_y = "0"' 'hcrtos_show_fps_counter = "false"' > "$$cfg.tmp"; \
	    fi; \
	    mv "$$cfg.tmp" "$$cfg"; \
	  done; \
	  echo "  OK  -> $$ROOT/system/Phobos/cores/leapui.mars"; \
	  echo "       $$ROOT/ROMS/LeapUI/leap.ui (0-byte boot stub)"; \
	  echo "       $$ROOT/system/configs/{Phobos,dartos}.opt  (hcrtos_core_path=\"leapui\")"

help:
	@echo "LeapUI - lazy make:"
	@echo "  make pc           build build/unix/leapui_libretro.so for PC testing (= make unix / platform=pc)"
	@echo "  make gb300        build build/dartos/leapui.mars + leapui.hcrtos for GB300/SF2000"
	@echo "                    (= make dartos / platform=gb300; needs the MIPS toolchain)"
	@echo "  make install      copy core + set boot core (Phobos.opt/dartos.opt) on an SD:"
	@echo "                    make install SDROOT=/path/to/sd"
	@echo "  make toolchain    download frog-toolchain into ./x-tools (once, ~150MB)"
	@echo "  make docker       build with docker (no toolchain install needed)"
	@echo "  make check        check the environment"
	@echo "  make clean        remove build/"
