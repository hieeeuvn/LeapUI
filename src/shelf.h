#ifndef LEAP_SHELF_H
#define LEAP_SHELF_H
#include <stdbool.h>

#define MAX_ROMS 512
#define MAX_PATH 512

typedef struct {
    char path[MAX_PATH];
    char name[128];
    char stem[128]; // without extension
} LeapCart;

typedef struct {
    LeapCart carts[MAX_ROMS];
    int count;
    int index;      // selected
    float scroll;   // smooth scroll position (float index)
    float vel;
} LeapShelf;

void shelf_init(LeapShelf *s);
int  shelf_scan(LeapShelf *s, const char *gba_dir);
void shelf_move(LeapShelf *s, int dir);
void shelf_update(LeapShelf *s, float dt);
const LeapCart* shelf_current(LeapShelf *s);
void shelf_set_index(LeapShelf *s, int idx);

#endif
