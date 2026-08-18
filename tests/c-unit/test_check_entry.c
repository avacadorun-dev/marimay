#include "test_checker_imports_support.h"

#include <string.h>

#include "ruda/check/entry.h"

static RudaCheckEntryStatus ruda_test_entry_select(
    const char *source,
    const char *root,
    RudaCheckedEntry *entry
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckEntryStatus status = RUDA_CHECK_ENTRY_INVALID_ARGUMENT;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    if (!ruda_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_check_ast(
            &fixture.ast, "test/entry", ruda_system_allocator(),
            ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK || checked.diagnostics.length != 0) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    status = ruda_check_select_entry(
        &checked, root, strlen(root), entry
    );
    if (status == RUDA_CHECK_ENTRY_OK) {
        RUDA_EXPECT(entry->function != NULL && entry->symbol != NULL);
    }
cleanup:
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
    return status;
}

static void ruda_test_entry_valid_signatures(void) {
    static const char result_source[] =
        "enum Failure { Invalid, }\n"
        "fn message(error: Failure): string { return \"invalid\"; }\n"
        "fn main(): Result<void, Failure> { return Err(Failure.Invalid); }\n";
    RudaCheckedEntry entry;
    memset(&entry, 0, sizeof(entry));
    RUDA_EXPECT(ruda_test_entry_select(
        "fn main(): void { return; }\n", "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_OK);
    RUDA_EXPECT(!entry.returns_result && entry.error_message == NULL);

    memset(&entry, 0, sizeof(entry));
    RUDA_EXPECT(ruda_test_entry_select(
        result_source, "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_OK);
    RUDA_EXPECT(entry.returns_result);
    RUDA_EXPECT(entry.error_message != NULL);
    RUDA_EXPECT(entry.error_type != RUDA_TYPE_INVALID);
}

static void ruda_test_entry_rejections(void) {
    static const char missing_contract[] =
        "enum Failure { Invalid, }\n"
        "fn main(): Result<void, Failure> { return Err(Failure.Invalid); }\n";
    RudaCheckedEntry entry;
    memset(&entry, 0, sizeof(entry));
    RUDA_EXPECT(ruda_test_entry_select(
        "fn helper(): void { return; }\n", "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_NOT_FOUND);
    RUDA_EXPECT(ruda_test_entry_select(
        "fn main(value: int): void { return; }\n", "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_INVALID_SIGNATURE);
    RUDA_EXPECT(ruda_test_entry_select(
        "fn main(): int { return 0; }\n", "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_INVALID_SIGNATURE);
    RUDA_EXPECT(ruda_test_entry_select(
        missing_contract, "test/entry", &entry
    ) == RUDA_CHECK_ENTRY_ERROR_CONTRACT_MISSING);
    RUDA_EXPECT(ruda_test_entry_select(
        "fn main(): void { return; }\n", "other/package", &entry
    ) == RUDA_CHECK_ENTRY_NOT_FOUND);
}

static void ruda_test_entry_requires_visible_error_contract(void) {
    static const char library[] =
        "public enum Failure { Invalid, }\n"
        "fn message(error: Failure): string { return \"invalid\"; }\n";
    static const char application[] =
        "import { Failure } from \"test/failure\";\n"
        "fn main(): Result<void, Failure> {\n"
        "  panic(\"unreachable\");\n"
        "}\n";
    RudaTestImportSource sources[] = {
        {"test/entry-app", "app/main.ruda", application},
        {"test/failure", "failure/failure.ruda", library}
    };
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    RudaCheckedEntry entry;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&entry, 0, sizeof(entry));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "test/entry-app", ruda_system_allocator(),
        ruda_check_graph_default_options(), &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(ruda_check_select_entry(
        &checked, "test/entry-app", sizeof("test/entry-app") - 1, &entry
    ) == RUDA_CHECK_ENTRY_ERROR_CONTRACT_MISSING);
    ruda_check_result_dispose(&checked);
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_test_entry_invalid_arguments(void) {
    RudaCheckedEntry entry;
    RudaCheckResult checked;
    memset(&entry, 0, sizeof(entry));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_check_select_entry(
        NULL, "test/entry", sizeof("test/entry") - 1, &entry
    ) == RUDA_CHECK_ENTRY_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_check_select_entry(
        &checked, "test/entry", sizeof("test/entry") - 1, &entry
    ) == RUDA_CHECK_ENTRY_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_check_select_entry(
        &checked, "", 0, &entry
    ) == RUDA_CHECK_ENTRY_INVALID_ARGUMENT);
}

void ruda_test_check_entry(void) {
    ruda_test_entry_valid_signatures();
    ruda_test_entry_rejections();
    ruda_test_entry_requires_visible_error_contract();
    ruda_test_entry_invalid_arguments();
}
