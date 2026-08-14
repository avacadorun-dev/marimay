#ifndef RUDA_CHECK_SERVICE_H
#define RUDA_CHECK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/check/checker.h"
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
    RUDA_COMPILER_INTERNAL_ERROR
} RudaCompilerStatus;

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
} RudaCompilerDocument;

typedef struct RudaCompilerService {
    RudaAllocator allocator;
    RudaSourceManager sources;
    RudaVector documents;
    RudaCompilerOptions options;
    size_t retained_snapshot_count;
    size_t retained_source_bytes;
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
    bool has_frontend_errors;
    bool initialized;
} RudaCompilerResult;

RudaCompilerOptions ruda_compiler_default_options(void);

/*
 * The allocator state must outlive the service and every result it creates.
 * service must be zero-initialized or a valid initialized service. Successful
 * reinitialization disposes and replaces it; failure leaves it unchanged.
 * Dispose every borrowed result before disposing or replacing the service.
 */
RudaCompilerStatus ruda_compiler_service_init(
    RudaCompilerService *service,
    RudaAllocator allocator,
    RudaCompilerOptions options
);
void ruda_compiler_service_dispose(RudaCompilerService *service);

/*
 * Adds a copied path/source snapshot with a caller-defined document version.
 * A later overlay must use a strictly greater version. Existing snapshots are
 * retained by the service, so an older result remains readable after edits.
 */
RudaCompilerStatus ruda_compiler_service_add_document(
    RudaCompilerService *service,
    const char *path,
    size_t path_length,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    RudaFileId *file_id
);
RudaCompilerStatus ruda_compiler_service_overlay(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length
);

/*
 * Copies current document metadata; no pointer into the document vector
 * escapes. The copied snapshot remains borrowed until service disposal or
 * successful reinitialization.
 */
bool ruda_compiler_service_document(
    const RudaCompilerService *service,
    RudaFileId file_id,
    RudaCompilerDocument *document
);

/*
 * Compiles the current snapshots in canonical exact-path order. Lexical,
 * parse, and type errors are owned diagnostics with RUDA_COMPILER_OK. A
 * frontend error suppresses AST publication and package checking. Unsupported
 * accepted language remains RUDA_COMPILER_INCOMPLETE_LANGUAGE. The result
 * borrows snapshots from service; dispose it before disposing service.
 *
 * This is the diagnostics/lifetime foundation of the I2 compiler service. The
 * I2 editor gate additionally requires checker-owned definition, symbol
 * search, cancellation, and completion APIs before public `ruda check` can be
 * enabled. result must be zero-initialized or a valid initialized result. On
 * success the previous result is disposed and atomically replaced; on failure
 * it remains unchanged.
 */
RudaCompilerStatus ruda_compiler_service_check(
    const RudaCompilerService *service,
    const char *package_name,
    RudaCompilerResult *result
);

/* Borrowed until result is successfully replaced or disposed. */
const RudaCompilerUnit *ruda_compiler_result_unit(
    const RudaCompilerResult *result,
    size_t index
);
bool ruda_compiler_result_has_errors(const RudaCompilerResult *result);
void ruda_compiler_result_dispose(RudaCompilerResult *result);

#endif
