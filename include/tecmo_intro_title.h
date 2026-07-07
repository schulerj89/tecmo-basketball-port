#ifndef TECMO_INTRO_TITLE_H
#define TECMO_INTRO_TITLE_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t tecmo_intro_title_palette_stage_for_frame(unsigned mode_frame_counter);
size_t tecmo_intro_title_tile_count(void);

bool tecmo_intro_title_draw(TecmoFramebuffer *fb,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            uint32_t chr_bank,
                            unsigned mode_frame_counter,
                            int origin_x,
                            int origin_y,
                            int scale,
                            bool draw_debug_bounds);

#endif
