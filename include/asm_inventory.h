#ifndef ASM_INVENTORY_H
#define ASM_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_MAX_PATH_TEXT 1024
#define TECMO_MAX_LABEL_TEXT 256
#define TECMO_MAX_NAME_TEXT 128

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

void asm_stats_add(AsmStats *dst, const AsmStats *src);
int asm_scan_file(const char *path, AsmStats *stats);
int asm_scan_tree(const char *root, const char *relative_dir, const char *extension, AsmStats *stats);

int tecmo_collect_rosters(const char *project_root, RosterTable *table);
void roster_table_free(RosterTable *table);

int tecmo_generate_roster_c(const char *project_root, const char *out_dir);
int tecmo_export_chr(const char *project_root, const char *out_path, uint64_t *bytes_written);
int tecmo_export_chr_png_sheets(const char *project_root, const char *out_dir, uint64_t *sheets_written);
int tecmo_analyze_chr(const char *project_root, uint64_t *byte_count);

void tecmo_print_summary(const char *project_root);
void tecmo_print_banks(const char *project_root);
void tecmo_print_chunks(const char *project_root);
void tecmo_print_assets(const char *project_root);
void tecmo_print_roster(const char *project_root, const char *team_filter);

#endif
