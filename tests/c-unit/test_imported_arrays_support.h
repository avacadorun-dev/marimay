#ifndef MMAY_TEST_IMPORTED_ARRAYS_SUPPORT_H
#define MMAY_TEST_IMPORTED_ARRAYS_SUPPORT_H

#include "test_checker_imports_support.h"

#include "mmay/bytecode/mir_lower.h"

#define MMAY_TEST_IMPORTED_ARRAY_MAX_FUNCTIONS 32

typedef struct MarimayTestImportedArrays {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[MMAY_TEST_IMPORTED_ARRAY_MAX_FUNCTIONS];
    const MarimayCheckedFunction *entry;
    size_t entry_index;
    bool initialized;
} MarimayTestImportedArrays;

bool mmay_test_imported_arrays_init(
    MarimayTestImportedArrays *test,
    MarimayAllocator allocator
);
void mmay_test_imported_arrays_dispose(MarimayTestImportedArrays *test);
MarimayMirBytecodeLowerStatus mmay_test_imported_arrays_lower(
    const MarimayTestImportedArrays *test,
    MarimayAllocator allocator,
    MarimayBytecodeProgram *program
);

#endif
