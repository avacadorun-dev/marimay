#include "test_checker_imports_support.h"

static const MarimayCheckedTypeAlias *mmay_array_type_alias(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL
        && index < checked->type_aliases.length; index += 1) {
        const MarimayCheckedTypeAlias *alias = mmay_checked_type_alias(
            checked,
            index
        );
        if (alias != NULL && alias->name.length == length
            && memcmp(alias->name.data, name, length) == 0) {
            return alias;
        }
    }
    return NULL;
}

static bool mmay_array_type_has_depth(
    const MarimayCheckResult *checked,
    MarimayTypeId type,
    size_t depth,
    MarimayTypeId leaf
) {
    while (depth != 0) {
        if (!mmay_type_array_element(&checked->types, type, &type)) {
            return false;
        }
        depth -= 1;
    }
    return type == leaf;
}

static void mmay_test_checker_array_type_identity(void) {
    static const char source[] =
        "type Edge { value: int, }\n"
        "type Slice<T> { value: T, }\n"
        "type Array<T> { value: T, }\n"
        "type Ints = []int;\n"
        "type Rows = [][]int;\n"
        "type Edges = []Edge;\n"
        "fn contextual(): [][]int { let rows: [][]int = []; return rows; }\n"
        "fn slice(value: Slice<int>): int { return value.value; }\n"
        "fn nominal(value: Array<int>): int { return value.value; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedTypeAlias *ints = NULL;
    const MarimayCheckedTypeAlias *rows = NULL;
    const MarimayCheckedTypeAlias *edges = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/cp18-array-types",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        ints = mmay_array_type_alias(&checked, "Ints");
        rows = mmay_array_type_alias(&checked, "Rows");
        edges = mmay_array_type_alias(&checked, "Edges");
        MMAY_EXPECT(ints != NULL && rows != NULL && edges != NULL);
        if (ints != NULL) {
            MMAY_EXPECT(mmay_array_type_has_depth(
                &checked, ints->target_type, 1, MMAY_TYPE_I64
            ));
        }
        if (rows != NULL) {
            MMAY_EXPECT(mmay_array_type_has_depth(
                &checked, rows->target_type, 2, MMAY_TYPE_I64
            ));
        }
        if (edges != NULL) {
            MarimayTypeId element = MMAY_TYPE_INVALID;
            MMAY_EXPECT(mmay_type_array_element(
                &checked.types, edges->target_type, &element
            ));
            MMAY_EXPECT(mmay_type_kind(
                &checked.types, element
            ) == MMAY_TYPE_KIND_NOMINAL);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_array_type_migration(void) {
    static const char source[] =
        "fn old(values: Array<Array<int>>): void {}\n";
    static const char old_type[] = "Array<Array<int>>";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/cp18-array-migration",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->code,
                MMAY_DIAGNOSTIC_ARRAY_TYPE_SPELLING
            ) == 0);
            MMAY_EXPECT(strcmp(
                diagnostic->message,
                "prelude managed arrays use '[]T'; replace 'Array<T>' with '[]T'"
            ) == 0);
            MMAY_EXPECT(diagnostic->start
                == (size_t)(strstr(source, old_type) - source));
            MMAY_EXPECT(diagnostic->end - diagnostic->start
                == strlen(old_type));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_imported_nominal_array_name(void) {
    static const MarimayTestImportSource sources[] = {
        {
            "types",
            "types/api.my",
            "public type Array<T> { public value: T, }\n"
        },
        {
            "app",
            "app/main.my",
            "import { Array } from \"types\";\n"
            "fn read(value: Array<int>): int { return value.value; }\n"
        }
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture,
        sources,
        sizeof(sources) / sizeof(sources[0])
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture,
        "app",
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_checker_array_type_allocations(void) {
    static const char source[] =
        "type Rows = [][]int;\n"
        "fn rows(): [][]int { let value: [][]int = []; return value; }\n";
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        status = mmay_check_ast(
            &fixture.ast,
            "test/cp18-array-allocation",
            mmay_fail_allocator(&state),
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 0);
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_array_type_spelling(void) {
    mmay_test_checker_array_type_identity();
    mmay_test_checker_array_type_migration();
    mmay_test_checker_imported_nominal_array_name();
    mmay_test_checker_array_type_allocations();
}
