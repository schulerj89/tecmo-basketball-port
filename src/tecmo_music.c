#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_music.h"

#include "tecmo_asset_pack.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUSIC_ENTRY_ID "audio/music"
#define MUSIC_HEADER_SIZE 128U
#define MUSIC_TRACK_OFFSET 128U
#define MUSIC_TRACK_STRIDE 48U
#define MUSIC_VOICE_STRIDE 8U
#define MUSIC_INSTRUCTION_STRIDE 16U
#define MUSIC_PAYLOAD_FNV1A32 0x05C00ECBU
#define MUSIC_CPU_CLOCK 1789773.0
#define MUSIC_INVALID_INSTRUCTION UINT32_MAX

_Static_assert(TECMO_MUSIC_TICK_NUMERATOR == 39375000U,
               "native music must use the exact NTSC tick numerator");
_Static_assert(TECMO_MUSIC_TICK_DENOMINATOR == 655171U,
               "native music must use the exact NTSC tick denominator");

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
    size_t i;
    for (i = 0U; i < count; ++i) {
        hash ^= bytes[i];
        hash *= 16777619U;
    }
    return hash;
}

static int make_path(char *path, size_t size, const char *root,
                     const char *suffix)
{
    size_t n;
    int written;
    if (path == NULL || size == 0U || root == NULL || root[0] == '\0') return -1;
    n = strlen(root);
    written = snprintf(path, size, "%s%s%s", root,
                       root[n - 1U] == '\\' || root[n - 1U] == '/' ? "" : "\\",
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
    size_t i;
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
    for (i = 0U; i < count; ++i) {
        int written;
        if (!file_exists(paths[i])) continue;
        written = snprintf(selected, size, "%s", paths[i]);
        return written >= 0 && (size_t)written < size;
    }
    return false;
}

static bool instruction_in_channel(uint32_t value, uint32_t first,
                                   uint32_t count)
{
    return value >= first && value - first < count;
}

static bool validate_walk(const TecmoMusicAsset *asset,
                          uint32_t instruction_index,
                          uint32_t first,
                          uint32_t count,
                          unsigned depth,
                          uint32_t path[TECMO_MUSIC_CALL_DEPTH],
                          uint8_t *visited,
                          uint8_t *reachable,
                          unsigned *max_depth)
{
    const TecmoMusicInstruction *instruction;
    size_t row_offset;
    unsigned i;
    if (depth > TECMO_MUSIC_CALL_DEPTH ||
        !instruction_in_channel(instruction_index, first, count))
        return false;
    row_offset = (size_t)depth * asset->instruction_count + instruction_index;
    if (visited[row_offset] != 0U) return true;
    visited[row_offset] = 1U;
    reachable[instruction_index] = 1U;
    if (depth > *max_depth) *max_depth = depth;
    instruction = &asset->instructions[instruction_index];
    if (instruction->type == TECMO_MUSIC_CALL) {
        if (depth >= TECMO_MUSIC_CALL_DEPTH) return false;
        for (i = 0U; i < depth; ++i)
            if (path[i] == instruction->target) return false;
        path[depth] = instruction->target;
        if (!validate_walk(asset, instruction->target, first, count,
                           depth + 1U, path, visited, reachable,
                           max_depth))
            return false;
    } else if (instruction->type == TECMO_MUSIC_LOOP) {
        if (!validate_walk(asset, instruction->target, first, count,
                           depth, path, visited, reachable, max_depth))
            return false;
    }
    if (instruction->next != MUSIC_INVALID_INSTRUCTION)
        return validate_walk(asset, instruction->next, first, count,
                             depth, path, visited, reachable, max_depth);
    return instruction->type == TECMO_MUSIC_END ||
           instruction->type == TECMO_MUSIC_RETURN;
}

static bool parse_payload(TecmoMusicAsset *asset, const uint8_t *bytes,
                          uint64_t count, bool enforce_payload_fingerprint)
{
    static const uint32_t track_fingerprints[TECMO_MUSIC_TRACK_COUNT] = {
        0x1270498BU, 0xBD91FCF1U, 0x69F85EC2U, 0x8122C6CFU
    };
    static const uint8_t track_ids[TECMO_MUSIC_TRACK_COUNT] = {
        TECMO_MUSIC_TRACK_GAMEPLAY,
        TECMO_MUSIC_TRACK_PRESENTATION,
        TECMO_MUSIC_TRACK_OPENING,
        TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER
    };
    uint32_t track_offset;
    uint32_t voice_offset;
    uint32_t pitch_offset;
    uint32_t instruction_offset;
    uint32_t expected_first = 0U;
    uint8_t *visited = NULL;
    uint8_t *reachable = NULL;
    uint32_t instruction_index;
    unsigned track_index;
    unsigned max_depth = 0U;
    bool ok = false;

    if (asset == NULL || bytes == NULL || count != TECMO_MUSIC_PAYLOAD_SIZE ||
        (enforce_payload_fingerprint &&
         fnv1a32(bytes, (size_t)count) != MUSIC_PAYLOAD_FNV1A32) ||
        memcmp(bytes, "TMUS", 4U) != 0 || read_u16(bytes + 4U) != 1U ||
        read_u16(bytes + 6U) != MUSIC_HEADER_SIZE ||
        read_u32(bytes + 8U) != TECMO_MUSIC_PAYLOAD_SIZE ||
        read_u32(bytes + 12U) != 0x06F2A750U ||
        read_u32(bytes + 16U) != 0xFC6A0BC1U ||
        read_u32(bytes + 20U) != 0x59366EC4U ||
        read_u32(bytes + 24U) != 0x3F5A394DU ||
        read_u16(bytes + 28U) != TECMO_MUSIC_TRACK_COUNT ||
        read_u16(bytes + 30U) != TECMO_MUSIC_CHANNEL_COUNT ||
        read_u16(bytes + 32U) != TECMO_MUSIC_VOICE_COUNT ||
        read_u16(bytes + 34U) != TECMO_MUSIC_PITCH_COUNT ||
        read_u16(bytes + 36U) != MUSIC_TRACK_STRIDE ||
        read_u16(bytes + 38U) != MUSIC_VOICE_STRIDE ||
        read_u16(bytes + 40U) != MUSIC_INSTRUCTION_STRIDE ||
        read_u16(bytes + 42U) > TECMO_MUSIC_CALL_DEPTH ||
        read_u32(bytes + 44U) != TECMO_MUSIC_SAMPLE_RATE ||
        read_u32(bytes + 48U) != TECMO_MUSIC_TICK_NUMERATOR ||
        read_u32(bytes + 52U) != TECMO_MUSIC_TICK_DENOMINATOR ||
        read_u32(bytes + 72U) != TECMO_MUSIC_INSTRUCTION_COUNT ||
        read_u16(bytes + 76U) != TECMO_MUSIC_LOOP_COUNT ||
        read_u16(bytes + 78U) != 0U)
        return false;
    for (instruction_index = 100U; instruction_index < MUSIC_HEADER_SIZE;
         ++instruction_index)
        if (bytes[instruction_index] != 0U) return false;
    track_offset = read_u32(bytes + 56U);
    voice_offset = read_u32(bytes + 60U);
    pitch_offset = read_u32(bytes + 64U);
    instruction_offset = read_u32(bytes + 68U);
    if (track_offset != MUSIC_TRACK_OFFSET ||
        voice_offset != track_offset + TECMO_MUSIC_TRACK_COUNT * MUSIC_TRACK_STRIDE ||
        pitch_offset != voice_offset + TECMO_MUSIC_VOICE_COUNT * MUSIC_VOICE_STRIDE ||
        instruction_offset != 768U ||
        (uint64_t)instruction_offset +
            (uint64_t)TECMO_MUSIC_INSTRUCTION_COUNT * MUSIC_INSTRUCTION_STRIDE != count)
        return false;

    asset->instructions = (TecmoMusicInstruction *)calloc(
        TECMO_MUSIC_INSTRUCTION_COUNT, sizeof(*asset->instructions));
    if (asset->instructions == NULL) return false;
    asset->instruction_count = TECMO_MUSIC_INSTRUCTION_COUNT;
    asset->sample_rate = read_u32(bytes + 44U);
    asset->tick_numerator = read_u32(bytes + 48U);
    asset->tick_denominator = read_u32(bytes + 52U);
    asset->payload_fingerprint = MUSIC_PAYLOAD_FNV1A32;

    for (track_index = 0U; track_index < TECMO_MUSIC_TRACK_COUNT; ++track_index) {
        const uint8_t *source = bytes + track_offset + track_index * MUSIC_TRACK_STRIDE;
        unsigned channel;
        if (bytes[80U + track_index * 4U] !=
                (uint8_t)(track_fingerprints[track_index] & 0xFFU) ||
            read_u32(bytes + 80U + track_index * 4U) != track_fingerprints[track_index] ||
            bytes[96U + track_index] != track_ids[track_index] ||
            source[0] != track_ids[track_index] || source[1] != 0U ||
            read_u16(source + 2U) != TECMO_MUSIC_CHANNEL_COUNT ||
            read_u32(source + 4U) != track_fingerprints[track_index])
            goto cleanup;
        for (instruction_index = 40U; instruction_index < MUSIC_TRACK_STRIDE;
             ++instruction_index)
            if (source[instruction_index] != 0U) goto cleanup;
        asset->tracks[track_index].id = source[0];
        asset->tracks[track_index].source_fingerprint = read_u32(source + 4U);
        for (channel = 0U; channel < TECMO_MUSIC_CHANNEL_COUNT; ++channel) {
            uint32_t first = read_u32(source + 8U + channel * 8U);
            uint32_t channel_count = read_u32(source + 12U + channel * 8U);
            if (first != expected_first || channel_count == 0U ||
                channel_count > TECMO_MUSIC_INSTRUCTION_COUNT - expected_first)
                goto cleanup;
            asset->tracks[track_index].channels[channel].first_instruction = first;
            asset->tracks[track_index].channels[channel].instruction_count = channel_count;
            expected_first += channel_count;
        }
    }
    if (expected_first != TECMO_MUSIC_INSTRUCTION_COUNT) goto cleanup;

    for (track_index = 0U; track_index < TECMO_MUSIC_VOICE_COUNT; ++track_index) {
        const uint8_t *source = bytes + voice_offset + track_index * MUSIC_VOICE_STRIDE;
        TecmoMusicVoice *voice = &asset->voices[track_index];
        voice->duty_and_timing = source[0];
        voice->initial_volume = source[1];
        voice->sweep = source[2];
        voice->attack = source[3];
        voice->decay = source[4];
        voice->sustain_ticks = source[5];
        voice->release = source[6];
        voice->peak_and_sustain_volume = source[7];
        if (voice->sweep != 0U || (voice->initial_volume & 0xF0U) != 0U)
            goto cleanup;
    }
    for (track_index = 0U; track_index < TECMO_MUSIC_PITCH_COUNT; ++track_index) {
        uint16_t period = read_u16(bytes + pitch_offset + track_index * 2U);
        if (period == 0U || period > 0x7FFU ||
            (track_index > 0U && period >= asset->pitch_periods[track_index - 1U]))
            goto cleanup;
        asset->pitch_periods[track_index] = period;
    }
    for (instruction_index = 0U;
         instruction_index < TECMO_MUSIC_INSTRUCTION_COUNT;
         ++instruction_index) {
        const uint8_t *source = bytes + instruction_offset +
            instruction_index * MUSIC_INSTRUCTION_STRIDE;
        TecmoMusicInstruction *instruction = &asset->instructions[instruction_index];
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

    visited = (uint8_t *)calloc((TECMO_MUSIC_CALL_DEPTH + 1U) *
                                TECMO_MUSIC_INSTRUCTION_COUNT, 1U);
    reachable = (uint8_t *)calloc(TECMO_MUSIC_INSTRUCTION_COUNT, 1U);
    if (visited == NULL || reachable == NULL) goto cleanup;
    for (track_index = 0U; track_index < TECMO_MUSIC_TRACK_COUNT; ++track_index) {
        unsigned channel;
        for (channel = 0U; channel < TECMO_MUSIC_CHANNEL_COUNT; ++channel) {
            const TecmoMusicChannelProgram *program =
                &asset->tracks[track_index].channels[channel];
            uint32_t path[TECMO_MUSIC_CALL_DEPTH];
            uint32_t end = program->first_instruction + program->instruction_count;
            uint32_t index;
            memset(path, 0, sizeof(path));
            for (index = program->first_instruction; index < end; ++index) {
                const TecmoMusicInstruction *instruction = &asset->instructions[index];
                bool has_next = instruction->next != MUSIC_INVALID_INSTRUCTION;
                bool has_target = instruction->target != MUSIC_INVALID_INSTRUCTION;
                if ((has_next && !instruction_in_channel(instruction->next,
                                                         program->first_instruction,
                                                         program->instruction_count)) ||
                    (has_target && !instruction_in_channel(instruction->target,
                                                           program->first_instruction,
                                                           program->instruction_count)))
                    goto cleanup;
                switch (instruction->type) {
                case TECMO_MUSIC_NOTE:
                    if (!has_next || has_target || instruction->value16 == 0U ||
                        (channel != 3U && (uint16_t)(instruction->value8 +
                            (channel == 2U ? 12U : 0U)) >= TECMO_MUSIC_PITCH_COUNT) ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX) goto cleanup;
                    break;
                case TECMO_MUSIC_SET_VOICE:
                    if (!has_next || has_target || instruction->value8 >= TECMO_MUSIC_VOICE_COUNT ||
                        instruction->value16 != 0U || instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX) goto cleanup;
                    break;
                case TECMO_MUSIC_LEGATO:
                case TECMO_MUSIC_BIND_PHRASES:
                    if (!has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U || instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX) goto cleanup;
                    break;
                case TECMO_MUSIC_PITCH_DELTA:
                    if (!has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U ||
                        instruction->loop_slot != UINT16_MAX ||
                        instruction->signed_value < -128 || instruction->signed_value > 127)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_REST:
                    if (!has_next || has_target || instruction->value16 == 0U ||
                        instruction->value16 > 127U || instruction->value8 != 0U ||
                        instruction->signed_value != 0 || instruction->loop_slot != UINT16_MAX)
                        goto cleanup;
                    break;
                case TECMO_MUSIC_LOOP:
                    if (!has_next || !has_target || instruction->value16 == 0U ||
                        instruction->value16 > 256U || instruction->value8 != 0U ||
                        instruction->signed_value != 0 ||
                        instruction->loop_slot >= TECMO_MUSIC_LOOP_COUNT) goto cleanup;
                    break;
                case TECMO_MUSIC_CALL:
                    if (!has_next || !has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U || instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX) goto cleanup;
                    break;
                case TECMO_MUSIC_END:
                case TECMO_MUSIC_RETURN:
                    if (has_next || has_target || instruction->value8 != 0U ||
                        instruction->value16 != 0U || instruction->signed_value != 0 ||
                        instruction->loop_slot != UINT16_MAX) goto cleanup;
                    break;
                default: goto cleanup;
                }
            }
            if (!validate_walk(asset, program->first_instruction,
                               program->first_instruction,
                               program->instruction_count, 0U, path,
                               visited, reachable, &max_depth))
                goto cleanup;
        }
    }
    if (max_depth != read_u16(bytes + 42U)) goto cleanup;
    for (instruction_index = 0U;
         instruction_index < TECMO_MUSIC_INSTRUCTION_COUNT;
         ++instruction_index)
        if (reachable[instruction_index] == 0U) goto cleanup;
    ok = true;

cleanup:
    free(visited);
    free(reachable);
    if (!ok) {
        free(asset->instructions);
        asset->instructions = NULL;
        asset->instruction_count = 0U;
    }
    return ok;
}

bool tecmo_music_asset_load(TecmoMusicAsset *asset, const char *project_root)
{
    char pack_path[1024];
    uint8_t *bytes = NULL;
    uint64_t count = 0U;
    bool ok;
    if (asset == NULL) return false;
    memset(asset, 0, sizeof(*asset));
    if (!select_asset_pack(project_root, pack_path, sizeof(pack_path)) ||
        tecmo_asset_pack_read_entry_exact(pack_path, MUSIC_ENTRY_ID,
                                          TECMO_MUSIC_PAYLOAD_SIZE,
                                          &bytes, &count) != 0) {
        (void)snprintf(asset->status, sizeof(asset->status),
                       "TMUS-1 audio/music entry unavailable");
        return false;
    }
    ok = parse_payload(asset, bytes, count, true);
    tecmo_asset_pack_free(bytes);
    asset->available = ok;
    (void)snprintf(asset->status, sizeof(asset->status), "%s",
                   ok ? "TMUS-1 native ROM music" :
                        "TMUS-1 asset contract rejected");
    return ok;
}

void tecmo_music_asset_shutdown(TecmoMusicAsset *asset)
{
    if (asset == NULL) return;
    free(asset->instructions);
    asset->instructions = NULL;
    asset->instruction_count = 0U;
    asset->available = false;
}

void tecmo_music_player_init(TecmoMusicPlayer *player,
                             const TecmoMusicAsset *asset)
{
    if (player == NULL) return;
    memset(player, 0, sizeof(*player));
    player->asset = asset;
    player->noise_lfsr = 1U;
    player->game_music_enabled = true;
}

static const TecmoMusicTrack *find_track(const TecmoMusicAsset *asset,
                                         uint8_t track_id)
{
    unsigned i;
    if (asset == NULL || !asset->available) return NULL;
    for (i = 0U; i < TECMO_MUSIC_TRACK_COUNT; ++i)
        if (asset->tracks[i].id == track_id) return &asset->tracks[i];
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
                ? (uint8_t)(channel->volume - amount) : 0U;
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
                ? (uint8_t)(channel->volume - amount) : 0U;
        }
        return;
    }
}

static void update_period(TecmoMusicPlayer *player, unsigned channel_index)
{
    TecmoMusicChannelState *channel = &player->channels[channel_index];
    int pitch_index;
    int period;
    if (channel_index == 3U) {
        channel->period = channel->note;
        return;
    }
    pitch_index = channel->note + (channel_index == 2U ? 12 : 0);
    if (pitch_index < 0 || pitch_index >= TECMO_MUSIC_PITCH_COUNT) {
        player->render_guard_failed = true;
        channel->note_on = false;
        return;
    }
    period = (int)player->asset->pitch_periods[pitch_index] - channel->pitch_delta;
    if (period < 0) period = 0;
    if (period > 0x7FF) period = 0x7FF;
    channel->period = (uint16_t)period;
}

static void process_channel(TecmoMusicPlayer *player, unsigned channel_index)
{
    TecmoMusicChannelState *channel = &player->channels[channel_index];
    unsigned guard;
    for (guard = 0U; guard < 4096U && channel->active; ++guard) {
        const TecmoMusicInstruction *instruction;
        if (channel->pc >= player->asset->instruction_count) break;
        instruction = &player->asset->instructions[channel->pc];
        switch (instruction->type) {
        case TECMO_MUSIC_NOTE:
            channel->note = instruction->value8;
            channel->duration_ticks = instruction->value16;
            channel->note_on = true;
            update_period(player, channel_index);
            if (!channel->legato_next && channel_index != 2U) {
                if (channel->voice_index >= TECMO_MUSIC_VOICE_COUNT) break;
                reset_envelope(channel, &player->asset->voices[channel->voice_index]);
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
            uint16_t *remaining = &channel->loop_remaining[instruction->loop_slot];
            if (*remaining == 0U) *remaining = instruction->value16;
            --*remaining;
            channel->pc = *remaining != 0U ? instruction->target : instruction->next;
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

static void music_tick(TecmoMusicPlayer *player)
{
    unsigned channel_index;
    bool any_active = false;
    if (player == NULL || player->asset == NULL || !player->asset->available) return;
    ++player->ticks_elapsed;
    if (player->track_pending) {
        const TecmoMusicTrack *track = find_track(player->asset,
                                                  player->pending_track_id);
        player->track_pending = false;
        if (track == NULL) {
            player->playing = false;
            player->render_guard_failed = true;
            return;
        }
        memset(player->channels, 0, sizeof(player->channels));
        for (channel_index = 0U; channel_index < TECMO_MUSIC_CHANNEL_COUNT;
             ++channel_index) {
            player->channels[channel_index].pc =
                track->channels[channel_index].first_instruction;
            player->channels[channel_index].active = true;
            process_channel(player, channel_index);
        }
        player->current_track_id = track->id;
    }
    for (channel_index = 0U; channel_index < TECMO_MUSIC_CHANNEL_COUNT;
         ++channel_index) {
        TecmoMusicChannelState *channel = &player->channels[channel_index];
        if (!channel->active) continue;
        if (channel->duration_ticks > 0U) {
            --channel->duration_ticks;
            if (channel->duration_ticks > 0U) {
                if (channel_index != 2U && channel->voice_index < TECMO_MUSIC_VOICE_COUNT)
                    tick_envelope(channel,
                                  &player->asset->voices[channel->voice_index]);
            } else {
                process_channel(player, channel_index);
            }
        } else {
            process_channel(player, channel_index);
        }
        if (channel->active) any_active = true;
    }
    player->playing = any_active;
}

bool tecmo_music_queue_track(TecmoMusicPlayer *player, uint8_t track_id)
{
    if (player == NULL || player->asset == NULL) return false;
    if (track_id == TECMO_MUSIC_TRACK_GAMEPLAY &&
        !player->game_music_enabled) return false;
    if (find_track(player->asset, track_id) == NULL) return false;
    player->pending_track_id = track_id;
    player->track_pending = true;
    player->playing = true;
    player->render_guard_failed = false;
    return true;
}

bool tecmo_music_queue_opening_once(TecmoMusicPlayer *player)
{
    if (player == NULL || player->opening_queued) return false;
    player->opening_queued = true;
    return tecmo_music_queue_track(player, TECMO_MUSIC_TRACK_OPENING);
}

void tecmo_music_set_game_music_enabled(TecmoMusicPlayer *player,
                                        bool enabled)
{
    if (player == NULL) return;
    player->game_music_enabled = enabled;
}

void tecmo_music_stop(TecmoMusicPlayer *player)
{
    if (player == NULL) return;
    memset(player->channels, 0, sizeof(player->channels));
    player->current_track_id = 0U;
    player->pending_track_id = 0U;
    player->playing = false;
    player->track_pending = false;
    player->render_guard_failed = false;
}

static double pulse_sample(TecmoMusicPlayer *player, unsigned channel_index)
{
    static const double duty[4] = {0.125, 0.25, 0.5, 0.75};
    TecmoMusicChannelState *channel = &player->channels[channel_index];
    const TecmoMusicVoice *voice;
    double frequency;
    if (!channel->active || !channel->note_on || channel->volume == 0U ||
        channel->voice_index >= TECMO_MUSIC_VOICE_COUNT) return 0.0;
    voice = &player->asset->voices[channel->voice_index];
    frequency = MUSIC_CPU_CLOCK / (16.0 * ((double)channel->period + 1.0));
    channel->phase += frequency / TECMO_MUSIC_SAMPLE_RATE;
    channel->phase -= floor(channel->phase);
    return (channel->phase < duty[voice->duty_and_timing & 3U] ? 1.0 : -1.0) *
           ((double)channel->volume / 15.0) * 0.22;
}

static double triangle_sample(TecmoMusicPlayer *player)
{
    TecmoMusicChannelState *channel = &player->channels[2];
    double frequency;
    double wave;
    if (!channel->active || !channel->note_on) return 0.0;
    frequency = MUSIC_CPU_CLOCK / (32.0 * ((double)channel->period + 1.0));
    channel->phase += frequency / TECMO_MUSIC_SAMPLE_RATE;
    channel->phase -= floor(channel->phase);
    wave = 1.0 - 4.0 * fabs(channel->phase - 0.5);
    return wave * 0.24;
}

static double noise_sample(TecmoMusicPlayer *player)
{
    static const uint16_t periods[16] = {
        4U, 8U, 16U, 32U, 64U, 96U, 128U, 160U,
        202U, 254U, 380U, 508U, 762U, 1016U, 2034U, 4068U
    };
    TecmoMusicChannelState *channel = &player->channels[3];
    double steps;
    if (!channel->active || !channel->note_on || channel->volume == 0U)
        return 0.0;
    steps = MUSIC_CPU_CLOCK / periods[channel->period & 0x0FU] /
            TECMO_MUSIC_SAMPLE_RATE;
    channel->noise_phase += steps;
    while (channel->noise_phase >= 1.0) {
        unsigned tap = (channel->period & 0x80U) != 0U ? 6U : 1U;
        uint16_t feedback = (uint16_t)((player->noise_lfsr ^
                                       (player->noise_lfsr >> tap)) & 1U);
        player->noise_lfsr = (uint16_t)((player->noise_lfsr >> 1U) |
                                        (feedback << 14U));
        channel->noise_phase -= 1.0;
    }
    return ((player->noise_lfsr & 1U) != 0U ? -1.0 : 1.0) *
           ((double)channel->volume / 15.0) * 0.18;
}

void tecmo_music_render_samples(TecmoMusicPlayer *player,
                                int16_t *samples,
                                size_t sample_count)
{
    size_t i;
    if (player == NULL || player->asset == NULL || !player->asset->available) {
        if (samples != NULL)
            memset(samples, 0, sample_count * sizeof(*samples));
        return;
    }
    for (i = 0U; i < sample_count; ++i) {
        int16_t value = tecmo_music_render_sample_with_overrides(player, 0U);
        if (samples != NULL) samples[i] = value;
    }
}

int16_t tecmo_music_render_sample_with_overrides(TecmoMusicPlayer *player,
                                                 uint8_t channel_mask)
{
    uint64_t tick_threshold;
    double channels[TECMO_MUSIC_CHANNEL_COUNT];
    double mixed = 0.0;
    unsigned channel;
    int value;
    if (player == NULL || player->asset == NULL || !player->asset->available)
        return 0;
    channels[0] = pulse_sample(player, 0U);
    channels[1] = pulse_sample(player, 1U);
    channels[2] = triangle_sample(player);
    channels[3] = noise_sample(player);
    for (channel = 0U; channel < TECMO_MUSIC_CHANNEL_COUNT; ++channel) {
        if ((channel_mask & (uint8_t)(1U << channel)) == 0U)
            mixed += channels[channel];
    }
    if (mixed > 1.0) mixed = 1.0;
    if (mixed < -1.0) mixed = -1.0;
    value = (int)(mixed * 28000.0);
    tick_threshold = (uint64_t)player->asset->sample_rate *
                     player->asset->tick_denominator;
    player->sample_tick_accumulator += player->asset->tick_numerator;
    while (player->sample_tick_accumulator >= tick_threshold) {
        player->sample_tick_accumulator -= tick_threshold;
        music_tick(player);
    }
    return (int16_t)value;
}

static uint32_t hash_channel_state(const TecmoMusicPlayer *player)
{
    uint32_t hash = 2166136261U;
    unsigned channel;
    for (channel = 0U; channel < TECMO_MUSIC_CHANNEL_COUNT; ++channel) {
        const TecmoMusicChannelState *state = &player->channels[channel];
        uint32_t values[5] = {
            state->pc, state->duration_ticks, state->period,
            (uint32_t)state->pitch_delta,
            (uint32_t)state->volume | ((uint32_t)state->envelope_phase << 8U) |
                ((uint32_t)state->active << 16U) | ((uint32_t)state->note_on << 17U)
        };
        unsigned value;
        for (value = 0U; value < 5U; ++value) {
            unsigned byte_index;
            for (byte_index = 0U; byte_index < 4U; ++byte_index) {
                hash ^= (uint8_t)(values[value] >> (byte_index * 8U));
                hash *= 16777619U;
            }
        }
    }
    return hash;
}

static uint32_t oscillator_hash(const TecmoMusicAsset *asset, unsigned kind)
{
    TecmoMusicPlayer player;
    int16_t samples[2048];
    size_t i;
    tecmo_music_player_init(&player, asset);
    if (kind == 0U) {
        player.channels[0].active = true;
        player.channels[0].note_on = true;
        player.channels[0].period = 0x100U;
        player.channels[0].volume = 11U;
        player.channels[0].voice_index = 0U;
    } else if (kind == 1U) {
        player.channels[2].active = true;
        player.channels[2].note_on = true;
        player.channels[2].period = 0x100U;
    } else {
        player.channels[3].active = true;
        player.channels[3].note_on = true;
        player.channels[3].period = 8U;
        player.channels[3].volume = 11U;
    }
    for (i = 0U; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        double value = kind == 0U ? pulse_sample(&player, 0U) :
                       kind == 1U ? triangle_sample(&player) :
                                    noise_sample(&player);
        samples[i] = (int16_t)(value * 28000.0);
    }
    return fnv1a32((const uint8_t *)samples, sizeof(samples));
}

static uint32_t envelope_hash(void)
{
    const TecmoMusicVoice voice = {0U, 3U, 0U, 1U, 1U, 2U, 1U, 0x48U};
    TecmoMusicChannelState channel;
    uint8_t states[64U * 7U];
    size_t i;
    memset(&channel, 0, sizeof(channel));
    channel.note_on = true;
    reset_envelope(&channel, &voice);
    for (i = 0U; i < 64U; ++i) {
        tick_envelope(&channel, &voice);
        states[i * 7U + 0U] = channel.volume;
        states[i * 7U + 1U] = channel.envelope_phase;
        states[i * 7U + 2U] = channel.attack_timer;
        states[i * 7U + 3U] = channel.decay_timer;
        states[i * 7U + 4U] = channel.sustain_timer;
        states[i * 7U + 5U] = (uint8_t)channel.release_timer;
        states[i * 7U + 6U] = (uint8_t)(channel.release_timer >> 8U);
    }
    return fnv1a32(states, sizeof(states));
}

static bool envelope_anchor_test(void)
{
    const TecmoMusicVoice voice = {0U, 3U, 0U, 0x01U, 0x01U, 3U,
                                   0x01U, 0x24U};
    TecmoMusicChannelState channel;

    memset(&channel, 0, sizeof(channel));
    channel.note_on = true;
    channel.volume = 4U;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 1U || channel.volume != 3U) return false;

    channel.envelope_phase = 1U;
    channel.volume = 2U;
    channel.sustain_timer = 3U;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 2U || channel.sustain_timer != 2U)
        return false;

    channel.envelope_phase = 2U;
    channel.volume = 2U;
    channel.sustain_timer = 0U;
    channel.release_timer = 0U;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 3U || channel.volume != 1U) return false;

    channel.envelope_phase = 0U;
    channel.volume = 3U;
    channel.attack_timer = 0U;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 0U || channel.volume != 4U) return false;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 1U || channel.volume != 3U) return false;

    channel.envelope_phase = 1U;
    channel.volume = 3U;
    channel.decay_timer = 0U;
    channel.sustain_timer = 3U;
    tick_envelope(&channel, &voice);
    if (channel.envelope_phase != 1U || channel.volume != 2U) return false;
    tick_envelope(&channel, &voice);
    return channel.envelope_phase == 2U && channel.sustain_timer == 2U;
}

static bool voice_timing_anchor_test(const TecmoMusicAsset *asset)
{
    const TecmoMusicVoice *raw08 = NULL;
    const TecmoMusicVoice *raw07 = NULL;
    TecmoMusicChannelState channel;
    unsigned i;

    if (asset == NULL || !asset->available) return false;
    for (i = 0U; i < TECMO_MUSIC_VOICE_COUNT; ++i) {
        if (asset->voices[i].duty_and_timing == 0x08U && raw08 == NULL)
            raw08 = &asset->voices[i];
        if (asset->voices[i].duty_and_timing == 0x07U && raw07 == NULL)
            raw07 = &asset->voices[i];
    }
    if (raw08 == NULL || raw07 == NULL ||
        voice_attack_shift(0x08U) != 0U ||
        voice_decay_shift(0x08U) != 0U ||
        voice_release_shift(0x08U) != 2U ||
        voice_attack_shift(0x07U) != 0U ||
        voice_decay_shift(0x07U) != 0U ||
        voice_release_shift(0x07U) != 1U)
        return false;

    memset(&channel, 0, sizeof(channel));
    reset_envelope(&channel, raw08);
    if (raw08->release != 0xF1U || channel.attack_timer != 0U ||
        channel.decay_timer != 0U || channel.release_timer != 60U)
        return false;

    memset(&channel, 0, sizeof(channel));
    reset_envelope(&channel, raw07);
    return raw07->release == 0xF1U && channel.attack_timer == 0U &&
           channel.decay_timer == 0U && channel.release_timer == 30U;
}

static bool pitch_delta_anchor_test(const TecmoMusicAsset *asset)
{
    const TecmoMusicTrack *track;
    TecmoMusicPlayer player;
    TecmoMusicChannelState *channel;
    const TecmoMusicInstruction *add;
    const TecmoMusicInstruction *reset_a;
    const TecmoMusicInstruction *reset_b;

    if (asset == NULL || asset->instruction_count <= 716U) return false;
    track = find_track(asset, 6U);
    if (track == NULL || track->channels[0].first_instruction != 448U ||
        track->channels[0].instruction_count != 271U)
        return false;
    add = &asset->instructions[450U];
    reset_a = &asset->instructions[492U];
    reset_b = &asset->instructions[716U];
    if (add->type != TECMO_MUSIC_PITCH_DELTA || add->signed_value != -4 ||
        reset_a->type != TECMO_MUSIC_PITCH_DELTA ||
        reset_a->signed_value != 0 ||
        reset_b->type != TECMO_MUSIC_PITCH_DELTA ||
        reset_b->signed_value != 0)
        return false;

    tecmo_music_player_init(&player, asset);
    channel = &player.channels[0];
    channel->active = true;
    channel->pc = 450U;
    channel->pitch_delta = 10;
    process_channel(&player, 0U);
    if (player.render_guard_failed || channel->pitch_delta != 6 ||
        channel->pc != 452U || channel->duration_ticks != 2U)
        return false;

    tecmo_music_player_init(&player, asset);
    channel = &player.channels[0];
    channel->active = true;
    channel->pc = 492U;
    channel->pitch_delta = 0x1234;
    process_channel(&player, 0U);
    if (player.render_guard_failed || channel->pitch_delta != 0 ||
        channel->pc != 494U || channel->duration_ticks != 11U)
        return false;

    tecmo_music_player_init(&player, asset);
    channel = &player.channels[0];
    channel->active = true;
    channel->pc = 716U;
    channel->pitch_delta = 0x1234;
    channel->loop_remaining[0] = 1U;
    process_channel(&player, 0U);
    return !player.render_guard_failed && channel->pitch_delta == 0 &&
           !channel->active && !channel->note_on;
}

static bool long_loop_anchor_test(const TecmoMusicAsset *asset)
{
    static const uint8_t track_ids[2] = {
        TECMO_MUSIC_TRACK_GAMEPLAY,
        TECMO_MUSIC_TRACK_PRESENTATION
    };
    unsigned track_index;

    for (track_index = 0U; track_index < 2U; ++track_index) {
        TecmoMusicPlayer player;
        bool saw_tuned_pitch = false;
        bool saw_reset_pitch = false;
        uint64_t tick;
        tecmo_music_player_init(&player, asset);
        if (!tecmo_music_queue_track(&player, track_ids[track_index]))
            return false;
        for (tick = 0U; tick < 100000U; ++tick) {
            unsigned channel_index;
            music_tick(&player);
            if (!player.playing || player.render_guard_failed)
                return false;
            for (channel_index = 0U;
                 channel_index < TECMO_MUSIC_CHANNEL_COUNT;
                 ++channel_index) {
                int32_t delta = player.channels[channel_index].pitch_delta;
                if (track_ids[track_index] == 5U) {
                    if ((channel_index == 0U && delta != -1) ||
                        (channel_index != 0U && delta != 0))
                        return false;
                } else if (channel_index == 0U) {
                    if (delta == -4) saw_tuned_pitch = true;
                    else if (delta == 0 && saw_tuned_pitch)
                        saw_reset_pitch = true;
                    else if (delta != 0)
                        return false;
                } else if (delta != 0) {
                    return false;
                }
            }
        }
        if (player.ticks_elapsed != 100000U ||
            (track_ids[track_index] == TECMO_MUSIC_TRACK_PRESENTATION &&
             (!saw_tuned_pitch || !saw_reset_pitch)))
            return false;
    }
    return true;
}

static bool track_duration_anchor_test(const TecmoMusicAsset *asset,
                                       uint8_t track_id,
                                       uint64_t expected_ticks,
                                       uint64_t *actual_ticks)
{
    TecmoMusicPlayer player;
    bool ok;
    unsigned channel_index;
    tecmo_music_player_init(&player, asset);
    ok = tecmo_music_queue_track(&player, track_id);
    while (ok && player.playing && player.ticks_elapsed < 100000U)
        music_tick(&player);
    if (actual_ticks != NULL) *actual_ticks = player.ticks_elapsed;
    if (!ok || player.playing || player.track_pending ||
        player.render_guard_failed || player.ticks_elapsed != expected_ticks)
        return false;
    for (channel_index = 0U; channel_index < TECMO_MUSIC_CHANNEL_COUNT;
         ++channel_index) {
        if (player.channels[channel_index].active ||
            player.channels[channel_index].note_on)
            return false;
    }
    return true;
}

static bool reject_mutated_payload(const uint8_t *source,
                                   size_t offset,
                                   uint32_t value,
                                   size_t value_size)
{
    uint8_t *copy = (uint8_t *)malloc(TECMO_MUSIC_PAYLOAD_SIZE);
    TecmoMusicAsset parsed;
    bool rejected;
    if (copy == NULL || offset > TECMO_MUSIC_PAYLOAD_SIZE ||
        value_size > TECMO_MUSIC_PAYLOAD_SIZE - offset || value_size > 4U) {
        free(copy);
        return false;
    }
    memcpy(copy, source, TECMO_MUSIC_PAYLOAD_SIZE);
    if (value_size >= 1U) copy[offset] = (uint8_t)value;
    if (value_size >= 2U) copy[offset + 1U] = (uint8_t)(value >> 8U);
    if (value_size >= 3U) copy[offset + 2U] = (uint8_t)(value >> 16U);
    if (value_size >= 4U) copy[offset + 3U] = (uint8_t)(value >> 24U);
    memset(&parsed, 0, sizeof(parsed));
    rejected = !parse_payload(&parsed, copy, TECMO_MUSIC_PAYLOAD_SIZE, false);
    tecmo_music_asset_shutdown(&parsed);
    free(copy);
    return rejected;
}

bool tecmo_music_self_test(const char *project_root,
                           char *message,
                           size_t message_size)
{
    TecmoMusicAsset asset;
    TecmoMusicPlayer player;
    TecmoMusicPlayer null_sink_player;
    TecmoMusicPlayer buffered_player;
    TecmoMusicPlayer duration_player;
    TecmoMusicPlayer startup_player;
    TecmoMusicPlayer cadence_player;
    int16_t samples[8192];
    uint32_t pcm_hash;
    uint32_t state_hash;
    uint32_t pulse_hash;
    uint32_t triangle_hash;
    uint32_t noise_hash;
    uint32_t env_hash;
    uint8_t *payload = NULL;
    uint64_t payload_size = 0U;
    char pack_path[1024];
    bool malformed_ok = false;
    bool gate_ok;
    bool null_sink_ok;
    bool duration_ok;
    bool anchor_ok;
    bool voice_ok;
    bool pitch_ok;
    bool long_loop_ok;
    bool pregame_matchup_ok;
    bool startup_ok;
    bool cadence_ok;
    uint64_t opening_ticks;
    uint64_t pregame_matchup_ticks;
    if (!tecmo_music_asset_load(&asset, project_root)) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size, "%s", asset.status);
        return false;
    }
    tecmo_music_player_init(&player, &asset);
    gate_ok = tecmo_music_queue_opening_once(&player) &&
              !tecmo_music_queue_opening_once(&player);
    tecmo_music_render_samples(&player, samples,
                               sizeof(samples) / sizeof(samples[0]));
    pcm_hash = fnv1a32((const uint8_t *)samples, sizeof(samples));
    state_hash = hash_channel_state(&player);
    pulse_hash = oscillator_hash(&asset, 0U);
    triangle_hash = oscillator_hash(&asset, 1U);
    noise_hash = oscillator_hash(&asset, 2U);
    env_hash = envelope_hash();
    anchor_ok = envelope_anchor_test();
    voice_ok = voice_timing_anchor_test(&asset);
    pitch_ok = pitch_delta_anchor_test(&asset);
    long_loop_ok = long_loop_anchor_test(&asset);
    pregame_matchup_ok = track_duration_anchor_test(
        &asset, TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER, 396U,
                                              &pregame_matchup_ticks);
    tecmo_music_player_init(&startup_player, &asset);
    startup_ok = tecmo_music_queue_track(&startup_player,
                                          TECMO_MUSIC_TRACK_OPENING) &&
                 startup_player.track_pending && startup_player.playing &&
                 startup_player.current_track_id == 0U &&
                 startup_player.ticks_elapsed == 0U &&
                 !startup_player.channels[0].active &&
                 !startup_player.channels[1].active &&
                 !startup_player.channels[2].active &&
                 !startup_player.channels[3].active;
    music_tick(&startup_player);
    startup_ok = startup_ok && !startup_player.track_pending &&
                  startup_player.current_track_id == TECMO_MUSIC_TRACK_OPENING &&
                 startup_player.ticks_elapsed == 1U &&
                 startup_player.channels[0].duration_ticks == 4U &&
                 startup_player.channels[1].duration_ticks == 21U &&
                 startup_player.channels[2].duration_ticks == 1U &&
                 startup_player.channels[2].note == 27U &&
                 startup_player.channels[3].duration_ticks == 10U &&
                 !startup_player.render_guard_failed;
    tecmo_music_player_init(&cadence_player, &asset);
    tecmo_music_render_samples(&cadence_player, NULL, TECMO_MUSIC_SAMPLE_RATE);
    cadence_ok = asset.tick_numerator == TECMO_MUSIC_TICK_NUMERATOR &&
                 asset.tick_denominator == TECMO_MUSIC_TICK_DENOMINATOR &&
                 cadence_player.ticks_elapsed == 60U &&
                 cadence_player.sample_tick_accumulator ==
                     (uint64_t)TECMO_MUSIC_SAMPLE_RATE *
                         TECMO_MUSIC_TICK_NUMERATOR -
                     60U * (uint64_t)TECMO_MUSIC_SAMPLE_RATE *
                         TECMO_MUSIC_TICK_DENOMINATOR;
    tecmo_music_player_init(&null_sink_player, &asset);
    tecmo_music_player_init(&buffered_player, &asset);
    null_sink_ok = tecmo_music_queue_track(&null_sink_player,
                                            TECMO_MUSIC_TRACK_OPENING) &&
                   tecmo_music_queue_track(&buffered_player,
                                           TECMO_MUSIC_TRACK_OPENING);
    tecmo_music_render_samples(&null_sink_player, NULL,
                               sizeof(samples) / sizeof(samples[0]));
    tecmo_music_render_samples(&buffered_player, samples,
                               sizeof(samples) / sizeof(samples[0]));
    null_sink_ok = null_sink_ok &&
                   memcmp(&null_sink_player, &buffered_player,
                          sizeof(null_sink_player)) == 0;
    tecmo_music_player_init(&duration_player, &asset);
    duration_ok = tecmo_music_queue_track(&duration_player,
                                           TECMO_MUSIC_TRACK_OPENING);
    while (duration_ok && duration_player.playing &&
           duration_player.ticks_elapsed < 100000U)
        music_tick(&duration_player);
    opening_ticks = duration_player.ticks_elapsed;
    /* Rev1 F7EE queue consumption through the first $063E==0 NMI is inclusive. */
    duration_ok = duration_ok && !duration_player.playing &&
                  !duration_player.render_guard_failed && opening_ticks == 2614U;
    tecmo_music_set_game_music_enabled(&player, false);
    gate_ok = gate_ok &&
              tecmo_music_queue_track(&player,
                                      TECMO_MUSIC_TRACK_PRESENTATION) &&
              player.track_pending &&
              player.pending_track_id == TECMO_MUSIC_TRACK_PRESENTATION &&
              !tecmo_music_queue_track(&player, TECMO_MUSIC_TRACK_GAMEPLAY) &&
              player.pending_track_id == TECMO_MUSIC_TRACK_PRESENTATION &&
              player.current_track_id == TECMO_MUSIC_TRACK_OPENING;
    tecmo_music_set_game_music_enabled(&player, true);
    gate_ok = gate_ok &&
              tecmo_music_queue_track(&player, TECMO_MUSIC_TRACK_GAMEPLAY) &&
              player.pending_track_id == TECMO_MUSIC_TRACK_GAMEPLAY;
    if (select_asset_pack(project_root, pack_path, sizeof(pack_path)) &&
        tecmo_asset_pack_read_entry_exact(pack_path, MUSIC_ENTRY_ID,
                                          TECMO_MUSIC_PAYLOAD_SIZE,
                                          &payload, &payload_size) == 0) {
        size_t loop_offset = 0U;
        size_t call_offset = 0U;
        uint32_t i;
        for (i = 0U; i < TECMO_MUSIC_INSTRUCTION_COUNT; ++i) {
            size_t offset = 768U + (size_t)i * MUSIC_INSTRUCTION_STRIDE;
            if (payload[offset] == TECMO_MUSIC_LOOP && loop_offset == 0U)
                loop_offset = offset;
            if (payload[offset] == TECMO_MUSIC_CALL && call_offset == 0U)
                call_offset = offset;
        }
        malformed_ok = payload_size == TECMO_MUSIC_PAYLOAD_SIZE &&
            reject_mutated_payload(payload, 4U, 2U, 2U) &&
            reject_mutated_payload(payload, 100U, 1U, 1U) &&
            reject_mutated_payload(payload, 68U, 0U, 4U) &&
            reject_mutated_payload(payload, 72U,
                                   TECMO_MUSIC_INSTRUCTION_COUNT - 1U, 4U) &&
            reject_mutated_payload(payload, 42U,
                                   TECMO_MUSIC_CALL_DEPTH + 1U, 2U) &&
            reject_mutated_payload(payload, 128U + 40U, 1U, 1U) &&
            reject_mutated_payload(payload, 320U + 2U, 1U, 1U) &&
            reject_mutated_payload(payload, 616U, 0U, 2U) &&
            reject_mutated_payload(payload, 768U, 0U, 1U) &&
            loop_offset != 0U &&
            reject_mutated_payload(payload, loop_offset + 14U,
                                   TECMO_MUSIC_LOOP_COUNT, 2U) &&
            call_offset != 0U &&
            reject_mutated_payload(payload, call_offset + 8U,
                (uint32_t)((call_offset - 768U) / MUSIC_INSTRUCTION_STRIDE), 4U);
    }
    tecmo_asset_pack_free(payload);
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size,
                       "TMUS-1 parser/state/synth: payload=%08X instructions=%u voices=%u pcm=%08X state=%08X pulse=%08X tri=%08X noise=%08X env=%08X opening_ticks=%llu pregame_matchup_ticks=%llu cadence=%s gate=%s startup=%s anchors=%s voice=%s pitch=%s long=%s null=%s malformed=%s",
                       asset.payload_fingerprint, asset.instruction_count,
                       TECMO_MUSIC_VOICE_COUNT, pcm_hash, state_hash,
                       pulse_hash, triangle_hash, noise_hash, env_hash,
                       (unsigned long long)opening_ticks,
                       (unsigned long long)pregame_matchup_ticks,
                       cadence_ok ? "pass" : "fail",
                       gate_ok ? "pass" : "fail",
                       startup_ok ? "pass" : "fail",
                       anchor_ok ? "pass" : "fail",
                       voice_ok ? "pass" : "fail",
                       pitch_ok ? "pass" : "fail",
                       long_loop_ok ? "pass" : "fail",
                       null_sink_ok ? "pass" : "fail",
                       malformed_ok ? "pass" : "fail");
    }
    tecmo_music_asset_shutdown(&asset);
    return cadence_ok && gate_ok && startup_ok && anchor_ok && voice_ok &&
           pitch_ok && long_loop_ok && pregame_matchup_ok && null_sink_ok &&
           duration_ok && malformed_ok && pcm_hash == 0x105B1338U &&
           state_hash == 0x1C74513CU &&
           pulse_hash == 0xD52B0696U && triangle_hash == 0x1C9A3181U &&
           noise_hash == 0x56252AAEU && env_hash == 0x6515A87AU;
}
