#ifndef MMAY_TEST_CHECKER_IMPORTS_SUPPORT_H
#define MMAY_TEST_CHECKER_IMPORTS_SUPPORT_H

#include "test_checker_support.h"

#define MMAY_TEST_IMPORT_MAX_FILES 16
#define MMAY_TEST_IMPORT_MAX_PACKAGES 8

typedef struct MarimayTestImportSource {
    const char *package_path;
    const char *source_path;
    const char *source;
} MarimayTestImportSource;

typedef struct MarimayCheckerImportFixture {
    MarimayCheckerFixture files[MMAY_TEST_IMPORT_MAX_FILES];
    const MarimayAstTree *package_asts[
        MMAY_TEST_IMPORT_MAX_PACKAGES
    ][MMAY_TEST_IMPORT_MAX_FILES];
    MarimayCheckPackageInput packages[MMAY_TEST_IMPORT_MAX_PACKAGES];
    size_t file_count;
    size_t package_count;
} MarimayCheckerImportFixture;

bool mmay_checker_import_fixture_init(
    MarimayCheckerImportFixture *fixture,
    const MarimayTestImportSource *sources,
    size_t source_count
);
void mmay_checker_import_fixture_dispose(
    MarimayCheckerImportFixture *fixture
);
MarimayCheckStatus mmay_checker_import_fixture_check(
    const MarimayCheckerImportFixture *fixture,
    const char *root,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
);
const MarimayCheckedFunction *mmay_checker_import_function(
    const MarimayCheckResult *result,
    const char *name,
    size_t *index
);
bool mmay_checker_import_mir_bytes(
    const MarimayCheckResult *result,
    MarimayAllocator allocator,
    MarimayOwnedBytes *bytes
);

#endif
