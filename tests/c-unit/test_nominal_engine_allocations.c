#include "test_checker_support.h"

#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/program.h"

#include <string.h>

static const char ruda_test_nominal_allocation_source[] =
    "type Box { mutable value: int; }\n"
    "enum Envelope { Wrapped(Box), Empty, }\n"
    "interface Show { show(): int; }\n"
    "fn show(value: Box): int { return value.value; }\n"
    "fn consume(value: Show): int { return value.show(); }\n"
    "fn main(): int {\n"
    "  const box = Box { value: 7 };\n"
    "  const envelope = Envelope.Wrapped(box);\n"
    "  return match envelope {\n"
    "    case Wrapped(value) => consume(value);\n"
    "    case Empty => 0;\n"
    "  };\n"
    "}\n";

static const RudaCheckedFunction *ruda_test_nominal_allocation_entry(
    const RudaCheckResult *checked
) {
    size_t index = 0;
    if (checked == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            index
        );
        if (function != NULL && function->name.length == sizeof("main") - 1
            && memcmp(
                function->name.data,
                "main",
                sizeof("main") - 1
            ) == 0) {
            return function;
        }
    }
    return NULL;
}

static void ruda_test_nominal_allocation_checked_shape(
    const RudaCheckResult *checked
) {
    size_t index = 0;
    size_t record_new_count = 0;
    size_t enum_new_count = 0;
    size_t interface_new_count = 0;
    size_t interface_call_count = 0;
    RUDA_EXPECT(checked != NULL && checked->initialized);
    if (checked == NULL || !checked->initialized) {
        return;
    }
    RUDA_EXPECT(checked->diagnostics.length == 0);
    RUDA_EXPECT(checked->records.length == 1);
    RUDA_EXPECT(checked->enums.length == 1);
    RUDA_EXPECT(checked->interfaces.length == 1);
    RUDA_EXPECT(checked->functions.length == 3);
    RUDA_EXPECT(ruda_test_nominal_allocation_entry(checked) != NULL);
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            index
        );
        RUDA_EXPECT(function != NULL);
        if (function == NULL) {
            continue;
        }
        record_new_count += ruda_test_mir_instruction_kind_count(
            &function->mir,
            RUDA_MIR_RECORD_NEW
        );
        enum_new_count += ruda_test_mir_instruction_kind_count(
            &function->mir,
            RUDA_MIR_ENUM_NEW
        );
        interface_new_count += ruda_test_mir_instruction_kind_count(
            &function->mir,
            RUDA_MIR_INTERFACE_NEW
        );
        interface_call_count += ruda_test_mir_instruction_kind_count(
            &function->mir,
            RUDA_MIR_CALL_INTERFACE
        );
    }
    RUDA_EXPECT(record_new_count == 1);
    RUDA_EXPECT(enum_new_count == 1);
    RUDA_EXPECT(interface_new_count == 1);
    RUDA_EXPECT(interface_call_count == 1);
}

static size_t ruda_test_nominal_checker_allocation_count(
    const RudaAstTree *ast
) {
    RudaFailAllocator state;
    RudaCheckResult checked;
    RudaCheckStatus status;
    size_t attempts = 0;
    memset(&checked, 0, sizeof(checked));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    status = ruda_check_ast(
        ast,
        "test/nominal-engine-allocations",
        ruda_fail_allocator(&state),
        ruda_check_default_options(),
        &checked
    );
    RUDA_EXPECT(status == RUDA_CHECK_OK);
    if (status == RUDA_CHECK_OK) {
        ruda_test_nominal_allocation_checked_shape(&checked);
    }
    attempts = state.attempts;
    ruda_check_result_dispose(&checked);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
    RUDA_EXPECT(attempts != 0);
    return attempts;
}

static void ruda_test_nominal_checker_allocation_failures(
    const RudaAstTree *ast
) {
    const size_t attempt_limit = ruda_test_nominal_checker_allocation_count(ast);
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0;
        failure_index <= attempt_limit && !reached_success;
        failure_index += 1) {
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
            ast,
            "test/nominal-engine-allocations",
            ruda_fail_allocator(&state),
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            ruda_test_nominal_allocation_checked_shape(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        ruda_check_result_dispose(&checked);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

static bool ruda_test_nominal_lowered_shape(
    const RudaBytecodeProgram *program,
    size_t function_count
) {
    size_t index = 0;
    size_t record_layouts = 0;
    size_t enum_layouts = 0;
    size_t interface_layouts = 0;
    size_t interface_constructions = 0;
    if (program == NULL || !program->initialized
        || program->functions.length != function_count
        || program->entry_function >= program->functions.length) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function =
            ruda_bytecode_program_function(program, index);
        if (function == NULL) {
            return false;
        }
        record_layouts += function->body.record_layouts.length;
        enum_layouts += function->body.enum_layouts.length;
        interface_layouts += function->body.interface_layouts.length;
        interface_constructions +=
            function->body.interface_constructions.length;
    }
    return record_layouts != 0 && enum_layouts != 0
        && interface_layouts != 0 && interface_constructions != 0;
}

static size_t ruda_test_nominal_program_lower_allocation_count(
    const RudaMirFunction *const *functions,
    size_t function_count,
    RudaSymbolId entry
) {
    RudaFailAllocator state;
    RudaBytecodeProgram program;
    RudaMirBytecodeLowerStatus status;
    size_t attempts = 0;
    memset(&program, 0, sizeof(program));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    status = ruda_mir_lower_bytecode_program(
        functions,
        function_count,
        entry,
        ruda_fail_allocator(&state),
        ruda_mir_bytecode_program_lower_default_options(),
        &program
    );
    RUDA_EXPECT(status == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_test_nominal_lowered_shape(&program, function_count));
    attempts = state.attempts;
    ruda_bytecode_program_dispose(&program);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
    RUDA_EXPECT(attempts != 0);
    return attempts;
}

static void ruda_test_nominal_program_lower_allocation_failures(
    const RudaCheckResult *checked
) {
    const RudaCheckedFunction *entry = ruda_test_nominal_allocation_entry(checked);
    const RudaMirFunction *functions[3];
    size_t function_count = 0;
    size_t failure_index = 0;
    size_t attempt_limit = 0;
    bool reached_success = false;
    RUDA_EXPECT(entry != NULL);
    RUDA_EXPECT(checked != NULL && checked->functions.length == 3);
    if (entry == NULL || checked == NULL || checked->functions.length != 3) {
        return;
    }
    for (function_count = 0; function_count < checked->functions.length;
        function_count += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            function_count
        );
        RUDA_EXPECT(function != NULL);
        if (function == NULL) {
            return;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
    }
    attempt_limit = ruda_test_nominal_program_lower_allocation_count(
        functions,
        function_count,
        entry->mir.symbol
    );
    for (failure_index = 0;
        failure_index <= attempt_limit && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        RudaMirBytecodeLowerStatus status;
        memset(&program, 0, sizeof(program));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        status = ruda_mir_lower_bytecode_program(
            functions,
            function_count,
            entry->mir.symbol,
            ruda_fail_allocator(&state),
            ruda_mir_bytecode_program_lower_default_options(),
            &program
        );
        if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            RUDA_EXPECT(ruda_test_nominal_lowered_shape(
                &program,
                function_count
            ));
        } else {
            RUDA_EXPECT(status == RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            RUDA_EXPECT(!program.initialized);
        }
        ruda_bytecode_program_dispose(&program);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_nominal_engine_allocations(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        ruda_test_nominal_allocation_source
    ));
    if (!fixture.ast_initialized) {
        ruda_checker_fixture_dispose(&fixture);
        return;
    }
    ruda_test_nominal_checker_allocation_failures(&fixture.ast);
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/nominal-engine-allocations",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        ruda_test_nominal_allocation_checked_shape(&checked);
        ruda_test_nominal_program_lower_allocation_failures(&checked);
    }
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}
