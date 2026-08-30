#include "checker_declaration_constants_api.h"
#include "checker_core_api.h"
#include "checker_declaration_functions_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"







 bool mmay_checker_private_evaluate_constant(
    MarimayChecker *checker,
    MarimayCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const MarimayAstTree *previous_ast = NULL;
    MarimayFunctionChecker function;
    MarimayExpressionValue expression;
    MarimayTypeId inferred_type = MMAY_TYPE_INVALID;
    MarimayMirVerifyStatus verify_status;
    MarimayMirInterpretOptions interpret_options;
    MarimayMirInterpretStatus interpret_status;
    bool locals_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&expression, 0, sizeof(expression));
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == MMAY_CHECK_CONSTANT_EVALUATED) {
        return true;
    }
    if (declaration->state == MMAY_CHECK_CONSTANT_INVALID) {
        return false;
    }
    if (declaration->state == MMAY_CHECK_CONSTANT_EVALUATING) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->constant_depth >= checker->options.max_constant_depth) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency depth limit reached",
            use_start,
            use_end
        );
        declaration->state = MMAY_CHECK_CONSTANT_INVALID;
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = MMAY_CHECK_CONSTANT_EVALUATING;
    checker->constant_depth += 1;
    if (!mmay_checker_private_checker_set_ast(checker, declaration->ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!mmay_checker_private_infer_constant_expression_type(
            checker,
            declaration->initializer,
            &inferred_type
        )) {
        goto cleanup;
    }
    if (declaration->type == MMAY_TYPE_INVALID) {
        declaration->type = inferred_type;
    } else if (declaration->type != inferred_type
        && !(mmay_checker_private_type_is_integer(declaration->type)
            && inferred_type == MMAY_TYPE_I64)) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (declaration->type != MMAY_TYPE_I64
        && !mmay_checker_private_type_is_integer(declaration->type)
        && declaration->type != MMAY_TYPE_F64
        && declaration->type != MMAY_TYPE_BOOL) {
        mmay_checker_fail_incomplete(
            checker,
            declaration->initializer,
            "constant evaluation for this value type"
        );
        goto cleanup;
    }
    function.checker = checker;
    function.block = MMAY_MIR_NO_BLOCK;
    function.result_type = declaration->type;
    function.diagnostic_start = checker->result->diagnostics.length;
    function.constant_evaluation = true;
    locals_initialized = mmay_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(MarimayCheckLocal)
    );
    if (!locals_initialized) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!mmay_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            NULL,
            0,
            declaration->type,
            checker->options.mir_options
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    if (!mmay_checker_private_function_add_block(&function, &function.block)
        || !mmay_compile_expression(
            &function,
            declaration->initializer,
            declaration->type,
            &expression
        )) {
        goto cleanup;
    }
    if (expression.type != declaration->type) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (!mmay_checker_private_function_set_return(
            &function,
            expression.register_id,
            declaration->initializer->end,
            declaration->initializer->end
        )) {
        goto cleanup;
    }
    verify_status = mmay_mir_verify(&function.mir);
    if (verify_status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == MMAY_MIR_VERIFY_LIMIT_REACHED) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    interpret_options = mmay_mir_interpret_default_options();
    interpret_options.max_steps = checker->options.max_constant_steps;
    interpret_options.remaining_shared_steps =
        &checker->remaining_constant_steps;
    interpret_status = mmay_mir_interpret(
        &function.mir,
        NULL,
        0,
        checker->result->allocator,
        interpret_options,
        &declaration->value
    );
    if (interpret_status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (interpret_status == MMAY_MIR_INTERPRET_STEP_LIMIT) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant evaluation step limit reached",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status == MMAY_MIR_INTERPRET_OVERFLOW
        || interpret_status == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO
        || interpret_status == MMAY_MIR_INTERPRET_INVALID_SHIFT) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant expression can panic",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status != MMAY_MIR_INTERPRET_OK
        || declaration->value.type != declaration->type) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = MMAY_CHECK_CONSTANT_EVALUATED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = MMAY_CHECK_CONSTANT_INVALID;
    }
    if (function.mir_initialized) {
        mmay_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        mmay_vector_dispose(&function.locals);
    }
    checker->constant_depth -= 1;
    if (previous_ast != NULL && !mmay_checker_private_checker_set_ast(checker, previous_ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_evaluate_all_constants(MarimayChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->constants.length
        && !mmay_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        MarimayCheckConstantDeclaration *declaration = mmay_vector_at(
            &checker->constants,
            index
        );
        if (declaration == NULL || !mmay_checker_private_checker_set_ast(
                checker,
                declaration->ast
            ) || !mmay_checker_private_evaluate_constant(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!mmay_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !mmay_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

 bool mmay_checker_private_publish_constants(MarimayChecker *checker) {
    size_t index = 0;
    if (!mmay_vector_reserve(
            &checker->result->constants,
            checker->constants.length
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        MarimayCheckConstantDeclaration *declaration = mmay_vector_at(
            &checker->constants,
            index
        );
        MarimayCheckedConstant constant;
        if (declaration == NULL
            || declaration->state != MMAY_CHECK_CONSTANT_EVALUATED) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&constant, 0, sizeof(constant));
        constant.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        constant.symbol = declaration->symbol;
        constant.value = declaration->value;
        constant.is_public = declaration->is_public;
        if (!mmay_vector_push(&checker->result->constants, &constant)) {
            mmay_owned_bytes_dispose(&constant.name);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
