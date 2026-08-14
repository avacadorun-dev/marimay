#ifndef RUDA_CHECK_SERVICE_WORKSPACE_H
#define RUDA_CHECK_SERVICE_WORKSPACE_H

#include "ruda/check/service_types.h"

RudaCompilerOptions ruda_compiler_default_options(void);

/*
 * The allocator state must outlive the service and every result it creates.
 * service must be zero-initialized or valid. Successful reinitialization
 * disposes and replaces it; failure leaves it unchanged. Dispose every result
 * before disposing or replacing the service.
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
 * retained, so an older result remains readable after edits.
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
 * Applies a full new snapshot plus an exact single-edit proof against the
 * current version. Invalid ranges, changed prefix/suffix bytes, or a stale
 * base leave the service unchanged. A later request may reuse the previous
 * parse tree; ordinary overlay performs full invalidation.
 */
RudaCompilerStatus ruda_compiler_service_overlay_edit(
    RudaCompilerService *service,
    RudaFileId file_id,
    uint64_t version,
    const void *source_bytes,
    size_t source_length,
    RudaCompilerEdit edit
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

#endif
