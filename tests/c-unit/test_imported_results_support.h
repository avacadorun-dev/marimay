#ifndef RUDA_TEST_IMPORTED_RESULTS_SUPPORT_H
#define RUDA_TEST_IMPORTED_RESULTS_SUPPORT_H

#include "test_checker_imports_support.h"

#include "ruda/bytecode/mir_lower.h"

#define RUDA_TEST_IMPORTED_RESULTS_MAX_FUNCTIONS 48

typedef struct RudaTestImportedResults {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[RUDA_TEST_IMPORTED_RESULTS_MAX_FUNCTIONS];
    const RudaCheckedFunction *entry;
    size_t entry_index;
    bool initialized;
} RudaTestImportedResults;

bool ruda_test_imported_results_init(
    RudaTestImportedResults *test,
    RudaAllocator allocator
);

void ruda_test_imported_results_dispose(RudaTestImportedResults *test);

RudaMirBytecodeLowerStatus ruda_test_imported_results_lower(
    const RudaTestImportedResults *test,
    RudaAllocator allocator,
    RudaBytecodeProgram *program
);

#endif
