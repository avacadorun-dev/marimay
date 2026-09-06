#ifndef MMAY_NATIVE_C_EMITTER_TEST_ENTRY_H
#define MMAY_NATIVE_C_EMITTER_TEST_ENTRY_H

#include "mmay/native/c_emitter.h"

typedef struct MarimayNativeCBuffer MarimayNativeCBuffer;

/* Validates a canonical table against already validated, sorted MIR. */
MarimayNativeCEmitStatus mmay_native_c_validate_test_entries(
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
);
/* Select before importing user arguments or allocating runtime state. */
bool mmay_native_c_emit_test_selector(MarimayNativeCBuffer *buffer, size_t count);
/* Reuses the ordinary argument, call, status and cleanup owners per case. */
bool mmay_native_c_emit_test_dispatch(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
);

#endif
