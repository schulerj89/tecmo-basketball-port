#ifndef TECMO_GAME_H
#define TECMO_GAME_H

#include "asm_inventory.h"
#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_intro_arena.h"
#include "tecmo_intro_layout.h"
#include "tecmo_intro_stage.h"
#include "tecmo_intro_trace.h"
#include "tecmo_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum TecmoPlayMode {
    TECMO_MODE_MAIN_MENU,
    TECMO_MODE_TITLE_SCREEN,
    TECMO_MODE_INTRO_PROBE,
    TECMO_MODE_CHR_PLAYGROUND,
    TECMO_MODE_FIRST_SPRITE,
    TECMO_MODE_PLAY_SETUP,
    TECMO_MODE_ROSTERS,
    TECMO_MODE_COURT
} TecmoPlayMode;

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
    TecmoIntroTraceSprite intro_trace_sprites[TECMO_MAX_INTRO_TRACE_SPRITES];
    size_t intro_trace_sprite_count;
    uint32_t intro_trace_chr_bank;
    uint8_t intro_l88e7_palette[16];
    TecmoIntroArenaCapture intro_arena_capture;
    char intro_l88e7_irq_vector[16];
    char intro_presents_data_cpu[16];
    bool intro_trace_available;
    bool intro_trace_truncated;
    bool intro_l88e7_palette_available;
    bool intro_l88e7_irq_vector_available;
    bool intro_presents_data_available;
    uint8_t intro_output_step;
    char intro_trace_status[96];
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
    unsigned mode_frame_counter;
    float frame_seconds;
    TecmoInput previous_input;
} TecmoRuntime;

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root);
void tecmo_runtime_shutdown(TecmoRuntime *runtime);
void tecmo_runtime_set_mode(TecmoRuntime *runtime, TecmoPlayMode mode);
void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input);
void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_runtime_flow_self_test(TecmoRuntime *runtime, char *message, size_t message_size);
void tecmo_render_original_title_probe(TecmoFramebuffer *framebuffer, const char *title_text);
void tecmo_render_intro_c051_d861_model(TecmoFramebuffer *framebuffer);
void tecmo_render_intro_license_screen(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
void tecmo_render_intro_arena_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
void tecmo_render_first_sprite_probe(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
void tecmo_render_intro_l88e7_proof(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
void tecmo_render_original_title_chr_probe(TecmoFramebuffer *framebuffer,
                                           const TecmoOriginalTitleGlyphs *glyphs,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           uint32_t chr_bank);

#ifdef _WIN32
int tecmo_run_win32_game(const char *project_root);
#endif

#endif
