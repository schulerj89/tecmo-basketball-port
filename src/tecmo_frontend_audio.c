#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_frontend_audio.h"
#include "tecmo_audio_output.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint32_t sample_hash(const int16_t *samples, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        uint16_t value = (uint16_t)samples[index];
        hash ^= (uint8_t)value;
        hash *= 16777619U;
        hash ^= (uint8_t)(value >> 8U);
        hash *= 16777619U;
    }
    return hash;
}

static bool finish_load(TecmoFrontendAudioAsset *asset,
                        const uint8_t metadata[20], bool loaded)
{
    if (asset == NULL) return false;
    if (!loaded) {
        (void)snprintf(asset->status, sizeof(asset->status), "%s",
                       asset->sfx.status);
        return false;
    }
    asset->title_stop_frame = metadata[0];
    asset->title_confirm_sfx_id = metadata[1];
    asset->menu_accept_sfx_id = metadata[2];
    asset->menu_music_track_id = metadata[3];
    asset->title_handoff_frame =
        (uint16_t)(metadata[4] | ((uint16_t)metadata[5] << 8U));
    asset->title_animation_frames =
        (uint16_t)(metadata[6] | ((uint16_t)metadata[7] << 8U));
    if (asset->title_stop_frame != 5U ||
        asset->title_confirm_sfx_id != 10U ||
        asset->menu_accept_sfx_id != 8U ||
        asset->menu_music_track_id != 6U ||
        asset->title_handoff_frame != 127U ||
        asset->title_animation_frames != 126U) {
        tecmo_gameplay_audio_asset_shutdown(&asset->sfx);
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TFSX-1 timing metadata rejected");
        return false;
    }
    (void)snprintf(asset->status, sizeof(asset->status),
                   "TFSX-1 native frontend audio");
    return true;
}

bool tecmo_frontend_audio_asset_load(TecmoFrontendAudioAsset *asset,
                                     const char *project_root)
{
    uint8_t metadata[20];
    bool loaded;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    memset(metadata, 0, sizeof(metadata));
    loaded = tecmo_gameplay_audio_frontend_asset_load(
        &asset->sfx, project_root, metadata);
    return finish_load(asset, metadata, loaded);
}

bool tecmo_frontend_audio_asset_load_from_pack(
    TecmoFrontendAudioAsset *asset, const char *asset_pack_path)
{
    uint8_t metadata[20];
    bool loaded;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    memset(metadata, 0, sizeof(metadata));
    loaded = tecmo_gameplay_audio_frontend_asset_load_from_pack(
        &asset->sfx, asset_pack_path, metadata);
    return finish_load(asset, metadata, loaded);
}

void tecmo_frontend_audio_asset_shutdown(TecmoFrontendAudioAsset *asset)
{
    if (asset == NULL) return;
    tecmo_gameplay_audio_asset_shutdown(&asset->sfx);
}

void tecmo_frontend_audio_player_init(TecmoFrontendAudioPlayer *player,
                                      const TecmoFrontendAudioAsset *asset,
                                      TecmoMusicPlayer *music)
{
    if (player == NULL) return;
    memset(player, 0, sizeof(*player));
    player->asset = asset;
    player->pack_identity_valid =
        music == NULL ||
        (asset != NULL && asset->sfx.available &&
         asset->sfx.asset_pack_path[0] != '\0' &&
         music->asset != NULL && music->asset->available &&
         strcmp(asset->sfx.asset_pack_path,
                music->asset->asset_pack_path) == 0);
    tecmo_gameplay_audio_player_init(
        &player->sfx, asset != NULL ? &asset->sfx : NULL, music);
}

bool tecmo_frontend_audio_queue_title_confirm(
    TecmoFrontendAudioPlayer *player)
{
    const TecmoFrontendAudioAsset *asset;
    bool queued;
    if (player == NULL || player->asset == NULL ||
        !player->pack_identity_valid)
        return false;
    asset = player->asset;
    queued = tecmo_gameplay_audio_queue_sfx_id(
        &player->sfx, asset->title_confirm_sfx_id);
    if (queued) ++player->title_confirm_queue_count;
    return queued;
}

bool tecmo_frontend_audio_queue_menu_accept(
    TecmoFrontendAudioPlayer *player)
{
    const TecmoFrontendAudioAsset *asset;
    bool queued;
    if (player == NULL || player->asset == NULL ||
        !player->pack_identity_valid)
        return false;
    asset = player->asset;
    queued = tecmo_gameplay_audio_queue_sfx_id(
        &player->sfx, asset->menu_accept_sfx_id);
    if (queued) ++player->menu_accept_queue_count;
    return queued;
}

void tecmo_frontend_audio_render_samples(TecmoFrontendAudioPlayer *player,
                                         int16_t *samples,
                                         size_t sample_count)
{
    if (sample_count > SIZE_MAX / sizeof(int16_t)) return;
    if (player == NULL || !player->pack_identity_valid) {
        tecmo_gameplay_audio_render_samples(NULL, samples, sample_count);
        return;
    }
    tecmo_gameplay_audio_render_samples(
        &player->sfx, samples, sample_count);
}

bool tecmo_frontend_audio_is_active(const TecmoFrontendAudioPlayer *player)
{
    return player != NULL && player->pack_identity_valid &&
           (player->sfx.sfx_pending || player->sfx.sfx_playing);
}

bool tecmo_frontend_audio_self_test(const char *project_root,
                                    char *message,
                                    size_t message_size)
{
    TecmoFrontendAudioAsset asset;
    TecmoFrontendAudioPlayer title;
    TecmoFrontendAudioPlayer menu;
    TecmoFrontendAudioPlayer routed;
    TecmoFrontendAudioPlayer guard_player;
    TecmoMusicAsset music_asset;
    TecmoMusicPlayer music_player;
    TecmoAudioOutput output;
    int16_t title_samples[4096];
    int16_t menu_samples[4096];
    uint32_t title_hash;
    uint32_t menu_hash;
    int16_t guard_sentinel;
    bool guard_ok;
    bool ok;
    memset(&asset, 0, sizeof(asset));
    memset(&music_asset, 0, sizeof(music_asset));
    memset(&output, 0, sizeof(output));
    memset(title_samples, 0, sizeof(title_samples));
    memset(menu_samples, 0, sizeof(menu_samples));
    if (!tecmo_frontend_audio_asset_load(&asset, project_root)) {
        if (message != NULL && message_size != 0U)
            (void)snprintf(message, message_size, "%s", asset.status);
        return false;
    }
    tecmo_frontend_audio_player_init(&title, &asset, NULL);
    tecmo_frontend_audio_player_init(&menu, &asset, NULL);
    tecmo_frontend_audio_player_init(&guard_player, &asset, NULL);
    {
        TecmoFrontendAudioPlayer guard_before = guard_player;
        guard_sentinel = (int16_t)0x2D2D;
        tecmo_frontend_audio_render_samples(
            &guard_player, &guard_sentinel,
            SIZE_MAX / sizeof(int16_t) + 1U);
        guard_ok = guard_sentinel == (int16_t)0x2D2D &&
                   memcmp(&guard_player, &guard_before,
                          sizeof(guard_player)) == 0;
        tecmo_frontend_audio_render_samples(
            &guard_player, NULL, SIZE_MAX / sizeof(int16_t) + 1U);
        guard_ok = guard_ok &&
                   memcmp(&guard_player, &guard_before,
                          sizeof(guard_player)) == 0;
    }
    ok = tecmo_frontend_audio_queue_title_confirm(&title) &&
         !tecmo_gameplay_audio_queue_sfx_id(&title.sfx, 8U + 1U) &&
         tecmo_frontend_audio_queue_menu_accept(&menu);
    tecmo_frontend_audio_render_samples(
        &title, title_samples,
        sizeof(title_samples) / sizeof(title_samples[0]));
    tecmo_frontend_audio_render_samples(
        &menu, menu_samples,
        sizeof(menu_samples) / sizeof(menu_samples[0]));
    title_hash = sample_hash(
        title_samples, sizeof(title_samples) / sizeof(title_samples[0]));
    menu_hash = sample_hash(
        menu_samples, sizeof(menu_samples) / sizeof(menu_samples[0]));
    if (tecmo_music_asset_load(&music_asset, project_root)) {
        tecmo_music_player_init(&music_player, &music_asset);
        tecmo_frontend_audio_player_init(
            &routed, &asset, &music_player);
        output.initialized = true;
        output.player = &music_player;
        ok = ok && tecmo_audio_output_select_frontend_player(
                       &output, &routed, &asset) &&
             tecmo_frontend_audio_queue_title_confirm(&routed) &&
             tecmo_audio_output_render_samples(&output, NULL, 64U) ==
                 TECMO_AUDIO_OUTPUT_RENDER_FRONTEND;
    } else {
        ok = false;
    }
    ok = ok && title_hash == 0x09718C9DU &&
         menu_hash == 0x100B5218U &&
         !title.sfx.render_guard_failed &&
         !menu.sfx.render_guard_failed &&
         title.title_confirm_queue_count == 1U &&
         title.menu_accept_queue_count == 0U &&
         menu.title_confirm_queue_count == 0U &&
         menu.menu_accept_queue_count == 1U && guard_ok;
    if (message != NULL && message_size != 0U) {
        (void)snprintf(
            message, message_size,
            "TFSX-1 frontend audio %s title=%08X menu=%08X",
            ok ? "ok" : "failed", title_hash, menu_hash);
    }
    tecmo_frontend_audio_asset_shutdown(&asset);
    tecmo_music_asset_shutdown(&music_asset);
    return ok;
}

bool tecmo_frontend_audio_cross_pack_self_test(
    const char *frontend_pack_path, const char *music_pack_path,
    char *message, size_t message_size)
{
    TecmoFrontendAudioAsset frontend_asset;
    TecmoFrontendAudioPlayer frontend_player;
    TecmoMusicAsset music_asset;
    TecmoMusicPlayer music_player;
    TecmoAudioOutput output;
    bool same_identity;
    bool ok;
    memset(&frontend_asset, 0, sizeof(frontend_asset));
    memset(&music_asset, 0, sizeof(music_asset));
    memset(&output, 0, sizeof(output));
    if (!tecmo_frontend_audio_asset_load_from_pack(
            &frontend_asset, frontend_pack_path) ||
        !tecmo_music_asset_load_from_pack(&music_asset, music_pack_path)) {
        if (message != NULL && message_size != 0U) {
            (void)snprintf(message, message_size, "%s",
                           !frontend_asset.sfx.available
                               ? frontend_asset.status
                               : music_asset.status);
        }
        tecmo_frontend_audio_asset_shutdown(&frontend_asset);
        tecmo_music_asset_shutdown(&music_asset);
        return false;
    }
    tecmo_music_player_init(&music_player, &music_asset);
    tecmo_frontend_audio_player_init(
        &frontend_player, &frontend_asset, &music_player);
    output.initialized = true;
    output.player = &music_player;
    same_identity =
        strcmp(frontend_asset.sfx.asset_pack_path,
               music_asset.asset_pack_path) == 0;
    if (same_identity) {
        ok = frontend_player.pack_identity_valid &&
             tecmo_audio_output_select_frontend_player(
                 &output, &frontend_player, &frontend_asset) &&
             tecmo_frontend_audio_queue_title_confirm(&frontend_player) &&
             tecmo_audio_output_render_samples(&output, NULL, 1U) ==
                 TECMO_AUDIO_OUTPUT_RENDER_FRONTEND;
    } else {
        ok = !frontend_player.pack_identity_valid &&
             !tecmo_frontend_audio_queue_title_confirm(&frontend_player) &&
             !tecmo_audio_output_select_frontend_player(
                 &output, &frontend_player, &frontend_asset) &&
             output.frontend_player == NULL &&
             output.frontend_asset == NULL;
        output.frontend_player = &frontend_player;
        output.frontend_asset = &frontend_asset;
        ok = ok &&
             tecmo_audio_output_render_samples(&output, NULL, 1U) ==
                 TECMO_AUDIO_OUTPUT_RENDER_MUSIC &&
             output.frontend_player == NULL &&
             output.frontend_asset == NULL;
    }
    if (message != NULL && message_size != 0U) {
        (void)snprintf(
            message, message_size,
            "TFSX-1/TMUS-1 %s provenance %s",
            same_identity ? "canonical-same-pack" : "cross-pack",
            ok ? (same_identity ? "accepted" : "rejected") : "failed");
    }
    tecmo_frontend_audio_asset_shutdown(&frontend_asset);
    tecmo_music_asset_shutdown(&music_asset);
    return ok;
}
