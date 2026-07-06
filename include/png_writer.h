#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include <stddef.h>
#include <stdint.h>

int png_write_rgba8(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height);

#endif
