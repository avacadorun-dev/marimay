#ifndef MMAY_MIR_EXECUTION_H
#define MMAY_MIR_EXECUTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/runtime/test_output.h"
#include "mmay/runtime/env.h"
#include "mmay/runtime/fs.h"
#include "mmay/runtime/process_context.h"
#include "mmay/types/type.h"

/*
 * Context-bound handle into a MarimayMirExecution object graph. Execution
 * provenance rejects foreign and stale handles; slot stores a kind-tagged
 * arena index and is never a host pointer.
 */
typedef struct MarimayMirReference {
    uint64_t execution_provenance;
    uint32_t slot;
} MarimayMirReference;

#define MMAY_MIR_INVALID_REFERENCE \
    ((MarimayMirReference){UINT64_MAX, UINT32_MAX})

typedef struct MarimayMirValue {
    MarimayTypeId type;
    union {
        int64_t i64;
        uint64_t integer_bits;
        uint64_t f64_bits;
        bool boolean;
        MarimayMirReference reference;
    } as;
} MarimayMirValue;

typedef struct MarimayMirExecutionOptions {
    size_t max_objects;
    size_t max_fields_per_object;
    size_t max_total_fields;
    size_t max_strings;
    size_t max_total_string_bytes;
    size_t max_arrays;
    size_t max_array_length;
    size_t max_total_array_elements;
    const MarimayEnvOptions *env;
    const MarimayFsOptions *fs;
    const MarimayProcessContextOptions *process_context;
} MarimayMirExecutionOptions;

typedef struct MarimayMirExecution {
    MarimayAllocator allocator;
    /* Oracle-owned record descriptors, retained until explicit disposal. */
    MarimayVector objects;
    /* Oracle-owned immutable enum descriptors, indexed separately by type. */
    MarimayVector enums;
    /* Oracle-owned immutable UTF-8 strings. */
    MarimayVector strings;
    /* Oracle-owned mutable arrays, indexed separately by exact type. */
    MarimayVector arrays;
    /* Oracle-owned immutable interface payloads and selected method tables. */
    MarimayVector interface_values;
    MarimayTestOutput test_output;
    MarimayEnvContext env;
    MarimayFsContext fs;
    MarimayProcessContextOptions process_context;
    bool env_initialized;
    bool fs_initialized;
    MarimayMirExecutionOptions options;
    size_t total_fields;
    size_t total_string_bytes;
    size_t total_array_elements;
    uint64_t provenance;
    bool initialized;
} MarimayMirExecution;

MarimayMirExecutionOptions mmay_mir_execution_default_options(void);
bool mmay_mir_reference_is_invalid(MarimayMirReference reference);
bool mmay_mir_reference_equal(
    MarimayMirReference left,
    MarimayMirReference right
);
bool mmay_mir_execution_init(
    MarimayMirExecution *execution,
    MarimayAllocator allocator,
    MarimayMirExecutionOptions options
);
/* Invalidates every MarimayMirReference owned by this execution. */
void mmay_mir_execution_dispose(MarimayMirExecution *execution);
/* Internal oracle inspection; no corresponding safe-source tag operation. */
bool mmay_mir_execution_inspect_enum(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t *variant,
    size_t *payload_count
);
bool mmay_mir_execution_enum_payload(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t index,
    MarimayMirValue *result
);
bool mmay_mir_execution_string_bytes(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    const unsigned char **bytes,
    size_t *length
);
bool mmay_mir_execution_array_length(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t *length
);
bool mmay_mir_execution_array_element(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t index,
    MarimayMirValue *result
);
/* Internal oracle inspection by allocation order. */
bool mmay_mir_execution_array_at(
    const MarimayMirExecution *execution,
    size_t ordinal,
    MarimayMirValue *result
);
/* Internal boundary validation for context-bound managed handles. */
bool mmay_mir_execution_owns_reference(
    const MarimayMirExecution *execution,
    MarimayMirValue value
);

#endif
