#include "test_checker_imports_support.h"

static const RudaCheckedTypeAlias *ruda_array_type_alias(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL
        && index < checked->type_aliases.length; index += 1) {
        const RudaCheckedTypeAlias *alias = ruda_checked_type_alias(
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

static bool ruda_array_type_has_depth(
    const RudaCheckResult *checked,
    RudaTypeId type,
    size_t depth,
    RudaTypeId leaf
) {
    while (depth != 0) {
        if (!ruda_type_array_element(&checked->types, type, &type)) {
            return false;
        }
        depth -= 1;
    }
    return type == leaf;
}

static void ruda_test_checker_array_type_identity(void) {
    static const char source[] =
        "type Edge { value: int; }\n"
        "type Slice<T> { value: T; }\n"
        "type Array<T> { value: T; }\n"
        "type Ints = []int;\n"
        "type Rows = [][]int;\n"
        "type Edges = []Edge;\n"
        "fn contextual(): [][]int { let rows: [][]int = []; return rows; }\n"
        "fn slice(value: Slice<int>): int { return value.value; }\n"
        "fn nominal(value: Array<int>): int { return value.value; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedTypeAlias *ints = NULL;
    const RudaCheckedTypeAlias *rows = NULL;
    const RudaCheckedTypeAlias *edges = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/cp18-array-types",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        ints = ruda_array_type_alias(&checked, "Ints");
        rows = ruda_array_type_alias(&checked, "Rows");
        edges = ruda_array_type_alias(&checked, "Edges");
        RUDA_EXPECT(ints != NULL && rows != NULL && edges != NULL);
        if (ints != NULL) {
            RUDA_EXPECT(ruda_array_type_has_depth(
                &checked, ints->target_type, 1, RUDA_TYPE_I64
            ));
        }
        if (rows != NULL) {
            RUDA_EXPECT(ruda_array_type_has_depth(
                &checked, rows->target_type, 2, RUDA_TYPE_I64
            ));
        }
        if (edges != NULL) {
            RudaTypeId element = RUDA_TYPE_INVALID;
            RUDA_EXPECT(ruda_type_array_element(
                &checked.types, edges->target_type, &element
            ));
            RUDA_EXPECT(ruda_type_kind(
                &checked.types, element
            ) == RUDA_TYPE_KIND_NOMINAL);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_array_type_migration(void) {
    static const char source[] =
        "fn old(values: Array<Array<int>>): void {}\n";
    static const char old_type[] = "Array<Array<int>>";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/cp18-array-migration",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_ARRAY_TYPE_SPELLING
            ) == 0);
            RUDA_EXPECT(strcmp(
                diagnostic->message,
                "prelude managed arrays use '[]T'; replace 'Array<T>' with '[]T'"
            ) == 0);
            RUDA_EXPECT(diagnostic->start
                == (size_t)(strstr(source, old_type) - source));
            RUDA_EXPECT(diagnostic->end - diagnostic->start
                == strlen(old_type));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_imported_nominal_array_name(void) {
    static const RudaTestImportSource sources[] = {
        {
            "types",
            "types/api.ruda",
            "public type Array<T> { public value: T; }\n"
        },
        {
            "app",
            "app/main.ruda",
            "import { Array } from \"types\";\n"
            "fn read(value: Array<int>): int { return value.value; }\n"
        }
    };
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture,
        sources,
        sizeof(sources) / sizeof(sources[0])
    ));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture,
        "app",
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_test_checker_array_type_allocations(void) {
    static const char source[] =
        "type Rows = [][]int;\n"
        "fn rows(): [][]int { let value: [][]int = []; return value; }\n";
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaCheckResult checked;
        RudaCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        status = ruda_check_ast(
            &fixture.ast,
            "test/cp18-array-allocation",
            ruda_fail_allocator(&state),
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 0);
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_array_type_spelling(void) {
    ruda_test_checker_array_type_identity();
    ruda_test_checker_array_type_migration();
    ruda_test_checker_imported_nominal_array_name();
    ruda_test_checker_array_type_allocations();
}
