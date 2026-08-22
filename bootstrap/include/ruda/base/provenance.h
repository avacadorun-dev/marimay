#ifndef RUDA_BASE_PROVENANCE_H
#define RUDA_BASE_PROVENANCE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Returns a process-unique, nonzero identity whose low 32 bits are zero.
 * The reserved bits permit context-bound immediate runtime values without
 * increasing the size of every managed reference.
 * Thread-safe on every Tier 1 C17 toolchain, including MSVC.
 */
bool ruda_new_provenance(uint64_t *result);

#endif
