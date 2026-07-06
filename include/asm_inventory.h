#ifndef ASM_INVENTORY_H
#define ASM_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_MAX_PATH_TEXT 1024
#define TECMO_MAX_LABEL_TEXT 256
#define TECMO_MAX_NAME_TEXT 128
#define TECMO_TITLE_MAX_CHARS 64
#define TECMO_TITLE_SETUP_MAX_TARGETS 16
#define TECMO_TITLE_SETUP_TABLE_REFS 5
#define TECMO_TITLE_SETUP_STREAM_TABLE_ENTRIES 15
#define TECMO_TITLE_SETUP_SELECTOR_ROWS 5

typedef struct AsmStats {
    uint64_t files;
    uint64_t lines;
    uint64_t comments;
    uint64_t labels;
    uint64_t semantic_labels;
    uint64_t segment_directives;
    uint64_t include_directives;
    uint64_t byte_directives;
    uint64_t word_directives;
    uint64_t emitted_bytes;
    uint64_t instructions;
} AsmStats;

typedef struct RosterRecord {
    char team[TECMO_MAX_NAME_TEXT];
    char player[TECMO_MAX_NAME_TEXT];
    char label[TECMO_MAX_LABEL_TEXT];
    uint8_t attrs[7];
    bool has_attrs;
} RosterRecord;

typedef struct RosterTable {
    RosterRecord *records;
    size_t count;
    size_t capacity;
} RosterTable;

typedef struct TecmoTitleGlyph {
    uint8_t character;
    uint8_t render_x;
    uint16_t ppu_address;
    uint8_t tile_index;
    uint8_t glyph_tiles[4];
} TecmoTitleGlyph;

typedef struct TecmoAddressCount {
    uint16_t target;
    uint8_t count;
} TecmoAddressCount;

typedef struct TecmoTitleSetupSummary {
    bool loaded;
    uint16_t exact_entry_start;
    uint16_t exact_entry_end;
    uint16_t adjacent_driver_start;
    uint16_t adjacent_driver_end;
    uint16_t stream_copy_start;
    uint16_t stream_copy_end;
    uint8_t driver_call_count;
    uint8_t driver_call_invocations;
    TecmoAddressCount driver_calls[TECMO_TITLE_SETUP_MAX_TARGETS];
    uint8_t driver_write_count;
    TecmoAddressCount driver_writes[TECMO_TITLE_SETUP_MAX_TARGETS];
    uint8_t stream_write_count;
    TecmoAddressCount stream_writes[TECMO_TITLE_SETUP_MAX_TARGETS];
    uint8_t table_reference_count;
    uint8_t verified_table_reference_count;
    uint8_t stream_table_entry_count;
    uint8_t verified_stream_table_entry_count;
    uint8_t selected_stream_count;
    uint8_t dynamic_selector_row_count;
    uint8_t terminated_selector_row_count;
    uint8_t stream_base_parameter_bytes;
    uint8_t stream_source_fields_per_record;
    uint8_t stream_staged_fields_per_record;
    uint8_t max_stream_record_count;
    uint16_t max_stream_bytes_consumed;
    uint16_t max_stream_emitted_bytes;
    bool stream_format_summary_loaded;
    bool stream_effect_summary_loaded;
    bool stream_staging_summary_loaded;
    uint16_t stream_staging_base_address;
    uint16_t stream_staging_first_write;
    uint16_t stream_staging_last_write;
    uint8_t stream_staging_stream_count;
    uint16_t stream_staging_record_count;
    uint16_t stream_staging_bytes_written;
    bool fixed_helper_summary_loaded;
    uint8_t fixed_helper_unique_count;
    uint8_t fixed_helper_call_invocations;
    uint8_t fixed_wait_call_count;
    uint16_t fixed_wait_request_total;
    uint8_t fixed_setup_finalize_call_count;
    uint8_t fixed_staging_seed_call_count;
    uint8_t fixed_stream_finalize_call_count;
    bool stream_decode_pending;
    bool fixed_helper_effects_pending;
    uint16_t first_unclassified_call;
} TecmoTitleSetupSummary;

typedef struct TecmoOriginalTitleGlyphs {
    char title_text[TECMO_MAX_NAME_TEXT];
    TecmoTitleGlyph glyphs[TECMO_TITLE_MAX_CHARS];
    size_t glyph_count;
    uint8_t dispatcher_call_index;
    uint8_t dispatcher_bank;
    uint16_t dispatcher_target;
    bool dispatcher_matches_expected;
    uint8_t chr_config_0100;
    uint8_t setup_selector_0352;
    uint8_t ba16_update_flags_or_05b6;
    bool ba16_update_flag_modeled;
    TecmoTitleSetupSummary setup_summary;
} TecmoOriginalTitleGlyphs;

void asm_stats_add(AsmStats *dst, const AsmStats *src);
int asm_scan_file(const char *path, AsmStats *stats);
int asm_scan_tree(const char *root, const char *relative_dir, const char *extension, AsmStats *stats);

int tecmo_collect_rosters(const char *project_root, RosterTable *table);
void roster_table_free(RosterTable *table);

int tecmo_generate_roster_c(const char *project_root, const char *out_dir);
int tecmo_load_original_title_text(const char *project_root, char *title, size_t title_size);
int tecmo_load_original_title_glyphs(const char *project_root, TecmoOriginalTitleGlyphs *glyphs);
int tecmo_load_chr_data(const char *project_root, uint8_t **bytes_out, uint64_t *byte_count);
void tecmo_free_buffer(void *buffer);
int tecmo_export_chr(const char *project_root, const char *out_path, uint64_t *bytes_written);
int tecmo_export_chr_png_sheets(const char *project_root, const char *out_dir, uint64_t *sheets_written);
int tecmo_analyze_chr(const char *project_root, uint64_t *byte_count);

void tecmo_print_summary(const char *project_root);
void tecmo_print_banks(const char *project_root);
void tecmo_print_chunks(const char *project_root);
void tecmo_print_assets(const char *project_root);
void tecmo_print_roster(const char *project_root, const char *team_filter);

#endif
