#ifndef MMAY_TEST_IMPORTED_RESULTS_SUPPORT_H
#define MMAY_TEST_IMPORTED_RESULTS_SUPPORT_H

#include "test_checker_imports_support.h"

#include "mmay/bytecode/mir_lower.h"

#define MMAY_TEST_IMPORTED_RESULTS_MAX_FUNCTIONS 48

typedef struct MarimayTestImportedResults {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[MMAY_TEST_IMPORTED_RESULTS_MAX_FUNCTIONS];
    const MarimayCheckedFunction *entry;
    size_t entry_index;
    bool initialized;
} MarimayTestImportedResults;

bool mmay_test_imported_results_init(
    MarimayTestImportedResults *test,
    MarimayAllocator allocator
);

void mmay_test_imported_results_dispose(MarimayTestImportedResults *test);

MarimayMirBytecodeLowerStatus mmay_test_imported_results_lower(
    const MarimayTestImportedResults *test,
    MarimayAllocator allocator,
    MarimayBytecodeProgram *program
);

#endif
