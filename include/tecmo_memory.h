#ifndef TECMO_MEMORY_H
#define TECMO_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TecmoMemoryArena {
    uint8_t *base;
    size_t capacity;
    size_t used;
} TecmoMemoryArena;

typedef struct TecmoGameMemory {
    TecmoMemoryArena permanent;
    TecmoMemoryArena transient;
    uint8_t cpu_ram[0x800];
    uint8_t ppu_vram[0x800];
    uint8_t palette_ram[0x20];
    uint8_t oam[0x100];
} TecmoGameMemory;

void tecmo_arena_init(TecmoMemoryArena *arena, void *base, size_t capacity);
void *tecmo_arena_push_size(TecmoMemoryArena *arena, size_t size, size_t alignment);
void tecmo_arena_reset(TecmoMemoryArena *arena);

uint8_t tecmo_cpu_ram_read(const TecmoGameMemory *memory, uint16_t address);
void tecmo_cpu_ram_write(TecmoGameMemory *memory, uint16_t address, uint8_t value);

#endif
