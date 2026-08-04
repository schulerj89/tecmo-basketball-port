#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tecmo_gameplay_scene_test_orchestrate(
    const char *project_root,
    const char *asset_pack_path,
    TecmoMusicPlayer *music_player,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneTestContext test;
    TecmoGameplayScene scene;

    memset(&test, 0, sizeof(test));
    memset(&scene, 0, sizeof(scene));
    test.project_root = project_root;
    test.asset_pack_path = asset_pack_path;
    test.music_player = music_player;
    test.message = message;
    test.message_size = message_size;
    test.scene = &scene;

    if (!tecmo_gameplay_scene_test_pretip(&test) ||
        !tecmo_gameplay_scene_test_render_contract(&test) ||
        !tecmo_gameplay_scene_test_shot_clock(&test) ||
        !tecmo_gameplay_scene_test_state_flow(&test) ||
        !tecmo_gameplay_scene_test_rules_restarts(&test)) {
        return false;
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    tecmo_gameplay_scene_test_message(
        message, message_size, "GAMEPLAY SCENE SELF TEST PASS");
    return true;
}
