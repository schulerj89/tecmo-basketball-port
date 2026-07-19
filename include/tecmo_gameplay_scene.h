#ifndef TECMO_GAMEPLAY_SCENE_H
#define TECMO_GAMEPLAY_SCENE_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_music.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SCENE_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT 5U
#define TECMO_GAMEPLAY_SCENE_NO_ACTOR 0xFFU
#define TECMO_GAMEPLAY_SCENE_NO_TEAM 0xFFU
#define TECMO_GAMEPLAY_SCENE_NES_WIDTH 256
#define TECMO_GAMEPLAY_SCENE_NES_HEIGHT 240

typedef enum TecmoGameplaySceneSource {
    TECMO_GAMEPLAY_SCENE_PRESEASON = 0,
    TECMO_GAMEPLAY_SCENE_SEASON,
    TECMO_GAMEPLAY_SCENE_SOURCE_COUNT
} TecmoGameplaySceneSource;

typedef enum TecmoGameplaySceneShotKind {
    TECMO_GAMEPLAY_SCENE_SHOT_NONE = 0,
    TECMO_GAMEPLAY_SCENE_SHOT_JUMP,
    /* Numeric ROM families; no unproven dunk/layup label. */
    TECMO_GAMEPLAY_SCENE_SHOT_CLOSE_VARIANT_0,
    TECMO_GAMEPLAY_SCENE_SHOT_CLOSE_VARIANT_2,
    TECMO_GAMEPLAY_SCENE_SHOT_KIND_COUNT
} TecmoGameplaySceneShotKind;

typedef struct TecmoGameplaySceneLaunch {
    TecmoGameplaySceneSource source;
    uint16_t game_index;
    uint8_t away_team;
    uint8_t home_team;
    uint8_t regulation_minutes;
    uint8_t difficulty;
    uint8_t control_mode;
    uint8_t speed_value;
    uint8_t controller_team[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    bool game_music_enabled;
} TecmoGameplaySceneLaunch;

typedef struct TecmoGameplaySceneResult {
    TecmoGameplaySceneSource source;
    uint16_t game_index;
    uint8_t away_team;
    uint8_t home_team;
    uint16_t away_score;
    uint16_t home_score;
    uint8_t overtime_count;
} TecmoGameplaySceneResult;

typedef struct TecmoGameplaySceneActor {
    int16_t x;
    int16_t y;
    int16_t anchor_x;
    int16_t anchor_y;
    uint16_t pose_index;
    uint8_t team;
    uint8_t roster_index;
    bool facing_right;
    bool active;
} TecmoGameplaySceneActor;

typedef struct TecmoGameplayScene {
    uint32_t lifecycle_tag;
    bool available;
    bool active;
    bool result_ready;
    char status[192];
    char asset_pack_path[1024];

    TecmoGameplayAssets assets;
    TecmoGameplayCourt court;
    TecmoGameplayCloseShotAssets close_shots;
    TecmoGameplayAudioAsset audio_asset;
    TecmoGameplayAudioPlayer audio_player;
    TecmoGameplayState state;
    TecmoGameplayEventBuffer events;
    TecmoGameplaySceneLaunch launch;
    TecmoGameplaySceneResult result;

    TecmoGameplaySceneActor actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t controlled_actor[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t ball_holder;
    int32_t ball_x_q8;
    int32_t ball_y_q8;
    int32_t shot_start_x_q8;
    int32_t shot_start_y_q8;
    int32_t shot_end_x_q8;
    int32_t shot_end_y_q8;
    uint16_t shot_frame;
    uint16_t shot_duration;
    uint16_t action_serial;
    uint16_t free_throw_frame;
    uint8_t shot_points;
    uint8_t shot_actor;
    uint8_t close_shot_step;
    TecmoGameplayCloseShotProfile close_shot_profile;
    TecmoGameplayCloseShotDirection close_shot_direction;
    TecmoGameplaySceneShotKind shot_kind;
    TecmoGameplayPhase previous_phase;
    uint32_t frame;
} TecmoGameplayScene;

/* Initialize exactly once before load/destroy. */
void tecmo_gameplay_scene_init(TecmoGameplayScene *scene);

/* Loads TGPL-1, TGCT-1, TGCS-1, TSFX-1, and TDMC-1 from one local pack.
   `asset_pack_path` may be NULL to use the strict runtime search order.
   Runtime data is never read from decompilation/capture paths. */
bool tecmo_gameplay_scene_load(TecmoGameplayScene *scene,
                               const char *project_root,
                               const char *asset_pack_path,
                               TecmoMusicPlayer *music_player);
void tecmo_gameplay_scene_destroy(TecmoGameplayScene *scene);

bool tecmo_gameplay_scene_launch(TecmoGameplayScene *scene,
                                 const TecmoGameplaySceneLaunch *launch);
bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two);
bool tecmo_gameplay_scene_result(const TecmoGameplayScene *scene,
                                 TecmoGameplaySceneResult *result);
void tecmo_gameplay_scene_end(TecmoGameplayScene *scene);

/* Draws the exact ROM-derived static court base and resolved ROM poses. Live
   close-shot playback is deliberately limited to TGCS profile 0/direction 0;
   actor-facing horizontal mirroring is a native approximation, not a mapping
   of the other ROM direction entries. HUD/presentation text is supplied by the
   runtime overlay. */
bool tecmo_gameplay_scene_draw(const TecmoGameplayScene *scene,
                               TecmoFramebuffer *framebuffer,
                               int origin_x,
                               int origin_y,
                               int scale,
                               bool include_actors);

const char *tecmo_gameplay_scene_shot_name(TecmoGameplaySceneShotKind kind);
bool tecmo_gameplay_scene_self_test(const char *project_root,
                                    const char *asset_pack_path,
                                    TecmoMusicPlayer *music_player,
                                    char *message,
                                    size_t message_size);

#endif
