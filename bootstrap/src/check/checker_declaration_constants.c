#include "checker_declaration_constants_api.h"
#include "checker_core_api.h"
#include "checker_declaration_functions_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"






 bool ruda_checker_private_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const RudaAstTree *previous_ast = NULL;
    RudaFunctionChecker function;
    RudaExpressionValue expression;
    RudaTypeId inferred_type = RUDA_TYPE_INVALID;
    RudaMirVerifyStatus verify_status;
    RudaMirInterpretOptions interpret_options;
    RudaMirInterpretStatus interpret_status;
    bool locals_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&expression, 0, sizeof(expression));
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_EVALUATED) {
        return true;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_INVALID) {
        return false;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_EVALUATING) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->constant_depth >= checker->options.max_constant_depth) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency depth limit reached",
            use_start,
            use_end
        );
        declaration->state = RUDA_CHECK_CONSTANT_INVALID;
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = RUDA_CHECK_CONSTANT_EVALUATING;
    checker->constant_depth += 1;
    if (!ruda_checker_private_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_checker_private_infer_constant_expression_type(
            checker,
            declaration->initializer,
            &inferred_type
        )) {
        goto cleanup;
    }
    if (declaration->type == RUDA_TYPE_INVALID) {
        declaration->type = inferred_type;
    } else if (declaration->type != inferred_type) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (declaration->type != RUDA_TYPE_I64
        && declaration->type != RUDA_TYPE_F64
        && declaration->type != RUDA_TYPE_BOOL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    function.checker = checker;
    function.block = RUDA_MIR_NO_BLOCK;
    function.result_type = declaration->type;
    function.diagnostic_start = checker->result->diagnostics.length;
    function.constant_evaluation = true;
    locals_initialized = ruda_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(RudaCheckLocal)
    );
    if (!locals_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!ruda_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            NULL,
            0,
            declaration->type,
            checker->options.mir_options
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    if (!ruda_checker_private_function_add_block(&function, &function.block)
        || !ruda_compile_expression(
            &function,
            declaration->initializer,
            declaration->type,
            &expression
        )) {
        goto cleanup;
    }
    if (expression.type != declaration->type) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (!ruda_checker_private_function_set_return(
            &function,
            expression.register_id,
            declaration->initializer->end,
            declaration->initializer->end
        )) {
        goto cleanup;
    }
    verify_status = ruda_mir_verify(&function.mir);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    interpret_options = ruda_mir_interpret_default_options();
    interpret_options.max_steps = checker->options.max_constant_steps;
    interpret_options.remaining_shared_steps =
        &checker->remaining_constant_steps;
    interpret_status = ruda_mir_interpret(
        &function.mir,
        NULL,
        0,
        checker->result->allocator,
        interpret_options,
        &declaration->value
    );
    if (interpret_status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (interpret_status == RUDA_MIR_INTERPRET_STEP_LIMIT) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant evaluation step limit reached",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status == RUDA_MIR_INTERPRET_OVERFLOW
        || interpret_status == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO
        || interpret_status == RUDA_MIR_INTERPRET_INVALID_SHIFT) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant expression can panic",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status != RUDA_MIR_INTERPRET_OK
        || declaration->value.type != declaration->type) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = RUDA_CHECK_CONSTANT_EVALUATED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = RUDA_CHECK_CONSTANT_INVALID;
    }
    if (function.mir_initialized) {
        ruda_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        ruda_vector_dispose(&function.locals);
    }
    checker->constant_depth -= 1;
    if (previous_ast != NULL && !ruda_checker_private_checker_set_ast(checker, previous_ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_evaluate_all_constants(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->constants.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckConstantDeclaration *declaration = ruda_vector_at(
            &checker->constants,
            index
        );
        if (declaration == NULL || !ruda_checker_private_checker_set_ast(
                checker,
                declaration->ast
            ) || !ruda_checker_private_evaluate_constant(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

 bool ruda_checker_private_publish_constants(RudaChecker *checker) {
    size_t index = 0;
    if (!ruda_vector_reserve(
            &checker->result->constants,
            checker->constants.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        RudaCheckConstantDeclaration *declaration = ruda_vector_at(
            &checker->constants,
            index
        );
        RudaCheckedConstant constant;
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_CONSTANT_EVALUATED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&constant, 0, sizeof(constant));
        constant.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        constant.symbol = declaration->symbol;
        constant.value = declaration->value;
        constant.is_public = declaration->is_public;
        if (!ruda_vector_push(&checker->result->constants, &constant)) {
            ruda_owned_bytes_dispose(&constant.name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
