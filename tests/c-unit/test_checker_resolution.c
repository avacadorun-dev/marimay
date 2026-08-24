#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/check/checker.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/mir/mir.h"
#include "mmay/native/c_emitter.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"
#include "mmay/types/type.h"

typedef struct MarimayCheckerResolutionFixture {
    MarimaySourceManager sources;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} MarimayCheckerResolutionFixture;

static bool mmay_checker_resolution_fixture_init(
    MarimayCheckerResolutionFixture *fixture,
    const char *source
) {
    const MarimaySourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(
            &fixture->sources,
            mmay_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (mmay_source_manager_add(
            &fixture->sources,
            "resolution.my",
            strlen("resolution.my"),
            source,
            strlen(source),
            &snapshot
        ) != MMAY_SOURCE_OK) {
        return false;
    }
    if (mmay_lex_snapshot(
            snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || mmay_parse_file(
            &fixture->lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || mmay_ast_project(
            &fixture->parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &fixture->ast
        ) != MMAY_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static void mmay_checker_resolution_fixture_dispose(
    MarimayCheckerResolutionFixture *fixture
) {
    if (fixture->ast_initialized) {
        mmay_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        mmay_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        mmay_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        mmay_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static const MarimayCheckedFunction *mmay_checker_resolution_function(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t name_length = name == NULL ? 0 : strlen(name);
    if (checked == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
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

static const MarimayMirCall *mmay_checker_resolution_call(
    const MarimayCheckedFunction *function,
    size_t index
) {
    if (function == NULL) {
        return NULL;
    }
    return mmay_vector_at_const(&function->mir.calls, index);
}

static void mmay_checker_resolution_expect(
    const char *source,
    const char *code,
    const char *message,
    const char *marked_text,
    size_t occurrence
) {
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    const char *marked = source;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, source));
    for (index = 0; index <= occurrence && marked != NULL; index += 1) {
        marked = strstr(marked, marked_text);
        if (index != occurrence && marked != NULL) {
            marked += strlen(marked_text);
        }
    }
    MMAY_EXPECT(marked != NULL);
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
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
            MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
            MMAY_EXPECT(diagnostic->message != NULL);
            MMAY_EXPECT(strcmp(diagnostic->message, message) == 0);
            MMAY_EXPECT(diagnostic->start == (size_t)(marked - source));
            MMAY_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);
}

static int64_t mmay_checker_resolution_interpret(
    const MarimayCheckResult *checked,
    const char *name
) {
    const MarimayCheckedFunction *function = mmay_checker_resolution_function(
        checked,
        name
    );
    MarimayMirValue result;
    size_t index = 0;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(function != NULL);
    if (function == NULL) {
        return 0;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        if (mmay_checked_function(checked, index) == function) {
            break;
        }
    }
    MMAY_EXPECT(mmay_check_interpret_function(
        checked,
        index,
        NULL,
        0,
        mmay_system_allocator(),
        mmay_check_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    return result.as.i64;
}

static void mmay_test_checker_resolution_free_dot_equivalence(void) {
    static const char source[] =
        "type User { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn both(): int {\n"
        "  const user = User { id: 7 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *callee = NULL;
    const MarimayCheckedFunction *both = NULL;
    const MarimayMirCall *dot_call = NULL;
    const MarimayMirCall *free_call = NULL;
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-equiv",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        callee = mmay_checker_resolution_function(&checked, "string");
        both = mmay_checker_resolution_function(&checked, "both");
        MMAY_EXPECT(callee != NULL && both != NULL);
        if (both != NULL) {
            MMAY_EXPECT(both->mir.calls.length == 2);
            MMAY_EXPECT(both->mir.call_arguments.length == 2);
            dot_call = mmay_checker_resolution_call(both, 0);
            free_call = mmay_checker_resolution_call(both, 1);
            MMAY_EXPECT(dot_call != NULL && free_call != NULL);
            if (dot_call != NULL && free_call != NULL && callee != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(
                    dot_call->callee,
                    free_call->callee
                ));
                MMAY_EXPECT(mmay_symbol_id_equal(
                    dot_call->callee,
                    callee->mir.symbol
                ));
            }
        }
        MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "both") == 14);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);
}

static void mmay_test_checker_resolution_alias_and_enum_overloads(void) {
    static const char alias_source[] =
        "type User { id: int, }\n"
        "type Handle = User;\n"
        "fn string(user: Handle): int { return user.id; }\n"
        "fn both(): int {\n"
        "  const user = User { id: 3 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    static const char enum_source[] =
        "type User { id: int, }\n"
        "enum Status { Ready, Done }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(status: Status): int {\n"
        "  return match status { Ready => 1, Done => 2, };\n"
        "}\n"
        "fn both(): int {\n"
        "  const user = User { id: 10 };\n"
        "  const status = Status.Ready;\n"
        "  return user.string() + string(status) + status.string() + string(user);\n"
        "}\n";
    static const char duplicate_alias[] =
        "type User { id: int, }\n"
        "type Handle = User;\n"
        "fn string(user: User): int { return 1; }\n"
        "fn string(user: Handle): int { return 2; }\n";
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *user_string = NULL;
    const MarimayCheckedFunction *status_string = NULL;
    const MarimayCheckedFunction *both = NULL;
    const MarimayMirCall *call = NULL;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, alias_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-alias",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        both = mmay_checker_resolution_function(&checked, "both");
        user_string = mmay_checker_resolution_function(&checked, "string");
        MMAY_EXPECT(both != NULL && user_string != NULL);
        if (both != NULL && user_string != NULL) {
            MMAY_EXPECT(both->mir.calls.length == 2);
            for (index = 0; index < 2; index += 1) {
                call = mmay_checker_resolution_call(both, index);
                MMAY_EXPECT(call != NULL);
                if (call != NULL) {
                    MMAY_EXPECT(mmay_symbol_id_equal(
                        call->callee,
                        user_string->mir.symbol
                    ));
                }
            }
        }
        MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "both") == 6);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, enum_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-enum",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 3);
        both = mmay_checker_resolution_function(&checked, "both");
        MMAY_EXPECT(both != NULL);
        if (both != NULL) {
            MMAY_EXPECT(both->mir.calls.length == 4);
            user_string = NULL;
            status_string = NULL;
            for (index = 0; index < checked.functions.length; index += 1) {
                const MarimayCheckedFunction *function = mmay_checked_function(
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
            MMAY_EXPECT(user_string != NULL && status_string != NULL);
            if (user_string != NULL && status_string != NULL) {
                MMAY_EXPECT(!mmay_symbol_id_equal(
                    user_string->mir.symbol,
                    status_string->mir.symbol
                ));
                call = mmay_checker_resolution_call(both, 0);
                MMAY_EXPECT(call != NULL && mmay_symbol_id_equal(
                    call->callee,
                    user_string->mir.symbol
                ));
                call = mmay_checker_resolution_call(both, 1);
                MMAY_EXPECT(call != NULL && mmay_symbol_id_equal(
                    call->callee,
                    status_string->mir.symbol
                ));
                call = mmay_checker_resolution_call(both, 2);
                MMAY_EXPECT(call != NULL && mmay_symbol_id_equal(
                    call->callee,
                    status_string->mir.symbol
                ));
                call = mmay_checker_resolution_call(both, 3);
                MMAY_EXPECT(call != NULL && mmay_symbol_id_equal(
                    call->callee,
                    user_string->mir.symbol
                ));
            }
        }
        MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "both") == 22);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);

    mmay_checker_resolution_expect(
        duplicate_alias,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "name is already declared in this scope",
        "string",
        1
    );
}

static void mmay_test_checker_resolution_receiver_once(void) {
    static const char source[] =
        "type Box { mutable value: int, }\n"
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
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-order",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "dotOrder")
            == 1212);
        MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "freeOrder")
            == 1212);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);
}

static void mmay_test_checker_resolution_negative_matrix(void) {
    mmay_checker_resolution_expect(
        "fn bad(): int { return missing(1); }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "unknown function name",
        "missing",
        0
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "fn bad(user: User): int { return user.missing(); }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "unknown function name",
        "missing",
        0
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "fn bad(): int { return User(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "called name is not a function",
        "User",
        1
    );
    mmay_checker_resolution_expect(
        "fn label(value: int): int { return value; }\n"
        "fn bad(value: int): int { return value.label(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "function is not dot-call eligible because its first parameter is not an exact locally owned nominal type",
        "label",
        1
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(): int { return 1.string(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "dot-call receiver is not a locally owned nominal type",
        "1",
        0
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "type Order { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(order: Order): int { return order.string(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "an eligible function with this name exists for a different locally owned nominal type",
        "order",
        1
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "type Order { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(): int { return string(); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "overloaded free call requires a first argument of a locally owned nominal type; value.name() and name(value) resolve to one symbol",
        "()",
        1
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "type Order { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(): int { return string(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "overloaded free call first argument is not a locally owned nominal type; value.name() and name(value) resolve to one symbol",
        "1",
        0
    );
    mmay_checker_resolution_expect(
        "type User { id: int, }\n"
        "type Order { id: int, }\n"
        "type Item { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn string(order: Order): int { return order.id; }\n"
        "fn bad(item: Item): int { return string(item); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "overloaded free call has no declaration for this first-argument nominal type; value.name() and name(value) resolve to one symbol",
        "item",
        1
    );
    mmay_checker_resolution_expect(
        "fn label(value: int): int { return value; }\n"
        "type User { id: int, }\n"
        "fn label(user: User): int { return user.id; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "name is already declared in this scope",
        "label",
        1
    );
}

static void mmay_test_checker_resolution_limits(void) {
    static const char two_calls[] =
        "type User { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn bad(user: User): int { return user.string() + string(user); }\n";
    static const char two_args[] =
        "type User { id: int, }\n"
        "fn extra(user: User, value: int): int { return user.id + value; }\n"
        "fn bad(user: User): int { return user.extra(1); }\n";
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options = mmay_check_default_options();
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, two_calls));
    options.mir_options.max_calls = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-limit-calls",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_resolution_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, two_args));
    options = mmay_check_default_options();
    options.mir_options.max_call_arguments = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-limit-args",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_resolution_fixture_dispose(&fixture);
}

static void mmay_test_checker_resolution_allocation_failures(void) {
    static const char source[] =
        "type User { id: int, }\n"
        "type Handle = User;\n"
        "enum Status { Ready, Done }\n"
        "fn string(user: Handle): int { return user.id; }\n"
        "fn string(status: Status): int {\n"
        "  return match status { Ready => 1, Done => 2, };\n"
        "}\n"
        "fn both(user: User, status: Status): int {\n"
        "  return user.string() + string(status);\n"
        "}\n";
    MarimayCheckerResolutionFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, source));
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
            "test/resolution-alloc",
            allocator,
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.functions.length == 3);
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
    mmay_checker_resolution_fixture_dispose(&fixture);
}

static void mmay_test_checker_resolution_engines(void) {
    static const char source[] =
        "type User { id: int, }\n"
        "fn string(user: User): int { return user.id; }\n"
        "fn main(): int {\n"
        "  const user = User { id: 21 };\n"
        "  return user.string() + string(user);\n"
        "}\n";
    MarimayCheckerResolutionFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayCheckedFunction *callee = NULL;
    const MarimayMirFunction *functions[2];
    MarimayBytecodeProgram program;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    size_t function_index = 0;
    bool checked_initialized = false;
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    memset(&native_program, 0, sizeof(native_program));
    memset(functions, 0, sizeof(functions));
    MMAY_EXPECT(mmay_checker_resolution_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/resolution-engines",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    checked_initialized = checked.initialized;
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(mmay_checker_resolution_interpret(&checked, "main") == 42);
    entry = mmay_checker_resolution_function(&checked, "main");
    callee = mmay_checker_resolution_function(&checked, "string");
    MMAY_EXPECT(entry != NULL && callee != NULL);
    if (entry == NULL || callee == NULL) {
        goto cleanup;
    }
    for (function_index = 0; function_index < checked.functions.length;
        function_index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked,
            function_index
        );
        if (function == NULL) {
            MMAY_EXPECT(false);
            goto cleanup;
        }
        functions[function_index] = &function->mir;
    }
    {
        MarimayMirBytecodeLowerStatus lower_status = mmay_mir_lower_bytecode_program(
            functions,
            2,
            entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &program
        );
        MarimayNativeCEmitStatus emit_status = MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
        native_program.functions = functions;
        native_program.function_count = 2;
        native_program.entry_symbol = entry->mir.symbol;
        emit_status = mmay_native_c_emit_program(
            &native_program,
            mmay_system_allocator(),
            mmay_native_c_emit_default_options(),
            &generated
        );
        MMAY_EXPECT(lower_status == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(emit_status == MMAY_NATIVE_C_EMIT_OK);
    }

cleanup:
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_program_dispose(&program);
    if (checked_initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_resolution_fixture_dispose(&fixture);
}

void mmay_test_checker_resolution(void) {
    mmay_test_checker_resolution_free_dot_equivalence();
    mmay_test_checker_resolution_alias_and_enum_overloads();
    mmay_test_checker_resolution_receiver_once();
    mmay_test_checker_resolution_negative_matrix();
    mmay_test_checker_resolution_limits();
    mmay_test_checker_resolution_allocation_failures();
    mmay_test_checker_resolution_engines();
}
