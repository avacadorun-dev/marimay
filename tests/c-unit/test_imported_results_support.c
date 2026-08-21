#include "test_imported_results_support.h"

static const char ruda_imported_results_producer[] =
    "public enum Choice<T> { Empty, Value(T), }\n"
    "public enum Problem { Missing, Rejected(int), }\n"
    "public fn choice(value: int): Result<Choice<[]int>, Problem> {\n"
    "  return Ok<Choice<[]int>, Problem>(Choice.Value([value]));\n"
    "}\n"
    "public fn noChoice(): Result<Choice<[]int>, Problem> {\n"
    "  return Ok(Choice.Empty);\n"
    "}\n"
    "public fn failure(): Result<Choice<[]int>, Problem> {\n"
    "  return Err<Choice<[]int>, Problem>(Problem.Rejected(9));\n"
    "}\n"
    "public fn nested(value: int): Result<Option<[]int>, Problem> {\n"
    "  return Ok<Option<[]int>, Problem>(Some([value]));\n"
    "}\n";

static const char ruda_imported_results_relay[] =
    "import { Choice } from \"acme/results\";\n"
    "public type RelayedChoice = Choice<[]int>;\n"
    "public fn relay(value: RelayedChoice): RelayedChoice { return value; }\n";

static const char ruda_imported_results_application[] =
    "import { Choice, Problem, choice, failure, nested, noChoice } from \"acme/results\";\n"
    "import * as results from \"acme/results\";\n"
    "import { RelayedChoice, relay } from \"acme/relay\";\n"
    "enum AppError { Producer(Problem), }\n"
    "fn appError(error: Problem): AppError { return AppError.Producer(error); }\n"
    "fn score(value: Choice<[]int>): int {\n"
    "  return match value { case Empty => 0; case Value(values) => values[0] + values.length(); };\n"
    "}\n"
    "fn optionScore(value: Option<[]int>): int {\n"
    "  return match value { case None => 0; case Some(values) => values[0]; };\n"
    "}\n"
    "fn propagated(): Result<int, AppError> {\n"
    "  const ignored = failure().mapError(appError)?;\n"
    "  return Ok(score(ignored));\n"
    "}\n"
    "fn direct(): Result<int, AppError> {\n"
    "  const first = choice(39).mapError(appError)?;\n"
    "  const empty = noChoice().mapError(appError)?;\n"
    "  const nestedValue = nested(7).mapError(appError)?;\n"
    "  const explicit: Option<[]int> = Some<[]int>([5]);\n"
    "  const inferred: Option<[]int> = Some([6]);\n"
    "  const failureScore = match propagated() {\n"
    "    case Ok(value) => value;\n"
    "    case Err(Producer(Rejected(value))) => value;\n"
    "    case Err(_) => 0;\n"
    "  };\n"
    "  return Ok(score(first) + score(empty) + optionScore(nestedValue) + optionScore(explicit) + optionScore(inferred) + failureScore);\n"
    "}\n"
    "fn namespaceScore(): int {\n"
    "  const value: results.Choice<[]int> = results.Choice.Value([20]);\n"
    "  return match value {\n"
    "    case results.Empty => 0;\n"
    "    case results.Value(values) => values[0] + values.length();\n"
    "  };\n"
    "}\n"
    "fn relayScore(): int {\n"
    "  const value: RelayedChoice = relay(RelayedChoice.Value([30]));\n"
    "  return match value { case Empty => 0; case Value(values) => values[0] + values.length(); };\n"
    "}\n"
    "fn main(): int {\n"
    "  return match direct() {\n"
    "    case Ok(value) => value + namespaceScore() + relayScore();\n"
    "    case Err(_) => -1;\n"
    "  };\n"
    "}\n";

bool ruda_test_imported_results_init(
    RudaTestImportedResults *test,
    RudaAllocator allocator
) {
    const RudaTestImportSource sources[] = {
        {"acme/app", "app/main.ruda", ruda_imported_results_application},
        {"acme/relay", "relay/api.ruda", ruda_imported_results_relay},
        {"acme/results", "results/api.ruda", ruda_imported_results_producer}
    };
    size_t index = 0;
    memset(test, 0, sizeof(*test));
    test->entry_index = SIZE_MAX;
    if (!ruda_checker_import_fixture_init(
            &test->fixture, sources, sizeof(sources) / sizeof(sources[0])
        ) || ruda_checker_import_fixture_check(
            &test->fixture,
            "acme/app",
            allocator,
            ruda_check_graph_default_options(),
            &test->checked
        ) != RUDA_CHECK_OK || !test->checked.initialized
        || test->checked.diagnostics.length != 0
        || test->checked.functions.length
            > RUDA_TEST_IMPORTED_RESULTS_MAX_FUNCTIONS) {
        ruda_test_imported_results_dispose(test);
        return false;
    }
    test->entry = ruda_checker_import_function(
        &test->checked, "main", &test->entry_index
    );
    if (test->entry == NULL) {
        ruda_test_imported_results_dispose(test);
        return false;
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &test->checked, index
        );
        if (function == NULL
            || ruda_mir_verify(&function->mir) != RUDA_MIR_VERIFY_OK) {
            ruda_test_imported_results_dispose(test);
            return false;
        }
        test->functions[index] = &function->mir;
    }
    test->initialized = true;
    return true;
}

void ruda_test_imported_results_dispose(RudaTestImportedResults *test) {
    if (test == NULL) {
        return;
    }
    if (test->checked.initialized) {
        ruda_check_result_dispose(&test->checked);
    }
    ruda_checker_import_fixture_dispose(&test->fixture);
    memset(test, 0, sizeof(*test));
}

RudaMirBytecodeLowerStatus ruda_test_imported_results_lower(
    const RudaTestImportedResults *test,
    RudaAllocator allocator,
    RudaBytecodeProgram *program
) {
    if (test == NULL || !test->initialized || test->entry == NULL) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    return ruda_mir_lower_bytecode_program(
        test->functions,
        test->checked.functions.length,
        test->entry->mir.symbol,
        allocator,
        ruda_mir_bytecode_program_lower_default_options(),
        program
    );
}
