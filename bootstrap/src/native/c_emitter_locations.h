#ifndef MMAY_NATIVE_C_EMITTER_LOCATIONS_H
#define MMAY_NATIVE_C_EMITTER_LOCATIONS_H

#include "mmay/native/c_emitter.h"
typedef struct MarimayNativeCBuffer MarimayNativeCBuffer;

/* Sorted MIR is already verified; metadata is checked before publication. */
bool mmay_native_c_locations_valid(const MarimayNativePanicLocation *locations,
    size_t count, const MarimayMirFunction *const *functions, size_t function_count);
/* Emits an allocation-free binary lookup and primary-range renderer. */
bool mmay_native_c_emit_locations(MarimayNativeCBuffer *buffer,
    const MarimayNativePanicLocation *locations, size_t count);

#endif
