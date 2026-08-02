#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native gameplay rendering, HUD, pre-tip, and cutaway composition. */

static void scene_fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                            int width, int height, uint32_t color);

static bool scene_framebuffer_valid(const TecmoFramebuffer *framebuffer,
                                    int origin_x, int origin_y, int scale)
{
    size_t pitch;
    size_t height;
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
        framebuffer->pitch_pixels < framebuffer->width || scale <= 0 ||
        scale > 8 || origin_x < 0 || origin_y < 0 ||
        origin_x > framebuffer->width -
                       TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale ||
        origin_y > framebuffer->height -
                       TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale) {
        return false;
    }
    pitch = (size_t)framebuffer->pitch_pixels;
    height = (size_t)framebuffer->height;
    return height == 0U || pitch <= SIZE_MAX / height;
}

bool tecmo_gameplay_scene_in_pretip(const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->lifecycle_tag == TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG &&
           scene->active &&
           tecmo_gameplay_pretip_is_presentation(&scene->pretip_state);
}

bool tecmo_gameplay_scene_render_build_background_context(
    const TecmoGameplayScene *scene,
    TecmoGameplayLiveBackgroundContext *context)
{
    bool pretip;
    uint8_t final_r1_selector;
    if (scene == NULL || context == NULL ||
        scene->launch.away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        scene->launch.home_team >= TECMO_GAMEPLAY_TEAM_LIMIT) {
        return false;
    }
    /*
     * TGPL's final live-band R1 selector is the pre-ASL team selector
     * ($40 + team id). During the pre-tip jump-contest the away team is
     * still the live court's team binding; after handoff the home team is
     * selected by the ordinary live path.
     */
    pretip = tecmo_gameplay_scene_in_pretip(scene);
    final_r1_selector = (uint8_t)(0x40U +
        (pretip ? scene->launch.away_team : scene->launch.home_team));
    return tecmo_gameplay_assets_build_live_background_context(
        &scene->assets, final_r1_selector, context);
}

static bool scene_background_tile_chr(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    unsigned row,
    uint8_t tile_id,
    uint32_t *chr_offset)
{
    uint8_t band;
    uint8_t pre_asl;
    uint8_t mmc3_bank;
    uint32_t offset;
    if (scene == NULL || context == NULL || chr_offset == NULL ||
        row >= TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES) {
        return false;
    }
    band = tecmo_gameplay_assets_live_band_for_scanline(
        (uint8_t)(row * 8U));
    pre_asl = tile_id < 0x80U ? context->pre_asl_r0[band]
                              : context->pre_asl_r1[band];
    mmc3_bank = (uint8_t)(pre_asl << 1U);
    offset = (uint32_t)mmc3_bank * 1024U +
             (uint32_t)(tile_id & 0x7FU) * 16U;
    if (offset > scene->assets.chr_storage_size ||
        scene->assets.chr_storage_size - offset < 16U) {
        return false;
    }
    *chr_offset = offset;
    return true;
}

static bool scene_hud_put_tile(TecmoGameplayPreparedHud *prepared,
                               unsigned row, unsigned column,
                               uint8_t tile)
{
    if (prepared == NULL ||
        row >= TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT ||
        column >= TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        prepared->occupied[row][column]) {
        return false;
    }
    prepared->occupied[row][column] = true;
    prepared->tiles[row][column] = tile;
    return true;
}

static bool scene_hud_put_font_character(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned row, unsigned column, unsigned char character)
{
    const TecmoStartGameMenuCell *font;
    size_t font_index;
    uint8_t tile;
    if (scene == NULL || prepared == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        character < TECMO_GAMEPLAY_HUD_FONT_FIRST ||
        character >= TECMO_GAMEPLAY_HUD_FONT_FIRST +
                         TECMO_GAMEPLAY_HUD_FONT_COUNT) {
        return false;
    }
    font_index = character - TECMO_GAMEPLAY_HUD_FONT_FIRST;
    tile = scene->hud_assets.font_tiles[font_index];
    font = &scene->pretip_team_data->font[font_index];
    if (font->tile_id != tile ||
        font->chr_offset > scene->assets.chr_storage_size ||
        scene->assets.chr_storage_size - font->chr_offset < 16U ||
        !scene_hud_put_tile(prepared, row, column, tile)) {
        return false;
    }
    prepared->chr_offsets[row][column] = font->chr_offset;
    prepared->chr_resolved[row][column] = true;
    return true;
}

static bool scene_hud_put_decimal(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned row, unsigned column, unsigned width, unsigned value)
{
    unsigned digit_index;
    if (scene == NULL || prepared == NULL || width == 0U ||
        column > TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        width > TECMO_GAMEPLAY_HUD_COLUMN_COUNT - column) {
        return false;
    }
    for (digit_index = 0U; digit_index < width; ++digit_index) {
        unsigned destination = column + width - digit_index - 1U;
        unsigned char character =
            (unsigned char)('0' + (value % 10U));
        if (!scene_hud_put_font_character(
                scene, prepared, row, destination, character)) {
            return false;
        }
        value /= 10U;
    }
    return value == 0U;
}

static bool scene_hud_put_bcd(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned row, unsigned column, uint8_t value)
{
    unsigned high = value >> 4U;
    unsigned low = value & 0x0FU;
    return high <= 9U && low <= 9U &&
           scene_hud_put_font_character(
               scene, prepared, row, column,
               (unsigned char)('0' + high)) &&
           scene_hud_put_font_character(
               scene, prepared, row, column + 1U,
               (unsigned char)('0' + low));
}

static bool scene_hud_actor_valid_for_team(
    const TecmoGameplayScene *scene, uint8_t actor_index,
    TecmoGameplayTeam team)
{
    return scene != NULL && team < TECMO_GAMEPLAY_TEAM_COUNT &&
           actor_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           scene->actors[actor_index].active &&
           scene->actors[actor_index].team == (uint8_t)team &&
           scene->actors[actor_index].roster_index <
               TECMO_TEAM_DATA_PLAYERS_PER_TEAM;
}

static bool scene_hud_selected_actor(const TecmoGameplayScene *scene,
                                     TecmoGameplayTeam team,
                                     uint8_t *actor_out)
{
    uint8_t reference = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t candidate;
    size_t controller;
    if (scene == NULL || actor_out == NULL ||
        team >= TECMO_GAMEPLAY_TEAM_COUNT) {
        return false;
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] == (uint8_t)team) {
            candidate = scene->controlled_actor[controller];
            if (!scene_hud_actor_valid_for_team(scene, candidate, team)) {
                return false;
            }
            *actor_out = candidate;
            return true;
        }
    }

    if (scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene->actors[scene->ball_holder].active) {
        reference = scene->ball_holder;
    } else if (scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
               scene->actors[scene->shot_actor].active) {
        reference = scene->shot_actor;
    }
    if (reference < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        candidate = (uint8_t)(
            (uint8_t)team * TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT +
            reference % TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
    } else {
        candidate = (uint8_t)(
            (uint8_t)team * TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
    }
    if (!scene_hud_actor_valid_for_team(scene, candidate, team)) {
        return false;
    }
    *actor_out = candidate;
    return true;
}

static bool scene_hud_put_player_name(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned column, const char name[21])
{
    size_t length = 0U;
    size_t separator = SIZE_MAX;
    size_t index;
    if (scene == NULL || prepared == NULL || name == NULL ||
        scene->hud_assets.font_tiles == NULL) {
        return false;
    }
    while (length < 21U && name[length] != '\0') ++length;
    if (length == 0U || length == 21U) return false;
    for (index = 0U; index < length; ++index) {
        if (name[index] == ' ') {
            separator = index;
            break;
        }
    }
    if (separator == SIZE_MAX || separator + 1U >= length ||
        !scene_hud_put_font_character(
            scene, prepared, 1U, column, (unsigned char)name[0]) ||
        !scene_hud_put_font_character(
            scene, prepared, 1U, column + 1U, '.')) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SURNAME_WIDTH; ++index) {
        size_t source = separator + 1U + index;
        unsigned char character;
        if (source < length) {
            character = (unsigned char)name[source];
            if (character < TECMO_GAMEPLAY_HUD_FONT_FIRST ||
                character >= TECMO_GAMEPLAY_HUD_FONT_FIRST +
                                 TECMO_GAMEPLAY_HUD_FONT_COUNT) {
                return false;
            }
        } else {
            character = ' ';
        }
        /* Bank02 writes every in-range table value verbatim. The shared
           TTDT font binding preserves even unused zero-valued punctuation. */
        if (!scene_hud_put_font_character(
                scene, prepared, 1U,
                column + 2U + (unsigned)index, character)) {
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_scene_render_prepare_live_hud(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayPreparedHud *prepared_out)
{
    TecmoGameplayPreparedHud prepared;
    uint8_t selected[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t team_ids[TECMO_GAMEPLAY_TEAM_COUNT];
    const TecmoTeamDataPlayer *selected_players[TECMO_GAMEPLAY_TEAM_COUNT];
    size_t team;
    unsigned row;
    unsigned column;
    if (scene == NULL || context == NULL || prepared_out == NULL ||
        !scene->hud_assets.available ||
        scene->hud_assets.team_label_tiles == NULL ||
        scene->hud_assets.font_tiles == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        scene->launch.away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        scene->launch.home_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    if (!tecmo_gameplay_scene_in_pretip(scene) &&
        !scene_ownership_valid(scene)) {
        return false;
    }
    team_ids[TECMO_GAMEPLAY_TEAM_AWAY] = scene->launch.away_team;
    team_ids[TECMO_GAMEPLAY_TEAM_HOME] = scene->launch.home_team;
    memset(&prepared, 0, sizeof(prepared));

    for (team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT; ++team) {
        uint8_t ppu_low = scene->hud_assets.team_ppu_low[team];
        unsigned absolute_row = ppu_low >> 5U;
        unsigned team_column = ppu_low & 0x1FU;
        size_t tile_index;
        if (absolute_row != TECMO_GAMEPLAY_HUD_PRIMARY_ROW ||
            team_column > TECMO_GAMEPLAY_HUD_COLUMN_COUNT -
                              TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH ||
            !scene_hud_selected_actor(
                scene, (TecmoGameplayTeam)team, &selected[team])) {
            return false;
        }
        selected_players[team] =
            &scene->pretip_team_data->players[team_ids[team]]
                [scene->actors[selected[team]].roster_index];
        for (tile_index = 0U;
             tile_index < TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH;
             ++tile_index) {
            if (!scene_hud_put_tile(
                    &prepared, 0U,
                    team_column + (unsigned)tile_index,
                    scene->hud_assets.team_label_tiles[team_ids[team]]
                                                       [tile_index])) {
                return false;
            }
        }
    }
    if (!scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_AWAY_SCORE_COLUMN,
            TECMO_GAMEPLAY_HUD_SCORE_WIDTH,
            scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] > 999U
                ? 999U
                : scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY]) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN,
            TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH,
            scene->state.clock_minutes > 99U
                ? 99U : scene->state.clock_minutes) ||
        !scene_hud_put_tile(
            &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN +
                TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH,
            TECMO_GAMEPLAY_HUD_COLON_TILE) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN +
                TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH + 1U,
            TECMO_GAMEPLAY_HUD_CLOCK_SECOND_WIDTH,
            scene->state.clock_seconds) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_HOME_SCORE_COLUMN,
            TECMO_GAMEPLAY_HUD_SCORE_WIDTH,
            scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] > 999U
                ? 999U
                : scene->state.score[TECMO_GAMEPLAY_TEAM_HOME]) ||
        !scene_hud_put_bcd(
            scene, &prepared, 1U,
            TECMO_GAMEPLAY_HUD_AWAY_NUMBER_COLUMN,
            selected_players[TECMO_GAMEPLAY_TEAM_AWAY]->attributes[1U]) ||
        !scene_hud_put_bcd(
            scene, &prepared, 1U,
            TECMO_GAMEPLAY_HUD_HOME_NUMBER_COLUMN,
            selected_players[TECMO_GAMEPLAY_TEAM_HOME]->attributes[1U]) ||
        !scene_hud_put_player_name(
            scene, &prepared,
            TECMO_GAMEPLAY_HUD_AWAY_PLAYER_COLUMN,
            selected_players[TECMO_GAMEPLAY_TEAM_AWAY]->name) ||
        !scene_hud_put_player_name(
            scene, &prepared,
            TECMO_GAMEPLAY_HUD_HOME_PLAYER_COLUMN,
            selected_players[TECMO_GAMEPLAY_TEAM_HOME]->name)) {
        return false;
    }
    /* The full-court nametable carries stale setup text in cells that the
       native HUD writer replaces. Own both fixed rows completely so only the
       team marks, scores, game clock, jersey numbers, and selected names
       survive; in particular, no setup-period text can leak between names. */
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (!prepared.occupied[row][column] &&
                !scene_hud_put_font_character(
                    scene, &prepared, row, column, ' ')) {
                return false;
            }
        }
    }
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (prepared.occupied[row][column] &&
                !prepared.chr_resolved[row][column] &&
                !scene_background_tile_chr(
                    scene, context,
                    TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row,
                    prepared.tiles[row][column],
                    &prepared.chr_offsets[row][column])) {
                return false;
            }
        }
    }
    *prepared_out = prepared;
    return true;
}

static void scene_draw_live_hud(
    const TecmoGameplayScene *scene, TecmoFramebuffer *view,
    const TecmoGameplayPreparedHud *prepared, int scale,
    const uint8_t live_palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE])
{
    uint32_t palette[4];
    uint32_t backing;
    unsigned row;
    unsigned column;
    size_t color;
    palette[0] = tecmo_nes_2c02_rgba(live_palette[0]);
    backing = tecmo_nes_2c02_rgba(live_palette[1]);
    for (color = 1U; color < 4U; ++color) {
        palette[color] = tecmo_nes_2c02_rgba(live_palette[color]);
    }
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (!prepared->occupied[row][column]) continue;
            /* The captured live nametable presents every dynamic HUD cell on
               the palette's black backing. Clear the replaced cell before
               drawing because the shared CHR helper treats color zero as
               transparent for sprite composition. */
            scene_fill_rect(
                view, (int)column * 8 * scale,
                (int)(TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row) * 8 * scale,
                8 * scale, 8 * scale, backing);
            tecmo_draw_chr_tile_at_offset_ex(
                view, scene->assets.chr_storage,
                scene->assets.chr_storage_size,
                prepared->chr_offsets[row][column],
                (int)column * 8 * scale,
                (int)(TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row) * 8 * scale,
                scale, palette, false, false);
        }
    }
}

static bool scene_framebuffer_subview(
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    TecmoFramebuffer *view)
{
    if (!scene_framebuffer_valid(framebuffer, origin_x, origin_y, scale) ||
        view == NULL) {
        return false;
    }
    view->pixels = framebuffer->pixels +
        (size_t)origin_y * (size_t)framebuffer->pitch_pixels +
        (size_t)origin_x;
    view->width = TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale;
    view->height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static bool scene_actor_palette_binding(const TecmoGameplayScene *scene,
                                        size_t actor_index,
                                        uint8_t *palette_group_out,
                                        uint8_t *uniform_color_out)
{
    uint8_t uniform_colors[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t team_id;
    uint8_t palette_group;
    uint8_t uniform_color;
    const TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    if (scene == NULL || palette_group_out == NULL ||
        uniform_color_out == NULL ||
        scene->pretip_team_data == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_team_data_resolve_gameplay_uniform_colors(
            scene->pretip_team_data, scene->launch.away_team,
            scene->launch.home_team, uniform_colors)) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (actor->team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        actor->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return false;
    }
    team_id = actor->team == TECMO_GAMEPLAY_TEAM_AWAY
                  ? scene->launch.away_team : scene->launch.home_team;
    if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT) return false;
    player = &scene->pretip_team_data->players[team_id][actor->roster_index];
    /* Bank02 $A8AE-$A8C9 rotates profile byte 2 bit 7 into $04B0 bit 0.
       Bank01 $B0ED then selects $B138/$B148 with that exact bit. */
    palette_group = (uint8_t)((player->profile[2U] & 0x80U) >> 7U);
    uniform_color = uniform_colors[actor->team];
    if (palette_group >= TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT ||
        uniform_color > 0x3FU) {
        return false;
    }
    *palette_group_out = palette_group;
    *uniform_color_out = uniform_color;
    return true;
}

static bool scene_build_matchup_live_palette(
    const TecmoGameplayScene *scene,
    uint8_t palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE])
{
    static const uint8_t fixed_live_palette[
        TECMO_GAMEPLAY_COURT_PALETTE_SIZE] = {
            0x16U,0x0FU,0x27U,0x30U,
            0x16U,0x0FU,0x17U,0x30U,
            0x16U,0x0FU,0x27U,0x12U,
            0x16U,0x0FU,0x17U,0x12U
    };
    uint8_t uniform_colors[TECMO_GAMEPLAY_TEAM_COUNT];
    if (scene == NULL || palette == NULL ||
        scene->court.palette == NULL ||
        memcmp(scene->court.palette, fixed_live_palette,
               sizeof(fixed_live_palette)) != 0 ||
        !tecmo_team_data_resolve_gameplay_uniform_colors(
            scene->pretip_team_data, scene->launch.away_team,
            scene->launch.home_team, uniform_colors)) {
        return false;
    }
    memcpy(palette, fixed_live_palette, sizeof(fixed_live_palette));
    /* Fixed $F2E2-$F2F1 is four profile/side palettes. Fixed
       $DEAB-$DEDF supplies the selected matchup colors for their fourth
       entries; $04B0 & 3 selects one of these four groups per actor. */
    palette[3U] = uniform_colors[TECMO_GAMEPLAY_TEAM_AWAY];
    palette[7U] = uniform_colors[TECMO_GAMEPLAY_TEAM_AWAY];
    palette[11U] = uniform_colors[TECMO_GAMEPLAY_TEAM_HOME];
    palette[15U] = uniform_colors[TECMO_GAMEPLAY_TEAM_HOME];
    return true;
}

static bool scene_apply_matchup_live_palette(
    const TecmoGameplayScene *scene,
    TecmoGameplayResolvedPose *pose)
{
    uint8_t palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE];
    size_t piece;
    if (pose == NULL ||
        !scene_build_matchup_live_palette(scene, palette)) {
        return false;
    }
    memcpy(pose->palette, palette, sizeof(pose->palette));
    for (piece = 0U; piece < pose->piece_count; ++piece) {
        uint8_t palette_index = pose->pieces[piece].palette_index;
        if (palette_index >= 4U) return false;
        memcpy(pose->pieces[piece].palette,
               palette + (size_t)palette_index * 4U,
               sizeof(pose->pieces[piece].palette));
    }
    return true;
}

bool tecmo_gameplay_scene_render_resolve_pose(const TecmoGameplayScene *scene,
                               uint16_t pointer_index,
                               uint8_t actor_slot_base,
                               uint8_t actor_attributes,
                               uint8_t palette_group,
                               bool apply_uniform_color,
                               uint8_t uniform_color,
                               TecmoGameplayResolvedPose *pose)
{
    TecmoGameplayPoseContext context;
    TecmoGameplayResolvedPose first;
    memset(&context, 0, sizeof(context));
    context.actor_slot_base = actor_slot_base;
    context.actor_attributes = actor_attributes;
    context.palette_group = palette_group;
    context.uniform_color = uniform_color;
    context.apply_uniform_color = apply_uniform_color;
    context.mmc3_r2_r5[0] = 0x40U;
    context.mmc3_r2_r5[1] = 0x41U;
    context.mmc3_r2_r5[2] = 0x42U;
    context.mmc3_r2_r5[3] = 0x43U;
    if (!tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                            &context, &first)) {
        return false;
    }
    context.mmc3_r2_r5[(actor_slot_base >> 6U) & 0x03U] =
        first.record_tag;
    return tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                              &context, pose);
}

bool tecmo_gameplay_scene_render_resolve_actor_pose(const TecmoGameplayScene *scene,
                                     size_t actor_index,
                                     TecmoGameplayResolvedPose *pose)
{
    uint8_t palette_group;
    uint8_t uniform_color;
    uint8_t actor_attributes;
    const TecmoGameplaySceneActor *actor;
    TecmoGameplayResolvedPose resolved;
    if (scene == NULL || pose == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (actor->team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        !scene_actor_palette_binding(scene, actor_index, &palette_group,
                                     &uniform_color)) {
        return false;
    }
    actor_attributes = (uint8_t)(palette_group | (actor->team << 1U));
    if (!tecmo_gameplay_scene_render_resolve_pose(scene, actor->pose_index,
                            actor->sprite_slot_base,
                            actor_attributes, palette_group, true,
                            uniform_color, &resolved) ||
        !scene_apply_matchup_live_palette(scene, &resolved)) {
        return false;
    }
    *pose = resolved;
    return true;
}

bool tecmo_gameplay_scene_in_dunk_presentation(
    const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->lifecycle_tag == TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG &&
           scene->active &&
           scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK &&
           scene->shot_frame >= TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
           scene->shot_frame < TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME;
}

static void scene_fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                            int width, int height, uint32_t color)
{
    int row;
    int column;
    for (row = 0; row < height; ++row) {
        uint32_t *pixels = framebuffer->pixels +
            (size_t)(y + row) * (size_t)framebuffer->pitch_pixels +
            (size_t)x;
        for (column = 0; column < width; ++column) {
            pixels[column] = color;
        }
    }
}

static bool scene_draw_dunk_presentation(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale)
{
    uint8_t stage;
    uint8_t side;
    uint8_t palette_group;
    uint8_t uniform_color;
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->shot_actor].team >=
            TECMO_GAMEPLAY_DUNK_SIDE_COUNT ||
        !scene_actor_palette_binding(scene, scene->shot_actor,
                                     &palette_group, &uniform_color)) {
        return false;
    }
    side = scene->actors[scene->shot_actor].team;
    if (scene->shot_frame >= TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME &&
        scene->shot_frame <= TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME) {
        return tecmo_gameplay_dunk_cutaway_stage_for_frame(
                   &scene->dunk_cutaway, scene->shot_frame, &stage) &&
               tecmo_gameplay_dunk_cutaway_draw(
                   &scene->dunk_cutaway, scene->assets.chr_storage,
                   scene->assets.chr_storage_size, framebuffer,
                   origin_x, origin_y, scale, side, palette_group,
                   uniform_color, stage);
    }
    scene_fill_rect(framebuffer, origin_x, origin_y,
                    TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale,
                    TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale,
                    tecmo_nes_2c02_rgba(0x0FU));
    return true;
}

static void scene_draw_pose(const TecmoGameplayScene *scene,
                            TecmoFramebuffer *framebuffer,
                            const TecmoGameplayResolvedPose *pose,
                            int base_x, int base_y,
                            int origin_x, int origin_y, int scale,
                            bool mirror_horizontal)
{
    size_t piece_index;
    for (piece_index = 0U; piece_index < pose->piece_count; ++piece_index) {
        const TecmoGameplayResolvedPiece *piece = &pose->pieces[piece_index];
        uint32_t palette[4] = {0U, 0U, 0U, 0U};
        size_t color;
        int piece_x = mirror_horizontal ? -piece->dx - 8 : piece->dx;
        bool flip_horizontal = piece->flip_horizontal ^ mirror_horizontal;
        int x = origin_x + (base_x + piece_x) * scale;
        int y = origin_y + (base_y + piece->dy) * scale;
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(piece->palette[color]);
        }
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, scene->assets.chr_storage,
            scene->assets.chr_storage_size, piece->top_chr_offset,
            x, y, scale, palette, flip_horizontal, false);
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, scene->assets.chr_storage,
            scene->assets.chr_storage_size, piece->bottom_chr_offset,
            x, y + 8 * scale, scale, palette,
            flip_horizontal, false);
    }
}

static void scene_make_bg_palette(uint32_t rgba[4],
                                  const uint8_t palette[16],
                                  uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    size_t color;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (color = 1U; color < 4U; ++color)
        rgba[color] = tecmo_nes_2c02_rgba(palette[base + color]);
}

static void scene_make_sprite_palette(uint32_t rgba[4],
                                      const uint8_t palette[16],
                                      uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    size_t color;
    rgba[0] = 0U;
    for (color = 1U; color < 4U; ++color)
        rgba[color] = tecmo_nes_2c02_rgba(palette[base + color]);
}

static bool scene_draw_pretip_cell(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    const TecmoStartGameMenuCell *cell,
    const uint8_t palette[16],
    int palette_override,
    int x,
    int y,
    int scale)
{
    uint32_t rgba[4];
    uint8_t index;
    if (scene == NULL || view == NULL || cell == NULL || palette == NULL ||
        scale <= 0 || cell->chr_offset > scene->assets.chr_storage_size ||
        scene->assets.chr_storage_size - cell->chr_offset < 16U) {
        return false;
    }
    index = palette_override >= 0
                ? (uint8_t)palette_override : cell->palette_index;
    if (index > 3U) return false;
    scene_make_bg_palette(rgba, palette, index);
    tecmo_draw_chr_tile_at_offset_ex(
        view, scene->assets.chr_storage, scene->assets.chr_storage_size,
        cell->chr_offset, x * scale, y * scale, scale, rgba, false, false);
    return true;
}

static bool scene_draw_pretip_text(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    const char *text,
    int x,
    int y,
    int scale,
    const uint8_t palette[16],
    int palette_index)
{
    size_t index;
    size_t length;
    if (text == NULL) return false;
    length = strlen(text);
    if (length > 16U || x < 0 || y < 0 ||
        x + (int)length * 16 > TECMO_GAMEPLAY_SCENE_NES_WIDTH ||
        y + 16 > TECMO_GAMEPLAY_SCENE_NES_HEIGHT) {
        return false;
    }
    for (index = 0U; text[index] != '\0'; ++index) {
        unsigned c = (unsigned char)text[index];
        uint8_t glyph;
        size_t tile_index;
        if (c == '.' || c == ' ') {
            glyph = 0x18U;
        } else if (c == '-') {
            glyph = 0x25U;
        } else if (c >= 0x17U && c < ':') {
            glyph = (uint8_t)(c - 0x17U);
        } else if (c >= 'A' && c <= 'Z') {
            size_t map_offset = c - 0x1DU;
            glyph = scene->pretip_assets.character_map[map_offset];
        } else {
            return false;
        }
        if (glyph >= TECMO_GAMEPLAY_PRETIP_GLYPH_COUNT) return false;
        for (tile_index = 0U;
             tile_index < TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT;
             ++tile_index) {
            uint8_t tile = scene->pretip_assets.character_tiles[
                (size_t)glyph * TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT +
                tile_index];
            uint8_t selector =
                scene->pretip_assets.card_chr_selector[
                    tile < 0x80U ? 0U : 1U];
            uint32_t chr_offset =
                (uint32_t)selector * 1024U +
                (uint32_t)(tile & 0x7FU) * 16U;
            uint32_t rgba[4];
            if (chr_offset + 16U > scene->assets.chr_storage_size ||
                palette_index < 0 || palette_index > 3) {
                return false;
            }
            scene_make_bg_palette(rgba, palette, (uint8_t)palette_index);
            tecmo_draw_chr_tile_at_offset_ex(
                view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, chr_offset,
                (x + (int)index * 16 +
                 (int)(tile_index % 2U) * 8) * scale,
                (y + (int)(tile_index / 2U) * 8) * scale,
                scale, rgba, false, false);
        }
    }
    return true;
}

static bool scene_validate_pretip_team(
    const TecmoGameplayScene *scene,
    uint8_t team_id,
    int logo_x,
    int logo_y,
    int scale)
{
    const TecmoTeamDataTeam *team;
    size_t index;
    if (scene == NULL || scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        team_id >= TECMO_TEAM_DATA_REAL_TEAM_COUNT || scale <= 0 ||
        logo_x < 0 || logo_y < 0) {
        return false;
    }
    team = &scene->pretip_team_data->teams[team_id];
    if (team->logo_width == 0U || team->logo_height == 0U ||
        team->logo_count == 0U ||
        (size_t)team->logo_width * team->logo_height != team->logo_count ||
        team->logo_count > TECMO_TEAM_DATA_LOGO_CELL_LIMIT ||
        team->profile_palette_group >= TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT ||
        (int)team->logo_width * 8 >
            TECMO_GAMEPLAY_SCENE_NES_WIDTH - logo_x ||
        (int)team->logo_height * 8 >
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT - logo_y) {
        return false;
    }
    for (index = 0U; index < team->logo_count; ++index) {
        const TecmoStartGameMenuCell *cell =
            &scene->pretip_team_data->logos[team_id][index];
        if (cell->chr_offset > scene->assets.chr_storage_size ||
            scene->assets.chr_storage_size - cell->chr_offset < 16U ||
            cell->palette_index > 3U) {
            return false;
        }
    }
    return true;
}

static bool scene_draw_pretip_team(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    uint8_t team_id,
    int logo_x,
    int logo_y,
    int scale,
    bool dim)
{
    const TecmoTeamDataTeam *team;
    uint8_t logo_palette[16];
    size_t index;
    if (!scene_validate_pretip_team(
            scene, team_id, logo_x, logo_y, scale)) {
        return false;
    }
    team = &scene->pretip_team_data->teams[team_id];
    memcpy(logo_palette,
           scene->pretip_team_data->profile_palettes[
               team->profile_palette_group],
           sizeof(logo_palette));
    logo_palette[0] = 0x0FU;
    if (dim) {
        for (index = 1U; index < sizeof(logo_palette); ++index) {
            logo_palette[index] = logo_palette[index] >= 0x10U
                                      ? (uint8_t)(logo_palette[index] - 0x10U)
                                      : 0x0FU;
        }
    }
    for (index = 0U; index < team->logo_count; ++index) {
        int column = (int)(index % team->logo_width);
        int row = (int)(index / team->logo_width);
        if (!scene_draw_pretip_cell(
                scene, view, &scene->pretip_team_data->logos[team_id][index],
                logo_palette, -1, logo_x + column * 8,
                logo_y + row * 8, scale)) {
            return false;
        }
    }
    return true;
}

static bool scene_draw_pretip_template(const TecmoGameplayScene *scene,
                                       TecmoFramebuffer *view,
                                       int scale)
{
    (void)scene;
    (void)scale;
    /*
     * Screen $15 is the ROM's blank dynamic-card nametable. Its visible
     * backdrop is universal black; all card lettering is subsequently written
     * by Bank06 $A125 rather than baked into the decoded nametable.
     */
    scene_fill_rect(view, 0, 0, view->width, view->height,
                    tecmo_nes_2c02_rgba(0x0FU));
    return true;
}

static uint8_t scene_pretip_closeup_motion(uint16_t phase_frame)
{
    uint16_t motion =
        phase_frame > 33U ? (uint16_t)((phase_frame - 33U) / 2U) : 0U;
    return motion < 25U ? (uint8_t)motion : 25U;
}

static bool scene_draw_pretip_closeup(const TecmoGameplayScene *scene,
                                      TecmoFramebuffer *view,
                                      int scale,
                                      uint16_t phase_frame)
{
    size_t index;
    uint8_t motion = scene_pretip_closeup_motion(phase_frame);
    if (scene->pretip_closeup == NULL ||
        !scene->pretip_closeup->available) return false;
    scene_fill_rect(view, 0, 0, view->width, view->height,
                    tecmo_nes_2c02_rgba(
                        scene->pretip_closeup->background_palette[0]));
    for (index = 0U; index < 960U; ++index) {
        const TecmoIntroWarriorsTile *cell =
            &scene->pretip_closeup->pages[0][index];
        uint32_t rgba[4];
        scene_make_bg_palette(
            rgba, scene->pretip_closeup->background_palette,
            cell->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, cell->moving_chr_offset,
            ((int)(index % 32U) * 8 + motion) * scale,
            (int)(index / 32U) * 8 * scale,
            scale, rgba, false, false);
    }
    scene_fill_rect(view, 0, 42 * scale, view->width, 4 * scale,
                    tecmo_nes_2c02_rgba(0x30U));
    scene_fill_rect(view, 0, 162 * scale, view->width, 4 * scale,
                    tecmo_nes_2c02_rgba(0x30U));
    for (index = 0U; index < TECMO_INTRO_WARRIORS_PIECE_COUNT; ++index) {
        const TecmoIntroWarriorsPiece *piece =
            &scene->pretip_closeup->pieces[index];
        uint32_t rgba[4];
        bool flip_x = (piece->flags & 1U) != 0U;
        bool flip_y = (piece->flags & 2U) != 0U;
        uint32_t top = flip_y ? piece->bottom_chr_offset
                              : piece->top_chr_offset;
        uint32_t bottom = flip_y ? piece->top_chr_offset
                                 : piece->bottom_chr_offset;
        int x = (98 - motion + piece->dx) * scale;
        /* D861 writes OAM Y; NES hardware displays the sprite one scanline
           below that stored coordinate. */
        int y = (93 + piece->dy) * scale;
        scene_make_sprite_palette(
            rgba, scene->pretip_closeup->sprite_palette,
            piece->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, top,
            x, y, scale, rgba, flip_x, flip_y);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, bottom,
            x, y + 8 * scale, scale, rgba, flip_x, flip_y);
    }
    return true;
}

static int scene_centered_text_x(const char *text)
{
    size_t length = text != NULL ? strlen(text) : 0U;
    if (length > 16U) return -1;
    return (int)((16U - length) / 2U) * 16;
}

static void scene_make_pretip_card_palette(const TecmoGameplayScene *scene,
                                           uint8_t palette[16],
                                           bool dim)
{
    size_t index;
    memcpy(palette, scene->pretip_assets.palette, 16U);
    palette[0] = 0x0FU;
    if (!dim) return;
    for (index = 1U; index < 16U; ++index) {
        palette[index] = palette[index] >= 0x10U
                             ? (uint8_t)(palette[index] - 0x10U)
                             : 0x0FU;
    }
}

static bool scene_draw_pretip_cards(const TecmoGameplayScene *scene,
                                    TecmoFramebuffer *framebuffer,
                                    int origin_x,
                                    int origin_y,
                                    int scale)
{
    TecmoFramebuffer view;
    TecmoGameplayPreTipPhase phase = scene->pretip_state.phase;
    uint16_t phase_frame = scene->pretip_state.phase_frame;
    uint8_t palette[16];
    bool dim;
    const TecmoTeamDataTeam *away;
    const TecmoTeamDataTeam *home;
    const char *mode_text;
    if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view)) {
        return false;
    }
    if (phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
        if (phase_frame < 28U ||
            phase_frame + 30U >= scene->pretip_assets.phase_frames[phase]) {
            scene_fill_rect(&view, 0, 0, view.width, view.height,
                            tecmo_nes_2c02_rgba(0x0FU));
            return true;
        }
        return scene_draw_pretip_closeup(
            scene, &view, scale, phase_frame);
    }
    if (phase == TECMO_GAMEPLAY_PRETIP_MATCHUP &&
        (!scene_validate_pretip_team(
             scene, scene->launch.away_team, 16, 32, scale) ||
         !scene_validate_pretip_team(
             scene, scene->launch.home_team, 16, 128, scale))) {
        return false;
    }
    if (!scene_draw_pretip_template(scene, &view, scale)) return false;
    if (phase == TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
        phase_frame < 16U) {
        return true;
    }
    dim = (phase == TECMO_GAMEPLAY_PRETIP_MATCHUP && phase_frame < 30U) ||
          (phase == TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
           phase_frame < 29U);
    scene_make_pretip_card_palette(scene, palette, dim);
    if (phase == TECMO_GAMEPLAY_PRETIP_PRESEASON) {
        mode_text = scene->launch.source == TECMO_GAMEPLAY_SCENE_PRESEASON
                        ? "PRESEASON" : "REGULAR SEASON";
        return scene_draw_pretip_text(
            scene, &view, mode_text, scene_centered_text_x(mode_text), 112,
            scale, palette, 2);
    }
    if (phase == TECMO_GAMEPLAY_PRETIP_MATCHUP) {
        away = &scene->pretip_team_data->teams[scene->launch.away_team];
        home = &scene->pretip_team_data->teams[scene->launch.home_team];
        return scene_draw_pretip_team(
                   scene, &view, scene->launch.away_team,
                   16, 32, scale, dim) &&
               scene_draw_pretip_text(
                   scene, &view, away->city,
                   scene_centered_text_x(away->city), 80, scale, palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, away->nickname,
                   scene_centered_text_x(away->nickname), 96, scale,
                   palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, "VS", scene_centered_text_x("VS"), 144,
                   scale, palette, 2) &&
               scene_draw_pretip_team(
                   scene, &view, scene->launch.home_team,
                   16, 128, scale, dim) &&
               scene_draw_pretip_text(
                   scene, &view, home->city,
                   scene_centered_text_x(home->city), 176, scale,
                   palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, home->nickname,
                   scene_centered_text_x(home->nickname), 192, scale,
                   palette, 2);
    }
    return scene_draw_pretip_text(
        scene, &view, "1ST PERIOD", scene_centered_text_x("1ST PERIOD"), 112,
        scale, palette, 2);
}

static bool scene_draw_pretip_descriptor_screen(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint8_t screen_index,
    uint8_t nametable_page)
{
    TecmoFramebuffer view;
    const TecmoGameplayScreenAsset *screen;
    unsigned row;
    unsigned column;
    if (screen_index >= TECMO_GAMEPLAY_ASSET_SCREEN_COUNT ||
        !scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view)) {
        return false;
    }
    screen = &scene->assets.screens[screen_index];
    scene_fill_rect(&view, 0, 0, view.width, view.height,
                    tecmo_nes_2c02_rgba(screen->palette[0]));
    for (row = 0U; row < 30U; ++row) {
        for (column = 0U; column < 32U; ++column) {
            TecmoGameplayResolvedOrientationTile tile;
            uint32_t rgba[4];
            size_t color;
            if (!tecmo_gameplay_assets_resolve_descriptor_tile(
                    &scene->assets, screen_index, nametable_page,
                    (uint8_t)row, (uint8_t)column, &tile)) {
                return false;
            }
            rgba[0] = tecmo_nes_2c02_rgba(screen->palette[0]);
            for (color = 1U; color < 4U; ++color)
                rgba[color] = tecmo_nes_2c02_rgba(tile.palette[color]);
            tecmo_draw_chr_tile_at_offset_ex(
                &view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, tile.chr_offset,
                (int)column * 8 * scale, (int)row * 8 * scale,
                scale, rgba, false, false);
        }
    }
    return true;
}

bool tecmo_gameplay_scene_draw(const TecmoGameplayScene *scene,
                               TecmoFramebuffer *framebuffer,
                               int origin_x,
                               int origin_y,
                               int scale,
                               bool include_actors)
{
    TecmoGameplayLiveBackgroundContext background_context;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoFramebuffer view;
    TecmoGameplayPreparedHud prepared_hud;
    TecmoGameplayResolvedPose actor_poses[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayResolvedPose ball_pose;
    uint8_t live_palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE];
    uint8_t order[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    bool draw_live_hud;
    unsigned row;
    unsigned column;
    size_t actor;
    size_t left;

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state) ||
        !scene_framebuffer_valid(framebuffer, origin_x, origin_y, scale)) {
        return false;
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        scene->pretip_state.phase <= TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
        return scene_draw_pretip_cards(
            scene, framebuffer, origin_x, origin_y, scale);
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        (scene->pretip_state.phase ==
             TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
         (scene->pretip_state.phase ==
              TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP &&
          scene->pretip_state.phase_frame < 30U))) {
        if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                       scale, &view)) {
            return false;
        }
        scene_fill_rect(&view, 0, 0, view.width, view.height,
                        tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
        /* TGPL screen $1B page 1 is the ROM phase with ball X 176..239
           and hands X 67..159; page 0 is the preceding/opposite phase. */
        if (scene->assets.screens[0].screen_id != 0x1BU) return false;
        return scene_draw_pretip_descriptor_screen(
            scene, framebuffer, origin_x, origin_y, scale, 0U, 1U);
    }
    if (tecmo_gameplay_scene_in_dunk_presentation(scene)) {
        return scene_draw_dunk_presentation(
            scene, framebuffer, origin_x, origin_y, scale);
    }
    if (scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        return tecmo_gameplay_violation_referee_draw(
            &scene->violation_referee_assets,
            scene->assets.chr_storage,
            scene->assets.chr_storage_size,
            framebuffer, origin_x, origin_y, scale,
            scene->state.violation, scene->state.phase_frame);
    }
    if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view) ||
        !scene_build_matchup_live_palette(scene, live_palette)) {
        return false;
    }
    memset(&court_frame, 0, sizeof(court_frame));
    if (include_actors && scene->active) {
        if (!tecmo_gameplay_scene_court_frame(scene, &court_frame)) {
            return false;
        }
    } else if (!tecmo_gameplay_scene_court_slice(
                   scene, &court_frame.slice)) {
        return false;
    }
    if (!tecmo_gameplay_scene_render_build_background_context(scene, &background_context) ||
        court_frame.slice.viewport.column_count < 32U ||
        court_frame.slice.viewport.column_count >
            TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE) {
        return false;
    }
    draw_live_hud = include_actors && scene->active &&
                    !tecmo_gameplay_scene_in_pretip(scene);
    for (row = 0U; row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
        for (column = 0U;
             column < court_frame.slice.viewport.column_count;
             ++column) {
            size_t cell =
                (size_t)row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE +
                column;
            uint32_t offset;
            if (court_frame.slice.viewport.palette_indices[cell] > 3U ||
                !scene_background_tile_chr(
                    scene, &background_context, row,
                    court_frame.slice.viewport.tiles[cell], &offset) ||
                offset + 16U >
                    scene->assets.chr_storage_size) {
                return false;
            }
        }
    }
    if (draw_live_hud &&
        !tecmo_gameplay_scene_render_prepare_live_hud(
            scene, &background_context, &prepared_hud)) {
        return false;
    }
    if (include_actors && scene->active) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            order[actor] = (uint8_t)actor;
            if (!tecmo_gameplay_scene_render_resolve_actor_pose(scene, actor,
                                          &actor_poses[actor])) {
                return false;
            }
        }
        if (!tecmo_gameplay_scene_render_resolve_pose(scene, TECMO_GAMEPLAY_BALL_POSE, 0xC1U,
                                0U, 0U, false, 0U, &ball_pose) ||
            !scene_apply_matchup_live_palette(scene, &ball_pose)) {
            return false;
        }
    }

    scene_fill_rect(&view, 0, 0, view.width, view.height,
                    tecmo_nes_2c02_rgba(live_palette[0]));
    for (row = 0U; row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
        for (column = 0U;
             column < court_frame.slice.viewport.column_count;
             ++column) {
            size_t cell =
                (size_t)row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE +
                column;
            uint32_t offset;
            uint8_t palette_index =
                court_frame.slice.viewport.palette_indices[cell];
            uint32_t palette[4];
            size_t color;
            (void)scene_background_tile_chr(
                scene, &background_context, row,
                court_frame.slice.viewport.tiles[cell], &offset);
            palette[0] = tecmo_nes_2c02_rgba(live_palette[0]);
            for (color = 1U; color < 4U; ++color) {
                palette[color] = tecmo_nes_2c02_rgba(
                    live_palette[(size_t)palette_index * 4U + color]);
            }
            tecmo_draw_chr_tile_at_offset_ex(
                &view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, offset,
                ((int)column * 8 -
                 (int)court_frame.slice.viewport.fine_scroll_x) * scale,
                (int)row * 8 * scale,
                scale, palette, false, false);
        }
    }
    if (draw_live_hud) {
        scene_draw_live_hud(
            scene, &view, &prepared_hud, scale, live_palette);
    }
    if (!include_actors || !scene->active) return true;

    for (left = 0U; left < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++left) {
        size_t right;
        for (right = left + 1U; right < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++right) {
            if (scene->actors[order[right]].position.y <
                scene->actors[order[left]].position.y) {
                uint8_t swap = order[left];
                order[left] = order[right];
                order[right] = swap;
            }
        }
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        uint8_t index = order[actor];
        if (!court_frame.projection.players[index].visible) continue;
        /* facing_right is the effective actor orientation: TGOR supplies the
           goal-derived baseline, while only deliberate horizontal movement
           and shot actions override it. Encoded tip/action poses retain their
           source orientation and therefore bypass this native mirror. */
        scene_draw_pose(scene, &view, &actor_poses[index],
                        court_frame.projection.players[index].screen_x,
                        court_frame.projection.players[index].screen_y,
                        0, 0, scale,
                        !scene->actors[index].pose_orientation_encoded &&
                        !scene->actors[index].facing_right);
    }
    if (court_frame.projection.ball.visible) {
        scene_draw_pose(scene, &view, &ball_pose,
                        court_frame.projection.ball.screen_x,
                        court_frame.projection.ball.screen_y,
                        0, 0, scale, false);
    }
    return true;
}
