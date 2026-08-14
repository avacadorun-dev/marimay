#ifndef RUDA_TEST_MIR_ARRAYS_SUPPORT_H
#define RUDA_TEST_MIR_ARRAYS_SUPPORT_H

#include "test_mir_support.h"

#include <stdint.h>

void ruda_test_mir_arrays_rhs(
    RudaSymbolId symbol,
    RudaTypeId array_type,
    uint64_t one,
    uint64_t five
);

#endif
