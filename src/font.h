#ifndef LEAP_FONT_H
#define LEAP_FONT_H
#include <stdint.h>

void font_init(void);
int  font_measure(const char *text);
void font_draw(uint16_t *fb, int fb_w, int fb_h, int x, int y, const char *text, uint16_t color);

// FrogUI compat
int font_measure_text(const char *text);
void font_draw_text(uint16_t *fb, int w, int h, int x, int y, const char *text, uint16_t color);
void font_load_from_settings(const char *name);
void font_draw_char(uint16_t *fb, int w, int h, int x, int y, char c, uint16_t color);

#define FONT_CHAR_WIDTH 18
#define FONT_CHAR_HEIGHT 16
#define FONT_CHAR_SPACING 13

#endif
