#ifndef TECMO_NES_VIDEO_H
#define TECMO_NES_VIDEO_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stdint.h>

uint32_t tecmo_nes_2c02_rgba(uint8_t color);
bool tecmo_nes_video_self_test(char *message, size_t message_size);
uint8_t tecmo_nes_attribute_palette_index(uint8_t attribute, int tile_row, int tile_col);

void tecmo_draw_chr_tile_at_offset_ex(TecmoFramebuffer *fb,
                                      const uint8_t *chr_bytes,
                                      uint64_t chr_byte_count,
                                      uint64_t tile_offset,
                                      int x,
                                      int y,
                                      int scale,
                                      const uint32_t palette[4],
                                      bool flip_horizontal,
                                      bool flip_vertical);

void tecmo_draw_chr_tile_ex(TecmoFramebuffer *fb,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            uint32_t chr_bank,
                            uint16_t tile,
                            int x,
                            int y,
                            int scale,
                            const uint32_t palette[4],
                            bool flip_horizontal,
                            bool flip_vertical);

void tecmo_draw_chr_tile(TecmoFramebuffer *fb,
                         const uint8_t *chr_bytes,
                         uint64_t chr_byte_count,
                         uint32_t chr_bank,
                         uint16_t tile,
                         int x,
                         int y,
                         int scale);

#endif
