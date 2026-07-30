#ifndef TECMO_FRONTEND_AUDIO_H
#define TECMO_FRONTEND_AUDIO_H

#include "tecmo_gameplay_audio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_FRONTEND_SFX_PAYLOAD_SIZE 1792U
#define TECMO_FRONTEND_SFX_PAYLOAD_FNV1A32 0x985DC7EDU

typedef struct TecmoFrontendAudioAsset {
    TecmoGameplayAudioAsset sfx;
    uint8_t title_stop_frame;
    uint8_t title_confirm_sfx_id;
    uint8_t menu_accept_sfx_id;
    uint8_t menu_music_track_id;
    uint16_t title_handoff_frame;
    uint16_t title_animation_frames;
    char status[160];
} TecmoFrontendAudioAsset;

typedef struct TecmoFrontendAudioPlayer {
    TecmoGameplayAudioPlayer sfx;
    const TecmoFrontendAudioAsset *asset;
    uint32_t title_confirm_queue_count;
    uint32_t menu_accept_queue_count;
    bool pack_identity_valid;
} TecmoFrontendAudioPlayer;

bool tecmo_frontend_audio_asset_load(TecmoFrontendAudioAsset *asset,
                                     const char *project_root);
bool tecmo_frontend_audio_asset_load_from_pack(
    TecmoFrontendAudioAsset *asset, const char *asset_pack_path);
void tecmo_frontend_audio_asset_shutdown(TecmoFrontendAudioAsset *asset);
void tecmo_frontend_audio_player_init(TecmoFrontendAudioPlayer *player,
                                      const TecmoFrontendAudioAsset *asset,
                                      TecmoMusicPlayer *music);
bool tecmo_frontend_audio_queue_title_confirm(
    TecmoFrontendAudioPlayer *player);
bool tecmo_frontend_audio_queue_menu_accept(
    TecmoFrontendAudioPlayer *player);
void tecmo_frontend_audio_render_samples(TecmoFrontendAudioPlayer *player,
                                         int16_t *samples,
                                         size_t sample_count);
bool tecmo_frontend_audio_is_active(const TecmoFrontendAudioPlayer *player);
bool tecmo_frontend_audio_self_test(const char *project_root,
                                    char *message,
                                    size_t message_size);
bool tecmo_frontend_audio_cross_pack_self_test(
    const char *frontend_pack_path,
    const char *music_pack_path,
    char *message,
    size_t message_size);

#endif
