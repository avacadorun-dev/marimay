#include "test_checker_imports_support.h"
#include "test_checker_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/program.h"
#include "mmay/check/checker.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/native/c_emitter.h"

static const char mmay_receiver_names_source[] =
    "type Counter { mutable value: int; }\n"
    "type Stack { values: []int; mutable size: int; }\n"
    "type Queue { values: []int; mutable size: int; }\n"
    "fn push(stack: Stack, value: int): void {\n"
    "  stack.values.push(value); stack.size += 1;\n"
    "}\n"
    "fn push(queue: Queue, value: int): void {\n"
    "  push(queue.values, value); queue.size += 1;\n"
    "}\n"
    "fn pop(stack: Stack): int {\n"
    "  stack.size -= 1; return stack.values[stack.size];\n"
    "}\n"
    "fn length(stack: Stack): int { return stack.size; }\n"
    "fn length(queue: Queue): int { return queue.size; }\n"
    "fn clear(stack: Stack): void { stack.size = 0; }\n"
    "fn makeStack(counter: Counter): Stack {\n"
    "  counter.value += 1; return Stack { values: [], size: 0 };\n"
    "}\n"
    "fn probe(): int {\n"
    "  const counter = Counter { value: 0 };\n"
    "  makeStack(counter).push(5);\n"
    "  const stack = Stack { values: [], size: 0 };\n"
    "  stack.push(7); push(stack, 9);\n"
    "  const queue = Queue { values: [], size: 0 };\n"
    "  push(queue, 3);\n"
    "  const raw = [1]; push(raw, 11); raw.push(13);\n"
    "  const score = stack.length() + length(stack) + queue.length()\n"
    "    + raw.length() + length(raw);\n"
    "  const taken = stack.pop(); stack.clear();\n"
    "  return counter.value * 10000 + score * 100 + taken * 10\n"
    "    + stack.length();\n"
    "}\n";

static bool mmay_receiver_bytes_equal(
    const MarimayOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static const MarimayCheckedFunction *mmay_receiver_function(
    const MarimayCheckResult *result,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            result, index
        );
        if (function != NULL
            && mmay_receiver_bytes_equal(&function->name, name)) {
            if (result_index != NULL) {
                *result_index = index;
            }
            return function;
        }
    }
    return NULL;
}

static const MarimayCheckedSymbol *mmay_receiver_symbol(
    const MarimayCheckResult *result,
    const char *name,
    MarimayCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == kind
            && mmay_receiver_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static const MarimayCheckedFunction *mmay_receiver_owned_function(
    const MarimayCheckResult *result,
    const char *name,
    MarimaySymbolId receiver
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            result, index
        );
        if (function != NULL && function->dot_eligible
            && mmay_receiver_bytes_equal(&function->name, name)
            && mmay_symbol_id_equal(function->receiver_symbol, receiver)) {
            return function;
        }
    }
    return NULL;
}

static size_t mmay_receiver_call_count(
    const MarimayMirFunction *function,
    MarimaySymbolId callee
) {
    size_t index = 0;
    size_t count = 0;
    for (index = 0; function != NULL && index < function->calls.length;
        index += 1) {
        const MarimayMirCall *call = mmay_vector_at_const(
            &function->calls, index
        );
        if (call != NULL && mmay_symbol_id_equal(call->callee, callee)) {
            count += 1;
        }
    }
    return count;
}

static void mmay_test_receiver_names_behavior(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *probe = NULL;
    const MarimayCheckedFunction *stack_push = NULL;
    const MarimayCheckedSymbol *stack = NULL;
    const MarimayMirFunction *functions[16];
    MarimayMirValue interpreted;
    MarimayBytecodeProgram bytecode;
    MarimayNativeCProgram native;
    MarimayOwnedBytes generated;
    size_t probe_index = SIZE_MAX;
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(&interpreted, 0, sizeof(interpreted));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&native, 0, sizeof(native));
    memset(&generated, 0, sizeof(generated));
    memset(functions, 0, sizeof(functions));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, mmay_receiver_names_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/receiver-names",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    probe = mmay_receiver_function(&checked, "probe", &probe_index);
    stack = mmay_receiver_symbol(
        &checked, "Stack", MMAY_CHECKED_SYMBOL_RECORD
    );
    MMAY_EXPECT(probe != NULL && stack != NULL);
    if (probe != NULL && stack != NULL) {
        stack_push = mmay_receiver_owned_function(
            &checked, "push", stack->symbol
        );
        MMAY_EXPECT(stack_push != NULL);
        if (stack_push != NULL) {
            MMAY_EXPECT(mmay_receiver_call_count(
                &probe->mir, stack_push->mir.symbol
            ) == 3);
        }
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &probe->mir, MMAY_MIR_ARRAY_PUSH
        ) == 2);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &probe->mir, MMAY_MIR_ARRAY_LENGTH
        ) == 4);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            probe_index,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &interpreted
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(interpreted.type == MMAY_TYPE_I64
            && interpreted.as.i64 == 11190);
    }
    MMAY_EXPECT(checked.functions.length <= 16);
    for (index = 0; index < checked.functions.length && index < 16;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        functions[index] = function == NULL ? NULL : &function->mir;
    }
    if (probe != NULL && checked.functions.length <= 16) {
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions,
            checked.functions.length,
            probe->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        native.functions = functions;
        native.function_count = checked.functions.length;
        native.entry_symbol = probe->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &native,
            mmay_system_allocator(),
            mmay_native_c_emit_default_options(),
            &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_program_dispose(&bytecode);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_receiver_names_diagnostics(void) {
    static const char wrong_receiver[] =
        "type Stack {}\n"
        "type Queue {}\n"
        "fn push(value: Stack, item: int): void {}\n"
        "fn probe(value: Queue): void { value.push(1); }\n";
    static const char ambiguous[] =
        "type Stack {}\n"
        "type Queue {}\n"
        "fn push(value: Stack, item: int): void {}\n"
        "fn push(value: Queue, item: int): void {}\n"
        "fn probe(): void { push(); }\n";
    static const char approximate[] =
        "type Stack {}\n"
        "fn push(value: Stack, item: int): void {}\n"
        "fn probe(value: Stack): void { value.puhs(1); }\n";
    mmay_checker_expect_one_diagnostic(
        wrong_receiver,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        2
    );
    mmay_checker_expect_one_diagnostic(
        ambiguous,
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "()",
        1
    );
    mmay_checker_expect_one_diagnostic(
        approximate,
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "puhs",
        0
    );
}

static void mmay_receiver_expect_import_diagnostic(
    const MarimayTestImportSource *sources,
    size_t source_count,
    const char *code
) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture,
        "app/root",
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 1);
    diagnostic = mmay_check_diagnostic(&checked, 0);
    MMAY_EXPECT(diagnostic != NULL && strcmp(diagnostic->code, code) == 0);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_receiver_names_import_diagnostics(void) {
    static const MarimayTestImportSource hidden[] = {
        {
            "acme/store", "store/private.my",
            "public type Stack {}\n"
            "fn push(value: Stack, item: int): void {}\n"
        },
        {
            "app/root", "root/private.my",
            "import { Stack } from \"acme/store\";\n"
            "fn probe(value: Stack): void { value.push(1); }\n"
        }
    };
    static const MarimayTestImportSource wrong[] = {
        {
            "acme/store", "store/wrong.my",
            "public type Stack {}\n"
            "public type Queue {}\n"
            "public fn push(value: Stack, item: int): void {}\n"
        },
        {
            "app/root", "root/wrong.my",
            "import { Queue } from \"acme/store\";\n"
            "fn probe(value: Queue): void { value.push(1); }\n"
        }
    };
    mmay_receiver_expect_import_diagnostic(
        hidden,
        sizeof(hidden) / sizeof(hidden[0]),
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT
    );
    mmay_receiver_expect_import_diagnostic(
        wrong,
        sizeof(wrong) / sizeof(wrong[0]),
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
}

static void mmay_test_receiver_names_allocation_failures(void) {
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, mmay_receiver_names_source));
    for (failure_index = 0; failure_index < 2048; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_check_ast(
            &fixture.ast,
            "test/receiver-names-allocation",
            mmay_fail_allocator(&state),
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
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

void mmay_test_receiver_collection_names(void) {
    mmay_test_receiver_names_behavior();
    mmay_test_receiver_names_diagnostics();
    mmay_test_receiver_names_import_diagnostics();
    mmay_test_receiver_names_allocation_failures();
}
