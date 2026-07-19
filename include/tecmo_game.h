#ifndef TECMO_GAME_H
#define TECMO_GAME_H

#include "asm_inventory.h"
#include "tecmo_controls.h"
#include "tecmo_all_star_menu.h"
#include "tecmo_framebuffer.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_intro_arena.h"
#include "tecmo_intro_finale.h"
#include "tecmo_intro_layout.h"
#include "tecmo_intro_post_arena.h"
#include "tecmo_intro_screen.h"
#include "tecmo_intro_stage.h"
#include "tecmo_intro_trace.h"
#include "tecmo_memory.h"
#include "tecmo_music.h"
#include "tecmo_preseason_menu.h"
#include "tecmo_season_menu.h"
#include "tecmo_start_game_menu.h"
#include "tecmo_team_data.h"
#include "tecmo_team_management.h"
#include "tecmo_title_screen.h"

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
    TECMO_MODE_COURT,
    TECMO_MODE_START_GAME_MENU,
    TECMO_MODE_PRESEASON_MENU,
    TECMO_MODE_ALL_STAR_MENU,
    TECMO_MODE_TEAM_DATA,
    TECMO_MODE_SEASON_MENU
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
    TecmoArenaTileLayer intro_arena_tile_layer;
    TecmoArenaNativeSpriteGroups intro_arena_sprite_groups;
    TecmoIntroArenaCapture intro_arena_capture;
    TecmoIntroReadyAsset intro_ready_asset;
    TecmoIntroWarriorsAsset intro_warriors_asset;
    TecmoIntroClippersAsset intro_clippers_asset;
    TecmoIntroBucksAsset intro_bucks_asset;
    TecmoIntroPassAsset intro_pass_asset;
    TecmoIntroFinaleAsset intro_finale_asset;
    TecmoIntroScreenAsset intro_presents_asset;
    TecmoIntroScreenAsset intro_license_asset;
    TecmoTitleAsset title_asset;
    TecmoStartGameMenuAsset start_game_menu_asset;
    TecmoStartGameMenuState start_game_menu_state;
    TecmoPreseasonAsset preseason_asset;
    TecmoPreseasonState preseason_state;
    TecmoAllStarAsset all_star_asset;
    TecmoAllStarState all_star_state;
    uint8_t all_star_committed_difficulty;
    TecmoMusicAsset music_asset;
    TecmoMusicPlayer music_player;
    TecmoGameplayScene gameplay_scene;
    TecmoTeamDataAsset team_data_asset;
    TecmoTeamDataState team_data_state;
    TecmoTeamManagementAsset team_management_asset;
    TecmoTeamManagementSession team_management_session;
    TecmoSeasonAsset season_asset;
    TecmoSeasonSession season_session;
    TecmoSeasonState season_state;
    char intro_l88e7_irq_vector[16];
    char intro_presents_data_cpu[16];
    bool intro_trace_available;
    bool intro_trace_truncated;
    bool intro_l88e7_palette_available;
    bool intro_l88e7_irq_vector_available;
    bool intro_presents_data_available;
    uint8_t intro_output_step;
    uint8_t intro_next_screen;
    bool intro_handoff_complete;
    bool title_start_armed;
    bool title_confirming;
    unsigned title_confirmation_frame;
    char intro_trace_status[96];
    TecmoPlayMode mode;
    bool normal_play_active;
    bool start_menu_return_pending;
    bool start_menu_return_from_season;
    bool start_menu_input_neutral_gate;
    uint8_t start_menu_return_root_selection;
    uint8_t start_menu_return_season_selection;
    uint8_t start_menu_return_music_value;
    uint8_t start_menu_return_speed_value;
    uint8_t start_menu_return_period_index;
    bool quit_requested;
    bool debug_overlay;
    bool title_probe_available;
    unsigned frame_counter;
    unsigned mode_frame_counter;
    float frame_seconds;
    TecmoInput previous_input;
    TecmoInput previous_player_two_input;
} TecmoRuntime;

#define TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER 0x01U

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root);
bool tecmo_runtime_init_with_flags(TecmoRuntime *runtime,
                                   TecmoGameMemory *memory,
                                   const char *project_root,
                                   unsigned flags);
void tecmo_runtime_shutdown(TecmoRuntime *runtime);
void tecmo_runtime_set_mode(TecmoRuntime *runtime, TecmoPlayMode mode);
void tecmo_runtime_update_players(TecmoRuntime *runtime,
                                  const TecmoInput *player_one,
                                  const TecmoInput *player_two);
void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input);
void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_gameplay_scene(const TecmoRuntime *runtime,
                                 TecmoFramebuffer *framebuffer);
bool tecmo_runtime_flow_self_test(TecmoRuntime *runtime, char *message, size_t message_size);
void tecmo_render_original_title_probe(TecmoFramebuffer *framebuffer, const char *title_text);
void tecmo_render_intro_c051_d861_model(TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_presents_screen(const TecmoRuntime *runtime,
                                        TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_license_screen(const TecmoRuntime *runtime,
                                       TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_arena_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_ready_screen(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_warriors_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_clippers_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_bucks_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_pass_transition(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
bool tecmo_render_intro_finale(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer);
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
