#ifndef TECMO_ASSET_PACK_SOURCE_MAP_H
#define TECMO_ASSET_PACK_SOURCE_MAP_H

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_gameplay_audio.h"
#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_court.h"
#include "tecmo_asset_pack_gameplay_close_shots.h"
#include "tecmo_asset_pack_music.h"

#include <stddef.h>
#include <stdint.h>

char *tecmo_asset_pack_build_ines_source_map(
    uint32_t mapper,
    uint32_t trainer_bytes,
    uint32_t prg_banks,
    uint32_t chr_banks,
    uint64_t prg_offset,
    uint64_t chr_offset,
    uint64_t chr_size,
    const TecmoOpeningScreenProvenance opening_provenance[2],
    const TecmoArenaBackgroundProvenance *background_provenance,
    const TecmoArenaSpriteGroupsProvenance *sprite_groups_provenance,
    const TecmoPostArenaProvenance *post_arena_provenance,
    const TecmoFinaleProvenance *finale_provenance,
    const TecmoTitleProvenance title_provenance[2],
    const TecmoStartGameMenuProvenance *start_menu_provenance,
    const TecmoPreseasonMenuProvenance *preseason_provenance,
    const TecmoAllStarMenuProvenance *all_star_provenance,
    const TecmoMusicProvenance *music_provenance,
    const TecmoGameplayAudioProvenance *gameplay_audio_provenance,
    const TecmoTeamDataProvenance *team_data_provenance,
    const TecmoTeamManagementProvenance *team_management_provenance,
    const TecmoSeasonMenuProvenance *season_provenance,
    const TecmoGameplayProvenance *gameplay_provenance,
    const TecmoGameplayCourtProvenance *gameplay_court_provenance,
    const TecmoGameplayCloseShotProvenance *close_shot_provenance,
    size_t *source_map_size_out);

#endif
