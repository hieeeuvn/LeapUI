/*  Environment callback to be used by the dartos frontend.
    Might be ported to other frontends and backported to Multicore,
    Used for FrogUI in an attempt to remove hacky workarounds and conform with libretro better. 
*/

/*TODO:     Implement a function to get fake rtc time
*/

#ifndef __DARTOS_H
#define __DARTOS_H

#include <libretro.h>

typedef struct {
    const void *frame;
    unsigned full_width;
    unsigned full_height;
    unsigned width;
    unsigned height;
    unsigned pitch;
    int x;
    int y;
    bool rgb32;
} FrameInfo;

typedef enum {
    ROTATE_0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270
} RotationMode;

typedef void (*retro_frontend_hcge_fb_fill_rect_t)(FrameInfo fill_frame, uint32_t color);
typedef void (*retro_frontend_hcge_accel_blit_t)(FrameInfo src_info, FrameInfo dst_info, RotationMode rotation);
typedef void (*retro_frontend_hcge_accel_stretch_blit_t)(FrameInfo src_info, FrameInfo dst_info);

struct retro_private_emulator_paths {
    const char *core_path;
    const char *rom_path;
};

struct retro_private_accel_functions {
    retro_frontend_hcge_fb_fill_rect_t hcge_fb_fill_rect;
    retro_frontend_hcge_accel_blit_t hcge_accel_blit;
    retro_frontend_hcge_accel_stretch_blit_t hcge_accel_stretch_blit;
};

//  This is meant to get the roms directory for cores like FrogUI
#define RETRO_ENVIRONMENT_GET_ROMS_DIRECTORY (RETRO_ENVIRONMENT_PRIVATE | 1)

//  This is meant to get the configs directory for cores like FrogUI
#define RETRO_ENVIRONMENT_GET_CONFIG_DIRECTORY (RETRO_ENVIRONMENT_PRIVATE | 2)

/*  This is meant to pass the rom and core directories to the frontend and
    signal to the frontend the core wants the frontend to run it */
#define RETRO_ENVIRONMENT_RUN_EMULATOR (RETRO_ENVIRONMENT_PRIVATE | 3)

/*  This is meant for passing HCGE Accelerated functions from the frontend to the core
    stuff like rectangle drawing blitting etc */
#define RETRO_ENVIRONMENT_GET_HCGE_ACCEL_FUNCTIONS (RETRO_ENVIRONMENT_PRIVATE | 4)

//  This is meant for cores like TGB Dual that support 2+ instances running
RETRO_API unsigned retro_get_sram_number(void); // Number of sram slots
RETRO_API void *retro_get_sram_data_ext(unsigned slot); // Get sram pointer from slot number
RETRO_API size_t retro_get_sram_size_ext(unsigned slot); // Get sram size from slot number

#endif //__DARTOS_H
