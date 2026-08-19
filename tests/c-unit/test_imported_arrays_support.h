#ifndef RUDA_TEST_IMPORTED_ARRAYS_SUPPORT_H
#define RUDA_TEST_IMPORTED_ARRAYS_SUPPORT_H

#include "test_checker_imports_support.h"

#include "ruda/bytecode/mir_lower.h"

#define RUDA_TEST_IMPORTED_ARRAY_MAX_FUNCTIONS 32

typedef struct RudaTestImportedArrays {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[RUDA_TEST_IMPORTED_ARRAY_MAX_FUNCTIONS];
    const RudaCheckedFunction *entry;
    size_t entry_index;
    bool initialized;
} RudaTestImportedArrays;

bool ruda_test_imported_arrays_init(
    RudaTestImportedArrays *test,
    RudaAllocator allocator
);
void ruda_test_imported_arrays_dispose(RudaTestImportedArrays *test);
RudaMirBytecodeLowerStatus ruda_test_imported_arrays_lower(
    const RudaTestImportedArrays *test,
    RudaAllocator allocator,
    RudaBytecodeProgram *program
);

#endif
