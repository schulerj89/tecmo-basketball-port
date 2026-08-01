#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool scene_build_background_context(
    const TecmoGameplayScene *scene,
    TecmoGameplayLiveBackgroundContext *context)
{
    return tecmo_gameplay_scene_render_build_background_context(scene, context);
}

static bool scene_prepare_live_hud(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayPreparedHud *prepared)
{
    return tecmo_gameplay_scene_render_prepare_live_hud(
        scene, context, prepared);
}

static bool scene_resolve_pose(
    const TecmoGameplayScene *scene,
    uint16_t pointer_index,
    uint8_t actor_slot_base,
    uint8_t actor_attributes,
    uint8_t palette_group,
    bool apply_uniform_color,
    uint8_t uniform_color,
    TecmoGameplayResolvedPose *pose)
{
    return tecmo_gameplay_scene_render_resolve_pose(
        scene, pointer_index, actor_slot_base, actor_attributes,
        palette_group, apply_uniform_color, uniform_color, pose);
}

static bool scene_resolve_actor_pose(
    const TecmoGameplayScene *scene,
    size_t actor_index,
    TecmoGameplayResolvedPose *pose)
{
    return tecmo_gameplay_scene_render_resolve_actor_pose(
        scene, actor_index, pose);
}

static bool scene_test_live_hud_contract(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayLiveBackgroundContext context;
    TecmoGameplayPreparedHud prepared;
    TecmoGameplayPreparedHud dynamic_prepared;
    TecmoGameplayScene dynamic;
    size_t row;
    size_t column;
    size_t occupied[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT] = {0U, 0U};
    const uint8_t *font;
    unsigned char selected_initial;
    uint8_t selected_roster;
    uint8_t selected_number_bcd;
    if (scene == NULL || !scene_build_background_context(scene, &context) ||
        !scene_prepare_live_hud(scene, &context, &prepared)) {
        return false;
    }
    font = scene->hud_assets.font_tiles;
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (!prepared.occupied[row][column]) continue;
            ++occupied[row];
            if (prepared.chr_offsets[row][column] >
                    scene->assets.chr_storage_size ||
                scene->assets.chr_storage_size -
                        prepared.chr_offsets[row][column] < 16U) {
                return false;
            }
        }
    }
    selected_roster = scene->actors[scene->controlled_actor[0U]].roster_index;
    selected_initial = (unsigned char)
        scene->pretip_team_data
            ->players[scene->launch.away_team][selected_roster].name[0U];
    selected_number_bcd = scene->pretip_team_data
        ->players[scene->launch.away_team][selected_roster].attributes[1U];
    if ((selected_number_bcd >> 4U) > 9U ||
        (selected_number_bcd & 0x0FU) > 9U ||
        occupied[0U] != TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        occupied[1U] != TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        memcmp(&prepared.tiles[0U][1U],
               scene->hud_assets.team_label_tiles[scene->launch.away_team],
               TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) != 0 ||
        memcmp(&prepared.tiles[0U][23U],
               scene->hud_assets.team_label_tiles[scene->launch.home_team],
               TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) != 0 ||
        prepared.tiles[0U][6U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][13U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][14U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][15U] != TECMO_GAMEPLAY_HUD_COLON_TILE ||
        prepared.tiles[1U][1U] !=
            font['0' + (selected_number_bcd >> 4U) -
                 TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][2U] !=
            font['0' + (selected_number_bcd & 0x0FU) -
                 TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][4U] !=
            font[selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][5U] !=
            font['.' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.chr_offsets[1U][4U] !=
            scene->pretip_team_data->font[
                selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST]
                .chr_offset ||
        prepared.tiles[1U][15U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][16U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][19U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][31U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST]) {
        return false;
    }

    dynamic = *scene;
    dynamic.state.score[TECMO_GAMEPLAY_TEAM_AWAY] = 123U;
    dynamic.state.score[TECMO_GAMEPLAY_TEAM_HOME] = UINT16_MAX;
    dynamic.state.clock_minutes = 1U;
    dynamic.state.clock_seconds = 23U;
    dynamic.state.shot_clock = 9U;
    if (!scene_build_background_context(&dynamic, &context) ||
        !scene_prepare_live_hud(
            &dynamic, &context, &dynamic_prepared) ||
        dynamic_prepared.tiles[0U][6U] !=
            font['1' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][7U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][8U] !=
            font['3' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][13U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][14U] !=
            font['1' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][16U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][17U] !=
            font['3' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][28U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][29U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][30U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        memcmp(dynamic_prepared.tiles[1U], prepared.tiles[1U],
               TECMO_GAMEPLAY_HUD_COLUMN_COUNT) != 0) {
        return false;
    }

    dynamic = *scene;
    dynamic.ball_holder = 1U;
    dynamic.controlled_actor[0U] = 1U;
    if (!scene_attach_ball(&dynamic) ||
        !scene_build_background_context(&dynamic, &context) ||
        !scene_prepare_live_hud(
            &dynamic, &context, &dynamic_prepared)) {
        return false;
    }
    selected_roster = dynamic.actors[1U].roster_index;
    selected_initial = (unsigned char)
        dynamic.pretip_team_data
            ->players[dynamic.launch.away_team][selected_roster].name[0U];
    selected_number_bcd = dynamic.pretip_team_data
        ->players[dynamic.launch.away_team][selected_roster].attributes[1U];
    return (selected_number_bcd >> 4U) <= 9U &&
           (selected_number_bcd & 0x0FU) <= 9U &&
           dynamic_prepared.tiles[1U][1U] ==
               font['0' + (selected_number_bcd >> 4U) -
                    TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.tiles[1U][2U] ==
               font['0' + (selected_number_bcd & 0x0FU) -
                    TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.tiles[1U][4U] ==
               font[selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.chr_offsets[1U][4U] ==
               dynamic.pretip_team_data->font[
                   selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST]
                   .chr_offset;
}

static bool scene_test_live_hud_equal(
    const TecmoGameplayScene *left,
    const TecmoGameplayScene *right)
{
    TecmoGameplayLiveBackgroundContext left_context;
    TecmoGameplayLiveBackgroundContext right_context;
    TecmoGameplayPreparedHud left_hud;
    TecmoGameplayPreparedHud right_hud;
    if (left == NULL || right == NULL ||
        !scene_build_background_context(left, &left_context) ||
        !scene_build_background_context(right, &right_context) ||
        !scene_prepare_live_hud(left, &left_context, &left_hud) ||
        !scene_prepare_live_hud(right, &right_context, &right_hud)) {
        return false;
    }
    return memcmp(&left_hud, &right_hud, sizeof(left_hud)) == 0;
}

static bool scene_test_projection_is_neutral(
    const TecmoGameplayActorProjection *projection)
{
    return projection != NULL && !projection->visible &&
           projection->screen_x == 0U && projection->screen_y == 0U;
}

static bool scene_test_stationary_projection_transition(
    const TecmoGameplaySceneCourtFrame *before,
    const TecmoGameplaySceneCourtFrame *after)
{
    int camera_delta;
    size_t actor;
    if (before == NULL || after == NULL ||
        before->contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        after->contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        before->slice.viewport.camera_x != before->projection.camera_x ||
        after->slice.viewport.camera_x != after->projection.camera_x) {
        return false;
    }
    camera_delta =
        (int)after->projection.camera_x -
        (int)before->projection.camera_x;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplayActorProjection *old_projection =
            &before->projection.players[actor];
        const TecmoGameplayActorProjection *new_projection =
            &after->projection.players[actor];
        if ((!old_projection->visible &&
             !scene_test_projection_is_neutral(old_projection)) ||
            (!new_projection->visible &&
             !scene_test_projection_is_neutral(new_projection))) {
            return false;
        }
        if (old_projection->visible && new_projection->visible &&
            ((int)new_projection->screen_x + camera_delta !=
                 (int)old_projection->screen_x ||
             new_projection->screen_y != old_projection->screen_y)) {
            return false;
        }
    }
    if ((!before->projection.ball.visible &&
         !scene_test_projection_is_neutral(
             &before->projection.ball)) ||
        (!after->projection.ball.visible &&
         !scene_test_projection_is_neutral(
             &after->projection.ball))) {
        return false;
    }
    return !before->projection.ball.visible ||
           !after->projection.ball.visible ||
           ((int)after->projection.ball.screen_x + camera_delta ==
                (int)before->projection.ball.screen_x &&
            after->projection.ball.screen_y ==
                before->projection.ball.screen_y);
}

static bool scene_test_pixels_equal(const uint32_t *pixels,
                                    size_t pixel_count,
                                    uint32_t expected)
{
    size_t pixel;
    if (pixels == NULL) return false;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != expected) return false;
    }
    return true;
}

static bool scene_test_outer_margin_equal(const uint32_t *pixels,
                                          int width,
                                          int height,
                                          int pitch,
                                          int origin_x,
                                          int origin_y,
                                          int view_width,
                                          int view_height,
                                          uint32_t expected)
{
    int y;
    int x;
    if (pixels == NULL || width <= 0 || height <= 0 || pitch < width ||
        origin_x < 0 || origin_y < 0 || view_width <= 0 || view_height <= 0 ||
        origin_x + view_width > width || origin_y + view_height > height) {
        return false;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            bool inside = x >= origin_x && x < origin_x + view_width &&
                          y >= origin_y && y < origin_y + view_height;
            if (!inside && pixels[(size_t)y * (size_t)pitch + (size_t)x] !=
                               expected) {
                return false;
            }
        }
    }
    return true;
}

bool tecmo_gameplay_scene_test_draw_exact_step(const TecmoGameplayScene *scene)
{
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    TecmoFramebuffer framebuffer;
    uint32_t *pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    bool drawn;
    if (pixels == NULL) return false;
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    drawn = tecmo_gameplay_scene_draw(scene, &framebuffer, 0, 0, 1, true);
    free(pixels);
    return drawn;
}

bool tecmo_gameplay_scene_test_render_contract(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;
    TecmoGameplayScene camera_probe;
    TecmoGameplayScene fine_scroll_probe;
    TecmoGameplayScene left_slice_probe;
    TecmoGameplayScene right_slice_probe;
    TecmoGameplayScene backcourt_probe;
    TecmoGameplayScene draw_probe;
    TecmoGameplayState gameplay_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayCameraState frozen_camera;
    TecmoGameplayActorProjection projection;
    TecmoGameplayCourtViewport viewport;
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtCoordinates unchanged_coordinates;
    TecmoGameplaySceneCourtProjection court_projection;
    TecmoGameplaySceneCourtProjection unchanged_court_projection;
    TecmoGameplaySceneCourtSlice court_slice;
    TecmoGameplaySceneCourtSlice unchanged_court_slice;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame previous_court_frame;
    TecmoGameplaySceneCourtFrame unchanged_court_frame;
    TecmoGameplaySceneActor boundary_actor;
    TecmoGameplayResolvedPose resolved_pose;
    TecmoFramebuffer framebuffer;
    TecmoFramebuffer invalid_framebuffer;
    TecmoFramebuffer guarded_framebuffer;
    uint32_t *pixels;
    uint32_t *guarded_pixels;
    uint32_t render_hash;
    uint32_t center_slice_hash;
    uint32_t left_slice_hash;
    uint32_t right_slice_hash;
    uint32_t frozen_follow_count;
    uint16_t original_pose;
    uint8_t holder;
    bool saw_coarse_crossing;
    bool saw_fine_scroll;
    bool saw_visibility_transition;
    size_t frame;
    size_t pixel;
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    const int guard_width = TECMO_GAMEPLAY_SCENE_NES_WIDTH + 24;
    const int guard_height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT + 20;
    const int guard_origin_x = 12;
    const int guard_origin_y = 10;
    const size_t guarded_pixel_count =
        (size_t)guard_width * (size_t)guard_height;

#define TEST_SCENE (*test->scene)
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay scene canonical launch rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (TEST_SCENE.camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        TEST_SCENE.camera_state.scroll_x != 0U ||
        TEST_SCENE.camera_state.scroll_aux != 0U ||
        TEST_SCENE.camera_state.nametable_page != 0U ||
        TEST_SCENE.camera_state.aux != 0U ||
        TEST_SCENE.camera_state.stream_direction != 0U ||
        TEST_SCENE.camera_state.layout_cursor != 0x21U ||
        TEST_SCENE.camera_state.left_threshold != 0x50U ||
        TEST_SCENE.camera_state.right_threshold != 0xA0U ||
        !TEST_SCENE.camera_state.thresholds_valid ||
        TEST_SCENE.camera_state.endpoint_latched ||
        TEST_SCENE.camera_follow_count != 0U ||
        TEST_SCENE.actors[0].position.x != 0x0160 ||
        TEST_SCENE.actors[0].position.y != 198 ||
        TEST_SCENE.actors[0].pose_index != 117U ||
        TEST_SCENE.actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].pose_index !=
            85U ||
        TEST_SCENE.ball_position.x_q8 != 0x0166 * 256 ||
        TEST_SCENE.ball_position.y_q8 != 176 * 256 ||
        !tecmo_gameplay_camera_state_live_valid(
            &TEST_SCENE.camera_assets, &TEST_SCENE.camera_state)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "TGCP-2 live prime/initial world-state contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!scene_test_live_hud_contract(&TEST_SCENE)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "THUD live score/player/clock projection contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    backcourt_probe = TEST_SCENE;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    backcourt_probe.actors[0U].position.x = 368;
    backcourt_probe.actors[0U].position.y = 148;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.actors[0U].facing_right = true;
    backcourt_probe.actors[0U].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[0U].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[0U].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 375 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live backcourt frontcourt latch failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    backcourt_probe.actors[0U].position.x = 380;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.ball_position.x_q8 = 386 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_HOME ||
        backcourt_probe.state.phase_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live backcourt settlement route failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    backcourt_probe = TEST_SCENE;
    holder = TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
    if (!scene_handoff_possession(
            &backcourt_probe, TECMO_GAMEPLAY_TEAM_HOME, holder) ||
        backcourt_probe.orientation_state.current_direction != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse backcourt possession setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    backcourt_probe.actors[holder].position.x = 398;
    backcourt_probe.actors[holder].position.y = 148;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.actors[holder].facing_right = false;
    backcourt_probe.actors[holder].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[holder].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[holder].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 392 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse live backcourt frontcourt latch failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    backcourt_probe.actors[holder].position.x = 387;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.ball_position.x_q8 = 383 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        backcourt_probe.state.phase_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse live backcourt settlement route failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!scene_resolve_actor_pose(&TEST_SCENE, 0U, &resolved_pose) ||
        resolved_pose.record_tag != 0x25U ||
        resolved_pose.mmc3_r2_r5[1] != 0x25U ||
        resolved_pose.piece_count != 4U ||
        resolved_pose.palette_group != 1U ||
        resolved_pose.actor_attributes != 1U ||
        resolved_pose.pieces[0].palette_index != 1U ||
        !resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0x30U ||
        memcmp(resolved_pose.palette,
               "\x16\x0F\x27\x30\x16\x0F\x17\x30"
               "\x16\x0F\x27\x2A\x16\x0F\x17\x2A", 16U) != 0 ||
        !scene_resolve_actor_pose(
            &TEST_SCENE, TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT,
            &resolved_pose) ||
        resolved_pose.palette_group != 1U ||
        resolved_pose.actor_attributes != 3U ||
        resolved_pose.pieces[0].palette_index != 3U ||
        memcmp(resolved_pose.pieces[0].palette,
               "\x16\x0F\x17\x2A", 4U) != 0 ||
        !resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0x2AU ||
        memcmp(resolved_pose.palette,
               "\x16\x0F\x27\x30\x16\x0F\x17\x30"
               "\x16\x0F\x27\x2A\x16\x0F\x17\x2A", 16U) != 0 ||
        !scene_resolve_pose(
            &TEST_SCENE, TECMO_GAMEPLAY_BALL_POSE, 0xC1U, 0U, 0U, false, 0U,
            &resolved_pose) ||
        resolved_pose.record_tag != 0x81U ||
        resolved_pose.mmc3_r2_r5[3] != 0x81U ||
        resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0U ||
        resolved_pose.piece_count != 1U ||
        resolved_pose.pieces[0].top_chr_offset != 0x20400U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "court actor/ball pose CHR-slot binding failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&coordinates, 0xA5, sizeof(coordinates));
    if (!tecmo_gameplay_scene_court_coordinates(
            &TEST_SCENE, &coordinates) ||
        coordinates.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG ||
        coordinates.players[0].x != 0x0160 ||
        coordinates.players[0].y != 198 ||
        coordinates.ball.x_q8 != 0x0166 * 256 ||
        coordinates.ball.y_q8 != 176 * 256 ||
        coordinates.hoops[0].x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        coordinates.hoops[0].y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        coordinates.hoops[1].x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        coordinates.hoops[1].y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical player/ball/hoop coordinate snapshot failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&court_projection, 0xA5, sizeof(court_projection));
    if (!tecmo_gameplay_scene_court_projection(
            &TEST_SCENE, &court_projection) ||
        court_projection.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_PROJECTION_TAG ||
        court_projection.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_projection.reserved != 0U ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0x60U ||
        court_projection.players[0].screen_y != 198U ||
        !court_projection.ball.visible ||
        court_projection.ball.screen_x != 0x66U ||
        court_projection.ball.screen_y != 176U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP scene projection snapshot failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&court_slice, 0xA5, sizeof(court_slice));
    if (!tecmo_gameplay_scene_court_slice(&TEST_SCENE, &court_slice) ||
        court_slice.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_SLICE_TAG ||
        court_slice.transition_serial != 0U ||
        court_slice.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        court_slice.direction != 0U ||
        court_slice.reserved != 0U ||
        court_slice.viewport.camera_x !=
            TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_slice.viewport.first_tile_x != 0x20U ||
        court_slice.viewport.fine_scroll_x != 0U ||
        court_slice.viewport.column_count != 32U ||
        court_slice.viewport.camera_x != court_projection.camera_x) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "possession-aware TGCT center slice snapshot failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&court_frame, 0xA5, sizeof(court_frame));
    if (!tecmo_gameplay_scene_court_frame(
            &TEST_SCENE, &court_frame) ||
        court_frame.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        court_frame.scene_frame != TEST_SCENE.frame ||
        court_frame.camera_follow_count != TEST_SCENE.camera_follow_count ||
        court_frame.reserved != 0U ||
        memcmp(&court_frame.slice, &court_slice,
               sizeof(court_slice)) != 0 ||
        memcmp(&court_frame.projection, &court_projection,
               sizeof(court_projection)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "camera-coherent center court frame snapshot failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    unchanged_coordinates = coordinates;
    unchanged_court_projection = court_projection;
    unchanged_court_slice = court_slice;
    unchanged_court_frame = court_frame;
    TEST_SCENE.ball_position.x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X + 1;
    if (tecmo_gameplay_scene_court_coordinates(
            &TEST_SCENE, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0 ||
        tecmo_gameplay_scene_court_projection(
            &TEST_SCENE, &court_projection) ||
        memcmp(&court_projection, &unchanged_court_projection,
               sizeof(court_projection)) != 0 ||
        tecmo_gameplay_scene_court_frame(
            &TEST_SCENE, &court_frame) ||
        memcmp(&court_frame, &unchanged_court_frame,
               sizeof(court_frame)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    camera_before = TEST_SCENE.camera_state;
    TEST_SCENE.camera_state.camera_x =
        TECMO_GAMEPLAY_COURT_MAX_CAMERA_X + 1U;
    if (tecmo_gameplay_scene_court_slice(
            &TEST_SCENE, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid camera mutated possession-aware TGCT slice");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.camera_state = camera_before;
    orientation_before = TEST_SCENE.orientation_state;
    TEST_SCENE.orientation_state.tracked_possession_team =
        TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME;
    if (tecmo_gameplay_scene_court_slice(
            &TEST_SCENE, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "mismatched possession mutated TGCT scene slice");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.orientation_state = orientation_before;
    TEST_SCENE.ball_position = unchanged_coordinates.ball;
    TEST_SCENE.actors[0].position.x = 0;
    if (!tecmo_gameplay_scene_court_projection(
            &TEST_SCENE, &court_projection) ||
        court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0U ||
        court_projection.players[0].screen_y != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP offscreen projection sentinel failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position =
        unchanged_coordinates.players[0];
    memset(&TEST_SCENE.shot_start_position, 0,
           sizeof(TEST_SCENE.shot_start_position));
    memset(&TEST_SCENE.shot_end_position, 0,
           sizeof(TEST_SCENE.shot_end_position));
    TEST_SCENE.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    TEST_SCENE.shot_actor = 0U;
    TEST_SCENE.jump_actor_altitude_q8 = 10U * 256U;
    if (!tecmo_gameplay_scene_court_projection(
            &TEST_SCENE, &court_projection) ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0x60U ||
        court_projection.players[0].screen_y != 188U ||
        court_projection.ball.screen_y != 176U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP jump-altitude projection failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    TEST_SCENE.shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    TEST_SCENE.jump_actor_altitude_q8 = 0U;
    TEST_SCENE.actors[0].position.x = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            &TEST_SCENE, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid player coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position =
        unchanged_coordinates.players[0];
    TEST_SCENE.actors[0].anchor.y =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_Y + 1);
    if (tecmo_gameplay_scene_court_coordinates(
            &TEST_SCENE, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid player anchor snapshot mutated output");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].anchor =
        unchanged_coordinates.players[0];
    TEST_SCENE.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    TEST_SCENE.shot_start_position.x_q8 = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            &TEST_SCENE, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid shot coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    memset(&TEST_SCENE.shot_start_position, 0,
           sizeof(TEST_SCENE.shot_start_position));

    memset(&boundary_actor, 0, sizeof(boundary_actor));
    boundary_actor.active = true;
    boundary_actor.position.y = TECMO_GAMEPLAY_MIN_Y;
    boundary_actor.position.x = -1;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x00DF ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-0 scene-safety boundary diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    boundary_actor.position.y = 128;
    boundary_actor.position.x = 300;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 300 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-1 scene-safety interior diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    boundary_actor.position.y = TECMO_GAMEPLAY_MAX_Y;
    boundary_actor.position.x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x0297 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-2 scene-safety boundary diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    memset(&projection, 0xA5, sizeof(projection));
    if (!tecmo_gameplay_camera_project_actor(
            &TEST_SCENE.camera_assets, &TEST_SCENE.camera_state,
            0x0100U, 100U, 10U, &projection) ||
        !projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 90U ||
        !tecmo_gameplay_camera_project_actor(
            &TEST_SCENE.camera_assets, &TEST_SCENE.camera_state,
            0x01FFU, 100U, 0U, &projection) ||
        !projection.visible || projection.screen_x != 0xFFU ||
        !tecmo_gameplay_camera_project_actor(
            &TEST_SCENE.camera_assets, &TEST_SCENE.camera_state,
            0x00FFU, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U ||
        !tecmo_gameplay_camera_project_actor(
            &TEST_SCENE.camera_assets, &TEST_SCENE.camera_state,
            0x0200U, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live actor projection/offscreen contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    left_slice_probe = TEST_SCENE;
    left_slice_probe.actors[left_slice_probe.ball_holder].position.x =
        0x00F3;
    left_slice_probe.actors[left_slice_probe.ball_holder].facing_right =
        true;
    if (!scene_attach_ball(&left_slice_probe) ||
        !tecmo_gameplay_scene_court_frame(
            &left_slice_probe, &previous_court_frame)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "left-possession ball placement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    saw_coarse_crossing = false;
    saw_fine_scroll = false;
    saw_visibility_transition = false;
    for (frame = 0U; frame < 200U; ++frame) {
        size_t projection_actor;
        uint16_t previous_camera_x =
            left_slice_probe.camera_state.camera_x;
        if (!tecmo_gameplay_scene_test_follow_live_camera_once(&left_slice_probe) ||
            !tecmo_gameplay_scene_court_frame(
                &left_slice_probe, &court_frame) ||
            court_frame.scene_frame !=
                previous_court_frame.scene_frame ||
            court_frame.camera_follow_count !=
                previous_court_frame.camera_follow_count + 1U ||
            court_frame.slice.possession !=
                TECMO_GAMEPLAY_TEAM_AWAY ||
            court_frame.slice.direction != 0U ||
            !scene_test_stationary_projection_transition(
                &previous_court_frame, &court_frame)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "left-possession camera follow failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        saw_coarse_crossing =
            saw_coarse_crossing ||
            court_frame.slice.viewport.first_tile_x !=
                previous_court_frame.slice.viewport.first_tile_x;
        saw_fine_scroll =
            saw_fine_scroll ||
            court_frame.slice.viewport.fine_scroll_x !=
                previous_court_frame.slice.viewport.fine_scroll_x;
        for (projection_actor = 0U;
             projection_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++projection_actor) {
            saw_visibility_transition =
                saw_visibility_transition ||
                court_frame.projection.players[
                    projection_actor].visible !=
                    previous_court_frame.projection.players[
                        projection_actor].visible;
        }
        previous_court_frame = court_frame;
        if (left_slice_probe.camera_state.camera_x ==
            previous_camera_x) {
            break;
        }
    }
    if (frame == 200U ||
        left_slice_probe.camera_state.camera_x != 0x0066U ||
        !saw_coarse_crossing || !saw_fine_scroll ||
        !saw_visibility_transition ||
        previous_court_frame.slice.possession !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        previous_court_frame.slice.direction != 0U ||
        previous_court_frame.slice.transition_serial != 0U ||
        previous_court_frame.slice.viewport.camera_x != 0x0066U ||
        previous_court_frame.projection.camera_x != 0x0066U ||
        previous_court_frame.slice.viewport.first_tile_x != 0x0CU ||
        previous_court_frame.slice.viewport.fine_scroll_x != 6U ||
        previous_court_frame.slice.viewport.column_count != 33U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "left-possession actor/court projection sweep failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    right_slice_probe = left_slice_probe;
    if (!scene_handoff_possession(
            &right_slice_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        !tecmo_gameplay_scene_court_frame(
            &right_slice_probe, &court_frame) ||
        court_frame.scene_frame != previous_court_frame.scene_frame ||
        court_frame.camera_follow_count !=
            previous_court_frame.camera_follow_count ||
        court_frame.slice.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        court_frame.slice.direction != 1U ||
        court_frame.slice.transition_serial != 1U ||
        memcmp(&court_frame.slice.viewport,
               &previous_court_frame.slice.viewport,
               sizeof(court_frame.slice.viewport)) != 0 ||
        memcmp(&court_frame.projection.players,
               &previous_court_frame.projection.players,
               sizeof(court_frame.projection.players)) != 0) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "possession reversal changed stationary actors");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    right_slice_probe.actors[right_slice_probe.ball_holder].position.x =
        0x020D;
    right_slice_probe.actors[right_slice_probe.ball_holder].facing_right =
        false;
    if (!scene_attach_ball(&right_slice_probe) ||
        !tecmo_gameplay_scene_court_frame(
            &right_slice_probe, &previous_court_frame)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "right-possession ball placement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    saw_coarse_crossing = false;
    saw_fine_scroll = false;
    saw_visibility_transition = false;
    for (frame = 0U; frame < 200U; ++frame) {
        size_t projection_actor;
        uint16_t previous_camera_x =
            right_slice_probe.camera_state.camera_x;
        if (!tecmo_gameplay_scene_test_follow_live_camera_once(&right_slice_probe) ||
            !tecmo_gameplay_scene_court_frame(
                &right_slice_probe, &court_frame) ||
            court_frame.scene_frame !=
                previous_court_frame.scene_frame ||
            court_frame.camera_follow_count !=
                previous_court_frame.camera_follow_count + 1U ||
            court_frame.slice.possession !=
                TECMO_GAMEPLAY_TEAM_HOME ||
            court_frame.slice.direction != 1U ||
            !scene_test_stationary_projection_transition(
                &previous_court_frame, &court_frame)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "right-possession camera follow failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        saw_coarse_crossing =
            saw_coarse_crossing ||
            court_frame.slice.viewport.first_tile_x !=
                previous_court_frame.slice.viewport.first_tile_x;
        saw_fine_scroll =
            saw_fine_scroll ||
            court_frame.slice.viewport.fine_scroll_x !=
                previous_court_frame.slice.viewport.fine_scroll_x;
        for (projection_actor = 0U;
             projection_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++projection_actor) {
            saw_visibility_transition =
                saw_visibility_transition ||
                court_frame.projection.players[
                    projection_actor].visible !=
                    previous_court_frame.projection.players[
                        projection_actor].visible;
        }
        previous_court_frame = court_frame;
        if (right_slice_probe.camera_state.camera_x ==
            previous_camera_x) {
            break;
        }
    }
    if (frame == 200U ||
        right_slice_probe.camera_state.camera_x != 0x0198U ||
        !saw_coarse_crossing || !saw_fine_scroll ||
        !saw_visibility_transition ||
        previous_court_frame.slice.possession !=
            TECMO_GAMEPLAY_TEAM_HOME ||
        previous_court_frame.slice.direction != 1U ||
        previous_court_frame.slice.transition_serial != 1U ||
        previous_court_frame.slice.viewport.camera_x != 0x0198U ||
        previous_court_frame.projection.camera_x != 0x0198U ||
        previous_court_frame.slice.viewport.first_tile_x != 0x33U ||
        previous_court_frame.slice.viewport.fine_scroll_x != 0U ||
        previous_court_frame.slice.viewport.column_count != 32U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "right-possession actor/court projection sweep failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!scene_test_live_hud_equal(&TEST_SCENE, &left_slice_probe) ||
        !scene_test_live_hud_equal(&TEST_SCENE, &right_slice_probe)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "live HUD changed across possession camera endpoints");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    camera_probe = TEST_SCENE;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01B9;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
        camera_probe.camera_follow_count != 1U ||
        camera_probe.camera_state.camera_x != 0x0107U ||
        camera_probe.camera_state.scroll_x != 0x07U ||
        camera_probe.camera_state.layout_cursor != 0x21U ||
        !camera_probe.camera_state.thresholds_valid ||
        !tecmo_gameplay_camera_state_live_valid(
            &camera_probe.camera_assets, &camera_probe.camera_state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single live camera follow/fine-scroll contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    fine_scroll_probe = camera_probe;
    camera_before = camera_probe.camera_state;
    frozen_follow_count = camera_probe.camera_follow_count;
    camera_probe.backcourt_state.frontcourt_established = 1U;
    if (!scene_handoff_possession(
            &camera_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        camera_probe.camera_state.camera_x != camera_before.camera_x ||
        camera_probe.camera_state.scroll_x != camera_before.scroll_x ||
        camera_probe.camera_state.thresholds_valid ||
        camera_probe.camera_state.endpoint_latched ||
        camera_probe.backcourt_state.frontcourt_established != 0U ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "possession camera continuity/latch reset failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01FF;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U ||
        camera_probe.camera_state.camera_x != camera_before.camera_x + 2U ||
        !camera_probe.camera_state.thresholds_valid ||
        !camera_probe.camera_state.endpoint_latched) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "first opposite-possession live follow failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 0U; frame < 200U; ++frame) {
        uint16_t previous_camera_x = camera_probe.camera_state.camera_x;
        if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
            camera_probe.camera_state.camera_x >
                TECMO_GAMEPLAY_COURT_MAX_CAMERA_X ||
            camera_probe.camera_state.layout_cursor > 0x34U ||
            !tecmo_gameplay_camera_state_live_valid(
                &camera_probe.camera_assets, &camera_probe.camera_state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "live camera endpoint bounds failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        if (camera_probe.camera_state.camera_x == previous_camera_x) break;
    }
    if (frame == 200U ||
        camera_probe.camera_state.layout_cursor != 0x34U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live camera did not settle at its cursor bound");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    frozen_camera = camera_probe.camera_state;
    frozen_follow_count = camera_probe.camera_follow_count;
    camera_probe.state.phase = TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE;
    if (!tecmo_gameplay_scene_test_follow_live_camera_once(&camera_probe) ||
        memcmp(&camera_probe.camera_state, &frozen_camera,
               sizeof(frozen_camera)) != 0 ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "free-throw camera freeze contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    camera_probe.state.phase = TECMO_GAMEPLAY_PHASE_LIVE;
    camera_probe.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_DUNK;
    camera_probe.shot_actor = camera_probe.ball_holder;
    camera_probe.shot_frame = TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME;
    if (!tecmo_gameplay_scene_test_follow_live_camera_once(&camera_probe) ||
        memcmp(&camera_probe.camera_state, &frozen_camera,
               sizeof(frozen_camera)) != 0 ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGDK cutaway camera freeze contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    camera_probe.shot_frame = TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME;
    if (!tecmo_gameplay_scene_test_follow_live_camera_once(&camera_probe) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGDK first-live-return camera resume failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    if (!tecmo_gameplay_court_orientation_state_valid(
            &TEST_SCENE.court_orientation, &TEST_SCENE.orientation_state) ||
        TEST_SCENE.orientation_state.current_direction != 0U ||
        TEST_SCENE.orientation_state.previous_direction != 0U ||
        TEST_SCENE.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        TEST_SCENE.orientation_state.transition_serial != 0U ||
        TEST_SCENE.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        TEST_SCENE.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation fresh-launch contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    orientation_before = TEST_SCENE.orientation_state;
    if (!scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        memcmp(&TEST_SCENE.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !tecmo_gameplay_reset_possession(
            &TEST_SCENE.state, TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        TEST_SCENE.orientation_state.current_direction != 1U ||
        TEST_SCENE.orientation_state.previous_direction != 0U ||
        TEST_SCENE.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME ||
        TEST_SCENE.orientation_state.transition_serial != 1U ||
        TEST_SCENE.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        TEST_SCENE.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "court-orientation changed-first handoff contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    orientation_before = TEST_SCENE.orientation_state;
    if (!scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        memcmp(&TEST_SCENE.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        TEST_SCENE.orientation_state.current_direction != 0U ||
        TEST_SCENE.orientation_state.previous_direction != 1U ||
        TEST_SCENE.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        TEST_SCENE.orientation_state.transition_serial != 2U ||
        TEST_SCENE.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        TEST_SCENE.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation no-op/roundtrip contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    gameplay_before = TEST_SCENE.state;
    orientation_before = TEST_SCENE.orientation_state;
    if (scene_handoff_possession(
            &TEST_SCENE, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT, 0U) ||
        memcmp(&TEST_SCENE.state, &gameplay_before,
               sizeof(gameplay_before)) != 0 ||
        memcmp(&TEST_SCENE.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_ownership_valid(&TEST_SCENE)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation invalid handoff mutated scene");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_scene_launch(&TEST_SCENE, &launch) ||
        TEST_SCENE.orientation_state.current_direction != 0U ||
        TEST_SCENE.orientation_state.transition_serial != 0U ||
        TEST_SCENE.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        TEST_SCENE.camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        TEST_SCENE.camera_state.layout_cursor != 0x21U ||
        !TEST_SCENE.camera_state.thresholds_valid ||
        TEST_SCENE.camera_state.endpoint_latched ||
        TEST_SCENE.camera_follow_count != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "court-orientation restart launch contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay render test allocation failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &TEST_SCENE, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "center-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    center_slice_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &left_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "left-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    left_slice_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &right_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "right-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    right_slice_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    if (center_slice_hash !=
            TECMO_GAMEPLAY_SCENE_CENTER_SLICE_FNV1A32 ||
        left_slice_hash !=
            TECMO_GAMEPLAY_SCENE_LEFT_SLICE_FNV1A32 ||
        right_slice_hash !=
            TECMO_GAMEPLAY_SCENE_RIGHT_SLICE_FNV1A32 ||
        center_slice_hash == left_slice_hash ||
        center_slice_hash == right_slice_hash ||
        left_slice_hash == right_slice_hash) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "possession TGCT slice hashes changed: center=%08X left=%08X right=%08X",
            (unsigned)center_slice_hash, (unsigned)left_slice_hash,
            (unsigned)right_slice_hash);
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(&TEST_SCENE, &framebuffer, 0, 0, 1, true)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "canonical gameplay render rejected");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    render_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    if (render_hash != TECMO_GAMEPLAY_SCENE_RENDER_FNV1A32) {
        char failure[128];
        (void)snprintf(failure, sizeof(failure),
                       "gameplay render hash mismatch: %08X",
                       (unsigned)render_hash);
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    draw_probe = TEST_SCENE;
    draw_probe.ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "background-only draw rejected invalid live ownership");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "live actor draw accepted invalid ownership or rendered partially");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    guarded_pixels = (uint32_t *)malloc(
        guarded_pixel_count * sizeof(*guarded_pixels));
    if (guarded_pixels == NULL) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "fine-scroll guarded render allocation failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    guarded_framebuffer.pixels = guarded_pixels;
    guarded_framebuffer.width = guard_width;
    guarded_framebuffer.height = guard_height;
    guarded_framebuffer.pitch_pixels = guard_width;
    for (pixel = 0U; pixel < guarded_pixel_count; ++pixel) {
        guarded_pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_court_slice_viewport(
            &fine_scroll_probe.court_world,
            fine_scroll_probe.camera_state.camera_x, &viewport) ||
        viewport.camera_x != 0x0107U ||
        viewport.first_tile_x != 0x20U ||
        viewport.fine_scroll_x != 7U ||
        viewport.column_count != 33U ||
        !tecmo_gameplay_scene_draw(
            &fine_scroll_probe, &guarded_framebuffer,
            guard_origin_x, guard_origin_y, 1, true) ||
        !scene_test_outer_margin_equal(
            guarded_pixels, guard_width, guard_height, guard_width,
            guard_origin_x, guard_origin_y,
            TECMO_GAMEPLAY_SCENE_NES_WIDTH,
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT, 0xA5A5A5A5U) ||
        scene_test_pixels_equal(
            guarded_pixels, guarded_pixel_count, 0xA5A5A5A5U)) {
        free(guarded_pixels);
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "33-column fine-scroll seam/margin render contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    free(guarded_pixels);

    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    draw_probe = TEST_SCENE;
    draw_probe.court_world.tiles_fingerprint ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "corrupt full-court world partially rendered");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    draw_probe = TEST_SCENE;
    draw_probe.camera_state.nametable_page ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid live camera partially rendered");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    draw_probe = TEST_SCENE;
    draw_probe.hud_assets.available = false;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "unavailable THUD asset partially rendered live scene");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    original_pose = TEST_SCENE.actors[0].pose_index;
    TEST_SCENE.actors[0].pose_index = UINT16_MAX;
    if (tecmo_gameplay_scene_draw(&TEST_SCENE, &framebuffer, 0, 0, 1, true)) {
        TEST_SCENE.actors[0].pose_index = original_pose;
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "invalid gameplay pose was accepted");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].pose_index = original_pose;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != 0xA5A5A5A5U) {
            free(pixels);
            tecmo_gameplay_scene_test_message(message, message_size,
                               "failed render partially modified pixels");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    invalid_framebuffer = framebuffer;
    invalid_framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH - 1;
    if (tecmo_gameplay_scene_draw(&TEST_SCENE, &invalid_framebuffer,
                                  0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "undersized gameplay framebuffer was accepted");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    free(pixels);
#undef TEST_SCENE
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
