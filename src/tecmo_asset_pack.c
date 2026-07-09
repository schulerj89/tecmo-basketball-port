#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_ASSET_PACK_VERSION 1U
#define TECMO_ASSET_PACK_HEADER_SIZE 40U
#define TECMO_ASSET_PACK_ENTRY_SIZE 128U
#define TECMO_ASSET_PACK_PRG_BANK_BYTES 0x4000ULL
#define TECMO_ASSET_PACK_CHR_BANK_BYTES 0x2000ULL

typedef struct TecmoAssetPackBuildEntry {
    char id[TECMO_ASSET_PACK_ID_SIZE];
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t rom_offset;
    uint64_t pack_offset;
    uint64_t size;
    uint32_t flags;
} TecmoAssetPackBuildEntry;

typedef struct TecmoAssetPackDirectoryEntry {
    char id[TECMO_ASSET_PACK_ID_SIZE];
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t rom_offset;
    uint64_t pack_offset;
    uint64_t size;
    uint32_t flags;
} TecmoAssetPackDirectoryEntry;

static uint32_t pack_fourcc(char a, char b, char c, char d)
{
    return ((uint32_t)(uint8_t)a) |
           ((uint32_t)(uint8_t)b << 8U) |
           ((uint32_t)(uint8_t)c << 16U) |
           ((uint32_t)(uint8_t)d << 24U);
}

static void set_message(char *message, size_t message_size, const char *text)
{
    if (message_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(message, message_size, "%s", text);
}

static int write_u32(FILE *file, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)(value & 0xFFU);
    bytes[1] = (uint8_t)((value >> 8U) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 16U) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 24U) & 0xFFU);
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static int write_u64(FILE *file, uint64_t value)
{
    uint8_t bytes[8];

    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = (uint8_t)((value >> (unsigned)(i * 8U)) & 0xFFU);
    }
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes) ? 0 : -1;
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0;

    for (size_t i = 0; i < 8U; ++i) {
        value |= (uint64_t)bytes[i] << (unsigned)(i * 8U);
    }
    return value;
}

static int write_header(FILE *file,
                        uint32_t entry_count,
                        uint64_t directory_offset,
                        uint64_t data_offset)
{
    if (fwrite("TAP1", 1U, 4U, file) != 4U ||
        write_u32(file, TECMO_ASSET_PACK_VERSION) != 0 ||
        write_u32(file, TECMO_ASSET_PACK_HEADER_SIZE) != 0 ||
        write_u32(file, TECMO_ASSET_PACK_ENTRY_SIZE) != 0 ||
        write_u32(file, entry_count) != 0 ||
        write_u64(file, directory_offset) != 0 ||
        write_u64(file, data_offset) != 0 ||
        write_u32(file, 0U) != 0) {
        return -1;
    }
    return 0;
}

static int write_directory_entry(FILE *file, const TecmoAssetPackBuildEntry *entry)
{
    uint8_t padding[20];

    memset(padding, 0, sizeof(padding));
    if (fwrite(entry->id, 1U, TECMO_ASSET_PACK_ID_SIZE, file) != TECMO_ASSET_PACK_ID_SIZE ||
        write_u32(file, entry->type) != 0 ||
        write_u32(file, entry->bank) != 0 ||
        write_u32(file, entry->cpu_address) != 0 ||
        write_u64(file, entry->rom_offset) != 0 ||
        write_u64(file, entry->pack_offset) != 0 ||
        write_u64(file, entry->size) != 0 ||
        write_u32(file, entry->flags) != 0 ||
        write_u32(file, 0U) != 0 ||
        fwrite(padding, 1U, sizeof(padding), file) != sizeof(padding)) {
        return -1;
    }
    return 0;
}

static int read_directory_entry(FILE *file, TecmoAssetPackDirectoryEntry *entry)
{
    uint8_t bytes[TECMO_ASSET_PACK_ENTRY_SIZE];

    if (fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes)) {
        return -1;
    }

    memset(entry, 0, sizeof(*entry));
    memcpy(entry->id, bytes, TECMO_ASSET_PACK_ID_SIZE);
    entry->id[TECMO_ASSET_PACK_ID_SIZE - 1U] = '\0';
    entry->type = read_u32(bytes + 64U);
    entry->bank = read_u32(bytes + 68U);
    entry->cpu_address = read_u32(bytes + 72U);
    entry->rom_offset = read_u64(bytes + 76U);
    entry->pack_offset = read_u64(bytes + 84U);
    entry->size = read_u64(bytes + 92U);
    entry->flags = read_u32(bytes + 100U);
    return 0;
}

static int add_entry(TecmoAssetPackBuildEntry **entries,
                     size_t *count,
                     size_t *capacity,
                     FILE *out,
                     const char *id,
                     uint32_t type,
                     uint32_t bank,
                     uint32_t cpu_address,
                     uint64_t rom_offset,
                     const uint8_t *data,
                     uint64_t size)
{
    TecmoAssetPackBuildEntry *entry;
    long position;

    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0U ? 32U : *capacity * 2U;
        TecmoAssetPackBuildEntry *new_entries =
            (TecmoAssetPackBuildEntry *)realloc(*entries, new_capacity * sizeof((*entries)[0]));
        if (new_entries == NULL) {
            return -1;
        }
        *entries = new_entries;
        *capacity = new_capacity;
    }

    position = ftell(out);
    if (position < 0) {
        return -1;
    }
    if (size > 0U && fwrite(data, 1U, (size_t)size, out) != (size_t)size) {
        return -1;
    }

    entry = &(*entries)[*count];
    memset(entry, 0, sizeof(*entry));
    (void)snprintf(entry->id, sizeof(entry->id), "%s", id);
    entry->type = type;
    entry->bank = bank;
    entry->cpu_address = cpu_address;
    entry->rom_offset = rom_offset;
    entry->pack_offset = (uint64_t)position;
    entry->size = size;
    entry->flags = 0U;
    ++(*count);
    return 0;
}

static int read_file(const char *path, uint8_t **bytes_out, uint64_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long size;
    uint8_t *bytes;

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL && size > 0) {
        fclose(file);
        return -1;
    }
    if (size > 0 && fread(bytes, 1U, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return -1;
    }
    fclose(file);

    *bytes_out = bytes;
    *size_out = (uint64_t)size;
    return 0;
}

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0;
    uint64_t prg_offset;
    uint64_t prg_size;
    uint64_t chr_offset;
    uint64_t chr_size;
    uint32_t prg_banks;
    uint32_t chr_banks;
    uint32_t mapper;
    uint32_t trainer_bytes;
    FILE *out = NULL;
    TecmoAssetPackBuildEntry *entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;
    uint64_t directory_offset;
    const uint32_t meta_type = pack_fourcc('M', 'E', 'T', 'A');
    const uint32_t prg_type = pack_fourcc('P', 'R', 'G', ' ');
    const uint32_t chr_type = pack_fourcc('C', 'H', 'R', ' ');
    char manifest[256];
    int result = -1;

    if (rom_path == NULL || out_path == NULL) {
        set_message(message, message_size, "ROM path and output path are required.");
        return -1;
    }
    if (read_file(rom_path, &rom, &rom_size) != 0) {
        set_message(message, message_size, "Could not read iNES ROM.");
        return -1;
    }
    if (rom_size < 16U ||
        rom[0] != 'N' ||
        rom[1] != 'E' ||
        rom[2] != 'S' ||
        rom[3] != 0x1AU) {
        set_message(message, message_size, "Input is not an iNES ROM.");
        goto cleanup;
    }

    prg_banks = rom[4];
    chr_banks = rom[5];
    mapper = (uint32_t)(rom[6] >> 4U) | ((uint32_t)rom[7] & 0xF0U);
    trainer_bytes = (rom[6] & 0x04U) != 0U ? 512U : 0U;
    prg_offset = 16ULL + (uint64_t)trainer_bytes;
    prg_size = (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    chr_offset = prg_offset + prg_size;
    chr_size = (uint64_t)chr_banks * TECMO_ASSET_PACK_CHR_BANK_BYTES;

    if (prg_banks == 0U || rom_size < chr_offset + chr_size) {
        set_message(message, message_size, "ROM is shorter than its iNES PRG/CHR sizes.");
        goto cleanup;
    }

    out = fopen(out_path, "wb");
    if (out == NULL) {
        set_message(message, message_size, "Could not open asset pack output.");
        goto cleanup;
    }
    if (write_header(out, 0U, 0U, TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size, "Could not write asset pack header.");
        goto cleanup;
    }

    (void)snprintf(manifest,
                   sizeof(manifest),
                   "format=tecmo.assetpack/1\nsource=ines\nmapper=%u\nprg_banks_16k=%u\nchr_banks_8k=%u\n",
                   mapper,
                   prg_banks,
                   chr_banks);
    if (add_entry(&entries,
                  &entry_count,
                  &entry_capacity,
                  out,
                  "system/manifest",
                  meta_type,
                  0U,
                  0U,
                  0U,
                  (const uint8_t *)manifest,
                  (uint64_t)strlen(manifest)) != 0) {
        set_message(message, message_size, "Could not write asset pack manifest.");
        goto cleanup;
    }

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        if (add_entry(&entries,
                      &entry_count,
                      &entry_capacity,
                      out,
                      id,
                      prg_type,
                      bank,
                      0x8000U,
                      offset,
                      rom + offset,
                      TECMO_ASSET_PACK_PRG_BANK_BYTES) != 0) {
            set_message(message, message_size, "Could not write PRG bank asset.");
            goto cleanup;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        if (add_entry(&entries,
                      &entry_count,
                      &entry_capacity,
                      out,
                      "prg/fixed",
                      prg_type,
                      fixed_bank,
                      0xC000U,
                      offset,
                      rom + offset,
                      TECMO_ASSET_PACK_PRG_BANK_BYTES) != 0) {
            set_message(message, message_size, "Could not write fixed PRG bank asset.");
            goto cleanup;
        }
    }

    if (chr_size > 0U) {
        if (add_entry(&entries,
                      &entry_count,
                      &entry_capacity,
                      out,
                      "chr/all",
                      chr_type,
                      0U,
                      0U,
                      chr_offset,
                      rom + chr_offset,
                      chr_size) != 0) {
            set_message(message, message_size, "Could not write CHR asset.");
            goto cleanup;
        }
        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;
            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            if (add_entry(&entries,
                          &entry_count,
                          &entry_capacity,
                          out,
                          id,
                          chr_type,
                          bank,
                          0U,
                          offset,
                          rom + offset,
                          TECMO_ASSET_PACK_CHR_BANK_BYTES) != 0) {
                set_message(message, message_size, "Could not write CHR bank asset.");
                goto cleanup;
            }
        }
    }

    {
        long dir_pos = ftell(out);
        if (dir_pos < 0) {
            set_message(message, message_size, "Could not locate asset pack directory.");
            goto cleanup;
        }
        directory_offset = (uint64_t)dir_pos;
    }

    for (size_t i = 0; i < entry_count; ++i) {
        if (write_directory_entry(out, &entries[i]) != 0) {
            set_message(message, message_size, "Could not write asset pack directory.");
            goto cleanup;
        }
    }

    if (fseek(out, 0L, SEEK_SET) != 0 ||
        write_header(out,
                     (uint32_t)entry_count,
                     directory_offset,
                     TECMO_ASSET_PACK_HEADER_SIZE) != 0) {
        set_message(message, message_size, "Could not finalize asset pack header.");
        goto cleanup;
    }

    (void)snprintf(message,
                   message_size,
                   "Wrote %u PRG banks, %u CHR banks, %u entries to %s",
                   prg_banks,
                   chr_banks,
                   (unsigned)entry_count,
                   out_path);
    result = 0;

cleanup:
    if (out != NULL) {
        fclose(out);
    }
    free(entries);
    free(rom);
    return result;
}

int tecmo_asset_pack_read_entry(const char *pack_path,
                                const char *entry_id,
                                uint8_t **bytes_out,
                                uint64_t *byte_count)
{
    FILE *file;
    uint8_t header[TECMO_ASSET_PACK_HEADER_SIZE];
    uint32_t version;
    uint32_t header_size;
    uint32_t entry_size;
    uint32_t entry_count;
    uint64_t directory_offset;
    uint8_t *bytes = NULL;
    int result = -1;

    if (pack_path == NULL || entry_id == NULL || bytes_out == NULL || byte_count == NULL) {
        return -1;
    }
    *bytes_out = NULL;
    *byte_count = 0U;

    file = fopen(pack_path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "TAP1", 4U) != 0) {
        goto cleanup;
    }

    version = read_u32(header + 4U);
    header_size = read_u32(header + 8U);
    entry_size = read_u32(header + 12U);
    entry_count = read_u32(header + 16U);
    directory_offset = read_u64(header + 20U);
    if (version != TECMO_ASSET_PACK_VERSION ||
        header_size != TECMO_ASSET_PACK_HEADER_SIZE ||
        entry_size != TECMO_ASSET_PACK_ENTRY_SIZE ||
        fseek(file, (long)directory_offset, SEEK_SET) != 0) {
        goto cleanup;
    }

    for (uint32_t i = 0; i < entry_count; ++i) {
        TecmoAssetPackDirectoryEntry entry;
        if (read_directory_entry(file, &entry) != 0) {
            goto cleanup;
        }
        if (strncmp(entry.id, entry_id, TECMO_ASSET_PACK_ID_SIZE) == 0) {
            if (entry.size > (uint64_t)SIZE_MAX) {
                goto cleanup;
            }
            bytes = (uint8_t *)malloc((size_t)entry.size);
            if (bytes == NULL && entry.size > 0U) {
                goto cleanup;
            }
            if (fseek(file, (long)entry.pack_offset, SEEK_SET) != 0 ||
                (entry.size > 0U &&
                 fread(bytes, 1U, (size_t)entry.size, file) != (size_t)entry.size)) {
                free(bytes);
                bytes = NULL;
                goto cleanup;
            }
            *bytes_out = bytes;
            *byte_count = entry.size;
            bytes = NULL;
            result = 0;
            goto cleanup;
        }
    }

cleanup:
    free(bytes);
    fclose(file);
    return result;
}

void tecmo_asset_pack_free(void *buffer)
{
    free(buffer);
}
