#ifndef RUDA_BASE_PROVENANCE_H
#define RUDA_BASE_PROVENANCE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Returns a process-unique, nonzero identity that is never UINT64_MAX.
 * Thread-safe on every Tier 1 C17 toolchain, including MSVC.
 */
bool ruda_new_provenance(uint64_t *result);

#endif
