#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "libretro.h"
#include "dartos.h"

extern retro_environment_t environ_cb;
extern void leapui_log(const char *fmt, ...);

uint32_t g_preview_width = 0;
uint32_t g_preview_height = 0;

char game_name_buf[256];
char core_name_buf[256];
char *ptr_gs_run_game_name = game_name_buf;

int fallback_load_rgb565_image(const char* filename, uint16_t* framebuffer, int width, int height) {
    FILE* file = fopen(filename, "rb");
    if (!file) return -1;
    size_t sz = width * height * sizeof(uint16_t);
    size_t r = fread(framebuffer, 1, sz, file);
    fclose(file);
    return r==sz?0:-1;
}
bool fallback_is_zip_wqw_file(const char *file_path, bool *is_wqw) {
    (void)file_path; if(is_wqw) *is_wqw=false; return false;
}
#ifndef DARTOS
unsigned long long fallback_os_get_tick_count() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (unsigned long long)ts.tv_sec*1000 + ts.tv_nsec/1000000;
}
#endif
void fallback_wrap_run_game(const char *path, int state){
    (void)path; (void)state;
#ifdef RETRO_ENVIRONMENT_RUN_EMULATOR
    // dartos path
    struct retro_private_emulator_paths {
        const char *core_path; const char *rom_path;
    };
    // dartos.h defines same struct
    struct retro_private_emulator_paths p;
    p.core_path = core_name_buf;
    p.rom_path  = game_name_buf;
    bool ret = environ_cb(0x20000|3, &p);
    leapui_log("RUN_EMULATOR ret=%d core=%s rom=%s", ret, p.core_path, p.rom_path);
    return;
#endif
    environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}
