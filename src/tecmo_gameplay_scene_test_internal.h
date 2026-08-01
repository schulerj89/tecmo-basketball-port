#ifndef TECMO_GAMEPLAY_SCENE_TEST_INTERNAL_H
#define TECMO_GAMEPLAY_SCENE_TEST_INTERNAL_H

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_asset_pack.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_nes_video.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct TecmoGameplaySceneTestContext {
    const char *project_root;
    const char *asset_pack_path;
    TecmoMusicPlayer *music_player;
    char *message;
    size_t message_size;
    TecmoGameplayScene *scene;
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
} TecmoGameplaySceneTestContext;

static inline void tecmo_gameplay_scene_test_message(
    char *message,
    size_t message_size,
    const char *text)
{
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size, "%s", text);
    }
}

void tecmo_gameplay_scene_test_set_skip_pretip(bool skip);
bool tecmo_gameplay_scene_test_follow_live_camera_once(
    TecmoGameplayScene *scene);
uint32_t tecmo_gameplay_scene_test_pixels_fnv1a32(
    const uint32_t *pixels,
    size_t pixel_count);

bool tecmo_gameplay_scene_test_pretip(
    TecmoGameplaySceneTestContext *test);
bool tecmo_gameplay_scene_test_render_contract(
    TecmoGameplaySceneTestContext *test);
bool tecmo_gameplay_scene_test_shot_clock(
    TecmoGameplaySceneTestContext *test);
bool tecmo_gameplay_scene_test_state_flow(
    TecmoGameplaySceneTestContext *test);
bool tecmo_gameplay_scene_test_orchestrate(
    const char *project_root,
    const char *asset_pack_path,
    TecmoMusicPlayer *music_player,
    char *message,
    size_t message_size);

bool tecmo_gameplay_scene_test_draw_exact_step(
    const TecmoGameplayScene *scene);
bool tecmo_gameplay_scene_test_close_clock_collision(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch);
bool tecmo_gameplay_scene_test_jump_period_expiry(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch);
bool tecmo_gameplay_scene_test_jump_make_period_expiry(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch,
    bool expiry_before_score);
bool tecmo_gameplay_scene_test_combined_restart_is_inert(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch,
    uint16_t action_serial);

#endif
