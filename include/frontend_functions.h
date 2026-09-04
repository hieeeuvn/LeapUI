#ifndef __FRONTEND_FUNCTIONS_H
#define __FRONTEND_FUNCTIONS_H

#include <stdint.h>

#if !defined(TickType_t)
typedef uint32_t TickType_t;
#endif

extern int (*xlog)(const char *, ...);
extern TickType_t xTaskGetTickCount(void);

#endif
