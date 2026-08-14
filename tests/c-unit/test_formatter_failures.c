#include "test_formatter_support.h"

void ruda_test_formatter_rejection_limits_and_allocation(void) {
    static const char valid[] = "fn main(): void { return; }\n";
    static const char valid_imports[] =
        "import * as zed from \"z.example/z\";\n"
        "import * as io from \"std/io\";\n";
    static const char valid_strings[] =
        "fn strings(): void { const raw = r\"\\\\server\\share\"; "
        "const control = \"\\u{1}\"; }\n";
    static const char invalid[] = "fn broken(: void {\n";
    RudaFormatterFixture valid_fixture;
    RudaFormatterFixture import_fixture;
    RudaFormatterFixture string_fixture;
    RudaFormatterFixture invalid_fixture;
    RudaFormatResult result;
    RudaFormatOptions options;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &valid_fixture,
        "valid.ruda",
        (const unsigned char *)valid,
        strlen(valid)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &import_fixture,
        "imports.ruda",
        (const unsigned char *)valid_imports,
        strlen(valid_imports)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &string_fixture,
        "strings.ruda",
        (const unsigned char *)valid_strings,
        strlen(valid_strings)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &invalid_fixture,
        "invalid.ruda",
        (const unsigned char *)invalid,
        strlen(invalid)
    ));
    RUDA_EXPECT(ruda_format_source(
        &invalid_fixture.lexed,
        &invalid_fixture.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &result
    ) == RUDA_FORMAT_SOURCE_INVALID);
    RUDA_EXPECT(!result.initialized);

    options = ruda_format_default_options();
    options.max_tokens = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_document_parts = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_output_bytes = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_nesting = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &valid_fixture.lexed,
            &valid_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &import_fixture.lexed,
            &import_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 192; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &string_fixture.lexed,
            &string_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_formatter_fixture_dispose(&invalid_fixture);
    ruda_formatter_fixture_dispose(&string_fixture);
    ruda_formatter_fixture_dispose(&import_fixture);
    ruda_formatter_fixture_dispose(&valid_fixture);
}

void ruda_test_formatter_rejects_invalid_types(void) {
    static const char *const invalid[] = {
        "type Missing = ; fn after(): void {}",
        "type EmptyArguments = Array<>; fn after(): void {}",
        "type Qualified = pkg.; fn after(): void {}",
        "type EmptyTuple = []; fn after(): void {}",
        "type NotFunction = (int); fn after(): void {}",
        "type Extra = int string; fn after(): void {}",
        "type MissingClose = Map<string, Array<int>; fn after(): void {}",
        "type ExtraClose = Array<int>>; fn after(): void {}",
        "fn emptyParameters<>(value: int): int { return value; }",
        "fn missingConstraint<T: >(value: T): T { return value; }"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        RudaFormatterFixture fixture;
        RudaFormatResult result;
        bool initialized = ruda_formatter_fixture_init(
            &fixture,
            "invalid-type.ruda",
            (const unsigned char *)invalid[index],
            strlen(invalid[index])
        );
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length != 0);
        RUDA_EXPECT(ruda_format_source(
            &fixture.lexed,
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &result
        ) == RUDA_FORMAT_SOURCE_INVALID);
        RUDA_EXPECT(!result.initialized);
        ruda_formatter_fixture_dispose(&fixture);
    }
}
