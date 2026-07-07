#ifndef TECMO_INTRO_LICENSE_H
#define TECMO_INTRO_LICENSE_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t tecmo_intro_license_tile_count(void);

bool tecmo_intro_license_draw(TecmoFramebuffer *fb,
                              const uint8_t *chr_bytes,
                              uint64_t chr_byte_count,
                              uint32_t chr_bank,
                              int origin_x,
                              int origin_y,
                              int scale);

#endif
