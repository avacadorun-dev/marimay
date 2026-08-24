#include "test_checker_support.h"

void mmay_test_checker_unsupported_is_explicit(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn sum(a: i32, b: i32): i32 { return a + b; }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn iterate(items: int): void { for item in items {} }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(strcmp(
            mmay_check_diagnostic(&checked, 0)->code,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH
        ) == 0);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "const message: string = \"not implemented\";\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_INCOMPLETE_LANGUAGE);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_limits_are_failure_atomic(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int { return a + b * c; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options = mmay_check_default_options();
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    options.mir_options.max_registers = 3;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type Pair { value: int, }\n"
        "fn same(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_registers = 2;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type Pair { first: int, second: int, }\n"
        "type Other { value: int, }\n"
        "fn build(): int {\n"
        "  const pair = Pair { second: 2, first: 1 };\n"
        "  const other = Other { value: 3 };\n"
        "  return pair.first + other.value;\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_record_fields = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_record_constructions = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_record_arguments = 2;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_record_layouts = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type Pair { first: int, second: int, }\n"
        "fn project(pair: Pair): int {\n"
        "  return match pair { Pair { first, second } => first + second, };\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.max_locals_per_function = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_record_fields = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_registers = 4;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "enum First { Unit, Pair(int, int), Named { x: int, y: int, }, }\n"
        "enum Second { Unit, }\n"
        "fn build(value: int): Second {\n"
        "  const unit = First.Unit;\n"
        "  const pair = First.Pair(value, value);\n"
        "  return Second.Unit;\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_enum_layouts = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_enum_variants = 3;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_enum_payloads = 3;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_enum_constructions = 2;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_enum_arguments = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "enum Choice { A, B(int, int), C, }\n"
        "fn choose(value: Choice): int {\n"
        "  return match value {\n"
        "    A => 0,\n"
        "    B(left, right) => left + right,\n"
        "    C => 3,\n"
        "  };\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_blocks = 5;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_locals_per_function = 2;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn limited(value: bool): int {\n"
        "  return match value {\n"
        "    true => panic(\"a\"),\n"
        "    false => panic(\"b\"),\n"
        "  };\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_string_constants = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn limited(value: string): int {\n"
        "  return match value { \"a\" => 1, \"b\" => 2, _ => 0, };\n"
        "}\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_string_constants = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.mir_options.max_string_bytes = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn limited(): int { panic(\"ab\"); }\n"
    ));
    options = mmay_check_default_options();
    options.mir_options.max_string_bytes = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_allocation_failures(void) {
    static const char source[] =
        "type Count = int;\n"
        "type Box { public count: Count, mutable next: Box, }\n"
        "type Pair { left: Count, mutable right: Count, }\n"
        "enum Signal { Idle, Paused, Counted(Count), }\n"
        "const one: Count = 1;\n"
        "fn seed(ready: bool): Count {\n"
        "  if ready && one == 1 { return one; } else { return 2; }\n"
        "}\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { true => 1, false => 0, };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { 0 => 0, _ as other => other, };\n"
        "}\n"
        "fn text(value: string): Count {\n"
        "  return match value { \"ready\" | r\"done\" => 1, _ => 0, };\n"
        "}\n"
        "fn allStop(value: bool): Count {\n"
        "  while match value {\n"
        "    true => { return 1; }, false => { return 0; },\n"
        "  } {}\n"
        "}\n"
        "fn guardStop(value: Count, ready: bool): Count {\n"
        "  return match value {\n"
        "    0 if match ready {\n"
        "      true => { return 1; }, false => { return 2; },\n"
        "    } => 9,\n"
        "    _ => 0,\n"
        "  };\n"
        "}\n"
        "fn stepStop(value: bool, ready: bool): Count {\n"
        "  let index = 0;\n"
        "  while ready { index = match value {\n"
        "    true => { return 1; }, false => { return 2; },\n"
        "  }; }\n"
        "  return 3;\n"
        "}\n"
        "fn panicStop(): Count { panic(\"allocation\"); }\n"
        "fn declaredStop(): never { panic(\"declared allocation\"); }\n"
        "fn callStop(): Count { declaredStop(); }\n"
        "fn flow(ready: bool, limit: Count): Count {\n"
        "  let value: Count = seed(ready);\n"
        "  while value < limit { value = value + 1; }\n"
        "  return value;\n"
        "}\n"
        "fn pair(value: Count): Count {\n"
        "  const item = Pair { right: value, left: value };\n"
        "  item.right += 1;\n"
        "  return item.right;\n"
        "}\n"
        "fn pairMatch(item: Pair): Count {\n"
        "  return match item { Pair { left, right } => left + right, };\n"
        "}\n"
        "fn pairSame(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn signal(value: Count): Signal {\n"
        "  return Signal.Counted(value);\n"
        "}\n"
        "fn signalValue(signal: Signal): Count {\n"
        "  return match signal {\n"
        "    Idle | Paused => { return 0; },\n"
        "    Counted(value) if value > 0 => { value },\n"
        "    Counted(_) => { 0 },\n"
        "  };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_check_ast(
            &fixture.ast,
            "test/checker",
            allocator,
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.functions.length == 16);
            MMAY_EXPECT(checked.constants.length == 1);
            MMAY_EXPECT(checked.type_aliases.length == 1);
            MMAY_EXPECT(checked.records.length == 2);
            MMAY_EXPECT(checked.enums.length == 1);
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

void mmay_test_checker_package_allocation_failures(void) {
    static const char entry_source[] =
        "fn entry(value: Count): Count { return helper(value) + offset; }\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { true => 1, false => 0, };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { 0 => 0, _ as other => other, };\n"
        "}\n"
        "fn project(value: Count): Count {\n"
        "  const marker = Marker { count: value };\n"
        "  return marker.count;\n"
        "}\n"
        "fn status(value: Count): Status {\n"
        "  return Status.Counted(value);\n"
        "}\n"
        "fn statusValue(status: Status): Count {\n"
        "  return match status {\n"
        "    Ready | Paused => 0,\n"
        "    Counted(value) if value > 0 => value,\n"
        "    Counted(_) => 0,\n"
        "  };\n"
        "}\n";
    static const char helper_source[] =
        "type Count = int;\n"
        "type Marker { public count: Count, }\n"
        "enum Status { Ready, Paused, Counted(Count), }\n"
        "const offset: Count = 1;\n"
        "fn helper(value: Count): Count { return value; }\n";
    MarimayCheckerFixture entry;
    MarimayCheckerFixture helper;
    const MarimayAstTree *asts[2];
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &entry,
        "a_entry.my",
        entry_source
    ));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &helper,
        "z_helper.my",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_check_package(
            asts,
            2,
            "test/package",
            allocator,
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.functions.length == 7);
            MMAY_EXPECT(checked.constants.length == 1);
            MMAY_EXPECT(checked.type_aliases.length == 1);
            MMAY_EXPECT(checked.records.length == 1);
            MMAY_EXPECT(checked.enums.length == 1);
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
    mmay_checker_fixture_dispose(&helper);
    mmay_checker_fixture_dispose(&entry);
}
