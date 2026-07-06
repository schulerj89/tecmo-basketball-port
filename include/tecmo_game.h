#ifndef TECMO_GAME_H
#define TECMO_GAME_H

#include "asm_inventory.h"
#include "tecmo_memory.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct TecmoInput {
    bool up;
    bool down;
    bool left;
    bool right;
    bool confirm;
    bool cancel;
    bool shoot;
    bool tab;
    bool bank_prev;
    bool bank_next;
    bool table_toggle;
    bool save;
    bool preset_rabbit;
    bool preset_tecmo;
    bool remove;
    bool debug_toggle;
} TecmoInput;

typedef struct TecmoFramebuffer {
    uint32_t *pixels;
    int width;
    int height;
    int pitch_pixels;
} TecmoFramebuffer;

typedef enum TecmoPlayMode {
    TECMO_MODE_MAIN_MENU,
    TECMO_MODE_TITLE_SCREEN,
    TECMO_MODE_INTRO_PROBE,
    TECMO_MODE_CHR_PLAYGROUND,
    TECMO_MODE_PLAY_SETUP,
    TECMO_MODE_ROSTERS,
    TECMO_MODE_COURT
} TecmoPlayMode;

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

typedef struct TecmoRuntime {
    TecmoGameMemory *memory;
    RosterTable roster;
    TecmoOriginalTitleGlyphs title_glyphs;
    TecmoOriginalTitleGlyphs intro_glyphs;
    uint8_t *title_chr_bytes;
    uint64_t title_chr_byte_count;
    char teams[32][TECMO_MAX_NAME_TEXT];
    size_t team_count;
    size_t selected_team;
    size_t selected_player;
    size_t selected_menu_item;
    uint32_t selected_chr_bank;
    uint32_t selected_chr_table;
    uint16_t intro_source_tile;
    int intro_canvas_cell_x;
    int intro_canvas_cell_y;
    bool intro_canvas_focus;
    TecmoIntroPlacement intro_placements[TECMO_MAX_INTRO_PLACEMENTS];
    size_t intro_placement_count;
    bool intro_layout_dirty;
    bool intro_layout_saved;
    char intro_layout_status[96];
    TecmoPlayMode mode;
    bool quit_requested;
    bool debug_overlay;
    bool title_probe_available;
    float player_x;
    float player_y;
    float ball_x;
    float ball_y;
    float ball_vx;
    float ball_vy;
    bool ball_in_air;
    unsigned score;
    unsigned frame_counter;
    float frame_seconds;
    TecmoInput previous_input;
} TecmoRuntime;

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root);
void tecmo_runtime_shutdown(TecmoRuntime *runtime);
void tecmo_runtime_set_mode(TecmoRuntime *runtime, TecmoPlayMode mode);
void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input);
void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
void tecmo_render_original_title_probe(TecmoFramebuffer *framebuffer, const char *title_text);
void tecmo_render_original_title_chr_probe(TecmoFramebuffer *framebuffer,
                                           const TecmoOriginalTitleGlyphs *glyphs,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           uint32_t chr_bank);

#ifdef _WIN32
int tecmo_run_win32_game(const char *project_root);
#endif

#endif
