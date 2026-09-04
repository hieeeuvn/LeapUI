#ifndef LEAP_RENDER_H
#define LEAP_RENDER_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define SCREEN_W 320
#define SCREEN_H 240
// Palette trong suot xanh la
#define C_BG_TOP    0x0A24 // #0a2a0a
#define C_BG_BOT    0x0421 // #082108
#define C_BG        C_BG_TOP
#define C_BORDER    0x3FE0 // #00ff80 green
#define C_TEXT      0x7FE0
#define C_DIM       0x4A69
#define C_SELECT    0xFFFF
#define C_HOUSING   C_BG
#define C_RECESS    C_BG_BOT
#define C_OPENING   C_BG_TOP
#define C_EDGE      C_BORDER
#define C_SELECT_BG C_BORDER
#define C_HEADER    C_BORDER
typedef struct { uint16_t *data; int w,h; } Thumb;
void render_clear(uint16_t *fb);
void render_fill(uint16_t *fb,int x,int y,int w,int h,uint16_t c);
void render_rect_border(uint16_t *fb,int x,int y,int w,int h,uint16_t c);
void render_header(uint16_t *fb);
void render_footer(uint16_t *fb);
void render_center_banner(uint16_t *fb, Thumb *th, const char *label);
void render_side_preview(uint16_t *fb, int side, Thumb *th, const char *label); // side -1 left, 1 right
void render_backdrop(uint16_t *fb);
bool thumb_load(const char *rom_path, Thumb *out);
void thumb_free(Thumb *t);
void thumb_draw_scaled(uint16_t *fb, Thumb *t,int x,int y,int w,int h);
uint16_t rgb_hex(uint32_t hex);
#endif
