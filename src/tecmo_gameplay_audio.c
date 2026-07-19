#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_audio.h"

#include "tecmo_asset_pack.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SFX_ENTRY_ID "audio/gameplay-sfx"
#define DMC_ENTRY_ID "audio/gameplay-dmc"
#define SFX_HEADER_SIZE 128U
#define SFX_EFFECT_OFFSET 128U
#define SFX_EFFECT_STRIDE 48U
#define SFX_VOICE_OFFSET 464U
#define SFX_VOICE_STRIDE 8U
#define SFX_PITCH_OFFSET 576U
#define SFX_INSTRUCTION_OFFSET 728U
#define SFX_INSTRUCTION_STRIDE 16U
#define DMC_HEADER_SIZE 128U
#define DMC_CLIP_OFFSET 128U
#define DMC_CLIP_STRIDE 32U
#define DMC_POOL_OFFSET 288U
#define DMC_POOL_STRIDE 16U
#define DMC_DATA_OFFSET 336U
#define GAMEPLAY_AUDIO_CPU_CLOCK 1789773.0
#define GAMEPLAY_AUDIO_CPU_CLOCK_INTEGER 1789773U
#define INVALID_INSTRUCTION UINT32_MAX

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static int make_path(char *path, size_t size, const char *root,
                     const char *suffix)
{
    size_t length;
    int written;
    if (path == NULL || size == 0U || root == NULL || root[0] == '\0')
        return -1;
    length = strlen(root);
    written = snprintf(path, size, "%s%s%s", root,
                       root[length - 1U] == '\\' ||
                               root[length - 1U] == '/'
                           ? ""
                           : "\\",
                       suffix);
    return written >= 0 && (size_t)written < size ? 0 : -1;
}

static bool file_exists(const char *path)
{
    FILE *file = path != NULL ? fopen(path, "rb") : NULL;
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static bool select_asset_pack(const char *root, char *selected, size_t size)
{
    const char *env = getenv("TECMO_ASSETPACK");
    const char *paths[4];
    char root_build[1024];
    char root_pack[1024];
    size_t count = 0U;
    size_t index;
    if (env != NULL && env[0] != '\0') {
        paths[count++] = env;
    } else {
        if (make_path(root_build, sizeof(root_build), root,
                      "build\\tecmo.assetpack") == 0)
            paths[count++] = root_build;
        if (make_path(root_pack, sizeof(root_pack), root,
                      "tecmo.assetpack") == 0)
            paths[count++] = root_pack;
        paths[count++] = "build\\tecmo.assetpack";
        paths[count++] = "tecmo.assetpack";
    }
    for (index = 0U; index < count; ++index) {
        int written;
        if (!file_exists(paths[index])) continue;
        written = snprintf(selected, size, "%s", paths[index]);
        return written >= 0 && (size_t)written < size;
    }
    return false;
}

static bool instruction_in_channel(uint32_t value, uint32_t first,
                                   uint32_t count)
{
    return value >= first && value - first < count;
}

static bool validate_walk(const TecmoGameplayAudioAsset *asset,
                          uint32_t instruction_index, uint32_t first,
                          uint32_t count, unsigned depth,
                          uint32_t path[TECMO_MUSIC_CALL_DEPTH],
                          uint8_t *visited, uint8_t *reachable,
                          unsigned *max_depth)
{
    const TecmoMusicInstruction *instruction;
    size_t row_offset;
    unsigned index;
    if (depth > TECMO_MUSIC_CALL_DEPTH ||
        !instruction_in_channel(instruction_index, first, count))
        return false;
    row_offset = (size_t)depth * TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT +
                 instruction_index;
    if (visited[row_offset] != 0U) return true;
    visited[row_offset] = 1U;
    reachable[instruction_index] = 1U;
    if (depth > *max_depth) *max_depth = depth;
    instruction = &asset->instructions[instruction_index];
    if (instruction->type == TECMO_MUSIC_CALL) {
        if (depth >= TECMO_MUSIC_CALL_DEPTH) return false;
        for (index = 0U; index < depth; ++index) {
            if (path[index] == instruction->target) return false;
        }
        path[depth] = instruction->target;
        if (!validate_walk(asset, instruction->target, first, count,
                           depth + 1U, path, visited, reachable, max_depth))
            return false;
    } else if (instruction->type == TECMO_MUSIC_LOOP) {
        if (!validate_walk(asset, instruction->target, first, count, depth,
                           path, visited, reachable, max_depth))
            return false;
    }
    if (instruction->next != INVALID_INSTRUCTION)
        return validate_walk(asset, instruction->next, first, count, depth,
                             path, visited, reachable, max_depth);
    return instruction->type == TECMO_MUSIC_END ||
           instruction->type == TECMO_MUSIC_RETURN;
}

static bool parse_sfx(TecmoGameplayAudioAsset *asset, const uint8_t *bytes,
                      uint64_t count, bool enforce_payload_fingerprint)
{
    static const uint8_t effect_ids[TECMO_GAMEPLAY_SFX_EFFECT_COUNT] = {
        3U, 5U, 6U, 11U, 12U, 13U, 14U
    };
    static const uint32_t effect_fingerprints[
        TECMO_GAMEPLAY_SFX_EFFECT_COUNT] = {
        0xB7138F94U, 0x28EE1024U, 0x34460805U, 0x93E7AC2CU,
        0xB172920DU, 0xDC401221U, 0xE3035B54U
    };
    uint8_t *visited = NULL;
    uint8_t *reachable = NULL;
    uint32_t expected_first = 0U;
    uint32_t instruction_index;
    unsigned effect_index;
    unsigned max_depth = 0U;
    bool ok = false;
    if (asset == NULL || bytes == NULL ||
        count != TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE ||
        (enforce_payload_fingerprint &&
         fnv1a32(bytes, (size_t)count) !=
             TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32) ||
        memcmp(bytes, "TSFX", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != SFX_HEADER_SIZE ||
        read_u32(bytes + 8U) != TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE ||
        read_u32(bytes + 12U) != 0x0650F5B0U ||
        read_u32(bytes + 16U) != 0x6283F255U ||
        read_u32(bytes + 20U) != 0x548EED95U ||
        read_u32(bytes + 24U) != 0x838408D4U ||
        read_u32(bytes + 28U) != 0xFC6A0BC1U ||
        read_u32(bytes + 32U) != 0x80402010U ||
        read_u16(bytes + 36U) != TECMO_GAMEPLAY_SFX_EFFECT_COUNT ||
        read_u16(bytes + 38U) != TECMO_GAMEPLAY_SFX_CHANNEL_COUNT ||
        read_u16(bytes + 40U) != TECMO_GAMEPLAY_SFX_VOICE_COUNT ||
        read_u16(bytes + 42U) != TECMO_GAMEPLAY_SFX_PITCH_COUNT ||
        read_u16(bytes + 44U) != SFX_EFFECT_STRIDE ||
        read_u16(bytes + 46U) != SFX_VOICE_STRIDE ||
        read_u16(bytes + 48U) != SFX_INSTRUCTION_STRIDE ||
        read_u16(bytes + 50U) > TECMO_MUSIC_CALL_DEPTH ||
        read_u32(bytes + 52U) != TECMO_MUSIC_SAMPLE_RATE ||
        read_u32(bytes + 56U) != TECMO_MUSIC_TICK_NUMERATOR ||
        read_u32(bytes + 60U) != TECMO_MUSIC_TICK_DENOMINATOR ||
        read_u32(bytes + 64U) != SFX_EFFECT_OFFSET ||
        read_u32(bytes + 68U) != SFX_VOICE_OFFSET ||
        read_u32(bytes + 72U) != SFX_PITCH_OFFSET ||
        read_u32(bytes + 76U) != SFX_INSTRUCTION_OFFSET ||
        read_u32(bytes + 80U) != TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT ||
        read_u16(bytes + 84U) != TECMO_MUSIC_LOOP_COUNT ||
        read_u16(bytes + 86U) != 0U)
        return false;
    for (instruction_index = 123U; instruction_index < SFX_HEADER_SIZE;
         ++instruction_index) {
        if (bytes[instruction_index] != 0U) return false;
    }
    asset->revision_token = read_u32(bytes + 12U);
    asset->sfx_payload_fingerprint = TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32;
    asset->instructions = (TecmoMusicInstruction *)calloc(
        TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT,
        sizeof(*asset->instructions));
    if (asset->instructions == NULL) return false;
    for (effect_index = 0U;
         effect_index < TECMO_GAMEPLAY_SFX_EFFECT_COUNT; ++effect_index) {
        const uint8_t *source = bytes + SFX_EFFECT_OFFSET +
            effect_index * SFX_EFFECT_STRIDE;
        unsigned channel;
        if (read_u32(bytes + 88U + effect_index * 4U) !=
                effect_fingerprints[effect_index] ||
            bytes[116U + effect_index] != effect_ids[effect_index] ||
            source[0] != effect_ids[effect_index] || source[1] != 0U ||
            read_u16(source + 2U) != TECMO_GAMEPLAY_SFX_CHANNEL_COUNT ||
            read_u32(source + 4U) != effect_fingerprints[effect_index])
            goto cleanup;
        for (instruction_index = 40U;
             instruction_index < SFX_EFFECT_STRIDE; ++instruction_index) {
            if (source[instruction_index] != 0U) goto cleanup;
        }
        asset->effects[effect_index].id = source[0];
        asset->effects[effect_index].source_fingerprint = read_u32(source + 4U);
        for (channel = 0U; channel < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT;
             ++channel) {
            uint32_t first = read_u32(source + 8U + channel * 8U);
            uint32_t channel_count = read_u32(source + 12U + channel * 8U);
            if (first != expected_first || channel_count == 0U ||
                channel_count >
                    TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT - expected_first)
                goto cleanup;
            asset->effects[effect_index].channels[channel]
                .first_instruction = first;
            asset->effects[effect_index].channels[channel]
                .instruction_count = channel_count;
            expected_first += channel_count;
        }
    }
    if (expected_first != TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT) goto cleanup;
    for (effect_index = 0U; effect_index < TECMO_GAMEPLAY_SFX_VOICE_COUNT;
         ++effect_index) {
        const uint8_t *source = bytes + SFX_VOICE_OFFSET +
            effect_index * SFX_VOICE_STRIDE;
        TecmoMusicVoice *voice = &asset->voices[effect_index];
        voice->duty_and_timing = source[0];
        voice->initial_volume = source[1];
        voice->sweep = source[2];
        voice->attack = source[3];
        voice->decay = source[4];
        voice->sustain_ticks = source[5];
        voice->release = source[6];
        voice->peak_and_sustain_volume = source[7];
        if ((voice->sweep != 0U && voice->sweep != 0x05U &&
             voice->sweep != 0x0FU) ||
            (voice->initial_volume & 0xE0U) != 0U)
            goto cleanup;
    }
    for (effect_index = 0U; effect_index < TECMO_GAMEPLAY_SFX_PITCH_COUNT;
         ++effect_index) {
        uint16_t period = read_u16(bytes + SFX_PITCH_OFFSET +
                                   effect_index * 2U);
        if (period == 0U || period > 0x7FFU ||
            (effect_index > 0U &&
             period >= asset->pitch_periods[effect_index - 1U]))
            goto cleanup;
        asset->pitch_periods[effect_index] = period;
    }
    for (instruction_index = 0U;
         instruction_index < TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT;
         ++instruction_index) {
        const uint8_t *source = bytes + SFX_INSTRUCTION_OFFSET +
            instruction_index * SFX_INSTRUCTION_STRIDE;
        TecmoMusicInstruction *instruction =
            &asset->instructions[instruction_index];
        instruction->type = (TecmoMusicInstructionType)source[0];
        instruction->value8 = source[1];
        instruction->value16 = read_u16(source + 2U);
        instruction->next = read_u32(source + 4U);
        instruction->target = read_u32(source + 8U);
        instruction->signed_value = (int16_t)read_u16(source + 12U);
        instruction->loop_slot = read_u16(source + 14U);
        if (instruction->type < TECMO_MUSIC_NOTE ||
            instruction->type > TECMO_MUSIC_RETURN)
            goto cleanup;
    }
    visited = (uint8_t *)calloc(
        (TECMO_MUSIC_CALL_DEPTH + 1U) *
            TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT,
        1U);
    reachable = (uint8_t *)calloc(TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT, 1U);
    if (visited == NULL || reachable == NULL) goto cleanup;
    for (effect_index = 0U;
         effect_index < TECMO_GAMEPLAY_SFX_EFFECT_COUNT; ++effect_index) {
        unsigned channel;
        for (channel = 0U; channel < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT;
             ++channel) {
            const TecmoMusicChannelProgram *program =
                &asset->effects[effect_index].channels[channel];
            uint32_t path[TECMO_MUSIC_CALL_DEPTH];
            uint32_t end = program->first_instruction +
                           program->instruction_count;
            uint32_t index;
            memset(path, 0, sizeof(path));
            for (index = program->first_instruction; index < end; ++index) {
                const TecmoMusicInstruction *instruction =
                    &asset->instructions[index];
                bool has_next = instruction->next != INVALID_INSTRUCTION;
                bool has_target = instruction->target != INVALID_INSTRUCTION;
                if ((has_next &&
                     !instruction_in_channel(instruction->next,
                                             program->first_instruction,
                                             program->instruction_count)) ||
                    (has_target &&
                     !instruction_in_channel(instruction->target,
                                             program->first_instruction,
                                             program->instruction_count)))
                    goto cleanup;
                switch (instruction->type) {
                case TECMO_MUSIC_NOTE:
                    if (!has_next || has_target ||
                        instruction->value16 == 0U ||
                        (channel != 3U &&
                         (uint16_t)(instruction->value8 +
                                    (channel == 2U ? 12U : 0U)) >=
                             TECMO_GAMEPLAY_SFX_PITCH_COUNT) ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_SET_VOICE:
                    if (!has_next || has_target ||
                        instruction->value8 >=
                            TECMO_GAMEPLAY_SFX_VOICE_COUNT ||
                        instruction->value16 != 0U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_LEGATO:
                case TECMO_MUSIC_BIND_PHRASES:
                    if (!has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_PITCH_DELTA:
                    if (!has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U ||
                        instruction->loop_slot != UINT16_MAX ||
                        instruction->signed_value < -128 ||
                        instruction->signed_value > 127)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_REST:
                    if (!has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 == 0U ||
                        instruction->value16 > 127U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_LOOP:
                    if (!has_next || !has_target || instruction->value8 != 0U ||
                        instruction->value16 == 0U ||
                        instruction->value16 > 256U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot >= TECMO_MUSIC_LOOP_COUNT)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_CALL:
                    if (!has_next || !has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_END:
                case TECMO_MUSIC_RETURN:
                    if (has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                default: goto cleanup;
                }
            }
            if (!validate_walk(asset, program->first_instruction,
                               program->first_instruction,
                               program->instruction_count, 0U, path, visited,
                               reachable, &max_depth))
                goto cleanup;
        }
    }
    if (max_depth != read_u16(bytes + 50U)) goto cleanup;
    for (instruction_index = 0U;
         instruction_index < TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT;
         ++instruction_index) {
        if (reachable[instruction_index] == 0U) goto cleanup;
    }
    ok = true;

cleanup:
    free(visited);
    free(reachable);
    if (!ok) {
        free(asset->instructions);
        asset->instructions = NULL;
    }
    return ok;
}

static bool parse_dmc(TecmoGameplayAudioAsset *asset, const uint8_t *bytes,
                      uint64_t count, bool enforce_payload_fingerprint)
{
    static const uint32_t pool_hash[TECMO_GAMEPLAY_DMC_POOL_COUNT] = {
        0x33E109D7U, 0x6ECC107CU, 0xF621FD7CU
    };
    static const uint32_t pool_size[TECMO_GAMEPLAY_DMC_POOL_COUNT] = {
        513U, 721U, 945U
    };
    static const uint8_t clip_pool[TECMO_GAMEPLAY_DMC_CLIP_COUNT] = {
        2U, 2U, 2U, 1U, 0U
    };
    static const uint8_t clip_rate[TECMO_GAMEPLAY_DMC_CLIP_COUNT] = {
        14U, 14U, 14U, 15U, 15U
    };
    static const uint32_t clip_size[TECMO_GAMEPLAY_DMC_CLIP_COUNT] = {
        161U, 769U, 945U, 721U, 513U
    };
    static const uint32_t trigger_hash[TECMO_GAMEPLAY_DMC_CLIP_COUNT] = {
        0xEE75A82EU, 0xEE75A82EU, 0x567D5B90U, 0x1C158FABU,
        0xF6CEC8DBU
    };
    uint32_t expected_data_offset = DMC_DATA_OFFSET;
    unsigned index;
    if (asset == NULL || bytes == NULL ||
        count != TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE ||
        (enforce_payload_fingerprint &&
         fnv1a32(bytes, (size_t)count) !=
             TECMO_GAMEPLAY_DMC_PAYLOAD_FNV1A32) ||
        memcmp(bytes, "TDMC", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != DMC_HEADER_SIZE ||
        read_u32(bytes + 8U) != TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE ||
        read_u32(bytes + 12U) != 0x0650F5B0U ||
        read_u32(bytes + 28U) != 0xEE75A82EU ||
        read_u32(bytes + 32U) != 0x567D5B90U ||
        read_u32(bytes + 36U) != 0x1C158FABU ||
        read_u32(bytes + 40U) != 0xF6CEC8DBU ||
        read_u32(bytes + 44U) != 0x181D6897U ||
        read_u32(bytes + 48U) != 0xAC51064EU ||
        read_u32(bytes + 52U) != 0x1185F342U ||
        read_u32(bytes + 56U) != 0x03BFDFA0U ||
        read_u16(bytes + 60U) != TECMO_GAMEPLAY_DMC_CLIP_COUNT ||
        read_u16(bytes + 62U) != TECMO_GAMEPLAY_DMC_POOL_COUNT ||
        read_u16(bytes + 64U) != DMC_CLIP_STRIDE ||
        read_u16(bytes + 66U) != DMC_POOL_STRIDE ||
        read_u32(bytes + 68U) != DMC_CLIP_OFFSET ||
        read_u32(bytes + 72U) != DMC_POOL_OFFSET ||
        read_u32(bytes + 76U) != DMC_DATA_OFFSET ||
        read_u32(bytes + 80U) != TECMO_GAMEPLAY_DMC_DATA_SIZE)
        return false;
    for (index = 0U; index < TECMO_GAMEPLAY_DMC_POOL_COUNT; ++index) {
        if (read_u32(bytes + 16U + index * 4U) != pool_hash[index])
            return false;
    }
    for (index = 84U; index < DMC_HEADER_SIZE; ++index) {
        if (bytes[index] != 0U) return false;
    }
    asset->dmc_data = (uint8_t *)malloc(TECMO_GAMEPLAY_DMC_DATA_SIZE);
    if (asset->dmc_data == NULL) return false;
    memcpy(asset->dmc_data, bytes + DMC_DATA_OFFSET,
           TECMO_GAMEPLAY_DMC_DATA_SIZE);
    for (index = 0U; index < TECMO_GAMEPLAY_DMC_POOL_COUNT; ++index) {
        const uint8_t *source = bytes + DMC_POOL_OFFSET +
            index * DMC_POOL_STRIDE;
        uint32_t data_offset = read_u32(source + 0U);
        uint32_t byte_count = read_u32(source + 4U);
        if (data_offset != expected_data_offset ||
            byte_count != pool_size[index] ||
            read_u32(source + 8U) != pool_hash[index] ||
            read_u32(source + 12U) != 0U ||
            data_offset > TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE ||
            byte_count > TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE - data_offset ||
            fnv1a32(bytes + data_offset, byte_count) != pool_hash[index])
            goto malformed;
        asset->dmc_pools[index].data_offset = data_offset - DMC_DATA_OFFSET;
        asset->dmc_pools[index].byte_count = byte_count;
        asset->dmc_pools[index].source_fingerprint = pool_hash[index];
        expected_data_offset += byte_count;
    }
    if (expected_data_offset != TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE)
        goto malformed;
    for (index = 0U; index < TECMO_GAMEPLAY_DMC_CLIP_COUNT; ++index) {
        const uint8_t *source = bytes + DMC_CLIP_OFFSET +
            index * DMC_CLIP_STRIDE;
        TecmoGameplayDmcClip *clip = &asset->dmc_clips[index];
        unsigned reserved;
        if (source[0] != index || source[1] != clip_pool[index] ||
            source[2] != clip_rate[index] || source[3] != 0U ||
            read_u32(source + 4U) != 0U ||
            read_u32(source + 8U) != clip_size[index] ||
            read_u32(source + 12U) != trigger_hash[index] ||
            source[16] != 0x1FU ||
            clip_size[index] > pool_size[clip_pool[index]])
            goto malformed;
        for (reserved = 17U; reserved < DMC_CLIP_STRIDE; ++reserved) {
            if (source[reserved] != 0U) goto malformed;
        }
        clip->id = source[0];
        clip->pool_index = source[1];
        clip->rate_index = source[2];
        clip->pool_offset = read_u32(source + 4U);
        clip->byte_count = read_u32(source + 8U);
        clip->trigger_fingerprint = read_u32(source + 12U);
    }
    asset->dmc_payload_fingerprint = TECMO_GAMEPLAY_DMC_PAYLOAD_FNV1A32;
    return true;

malformed:
    free(asset->dmc_data);
    asset->dmc_data = NULL;
    return false;
}

static bool revision_tokens_match(uint32_t sfx_token, uint32_t dmc_token)
{
    return sfx_token == dmc_token;
}

bool tecmo_gameplay_audio_asset_load_from_pack(
    TecmoGameplayAudioAsset *asset,
    const char *asset_pack_path)
{
    uint8_t *sfx_bytes = NULL;
    uint8_t *dmc_bytes = NULL;
    uint64_t sfx_count = 0U;
    uint64_t dmc_count = 0U;
    bool ok;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (asset_pack_path == NULL || asset_pack_path[0] == '\0' ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, SFX_ENTRY_ID, TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE,
            &sfx_bytes, &sfx_count) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, DMC_ENTRY_ID, TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE,
            &dmc_bytes, &dmc_count) != 0) {
        tecmo_asset_pack_free(sfx_bytes);
        tecmo_asset_pack_free(dmc_bytes);
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TSFX-1/TDMC-1 gameplay audio entries unavailable");
        return false;
    }
    ok = parse_sfx(asset, sfx_bytes, sfx_count, true) &&
         parse_dmc(asset, dmc_bytes, dmc_count, true) &&
         revision_tokens_match(asset->revision_token,
                               read_u32(dmc_bytes + 12U));
    tecmo_asset_pack_free(sfx_bytes);
    tecmo_asset_pack_free(dmc_bytes);
    if (!ok) {
        free(asset->instructions);
        free(asset->dmc_data);
        asset->instructions = NULL;
        asset->dmc_data = NULL;
    }
    asset->available = ok;
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TSFX-1/TDMC-1 native gameplay audio"
                      : "TSFX-1/TDMC-1 asset contract rejected");
    return ok;
}

bool tecmo_gameplay_audio_asset_load(TecmoGameplayAudioAsset *asset,
                                     const char *project_root)
{
    char pack_path[1024];
    if (asset == NULL) return false;
    if (!select_asset_pack(project_root, pack_path, sizeof(pack_path))) {
        memset(asset, 0, sizeof(*asset));
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TSFX-1/TDMC-1 gameplay audio entries unavailable");
        return false;
    }
    return tecmo_gameplay_audio_asset_load_from_pack(asset, pack_path);
}

void tecmo_gameplay_audio_asset_shutdown(TecmoGameplayAudioAsset *asset)
{
    if (asset == NULL) return;
    free(asset->instructions);
    free(asset->dmc_data);
    asset->instructions = NULL;
    asset->dmc_data = NULL;
    asset->available = false;
}

void tecmo_gameplay_audio_player_init(TecmoGameplayAudioPlayer *player,
                                      const TecmoGameplayAudioAsset *asset,
                                      TecmoMusicPlayer *music)
{
    if (player == NULL) return;
    memset(player, 0, sizeof(*player));
    player->asset = asset;
    player->music = music;
    player->noise_lfsr = 1U;
    player->dmc.output_level = 64U;
}

static const TecmoGameplaySfxEffect *find_effect(
    const TecmoGameplayAudioAsset *asset, uint8_t id)
{
    unsigned index;
    if (asset == NULL || !asset->available) return NULL;
    for (index = 0U; index < TECMO_GAMEPLAY_SFX_EFFECT_COUNT; ++index) {
        if (asset->effects[index].id == id) return &asset->effects[index];
    }
    return NULL;
}

static uint8_t envelope_timer8(uint8_t parameter, uint8_t shift)
{
    return (uint8_t)((parameter >> 4U) << shift);
}

static uint16_t envelope_timer16(uint8_t parameter, uint8_t shift)
{
    return (uint16_t)((uint16_t)(parameter >> 4U) << shift);
}

static uint8_t voice_attack_shift(uint8_t duty_and_timing)
{
    return (uint8_t)((duty_and_timing >> 7U) & 1U);
}

static uint8_t voice_decay_shift(uint8_t duty_and_timing)
{
    return (uint8_t)((duty_and_timing >> 5U) & 3U);
}

static uint8_t voice_release_shift(uint8_t duty_and_timing)
{
    return (uint8_t)((duty_and_timing >> 2U) & 7U);
}

static void reset_envelope(TecmoMusicChannelState *channel,
                           const TecmoMusicVoice *voice)
{
    channel->volume = (uint8_t)(voice->initial_volume & 0x0FU);
    channel->envelope_phase = 0U;
    channel->attack_timer = envelope_timer8(
        voice->attack, voice_attack_shift(voice->duty_and_timing));
    channel->decay_timer = envelope_timer8(
        voice->decay, voice_decay_shift(voice->duty_and_timing));
    channel->sustain_timer = voice->sustain_ticks;
    channel->release_timer = envelope_timer16(
        voice->release, voice_release_shift(voice->duty_and_timing));
}

static void tick_envelope(TecmoMusicChannelState *channel,
                          const TecmoMusicVoice *voice)
{
    uint8_t peak = (uint8_t)(voice->peak_and_sustain_volume & 0x0FU);
    uint8_t sustain = (uint8_t)(voice->peak_and_sustain_volume >> 4U);
    if (!channel->note_on) return;
    for (;;) {
        if (channel->envelope_phase == 0U) {
            uint8_t amount = (uint8_t)(voice->attack & 0x0FU);
            if (channel->volume == peak) {
                channel->envelope_phase = 1U;
                continue;
            }
            if (channel->attack_timer > 0U) {
                --channel->attack_timer;
                return;
            }
            channel->attack_timer = envelope_timer8(
                voice->attack, voice_attack_shift(voice->duty_and_timing));
            channel->volume = (uint8_t)(channel->volume + amount);
            if (channel->volume >= peak || channel->volume > 15U)
                channel->volume = peak;
            return;
        }
        if (channel->envelope_phase == 1U) {
            uint8_t amount = (uint8_t)(voice->decay & 0x0FU);
            if (amount == 0U || channel->volume == sustain) {
                channel->envelope_phase = 2U;
                continue;
            }
            if (channel->decay_timer > 0U) {
                --channel->decay_timer;
                return;
            }
            channel->decay_timer = envelope_timer8(
                voice->decay, voice_decay_shift(voice->duty_and_timing));
            channel->volume = channel->volume > amount
                                  ? (uint8_t)(channel->volume - amount)
                                  : 0U;
            if (channel->volume < sustain) channel->volume = sustain;
            return;
        }
        if (channel->envelope_phase == 2U) {
            --channel->sustain_timer;
            if (channel->sustain_timer == 0xFFU) {
                channel->envelope_phase = 3U;
                continue;
            }
            return;
        }
        if (channel->volume > 0U) {
            uint8_t amount = (uint8_t)(voice->release & 0x0FU);
            if (channel->release_timer > 0U) {
                --channel->release_timer;
                return;
            }
            channel->release_timer = envelope_timer16(
                voice->release, voice_release_shift(voice->duty_and_timing));
            channel->volume = channel->volume > amount
                                  ? (uint8_t)(channel->volume - amount)
                                  : 0U;
        }
        return;
    }
}

static void update_period(TecmoGameplayAudioPlayer *player,
                          unsigned channel_index)
{
    TecmoMusicChannelState *channel = &player->sfx_channels[channel_index];
    int pitch_index;
    int period;
    if (channel_index == 3U) {
        channel->period = channel->note;
        return;
    }
    pitch_index = channel->note + (channel_index == 2U ? 12 : 0);
    if (pitch_index < 0 || pitch_index >= TECMO_GAMEPLAY_SFX_PITCH_COUNT) {
        player->render_guard_failed = true;
        channel->note_on = false;
        return;
    }
    period = (int)player->asset->pitch_periods[pitch_index] -
             channel->pitch_delta;
    if (period < 0) period = 0;
    if (period > 0x7FF) period = 0x7FF;
    channel->period = (uint16_t)period;
}

static void process_channel(TecmoGameplayAudioPlayer *player,
                            unsigned channel_index)
{
    TecmoMusicChannelState *channel = &player->sfx_channels[channel_index];
    unsigned guard;
    for (guard = 0U; guard < 4096U && channel->active; ++guard) {
        const TecmoMusicInstruction *instruction;
        if (channel->pc >= TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT) break;
        instruction = &player->asset->instructions[channel->pc];
        switch (instruction->type) {
        case TECMO_MUSIC_NOTE:
            channel->note = instruction->value8;
            channel->duration_ticks = instruction->value16;
            channel->note_on = true;
            update_period(player, channel_index);
            if (!channel->legato_next && channel_index != 2U) {
                if (channel->voice_index >= TECMO_GAMEPLAY_SFX_VOICE_COUNT)
                    break;
                reset_envelope(
                    channel, &player->asset->voices[channel->voice_index]);
            }
            channel->legato_next = false;
            channel->pc = instruction->next;
            return;
        case TECMO_MUSIC_SET_VOICE:
            channel->voice_index = instruction->value8;
            channel->pc = instruction->next;
            break;
        case TECMO_MUSIC_LEGATO:
            channel->legato_next = true;
            channel->pc = instruction->next;
            break;
        case TECMO_MUSIC_PITCH_DELTA:
            if (instruction->signed_value == 0)
                channel->pitch_delta = 0;
            else
                channel->pitch_delta += instruction->signed_value;
            channel->pc = instruction->next;
            break;
        case TECMO_MUSIC_REST:
            channel->note_on = false;
            channel->duration_ticks = instruction->value16;
            channel->pc = instruction->next;
            return;
        case TECMO_MUSIC_LOOP: {
            uint16_t *remaining =
                &channel->loop_remaining[instruction->loop_slot];
            if (*remaining == 0U) *remaining = instruction->value16;
            --*remaining;
            channel->pc = *remaining != 0U ? instruction->target
                                           : instruction->next;
            break;
        }
        case TECMO_MUSIC_BIND_PHRASES:
            channel->pc = instruction->next;
            break;
        case TECMO_MUSIC_CALL:
            if (channel->call_depth >= TECMO_MUSIC_CALL_DEPTH) break;
            channel->call_stack[channel->call_depth++] = instruction->next;
            channel->pc = instruction->target;
            break;
        case TECMO_MUSIC_RETURN:
            if (channel->call_depth == 0U) {
                channel->active = false;
                channel->note_on = false;
            } else {
                channel->pc = channel->call_stack[--channel->call_depth];
            }
            break;
        case TECMO_MUSIC_END:
            channel->active = false;
            channel->note_on = false;
            break;
        default:
            channel->active = false;
            channel->note_on = false;
            break;
        }
    }
    if (channel->active) {
        channel->active = false;
        channel->note_on = false;
        player->render_guard_failed = true;
    }
}

static void sfx_tick(TecmoGameplayAudioPlayer *player)
{
    unsigned channel_index;
    bool any_active = false;
    ++player->ticks_elapsed;
    if (player->sfx_pending) {
        const TecmoGameplaySfxEffect *effect =
            find_effect(player->asset, player->pending_sfx_id);
        player->sfx_pending = false;
        if (effect == NULL) {
            player->sfx_playing = false;
            player->render_guard_failed = true;
            return;
        }
        memset(player->sfx_channels, 0, sizeof(player->sfx_channels));
        for (channel_index = 0U;
             channel_index < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT;
             ++channel_index) {
            player->sfx_channels[channel_index].pc =
                effect->channels[channel_index].first_instruction;
            player->sfx_channels[channel_index].active = true;
            process_channel(player, channel_index);
        }
        player->current_sfx_id = effect->id;
    }
    for (channel_index = 0U;
         channel_index < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT; ++channel_index) {
        TecmoMusicChannelState *channel =
            &player->sfx_channels[channel_index];
        if (!channel->active) continue;
        if (channel->duration_ticks > 0U) {
            --channel->duration_ticks;
            if (channel->duration_ticks > 0U) {
                if (channel_index != 2U &&
                    channel->voice_index < TECMO_GAMEPLAY_SFX_VOICE_COUNT)
                    tick_envelope(
                        channel,
                        &player->asset->voices[channel->voice_index]);
            } else {
                process_channel(player, channel_index);
            }
        } else {
            process_channel(player, channel_index);
        }
        if (channel->active) any_active = true;
    }
    player->sfx_playing = any_active;
}

static double pulse_sample(TecmoGameplayAudioPlayer *player,
                           unsigned channel_index)
{
    static const double duty[4] = {0.125, 0.25, 0.5, 0.75};
    TecmoMusicChannelState *channel =
        &player->sfx_channels[channel_index];
    const TecmoMusicVoice *voice;
    double frequency;
    if (!channel->active || !channel->note_on || channel->volume == 0U ||
        channel->voice_index >= TECMO_GAMEPLAY_SFX_VOICE_COUNT)
        return 0.0;
    voice = &player->asset->voices[channel->voice_index];
    frequency = GAMEPLAY_AUDIO_CPU_CLOCK /
                (16.0 * ((double)channel->period + 1.0));
    channel->phase += frequency / TECMO_MUSIC_SAMPLE_RATE;
    channel->phase -= floor(channel->phase);
    return (channel->phase < duty[voice->duty_and_timing & 3U] ? 1.0
                                                               : -1.0) *
           ((double)channel->volume / 15.0) * 0.22;
}

static double triangle_sample(TecmoGameplayAudioPlayer *player)
{
    TecmoMusicChannelState *channel = &player->sfx_channels[2];
    double frequency;
    double wave;
    if (!channel->active || !channel->note_on) return 0.0;
    frequency = GAMEPLAY_AUDIO_CPU_CLOCK /
                (32.0 * ((double)channel->period + 1.0));
    channel->phase += frequency / TECMO_MUSIC_SAMPLE_RATE;
    channel->phase -= floor(channel->phase);
    wave = 1.0 - 4.0 * fabs(channel->phase - 0.5);
    return wave * 0.24;
}

static double noise_sample(TecmoGameplayAudioPlayer *player)
{
    static const uint16_t periods[16] = {
        4U, 8U, 16U, 32U, 64U, 96U, 128U, 160U,
        202U, 254U, 380U, 508U, 762U, 1016U, 2034U, 4068U
    };
    TecmoMusicChannelState *channel = &player->sfx_channels[3];
    double steps;
    if (!channel->active || !channel->note_on || channel->volume == 0U)
        return 0.0;
    steps = GAMEPLAY_AUDIO_CPU_CLOCK / periods[channel->period & 0x0FU] /
            TECMO_MUSIC_SAMPLE_RATE;
    channel->noise_phase += steps;
    while (channel->noise_phase >= 1.0) {
        unsigned tap = (channel->period & 0x80U) != 0U ? 6U : 1U;
        uint16_t feedback = (uint16_t)((player->noise_lfsr ^
                                       (player->noise_lfsr >> tap)) &
                                      1U);
        player->noise_lfsr = (uint16_t)((player->noise_lfsr >> 1U) |
                                        (feedback << 14U));
        channel->noise_phase -= 1.0;
    }
    return ((player->noise_lfsr & 1U) != 0U ? -1.0 : 1.0) *
           ((double)channel->volume / 15.0) * 0.18;
}

static uint8_t active_sfx_mask(const TecmoGameplayAudioPlayer *player)
{
    uint8_t mask = 0U;
    unsigned channel;
    for (channel = 0U; channel < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT;
         ++channel) {
        if (player->sfx_channels[channel].active)
            mask |= (uint8_t)(1U << channel);
    }
    return mask;
}

static double render_dmc_sample(TecmoGameplayAudioPlayer *player)
{
    TecmoGameplayDmcState *state = &player->dmc;
    const TecmoGameplayDmcPool *pool;
    uint32_t threshold;
    if (!state->active)
        return ((double)((int)state->output_level - 64) / 64.0) * 0.30;
    pool = &player->asset->dmc_pools[state->pool_index];
    threshold = (uint32_t)TECMO_MUSIC_SAMPLE_RATE * state->period_cycles;
    state->cycle_accumulator += GAMEPLAY_AUDIO_CPU_CLOCK_INTEGER;
    while (state->active && state->cycle_accumulator >= threshold) {
        state->cycle_accumulator -= threshold;
        if (state->bits_remaining == 0U) {
            if (state->byte_index >= state->byte_count) {
                state->active = false;
                break;
            }
            state->shift_register = player->asset->dmc_data[
                pool->data_offset + state->byte_index++];
            state->bits_remaining = 8U;
        }
        if ((state->shift_register & 1U) != 0U) {
            if (state->output_level <= 125U) state->output_level += 2U;
        } else if (state->output_level >= 2U) {
            state->output_level -= 2U;
        }
        state->shift_register >>= 1U;
        --state->bits_remaining;
    }
    return ((double)((int)state->output_level - 64) / 64.0) * 0.30;
}

bool tecmo_gameplay_audio_queue_dmc_clip(TecmoGameplayAudioPlayer *player,
                                         TecmoGameplayDmcClipId clip_id)
{
    static const uint16_t dmc_period_cycles[16] = {
        428U, 380U, 340U, 320U, 286U, 254U, 226U, 214U,
        190U, 160U, 142U, 128U, 106U, 85U, 72U, 54U
    };
    const TecmoGameplayDmcClip *clip;
    uint8_t output_level;
    if (player == NULL || player->asset == NULL ||
        !player->asset->available || clip_id < 0 ||
        (unsigned)clip_id >= TECMO_GAMEPLAY_DMC_CLIP_COUNT)
        return false;
    clip = &player->asset->dmc_clips[(unsigned)clip_id];
    output_level = player->dmc.output_level;
    memset(&player->dmc, 0, sizeof(player->dmc));
    player->dmc.pool_index = clip->pool_index;
    player->dmc.byte_index = clip->pool_offset;
    player->dmc.byte_count = clip->pool_offset + clip->byte_count;
    player->dmc.period_cycles = dmc_period_cycles[clip->rate_index];
    player->dmc.output_level = output_level;
    player->dmc.active = true;
    return true;
}

bool tecmo_gameplay_audio_queue_event(TecmoGameplayAudioPlayer *player,
                                      TecmoGameplayAudioEvent event)
{
    uint8_t sfx_id;
    if (player == NULL || player->asset == NULL ||
        !player->asset->available)
        return false;
    switch (event) {
    case TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER: sfx_id = 3U; break;
    case TECMO_GAMEPLAY_AUDIO_COUNTDOWN: sfx_id = 14U; break;
    case TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE: sfx_id = 11U; break;
    case TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_12: sfx_id = 12U; break;
    case TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_13: sfx_id = 13U; break;
    case TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE:
        return tecmo_gameplay_audio_queue_dmc_clip(
            player, TECMO_GAMEPLAY_DMC_HELD_BALL_DRIBBLE);
    case TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE: sfx_id = 6U; break;
    case TECMO_GAMEPLAY_AUDIO_BANK05_9FEC_CUE: sfx_id = 5U; break;
    default: return false;
    }
    if (find_effect(player->asset, sfx_id) == NULL) return false;
    player->pending_sfx_id = sfx_id;
    player->sfx_pending = true;
    player->sfx_playing = true;
    player->render_guard_failed = false;
    return true;
}

bool tecmo_gameplay_audio_queue_game_music(TecmoGameplayAudioPlayer *player)
{
    return player != NULL && player->music != NULL &&
           tecmo_music_queue_track(player->music,
                                   TECMO_MUSIC_TRACK_GAMEPLAY);
}

bool tecmo_gameplay_audio_queue_pregame_matchup_stinger(
    TecmoGameplayAudioPlayer *player)
{
    return player != NULL && player->music != NULL &&
           tecmo_music_queue_track(
               player->music,
               TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER);
}

void tecmo_gameplay_audio_set_game_music_enabled(
    TecmoGameplayAudioPlayer *player, bool enabled)
{
    if (player != NULL && player->music != NULL)
        tecmo_music_set_game_music_enabled(player->music, enabled);
}

void tecmo_gameplay_audio_stop_all(TecmoGameplayAudioPlayer *player)
{
    uint8_t dmc_output_level;
    if (player == NULL) return;
    dmc_output_level = player->dmc.output_level;
    tecmo_music_stop(player->music);
    memset(player->sfx_channels, 0, sizeof(player->sfx_channels));
    memset(&player->dmc, 0, sizeof(player->dmc));
    player->dmc.output_level = dmc_output_level;
    player->current_sfx_id = 0U;
    player->pending_sfx_id = 0U;
    player->sfx_pending = false;
    player->sfx_playing = false;
    player->render_guard_failed = false;
}

void tecmo_gameplay_audio_render_samples(TecmoGameplayAudioPlayer *player,
                                         int16_t *samples,
                                         size_t sample_count)
{
    uint64_t tick_threshold;
    size_t index;
    if (player == NULL || player->asset == NULL ||
        !player->asset->available) {
        if (samples != NULL)
            memset(samples, 0, sample_count * sizeof(*samples));
        return;
    }
    tick_threshold = (uint64_t)TECMO_MUSIC_SAMPLE_RATE *
                     TECMO_MUSIC_TICK_DENOMINATOR;
    for (index = 0U; index < sample_count; ++index) {
        uint8_t override_mask = active_sfx_mask(player);
        double mixed =
            (double)tecmo_music_render_sample_with_overrides(
                player->music, override_mask) /
            28000.0;
        int value;
        mixed += pulse_sample(player, 0U) + pulse_sample(player, 1U) +
                 triangle_sample(player) + noise_sample(player) +
                 render_dmc_sample(player);
        if (mixed > 1.0) mixed = 1.0;
        if (mixed < -1.0) mixed = -1.0;
        value = (int)(mixed * 28000.0);
        if (samples != NULL) samples[index] = (int16_t)value;
        player->sample_tick_accumulator += TECMO_MUSIC_TICK_NUMERATOR;
        while (player->sample_tick_accumulator >= tick_threshold) {
            player->sample_tick_accumulator -= tick_threshold;
            sfx_tick(player);
        }
    }
}

static uint32_t hash_state(const TecmoGameplayAudioPlayer *player)
{
    uint32_t hash = 2166136261U;
    unsigned channel;
    for (channel = 0U; channel < TECMO_GAMEPLAY_SFX_CHANNEL_COUNT;
         ++channel) {
        const TecmoMusicChannelState *state =
            &player->sfx_channels[channel];
        uint32_t values[4] = {
            state->pc,
            state->duration_ticks,
            state->period,
            (uint32_t)state->volume |
                ((uint32_t)state->active << 16U) |
                ((uint32_t)state->note_on << 17U)
        };
        unsigned value;
        for (value = 0U; value < 4U; ++value) {
            unsigned byte_index;
            for (byte_index = 0U; byte_index < 4U; ++byte_index) {
                hash ^= (uint8_t)(values[value] >> (byte_index * 8U));
                hash *= 16777619U;
            }
        }
    }
    hash ^= player->dmc.output_level;
    hash *= 16777619U;
    hash ^= (uint8_t)player->dmc.active;
    hash *= 16777619U;
    return hash;
}

bool tecmo_gameplay_audio_self_test(const char *project_root,
                                    char *message, size_t message_size)
{
    TecmoGameplayAudioAsset asset;
    TecmoMusicAsset music_asset;
    TecmoMusicPlayer music_a;
    TecmoMusicPlayer music_b;
    TecmoGameplayAudioPlayer player;
    TecmoGameplayAudioPlayer control;
    TecmoGameplayAudioPlayer dmc_control;
    int16_t samples[TECMO_MUSIC_SAMPLE_RATE];
    int16_t held_samples[2];
    uint32_t pcm_hash;
    uint32_t state_hash;
    bool override_ok;
    bool gate_ok;
    bool cadence_ok;
    bool dmc_ok;
    bool mailbox_ok;
    bool cross_pack_ok;
    bool event_map_ok;
    bool clear_ok;
    bool dmc_continuity_ok;
    bool ok = false;
    memset(&asset, 0, sizeof(asset));
    memset(&music_asset, 0, sizeof(music_asset));
    if (!tecmo_gameplay_audio_asset_load(&asset, project_root) ||
        !tecmo_music_asset_load(&music_asset, project_root))
        goto cleanup;
    tecmo_music_player_init(&music_a, &music_asset);
    tecmo_music_player_init(&music_b, &music_asset);
    tecmo_gameplay_audio_player_init(&player, &asset, &music_a);
    tecmo_gameplay_audio_player_init(&control, &asset, &music_b);
    tecmo_gameplay_audio_player_init(&dmc_control, &asset, NULL);
    event_map_ok = tecmo_gameplay_audio_queue_event(
                       &player, TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE) &&
                   player.pending_sfx_id == 6U &&
                   tecmo_gameplay_audio_queue_event(
                       &player, TECMO_GAMEPLAY_AUDIO_BANK05_9FEC_CUE) &&
                   player.pending_sfx_id == 5U;
    if (!tecmo_gameplay_audio_queue_game_music(&player) ||
        !tecmo_gameplay_audio_queue_game_music(&control) ||
        !tecmo_gameplay_audio_queue_event(
            &player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE))
        goto cleanup;
    tecmo_gameplay_audio_render_samples(
        &player, samples, TECMO_MUSIC_SAMPLE_RATE);
    tecmo_gameplay_audio_render_samples(
        &control, NULL, TECMO_MUSIC_SAMPLE_RATE);
    pcm_hash = fnv1a32((const uint8_t *)samples, sizeof(samples));
    state_hash = hash_state(&player);
    override_ok = music_a.ticks_elapsed == music_b.ticks_elapsed &&
                  music_a.sample_tick_accumulator ==
                      music_b.sample_tick_accumulator &&
                  music_a.noise_lfsr == music_b.noise_lfsr &&
                  memcmp(music_a.channels, music_b.channels,
                         sizeof(music_a.channels)) == 0;
    cadence_ok = player.ticks_elapsed == music_a.ticks_elapsed &&
                 player.ticks_elapsed == 60U;
    tecmo_gameplay_audio_set_game_music_enabled(&player, false);
    gate_ok = !tecmo_gameplay_audio_queue_game_music(&player) &&
              tecmo_gameplay_audio_queue_pregame_matchup_stinger(&player);
    mailbox_ok = tecmo_gameplay_audio_queue_event(
                     &player, TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER) &&
                 tecmo_gameplay_audio_queue_event(
                     &player, TECMO_GAMEPLAY_AUDIO_COUNTDOWN) &&
                 player.pending_sfx_id == 14U;
    cross_pack_ok = revision_tokens_match(asset.revision_token,
                                           0x0650F5B0U) &&
                    !revision_tokens_match(asset.revision_token,
                                           asset.revision_token ^ 1U);
    dmc_ok = tecmo_gameplay_audio_queue_event(
                 &player, TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE) &&
             tecmo_gameplay_audio_queue_event(
                 &player, TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER);
    tecmo_gameplay_audio_render_samples(&player, NULL, 512U);
    dmc_ok = dmc_ok && player.dmc.active && player.sfx_pending;
    dmc_control.dmc.output_level = 90U;
    dmc_continuity_ok = tecmo_gameplay_audio_queue_dmc_clip(
                            &dmc_control,
                            TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT) &&
                        dmc_control.dmc.output_level == 90U;
    tecmo_gameplay_audio_render_samples(&dmc_control, NULL, 128U);
    if (dmc_continuity_ok) {
        uint8_t retrigger_level = dmc_control.dmc.output_level;
        dmc_continuity_ok = dmc_control.dmc.active &&
            tecmo_gameplay_audio_queue_dmc_clip(
                &dmc_control, TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT) &&
            dmc_control.dmc.output_level == retrigger_level &&
            dmc_control.dmc.cycle_accumulator == 0U &&
            dmc_control.dmc.bits_remaining == 0U;
    }
    tecmo_gameplay_audio_render_samples(&dmc_control, NULL, 4096U);
    if (dmc_continuity_ok) {
        uint8_t held_level = dmc_control.dmc.output_level;
        int held_value = (int)(
            ((double)((int)held_level - 64) / 64.0) * 0.30 * 28000.0);
        tecmo_gameplay_audio_render_samples(
            &dmc_control, held_samples, 2U);
        dmc_continuity_ok = !dmc_control.dmc.active &&
            held_level != 64U && held_samples[0] == held_value &&
            held_samples[1] == held_value &&
            tecmo_gameplay_audio_queue_dmc_clip(
                &dmc_control, TECMO_GAMEPLAY_DMC_BANK05_ABF5) &&
            dmc_control.dmc.output_level == held_level;
    }
    tecmo_gameplay_audio_render_samples(&dmc_control, NULL, 64U);
    if (dmc_continuity_ok) {
        uint8_t chained_level = dmc_control.dmc.output_level;
        dmc_continuity_ok = dmc_control.dmc.active &&
            tecmo_gameplay_audio_queue_dmc_clip(
                &dmc_control, TECMO_GAMEPLAY_DMC_BANK05_A9C5) &&
            dmc_control.dmc.output_level == chained_level;
        tecmo_gameplay_audio_stop_all(&dmc_control);
        dmc_continuity_ok = dmc_continuity_ok &&
            !dmc_control.dmc.active &&
            dmc_control.dmc.output_level == chained_level;
    }
    clear_ok = tecmo_gameplay_audio_queue_event(
                   &control, TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE) &&
               tecmo_gameplay_audio_queue_dmc_clip(
                   &control, TECMO_GAMEPLAY_DMC_BANK05_ABF5);
    tecmo_gameplay_audio_stop_all(&control);
    clear_ok = clear_ok && !control.sfx_pending && !control.sfx_playing &&
               !control.dmc.active && !music_b.playing &&
               !music_b.track_pending;
    ok = override_ok && gate_ok && cadence_ok && dmc_ok && mailbox_ok &&
         event_map_ok && dmc_continuity_ok &&
         clear_ok &&
         cross_pack_ok &&
         !player.render_guard_failed;
    if (message != NULL && message_size > 0U) {
        (void)snprintf(
            message, message_size,
            "TSFX-1/TDMC-1 gameplay audio: sfx=%08X dmc=%08X pcm=%08X state=%08X instructions=%u voices=%u events=%s override=%s cadence=%s gate=%s mailbox=%s independent=%s dmc-continuity=%s clear=%s crosspack=%s",
            asset.sfx_payload_fingerprint,
            asset.dmc_payload_fingerprint, pcm_hash, state_hash,
            TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT,
            TECMO_GAMEPLAY_SFX_VOICE_COUNT,
            event_map_ok ? "pass" : "fail",
            override_ok ? "pass" : "fail",
            cadence_ok ? "pass" : "fail", gate_ok ? "pass" : "fail",
            mailbox_ok ? "pass" : "fail", dmc_ok ? "pass" : "fail",
            dmc_continuity_ok ? "pass" : "fail",
            clear_ok ? "pass" : "fail",
            cross_pack_ok ? "pass" : "fail");
    }

cleanup:
    tecmo_gameplay_audio_asset_shutdown(&asset);
    tecmo_music_asset_shutdown(&music_asset);
    if (!ok && message != NULL && message_size > 0U && message[0] == '\0') {
        (void)snprintf(message, message_size,
                       "TSFX-1/TDMC-1 gameplay audio self-test failed");
    }
    return ok;
}
