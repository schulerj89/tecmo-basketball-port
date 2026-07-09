#ifndef TECMO_ASSET_PACK_H
#define TECMO_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_ID_SIZE 64U
#define TECMO_ASSET_PACK_FOURCC(a, b, c, d) \
    (((uint32_t)(uint8_t)(a)) |              \
     ((uint32_t)(uint8_t)(b) << 8U) |        \
     ((uint32_t)(uint8_t)(c) << 16U) |       \
     ((uint32_t)(uint8_t)(d) << 24U))

#define TECMO_ASSET_PACK_TYPE_META TECMO_ASSET_PACK_FOURCC('M', 'E', 'T', 'A')
#define TECMO_ASSET_PACK_TYPE_PRG TECMO_ASSET_PACK_FOURCC('P', 'R', 'G', ' ')
#define TECMO_ASSET_PACK_TYPE_CHR TECMO_ASSET_PACK_FOURCC('C', 'H', 'R', ' ')
#define TECMO_ASSET_PACK_TYPE_DATA TECMO_ASSET_PACK_FOURCC('D', 'A', 'T', 'A')

#define TECMO_ASSET_PACK_FLAG_RAW_ROM 0x00000001U
#define TECMO_ASSET_PACK_FLAG_DERIVED 0x00000002U
#define TECMO_ASSET_PACK_FLAG_LOCAL 0x00000004U

typedef struct TecmoAssetPackBuilder TecmoAssetPackBuilder;

typedef struct TecmoAssetPackEntryInfo {
    const char *id;
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t source_offset;
    uint32_t flags;
} TecmoAssetPackEntryInfo;

typedef struct TecmoAssetPackDirectoryEntryInfo {
    const char *id;
    uint32_t type;
    uint32_t bank;
    uint32_t cpu_address;
    uint64_t source_offset;
    uint64_t byte_count;
    uint32_t flags;
} TecmoAssetPackDirectoryEntryInfo;

typedef int (*TecmoAssetPackListCallback)(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                          void *user_data);

int tecmo_asset_pack_builder_begin(TecmoAssetPackBuilder **builder_out,
                                   const char *out_path,
                                   char *message,
                                   size_t message_size);

int tecmo_asset_pack_builder_add_memory(TecmoAssetPackBuilder *builder,
                                        const TecmoAssetPackEntryInfo *entry_info,
                                        const void *data,
                                        uint64_t byte_count,
                                        char *message,
                                        size_t message_size);

int tecmo_asset_pack_builder_add_file(TecmoAssetPackBuilder *builder,
                                      const TecmoAssetPackEntryInfo *entry_info,
                                      const char *local_path,
                                      char *message,
                                      size_t message_size);

/* Finish and cancel both release the builder handle. */
int tecmo_asset_pack_builder_finish(TecmoAssetPackBuilder *builder,
                                    char *message,
                                    size_t message_size);

void tecmo_asset_pack_builder_cancel(TecmoAssetPackBuilder *builder);

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size);

int tecmo_asset_pack_read_entry(const char *pack_path,
                                const char *entry_id,
                                uint8_t **bytes_out,
                                uint64_t *byte_count);

/* Callback id pointers are valid only for the duration of each callback call.
   A non-zero callback result stops enumeration and is returned to the caller. */
int tecmo_asset_pack_list_entries(const char *pack_path,
                                  TecmoAssetPackListCallback callback,
                                  void *user_data);

/* Writes a human-readable directory listing. required_size_out includes the NUL terminator. */
int tecmo_asset_pack_dump_directory(const char *pack_path,
                                    char *buffer,
                                    size_t buffer_size,
                                    size_t *required_size_out);

int tecmo_asset_pack_self_test(char *message, size_t message_size);

void tecmo_asset_pack_free(void *buffer);

#endif
