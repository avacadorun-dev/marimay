#include "test_checker_support.h"

static void mmay_test_checker_generics_limits(void) {
    static const char too_many_parameters[] =
        "fn pair<T, U>(left: T, right: U): T { return left; }\n"
        "fn main(): int { return pair(1, 2); }\n";
    static const char too_many_instances[] =
        "fn identity<T>(value: T): T { return value; }\n"
        "fn main(): int { return identity(7); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, too_many_parameters));
    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.type_options.max_type_arguments = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/generics-type-argument-limit",
        mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, too_many_instances));
    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.max_functions = 2;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/generics-specialization-limit",
        mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_generics_recursion_and_size(void) {
    static const char recursive_source[] =
        "fn recurse<T>(value: T): T { return recurse(value); }\n"
        "fn main(): int { return recurse(7); }\n";
    static const char nested_source[] =
        "fn nested<T>(value: Result<Option<T>, T>): Result<Option<T>, T> {\n"
        "  return value;\n"
        "}\n"
        "fn main(): int { return 0; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, recursive_source));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/generics-recursion",
        mmay_system_allocator(), mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, nested_source));
    memset(&checked, 0, sizeof(checked));
    options = mmay_check_default_options();
    options.type_options.max_types = MMAY_TYPE_FIRST_DYNAMIC + 1;
    {
        MarimayCheckStatus nested_status = mmay_check_ast(
        &fixture.ast, "test/generics-nested-type-limit",
        mmay_system_allocator(), options, &checked
        );
        MMAY_EXPECT(nested_status == MMAY_CHECK_LIMIT_REACHED);
    }
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_generics_allocation_failures(void) {
    static const char source[] =
        "fn identity<T>(value: T): T { return value; }\n"
        "fn main(): int { return identity(7); }\n";
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_check_ast(
            &fixture.ast, "test/generics-allocation",
            allocator, mmay_check_default_options(), &checked
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

void mmay_test_checker_generics(void) {
    static const char identity_source[] =
        "fn identity<T>(value: T): T { return value; }\n"
        "enum Maybe<T> { None, Some(T), }\n"
        "type Box<T> { value: T, }\n"
        "enum TryError { Bad, }\n"
        "enum InputError { Bad, }\n"
        "enum OutputError { Config, }\n"
        "interface Show { show(): string; }\n"
        "type Thing { value: int, }\n"
        "fn option(): Option<int> { return Some(7); }\n"
        "fn result(): Result<int, int> { return Ok(7); }\n"
        "fn maybe(): Maybe<int> { return Maybe.Some(7); }\n"
        "fn box(): Box<int> { return Box { value: 7 }; }\n"
        "fn boxValue(): int { return box().value; }\n"
        "fn produced(): Result<int, TryError> { return Ok(7); }\n"
        "fn resultValue(value: Result<int, TryError>): int {\n"
        "  return match value { Ok(number) => number, Err(_) => 0, };\n"
        "}\n"
        "fn resultMatched(): int { return resultValue(produced()); }\n"
        "fn propagated(): Result<int, TryError> { const value = produced()?; return Ok(value); }\n"
        "fn input(): Result<int, InputError> { return Ok(7); }\n"
        "fn configError(error: InputError): OutputError { return OutputError.Config; }\n"
        "fn mapped(): Result<int, OutputError> { return input().mapError(configError); }\n"
        "fn show(value: Thing): string { return \"thing\"; }\n"
        "fn constrained<T: Show>(value: T): T { return value; }\n"
        "fn constrainedUse(): Thing { return constrained(Thing { value: 1 }); }\n"
        "fn main(): int { return identity(7); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, identity_source));
    MarimayCheckStatus status = mmay_check_ast(
        &fixture.ast,
        "test/generics",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    );
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 18); /* Includes the ordinary mapper adapter. */
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "fn identity<T>(value: T): T { return value; }\n"
        "fn main(): int { return identity<int, string>(7); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "identity",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "interface Show { show(): string; }\n"
        "fn constrained<T: Show>(value: T): T { return value; }\n"
        "fn main(): int { return constrained(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "constrained",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum InputError { Bad, }\n"
        "enum OutputError { Config, }\n"
        "fn input(): Result<int, InputError> { return Ok(1); }\n"
        "fn main(): Result<int, OutputError> { const value = input()?; return Ok(value); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "input()?",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum InputError { Bad, }\n"
        "enum OutputError { Config, }\n"
        "fn input(): Result<int, InputError> { return Ok(1); }\n"
        "fn wrong(error: OutputError): InputError { return InputError.Bad; }\n"
        "fn main(): Result<int, OutputError> { const value = input().mapError(wrong); return Ok(value); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "wrong",
        1
    );
    mmay_test_checker_generics_recursion_and_size();
    mmay_test_checker_generics_limits();
    mmay_test_checker_generics_allocation_failures();
}
