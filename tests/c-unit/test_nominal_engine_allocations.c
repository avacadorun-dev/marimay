#include "test_checker_support.h"

#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/program.h"

#include <string.h>

static const char mmay_test_nominal_allocation_source[] =
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

static const MarimayCheckedFunction *mmay_test_nominal_allocation_entry(
    const MarimayCheckResult *checked
) {
    size_t index = 0;
    if (checked == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
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

static void mmay_test_nominal_allocation_checked_shape(
    const MarimayCheckResult *checked
) {
    size_t index = 0;
    size_t record_new_count = 0;
    size_t enum_new_count = 0;
    size_t interface_new_count = 0;
    size_t interface_call_count = 0;
    MMAY_EXPECT(checked != NULL && checked->initialized);
    if (checked == NULL || !checked->initialized) {
        return;
    }
    MMAY_EXPECT(checked->diagnostics.length == 0);
    MMAY_EXPECT(checked->records.length == 1);
    MMAY_EXPECT(checked->enums.length == 1);
    MMAY_EXPECT(checked->interfaces.length == 1);
    MMAY_EXPECT(checked->functions.length == 3);
    MMAY_EXPECT(mmay_test_nominal_allocation_entry(checked) != NULL);
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked,
            index
        );
        MMAY_EXPECT(function != NULL);
        if (function == NULL) {
            continue;
        }
        record_new_count += mmay_test_mir_instruction_kind_count(
            &function->mir,
            MMAY_MIR_RECORD_NEW
        );
        enum_new_count += mmay_test_mir_instruction_kind_count(
            &function->mir,
            MMAY_MIR_ENUM_NEW
        );
        interface_new_count += mmay_test_mir_instruction_kind_count(
            &function->mir,
            MMAY_MIR_INTERFACE_NEW
        );
        interface_call_count += mmay_test_mir_instruction_kind_count(
            &function->mir,
            MMAY_MIR_CALL_INTERFACE
        );
    }
    MMAY_EXPECT(record_new_count == 1);
    MMAY_EXPECT(enum_new_count == 1);
    MMAY_EXPECT(interface_new_count == 1);
    MMAY_EXPECT(interface_call_count == 1);
}

static size_t mmay_test_nominal_checker_allocation_count(
    const MarimayAstTree *ast
) {
    MarimayFailAllocator state;
    MarimayCheckResult checked;
    MarimayCheckStatus status;
    size_t attempts = 0;
    memset(&checked, 0, sizeof(checked));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    status = mmay_check_ast(
        ast,
        "test/nominal-engine-allocations",
        mmay_fail_allocator(&state),
        mmay_check_default_options(),
        &checked
    );
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (status == MMAY_CHECK_OK) {
        mmay_test_nominal_allocation_checked_shape(&checked);
    }
    attempts = state.attempts;
    mmay_check_result_dispose(&checked);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    MMAY_EXPECT(attempts != 0);
    return attempts;
}

static void mmay_test_nominal_checker_allocation_failures(
    const MarimayAstTree *ast
) {
    const size_t attempt_limit = mmay_test_nominal_checker_allocation_count(ast);
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0;
        failure_index <= attempt_limit && !reached_success;
        failure_index += 1) {
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
            ast,
            "test/nominal-engine-allocations",
            mmay_fail_allocator(&state),
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            mmay_test_nominal_allocation_checked_shape(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

static bool mmay_test_nominal_lowered_shape(
    const MarimayBytecodeProgram *program,
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
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
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

static size_t mmay_test_nominal_program_lower_allocation_count(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId entry
) {
    MarimayFailAllocator state;
    MarimayBytecodeProgram program;
    MarimayMirBytecodeLowerStatus status;
    size_t attempts = 0;
    memset(&program, 0, sizeof(program));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    status = mmay_mir_lower_bytecode_program(
        functions,
        function_count,
        entry,
        mmay_fail_allocator(&state),
        mmay_mir_bytecode_program_lower_default_options(),
        &program
    );
    MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_test_nominal_lowered_shape(&program, function_count));
    attempts = state.attempts;
    mmay_bytecode_program_dispose(&program);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    MMAY_EXPECT(attempts != 0);
    return attempts;
}

static void mmay_test_nominal_program_lower_allocation_failures(
    const MarimayCheckResult *checked
) {
    const MarimayCheckedFunction *entry = mmay_test_nominal_allocation_entry(checked);
    const MarimayMirFunction *functions[3];
    size_t function_count = 0;
    size_t failure_index = 0;
    size_t attempt_limit = 0;
    bool reached_success = false;
    MMAY_EXPECT(entry != NULL);
    MMAY_EXPECT(checked != NULL && checked->functions.length == 3);
    if (entry == NULL || checked == NULL || checked->functions.length != 3) {
        return;
    }
    for (function_count = 0; function_count < checked->functions.length;
        function_count += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked,
            function_count
        );
        MMAY_EXPECT(function != NULL);
        if (function == NULL) {
            return;
        }
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
    }
    attempt_limit = mmay_test_nominal_program_lower_allocation_count(
        functions,
        function_count,
        entry->mir.symbol
    );
    for (failure_index = 0;
        failure_index <= attempt_limit && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program;
        MarimayMirBytecodeLowerStatus status;
        memset(&program, 0, sizeof(program));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        status = mmay_mir_lower_bytecode_program(
            functions,
            function_count,
            entry->mir.symbol,
            mmay_fail_allocator(&state),
            mmay_mir_bytecode_program_lower_default_options(),
            &program
        );
        if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            MMAY_EXPECT(mmay_test_nominal_lowered_shape(
                &program,
                function_count
            ));
        } else {
            MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            MMAY_EXPECT(!program.initialized);
        }
        mmay_bytecode_program_dispose(&program);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_nominal_engine_allocations(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        mmay_test_nominal_allocation_source
    ));
    if (!fixture.ast_initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    mmay_test_nominal_checker_allocation_failures(&fixture.ast);
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/nominal-engine-allocations",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        mmay_test_nominal_allocation_checked_shape(&checked);
        mmay_test_nominal_program_lower_allocation_failures(&checked);
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
