#ifndef TECMO_INTRO_LAYOUT_H
#define TECMO_INTRO_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INTRO_CANVAS_CELL_SIZE 16
#define INTRO_CANVAS_CELLS_X 17
#define INTRO_CANVAS_CELLS_Y 14

#define TECMO_MAX_INTRO_PLACEMENTS 32
#define TECMO_MAX_INTRO_PLACEMENT_TILES 8

typedef struct TecmoIntroPlacement {
    bool active;
    uint32_t chr_bank;
    uint32_t chr_table;
    uint16_t tile_ids[TECMO_MAX_INTRO_PLACEMENT_TILES];
    size_t tile_count;
    int canvas_cell_x;
    int canvas_cell_y;
    int pixel_x;
    int pixel_y;
    int scale;
    char label[32];
} TecmoIntroPlacement;

struct TecmoRuntime;

void tecmo_intro_layout_init(struct TecmoRuntime *runtime);
int tecmo_intro_layout_save(struct TecmoRuntime *runtime);
void tecmo_intro_layout_add_current(struct TecmoRuntime *runtime);
void tecmo_intro_layout_add_rabbit_preset(struct TecmoRuntime *runtime);
void tecmo_intro_layout_add_tecmo_logo_preset(struct TecmoRuntime *runtime);
void tecmo_intro_layout_add_composite_preset(struct TecmoRuntime *runtime);
void tecmo_intro_layout_remove_last(struct TecmoRuntime *runtime);
void tecmo_intro_layout_move_source_tile(struct TecmoRuntime *runtime, int dx, int dy);
void tecmo_intro_layout_move_canvas_cursor(struct TecmoRuntime *runtime, int dx, int dy);

#endif
