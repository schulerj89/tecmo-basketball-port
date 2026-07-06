#ifndef ASM_INVENTORY_H
#define ASM_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_MAX_PATH_TEXT 1024
#define TECMO_MAX_LABEL_TEXT 256
#define TECMO_MAX_NAME_TEXT 128
#define TECMO_TITLE_MAX_CHARS 64

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
