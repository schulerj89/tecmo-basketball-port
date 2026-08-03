#include "tecmo_asset_pack_gameplay_audio.h"

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum GameplayAudioInstructionType {
    GAMEPLAY_AUDIO_NOTE = 1,
    GAMEPLAY_AUDIO_VOICE = 2,
    GAMEPLAY_AUDIO_LEGATO = 3,
    GAMEPLAY_AUDIO_PITCH = 4,
    GAMEPLAY_AUDIO_END = 5,
    GAMEPLAY_AUDIO_REST = 6,
    GAMEPLAY_AUDIO_LOOP = 7,
    GAMEPLAY_AUDIO_BIND_PHRASES = 8,
    GAMEPLAY_AUDIO_CALL = 9,
    GAMEPLAY_AUDIO_RETURN = 10
};

typedef struct SfxSource {
    uint8_t id;
    uint16_t begin;
    uint16_t end;
    uint32_t fingerprint;
} SfxSource;

typedef struct ImportedInstruction {
    uint16_t address;
    uint16_t next_address;
    uint16_t target_address;
    uint16_t value16;
    int16_t signed_value;
    uint16_t loop_slot;
    uint8_t type;
    uint8_t value8;
    uint8_t byte_count;
} ImportedInstruction;

typedef struct ImportedChannel {
    ImportedInstruction instructions[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS];
    int16_t address_to_index[0x4000];
    int16_t byte_owner[0x4000];
    uint16_t work[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS];
    uint16_t instruction_count;
    uint16_t work_count;
    uint16_t loop_count;
    uint16_t entry_index;
} ImportedChannel;

typedef struct ImportedSfx {
    ImportedChannel channels[TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_COUNT]
                            [TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT];
    uint8_t voices[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_VOICES][8];
    uint16_t voice_count;
    uint32_t instruction_count;
    uint16_t max_call_depth;
} ImportedSfx;

static const SfxSource sfx_sources[TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_COUNT] = {
    {3U, 0x9DF7U, 0x9E13U, 0xB7138F94U},
    {5U, 0x8C5DU, 0x8CA3U, 0x28EE1024U},
    {6U, 0x9D8BU, 0x9DF7U, 0x34460805U},
    {11U, 0x8AC4U, 0x8AF6U, 0x93E7AC2CU},
    {12U, 0x8AF6U, 0x8B35U, 0xB172920DU},
    {13U, 0x8B35U, 0x8B6EU, 0xDC401221U},
    {14U, 0x8B6EU, 0x8B97U, 0xE3035B54U}
};

static const SfxSource frontend_sfx_sources[
    TECMO_ASSET_PACK_FRONTEND_SFX_EFFECT_COUNT] = {
    {8U, 0x8BF7U, 0x8C2AU, 0xAC9D4C1FU},
    {10U, 0x8B97U, 0x8BF7U, 0x963DC35EU}
};

static const uint8_t frontend_audio_rev1_ines_header[16] = {
    0x4EU, 0x45U, 0x53U, 0x1AU, 0x08U, 0x20U, 0x42U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
};

static const uint8_t frontend_audio_rev1_sha256[32] = {
    0x07U, 0x6AU, 0x6BU, 0xEBU, 0x27U, 0x3FU, 0xABU, 0x39U,
    0x19U, 0x8CU, 0x87U, 0xAEU, 0x6AU, 0xF6U, 0x9FU, 0x80U,
    0xAAU, 0x54U, 0x8DU, 0x68U, 0x17U, 0x75U, 0x38U, 0x29U,
    0xF2U, 0xC2U, 0xBDU, 0xE1U, 0xF9U, 0x74U, 0x75U, 0xC4U
};

static int range_valid(uint64_t offset, uint64_t count, uint64_t size)
{
    return offset <= size && count <= size - offset;
}

static int checked_add_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == NULL || right > UINT64_MAX - left) return 0;
    *result = left + right;
    return 1;
}

static int checked_mul_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == NULL || (left != 0U && right > UINT64_MAX / left))
        return 0;
    *result = left * right;
    return 1;
}

static int prg_layout_valid(uint64_t prg_offset, uint32_t prg_banks,
                            uint32_t minimum_banks, uint64_t rom_size)
{
    uint64_t prg_bytes;
    uint64_t end;
    return prg_banks >= minimum_banks &&
           checked_mul_u64(prg_banks, TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           &prg_bytes) &&
           checked_add_u64(prg_offset, prg_bytes, &end) &&
           range_valid(prg_offset, prg_bytes, rom_size);
}

static int prg_bank_range_valid(uint64_t prg_offset, uint32_t prg_banks,
                                uint32_t bank, uint64_t count,
                                uint64_t rom_size)
{
    uint64_t prg_bytes;
    uint64_t prg_end;
    uint64_t bank_delta;
    uint64_t bank_start;
    if (bank >= prg_banks ||
        !checked_mul_u64(prg_banks, TECMO_ASSET_PACK_PRG_BANK_BYTES,
                         &prg_bytes) ||
        !checked_add_u64(prg_offset, prg_bytes, &prg_end) ||
        !checked_mul_u64(bank, TECMO_ASSET_PACK_PRG_BANK_BYTES,
                         &bank_delta) ||
        !checked_add_u64(prg_offset, bank_delta, &bank_start))
        return 0;
    return range_valid(bank_start, count, prg_end) &&
           range_valid(bank_start, count, rom_size);
}

static uint64_t bank_offset(uint64_t prg_offset, uint32_t bank,
                            uint16_t address)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(address - 0x8000U);
}

static uint64_t fixed_offset(uint64_t prg_offset, uint32_t prg_banks,
                             uint16_t address)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) *
                            TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(address - 0xC000U);
}

static uint8_t source_byte(const uint8_t *rom, uint64_t bank04_offset,
                           uint16_t address)
{
    return rom[(size_t)(bank04_offset + (uint64_t)(address - 0x8000U))];
}

static uint16_t source_word(const uint8_t *rom, uint64_t bank04_offset,
                            uint16_t address)
{
    return (uint16_t)(source_byte(rom, bank04_offset, address) |
                      ((uint16_t)source_byte(
                           rom, bank04_offset,
                           (uint16_t)(address + 1U)) << 8U));
}

static int add_voice(ImportedSfx *sfx, const uint8_t voice[8])
{
    uint16_t index;
    for (index = 0U; index < sfx->voice_count; ++index) {
        if (memcmp(sfx->voices[index], voice, 8U) == 0) return (int)index;
    }
    if (sfx->voice_count >= TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_VOICES)
        return -1;
    memcpy(sfx->voices[sfx->voice_count], voice, 8U);
    return (int)sfx->voice_count++;
}

static int source_span_valid(uint16_t address, uint8_t byte_count,
                             const SfxSource *source)
{
    return address >= source->begin && address < source->end &&
           byte_count != 0U &&
           (uint32_t)address + byte_count <= source->end;
}

static int queue_address(ImportedChannel *channel, uint16_t address)
{
    if (address < 0x8000U || address >= 0xC000U) return -1;
    if (channel->address_to_index[address - 0x8000U] >= 0) return 0;
    if (channel->work_count >=
        TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS)
        return -1;
    channel->work[channel->work_count++] = address;
    return 0;
}

static int claim_bytes(ImportedChannel *channel, uint16_t address,
                       uint8_t byte_count, const SfxSource *source)
{
    uint16_t index;
    if (!source_span_valid(address, byte_count, source)) return -1;
    for (index = 0U; index < byte_count; ++index) {
        int16_t owner = channel->byte_owner[address - 0x8000U + index];
        if (owner >= 0 && (uint16_t)owner != channel->instruction_count)
            return -1;
    }
    for (index = 0U; index < byte_count; ++index) {
        channel->byte_owner[address - 0x8000U + index] =
            (int16_t)channel->instruction_count;
    }
    return 0;
}

static int parse_instruction(ImportedSfx *sfx, ImportedChannel *channel,
                             const uint8_t *rom, uint64_t bank04_offset,
                             const SfxSource *source,
                             uint16_t phrase_directory,
                             uint8_t channel_index, uint16_t address,
                             char *message, size_t message_size)
{
    ImportedInstruction instruction;
    uint8_t opcode;
    uint16_t target;
    uint16_t index;
    int voice_index;
    if (!source_span_valid(address, 1U, source)) return -1;
    if (channel->address_to_index[address - 0x8000U] >= 0) return 0;
    if (channel->instruction_count >=
        TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS)
        return -1;
    memset(&instruction, 0, sizeof(instruction));
    instruction.address = address;
    instruction.next_address = UINT16_MAX;
    instruction.target_address = UINT16_MAX;
    instruction.loop_slot = UINT16_MAX;
    opcode = source_byte(rom, bank04_offset, address);

    if (opcode < 0x80U) {
        uint8_t note;
        uint16_t pitch_index;
        if (!source_span_valid(address, 2U, source)) goto truncated;
        note = source_byte(rom, bank04_offset, (uint16_t)(address + 1U));
        pitch_index = channel_index == 2U ? (uint16_t)(note + 12U) : note;
        if (opcode == 0U ||
            (channel_index != 3U &&
             pitch_index >= TECMO_ASSET_PACK_MUSIC_PITCH_COUNT))
            return -1;
        instruction.type = GAMEPLAY_AUDIO_NOTE;
        instruction.value8 = note;
        instruction.value16 = opcode;
        instruction.byte_count = 2U;
    } else if (opcode == 0x80U) {
        uint8_t voice[8];
        if (!source_span_valid(address, 9U, source)) goto truncated;
        for (index = 0U; index < 8U; ++index) {
            voice[index] = source_byte(
                rom, bank04_offset, (uint16_t)(address + 1U + index));
        }
        if ((voice[2] != 0U && voice[2] != 0x05U &&
             voice[2] != 0x0FU && voice[2] != 0x10U) ||
            (voice[1] & 0xE0U) != 0U)
            return -1;
        voice_index = add_voice(sfx, voice);
        if (voice_index < 0) return -1;
        instruction.type = GAMEPLAY_AUDIO_VOICE;
        instruction.value8 = (uint8_t)voice_index;
        instruction.byte_count = 9U;
    } else if (opcode == 0x90U) {
        instruction.type = GAMEPLAY_AUDIO_LEGATO;
        instruction.byte_count = 1U;
    } else if (opcode == 0x91U) {
        if (!source_span_valid(address, 2U, source)) goto truncated;
        instruction.type = GAMEPLAY_AUDIO_PITCH;
        instruction.signed_value = (int8_t)source_byte(
            rom, bank04_offset, (uint16_t)(address + 1U));
        instruction.byte_count = 2U;
    } else if (opcode == 0xA0U) {
        instruction.type = GAMEPLAY_AUDIO_END;
        instruction.byte_count = 1U;
    } else if (opcode == 0xB0U) {
        if (!source_span_valid(address, 2U, source)) goto truncated;
        instruction.type = GAMEPLAY_AUDIO_REST;
        instruction.value16 = (uint16_t)(source_byte(
            rom, bank04_offset, (uint16_t)(address + 1U)) & 0x7FU);
        instruction.byte_count = 2U;
        if (instruction.value16 == 0U) return -1;
    } else if (opcode == 0xC0U) {
        if (!source_span_valid(address, 4U, source)) goto truncated;
        target = source_word(rom, bank04_offset, (uint16_t)(address + 1U));
        if (target < source->begin || target >= source->end ||
            channel->loop_count >= TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_LOOPS)
            return -1;
        instruction.type = GAMEPLAY_AUDIO_LOOP;
        instruction.value16 = source_byte(
            rom, bank04_offset, (uint16_t)(address + 3U));
        if (instruction.value16 == 0U) instruction.value16 = 256U;
        instruction.loop_slot = 0U;
        instruction.target_address = target;
        instruction.byte_count = 4U;
        ++channel->loop_count;
    } else if (opcode == 0xD0U) {
        if (!source_span_valid(address, 3U, source)) goto truncated;
        if (source_word(rom, bank04_offset,
                        (uint16_t)(address + 1U)) != phrase_directory)
            return -1;
        instruction.type = GAMEPLAY_AUDIO_BIND_PHRASES;
        instruction.byte_count = 3U;
    } else if (opcode >= 0xE0U && opcode <= 0xFEU) {
        uint16_t pointer_address = (uint16_t)(
            phrase_directory + (uint16_t)(opcode - 0xE0U) * 2U);
        if (!source_span_valid(pointer_address, 2U, source)) return -1;
        target = source_word(rom, bank04_offset, pointer_address);
        if (target < source->begin || target >= source->end) return -1;
        instruction.type = GAMEPLAY_AUDIO_CALL;
        instruction.target_address = target;
        instruction.byte_count = 1U;
    } else if (opcode == 0xFFU) {
        instruction.type = GAMEPLAY_AUDIO_RETURN;
        instruction.byte_count = 1U;
    } else {
        return -1;
    }

    if (claim_bytes(channel, address, instruction.byte_count, source) != 0)
        return -1;
    channel->address_to_index[address - 0x8000U] =
        (int16_t)channel->instruction_count;
    channel->instructions[channel->instruction_count++] = instruction;
    if (instruction.type != GAMEPLAY_AUDIO_END &&
        instruction.type != GAMEPLAY_AUDIO_RETURN) {
        uint16_t next = (uint16_t)(address + instruction.byte_count);
        channel->instructions[channel->instruction_count - 1U].next_address =
            next;
        if (queue_address(channel, next) != 0) return -1;
    }
    if ((instruction.type == GAMEPLAY_AUDIO_LOOP ||
         instruction.type == GAMEPLAY_AUDIO_CALL) &&
        queue_address(channel, instruction.target_address) != 0)
        return -1;
    return 0;

truncated:
    tecmo_asset_pack_set_message(
        message, message_size,
        "Gameplay SFX instruction is truncated by its source boundary.");
    return -1;
}

static int validate_walk(
    const ImportedChannel *channel, uint16_t instruction_index,
    uint16_t depth,
    uint16_t path[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH],
    uint8_t visited[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH + 1U]
                   [TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS],
    uint16_t *max_depth)
{
    const ImportedInstruction *instruction;
    int16_t next_index;
    int16_t target_index;
    uint16_t index;
    if (depth > TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH ||
        instruction_index >= channel->instruction_count)
        return -1;
    if (visited[depth][instruction_index] != 0U) return 0;
    visited[depth][instruction_index] = 1U;
    if (depth > *max_depth) *max_depth = depth;
    instruction = &channel->instructions[instruction_index];
    target_index = instruction->target_address == UINT16_MAX
                       ? -1
                       : channel->address_to_index[
                             instruction->target_address - 0x8000U];
    next_index = instruction->next_address == UINT16_MAX
                     ? -1
                     : channel->address_to_index[
                           instruction->next_address - 0x8000U];
    if (instruction->type == GAMEPLAY_AUDIO_CALL) {
        if (target_index < 0 ||
            depth >= TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH)
            return -1;
        for (index = 0U; index < depth; ++index) {
            if (path[index] == (uint16_t)target_index) return -1;
        }
        path[depth] = (uint16_t)target_index;
        memset(visited[depth + 1U], 0,
               TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS);
        if (validate_walk(channel, (uint16_t)target_index,
                          (uint16_t)(depth + 1U), path, visited,
                          max_depth) != 0)
            return -1;
    } else if (instruction->type == GAMEPLAY_AUDIO_LOOP) {
        if (target_index < 0 ||
            validate_walk(channel, (uint16_t)target_index, depth, path,
                          visited, max_depth) != 0)
            return -1;
    }
    if (next_index >= 0)
        return validate_walk(channel, (uint16_t)next_index, depth, path,
                             visited, max_depth);
    return instruction->type == GAMEPLAY_AUDIO_END ||
                   instruction->type == GAMEPLAY_AUDIO_RETURN
               ? 0
               : -1;
}

static int compile_channel(ImportedSfx *sfx, ImportedChannel *channel,
                           const uint8_t *rom, uint64_t bank04_offset,
                           const SfxSource *source,
                           uint16_t phrase_directory,
                           uint8_t channel_index, uint16_t entry_address,
                           char *message, size_t message_size)
{
    uint8_t (*visited)[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS];
    uint16_t path[TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH];
    uint16_t max_depth = 0U;
    size_t index;
    memset(channel, 0, sizeof(*channel));
    for (index = 0U; index < 0x4000U; ++index) {
        channel->address_to_index[index] = -1;
        channel->byte_owner[index] = -1;
    }
    if (queue_address(channel, entry_address) != 0) return -1;
    while (channel->work_count > 0U) {
        uint16_t address = channel->work[--channel->work_count];
        if (parse_instruction(sfx, channel, rom, bank04_offset, source,
                              phrase_directory, channel_index, address,
                              message, message_size) != 0)
            return -1;
    }
    if (channel->address_to_index[entry_address - 0x8000U] < 0) return -1;
    channel->entry_index = (uint16_t)channel->address_to_index[
        entry_address - 0x8000U];
    visited = (uint8_t (*)
                   [TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS])
        calloc(TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CALL_DEPTH + 1U,
               TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_CHANNEL_INSTRUCTIONS);
    if (visited == NULL) return -1;
    memset(path, 0, sizeof(path));
    if (validate_walk(channel, channel->entry_index, 0U, path, visited,
                      &max_depth) != 0) {
        free(visited);
        return -1;
    }
    free(visited);
    if (max_depth > sfx->max_call_depth) sfx->max_call_depth = max_depth;
    sfx->instruction_count += channel->instruction_count;
    return sfx->instruction_count <=
                   TECMO_ASSET_PACK_GAMEPLAY_SFX_MAX_INSTRUCTIONS
               ? 0
               : -1;
}

static int compile_effect(ImportedSfx *sfx, unsigned effect_index,
                          const SfxSource *sources,
                          const uint8_t *rom, uint64_t bank04_offset,
                          char *message, size_t message_size)
{
    const SfxSource *source = &sources[effect_index];
    uint16_t cursor = source->begin;
    uint16_t entries[TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT];
    unsigned channel;
    for (channel = 0U;
         channel < TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT; ++channel) {
        if ((uint32_t)cursor + 3U > source->end ||
            source_byte(rom, bank04_offset, cursor) != channel)
            return -1;
        entries[channel] = source_word(rom, bank04_offset,
                                       (uint16_t)(cursor + 1U));
        cursor = (uint16_t)(cursor + 3U);
    }
    if (cursor >= source->end ||
        source_byte(rom, bank04_offset, cursor) != 0xFFU)
        return -1;
    ++cursor;
    for (channel = 0U;
         channel < TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT; ++channel) {
        if (compile_channel(sfx, &sfx->channels[effect_index][channel],
                            rom, bank04_offset, source, cursor,
                            (uint8_t)channel, entries[channel], message,
                            message_size) != 0)
            return -1;
    }
    return 0;
}

static size_t align4(size_t value)
{
    return (value + 3U) & ~(size_t)3U;
}

static int serialize_sfx(const ImportedSfx *sfx, const SfxSource *sources,
                         unsigned effect_count, const char magic[4],
                         const uint32_t semantic_fingerprints[5],
                         const uint8_t frontend_metadata[20],
                         const uint8_t *rom, uint64_t fixed_source,
                         uint8_t **payload_out, size_t *payload_size_out)
{
    size_t effect_offset = TECMO_ASSET_PACK_GAMEPLAY_SFX_HEADER_SIZE;
    size_t voice_offset = effect_offset +
        effect_count *
            TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_STRIDE;
    size_t pitch_offset = voice_offset +
        sfx->voice_count * TECMO_ASSET_PACK_GAMEPLAY_SFX_VOICE_STRIDE;
    size_t instruction_offset = align4(
        pitch_offset + TECMO_ASSET_PACK_MUSIC_PITCH_COUNT * 2U);
    size_t payload_size = instruction_offset +
        sfx->instruction_count *
            TECMO_ASSET_PACK_GAMEPLAY_SFX_INSTRUCTION_STRIDE;
    uint8_t *payload = (uint8_t *)calloc(payload_size, 1U);
    uint32_t global_instruction = 0U;
    unsigned effect_index;
    uint16_t index;
    if (payload == NULL || payload_size > UINT32_MAX) return -1;
    memcpy(payload, magic, 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_SFX_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 8U, (uint32_t)payload_size);
    tecmo_asset_pack_store_u32(payload + 12U, 0x0650F5B0U);
    for (effect_index = 0U; effect_index < 5U; ++effect_index) {
        tecmo_asset_pack_store_u32(payload + 16U + effect_index * 4U,
                                   semantic_fingerprints[effect_index]);
    }
    tecmo_asset_pack_store_u16(
        payload + 36U, (uint16_t)effect_count);
    tecmo_asset_pack_store_u16(
        payload + 38U, TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT);
    tecmo_asset_pack_store_u16(payload + 40U, sfx->voice_count);
    tecmo_asset_pack_store_u16(
        payload + 42U, TECMO_ASSET_PACK_MUSIC_PITCH_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_STRIDE);
    tecmo_asset_pack_store_u16(
        payload + 46U, TECMO_ASSET_PACK_GAMEPLAY_SFX_VOICE_STRIDE);
    tecmo_asset_pack_store_u16(
        payload + 48U, TECMO_ASSET_PACK_GAMEPLAY_SFX_INSTRUCTION_STRIDE);
    tecmo_asset_pack_store_u16(payload + 50U, sfx->max_call_depth);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_MUSIC_SAMPLE_RATE);
    tecmo_asset_pack_store_u32(
        payload + 56U, TECMO_ASSET_PACK_MUSIC_TICK_NUMERATOR);
    tecmo_asset_pack_store_u32(
        payload + 60U, TECMO_ASSET_PACK_MUSIC_TICK_DENOMINATOR);
    tecmo_asset_pack_store_u32(payload + 64U, (uint32_t)effect_offset);
    tecmo_asset_pack_store_u32(payload + 68U, (uint32_t)voice_offset);
    tecmo_asset_pack_store_u32(payload + 72U, (uint32_t)pitch_offset);
    tecmo_asset_pack_store_u32(payload + 76U,
                               (uint32_t)instruction_offset);
    tecmo_asset_pack_store_u32(payload + 80U, sfx->instruction_count);
    tecmo_asset_pack_store_u16(
        payload + 84U, TECMO_ASSET_PACK_GAMEPLAY_SFX_LOOP_STATE_COUNT);
    for (effect_index = 0U;
         effect_index < effect_count;
         ++effect_index) {
        tecmo_asset_pack_store_u32(payload + 88U + effect_index * 4U,
                                   sources[effect_index].fingerprint);
        payload[116U + effect_index] = sources[effect_index].id;
    }
    if (frontend_metadata != NULL)
        memcpy(payload + 96U, frontend_metadata, 20U);
    for (index = 0U; index < sfx->voice_count; ++index) {
        memcpy(payload + voice_offset +
                           index * TECMO_ASSET_PACK_GAMEPLAY_SFX_VOICE_STRIDE,
               sfx->voices[index],
               TECMO_ASSET_PACK_GAMEPLAY_SFX_VOICE_STRIDE);
    }
    for (index = 0U; index < TECMO_ASSET_PACK_MUSIC_PITCH_COUNT; ++index) {
        uint16_t period = tecmo_asset_pack_read_u16(
            rom + (size_t)(fixed_source + 0x393BU + index * 2U));
        tecmo_asset_pack_store_u16(payload + pitch_offset + index * 2U,
                                   period);
    }
    for (effect_index = 0U;
         effect_index < effect_count;
         ++effect_index) {
        uint8_t *effect = payload + effect_offset +
            effect_index * TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_STRIDE;
        unsigned channel_index;
        effect[0] = sources[effect_index].id;
        tecmo_asset_pack_store_u16(
            effect + 2U, TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT);
        tecmo_asset_pack_store_u32(effect + 4U,
                                   sources[effect_index].fingerprint);
        for (channel_index = 0U;
             channel_index < TECMO_ASSET_PACK_GAMEPLAY_SFX_CHANNEL_COUNT;
             ++channel_index) {
            const ImportedChannel *channel =
                &sfx->channels[effect_index][channel_index];
            uint32_t channel_base = global_instruction;
            uint16_t instruction_index;
            tecmo_asset_pack_store_u32(
                effect + 8U + channel_index * 8U,
                channel_base + channel->entry_index);
            tecmo_asset_pack_store_u32(
                effect + 12U + channel_index * 8U,
                channel->instruction_count);
            for (instruction_index = 0U;
                 instruction_index < channel->instruction_count;
                 ++instruction_index) {
                const ImportedInstruction *source =
                    &channel->instructions[instruction_index];
                uint8_t *dest = payload + instruction_offset +
                    (channel_base + instruction_index) *
                        TECMO_ASSET_PACK_GAMEPLAY_SFX_INSTRUCTION_STRIDE;
                int16_t next = source->next_address == UINT16_MAX
                                   ? -1
                                   : channel->address_to_index[
                                         source->next_address - 0x8000U];
                int16_t target = source->target_address == UINT16_MAX
                                     ? -1
                                     : channel->address_to_index[
                                           source->target_address - 0x8000U];
                dest[0] = source->type;
                dest[1] = source->value8;
                tecmo_asset_pack_store_u16(dest + 2U, source->value16);
                tecmo_asset_pack_store_u32(
                    dest + 4U, next < 0 ? UINT32_MAX
                                        : channel_base + (uint16_t)next);
                tecmo_asset_pack_store_u32(
                    dest + 8U, target < 0 ? UINT32_MAX
                                          : channel_base + (uint16_t)target);
                tecmo_asset_pack_store_u16(
                    dest + 12U, (uint16_t)source->signed_value);
                tecmo_asset_pack_store_u16(dest + 14U,
                                           source->loop_slot);
            }
            global_instruction += channel->instruction_count;
        }
    }
    if (global_instruction != sfx->instruction_count) {
        free(payload);
        return -1;
    }
    *payload_out = payload;
    *payload_size_out = payload_size;
    return 0;
}

static int serialize_dmc(const uint8_t *rom, uint64_t fixed_source,
                         uint8_t **payload_out, size_t *payload_size_out)
{
    static const uint16_t pool_cpu[3] = {0xC080U, 0xC440U, 0xC740U};
    static const uint32_t pool_size[3] = {
        TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL0_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL1_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL2_SIZE
    };
    static const uint32_t pool_fingerprint[3] = {
        0x33E109D7U, 0x6ECC107CU, 0xF621FD7CU
    };
    static const uint8_t clip_pool[5] = {2U, 2U, 2U, 1U, 0U};
    static const uint8_t clip_rate[5] = {14U, 14U, 14U, 15U, 15U};
    static const uint32_t clip_size[5] = {161U, 769U, 945U, 721U, 513U};
    static const uint32_t trigger_fingerprint[5] = {
        0xEE75A82EU, 0xEE75A82EU, 0x567D5B90U, 0x1C158FABU,
        0xF6CEC8DBU
    };
    uint8_t *payload = (uint8_t *)calloc(
        TECMO_ASSET_PACK_GAMEPLAY_DMC_SIZE, 1U);
    uint32_t data_cursor = TECMO_ASSET_PACK_GAMEPLAY_DMC_DATA_OFFSET;
    unsigned index;
    if (payload == NULL) return -1;
    memcpy(payload, "TDMC", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_DMC_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_DMC_SIZE);
    tecmo_asset_pack_store_u32(payload + 12U, 0x0650F5B0U);
    for (index = 0U; index < 3U; ++index) {
        tecmo_asset_pack_store_u32(payload + 16U + index * 4U,
                                   pool_fingerprint[index]);
    }
    tecmo_asset_pack_store_u32(payload + 28U, 0xEE75A82EU);
    tecmo_asset_pack_store_u32(payload + 32U, 0x567D5B90U);
    tecmo_asset_pack_store_u32(payload + 36U, 0x1C158FABU);
    tecmo_asset_pack_store_u32(payload + 40U, 0xF6CEC8DBU);
    tecmo_asset_pack_store_u32(payload + 44U, 0x181D6897U);
    tecmo_asset_pack_store_u32(payload + 48U, 0xAC51064EU);
    tecmo_asset_pack_store_u32(payload + 52U, 0x1185F342U);
    tecmo_asset_pack_store_u32(payload + 56U, 0x03BFDFA0U);
    tecmo_asset_pack_store_u16(
        payload + 60U, TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIP_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 62U, TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 64U, TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIP_STRIDE);
    tecmo_asset_pack_store_u16(
        payload + 66U, TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 68U, TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIPS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 72U, TECMO_ASSET_PACK_GAMEPLAY_DMC_POOLS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 76U, TECMO_ASSET_PACK_GAMEPLAY_DMC_DATA_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 80U,
        TECMO_ASSET_PACK_GAMEPLAY_DMC_SIZE -
            TECMO_ASSET_PACK_GAMEPLAY_DMC_DATA_OFFSET);
    for (index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIP_COUNT; ++index) {
        uint8_t *clip = payload + TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIPS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DMC_CLIP_STRIDE;
        clip[0] = (uint8_t)index;
        clip[1] = clip_pool[index];
        clip[2] = clip_rate[index];
        clip[3] = 0U; /* no loop and no IRQ */
        tecmo_asset_pack_store_u32(clip + 4U, 0U);
        tecmo_asset_pack_store_u32(clip + 8U, clip_size[index]);
        tecmo_asset_pack_store_u32(clip + 12U,
                                   trigger_fingerprint[index]);
        clip[16] = 0x1FU; /* exact $4015 enable value */
    }
    for (index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL_COUNT; ++index) {
        uint8_t *pool = payload + TECMO_ASSET_PACK_GAMEPLAY_DMC_POOLS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DMC_POOL_STRIDE;
        tecmo_asset_pack_store_u32(pool + 0U, data_cursor);
        tecmo_asset_pack_store_u32(pool + 4U, pool_size[index]);
        tecmo_asset_pack_store_u32(pool + 8U, pool_fingerprint[index]);
        memcpy(payload + data_cursor,
               rom + (size_t)(fixed_source + pool_cpu[index] - 0xC000U),
               pool_size[index]);
        data_cursor += pool_size[index];
    }
    if (data_cursor != TECMO_ASSET_PACK_GAMEPLAY_DMC_SIZE) {
        free(payload);
        return -1;
    }
    *payload_out = payload;
    *payload_size_out = TECMO_ASSET_PACK_GAMEPLAY_DMC_SIZE;
    return 0;
}

static int validate_revision(const uint8_t *rom, uint64_t bank04_source,
                             uint64_t bank05_source,
                             uint64_t fixed_source,
                             char *message, size_t message_size)
{
    static const uint16_t expected_directory[16] = {
        0x8CB4U, 0x8CD0U, 0x8AC4U, 0x9DF7U,
        0x8CA3U, 0x8C5DU, 0x9D8BU, 0x8C5DU,
        0x8BF7U, 0x8C2AU, 0x8B97U, 0x8AC4U,
        0x8AF6U, 0x8B35U, 0x8B6EU, 0x8B97U
    };
    static const struct FingerprintRange {
        uint16_t cpu;
        uint16_t size;
        uint32_t fingerprint;
    } fixed_ranges[] = {
        {0xEB2BU, 11U, 0x181D6897U},
        {0xE747U, 30U, 0xAC51064EU},
        {0xE58DU, 92U, 0x1185F342U},
        {0xE69FU, 41U, 0x03BFDFA0U},
        {0xE7DBU, 5U, 0xFA9A48DBU},
        {0xE863U, 5U, 0xE30ADA62U},
        {0xE86DU, 5U, 0xFA9A48DBU}
    }, bank05_ranges[] = {
        {0xA8D6U, 19U, 0xEE75A82EU},
        {0xA9C5U, 21U, 0x567D5B90U},
        {0xABF5U, 21U, 0x1C158FABU},
        {0xB5ABU, 21U, 0xF6CEC8DBU},
        {0x9FECU, 5U, 0x5824A080U},
        {0xAD01U, 14U, 0xB7141C72U},
        {0xB1D1U, 22U, 0xCFCD9759U}
    };
    static const uint16_t pool_cpu[3] = {0xC080U, 0xC440U, 0xC740U};
    static const uint32_t pool_size[3] = {513U, 721U, 945U};
    static const uint32_t pool_hash[3] = {
        0x33E109D7U, 0x6ECC107CU, 0xF621FD7CU
    };
    unsigned index;
    if (tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank04_source + 0x0AA4U), 32U) !=
            0x6283F255U ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank04_source + 0x0AA4U), 556U) !=
            0x548EED95U ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank04_source + 0x1D8BU), 136U) !=
            0x838408D4U ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(fixed_source + 0x32F2U),
            TECMO_ASSET_PACK_MUSIC_ENGINE_SIZE) != 0xFC6A0BC1U ||
        memcmp(rom + (size_t)(fixed_source + 0x33F2U),
               "\x10\x20\x40\x80\x00\x00\x00\x00", 8U) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "Rev1 gameplay audio directory or fixed-engine fingerprint mismatch.");
        return -1;
    }
    for (index = 0U; index < 16U; ++index) {
        if (source_word(rom, bank04_source,
                        (uint16_t)(0x8AA4U + index * 2U)) !=
            expected_directory[index]) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 gameplay audio source fingerprint mismatch.");
            return -1;
        }
    }
    for (index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_COUNT; ++index) {
        const SfxSource *source = &sfx_sources[index];
        if (tecmo_asset_pack_fnv1a32(
                rom + (size_t)(bank04_source + source->begin - 0x8000U),
                (size_t)(source->end - source->begin)) !=
            source->fingerprint) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 gameplay audio source fingerprint mismatch.");
            return -1;
        }
    }
    for (index = 0U; index < sizeof(fixed_ranges) / sizeof(fixed_ranges[0]);
         ++index) {
        if (tecmo_asset_pack_fnv1a32(
                rom + (size_t)(fixed_source + fixed_ranges[index].cpu -
                               0xC000U),
                fixed_ranges[index].size) != fixed_ranges[index].fingerprint) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 gameplay audio source fingerprint mismatch.");
            return -1;
        }
    }
    for (index = 0U;
         index < sizeof(bank05_ranges) / sizeof(bank05_ranges[0]); ++index) {
        if (tecmo_asset_pack_fnv1a32(
                rom + (size_t)(bank05_source + bank05_ranges[index].cpu -
                               0x8000U),
                bank05_ranges[index].size) != bank05_ranges[index].fingerprint) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 gameplay audio source fingerprint mismatch.");
            return -1;
        }
    }
    for (index = 0U; index < 3U; ++index) {
        if (tecmo_asset_pack_fnv1a32(
                rom + (size_t)(fixed_source + pool_cpu[index] - 0xC000U),
                pool_size[index]) != pool_hash[index]) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 gameplay audio source fingerprint mismatch.");
            return -1;
        }
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_audio(
    const uint8_t *rom, uint64_t rom_size, uint64_t prg_offset,
    uint32_t prg_banks, int enforce_revision_fingerprints,
    uint8_t **sfx_payload_out, size_t *sfx_payload_size_out,
    uint8_t **dmc_payload_out, size_t *dmc_payload_size_out,
    TecmoGameplayAudioProvenance *provenance,
    char *message, size_t message_size)
{
    ImportedSfx *sfx = NULL;
    uint64_t bank04_source;
    uint64_t bank05_source;
    uint64_t fixed_source;
    unsigned index;
    int result = -1;
    if (rom == NULL || sfx_payload_out == NULL ||
        sfx_payload_size_out == NULL || dmc_payload_out == NULL ||
        dmc_payload_size_out == NULL || provenance == NULL || prg_banks < 8U)
        return -1;
    *sfx_payload_out = NULL;
    *sfx_payload_size_out = 0U;
    *dmc_payload_out = NULL;
    *dmc_payload_size_out = 0U;
    memset(provenance, 0, sizeof(*provenance));
    if (!prg_layout_valid(prg_offset, prg_banks, 8U, rom_size)) return -1;
    bank04_source = bank_offset(prg_offset, 4U, 0x8000U);
    bank05_source = bank_offset(prg_offset, 5U, 0x8000U);
    fixed_source = fixed_offset(prg_offset, prg_banks, 0xC000U);
    if (!prg_bank_range_valid(prg_offset, prg_banks, 4U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !prg_bank_range_valid(prg_offset, prg_banks, 5U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !prg_bank_range_valid(prg_offset, prg_banks, prg_banks - 1U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size))
        return -1;
    if (enforce_revision_fingerprints &&
        validate_revision(rom, bank04_source, bank05_source, fixed_source,
                          message, message_size) != 0)
        return -1;
    sfx = (ImportedSfx *)calloc(1U, sizeof(*sfx));
    if (sfx == NULL) return -1;
    for (index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_COUNT; ++index) {
        if (compile_effect(sfx, index, sfx_sources, rom, bank04_source, message,
                           message_size) != 0) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Could not compile strict gameplay SFX semantics.");
            goto cleanup;
        }
    }
    {
        static const uint32_t semantic_fingerprints[5] = {
            0x6283F255U, 0x548EED95U, 0x838408D4U, 0xFC6A0BC1U,
            0x80402010U
        };
        if (serialize_sfx(sfx, sfx_sources,
                          TECMO_ASSET_PACK_GAMEPLAY_SFX_EFFECT_COUNT,
                          "TSFX", semantic_fingerprints, NULL, rom,
                          fixed_source, sfx_payload_out,
                      sfx_payload_size_out) != 0 ||
            serialize_dmc(rom, fixed_source, dmc_payload_out,
                          dmc_payload_size_out) != 0)
            goto cleanup;
        if (enforce_revision_fingerprints &&
            (*sfx_payload_size_out != 2824U ||
             tecmo_asset_pack_fnv1a32(*sfx_payload_out,
                                      *sfx_payload_size_out) !=
                 0x968A5DE6U || sfx->instruction_count != 131U ||
             sfx->voice_count != 14U || *dmc_payload_size_out != 2515U ||
             tecmo_asset_pack_fnv1a32(*dmc_payload_out,
                                      *dmc_payload_size_out) !=
                 0xAD70E6E8U)) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Rev1 TSFX-1/TDMC-1 serialized postconditions mismatch.");
            goto cleanup;
        }
    }
    provenance->sfx_directory_offset = bank04_source + 0x0AA4U;
    provenance->sfx_core_offset = bank04_source + 0x0AA4U;
    provenance->sfx_extension_offset = bank04_source + 0x1D8BU;
    provenance->engine_offset = fixed_source + 0x32F2U;
    provenance->event_offsets[0] = fixed_source + 0x27DBU;
    provenance->event_offsets[1] = fixed_source + 0x2863U;
    provenance->event_offsets[2] = fixed_source + 0x286DU;
    provenance->event_offsets[3] = bank05_source + 0x1FECU;
    provenance->event_offsets[4] = bank05_source + 0x2D01U;
    provenance->event_offsets[5] = bank05_source + 0x31D1U;
    provenance->dmc_pool_offsets[0] = fixed_source + 0x0080U;
    provenance->dmc_pool_offsets[1] = fixed_source + 0x0440U;
    provenance->dmc_pool_offsets[2] = fixed_source + 0x0740U;
    provenance->dmc_trigger_offsets[0] = bank05_source + 0x28D6U;
    provenance->dmc_trigger_offsets[1] = bank05_source + 0x29C5U;
    provenance->dmc_trigger_offsets[2] = bank05_source + 0x2BF5U;
    provenance->dmc_trigger_offsets[3] = bank05_source + 0x35ABU;
    provenance->gameplay_gate_offset = fixed_source + 0x2B2BU;
    provenance->restart_offset = fixed_source + 0x2747U;
    provenance->period_offset = fixed_source + 0x258DU;
    provenance->final_offset = fixed_source + 0x269FU;
    provenance->sfx_payload_size = (uint32_t)*sfx_payload_size_out;
    provenance->sfx_payload_fingerprint = tecmo_asset_pack_fnv1a32(
        *sfx_payload_out, *sfx_payload_size_out);
    provenance->sfx_instruction_count = sfx->instruction_count;
    provenance->sfx_voice_count = sfx->voice_count;
    provenance->dmc_payload_size = (uint32_t)*dmc_payload_size_out;
    provenance->dmc_payload_fingerprint = tecmo_asset_pack_fnv1a32(
        *dmc_payload_out, *dmc_payload_size_out);
    result = 0;

cleanup:
    if (result != 0) {
        free(*sfx_payload_out);
        free(*dmc_payload_out);
        *sfx_payload_out = NULL;
        *dmc_payload_out = NULL;
        *sfx_payload_size_out = 0U;
        *dmc_payload_size_out = 0U;
    }
    free(sfx);
    return result;
}

int tecmo_asset_pack_build_frontend_audio(
    const uint8_t *rom, uint64_t rom_size, uint64_t prg_offset,
    uint32_t prg_banks, int enforce_revision_fingerprints,
    uint8_t **payload_out, size_t *payload_size_out,
    TecmoFrontendAudioProvenance *provenance,
    char *message, size_t message_size)
{
    static const struct FingerprintRange {
        uint8_t bank;
        uint16_t cpu;
        uint16_t size;
        uint32_t fingerprint;
        int fixed;
    } ranges[] = {
        {4U, 0x8AA4U, 32U, 0x6283F255U, 0},
        {4U, 0x8BF7U, 51U, 0xAC9D4C1FU, 0},
        {4U, 0x8B97U, 96U, 0x963DC35EU, 0},
        {3U, 0x8056U, 32U, 0x4A97C61DU, 0},
        {3U, 0x8076U, 27U, 0x0C902C97U, 0},
        {0U, 0xC003U, 3U, 0x0F4103F2U, 1},
        {0U, 0xD92EU, 119U, 0x105B38A7U, 1},
        {0U, 0xDB25U, 99U, 0x5D98AB7AU, 1},
        {0U, 0xE3FAU, 32U, 0xB8BA175BU, 1},
        {0U, 0xC024U, 6U, 0xB4100BD2U, 1},
        {0U, 0xCBAFU, 12U, 0xAB3677A6U, 1},
        {0U, 0xEC06U, 32U, 0xF1BCC8E2U, 1},
        {0U, 0xD768U, 43U, 0x68D40771U, 1},
        {0U, 0xE477U, 42U, 0x71B4A4A8U, 1},
        {0U, 0xF2F2U, 8U, 0x17DE7030U, 1}
    };
    static const uint32_t semantic_fingerprints[5] = {
        0x6283F255U, 0x4A97C61DU, 0x0C902C97U, 0x68D40771U,
        0xF1BCC8E2U
    };
    uint8_t metadata[20] = {
        5U, 10U, 8U, 6U, 127U, 0U, 126U, 0U,
        0x30U, 0x70U, 0xDEU, 0x17U,
        0xA8U, 0xA4U, 0xB4U, 0x71U,
        0x8AU, 0xA8U, 0x4CU, 0xCAU
    };
    ImportedSfx *sfx = NULL;
    uint64_t bank04_source;
    uint64_t fixed_source;
    unsigned index;
    int result = -1;
    if (rom == NULL || payload_out == NULL || payload_size_out == NULL ||
        provenance == NULL || prg_banks < 8U)
        return -1;
    *payload_out = NULL;
    *payload_size_out = 0U;
    memset(provenance, 0, sizeof(*provenance));
    if (!prg_layout_valid(prg_offset, prg_banks, 8U, rom_size)) return -1;
    bank04_source = bank_offset(prg_offset, 4U, 0x8000U);
    fixed_source = fixed_offset(prg_offset, prg_banks, 0xC000U);
    if (!prg_bank_range_valid(prg_offset, prg_banks, 4U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !prg_bank_range_valid(prg_offset, prg_banks, 3U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !prg_bank_range_valid(prg_offset, prg_banks, prg_banks - 1U,
                              TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size))
        return -1;
    if (enforce_revision_fingerprints) {
        for (index = 0U; index < sizeof(ranges) / sizeof(ranges[0]);
             ++index) {
            uint64_t source = ranges[index].fixed
                ? fixed_source + ranges[index].cpu - 0xC000U
                : bank_offset(prg_offset, ranges[index].bank,
                              ranges[index].cpu);
            if (!range_valid(source, ranges[index].size, rom_size) ||
                tecmo_asset_pack_fnv1a32(
                    rom + (size_t)source, ranges[index].size) !=
                    ranges[index].fingerprint) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "Frontend audio revision fingerprint mismatch.");
                return -1;
            }
        }
    }
    sfx = (ImportedSfx *)calloc(1U, sizeof(*sfx));
    if (sfx == NULL) return -1;
    for (index = 0U; index < TECMO_ASSET_PACK_FRONTEND_SFX_EFFECT_COUNT;
         ++index) {
        if (compile_effect(sfx, index, frontend_sfx_sources, rom,
                           bank04_source, message, message_size) != 0) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "Could not compile strict frontend SFX semantics.");
            goto cleanup;
        }
    }
    if (serialize_sfx(sfx, frontend_sfx_sources,
                      TECMO_ASSET_PACK_FRONTEND_SFX_EFFECT_COUNT,
                      "TFSX", semantic_fingerprints, metadata, rom,
                      fixed_source, payload_out, payload_size_out) != 0)
        goto cleanup;
    if (enforce_revision_fingerprints &&
        (*payload_size_out != 1792U ||
         tecmo_asset_pack_fnv1a32(*payload_out, *payload_size_out) !=
             0x985DC7EDU || sfx->instruction_count != 87U ||
         sfx->voice_count != 3U)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "Rev1 TFSX-1 serialized postconditions mismatch.");
        goto cleanup;
    }
    provenance->sfx_directory_offset =
        bank_offset(prg_offset, 4U, 0x8AA4U);
    provenance->effect_offsets[0] =
        bank_offset(prg_offset, 4U, 0x8BF7U);
    provenance->effect_offsets[1] =
        bank_offset(prg_offset, 4U, 0x8B97U);
    provenance->title_setup_offset =
        bank_offset(prg_offset, 3U, 0x8056U);
    provenance->title_confirm_offset =
        bank_offset(prg_offset, 3U, 0x8076U);
    provenance->title_setup_bridge_offset = fixed_source + 0x0003U;
    provenance->title_setup_transition_offset = fixed_source + 0x192EU;
    provenance->title_setup_fade_offset = fixed_source + 0x1B25U;
    provenance->frame_yield_helper_offset = fixed_source + 0x23FAU;
    provenance->title_stop_bridge_offset = fixed_source + 0x0024U;
    provenance->title_stop_dispatch_offset = fixed_source + 0x0BAFU;
    provenance->title_stop_offset = fixed_source + 0x2C06U;
    provenance->menu_accept_offset = fixed_source + 0x1768U;
    provenance->menu_flow_offset = fixed_source + 0x2477U;
    provenance->mailboxes_offset = fixed_source + 0x32F2U;
    provenance->payload_size = (uint32_t)*payload_size_out;
    provenance->payload_fingerprint = tecmo_asset_pack_fnv1a32(
        *payload_out, *payload_size_out);
    provenance->instruction_count = sfx->instruction_count;
    provenance->voice_count = sfx->voice_count;
    result = 0;

cleanup:
    if (result != 0) {
        free(*payload_out);
        *payload_out = NULL;
        *payload_size_out = 0U;
    }
    free(sfx);
    return result;
}

int tecmo_asset_pack_frontend_audio_source_test(
    const char *rom_path, char *message, size_t message_size)
{
    const uint64_t expected_rom_size =
        sizeof(frontend_audio_rev1_ines_header) +
        8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES +
        32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t *rom = NULL;
    uint8_t *payload = NULL;
    uint8_t *invalid_payload = NULL;
    uint8_t input_sha256[32];
    uint64_t rom_size = 0U;
    size_t payload_size = 0U;
    size_t invalid_payload_size = 0U;
    TecmoFrontendAudioProvenance provenance;
    TecmoFrontendAudioProvenance invalid_provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != expected_rom_size ||
        memcmp(rom, frontend_audio_rev1_ines_header,
               sizeof(frontend_audio_rev1_ines_header)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct source test requires the exact Rev1 iNES layout.");
        free(rom);
        return -1;
    }
    memset(&invalid_provenance, 0, sizeof(invalid_provenance));
    if (tecmo_asset_pack_build_frontend_audio(
            rom, rom_size, UINT64_MAX, 8U, 1,
            &invalid_payload, &invalid_payload_size, &invalid_provenance,
            message, message_size) == 0 || invalid_payload != NULL ||
        invalid_payload_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct builder offset guard accepted an invalid layout.");
        free(invalid_payload);
        free(rom);
        return -1;
    }
    free(invalid_payload);

    invalid_payload = NULL;
    invalid_payload_size = 0U;
    if (tecmo_asset_pack_build_frontend_audio(
            rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 0U, 1,
            &invalid_payload, &invalid_payload_size, &invalid_provenance,
            message, message_size) == 0 || invalid_payload != NULL ||
        invalid_payload_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct builder zero-bank guard accepted an invalid layout.");
        free(invalid_payload);
        free(rom);
        return -1;
    }
    free(invalid_payload);

    invalid_payload = NULL;
    invalid_payload_size = 0U;
    if (tecmo_asset_pack_build_frontend_audio(
            rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 7U, 1,
            &invalid_payload, &invalid_payload_size, &invalid_provenance,
            message, message_size) == 0 || invalid_payload != NULL ||
        invalid_payload_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct builder undersized-bank guard accepted an invalid layout.");
        free(invalid_payload);
        free(rom);
        return -1;
    }
    free(invalid_payload);

    result = tecmo_asset_pack_build_frontend_audio(
        rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 8U, 1,
        &payload, &payload_size, &provenance, message, message_size);
    if (result == 0 &&
        (tecmo_asset_pack_sha256_digest(
             rom, (size_t)rom_size, input_sha256) != 0 ||
         memcmp(input_sha256, frontend_audio_rev1_sha256,
                sizeof(input_sha256)) != 0)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TFSX-1 direct source test full-ROM SHA-256 mismatch.");
        result = -1;
    }
    if (result == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "Built strict ROM-derived TFSX-1 frontend audio source.");
    }
    free(payload);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_audio_source_test(
    const char *rom_path, char *message, size_t message_size)
{
    const uint64_t expected_rom_size =
        sizeof(frontend_audio_rev1_ines_header) +
        8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES +
        32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t *rom = NULL;
    uint8_t *sfx_payload = NULL;
    uint8_t *dmc_payload = NULL;
    uint8_t *invalid_sfx = NULL;
    uint8_t *invalid_dmc = NULL;
    uint8_t input_sha256[32];
    uint64_t rom_size = 0U;
    size_t sfx_payload_size = 0U;
    size_t dmc_payload_size = 0U;
    size_t invalid_sfx_size = 0U;
    size_t invalid_dmc_size = 0U;
    TecmoGameplayAudioProvenance provenance;
    TecmoGameplayAudioProvenance invalid_provenance;
    int result;

    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != expected_rom_size ||
        memcmp(rom, frontend_audio_rev1_ines_header,
               sizeof(frontend_audio_rev1_ines_header)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct source test requires the exact Rev1 iNES layout.");
        free(rom);
        return -1;
    }

    /* Exercise the public arithmetic guard before any source pointer is
       formed. This remains a gameplay-importer-only gate. */
    memset(&invalid_provenance, 0, sizeof(invalid_provenance));
    if (tecmo_asset_pack_build_gameplay_audio(
            rom, rom_size, UINT64_MAX, 8U, 1,
            &invalid_sfx, &invalid_sfx_size, &invalid_dmc,
            &invalid_dmc_size, &invalid_provenance, message, message_size) == 0 ||
        invalid_sfx != NULL || invalid_dmc != NULL ||
        invalid_sfx_size != 0U || invalid_dmc_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct builder offset guard accepted an invalid layout.");
        free(invalid_sfx);
        free(invalid_dmc);
        free(rom);
        return -1;
    }
    free(invalid_sfx);
    free(invalid_dmc);

    invalid_sfx = NULL;
    invalid_dmc = NULL;
    invalid_sfx_size = 0U;
    invalid_dmc_size = 0U;
    if (tecmo_asset_pack_build_gameplay_audio(
            rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 0U, 1,
            &invalid_sfx, &invalid_sfx_size, &invalid_dmc,
            &invalid_dmc_size, &invalid_provenance, message, message_size) == 0 ||
        invalid_sfx != NULL || invalid_dmc != NULL ||
        invalid_sfx_size != 0U || invalid_dmc_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct builder zero-bank guard accepted an invalid layout.");
        free(invalid_sfx);
        free(invalid_dmc);
        free(rom);
        return -1;
    }

    invalid_sfx = NULL;
    invalid_dmc = NULL;
    invalid_sfx_size = 0U;
    invalid_dmc_size = 0U;
    if (tecmo_asset_pack_build_gameplay_audio(
            rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 7U, 1,
            &invalid_sfx, &invalid_sfx_size, &invalid_dmc,
            &invalid_dmc_size, &invalid_provenance, message, message_size) == 0 ||
        invalid_sfx != NULL || invalid_dmc != NULL ||
        invalid_sfx_size != 0U || invalid_dmc_size != 0U) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct builder undersized-bank guard accepted an invalid layout.");
        free(invalid_sfx);
        free(invalid_dmc);
        free(rom);
        return -1;
    }
    free(invalid_sfx);
    free(invalid_dmc);

    /* Revision validation deliberately precedes the full-ROM identity check:
       bounded gameplay source mutations must fail as gameplay-source
       mismatches, not as an indistinguishable final SHA rejection. */
    result = tecmo_asset_pack_build_gameplay_audio(
        rom, rom_size, sizeof(frontend_audio_rev1_ines_header), 8U, 1,
        &sfx_payload, &sfx_payload_size, &dmc_payload, &dmc_payload_size,
        &provenance, message, message_size);
    if (result == 0 &&
        (sfx_payload_size != 2824U || dmc_payload_size != 2515U ||
         provenance.sfx_payload_size != 2824U ||
         provenance.sfx_payload_fingerprint != 0x968A5DE6U ||
         provenance.sfx_instruction_count != 131U ||
         provenance.sfx_voice_count != 14U ||
         provenance.dmc_payload_size != 2515U ||
         provenance.dmc_payload_fingerprint != 0xAD70E6E8U ||
         tecmo_asset_pack_fnv1a32(sfx_payload, sfx_payload_size) !=
             0x968A5DE6U ||
         tecmo_asset_pack_fnv1a32(dmc_payload, dmc_payload_size) !=
             0xAD70E6E8U)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "Rev1 TSFX-1/TDMC-1 gameplay source postconditions mismatch.");
        result = -1;
    }
    if (result == 0 &&
        (tecmo_asset_pack_sha256_digest(
             rom, (size_t)rom_size, input_sha256) != 0 ||
         memcmp(input_sha256, frontend_audio_rev1_sha256,
                sizeof(input_sha256)) != 0)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSFX-1/TDMC-1 direct source test full-ROM SHA-256 mismatch.");
        result = -1;
    }
    if (result == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "Built strict ROM-derived TSFX-1/TDMC-1 gameplay audio source.");
    }
    free(sfx_payload);
    free(dmc_payload);
    free(rom);
    return result;
}
