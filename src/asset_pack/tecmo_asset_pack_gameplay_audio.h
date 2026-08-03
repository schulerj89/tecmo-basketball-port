#ifndef TECMO_ASSET_PACK_GAMEPLAY_AUDIO_H
#define TECMO_ASSET_PACK_GAMEPLAY_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct TecmoGameplayAudioProvenance {
    uint64_t sfx_directory_offset;
    uint64_t sfx_core_offset;
    uint64_t sfx_extension_offset;
    uint64_t engine_offset;
    uint64_t event_offsets[6];
    uint64_t dmc_pool_offsets[3];
    uint64_t dmc_trigger_offsets[4];
    uint64_t gameplay_gate_offset;
    uint64_t restart_offset;
    uint64_t period_offset;
    uint64_t final_offset;
    uint32_t sfx_payload_size;
    uint32_t sfx_payload_fingerprint;
    uint32_t sfx_instruction_count;
    uint16_t sfx_voice_count;
    uint32_t dmc_payload_size;
    uint32_t dmc_payload_fingerprint;
} TecmoGameplayAudioProvenance;

typedef struct TecmoFrontendAudioProvenance {
    uint64_t sfx_directory_offset;
    uint64_t effect_offsets[2];
    uint64_t title_setup_offset;
    uint64_t title_confirm_offset;
    uint64_t title_setup_bridge_offset;
    uint64_t title_setup_transition_offset;
    uint64_t title_setup_fade_offset;
    uint64_t frame_yield_helper_offset;
    uint64_t title_stop_bridge_offset;
    uint64_t title_stop_dispatch_offset;
    uint64_t title_stop_offset;
    uint64_t menu_accept_offset;
    uint64_t menu_flow_offset;
    uint64_t mailboxes_offset;
    uint32_t payload_size;
    uint32_t payload_fingerprint;
    uint32_t instruction_count;
    uint16_t voice_count;
} TecmoFrontendAudioProvenance;

int tecmo_asset_pack_build_gameplay_audio(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t **sfx_payload_out,
    size_t *sfx_payload_size_out,
    uint8_t **dmc_payload_out,
    size_t *dmc_payload_size_out,
    TecmoGameplayAudioProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_frontend_audio(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t **payload_out,
    size_t *payload_size_out,
    TecmoFrontendAudioProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_frontend_audio_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_audio_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);

#endif
