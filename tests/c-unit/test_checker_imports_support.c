#include "test_checker_imports_support.h"

#include <string.h>

static size_t ruda_checker_import_package(
    RudaCheckerImportFixture *fixture,
    const char *path
) {
    size_t index = 0;
    for (index = 0; index < fixture->package_count; index += 1) {
        if (strcmp(fixture->packages[index].import_path, path) == 0) {
            return index;
        }
    }
    if (fixture->package_count >= RUDA_TEST_IMPORT_MAX_PACKAGES) {
        return SIZE_MAX;
    }
    index = fixture->package_count;
    fixture->package_count += 1;
    fixture->packages[index].import_path = path;
    fixture->packages[index].import_path_length = strlen(path);
    fixture->packages[index].asts = fixture->package_asts[index];
    return index;
}

bool ruda_checker_import_fixture_init(
    RudaCheckerImportFixture *fixture,
    const RudaTestImportSource *sources,
    size_t source_count
) {
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    if (sources == NULL || source_count == 0
        || source_count > RUDA_TEST_IMPORT_MAX_FILES) {
        return false;
    }
    for (index = 0; index < source_count; index += 1) {
        size_t package_index = ruda_checker_import_package(
            fixture, sources[index].package_path
        );
        RudaCheckPackageInput *package = NULL;
        fixture->file_count = index + 1;
        if (package_index == SIZE_MAX || !ruda_checker_fixture_init_path(
                &fixture->files[index],
                sources[index].source_path,
                sources[index].source
            )) {
            ruda_checker_import_fixture_dispose(fixture);
            return false;
        }
        package = &fixture->packages[package_index];
        if (package->ast_count >= RUDA_TEST_IMPORT_MAX_FILES) {
            ruda_checker_import_fixture_dispose(fixture);
            return false;
        }
        fixture->package_asts[package_index][package->ast_count] =
            &fixture->files[index].ast;
        package->ast_count += 1;
    }
    return true;
}

void ruda_checker_import_fixture_dispose(
    RudaCheckerImportFixture *fixture
) {
    size_t index = fixture == NULL ? 0 : fixture->file_count;
    while (index != 0) {
        index -= 1;
        ruda_checker_fixture_dispose(&fixture->files[index]);
    }
    if (fixture != NULL) {
        memset(fixture, 0, sizeof(*fixture));
    }
}

RudaCheckStatus ruda_checker_import_fixture_check(
    const RudaCheckerImportFixture *fixture,
    const char *root,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
) {
    return ruda_check_package_graph(
        fixture->packages,
        fixture->package_count,
        root,
        strlen(root),
        allocator,
        options,
        result
    );
}

const RudaCheckedFunction *ruda_checker_import_function(
    const RudaCheckResult *result,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            if (result_index != NULL) {
                *result_index = index;
            }
            return function;
        }
    }
    return NULL;
}
