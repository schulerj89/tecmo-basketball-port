#ifndef TECMO_ASSET_PACK_MUSIC_H
#define TECMO_ASSET_PACK_MUSIC_H

#include <stddef.h>
#include <stdint.h>

typedef struct TecmoMusicProvenance {
    uint64_t source_offset;
    uint64_t directory_offset;
    uint64_t engine_offset;
    uint64_t pitch_offset;
    uint64_t opening_queue_offset;
    uint64_t opening_first_route_offset;
    uint64_t menu_queue_offset;
    uint64_t pregame_matchup_queue_offset;
    uint64_t track_offsets[4];
    uint32_t track_sizes[4];
    uint32_t payload_size;
    uint32_t payload_fingerprint;
    uint32_t instruction_count;
    uint16_t voice_count;
} TecmoMusicProvenance;

int tecmo_asset_pack_build_music(const uint8_t *rom,
                                 uint64_t rom_size,
                                 uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 int enforce_revision_fingerprints,
                                 uint8_t **payload_out,
                                 size_t *payload_size_out,
                                 TecmoMusicProvenance *provenance,
                                 char *message,
                                 size_t message_size);

#endif
