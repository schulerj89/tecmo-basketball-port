#include "tecmo_memory.h"

#include <string.h>

static size_t align_forward_size(size_t value, size_t alignment)
{
    size_t mask;
    if (alignment == 0) {
        return value;
    }
    mask = alignment - 1U;
    return (value + mask) & ~mask;
}

void tecmo_arena_init(TecmoMemoryArena *arena, void *base, size_t capacity)
{
    arena->base = (uint8_t *)base;
    arena->capacity = capacity;
    arena->used = 0;
}

void *tecmo_arena_push_size(TecmoMemoryArena *arena, size_t size, size_t alignment)
{
    size_t aligned_used = align_forward_size(arena->used, alignment);
    if (aligned_used > arena->capacity || size > arena->capacity - aligned_used) {
        return 0;
    }

    void *result = arena->base + aligned_used;
    arena->used = aligned_used + size;
    memset(result, 0, size);
    return result;
}

void tecmo_arena_reset(TecmoMemoryArena *arena)
{
    arena->used = 0;
}

uint8_t tecmo_cpu_ram_read(const TecmoGameMemory *memory, uint16_t address)
{
    return memory->cpu_ram[address & 0x07FFU];
}

void tecmo_cpu_ram_write(TecmoGameMemory *memory, uint16_t address, uint8_t value)
{
    memory->cpu_ram[address & 0x07FFU] = value;
}
