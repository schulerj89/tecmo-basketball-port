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

static bool scene_test_background_selector_contract(
    const TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    static const uint8_t team_probes[] = {
        0U, 3U, 10U, TECMO_TEAM_DATA_LAKERS_TEAM_ID,
        TECMO_GAMEPLAY_TEAM_LIMIT - 1U
    };
    TecmoGameplayScene probe;
    TecmoGameplayLiveBackgroundContext context;
    TecmoGameplayLiveBackgroundContext unchanged;
    size_t index;
    if (scene == NULL) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "background selector scene was null");
        return false;
    }

    probe = *scene;
    probe.active = true;
    probe.pretip_state.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    probe.pretip_state.phase = TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST;
    probe.pretip_state.aborted = false;
    probe.pretip_state.live_handoff = false;
    for (index = 0U; index < sizeof(team_probes); ++index) {
        uint8_t expected = (uint8_t)(0x40U + team_probes[index]);
        probe.launch.away_team = team_probes[index];
        probe.launch.home_team = team_probes[index] == 0U ? 1U : 0U;
        memset(&context, 0xA5, sizeof(context));
        if (!scene_build_background_context(&probe, &context) ||
            context.pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT - 1U] !=
                expected ||
            context.pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT - 1U] ==
                0x3FU) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "pre-tip final R1 away-team selector contract failed");
            return false;
        }
    }

    probe = *scene;
    probe.active = true;
    probe.pretip_state.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    probe.pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
    probe.pretip_state.aborted = false;
    probe.pretip_state.live_handoff = true;
    probe.launch.away_team = 3U;
    for (index = 0U; index < sizeof(team_probes); ++index) {
        probe.launch.home_team = team_probes[index];
        memset(&context, 0xA5, sizeof(context));
        if (!scene_build_background_context(&probe, &context) ||
            context.pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT - 1U] !=
                (uint8_t)(0x40U + team_probes[index])) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "non-tip final R1 home-team selector contract changed");
            return false;
        }
    }

    memset(&context, 0xA5, sizeof(context));
    unchanged = context;
    probe = *scene;
    probe.pretip_state.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    probe.pretip_state.phase = TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST;
    probe.pretip_state.aborted = false;
    probe.pretip_state.live_handoff = false;
    probe.launch.away_team = TECMO_GAMEPLAY_TEAM_LIMIT;
    if (scene_build_background_context(&probe, &context) ||
        memcmp(&context, &unchanged, sizeof(context)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "malformed pre-tip away team was accepted or mutated output");
        return false;
    }
    probe = *scene;
    probe.launch.home_team = TECMO_GAMEPLAY_TEAM_LIMIT;
    memset(&context, 0xA5, sizeof(context));
    unchanged = context;
    if (scene_build_background_context(&probe, &context) ||
        memcmp(&context, &unchanged, sizeof(context)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "malformed non-tip home team was accepted or mutated output");
        return false;
    }
    memset(&context, 0xA5, sizeof(context));
    unchanged = context;
    if (scene_build_background_context(NULL, &context) ||
        memcmp(&context, &unchanged, sizeof(context)) != 0 ||
        scene_build_background_context(scene, NULL) ||
        tecmo_gameplay_assets_build_live_background_context(
            &scene->assets, 0x3FU, &context) ||
        tecmo_gameplay_assets_build_live_background_context(
            &scene->assets, 0x5BU, &context)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "malformed live selector input was accepted or mutated output");
        return false;
    }
    return true;
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

static bool scene_test_launch(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch,
    char *message,
    size_t message_size)
{
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, launch)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay scene canonical launch rejected");
        return false;
    }
    return true;
}

static bool scene_test_initial_world_state(
    const TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    if (scene->camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        scene->camera_state.scroll_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        scene->camera_state.scroll_aux != 0U ||
        scene->camera_state.nametable_page != 1U ||
        scene->camera_state.aux != 0U ||
        scene->camera_state.stream_direction != 1U ||
        scene->camera_state.layout_cursor != 0x0FU ||
        scene->camera_state.left_threshold != 0xD8U ||
        scene->camera_state.right_threshold != 0xE8U ||
        !scene->camera_state.thresholds_valid ||
        !scene->camera_state.endpoint_latched ||
        scene->camera_follow_count != 0U ||
        scene->actors[0].position.x != 0x0160 ||
        scene->actors[0].position.y != 198 ||
        scene->actors[0].pose_index != 149U ||
        scene->actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].pose_index !=
            181U ||
        scene->ball_position.x_q8 != 348 * 256 ||
        scene->ball_position.y_q8 != 181 * 256 ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "TGCP-2 live prime/initial world-state contract failed");
        return false;
    }
    return true;
}

static bool scene_test_hud(
    const TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    if (!scene_test_live_hud_contract(scene)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "THUD live score/player/clock projection contract failed");
        return false;
    }
    return true;
}

static bool scene_test_backcourt(
    const TecmoGameplayScene *scene,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene backcourt_probe = *scene;
    uint8_t holder;
    backcourt_probe = *scene;
    memset(p1, 0, sizeof(*p1));
    memset(p2, 0, sizeof(*p2));
    backcourt_probe.actors[0U].position.x = 368;
    backcourt_probe.actors[0U].position.y = 148;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.actors[0U].facing_right = true;
    backcourt_probe.actors[0U].movement_direction = 0U;
    backcourt_probe.actors[0U].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[0U].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[0U].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 375 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, p1, p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live backcourt frontcourt latch failed");
        return false;
    }
    backcourt_probe.actors[0U].position.x = 380;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.ball_position.x_q8 = 386 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, p1, p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_HOME ||
        backcourt_probe.state.phase_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live backcourt settlement route failed");
        return false;
    }
    backcourt_probe = *scene;
    holder = TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
    if (!scene_handoff_possession(
            &backcourt_probe, TECMO_GAMEPLAY_TEAM_HOME, holder) ||
        backcourt_probe.orientation_state.current_direction != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse backcourt possession setup failed");
        return false;
    }
    backcourt_probe.actors[holder].position.x = 398;
    backcourt_probe.actors[holder].position.y = 148;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.actors[holder].facing_right = false;
    backcourt_probe.actors[holder].movement_direction = 1U;
    backcourt_probe.actors[holder].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[holder].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[holder].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 392 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, p1, p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse live backcourt frontcourt latch failed");
        return false;
    }
    backcourt_probe.actors[holder].position.x = 387;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.ball_position.x_q8 = 383 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, p1, p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        backcourt_probe.state.phase_frame != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "reverse live backcourt settlement route failed");
        return false;
    }
    return true;
}

static bool scene_test_pose(
    const TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    TecmoGameplayResolvedPose resolved_pose;
    if (!scene_resolve_actor_pose(scene, 0U, &resolved_pose) ||
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
            scene, TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT,
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
            scene, TECMO_GAMEPLAY_BALL_POSE, 0xC1U, 0U, 0U, false, 0U,
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
        return false;
    }
    return true;
}

static bool scene_test_transactional_snapshots(
    TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    TecmoGameplayCameraState camera_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtCoordinates unchanged_coordinates;
    TecmoGameplaySceneCourtProjection court_projection;
    TecmoGameplaySceneCourtProjection unchanged_court_projection;
    TecmoGameplaySceneCourtSlice court_slice;
    TecmoGameplaySceneCourtSlice unchanged_court_slice;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame unchanged_court_frame;
    memset(&coordinates, 0xA5, sizeof(coordinates));
    if (!tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        coordinates.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG ||
        coordinates.players[0].x != 0x0160 ||
        coordinates.players[0].y != 198 ||
        coordinates.ball.x_q8 != 348 * 256 ||
        coordinates.ball.y_q8 != 181 * 256 ||
        coordinates.hoops[0].x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        coordinates.hoops[0].y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        coordinates.hoops[1].x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        coordinates.hoops[1].y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical player/ball/hoop coordinate snapshot failed");
        return false;
    }
    memset(&court_projection, 0xA5, sizeof(court_projection));
    if (!tecmo_gameplay_scene_court_projection(
            scene, &court_projection) ||
        court_projection.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_PROJECTION_TAG ||
        court_projection.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_projection.reserved != 0U ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0xDCU ||
        court_projection.players[0].screen_y != 198U ||
        !court_projection.ball.visible ||
        court_projection.ball.screen_x != 0xD8U ||
        court_projection.ball.screen_y != 181U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP scene projection snapshot failed");
        return false;
    }
    memset(&court_slice, 0xA5, sizeof(court_slice));
    if (!tecmo_gameplay_scene_court_slice(scene, &court_slice) ||
        court_slice.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_SLICE_TAG ||
        court_slice.transition_serial != 0U ||
        court_slice.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        court_slice.direction != 0U ||
        court_slice.reserved != 0U ||
        court_slice.viewport.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_slice.viewport.first_tile_x != 0x10U ||
        court_slice.viewport.fine_scroll_x != 4U ||
        court_slice.viewport.column_count != 33U ||
        court_slice.viewport.camera_x != court_projection.camera_x) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "possession-aware TGCT center slice snapshot failed");
        return false;
    }
    memset(&court_frame, 0xA5, sizeof(court_frame));
    if (!tecmo_gameplay_scene_court_frame(
            scene, &court_frame) ||
        court_frame.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        court_frame.scene_frame != scene->frame ||
        court_frame.camera_follow_count != scene->camera_follow_count ||
        court_frame.reserved != 0U ||
        memcmp(&court_frame.slice, &court_slice,
               sizeof(court_slice)) != 0 ||
        memcmp(&court_frame.projection, &court_projection,
               sizeof(court_projection)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "camera-coherent center court frame snapshot failed");
        return false;
    }
    unchanged_coordinates = coordinates;
    unchanged_court_projection = court_projection;
    unchanged_court_slice = court_slice;
    unchanged_court_frame = court_frame;
    scene->ball_position.x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X + 1;
    if (tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0 ||
        tecmo_gameplay_scene_court_projection(
            scene, &court_projection) ||
        memcmp(&court_projection, &unchanged_court_projection,
               sizeof(court_projection)) != 0 ||
        tecmo_gameplay_scene_court_frame(
            scene, &court_frame) ||
        memcmp(&court_frame, &unchanged_court_frame,
               sizeof(court_frame)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid coordinate snapshot mutated output");
        return false;
    }
    camera_before = scene->camera_state;
    scene->camera_state.camera_x =
        TECMO_GAMEPLAY_COURT_MAX_CAMERA_X + 1U;
    if (tecmo_gameplay_scene_court_slice(
            scene, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid camera mutated possession-aware TGCT slice");
        return false;
    }
    scene->camera_state = camera_before;
    orientation_before = scene->orientation_state;
    scene->orientation_state.tracked_possession_team =
        TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME;
    if (tecmo_gameplay_scene_court_slice(
            scene, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "mismatched possession mutated TGCT scene slice");
        return false;
    }
    scene->orientation_state = orientation_before;
    scene->ball_position = unchanged_coordinates.ball;
    scene->actors[0].position.x = 0;
    if (!tecmo_gameplay_scene_court_projection(
            scene, &court_projection) ||
        court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0U ||
        court_projection.players[0].screen_y != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP offscreen projection sentinel failed");
        return false;
    }
    scene->actors[0].position =
        unchanged_coordinates.players[0];
    memset(&scene->shot_start_position, 0,
           sizeof(scene->shot_start_position));
    memset(&scene->shot_end_position, 0,
           sizeof(scene->shot_end_position));
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    scene->shot_actor = 0U;
    scene->jump_actor_altitude_q8 = 10U * 256U;
    if (!tecmo_gameplay_scene_court_projection(
            scene, &court_projection) ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0xDCU ||
        court_projection.players[0].screen_y != 188U ||
        court_projection.ball.screen_y != 181U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "canonical TGCP jump-altitude projection failed");
        return false;
    }
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->jump_actor_altitude_q8 = 0U;
    scene->actors[0].position.x = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid player coordinate snapshot mutated output");
        return false;
    }
    scene->actors[0].position =
        unchanged_coordinates.players[0];
    scene->actors[0].anchor.y =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_Y + 1);
    if (tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid player anchor snapshot mutated output");
        return false;
    }
    scene->actors[0].anchor =
        unchanged_coordinates.players[0];
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    scene->shot_start_position.x_q8 = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid shot coordinate snapshot mutated output");
        return false;
    }
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    memset(&scene->shot_start_position, 0,
           sizeof(scene->shot_start_position));

    return true;
}

static bool scene_test_bounds_and_projection(
    TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneActor boundary_actor;
    TecmoGameplayActorProjection projection;
    memset(&boundary_actor, 0, sizeof(boundary_actor));
    boundary_actor.active = true;
    boundary_actor.position.y = TECMO_GAMEPLAY_MIN_Y;
    boundary_actor.position.x = -1;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x00DF ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-0 scene-safety boundary diverged");
        return false;
    }
    boundary_actor.position.y = 128;
    boundary_actor.position.x = 300;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 300 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-1 scene-safety interior diverged");
        return false;
    }
    boundary_actor.position.y = TECMO_GAMEPLAY_MAX_Y;
    boundary_actor.position.x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x0297 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "page-2 scene-safety boundary diverged");
        return false;
    }

    memset(&projection, 0xA5, sizeof(projection));
    if (!tecmo_gameplay_camera_project_actor(
            &scene->camera_assets, &scene->camera_state,
            0x0100U, 100U, 10U, &projection) ||
        !projection.visible || projection.screen_x != 0x7CU ||
        projection.screen_y != 90U ||
        !tecmo_gameplay_camera_project_actor(
            &scene->camera_assets, &scene->camera_state,
            0x0183U, 100U, 0U, &projection) ||
        !projection.visible || projection.screen_x != 0xFFU ||
        !tecmo_gameplay_camera_project_actor(
            &scene->camera_assets, &scene->camera_state,
            0x0083U, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U ||
        !tecmo_gameplay_camera_project_actor(
            &scene->camera_assets, &scene->camera_state,
            0x0184U, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live actor projection/offscreen contract failed");
        return false;
    }

    return true;
}

static bool scene_test_camera_endpoint_sweep(
    TecmoGameplayScene *probe,
    uint16_t holder_x,
    bool facing_right,
    TecmoGameplayTeam possession,
    uint8_t direction,
    uint16_t expected_transition_serial,
    uint16_t expected_camera_x,
    uint8_t expected_first_tile_x,
    uint8_t expected_fine_scroll_x,
    uint8_t expected_column_count,
    const char *placement_message,
    const char *follow_message,
    const char *sweep_message,
    TecmoGameplaySceneCourtFrame *last_frame,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame previous_court_frame;
    bool saw_coarse_crossing = false;
    bool saw_fine_scroll = false;
    bool saw_visibility_transition = false;
    size_t frame;

    probe->actors[probe->ball_holder].position.x = holder_x;
    probe->actors[probe->ball_holder].facing_right = facing_right;
    probe->actors[probe->ball_holder].movement_direction =
        facing_right ? 0U : 1U;
    if (!scene_attach_ball(probe) ||
        !tecmo_gameplay_scene_court_frame(
            probe, &previous_court_frame)) {
        tecmo_gameplay_scene_test_message(
            message, message_size, placement_message);
        return false;
    }
    for (frame = 0U; frame < 200U; ++frame) {
        size_t projection_actor;
        uint16_t previous_camera_x =
            probe->camera_state.camera_x;
        if (!tecmo_gameplay_scene_test_follow_live_camera_once(probe) ||
            !tecmo_gameplay_scene_court_frame(
                probe, &court_frame) ||
            court_frame.scene_frame != previous_court_frame.scene_frame ||
            court_frame.camera_follow_count !=
                previous_court_frame.camera_follow_count + 1U ||
            court_frame.slice.possession != possession ||
            court_frame.slice.direction != direction ||
            !scene_test_stationary_projection_transition(
                &previous_court_frame, &court_frame)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, follow_message);
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
        if (probe->camera_state.camera_x == previous_camera_x) break;
    }
    if (frame == 200U ||
        probe->camera_state.camera_x != expected_camera_x ||
        !saw_coarse_crossing || !saw_fine_scroll ||
        !saw_visibility_transition ||
        previous_court_frame.slice.possession != possession ||
        previous_court_frame.slice.direction != direction ||
        previous_court_frame.slice.transition_serial !=
            expected_transition_serial ||
        previous_court_frame.slice.viewport.camera_x !=
            expected_camera_x ||
        previous_court_frame.projection.camera_x != expected_camera_x ||
        previous_court_frame.slice.viewport.first_tile_x !=
            expected_first_tile_x ||
        previous_court_frame.slice.viewport.fine_scroll_x !=
            expected_fine_scroll_x ||
        previous_court_frame.slice.viewport.column_count !=
            expected_column_count) {
        tecmo_gameplay_scene_test_message(
            message, message_size, sweep_message);
        return false;
    }
    if (last_frame != NULL) *last_frame = previous_court_frame;
    return true;
}

static bool scene_test_camera_sweeps(
    const TecmoGameplayScene *scene,
    TecmoGameplayScene *left_probe,
    TecmoGameplayScene *right_probe,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame previous_court_frame;

    *left_probe = *scene;
    if (!scene_test_camera_endpoint_sweep(
            left_probe, 0x00F3U, false, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
            0U, 0x0066U, 0x0CU, 6U, 33U,
            "left-possession ball placement failed",
            "left-possession camera follow failed",
            "left-possession actor/court projection sweep failed",
            &previous_court_frame, message, message_size)) {
        return false;
    }
    *right_probe = *left_probe;
    if (!scene_handoff_possession(
            right_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        !tecmo_gameplay_scene_court_frame(
            right_probe, &court_frame) ||
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
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "possession reversal changed stationary actors");
        return false;
    }
    if (!scene_test_camera_endpoint_sweep(
            right_probe, 0x020DU, true, TECMO_GAMEPLAY_TEAM_HOME, 1U,
            1U, 0x0198U, 0x33U, 0U, 32U,
            "right-possession ball placement failed",
            "right-possession camera follow failed",
            "right-possession actor/court projection sweep failed",
            &court_frame, message, message_size)) {
        return false;
    }
    if (!scene_test_live_hud_equal(scene, left_probe) ||
        !scene_test_live_hud_equal(scene, right_probe)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "live HUD changed across possession camera endpoints");
        return false;
    }
    return true;
}

static bool scene_test_camera_continuity(
    const TecmoGameplayScene *scene,
    TecmoGameplayScene *fine_scroll_probe,
    TecmoControlFrame *p1,
    TecmoControlFrame *p2,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene camera_probe = *scene;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayCameraState frozen_camera;
    uint32_t frozen_follow_count;
    size_t frame;
    camera_probe = *scene;
    memset(p1, 0, sizeof(*p1));
    memset(p2, 0, sizeof(*p2));
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01B9;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, p1, p2) ||
        camera_probe.camera_follow_count != 1U ||
        camera_probe.camera_state.camera_x != 0x0086U ||
        camera_probe.camera_state.scroll_x != 0x86U ||
        camera_probe.camera_state.layout_cursor != 0x0FU ||
        !camera_probe.camera_state.thresholds_valid ||
        !tecmo_gameplay_camera_state_live_valid(
            &camera_probe.camera_assets, &camera_probe.camera_state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "single live camera follow/fine-scroll contract failed");
        return false;
    }
    *fine_scroll_probe = camera_probe;
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
        return false;
    }
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01FF;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, p1, p2) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U ||
        camera_probe.camera_state.camera_x != camera_before.camera_x + 2U ||
        !camera_probe.camera_state.thresholds_valid ||
        !camera_probe.camera_state.endpoint_latched) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "first opposite-possession live follow failed");
        return false;
    }
    for (frame = 0U; frame < 200U; ++frame) {
        uint16_t previous_camera_x = camera_probe.camera_state.camera_x;
        if (!tecmo_gameplay_scene_update(&camera_probe, p1, p2) ||
            camera_probe.camera_state.camera_x >
                TECMO_GAMEPLAY_COURT_MAX_CAMERA_X ||
            camera_probe.camera_state.layout_cursor > 0x34U ||
            !tecmo_gameplay_camera_state_live_valid(
                &camera_probe.camera_assets, &camera_probe.camera_state)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "live camera endpoint bounds failed");
            return false;
        }
        if (camera_probe.camera_state.camera_x == previous_camera_x) break;
    }
    if (frame == 200U ||
        camera_probe.camera_state.layout_cursor != 0x34U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live camera did not settle at its cursor bound");
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
        return false;
    }
    camera_probe.shot_frame = TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME;
    if (!tecmo_gameplay_scene_test_follow_live_camera_once(&camera_probe) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGDK first-live-return camera resume failed");
        return false;
    }

    return true;
}

static bool scene_test_orientation_contract(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch,
    char *message,
    size_t message_size)
{
    TecmoGameplayState gameplay_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayScene orientation_probe;
    TecmoControlFrame neutral;
    TecmoControlFrame horizontal;
    TecmoGameplayCourtCoordinateQ8 attached_ball;
    bool facing_right;
    size_t actor;
    if (!tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        scene->orientation_state.current_direction != 0U ||
        scene->orientation_state.previous_direction != 0U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene->orientation_state.transition_serial != 0U ||
        scene->orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene->orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation fresh-launch contract failed");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        bool expected_facing;
        if (!scene_goal_facing_right_for_team(
            scene, (TecmoGameplayTeam)scene->actors[actor].team,
                &expected_facing) ||
            scene->actors[actor].facing_right != expected_facing ||
            (scene->actors[actor].team == TECMO_GAMEPLAY_TEAM_AWAY
                ? scene->actors[actor].facing_right
                : !scene->actors[actor].facing_right)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "fresh Away-left/Home-right facing matrix failed");
            return false;
        }
    }
    orientation_probe = *scene;
    facing_right = true;
    orientation_probe.orientation_state.offensive_hoop.y =
        (int16_t)(TECMO_GAMEPLAY_COURT_HOOP_Y - 1);
    if (scene_goal_facing_right_for_team(
            &orientation_probe, TECMO_GAMEPLAY_TEAM_AWAY,
            &facing_right) || !facing_right) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "malformed orientation facing was not fail-closed");
        return false;
    }
    orientation_probe = *scene;
    facing_right = true;
    orientation_probe.orientation_state.tracked_possession_team = 2U;
    if (scene_goal_facing_right_for_team(
            &orientation_probe, TECMO_GAMEPLAY_TEAM_AWAY,
            &facing_right) || !facing_right ||
        scene_goal_facing_right_for_team(
            scene, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT,
            &facing_right)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "malformed team/orientation facing was not fail-closed");
        return false;
    }
    orientation_probe = *scene;
    orientation_probe.orientation_state.current_direction = 1U;
    orientation_probe.orientation_state.previous_direction = 0U;
    orientation_probe.orientation_state.transition_serial = 1U;
    orientation_probe.orientation_state.tracked_possession_team =
        TECMO_GAMEPLAY_TEAM_AWAY;
    orientation_probe.orientation_state.offensive_hoop =
        scene->court_orientation.hoops[1U];
    if (!scene_goal_facing_right_for_team(
            &orientation_probe, TECMO_GAMEPLAY_TEAM_AWAY,
            &facing_right) || !facing_right ||
        !scene_goal_facing_right_for_team(
            &orientation_probe, TECMO_GAMEPLAY_TEAM_HOME,
            &facing_right) || facing_right) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "crossed team/direction facing matrix failed");
        return false;
    }
    memset(&neutral, 0, sizeof(neutral));
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        !scene_move_controlled_actor(scene, 0U, &neutral) ||
        scene->actors[0U].facing_right ||
        !scene_attached_ball_position(
            &scene->actors[0U], &attached_ball) ||
        attached_ball.x_q8 !=
            (int32_t)(scene->actors[0U].position.x - 7) * 256) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "neutral movement changed Away goal-facing baseline");
        return false;
    }
    memset(&horizontal, 0, sizeof(horizontal));
    horizontal.held.right = true;
    if (!scene_move_controlled_actor(scene, 0U, &horizontal) ||
        !scene->actors[0U].facing_right ||
        !scene_attached_ball_position(
            &scene->actors[0U], &attached_ball) ||
        attached_ball.x_q8 !=
            (int32_t)(scene->actors[0U].position.x + 7) * 256 ||
        !scene_move_controlled_actor(scene, 0U, &neutral) ||
        !scene->actors[0U].facing_right ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        scene->actors[0U].facing_right) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "Away horizontal facing override/reset matrix failed");
        return false;
    }
    orientation_before = scene->orientation_state;
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        memcmp(&scene->orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !tecmo_gameplay_reset_possession(
            &scene->state, TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        scene->orientation_state.current_direction != 1U ||
        scene->orientation_state.previous_direction != 0U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME ||
        scene->orientation_state.transition_serial != 1U ||
        scene->orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        scene->orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "court-orientation changed-first handoff contract failed");
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        bool expected_facing;
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)scene->actors[actor].team,
                &expected_facing) ||
            scene->actors[actor].facing_right != expected_facing ||
            (scene->actors[actor].team == TECMO_GAMEPLAY_TEAM_AWAY
                ? scene->actors[actor].facing_right
                : !scene->actors[actor].facing_right)) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "Home-right/Away-left possession transition facing failed");
            return false;
        }
    }
    memset(&horizontal, 0, sizeof(horizontal));
    horizontal.held.left = true;
    if (!scene_move_controlled_actor(scene, 1U, &horizontal) ||
        scene->actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].facing_right ||
        !scene_move_controlled_actor(scene, 1U, &neutral) ||
        scene->actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].facing_right ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        !scene->actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].facing_right) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "Home horizontal facing override/reset matrix failed");
        return false;
    }
    orientation_before = scene->orientation_state;
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        memcmp(&scene->orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        scene->orientation_state.current_direction != 0U ||
        scene->orientation_state.previous_direction != 1U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene->orientation_state.transition_serial != 2U ||
        scene->orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene->orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation no-op/roundtrip contract failed");
        return false;
    }
    gameplay_before = scene->state;
    orientation_before = scene->orientation_state;
    if (scene_handoff_possession(
            scene, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT, 0U) ||
        memcmp(&scene->state, &gameplay_before,
               sizeof(gameplay_before)) != 0 ||
        memcmp(&scene->orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_ownership_valid(scene)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "court-orientation invalid handoff mutated scene");
        return false;
    }
    if (!tecmo_gameplay_scene_launch(scene, launch) ||
        scene->orientation_state.current_direction != 0U ||
        scene->orientation_state.transition_serial != 0U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene->camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        scene->camera_state.layout_cursor != 0x0FU ||
        !scene->camera_state.thresholds_valid ||
        !scene->camera_state.endpoint_latched ||
        scene->camera_follow_count != 0U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "court-orientation restart launch contract failed");
        return false;
    }
    return true;
}


typedef struct TecmoGameplaySceneTestRenderBuffers {
    uint32_t *pixels;
    size_t pixel_count;
    TecmoFramebuffer framebuffer;
} TecmoGameplaySceneTestRenderBuffers;

static bool scene_test_pretip_logo_contract(
    const TecmoGameplayScene *scene,
    char *message,
    size_t message_size)
{
    const int width = TECMO_GAMEPLAY_SCENE_NES_WIDTH + 24;
    const int height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT + 20;
    const int origin_x = 12;
    const int origin_y = 10;
    const size_t pixel_count = (size_t)width * (size_t)height;
    const uint32_t sentinel = 0xA5A5A5A5U;
    TecmoGameplayScene probe;
    TecmoTeamDataAsset *team_data = NULL;
    TecmoFramebuffer framebuffer;
    uint32_t *pixels = NULL;
    size_t pixel;
    const char *failure = NULL;

    if (scene == NULL || scene->pretip_team_data == NULL) {
        failure = "pre-tip logo data was unavailable";
        goto done;
    }
    team_data = (TecmoTeamDataAsset *)malloc(sizeof(*team_data));
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (team_data == NULL || pixels == NULL) {
        failure = "pre-tip logo contract allocation failed";
        goto done;
    }
    *team_data = *scene->pretip_team_data;
    probe = *scene;
    probe.active = true;
    probe.pretip_team_data = team_data;
    probe.launch.away_team = 3U;
    probe.launch.home_team = 10U;
    probe.pretip_state.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    probe.pretip_state.phase = TECMO_GAMEPLAY_PRETIP_MATCHUP;
    probe.pretip_state.phase_frame = 30U;
    probe.pretip_state.aborted = false;
    probe.pretip_state.live_handoff = false;
    framebuffer.pixels = pixels;
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.pitch_pixels = width;

    /* The renderer must accept both compact and full 10x6 TTDT logos. */
    team_data->teams[3U].logo_width = 1U;
    team_data->teams[3U].logo_height = 1U;
    team_data->teams[3U].logo_count = 1U;
    team_data->teams[10U].logo_width = 2U;
    team_data->teams[10U].logo_height = 2U;
    team_data->teams[10U].logo_count = 4U;
    for (pixel = 0U; pixel < pixel_count; ++pixel) pixels[pixel] = sentinel;
    if (!tecmo_gameplay_scene_draw(
            &probe, &framebuffer, origin_x, origin_y, 1, false) ||
        !scene_test_outer_margin_equal(
            pixels, width, height, width, origin_x, origin_y,
            TECMO_GAMEPLAY_SCENE_NES_WIDTH,
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT, sentinel) ||
        scene_test_pixels_equal(pixels, pixel_count, sentinel)) {
        failure = "variable TTDT logo dimensions rendered incorrectly";
        goto done;
    }

    team_data->teams[3U].logo_width = 10U;
    team_data->teams[3U].logo_height = 6U;
    team_data->teams[3U].logo_count = 60U;
    team_data->teams[10U].logo_width = 1U;
    team_data->teams[10U].logo_height = 1U;
    team_data->teams[10U].logo_count = 1U;
    for (pixel = 0U; pixel < pixel_count; ++pixel) pixels[pixel] = sentinel;
    if (!tecmo_gameplay_scene_draw(
            &probe, &framebuffer, origin_x, origin_y, 1, false) ||
        !scene_test_outer_margin_equal(
            pixels, width, height, width, origin_x, origin_y,
            TECMO_GAMEPLAY_SCENE_NES_WIDTH,
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT, sentinel) ||
        scene_test_pixels_equal(pixels, pixel_count, sentinel)) {
        failure = "10x6 TTDT logo rendered incorrectly";
        goto done;
    }

    /* A malformed logo must be rejected before the card template touches the
       destination framebuffer. */
    team_data->teams[3U].logo_width = 3U;
    team_data->teams[3U].logo_height = 2U;
    team_data->teams[3U].logo_count = 5U;
    for (pixel = 0U; pixel < pixel_count; ++pixel) pixels[pixel] = sentinel;
    if (tecmo_gameplay_scene_draw(
            &probe, &framebuffer, origin_x, origin_y, 1, false) ||
        !scene_test_pixels_equal(pixels, pixel_count, sentinel)) {
        failure = "malformed TTDT logo was accepted or partially rendered";
        goto done;
    }
    probe.launch.home_team = TECMO_GAMEPLAY_TEAM_LIMIT;
    for (pixel = 0U; pixel < pixel_count; ++pixel) pixels[pixel] = sentinel;
    if (tecmo_gameplay_scene_draw(
            &probe, &framebuffer, origin_x, origin_y, 1, false) ||
        !scene_test_pixels_equal(pixels, pixel_count, sentinel)) {
        failure = "malformed pre-tip team id was accepted or partially rendered";
        goto done;
    }

done:
    free(pixels);
    free(team_data);
    if (failure != NULL) {
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    return true;
}

static bool scene_test_render_hashes(
    const TecmoGameplayScene *scene,
    const TecmoGameplayScene *left_slice_probe,
    const TecmoGameplayScene *right_slice_probe,
    TecmoGameplaySceneTestRenderBuffers *buffers,
    char *message,
    size_t message_size)
{
    uint32_t *pixels;
    TecmoFramebuffer framebuffer;
    uint32_t render_hash;
    uint32_t center_slice_hash;
    uint32_t left_slice_hash;
    uint32_t right_slice_hash;
    size_t pixel;
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "gameplay render test allocation failed");
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
            scene, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "center-possession TGCT slice render rejected");
        return false;
    }
    center_slice_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            left_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "left-possession TGCT slice render rejected");
        return false;
    }
    left_slice_hash = tecmo_gameplay_scene_test_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            right_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "right-possession TGCT slice render rejected");
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
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(scene, &framebuffer, 0, 0, 1, true)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "canonical gameplay render rejected");
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
        return false;
    }
    buffers->pixels = pixels;
    buffers->pixel_count = pixel_count;
    buffers->framebuffer = framebuffer;
    return true;
}

static bool scene_test_framebuffer_fail_closed(
    TecmoGameplayScene *scene,
    const TecmoGameplayScene *fine_scroll_probe,
    TecmoGameplaySceneTestRenderBuffers *buffers,
    char *message,
    size_t message_size)
{
    uint32_t *pixels = buffers->pixels;
    const size_t pixel_count = buffers->pixel_count;
    TecmoFramebuffer framebuffer = buffers->framebuffer;
    TecmoGameplayScene draw_probe;
    TecmoFramebuffer guarded_framebuffer;
    TecmoFramebuffer invalid_framebuffer;
    uint32_t *guarded_pixels;
    TecmoGameplayCourtViewport viewport;
    uint16_t original_pose;
    size_t pixel;
    const int guard_width = TECMO_GAMEPLAY_SCENE_NES_WIDTH + 24;
    const int guard_height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT + 20;
    const int guard_origin_x = 12;
    const int guard_origin_y = 10;
    const size_t guarded_pixel_count =
        (size_t)guard_width * (size_t)guard_height;
    draw_probe = *scene;
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
        return false;
    }
    guarded_pixels = (uint32_t *)malloc(
        guarded_pixel_count * sizeof(*guarded_pixels));
    if (guarded_pixels == NULL) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "fine-scroll guarded render allocation failed");
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
            &fine_scroll_probe->court_world,
            fine_scroll_probe->camera_state.camera_x, &viewport) ||
        viewport.camera_x != 0x0086U ||
        viewport.first_tile_x != 0x10U ||
        viewport.fine_scroll_x != 6U ||
        viewport.column_count != 33U ||
        !tecmo_gameplay_scene_draw(
            fine_scroll_probe, &guarded_framebuffer,
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
        return false;
    }
    free(guarded_pixels);

    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    draw_probe = *scene;
    draw_probe.court_world.tiles_fingerprint ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "corrupt full-court world partially rendered");
        return false;
    }
    draw_probe = *scene;
    draw_probe.camera_state.nametable_page ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "invalid live camera partially rendered");
        return false;
    }
    draw_probe = *scene;
    draw_probe.hud_assets.available = false;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "unavailable THUD asset partially rendered live scene");
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    original_pose = scene->actors[0].pose_index;
    scene->actors[0].pose_index = UINT16_MAX;
    if (tecmo_gameplay_scene_draw(scene, &framebuffer, 0, 0, 1, true)) {
        scene->actors[0].pose_index = original_pose;
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "invalid gameplay pose was accepted");
        return false;
    }
    scene->actors[0].pose_index = original_pose;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != 0xA5A5A5A5U) {
            free(pixels);
            tecmo_gameplay_scene_test_message(message, message_size,
                               "failed render partially modified pixels");
            return false;
        }
    }
    invalid_framebuffer = framebuffer;
    invalid_framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH - 1;
    if (tecmo_gameplay_scene_draw(scene, &invalid_framebuffer,
                                  0, 0, 1, false)) {
        free(pixels);
        tecmo_gameplay_scene_test_message(message, message_size,
                           "undersized gameplay framebuffer was accepted");
        return false;
    }
    free(pixels);
    buffers->pixels = NULL;
    return true;
}


bool tecmo_gameplay_scene_test_render_contract(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplayScene *scene = test->scene;
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;
    TecmoGameplayScene left_probe;
    TecmoGameplayScene right_probe;
    TecmoGameplayScene fine_scroll_probe;
    TecmoGameplaySceneTestRenderBuffers buffers = {0};

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!scene_test_launch(
            scene, &launch, message, message_size) ||
        !scene_test_initial_world_state(
            scene, message, message_size) ||
        !scene_test_hud(scene, message, message_size) ||
        !scene_test_backcourt(
            scene, &p1, &p2, message, message_size) ||
        !scene_test_pose(scene, message, message_size) ||
        !scene_test_transactional_snapshots(
            scene, message, message_size) ||
        !scene_test_bounds_and_projection(
            scene, message, message_size) ||
        !scene_test_camera_sweeps(
            scene, &left_probe, &right_probe, message, message_size) ||
        !scene_test_camera_continuity(
            scene, &fine_scroll_probe, &p1, &p2,
            message, message_size) ||
        !scene_test_orientation_contract(
            scene, &launch, message, message_size) ||
        !scene_test_background_selector_contract(
            scene, message, message_size) ||
        !scene_test_pretip_logo_contract(
            scene, message, message_size)) {
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    if (!scene_test_render_hashes(
            scene, &left_probe, &right_probe, &buffers,
            message, message_size) ||
        !scene_test_framebuffer_fail_closed(
            scene, &fine_scroll_probe, &buffers,
            message, message_size)) {
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
