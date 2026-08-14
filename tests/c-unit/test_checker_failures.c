#include "test_checker_support.h"

void ruda_test_checker_unsupported_is_explicit(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn sum(a: i32, b: i32): i32 { return a + b; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn iterate(items: int): void { for const item of items {} }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "const message: string = \"not implemented\";\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_limits_are_failure_atomic(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int { return a + b * c; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options = ruda_check_default_options();
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options.mir_options.max_registers = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { value: int; }\n"
        "fn same(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_registers = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { first: int; second: int; }\n"
        "type Other { value: int; }\n"
        "fn build(): int {\n"
        "  const pair = Pair { second: 2, first: 1 };\n"
        "  const other = Other { value: 3 };\n"
        "  return pair.first + other.value;\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_record_fields = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_constructions = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_arguments = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_layouts = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { first: int; second: int; }\n"
        "fn project(pair: Pair): int {\n"
        "  return match pair { case Pair { first, second } => first + second; };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.max_locals_per_function = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_fields = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_registers = 4;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum First { Unit, Pair(int, int), Named { x: int; y: int; }, }\n"
        "enum Second { Unit, }\n"
        "fn build(value: int): Second {\n"
        "  const unit = First.Unit;\n"
        "  const pair = First.Pair(value, value);\n"
        "  return Second.Unit;\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_enum_layouts = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_variants = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_payloads = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_constructions = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_arguments = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum Choice { A, B(int, int), C, }\n"
        "fn choose(value: Choice): int {\n"
        "  return match value {\n"
        "    case A => 0;\n"
        "    case B(left, right) => left + right;\n"
        "    case C => 3;\n"
        "  };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_blocks = 5;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_locals_per_function = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(\"a\");\n"
        "    case false => panic(\"b\");\n"
        "  };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(value: string): int {\n"
        "  return match value { case \"a\" => 1; case \"b\" => 2; case _ => 0; };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_string_bytes = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(): int { panic(\"ab\"); }\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_bytes = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_allocation_failures(void) {
    static const char source[] =
        "type Count = int;\n"
        "type Box { public count: Count; mutable next: Box; }\n"
        "type Pair { left: Count; mutable right: Count; }\n"
        "enum Signal { Idle, Paused, Counted(Count), }\n"
        "const one: Count = 1;\n"
        "fn seed(ready: bool): Count {\n"
        "  if ready && one == 1 { return one; } else { return 2; }\n"
        "}\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { case 0 => 0; case other => other; };\n"
        "}\n"
        "fn text(value: string): Count {\n"
        "  return match value { case \"ready\" | r\"done\" => 1; case _ => 0; };\n"
        "}\n"
        "fn allStop(value: bool): Count {\n"
        "  while match value {\n"
        "    case true => { return 1; } case false => { return 0; }\n"
        "  } {}\n"
        "}\n"
        "fn guardStop(value: Count, ready: bool): Count {\n"
        "  return match value {\n"
        "    case 0 if match ready {\n"
        "      case true => { return 1; } case false => { return 2; }\n"
        "    } => 9;\n"
        "    case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn stepStop(value: bool, ready: bool): Count {\n"
        "  for let index = 0; ready; index = match value {\n"
        "    case true => { return 1; } case false => { return 2; }\n"
        "  } {}\n"
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
        "  return match item { case Pair { left, right } => left + right; };\n"
        "}\n"
        "fn pairSame(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn signal(value: Count): Signal {\n"
        "  return Signal.Counted(value);\n"
        "}\n"
        "fn signalValue(signal: Signal): Count {\n"
        "  return match signal {\n"
        "    case Idle | Paused => { return 0; }\n"
        "    case Counted(value) if value > 0 => { yield value; }\n"
        "    case Counted(_) => { yield 0; }\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_ast(
            &fixture.ast,
            "test/checker",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.functions.length == 16);
            RUDA_EXPECT(checked.constants.length == 1);
            RUDA_EXPECT(checked.type_aliases.length == 1);
            RUDA_EXPECT(checked.records.length == 2);
            RUDA_EXPECT(checked.enums.length == 1);
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

void ruda_test_checker_package_allocation_failures(void) {
    static const char entry_source[] =
        "fn entry(value: Count): Count { return helper(value) + offset; }\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { case 0 => 0; case other => other; };\n"
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
        "    case Ready | Paused => 0;\n"
        "    case Counted(value) if value > 0 => value;\n"
        "    case Counted(_) => 0;\n"
        "  };\n"
        "}\n";
    static const char helper_source[] =
        "type Count = int;\n"
        "type Marker { public count: Count; }\n"
        "enum Status { Ready, Paused, Counted(Count), }\n"
        "const offset: Count = 1;\n"
        "fn helper(value: Count): Count { return value; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture helper;
    const RudaAstTree *asts[2];
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_helper.ruda",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_package(
            asts,
            2,
            "test/package",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.functions.length == 7);
            RUDA_EXPECT(checked.constants.length == 1);
            RUDA_EXPECT(checked.type_aliases.length == 1);
            RUDA_EXPECT(checked.records.length == 1);
            RUDA_EXPECT(checked.enums.length == 1);
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
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);
}
