#include "test_checker_imports_support.h"
#include "test_checker_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/program.h"
#include "ruda/check/checker.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/native/c_emitter.h"

static const char ruda_receiver_names_source[] =
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

static bool ruda_receiver_bytes_equal(
    const RudaOwnedBytes *bytes,
    const char *text
) {
    size_t length = text == NULL ? 0 : strlen(text);
    return bytes != NULL && text != NULL && bytes->length == length
        && (length == 0 || memcmp(bytes->data, text, length) == 0);
}

static const RudaCheckedFunction *ruda_receiver_function(
    const RudaCheckResult *result,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        if (function != NULL
            && ruda_receiver_bytes_equal(&function->name, name)) {
            if (result_index != NULL) {
                *result_index = index;
            }
            return function;
        }
    }
    return NULL;
}

static const RudaCheckedSymbol *ruda_receiver_symbol(
    const RudaCheckResult *result,
    const char *name,
    RudaCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->symbols.length;
        index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == kind
            && ruda_receiver_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static const RudaCheckedFunction *ruda_receiver_owned_function(
    const RudaCheckResult *result,
    const char *name,
    RudaSymbolId receiver
) {
    size_t index = 0;
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        if (function != NULL && function->dot_eligible
            && ruda_receiver_bytes_equal(&function->name, name)
            && ruda_symbol_id_equal(function->receiver_symbol, receiver)) {
            return function;
        }
    }
    return NULL;
}

static size_t ruda_receiver_call_count(
    const RudaMirFunction *function,
    RudaSymbolId callee
) {
    size_t index = 0;
    size_t count = 0;
    for (index = 0; function != NULL && index < function->calls.length;
        index += 1) {
        const RudaMirCall *call = ruda_vector_at_const(
            &function->calls, index
        );
        if (call != NULL && ruda_symbol_id_equal(call->callee, callee)) {
            count += 1;
        }
    }
    return count;
}

static void ruda_test_receiver_names_behavior(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *probe = NULL;
    const RudaCheckedFunction *stack_push = NULL;
    const RudaCheckedSymbol *stack = NULL;
    const RudaMirFunction *functions[16];
    RudaMirValue interpreted;
    RudaBytecodeProgram bytecode;
    RudaNativeCProgram native;
    RudaOwnedBytes generated;
    size_t probe_index = SIZE_MAX;
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(&interpreted, 0, sizeof(interpreted));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&native, 0, sizeof(native));
    memset(&generated, 0, sizeof(generated));
    memset(functions, 0, sizeof(functions));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, ruda_receiver_names_source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/receiver-names",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    probe = ruda_receiver_function(&checked, "probe", &probe_index);
    stack = ruda_receiver_symbol(
        &checked, "Stack", RUDA_CHECKED_SYMBOL_RECORD
    );
    RUDA_EXPECT(probe != NULL && stack != NULL);
    if (probe != NULL && stack != NULL) {
        stack_push = ruda_receiver_owned_function(
            &checked, "push", stack->symbol
        );
        RUDA_EXPECT(stack_push != NULL);
        if (stack_push != NULL) {
            RUDA_EXPECT(ruda_receiver_call_count(
                &probe->mir, stack_push->mir.symbol
            ) == 3);
        }
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &probe->mir, RUDA_MIR_ARRAY_PUSH
        ) == 2);
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &probe->mir, RUDA_MIR_ARRAY_LENGTH
        ) == 4);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            probe_index,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &interpreted
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(interpreted.type == RUDA_TYPE_I64
            && interpreted.as.i64 == 11190);
    }
    RUDA_EXPECT(checked.functions.length <= 16);
    for (index = 0; index < checked.functions.length && index < 16;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        functions[index] = function == NULL ? NULL : &function->mir;
    }
    if (probe != NULL && checked.functions.length <= 16) {
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions,
            checked.functions.length,
            probe->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &bytecode
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        native.functions = functions;
        native.function_count = checked.functions.length;
        native.entry_symbol = probe->mir.symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &native,
            ruda_system_allocator(),
            ruda_native_c_emit_default_options(),
            &generated
        ) == RUDA_NATIVE_C_EMIT_OK);
    }
    ruda_owned_bytes_dispose(&generated);
    ruda_bytecode_program_dispose(&bytecode);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_receiver_names_diagnostics(void) {
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
    ruda_checker_expect_one_diagnostic(
        wrong_receiver,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        ambiguous,
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "()",
        1
    );
    ruda_checker_expect_one_diagnostic(
        approximate,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "puhs",
        0
    );
}

static void ruda_receiver_expect_import_diagnostic(
    const RudaTestImportSource *sources,
    size_t source_count,
    const char *code
) {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture,
        "app/root",
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 1);
    diagnostic = ruda_check_diagnostic(&checked, 0);
    RUDA_EXPECT(diagnostic != NULL && strcmp(diagnostic->code, code) == 0);
    ruda_check_result_dispose(&checked);
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_test_receiver_names_import_diagnostics(void) {
    static const RudaTestImportSource hidden[] = {
        {
            "acme/store", "store/private.ruda",
            "public type Stack {}\n"
            "fn push(value: Stack, item: int): void {}\n"
        },
        {
            "app/root", "root/private.ruda",
            "import { Stack } from \"acme/store\";\n"
            "fn probe(value: Stack): void { value.push(1); }\n"
        }
    };
    static const RudaTestImportSource wrong[] = {
        {
            "acme/store", "store/wrong.ruda",
            "public type Stack {}\n"
            "public type Queue {}\n"
            "public fn push(value: Stack, item: int): void {}\n"
        },
        {
            "app/root", "root/wrong.ruda",
            "import { Queue } from \"acme/store\";\n"
            "fn probe(value: Queue): void { value.push(1); }\n"
        }
    };
    ruda_receiver_expect_import_diagnostic(
        hidden,
        sizeof(hidden) / sizeof(hidden[0]),
        RUDA_DIAGNOSTIC_PRIVATE_IMPORT
    );
    ruda_receiver_expect_import_diagnostic(
        wrong,
        sizeof(wrong) / sizeof(wrong[0]),
        RUDA_DIAGNOSTIC_TYPE_MISMATCH
    );
}

static void ruda_test_receiver_names_allocation_failures(void) {
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, ruda_receiver_names_source));
    for (failure_index = 0; failure_index < 2048; failure_index += 1) {
        RudaFailAllocator state;
        RudaCheckResult checked;
        RudaCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_check_ast(
            &fixture.ast,
            "test/receiver-names-allocation",
            ruda_fail_allocator(&state),
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 0);
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

void ruda_test_receiver_collection_names(void) {
    ruda_test_receiver_names_behavior();
    ruda_test_receiver_names_diagnostics();
    ruda_test_receiver_names_import_diagnostics();
    ruda_test_receiver_names_allocation_failures();
}
