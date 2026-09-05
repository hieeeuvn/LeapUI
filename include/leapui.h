#ifndef LEAPUI_H
#define LEAPUI_H
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 320
#define SCREEN_H 240

// Slot-inspired insert/eject timing (seconds)
#define INSERT_S 0.73f
#define INSERT_HOLD_S 0.28f
#define SEATED_AT (INSERT_S - INSERT_HOLD_S)
#define EJECT_S SEATED_AT

// Hold threshold for clean boot (ms)
#define PLAY_HOLD_MS 500

typedef enum {
    PHASE_SHELF,
    PHASE_INSERTING,
    PHASE_PLAYING,
    PHASE_EJECTING,
    PHASE_ABOUT
} LeapPhase;

#endif
