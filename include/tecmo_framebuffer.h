#ifndef TECMO_FRAMEBUFFER_H
#define TECMO_FRAMEBUFFER_H

#include <stdint.h>

typedef struct TecmoFramebuffer {
    uint32_t *pixels;
    int width;
    int height;
    int pitch_pixels;
} TecmoFramebuffer;

#endif
