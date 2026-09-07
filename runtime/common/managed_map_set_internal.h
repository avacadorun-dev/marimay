#ifndef MMAY_MANAGED_MAP_SET_INTERNAL_H
#define MMAY_MANAGED_MAP_SET_INTERNAL_H

#include "mmay/runtime/map.h"

/* Fully checked Set insertion through the shared map storage owner. Retains
 * Set receiver/output error ordering, key validation and failure atomicity;
 * no borrowed object or storage address is exposed to the Set API owner. */
MarimayManagedStatus mmay_managed_map_add_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    bool *inserted
);

#endif
