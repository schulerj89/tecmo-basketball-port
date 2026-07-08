#ifndef TECMO_INTRO_TRACE_H
#define TECMO_INTRO_TRACE_H

#include <stddef.h>
#include <stdint.h>

#define TECMO_MAX_INTRO_TRACE_SPRITES 192

#define INTRO_TRACE_GROUP_RABBIT 1U
#define INTRO_TRACE_GROUP_TECMO_STREAM 2U
#define INTRO_TRACE_GROUP_TECMO_LOGO 3U
#define INTRO_TRACE_GROUP_A7DB_SELECTOR0 4U

typedef struct TecmoIntroTraceSprite {
    uint8_t group;
    uint8_t tile_low;
    uint8_t attributes;
    int screen_x;
    int screen_y;
} TecmoIntroTraceSprite;

struct TecmoRuntime;

void tecmo_intro_trace_load(struct TecmoRuntime *runtime, const char *project_root);
size_t tecmo_intro_trace_group_count(const struct TecmoRuntime *runtime, uint8_t group);

#endif
