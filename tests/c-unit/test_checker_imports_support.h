#ifndef RUDA_TEST_CHECKER_IMPORTS_SUPPORT_H
#define RUDA_TEST_CHECKER_IMPORTS_SUPPORT_H

#include "test_checker_support.h"

#define RUDA_TEST_IMPORT_MAX_FILES 16
#define RUDA_TEST_IMPORT_MAX_PACKAGES 8

typedef struct RudaTestImportSource {
    const char *package_path;
    const char *source_path;
    const char *source;
} RudaTestImportSource;

typedef struct RudaCheckerImportFixture {
    RudaCheckerFixture files[RUDA_TEST_IMPORT_MAX_FILES];
    const RudaAstTree *package_asts[
        RUDA_TEST_IMPORT_MAX_PACKAGES
    ][RUDA_TEST_IMPORT_MAX_FILES];
    RudaCheckPackageInput packages[RUDA_TEST_IMPORT_MAX_PACKAGES];
    size_t file_count;
    size_t package_count;
} RudaCheckerImportFixture;

bool ruda_checker_import_fixture_init(
    RudaCheckerImportFixture *fixture,
    const RudaTestImportSource *sources,
    size_t source_count
);
void ruda_checker_import_fixture_dispose(
    RudaCheckerImportFixture *fixture
);
RudaCheckStatus ruda_checker_import_fixture_check(
    const RudaCheckerImportFixture *fixture,
    const char *root,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
);
const RudaCheckedFunction *ruda_checker_import_function(
    const RudaCheckResult *result,
    const char *name,
    size_t *index
);

#endif
