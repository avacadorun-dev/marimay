#include "test_checker_imports_support.h"

#include <string.h>

#include "mmay/check/entry.h"

static MarimayCheckEntryStatus mmay_test_entry_select(
    const char *source,
    const char *root,
    MarimayCheckedEntry *entry
) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckEntryStatus status = MMAY_CHECK_ENTRY_INVALID_ARGUMENT;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    if (!mmay_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (mmay_check_ast(
            &fixture.ast, "test/entry", mmay_system_allocator(),
            mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK || checked.diagnostics.length != 0) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    status = mmay_check_select_entry(
        &checked, root, strlen(root), entry
    );
    if (status == MMAY_CHECK_ENTRY_OK) {
        MMAY_EXPECT(entry->function != NULL && entry->symbol != NULL);
    }
cleanup:
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
    return status;
}

static void mmay_test_entry_valid_signatures(void) {
    static const char result_source[] =
        "enum Failure { Invalid, }\n"
        "fn message(error: Failure): string { return \"invalid\"; }\n"
        "fn main(): Result<void, Failure> { return Err(Failure.Invalid); }\n";
    static const char argument_result_source[] =
        "enum Failure { Invalid, }\n"
        "fn message(error: Failure): string { return \"invalid\"; }\n"
        "fn main(arguments: []string): Result<int, Failure> {\n"
        "  return Ok(arguments.length());\n"
        "}\n";
    MarimayCheckedEntry entry;
    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(): void { return; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_OK);
    MMAY_EXPECT(!entry.takes_arguments && !entry.returns_status
        && !entry.returns_result && entry.error_message == NULL);

    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(): int { return 23; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_OK);
    MMAY_EXPECT(!entry.takes_arguments && entry.returns_status
        && !entry.returns_result);

    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_entry_select(
        result_source, "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_OK);
    MMAY_EXPECT(!entry.takes_arguments && !entry.returns_status
        && entry.returns_result);
    MMAY_EXPECT(entry.error_message != NULL);
    MMAY_EXPECT(entry.error_type != MMAY_TYPE_INVALID);

    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_entry_select(
        argument_result_source, "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_OK);
    MMAY_EXPECT(entry.takes_arguments && entry.returns_status
        && entry.returns_result && entry.error_message != NULL);
}

static void mmay_test_entry_rejections(void) {
    static const char missing_contract[] =
        "enum Failure { Invalid, }\n"
        "fn main(): Result<void, Failure> { return Err(Failure.Invalid); }\n";
    MarimayCheckedEntry entry;
    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_entry_select(
        "fn helper(): void { return; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_NOT_FOUND);
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(value: int): void { return; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_INVALID_SIGNATURE);
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(values: []int): void { return; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_INVALID_SIGNATURE);
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(): string { return \"bad\"; }\n", "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_INVALID_SIGNATURE);
    MMAY_EXPECT(mmay_test_entry_select(
        missing_contract, "test/entry", &entry
    ) == MMAY_CHECK_ENTRY_ERROR_CONTRACT_MISSING);
    MMAY_EXPECT(mmay_test_entry_select(
        "fn main(): void { return; }\n", "other/package", &entry
    ) == MMAY_CHECK_ENTRY_NOT_FOUND);
}

static void mmay_test_entry_requires_visible_error_contract(void) {
    static const char library[] =
        "public enum Failure { Invalid, }\n"
        "fn message(error: Failure): string { return \"invalid\"; }\n";
    static const char application[] =
        "import { Failure } from \"test/failure\";\n"
        "fn main(): Result<void, Failure> {\n"
        "  panic(\"unreachable\");\n"
        "}\n";
    MarimayTestImportSource sources[] = {
        {"test/entry-app", "app/main.my", application},
        {"test/failure", "failure/failure.my", library}
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckedEntry entry;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "test/entry-app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(mmay_check_select_entry(
        &checked, "test/entry-app", sizeof("test/entry-app") - 1, &entry
    ) == MMAY_CHECK_ENTRY_ERROR_CONTRACT_MISSING);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_entry_invalid_arguments(void) {
    MarimayCheckedEntry entry;
    MarimayCheckResult checked;
    memset(&entry, 0, sizeof(entry));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_check_select_entry(
        NULL, "test/entry", sizeof("test/entry") - 1, &entry
    ) == MMAY_CHECK_ENTRY_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_check_select_entry(
        &checked, "test/entry", sizeof("test/entry") - 1, &entry
    ) == MMAY_CHECK_ENTRY_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_check_select_entry(
        &checked, "", 0, &entry
    ) == MMAY_CHECK_ENTRY_INVALID_ARGUMENT);
}

void mmay_test_check_entry(void) {
    mmay_test_entry_valid_signatures();
    mmay_test_entry_rejections();
    mmay_test_entry_requires_visible_error_contract();
    mmay_test_entry_invalid_arguments();
}
