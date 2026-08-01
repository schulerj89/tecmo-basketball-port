#include "tecmo_game.h"
#include "tecmo_intro_arena.h"
#include "tecmo_intro_finale.h"
#include "tecmo_intro_post_arena.h"
#include "tecmo_intro_screen.h"
#include "tecmo_intro_stage.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static bool print_intro_opening_status(
    const TecmoRuntime *runtime,
    const char *mode_name)
{
    const bool presents_renderable =
        runtime->intro_presents_asset.available &&
        tecmo_intro_screen_chr_available(&runtime->intro_presents_asset,
                                         runtime->title_chr_bytes,
                                         runtime->title_chr_byte_count);
    const bool license_renderable =
        runtime->intro_license_asset.available &&
        tecmo_intro_screen_chr_available(&runtime->intro_license_asset,
                                         runtime->title_chr_bytes,
                                         runtime->title_chr_byte_count);
    const bool license_mode = strcmp(mode_name, "intro-license") == 0 ||
                              strcmp(mode_name, "play-step7") == 0;
    const bool loose_trace_enabled =
        strcmp(runtime->intro_trace_status, "LOOSE INTRO TRACE DISABLED") != 0;
    const TecmoIntroScreenAsset *asset = license_mode
                                             ? &runtime->intro_license_asset
                                             : &runtime->intro_presents_asset;

    if (!(strcmp(mode_name, "intro-license") == 0 ||
          strcmp(mode_name, "play") == 0 ||
          strncmp(mode_name, "play-fade", 9) == 0 ||
          strcmp(mode_name, "play-step6") == 0 ||
          strcmp(mode_name, "play-step7") == 0 ||
          strcmp(mode_name, "title-screen") == 0 ||
          strcmp(mode_name, "boot-title") == 0)) {
        return false;
    }
    printf("intro-opening-render-source presents=%u license=%u chr=%u schema=TISC-1 loose_trace=%u\n",
           presents_renderable ? 1U : 0U,
           license_renderable ? 1U : 0U,
           runtime->title_chr_bytes != NULL ? 1U : 0U,
           loose_trace_enabled ? 1U : 0U);
    printf("intro-opening-state kind=%s frame=%u palette=%u duration=%u sprites=%u\n",
           license_mode ? "nba-license" : "tecmo-presents",
           runtime->mode_frame_counter,
           (unsigned)tecmo_intro_screen_palette_stage(
               asset, runtime->mode_frame_counter),
           (unsigned)asset->duration_frames,
           asset->sprite_count > 0U &&
                   runtime->mode_frame_counter >= asset->sprite_first_frame &&
                   runtime->mode_frame_counter < asset->sprite_hide_frame
               ? (unsigned)asset->sprite_count
               : 0U);
    return true;
}

static bool print_intro_arena_status(
    const TecmoRuntime *runtime,
    const char *mode_name,
    bool arena_rendered)
{
    const char *assetpack_marker = "assetpack entry ";
    const char *entry_start;
    char entry_id[64];
    bool native_layer_available;
    bool native_sprite_groups_available;
    size_t native_sprite_group_count;
    size_t jumbotron_piece_count;
    size_t goal_piece_count;
    TecmoIntroArenaTransitionState arena_state;
    TecmoArenaNativeSpriteVisibleCounts visible_counts = {0U, 0U};

    if (strncmp(mode_name, "intro-arena", 11) != 0) return false;
    entry_id[0] = '\0';
    entry_start = strstr(runtime->intro_arena_capture.status, assetpack_marker);
    if (entry_start != NULL) {
        const char *entry_end;
        size_t entry_length;
        entry_start += strlen(assetpack_marker);
        entry_end = strstr(entry_start, " pack ");
        entry_length = entry_end != NULL ? (size_t)(entry_end - entry_start) : 0U;
        if (entry_length > 0U && entry_length < sizeof(entry_id)) {
            memcpy(entry_id, entry_start, entry_length);
            entry_id[entry_length] = '\0';
        }
    }
    printf("intro-capture-status kind=arena available=%u nt=%u attr=%u pal=%u oam=%u\n",
           runtime->intro_arena_capture.available ? 1U : 0U,
           (unsigned)(runtime->intro_arena_capture.tile_count[0] +
                      runtime->intro_arena_capture.tile_count[1]),
           runtime->intro_arena_capture.available
               ? (unsigned)(TECMO_INTRO_ARENA_PAGE_COUNT * 64U)
               : 0U,
           (unsigned)runtime->intro_arena_capture.palette_stage_count,
           (unsigned)runtime->intro_arena_capture.sprite_count);
    printf("intro-capture-source kind=arena assetpack=%u entry=%s\n",
           entry_id[0] != '\0' ? 1U : 0U,
           entry_id[0] != '\0' ? entry_id : "none");
    native_layer_available = tecmo_intro_arena_tile_layer_chr_available(
        &runtime->intro_arena_tile_layer,
        runtime->title_chr_bytes,
        runtime->title_chr_byte_count);
    native_sprite_groups_available = tecmo_intro_arena_native_sprite_chr_available(
        &runtime->intro_arena_sprite_groups,
        runtime->title_chr_bytes,
        runtime->title_chr_byte_count);
    native_sprite_group_count = tecmo_intro_arena_native_sprite_group_count(
        &runtime->intro_arena_sprite_groups);
    jumbotron_piece_count = tecmo_intro_arena_native_sprite_piece_count(
        &runtime->intro_arena_sprite_groups,
        TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON);
    goal_piece_count = tecmo_intro_arena_native_sprite_piece_count(
        &runtime->intro_arena_sprite_groups,
        TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL);
    if (native_sprite_groups_available) {
        tecmo_intro_arena_transition_state(runtime->mode_frame_counter,
                                           &arena_state);
        visible_counts = tecmo_intro_arena_native_sprite_visible_counts(
            &runtime->intro_arena_sprite_groups, &arena_state);
    }
    printf("intro-arena-render-source kind=arena exact_layer=%u rendered=%u cells=%u palette=16 sprite_groups=%u jumbotron_pieces=%u goal_pieces=%u visible_jumbotron=%u visible_goal=%u\n",
           native_layer_available ? 1U : 0U,
           arena_rendered ? 1U : 0U,
           native_layer_available
               ? (unsigned)runtime->intro_arena_tile_layer.cell_count
               : 0U,
           native_sprite_groups_available
               ? (unsigned)native_sprite_group_count
               : 0U,
           native_sprite_groups_available
               ? (unsigned)jumbotron_piece_count
               : 0U,
           native_sprite_groups_available
               ? (unsigned)goal_piece_count
               : 0U,
           (unsigned)visible_counts.jumbotron,
           (unsigned)visible_counts.goal);
    return true;
}

static bool print_intro_finale_status(
    const TecmoRuntime *runtime,
    const char *mode_name)
{
    TecmoIntroFinaleState state;
    if (!(strncmp(mode_name, "intro-finale", 12) == 0 ||
          strcmp(mode_name, "play-step14") == 0)) {
        return false;
    }
    tecmo_intro_finale_state(&runtime->intro_finale_asset,
                             runtime->mode_frame_counter, &state);
    printf("intro-finale-render-source finale=%u chr=%u schema=TFIN-1\n",
           runtime->intro_finale_asset.available ? 1U : 0U,
           runtime->title_chr_bytes != NULL ? 1U : 0U);
    printf("intro-finale-state frame=%u scene=%s phase=%s local=%u palette=%u variant=%u loop=%u anchor=%u,%u title=%u primary=%u:%u secondary=%u:%u sprites=%u black=%u hold=%u\n",
           runtime->mode_frame_counter,
           tecmo_intro_finale_scene_name(state.scene),
           tecmo_intro_finale_phase_name(state.phase), state.scene_frame,
           (unsigned)state.palette_stage,
           (unsigned)state.sprite_variant_index,
           (unsigned)state.short_loop_step, (unsigned)state.player_x,
           (unsigned)state.player_y, (unsigned)state.title_slots_written,
           (unsigned)state.scroll_page, (unsigned)state.scroll_x,
           (unsigned)state.secondary_scroll_page,
           (unsigned)state.secondary_scroll_x,
           state.sprites_visible ? 1U : 0U, state.black ? 1U : 0U,
           state.persistent_hold ? 1U : 0U);
    return true;
}

static bool print_intro_post_status(
    const TecmoRuntime *runtime,
    const char *mode_name)
{
    TecmoIntroReadyState ready_state;
    TecmoIntroWarriorsState warriors_state;
    TecmoIntroClippersState clippers_state;
    TecmoIntroBucksState bucks_state;
    TecmoIntroPassState pass_state;
    if (!(strncmp(mode_name, "intro-ready", 11) == 0 ||
          strncmp(mode_name, "intro-warriors", 14) == 0 ||
          strncmp(mode_name, "intro-clippers", 14) == 0 ||
          strncmp(mode_name, "intro-bucks", 11) == 0 ||
          strncmp(mode_name, "intro-pass", 10) == 0 ||
          strcmp(mode_name, "play-step9") == 0 ||
          strcmp(mode_name, "play-step10") == 0 ||
          strcmp(mode_name, "play-step11") == 0)) {
        return false;
    }
    tecmo_intro_ready_state(runtime->mode_frame_counter, &ready_state);
    tecmo_intro_warriors_state(runtime->mode_frame_counter, &warriors_state);
    tecmo_intro_clippers_state(runtime->mode_frame_counter, &clippers_state);
    tecmo_intro_bucks_state(runtime->mode_frame_counter, &bucks_state);
    tecmo_intro_pass_state(runtime->mode_frame_counter, &pass_state);
    printf("intro-post-render-source ready=%u warriors=%u clippers=%u bucks=%u pass=%u chr=%u ready_schema=TRDY-1 warriors_schema=TWAR-1 clippers_schema=TCLP-1 bucks_schema=TBUC-1 pass_schema=TPAS-1\n",
           runtime->intro_ready_asset.available ? 1U : 0U,
           runtime->intro_warriors_asset.available ? 1U : 0U,
           runtime->intro_clippers_asset.available ? 1U : 0U,
           runtime->intro_bucks_asset.available ? 1U : 0U,
           runtime->intro_pass_asset.available ? 1U : 0U,
           runtime->title_chr_bytes != NULL ? 1U : 0U);
    if (strncmp(mode_name, "intro-ready", 11) == 0 ||
        strcmp(mode_name, "play-step9") == 0) {
        printf("intro-ready-state frame=%u palette=%u mask=%u black=%u handoff=%u\n",
               runtime->mode_frame_counter,
               (unsigned)ready_state.palette_stage,
               (unsigned)ready_state.mask_index,
               ready_state.black ? 1U : 0U,
               ready_state.handoff ? 1U : 0U);
    } else if (strncmp(mode_name, "intro-warriors", 14) == 0 ||
               strcmp(mode_name, "play-step10") == 0) {
        printf("intro-warriors-state frame=%u phase=%s palette=%u pan=%u wordmark=%u patches=%u black=%u handoff=%u next_screen=%02X\n",
               runtime->mode_frame_counter,
               tecmo_intro_warriors_phase_name(warriors_state.phase),
               (unsigned)warriors_state.palette_stage,
               (unsigned)warriors_state.pan,
               (unsigned)warriors_state.wordmark_glyph_count,
               (unsigned)warriors_state.patch_count,
               warriors_state.black ? 1U : 0U,
               warriors_state.handoff ? 1U : 0U,
               (unsigned)warriors_state.next_screen);
    } else if (strncmp(mode_name, "intro-clippers", 14) == 0 ||
               strcmp(mode_name, "play-step11") == 0) {
        printf("intro-clippers-state frame=%u palette=%u motion=%u scroll=%u page=%u wordmark=%u handoff=%u next_route=%04X\n",
               runtime->mode_frame_counter,
               (unsigned)clippers_state.palette_stage,
               (unsigned)clippers_state.motion,
               (unsigned)clippers_state.scroll_x,
               (unsigned)clippers_state.pose_page,
               clippers_state.wordmark_visible ? 1U : 0U,
               clippers_state.handoff ? 1U : 0U,
               (unsigned)clippers_state.next_route);
    } else if (strncmp(mode_name, "intro-bucks", 11) == 0) {
        printf("intro-bucks-state frame=%u palette=%u flash=%u scroll=%u wordmark=%u prior=%u black=%u handoff=%u next_route=%04X\n",
               runtime->mode_frame_counter,
               (unsigned)bucks_state.palette_stage,
               (unsigned)bucks_state.flash_pass,
               (unsigned)bucks_state.scroll_x,
               (unsigned)bucks_state.wordmark_glyph_count,
               bucks_state.prior ? 1U : 0U,
               bucks_state.black ? 1U : 0U,
               bucks_state.handoff ? 1U : 0U,
               (unsigned)bucks_state.next_route);
    } else {
        printf("intro-pass-state frame=%u phase=%s palette=%u x=%u scroll=%u first=%u second=%u sprites=%u black=%u handoff=%u next_route=%04X\n",
               runtime->mode_frame_counter,
               tecmo_intro_pass_phase_name(pass_state.phase),
               (unsigned)pass_state.palette_stage,
               (unsigned)pass_state.player_x,
               (unsigned)pass_state.scroll_x,
               (unsigned)pass_state.first_move_count,
               (unsigned)pass_state.second_move_count,
               pass_state.sprites_visible ? 1U : 0U,
               pass_state.black ? 1U : 0U,
               pass_state.handoff ? 1U : 0U,
               (unsigned)pass_state.next_route);
    }
    return true;
}

void tecmo_cli_print_intro_render_capture_status(
    const TecmoRuntime *runtime,
    const char *mode_name,
    bool arena_rendered)
{
    if (runtime == NULL || mode_name == NULL) return;
    if (print_intro_opening_status(runtime, mode_name) ||
        print_intro_arena_status(runtime, mode_name, arena_rendered) ||
        print_intro_finale_status(runtime, mode_name) ||
        print_intro_post_status(runtime, mode_name)) {
        return;
    }
}

bool tecmo_cli_parse_finale_render_mode(const char *mode_name,
                                     unsigned *frame_out,
                                     bool *debug_out)
{
    static const struct FinaleModePrefix {
        const char *clean_prefix;
        const char *debug_prefix;
        TecmoIntroFinaleScene scene;
    } scene_prefixes[] = {
        {"intro-finale-opening-clean-frame", "intro-finale-opening-frame",
         TECMO_INTRO_FINALE_OPENING_SCREEN},
        {"intro-finale-short-clean-frame", "intro-finale-short-frame",
         TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP},
        {"intro-finale-reverse-clean-frame", "intro-finale-reverse-frame",
         TECMO_INTRO_FINALE_SELECTOR_TRANSITION},
        {"intro-finale-staged-clean-frame", "intro-finale-staged-frame",
         TECMO_INTRO_FINALE_STAGED_GROUP},
        {"intro-finale-title-clean-frame", "intro-finale-title-frame",
         TECMO_INTRO_FINALE_TITLE},
        {"intro-finale-hold-clean-frame", "intro-finale-hold-frame",
         TECMO_INTRO_FINALE_TERMINATOR_HOLD}
    };
    unsigned local_frame;

    if (tecmo_cli_parse_render_frame_suffix(mode_name, "intro-finale-clean-frame", &local_frame)) {
        *frame_out = local_frame;
        *debug_out = false;
        return true;
    }
    if (tecmo_cli_parse_render_frame_suffix(mode_name, "intro-finale-frame", &local_frame)) {
        *frame_out = local_frame;
        *debug_out = true;
        return true;
    }
    for (size_t i = 0U; i < sizeof(scene_prefixes) / sizeof(scene_prefixes[0]); ++i) {
        unsigned start = tecmo_intro_finale_scene_start_frame(scene_prefixes[i].scene);
        unsigned duration = tecmo_intro_finale_scene_duration(scene_prefixes[i].scene);
        if (tecmo_cli_parse_render_frame_suffix(mode_name, scene_prefixes[i].clean_prefix,
                                      &local_frame)) {
            if ((duration != 0U && local_frame >= duration) ||
                local_frame > UINT_MAX - start) return false;
            *frame_out = start + local_frame;
            *debug_out = false;
            return true;
        }
        if (tecmo_cli_parse_render_frame_suffix(mode_name, scene_prefixes[i].debug_prefix,
                                      &local_frame)) {
            if ((duration != 0U && local_frame >= duration) ||
                local_frame > UINT_MAX - start) return false;
            *frame_out = start + local_frame;
            *debug_out = true;
            return true;
        }
    }
    return false;
}
