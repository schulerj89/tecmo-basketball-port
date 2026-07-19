#include "tecmo_asset_pack_music.h"

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum MusicInstructionType {
    MUSIC_INSTRUCTION_NOTE = 1,
    MUSIC_INSTRUCTION_VOICE = 2,
    MUSIC_INSTRUCTION_LEGATO = 3,
    MUSIC_INSTRUCTION_PITCH = 4,
    MUSIC_INSTRUCTION_END = 5,
    MUSIC_INSTRUCTION_REST = 6,
    MUSIC_INSTRUCTION_LOOP = 7,
    MUSIC_INSTRUCTION_BIND_PHRASES = 8,
    MUSIC_INSTRUCTION_CALL = 9,
    MUSIC_INSTRUCTION_RETURN = 10
};

typedef struct MusicTrackSource {
    uint8_t id;
    uint16_t begin;
    uint16_t end;
    uint32_t fingerprint;
} MusicTrackSource;

typedef struct ImportedMusicInstruction {
    uint16_t address;
    uint16_t next_address;
    uint16_t target_address;
    uint16_t value16;
    int16_t signed_value;
    uint16_t loop_slot;
    uint8_t type;
    uint8_t value8;
    uint8_t byte_count;
} ImportedMusicInstruction;

typedef struct ImportedMusicChannel {
    ImportedMusicInstruction instructions[TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS];
    int16_t address_to_index[0x4000];
    int16_t byte_owner[0x4000];
    uint16_t work[TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS];
    uint16_t instruction_count;
    uint16_t work_count;
    uint16_t loop_count;
    uint16_t entry_index;
} ImportedMusicChannel;

typedef struct ImportedMusic {
    ImportedMusicChannel channels[TECMO_ASSET_PACK_MUSIC_TRACK_COUNT]
                                 [TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT];
    uint8_t voices[TECMO_ASSET_PACK_MUSIC_MAX_VOICES][8];
    uint16_t voice_count;
    uint16_t pitch[TECMO_ASSET_PACK_MUSIC_PITCH_COUNT];
    uint32_t instruction_count;
    uint16_t max_call_depth;
} ImportedMusic;

static const MusicTrackSource music_tracks[TECMO_ASSET_PACK_MUSIC_TRACK_COUNT] = {
    {5U, 0x92F4U, 0x96C3U, 0x1270498BU},
    {6U, 0x96C3U, 0x9D8BU, 0xBD91FCF1U},
    {7U, 0x8CE2U, 0x92F4U, 0x69F85EC2U},
    {8U, 0x9E13U, 0x9F06U, 0x8122C6CFU}
};

static int range_valid(uint64_t offset, uint64_t count, uint64_t size)
{
    return offset <= size && count <= size - offset;
}

static uint64_t bank_offset(uint64_t prg_offset, uint32_t bank, uint16_t address)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(address - 0x8000U);
}

static uint64_t fixed_offset(uint64_t prg_offset, uint32_t prg_banks,
                             uint16_t address)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES +
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
                      ((uint16_t)source_byte(rom, bank04_offset,
                                             (uint16_t)(address + 1U)) << 8U));
}

static int add_voice(ImportedMusic *music, const uint8_t voice[8])
{
    uint16_t i;
    for (i = 0U; i < music->voice_count; ++i) {
        if (memcmp(music->voices[i], voice, 8U) == 0) return (int)i;
    }
    if (music->voice_count >= TECMO_ASSET_PACK_MUSIC_MAX_VOICES) return -1;
    memcpy(music->voices[music->voice_count], voice, 8U);
    return (int)music->voice_count++;
}

static int queue_address(ImportedMusicChannel *channel, uint16_t address)
{
    if (address < 0x8000U || address >= 0xC000U) return -1;
    if (channel->address_to_index[address - 0x8000U] >= 0) return 0;
    if (channel->work_count >= TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS)
        return -1;
    channel->work[channel->work_count++] = address;
    return 0;
}

static int claim_instruction_bytes(ImportedMusicChannel *channel,
                                   uint16_t address,
                                   uint8_t byte_count,
                                   uint16_t track_begin,
                                   uint16_t track_end)
{
    uint16_t i;
    if (address < track_begin || address >= track_end ||
        byte_count == 0U || (uint32_t)address + byte_count > track_end)
        return -1;
    for (i = 0U; i < byte_count; ++i) {
        int16_t owner = channel->byte_owner[address - 0x8000U + i];
        if (owner >= 0 && (uint16_t)owner != channel->instruction_count) return -1;
    }
    for (i = 0U; i < byte_count; ++i)
        channel->byte_owner[address - 0x8000U + i] =
            (int16_t)channel->instruction_count;
    return 0;
}

static int source_span_valid(uint16_t address, uint8_t byte_count,
                             const MusicTrackSource *track)
{
    return address >= track->begin && address < track->end &&
           byte_count != 0U &&
           (uint32_t)address + byte_count <= track->end;
}

static int parse_instruction(ImportedMusic *music,
                             ImportedMusicChannel *channel,
                             const uint8_t *rom,
                             uint64_t bank04_offset,
                             const MusicTrackSource *track,
                             uint16_t phrase_directory,
                             uint8_t channel_index,
                             uint16_t address,
                             char *message,
                             size_t message_size)
{
    ImportedMusicInstruction instruction;
    uint8_t opcode;
    int voice_index;
    uint16_t target;
    uint16_t i;

    if (address < track->begin || address >= track->end) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Music control target is outside its track.");
        return -1;
    }
    if (channel->address_to_index[address - 0x8000U] >= 0) return 0;
    if (channel->instruction_count >= TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Music channel has too many semantic instructions.");
        return -1;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.address = address;
    instruction.next_address = UINT16_MAX;
    instruction.target_address = UINT16_MAX;
    instruction.loop_slot = UINT16_MAX;
    opcode = source_byte(rom, bank04_offset, address);

    if (opcode < 0x80U) {
        if (!source_span_valid(address, 2U, track)) goto truncated;
        uint8_t note = source_byte(rom, bank04_offset, (uint16_t)(address + 1U));
        uint16_t pitch_index = channel_index == 2U ? (uint16_t)(note + 12U) : note;
        if (opcode == 0U || (channel_index != 3U && pitch_index >= TECMO_ASSET_PACK_MUSIC_PITCH_COUNT)) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music note duration or pitch is out of range.");
            return -1;
        }
        instruction.type = MUSIC_INSTRUCTION_NOTE;
        instruction.value8 = note;
        instruction.value16 = opcode;
        instruction.byte_count = 2U;
    } else if (opcode == 0x80U) {
        uint8_t voice[8];
        if (!source_span_valid(address, 9U, track)) goto truncated;
        for (i = 0U; i < 8U; ++i)
            voice[i] = source_byte(rom, bank04_offset,
                                   (uint16_t)(address + 1U + i));
        if (voice[2] != 0U || (voice[1] & 0xF0U) != 0U) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music voice uses an unsupported Rev1 sweep or volume field.");
            return -1;
        }
        voice_index = add_voice(music, voice);
        if (voice_index < 0) return -1;
        instruction.type = MUSIC_INSTRUCTION_VOICE;
        instruction.value8 = (uint8_t)voice_index;
        instruction.byte_count = 9U;
    } else if (opcode == 0x90U) {
        instruction.type = MUSIC_INSTRUCTION_LEGATO;
        instruction.byte_count = 1U;
    } else if (opcode == 0x91U) {
        if (!source_span_valid(address, 2U, track)) goto truncated;
        instruction.type = MUSIC_INSTRUCTION_PITCH;
        instruction.signed_value = (int8_t)source_byte(
            rom, bank04_offset, (uint16_t)(address + 1U));
        instruction.byte_count = 2U;
    } else if (opcode == 0xA0U) {
        instruction.type = MUSIC_INSTRUCTION_END;
        instruction.byte_count = 1U;
    } else if (opcode == 0xB0U) {
        if (!source_span_valid(address, 2U, track)) goto truncated;
        instruction.type = MUSIC_INSTRUCTION_REST;
        instruction.value16 = (uint16_t)(source_byte(
            rom, bank04_offset, (uint16_t)(address + 1U)) & 0x7FU);
        instruction.byte_count = 2U;
        if (instruction.value16 == 0U) return -1;
    } else if (opcode == 0xC0U) {
        if (!source_span_valid(address, 4U, track)) goto truncated;
        target = source_word(rom, bank04_offset, (uint16_t)(address + 1U));
        if (target < track->begin || target >= track->end ||
            channel->loop_count >= TECMO_ASSET_PACK_MUSIC_MAX_LOOPS) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music loop target or loop count is invalid.");
            return -1;
        }
        instruction.type = MUSIC_INSTRUCTION_LOOP;
        instruction.value16 = source_byte(rom, bank04_offset,
                                          (uint16_t)(address + 3U));
        if (instruction.value16 == 0U) instruction.value16 = 256U;
        instruction.loop_slot = 0U;
        ++channel->loop_count;
        instruction.target_address = target;
        instruction.byte_count = 4U;
    } else if (opcode == 0xD0U) {
        if (!source_span_valid(address, 3U, track)) goto truncated;
        target = source_word(rom, bank04_offset, (uint16_t)(address + 1U));
        if (target != phrase_directory) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music phrase binding does not match its track directory.");
            return -1;
        }
        instruction.type = MUSIC_INSTRUCTION_BIND_PHRASES;
        instruction.byte_count = 3U;
    } else if (opcode >= 0xE0U && opcode <= 0xFEU) {
        uint16_t pointer_address = (uint16_t)(phrase_directory +
                                             (uint16_t)(opcode - 0xE0U) * 2U);
        if (pointer_address < track->begin ||
            (uint32_t)pointer_address + 2U > track->end) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music phrase index is out of range.");
            return -1;
        }
        target = source_word(rom, bank04_offset, pointer_address);
        if (target < track->begin || target >= track->end) return -1;
        instruction.type = MUSIC_INSTRUCTION_CALL;
        instruction.target_address = target;
        instruction.byte_count = 1U;
    } else if (opcode == 0xFFU) {
        instruction.type = MUSIC_INSTRUCTION_RETURN;
        instruction.byte_count = 1U;
    } else {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Music stream contains an unsupported command.");
        return -1;
    }

    if (claim_instruction_bytes(channel, address, instruction.byte_count,
                                track->begin, track->end) != 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Music target overlaps another instruction.");
        return -1;
    }
    channel->address_to_index[address - 0x8000U] =
        (int16_t)channel->instruction_count;
    channel->instructions[channel->instruction_count++] = instruction;

    if (instruction.type != MUSIC_INSTRUCTION_END &&
        instruction.type != MUSIC_INSTRUCTION_RETURN) {
        uint16_t next = (uint16_t)(address + instruction.byte_count);
        channel->instructions[channel->instruction_count - 1U].next_address = next;
        if (queue_address(channel, next) != 0) return -1;
    }
    if ((instruction.type == MUSIC_INSTRUCTION_LOOP ||
         instruction.type == MUSIC_INSTRUCTION_CALL) &&
        queue_address(channel, instruction.target_address) != 0)
        return -1;
    return 0;

truncated:
    tecmo_asset_pack_set_message(message, message_size,
                                 "Music instruction is truncated by its track boundary.");
    return -1;
}

static int validate_call_walk(const ImportedMusicChannel *channel,
                              uint16_t instruction_index,
                              uint16_t depth,
                              uint16_t path[TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH],
                              uint8_t visited[TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH + 1U]
                                             [TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS],
                              uint16_t *max_depth)
{
    const ImportedMusicInstruction *instruction;
    int16_t next_index;
    int16_t target_index;
    uint16_t i;
    if (depth > TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH ||
        instruction_index >= channel->instruction_count)
        return -1;
    if (visited[depth][instruction_index] != 0U) return 0;
    visited[depth][instruction_index] = 1U;
    if (depth > *max_depth) *max_depth = depth;
    instruction = &channel->instructions[instruction_index];
    target_index = instruction->target_address == UINT16_MAX ? -1 :
        channel->address_to_index[instruction->target_address - 0x8000U];
    next_index = instruction->next_address == UINT16_MAX ? -1 :
        channel->address_to_index[instruction->next_address - 0x8000U];

    if (instruction->type == MUSIC_INSTRUCTION_CALL) {
        if (target_index < 0 || depth >= TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH)
            return -1;
        for (i = 0U; i < depth; ++i)
            if (path[i] == (uint16_t)target_index) return -1;
        path[depth] = (uint16_t)target_index;
        memset(visited[depth + 1U], 0,
               TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS);
        if (validate_call_walk(channel, (uint16_t)target_index,
                               (uint16_t)(depth + 1U), path, visited,
                               max_depth) != 0)
            return -1;
    } else if (instruction->type == MUSIC_INSTRUCTION_LOOP) {
        if (target_index < 0 ||
            validate_call_walk(channel, (uint16_t)target_index, depth,
                               path, visited, max_depth) != 0)
            return -1;
    }
    if (next_index >= 0)
        return validate_call_walk(channel, (uint16_t)next_index, depth,
                                  path, visited, max_depth);
    return instruction->type == MUSIC_INSTRUCTION_END ||
           instruction->type == MUSIC_INSTRUCTION_RETURN ? 0 : -1;
}

static int compile_channel(ImportedMusic *music,
                           ImportedMusicChannel *channel,
                           const uint8_t *rom,
                           uint64_t bank04_offset,
                           const MusicTrackSource *track,
                           uint16_t phrase_directory,
                           uint8_t channel_index,
                           uint16_t entry_address,
                           char *message,
                           size_t message_size)
{
    uint8_t (*visited)[TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS];
    uint16_t path[TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH];
    uint16_t max_depth = 0U;
    size_t i;
    memset(channel, 0, sizeof(*channel));
    for (i = 0U; i < 0x4000U; ++i) {
        channel->address_to_index[i] = -1;
        channel->byte_owner[i] = -1;
    }
    if (queue_address(channel, entry_address) != 0) return -1;
    while (channel->work_count > 0U) {
        uint16_t address = channel->work[--channel->work_count];
        if (parse_instruction(music, channel, rom, bank04_offset, track,
                              phrase_directory, channel_index, address,
                              message, message_size) != 0)
            return -1;
    }
    if (channel->address_to_index[entry_address - 0x8000U] < 0) return -1;
    channel->entry_index =
        (uint16_t)channel->address_to_index[entry_address - 0x8000U];
    visited = (uint8_t (*)[TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS])calloc(
        TECMO_ASSET_PACK_MUSIC_MAX_CALL_DEPTH + 1U,
        TECMO_ASSET_PACK_MUSIC_MAX_CHANNEL_INSTRUCTIONS);
    if (visited == NULL) return -1;
    memset(path, 0, sizeof(path));
    if (validate_call_walk(channel, channel->entry_index, 0U, path,
                           visited, &max_depth) != 0) {
        free(visited);
        tecmo_asset_pack_set_message(message, message_size,
                                     "Music phrase graph is recursive or exceeds the call-depth bound.");
        return -1;
    }
    free(visited);
    if (max_depth > music->max_call_depth) music->max_call_depth = max_depth;
    music->instruction_count += channel->instruction_count;
    if (music->instruction_count > TECMO_ASSET_PACK_MUSIC_MAX_INSTRUCTIONS)
        return -1;
    return 0;
}

static int compile_track(ImportedMusic *music,
                         unsigned track_index,
                         const uint8_t *rom,
                         uint64_t bank04_offset,
                         char *message,
                         size_t message_size)
{
    const MusicTrackSource *track = &music_tracks[track_index];
    uint16_t cursor = track->begin;
    uint16_t entries[TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT];
    unsigned channel;

    for (channel = 0U; channel < TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT; ++channel) {
        if ((uint32_t)cursor + 3U > track->end ||
            source_byte(rom, bank04_offset, cursor) != channel) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music descriptor channel order is malformed.");
            return -1;
        }
        entries[channel] = source_word(rom, bank04_offset,
                                       (uint16_t)(cursor + 1U));
        cursor = (uint16_t)(cursor + 3U);
    }
    if (cursor >= track->end || source_byte(rom, bank04_offset, cursor) != 0xFFU)
        return -1;
    ++cursor;
    for (channel = 0U; channel < TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT; ++channel) {
        if (compile_channel(music, &music->channels[track_index][channel],
                            rom, bank04_offset, track, cursor,
                            (uint8_t)channel, entries[channel],
                            message, message_size) != 0)
            return -1;
    }
    return 0;
}

static int validate_revision(const uint8_t *rom,
                             uint64_t bank04_offset,
                             uint64_t bank06_offset,
                             uint64_t fixed_bank_offset,
                             char *message,
                             size_t message_size)
{
    unsigned i;
    if (tecmo_asset_pack_fnv1a32(rom + (size_t)(bank04_offset + 0x0AA4U),
                                 TECMO_ASSET_PACK_MUSIC_SOURCE_SIZE) != 0x06F2A750U ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)(bank04_offset + 0x0CD0U),
                                 TECMO_ASSET_PACK_MUSIC_DIRECTORY_SIZE) != 0x59366EC4U ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank04_offset +
                TECMO_ASSET_PACK_MUSIC_OPENING_QUEUE_CPU - 0x8000U),
            TECMO_ASSET_PACK_MUSIC_OPENING_QUEUE_SIZE) != 0xFCDCAFEFU ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank04_offset +
                TECMO_ASSET_PACK_MUSIC_OPENING_FIRST_ROUTE_CPU - 0x8000U),
            TECMO_ASSET_PACK_MUSIC_OPENING_FIRST_ROUTE_SIZE) != 0x07FD2C8DU ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(fixed_bank_offset +
                TECMO_ASSET_PACK_MUSIC_MENU_QUEUE_CPU - 0xC000U),
            TECMO_ASSET_PACK_MUSIC_MENU_QUEUE_SIZE) != 0x0ADC9176U ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)(bank06_offset +
                TECMO_ASSET_PACK_MUSIC_PREGAME_MATCHUP_QUEUE_CPU - 0x8000U),
            TECMO_ASSET_PACK_MUSIC_PREGAME_MATCHUP_QUEUE_SIZE) != 0x1E564AC0U ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)(fixed_bank_offset + 0x32F2U),
                                 TECMO_ASSET_PACK_MUSIC_ENGINE_SIZE) != 0xFC6A0BC1U ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)(fixed_bank_offset + 0x393BU),
                                 TECMO_ASSET_PACK_MUSIC_PITCH_COUNT * 2U) != 0x3F5A394DU) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Rev1 music engine/data/queue fingerprint mismatch.");
        return -1;
    }
    for (i = 0U; i < TECMO_ASSET_PACK_MUSIC_TRACK_COUNT; ++i) {
        const MusicTrackSource *track = &music_tracks[i];
        if (source_word(rom, bank04_offset,
                        (uint16_t)(TECMO_ASSET_PACK_MUSIC_DIRECTORY_CPU +
                                   track->id * 2U)) != track->begin ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)(bank04_offset + track->begin - 0x8000U),
                (size_t)(track->end - track->begin)) != track->fingerprint) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Rev1 requested music-track fingerprint mismatch.");
            return -1;
        }
    }
    return 0;
}

static size_t align4(size_t value)
{
    return (value + 3U) & ~(size_t)3U;
}

static int serialize_music(const ImportedMusic *music,
                           const uint8_t *rom,
                           uint64_t fixed_bank_offset,
                           uint8_t **payload_out,
                           size_t *payload_size_out)
{
    size_t track_offset = TECMO_ASSET_PACK_MUSIC_HEADER_SIZE;
    size_t voice_offset = track_offset + TECMO_ASSET_PACK_MUSIC_TRACK_COUNT *
                                         TECMO_ASSET_PACK_MUSIC_TRACK_STRIDE;
    size_t pitch_offset = voice_offset + music->voice_count *
                                         TECMO_ASSET_PACK_MUSIC_VOICE_STRIDE;
    size_t instruction_offset = align4(pitch_offset +
        TECMO_ASSET_PACK_MUSIC_PITCH_COUNT * 2U);
    size_t payload_size = instruction_offset +
        music->instruction_count * TECMO_ASSET_PACK_MUSIC_INSTRUCTION_STRIDE;
    uint8_t *payload;
    uint32_t global_instruction = 0U;
    unsigned track_index;
    uint16_t voice;
    uint16_t pitch;

    payload = (uint8_t *)calloc(payload_size, 1U);
    if (payload == NULL) return -1;
    memcpy(payload, "TMUS", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_MUSIC_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 8U, (uint32_t)payload_size);
    tecmo_asset_pack_store_u32(payload + 12U, 0x06F2A750U);
    tecmo_asset_pack_store_u32(payload + 16U, 0xFC6A0BC1U);
    tecmo_asset_pack_store_u32(payload + 20U, 0x59366EC4U);
    tecmo_asset_pack_store_u32(payload + 24U, 0x3F5A394DU);
    tecmo_asset_pack_store_u16(payload + 28U, TECMO_ASSET_PACK_MUSIC_TRACK_COUNT);
    tecmo_asset_pack_store_u16(payload + 30U, TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT);
    tecmo_asset_pack_store_u16(payload + 32U, music->voice_count);
    tecmo_asset_pack_store_u16(payload + 34U, TECMO_ASSET_PACK_MUSIC_PITCH_COUNT);
    tecmo_asset_pack_store_u16(payload + 36U, TECMO_ASSET_PACK_MUSIC_TRACK_STRIDE);
    tecmo_asset_pack_store_u16(payload + 38U, TECMO_ASSET_PACK_MUSIC_VOICE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 40U, TECMO_ASSET_PACK_MUSIC_INSTRUCTION_STRIDE);
    tecmo_asset_pack_store_u16(payload + 42U, music->max_call_depth);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_MUSIC_SAMPLE_RATE);
    tecmo_asset_pack_store_u32(payload + 48U, TECMO_ASSET_PACK_MUSIC_TICK_NUMERATOR);
    tecmo_asset_pack_store_u32(payload + 52U, TECMO_ASSET_PACK_MUSIC_TICK_DENOMINATOR);
    tecmo_asset_pack_store_u32(payload + 56U, (uint32_t)track_offset);
    tecmo_asset_pack_store_u32(payload + 60U, (uint32_t)voice_offset);
    tecmo_asset_pack_store_u32(payload + 64U, (uint32_t)pitch_offset);
    tecmo_asset_pack_store_u32(payload + 68U, (uint32_t)instruction_offset);
    tecmo_asset_pack_store_u32(payload + 72U, music->instruction_count);
    tecmo_asset_pack_store_u16(payload + 76U,
                               TECMO_ASSET_PACK_MUSIC_LOOP_STATE_COUNT);
    for (track_index = 0U; track_index < TECMO_ASSET_PACK_MUSIC_TRACK_COUNT;
         ++track_index) {
        tecmo_asset_pack_store_u32(payload + 80U + track_index * 4U,
                                   music_tracks[track_index].fingerprint);
        payload[96U + track_index] = music_tracks[track_index].id;
    }

    for (voice = 0U; voice < music->voice_count; ++voice)
        memcpy(payload + voice_offset + voice * TECMO_ASSET_PACK_MUSIC_VOICE_STRIDE,
               music->voices[voice], TECMO_ASSET_PACK_MUSIC_VOICE_STRIDE);
    for (pitch = 0U; pitch < TECMO_ASSET_PACK_MUSIC_PITCH_COUNT; ++pitch) {
        uint16_t value = tecmo_asset_pack_read_u16(
            rom + (size_t)(fixed_bank_offset + 0x393BU + pitch * 2U));
        tecmo_asset_pack_store_u16(payload + pitch_offset + pitch * 2U, value);
    }

    for (track_index = 0U; track_index < TECMO_ASSET_PACK_MUSIC_TRACK_COUNT;
         ++track_index) {
        uint8_t *track_bytes = payload + track_offset +
            track_index * TECMO_ASSET_PACK_MUSIC_TRACK_STRIDE;
        unsigned channel_index;
        track_bytes[0] = music_tracks[track_index].id;
        tecmo_asset_pack_store_u16(track_bytes + 2U,
                                   TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT);
        tecmo_asset_pack_store_u32(track_bytes + 4U,
                                   music_tracks[track_index].fingerprint);
        for (channel_index = 0U;
             channel_index < TECMO_ASSET_PACK_MUSIC_CHANNEL_COUNT;
             ++channel_index) {
            const ImportedMusicChannel *channel =
                &music->channels[track_index][channel_index];
            uint32_t channel_base = global_instruction;
            uint16_t instruction_index;
            tecmo_asset_pack_store_u32(track_bytes + 8U + channel_index * 8U,
                                       channel_base + channel->entry_index);
            tecmo_asset_pack_store_u32(track_bytes + 12U + channel_index * 8U,
                                       channel->instruction_count);
            for (instruction_index = 0U;
                 instruction_index < channel->instruction_count;
                 ++instruction_index) {
                const ImportedMusicInstruction *source =
                    &channel->instructions[instruction_index];
                uint8_t *dest = payload + instruction_offset +
                    (channel_base + instruction_index) *
                        TECMO_ASSET_PACK_MUSIC_INSTRUCTION_STRIDE;
                int16_t next = source->next_address == UINT16_MAX ? -1 :
                    channel->address_to_index[source->next_address - 0x8000U];
                int16_t target = source->target_address == UINT16_MAX ? -1 :
                    channel->address_to_index[source->target_address - 0x8000U];
                dest[0] = source->type;
                dest[1] = source->value8;
                tecmo_asset_pack_store_u16(dest + 2U, source->value16);
                tecmo_asset_pack_store_u32(dest + 4U,
                    next < 0 ? UINT32_MAX : channel_base + (uint16_t)next);
                tecmo_asset_pack_store_u32(dest + 8U,
                    target < 0 ? UINT32_MAX : channel_base + (uint16_t)target);
                tecmo_asset_pack_store_u16(dest + 12U,
                                           (uint16_t)source->signed_value);
                tecmo_asset_pack_store_u16(dest + 14U, source->loop_slot);
            }
            global_instruction += channel->instruction_count;
        }
    }
    if (global_instruction != music->instruction_count) {
        free(payload);
        return -1;
    }
    *payload_out = payload;
    *payload_size_out = payload_size;
    return 0;
}

int tecmo_asset_pack_build_music(const uint8_t *rom,
                                 uint64_t rom_size,
                                 uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 int enforce_revision_fingerprints,
                                 uint8_t **payload_out,
                                 size_t *payload_size_out,
                                 TecmoMusicProvenance *provenance,
                                 char *message,
                                 size_t message_size)
{
    ImportedMusic *music = NULL;
    uint64_t bank04_source;
    uint64_t bank06_source;
    uint64_t fixed_source;
    unsigned i;
    int result = -1;
    if (rom == NULL || payload_out == NULL || payload_size_out == NULL ||
        provenance == NULL || prg_banks < 8U) return -1;
    *payload_out = NULL;
    *payload_size_out = 0U;
    memset(provenance, 0, sizeof(*provenance));
    bank04_source = bank_offset(prg_offset, 4U, 0x8000U);
    bank06_source = bank_offset(prg_offset, 6U, 0x8000U);
    fixed_source = fixed_offset(prg_offset, prg_banks, 0xC000U);
    if (!range_valid(bank04_source, TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !range_valid(bank06_source, TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size) ||
        !range_valid(fixed_source, TECMO_ASSET_PACK_PRG_BANK_BYTES, rom_size))
        return -1;
    if (enforce_revision_fingerprints &&
        validate_revision(rom, bank04_source, bank06_source, fixed_source,
                          message, message_size) != 0)
        return -1;

    music = (ImportedMusic *)calloc(1U, sizeof(*music));
    if (music == NULL) return -1;
    for (i = 0U; i < TECMO_ASSET_PACK_MUSIC_PITCH_COUNT; ++i) {
        music->pitch[i] = tecmo_asset_pack_read_u16(
            rom + (size_t)(fixed_source + 0x393BU + i * 2U));
        if (music->pitch[i] == 0U || music->pitch[i] > 0x7FFU ||
            (i > 0U && music->pitch[i] >= music->pitch[i - 1U])) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Music pitch table is malformed.");
            goto cleanup;
        }
    }
    for (i = 0U; i < TECMO_ASSET_PACK_MUSIC_TRACK_COUNT; ++i) {
        if (compile_track(music, i, rom, bank04_source,
                          message, message_size) != 0)
            goto cleanup;
    }
    if (enforce_revision_fingerprints && music->voice_count != 37U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Rev1 music voice count mismatch.");
        goto cleanup;
    }
    if (serialize_music(music, rom, fixed_source,
                        payload_out, payload_size_out) != 0)
        goto cleanup;

    provenance->source_offset = bank04_source + 0x0AA4U;
    provenance->directory_offset = bank04_source + 0x0CD0U;
    provenance->engine_offset = fixed_source + 0x32F2U;
    provenance->pitch_offset = fixed_source + 0x393BU;
    provenance->opening_queue_offset = bank04_source +
        TECMO_ASSET_PACK_MUSIC_OPENING_QUEUE_CPU - 0x8000U;
    provenance->opening_first_route_offset = bank04_source +
        TECMO_ASSET_PACK_MUSIC_OPENING_FIRST_ROUTE_CPU - 0x8000U;
    provenance->menu_queue_offset = fixed_source +
        TECMO_ASSET_PACK_MUSIC_MENU_QUEUE_CPU - 0xC000U;
    provenance->pregame_matchup_queue_offset = bank06_source +
        TECMO_ASSET_PACK_MUSIC_PREGAME_MATCHUP_QUEUE_CPU - 0x8000U;
    for (i = 0U; i < TECMO_ASSET_PACK_MUSIC_TRACK_COUNT; ++i) {
        provenance->track_offsets[i] = bank04_source + music_tracks[i].begin - 0x8000U;
        provenance->track_sizes[i] = music_tracks[i].end - music_tracks[i].begin;
    }
    provenance->payload_size = (uint32_t)*payload_size_out;
    provenance->payload_fingerprint =
        tecmo_asset_pack_fnv1a32(*payload_out, *payload_size_out);
    provenance->instruction_count = music->instruction_count;
    provenance->voice_count = music->voice_count;
    result = 0;

cleanup:
    if (result != 0) {
        free(*payload_out);
        *payload_out = NULL;
        *payload_size_out = 0U;
    }
    free(music);
    return result;
}
