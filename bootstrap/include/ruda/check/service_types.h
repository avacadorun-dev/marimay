#ifndef RUDA_CHECK_SERVICE_TYPES_H
#define RUDA_CHECK_SERVICE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/check/package.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

typedef enum RudaCompilerStatus {
    RUDA_COMPILER_OK,
    RUDA_COMPILER_INVALID_ARGUMENT,
    RUDA_COMPILER_ALLOCATION_FAILED,
    RUDA_COMPILER_LIMIT_REACHED,
    RUDA_COMPILER_DUPLICATE_PATH,
    RUDA_COMPILER_UNKNOWN_DOCUMENT,
    RUDA_COMPILER_STALE_VERSION,
    RUDA_COMPILER_INCOMPLETE_LANGUAGE,
    RUDA_COMPILER_INTERNAL_ERROR,
    RUDA_COMPILER_CANCELLED,
    RUDA_COMPILER_STALE_RESULT
} RudaCompilerStatus;

typedef struct RudaCompilerEdit {
    uint64_t base_version;
    size_t start;
    size_t previous_end;
    size_t new_end;
} RudaCompilerEdit;

typedef bool (*RudaCompilerCancelPoll)(void *context);

typedef struct RudaCompilerCheckStats {
    size_t full_parse_count;
    size_t incremental_parse_count;
    size_t reused_top_level_nodes;
    size_t reparsed_bytes;
} RudaCompilerCheckStats;

typedef struct RudaCompilerOptions {
    size_t max_documents;
    size_t max_snapshots_per_document;
    size_t max_total_snapshots;
    size_t max_path_bytes;
    size_t max_source_bytes;
    size_t max_total_source_bytes;
    RudaLexOptions lex_options;
    RudaParseOptions parse_options;
    RudaAstOptions ast_options;
    RudaCheckOptions check_options;
} RudaCompilerOptions;

typedef struct RudaCompilerDocument {
    RudaFileId file_id;
    uint64_t version;
    const RudaSourceSnapshot *snapshot;
    size_t snapshot_count;
    const RudaSourceSnapshot *previous_snapshot;
    RudaCompilerEdit edit;
    bool has_edit;
} RudaCompilerDocument;

typedef struct RudaCompilerService {
    RudaAllocator allocator;
    RudaSourceManager sources;
    RudaVector documents;
    RudaCompilerOptions options;
    size_t retained_snapshot_count;
    size_t retained_source_bytes;
    uint64_t generation;
    bool initialized;
} RudaCompilerService;

typedef struct RudaCompilerUnit {
    RudaFileId file_id;
    uint64_t version;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
} RudaCompilerUnit;

typedef struct RudaCompilerResult {
    RudaAllocator allocator;
    RudaVector units;
    RudaCheckResult checked;
    uint64_t workspace_generation;
    bool has_frontend_errors;
    bool initialized;
} RudaCompilerResult;

typedef struct RudaCompilerCheckRequest {
    const RudaCompilerResult *previous;
    RudaCompilerCancelPoll cancel_poll;
    void *cancel_context;
    RudaCompilerCheckStats *stats;
} RudaCompilerCheckRequest;

typedef struct RudaCompilerPackage {
    const char *import_path;
    size_t import_path_length;
    const RudaFileId *file_ids;
    size_t file_count;
} RudaCompilerPackage;

#endif
