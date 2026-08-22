#include "test_formatter_support.h"

void mmay_test_formatter_rejection_limits_and_allocation(void) {
    static const char valid[] = "fn main(): void { return; }\n";
    static const char valid_imports[] =
        "import * as zed from \"z.example/z\";\n"
        "import * as io from \"std/io\";\n";
    static const char valid_strings[] =
        "fn strings(): void { const raw = r\"\\\\server\\share\"; "
        "const control = \"\\u{1}\"; }\n";
    static const char invalid[] = "fn broken(: void {\n";
    MarimayFormatterFixture valid_fixture;
    MarimayFormatterFixture import_fixture;
    MarimayFormatterFixture string_fixture;
    MarimayFormatterFixture invalid_fixture;
    MarimayFormatResult result;
    MarimayFormatOptions options;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &valid_fixture,
        "valid.my",
        (const unsigned char *)valid,
        strlen(valid)
    ));
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &import_fixture,
        "imports.my",
        (const unsigned char *)valid_imports,
        strlen(valid_imports)
    ));
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &string_fixture,
        "strings.my",
        (const unsigned char *)valid_strings,
        strlen(valid_strings)
    ));
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &invalid_fixture,
        "invalid.my",
        (const unsigned char *)invalid,
        strlen(invalid)
    ));
    MMAY_EXPECT(mmay_format_source(
        &invalid_fixture.lexed,
        &invalid_fixture.parsed,
        mmay_system_allocator(),
        mmay_format_default_options(),
        &result
    ) == MMAY_FORMAT_SOURCE_INVALID);
    MMAY_EXPECT(!result.initialized);

    options = mmay_format_default_options();
    options.max_tokens = 1;
    MMAY_EXPECT(mmay_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_FORMAT_LIMIT_REACHED);
    options = mmay_format_default_options();
    options.max_document_parts = 1;
    MMAY_EXPECT(mmay_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_FORMAT_LIMIT_REACHED);
    options = mmay_format_default_options();
    options.max_output_bytes = 1;
    MMAY_EXPECT(mmay_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_FORMAT_LIMIT_REACHED);
    options = mmay_format_default_options();
    options.max_nesting = 1;
    MMAY_EXPECT(mmay_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_FORMAT_LIMIT_REACHED);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_format_source(
            &valid_fixture.lexed,
            &valid_fixture.parsed,
            allocator,
            mmay_format_default_options(),
            &result
        );
        if (status == MMAY_FORMAT_OK) {
            reached_success = true;
            mmay_format_result_dispose(&result);
        } else {
            MMAY_EXPECT(status == MMAY_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_format_source(
            &import_fixture.lexed,
            &import_fixture.parsed,
            allocator,
            mmay_format_default_options(),
            &result
        );
        if (status == MMAY_FORMAT_OK) {
            reached_success = true;
            mmay_format_result_dispose(&result);
        } else {
            MMAY_EXPECT(status == MMAY_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 192; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_format_source(
            &string_fixture.lexed,
            &string_fixture.parsed,
            allocator,
            mmay_format_default_options(),
            &result
        );
        if (status == MMAY_FORMAT_OK) {
            reached_success = true;
            mmay_format_result_dispose(&result);
        } else {
            MMAY_EXPECT(status == MMAY_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_formatter_fixture_dispose(&invalid_fixture);
    mmay_formatter_fixture_dispose(&string_fixture);
    mmay_formatter_fixture_dispose(&import_fixture);
    mmay_formatter_fixture_dispose(&valid_fixture);
}

void mmay_test_formatter_rejects_invalid_types(void) {
    static const char *const invalid[] = {
        "type Missing = ; fn after(): void {}",
        "type EmptyArguments = Array<>; fn after(): void {}",
        "type Qualified = pkg.; fn after(): void {}",
        "type EmptyTuple = []; fn after(): void {}",
        "type NotFunction = (int); fn after(): void {}",
        "type Extra = int string; fn after(): void {}",
        "type MissingClose = Map<string, []int; fn after(): void {}",
        "type ExtraClose = []int>; fn after(): void {}",
        "fn emptyParameters<>(value: int): int { return value; }",
        "fn missingConstraint<T: >(value: T): T { return value; }"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MarimayFormatterFixture fixture;
        MarimayFormatResult result;
        bool initialized = mmay_formatter_fixture_init(
            &fixture,
            "invalid-type.my",
            (const unsigned char *)invalid[index],
            strlen(invalid[index])
        );
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length != 0);
        MMAY_EXPECT(mmay_format_source(
            &fixture.lexed,
            &fixture.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &result
        ) == MMAY_FORMAT_SOURCE_INVALID);
        MMAY_EXPECT(!result.initialized);
        mmay_formatter_fixture_dispose(&fixture);
    }
}
