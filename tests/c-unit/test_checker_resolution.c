#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/check/checker.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/mir/mir.h"
#include "ruda/native/c_emitter.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
#include "ruda/types/type.h"

typedef struct RudaCheckerResolutionFixture {
    RudaSourceManager sources;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} RudaCheckerResolutionFixture;

static bool ruda_checker_resolution_fixture_init(
    RudaCheckerResolutionFixture *fixture,
    const char *source
) {
    const RudaSourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(
            &fixture->sources,
            ruda_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (ruda_source_manager_add(
            &fixture->sources,
            "resolution.ruda",
            strlen("resolution.ruda"),
            source,
            strlen(source),
            &snapshot
        ) != RUDA_SOURCE_OK) {
        return false;
    }
    if (ruda_lex_snapshot(
            snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || ruda_ast_project(
            &fixture->parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &fixture->ast
        ) != RUDA_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static void ruda_checker_resolution_fixture_dispose(
    RudaCheckerResolutionFixture *fixture
) {
    if (fixture->ast_initialized) {
        ruda_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        ruda_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        ruda_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        ruda_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static const RudaCheckedFunction *ruda_checker_resolution_function(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t name_length = name == NULL ? 0 : strlen(name);
    if (checked == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            index
        );
        if (function != NULL && function->name.length == name_length
            && memcmp(function->name.data, name, name_length) == 0) {
            return function;
        }
    }
    return NULL;
}

static const RudaMirCall *ruda_checker_resolution_call(
    const RudaCheckedFunction *function,
    size_t index
) {
    if (function == NULL) {
        return NULL;
    }
    return ruda_vector_at_const(&function->mir.calls, index);
}

static void ruda_checker_resolution_expect(
    const char *source,
    const char *code,
    const char *message,
    const char *marked_text,
    size_t occurrence
) {
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const char *marked = source;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, source));
    for (index = 0; index <= occurrence && marked != NULL; index += 1) {
        marked = strstr(marked, marked_text);
        if (index != occurrence && marked != NULL) {
            marked += strlen(marked_text);
        }
    }
    RUDA_EXPECT(marked != NULL);
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            if (strcmp(diagnostic->code, code) != 0
                || diagnostic->message == NULL
                || strcmp(diagnostic->message, message) != 0
                || diagnostic->start != (size_t)(marked - source)
                || diagnostic->end
                    != (size_t)(marked - source) + strlen(marked_text)) {
                fprintf(
                    stderr,
                    "resolution diagnostic mismatch: code %s message '%s' "
                    "span %zu..%zu\n",
                    diagnostic->code == NULL ? "" : diagnostic->code,
                    diagnostic->message == NULL ? "" : diagnostic->message,
                    diagnostic->start,
                    diagnostic->end
                );
            }
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->message != NULL);
            RUDA_EXPECT(strcmp(diagnostic->message, message) == 0);
            RUDA_EXPECT(diagnostic->start == (size_t)(marked - source));
            RUDA_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);
}

static int64_t ruda_checker_resolution_interpret(
    const RudaCheckResult *checked,
    const char *name
) {
    const RudaCheckedFunction *function = ruda_checker_resolution_function(
        checked,
        name
    );
    RudaMirValue result;
    size_t index = 0;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(function != NULL);
    if (function == NULL) {
        return 0;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        if (ruda_checked_function(checked, index) == function) {
            break;
        }
    }
    RUDA_EXPECT(ruda_check_interpret_function(
        checked,
        index,
        NULL,
        0,
        ruda_system_allocator(),
        ruda_check_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    return result.as.i64;
}

static void ruda_test_checker_resolution_free_dot_equivalence(void) {
    static const char source[] =
        "type User { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn both(): int {\n"
        "  const user = User { id: 7 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *callee = NULL;
    const RudaCheckedFunction *both = NULL;
    const RudaMirCall *dot_call = NULL;
    const RudaMirCall *free_call = NULL;
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-equiv",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        callee = ruda_checker_resolution_function(&checked, "string");
        both = ruda_checker_resolution_function(&checked, "both");
        RUDA_EXPECT(callee != NULL && both != NULL);
        if (both != NULL) {
            RUDA_EXPECT(both->mir.calls.length == 2);
            RUDA_EXPECT(both->mir.call_arguments.length == 2);
            dot_call = ruda_checker_resolution_call(both, 0);
            free_call = ruda_checker_resolution_call(both, 1);
            RUDA_EXPECT(dot_call != NULL && free_call != NULL);
            if (dot_call != NULL && free_call != NULL && callee != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(
                    dot_call->callee,
                    free_call->callee
                ));
                RUDA_EXPECT(ruda_symbol_id_equal(
                    dot_call->callee,
                    callee->mir.symbol
                ));
            }
        }
        RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "both") == 14);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);
}

static void ruda_test_checker_resolution_alias_and_enum_overloads(void) {
    static const char alias_source[] =
        "type User { id: int; }\n"
        "type Handle = User;\n"
        "fn string(user: Handle): int { return user.id; }\n"
        "fn both(): int {\n"
        "  const user = User { id: 3 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    static const char enum_source[] =
        "type User { id: int; }\n"
        "enum Status { Ready, Done }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(status: Status): int {\n"
        "  return match status { case Ready => 1; case Done => 2; };\n"
        "}\n"
        "fn both(): int {\n"
        "  const user = User { id: 10 };\n"
        "  const status = Status.Ready;\n"
        "  return user.string() + string(status) + status.string()"
        " + string(user);\n"
        "}\n";
    static const char duplicate_alias[] =
        "type User { id: int; }\n"
        "type Handle = User;\n"
        "fn string(user: User): int { return 1; }\n"
        "fn string(user: Handle): int { return 2; }\n";
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *user_string = NULL;
    const RudaCheckedFunction *status_string = NULL;
    const RudaCheckedFunction *both = NULL;
    const RudaMirCall *call = NULL;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, alias_source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-alias",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        both = ruda_checker_resolution_function(&checked, "both");
        user_string = ruda_checker_resolution_function(&checked, "string");
        RUDA_EXPECT(both != NULL && user_string != NULL);
        if (both != NULL && user_string != NULL) {
            RUDA_EXPECT(both->mir.calls.length == 2);
            for (index = 0; index < 2; index += 1) {
                call = ruda_checker_resolution_call(both, index);
                RUDA_EXPECT(call != NULL);
                if (call != NULL) {
                    RUDA_EXPECT(ruda_symbol_id_equal(
                        call->callee,
                        user_string->mir.symbol
                    ));
                }
            }
        }
        RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "both") == 6);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, enum_source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-enum",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 3);
        both = ruda_checker_resolution_function(&checked, "both");
        RUDA_EXPECT(both != NULL);
        if (both != NULL) {
            RUDA_EXPECT(both->mir.calls.length == 4);
            user_string = NULL;
            status_string = NULL;
            for (index = 0; index < checked.functions.length; index += 1) {
                const RudaCheckedFunction *function = ruda_checked_function(
                    &checked,
                    index
                );
                if (function != NULL && function->name.length == 6
                    && memcmp(function->name.data, "string", 6) == 0) {
                    if (user_string == NULL) {
                        user_string = function;
                    } else {
                        status_string = function;
                    }
                }
            }
            RUDA_EXPECT(user_string != NULL && status_string != NULL);
            if (user_string != NULL && status_string != NULL) {
                RUDA_EXPECT(!ruda_symbol_id_equal(
                    user_string->mir.symbol,
                    status_string->mir.symbol
                ));
                call = ruda_checker_resolution_call(both, 0);
                RUDA_EXPECT(call != NULL && ruda_symbol_id_equal(
                    call->callee,
                    user_string->mir.symbol
                ));
                call = ruda_checker_resolution_call(both, 1);
                RUDA_EXPECT(call != NULL && ruda_symbol_id_equal(
                    call->callee,
                    status_string->mir.symbol
                ));
                call = ruda_checker_resolution_call(both, 2);
                RUDA_EXPECT(call != NULL && ruda_symbol_id_equal(
                    call->callee,
                    status_string->mir.symbol
                ));
                call = ruda_checker_resolution_call(both, 3);
                RUDA_EXPECT(call != NULL && ruda_symbol_id_equal(
                    call->callee,
                    user_string->mir.symbol
                ));
            }
        }
        RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "both") == 22);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);

    ruda_checker_resolution_expect(
        duplicate_alias,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "name is already declared in this scope",
        "string",
        1
    );
}

static void ruda_test_checker_resolution_receiver_once(void) {
    static const char source[] =
        "type Box { mutable value: int; }\n"
        "fn mark(box: Box, bit: int): Box {\n"
        "  box.value = box.value * 10 + bit;\n"
        "  return box;\n"
        "}\n"
        "fn combine(box: Box, extra: int): int {\n"
        "  return box.value * 100 + extra;\n"
        "}\n"
        "fn dotOrder(): int {\n"
        "  let box = Box { value: 0 };\n"
        "  return mark(box, 1).combine(mark(box, 2).value);\n"
        "}\n"
        "fn freeOrder(): int {\n"
        "  let box = Box { value: 0 };\n"
        "  return combine(mark(box, 1), mark(box, 2).value);\n"
        "}\n";
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-order",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "dotOrder")
            == 1212);
        RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "freeOrder")
            == 1212);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);
}

static void ruda_test_checker_resolution_negative_matrix(void) {
    ruda_checker_resolution_expect(
        "fn bad(): int { return missing(1); }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "unknown function name",
        "missing",
        0
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "fn bad(user: User): int { return user.missing(); }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "unknown function name",
        "missing",
        0
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "fn bad(): int { return User(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "called name is not a function",
        "User",
        1
    );
    ruda_checker_resolution_expect(
        "fn label(value: int): int { return value; }\n"
        "fn bad(value: int): int { return value.label(); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "function is not dot-call eligible because its first parameter is not an exact locally owned nominal type",
        "label",
        1
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(): int { return 1.string(); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "dot-call receiver is not a locally owned nominal type",
        "1",
        0
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "type Order { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(order: Order): int { return order.string(); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "an eligible function with this name exists for a different locally owned nominal type",
        "order",
        1
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "type Order { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(): int { return string(); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "overloaded free call requires a first argument of a locally owned nominal type; value.name() and name(value) resolve to one symbol",
        "()",
        1
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "type Order { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(): int { return string(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "overloaded free call first argument is not a locally owned nominal type; value.name() and name(value) resolve to one symbol",
        "1",
        0
    );
    ruda_checker_resolution_expect(
        "type User { id: int; }\n"
        "type Order { id: int; }\n"
        "type Item { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(item: Item): int { return string(item); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "overloaded free call has no declaration for this first-argument nominal type; value.name() and name(value) resolve to one symbol",
        "item",
        1
    );
    ruda_checker_resolution_expect(
        "fn label(value: int): int { return value; }\n"
        "type User { id: int; }\n"
        "fn label(user: User): int { return user.id; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "name is already declared in this scope",
        "label",
        1
    );
}

static void ruda_test_checker_resolution_limits(void) {
    static const char two_calls[] =
        "type User { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(user: User): int { return user.string() + string(user); }\n";
    static const char two_args[] =
        "type User { id: int; }\n"
        "fn extra(user: User, value: int): int { return user.id + value; }\n"
        "fn bad(user: User): int { return user.extra(1); }\n";
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options = ruda_check_default_options();
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, two_calls));
    options.mir_options.max_calls = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-limit-calls",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_resolution_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, two_args));
    options = ruda_check_default_options();
    options.mir_options.max_call_arguments = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-limit-args",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_resolution_fixture_dispose(&fixture);
}

static void ruda_test_checker_resolution_allocation_failures(void) {
    static const char source[] =
        "type User { id: int; }\n"
        "type Handle = User;\n"
        "enum Status { Ready, Done }\n"
        "fn string(user: Handle): int { return user.id; }\n"
        "fn string(status: Status): int {\n"
        "  return match status { case Ready => 1; case Done => 2; };\n"
        "}\n"
        "fn both(user: User, status: Status): int {\n"
        "  return user.string() + string(status);\n"
        "}\n";
    RudaCheckerResolutionFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, source));
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
            "test/resolution-alloc",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.functions.length == 3);
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
    ruda_checker_resolution_fixture_dispose(&fixture);
}

static void ruda_test_checker_resolution_engines(void) {
    static const char source[] =
        "type User { id: int; }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn main(): int {\n"
        "  const user = User { id: 21 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    RudaCheckerResolutionFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaCheckedFunction *callee = NULL;
    const RudaMirFunction *functions[2];
    RudaBytecodeProgram program;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    size_t function_index = 0;
    bool checked_initialized = false;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&native_program, 0, sizeof(native_program));
    memset(functions, 0, sizeof(functions));
    RUDA_EXPECT(ruda_checker_resolution_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/resolution-engines",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    checked_initialized = checked.initialized;
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(ruda_checker_resolution_interpret(&checked, "main") == 42);
    entry = ruda_checker_resolution_function(&checked, "main");
    callee = ruda_checker_resolution_function(&checked, "string");
    RUDA_EXPECT(entry != NULL && callee != NULL);
    if (entry == NULL || callee == NULL) {
        goto cleanup;
    }
    for (function_index = 0; function_index < checked.functions.length;
        function_index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked,
            function_index
        );
        if (function == NULL) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        functions[function_index] = &function->mir;
    }
    {
        RudaMirBytecodeLowerStatus lower_status = ruda_mir_lower_bytecode_program(
            functions,
            2,
            entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &program
        );
        RudaNativeCEmitStatus emit_status = RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry->mir.symbol;
        emit_status = ruda_native_c_emit_program(
            &native_program,
            ruda_system_allocator(),
            ruda_native_c_emit_default_options(),
            &generated
        );
        RUDA_EXPECT(lower_status == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(emit_status == RUDA_NATIVE_C_EMIT_OK);
    }

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (checked_initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_resolution_fixture_dispose(&fixture);
}

void ruda_test_checker_resolution(void) {
    ruda_test_checker_resolution_free_dot_equivalence();
    ruda_test_checker_resolution_alias_and_enum_overloads();
    ruda_test_checker_resolution_receiver_once();
    ruda_test_checker_resolution_negative_matrix();
    ruda_test_checker_resolution_limits();
    ruda_test_checker_resolution_allocation_failures();
    ruda_test_checker_resolution_engines();
}
