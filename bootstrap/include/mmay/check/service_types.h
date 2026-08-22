#ifndef MMAY_CHECK_SERVICE_TYPES_H
#define MMAY_CHECK_SERVICE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/check/package.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

typedef enum MarimayCompilerStatus {
    MMAY_COMPILER_OK,
    MMAY_COMPILER_INVALID_ARGUMENT,
    MMAY_COMPILER_ALLOCATION_FAILED,
    MMAY_COMPILER_LIMIT_REACHED,
    MMAY_COMPILER_DUPLICATE_PATH,
    MMAY_COMPILER_UNKNOWN_DOCUMENT,
    MMAY_COMPILER_STALE_VERSION,
    MMAY_COMPILER_INCOMPLETE_LANGUAGE,
    MMAY_COMPILER_INTERNAL_ERROR,
    MMAY_COMPILER_CANCELLED,
    MMAY_COMPILER_STALE_RESULT
} MarimayCompilerStatus;

typedef struct MarimayCompilerEdit {
    uint64_t base_version;
    size_t start;
    size_t previous_end;
    size_t new_end;
} MarimayCompilerEdit;

typedef bool (*MarimayCompilerCancelPoll)(void *context);

typedef struct MarimayCompilerCheckStats {
    size_t full_parse_count;
    size_t incremental_parse_count;
    size_t reused_top_level_nodes;
    size_t reparsed_bytes;
} MarimayCompilerCheckStats;

typedef struct MarimayCompilerOptions {
    size_t max_documents;
    size_t max_snapshots_per_document;
    size_t max_total_snapshots;
    size_t max_path_bytes;
    size_t max_source_bytes;
    size_t max_total_source_bytes;
    MarimayLexOptions lex_options;
    MarimayParseOptions parse_options;
    MarimayAstOptions ast_options;
    MarimayCheckOptions check_options;
} MarimayCompilerOptions;

typedef struct MarimayCompilerDocument {
    MarimayFileId file_id;
    uint64_t version;
    const MarimaySourceSnapshot *snapshot;
    size_t snapshot_count;
    const MarimaySourceSnapshot *previous_snapshot;
    MarimayCompilerEdit edit;
    bool has_edit;
} MarimayCompilerDocument;

typedef struct MarimayCompilerService {
    MarimayAllocator allocator;
    MarimaySourceManager sources;
    MarimayVector documents;
    MarimayCompilerOptions options;
    size_t retained_snapshot_count;
    size_t retained_source_bytes;
    uint64_t generation;
    bool initialized;
} MarimayCompilerService;

typedef struct MarimayCompilerUnit {
    MarimayFileId file_id;
    uint64_t version;
    const MarimaySourceSnapshot *snapshot;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
} MarimayCompilerUnit;

typedef struct MarimayCompilerResult {
    MarimayAllocator allocator;
    MarimayVector units;
    MarimayCheckResult checked;
    uint64_t workspace_generation;
    bool has_frontend_errors;
    bool initialized;
} MarimayCompilerResult;

typedef struct MarimayCompilerCheckRequest {
    const MarimayCompilerResult *previous;
    MarimayCompilerCancelPoll cancel_poll;
    void *cancel_context;
    MarimayCompilerCheckStats *stats;
} MarimayCompilerCheckRequest;

typedef struct MarimayCompilerPackage {
    const char *import_path;
    size_t import_path_length;
    const MarimayFileId *file_ids;
    size_t file_count;
} MarimayCompilerPackage;

#endif
