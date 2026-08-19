#include "test_imported_arrays_support.h"

static const char ruda_imported_arrays_producer[] =
    "public type Cell { public mutable value: int; }\n"
    "public type Box<T> { public mutable value: T; }\n"
    "public type Cells = Array<Cell>;\n"
    "public enum Mark { Low, High(int), }\n"
    "public fn makeCells(): Cells {\n"
    "  return [Cell { value: 4 }, Cell { value: 6 }];\n"
    "}\n"
    "public fn mutateCells(values: Cells): Cells {\n"
    "  values[0].value += values[1].value;\n"
    "  values.push(values[0]);\n"
    "  return values;\n"
    "}\n"
    "public fn replaceCell(values: Cells): Cells {\n"
    "  values[1] = values[0];\n"
    "  return values;\n"
    "}\n"
    "public fn sumCells(values: Cells): int {\n"
    "  let total = 0;\n"
    "  for const value of values { total += value.value; }\n"
    "  return total;\n"
    "}\n"
    "public fn makeBoxes(): Array<Box<int>> {\n"
    "  return [Box { value: 5 }, Box { value: 7 }];\n"
    "}\n"
    "public fn mutateBoxes(values: Array<Box<int>>): Array<Box<int>> {\n"
    "  values[0].value += values[1].value;\n"
    "  return values;\n"
    "}\n"
    "public fn makeNested(): Array<Array<Cell>> {\n"
    "  return [[Cell { value: 3 }]];\n"
    "}\n"
    "public fn readNested(values: Array<Array<Cell>>): int {\n"
    "  return values[0][0].value;\n"
    "}\n"
    "public fn makeNames(): Array<string> { return [\"ada\"]; }\n"
    "public fn extendNames(values: Array<string>): Array<string> {\n"
    "  values.push(\"grace\");\n"
    "  return values;\n"
    "}\n"
    "public fn makeMarks(): Array<Mark> {\n"
    "  return [Mark.Low, Mark.High(8)];\n"
    "}\n"
    "public fn scoreMarks(values: Array<Mark>): int {\n"
    "  return match values[1] {\n"
    "    case Low => 0;\n"
    "    case High(value) => value;\n"
    "  };\n"
    "}\n";

static const char ruda_imported_arrays_relay[] =
    "import { Cells } from \"acme/store\";\n"
    "public type RelayedCells = Cells;\n"
    "public fn pass(values: RelayedCells): RelayedCells { return values; }\n";

static const char ruda_imported_arrays_application[] =
    "import {\n"
    "  Cells as ImportedCells,\n"
    "  makeCells as createCells,\n"
    "  mutateCells as amendCells,\n"
    "  sumCells,\n"
    "} from \"acme/store\";\n"
    "import * as store from \"acme/store\";\n"
    "import { pass } from \"acme/relay\";\n"
    "fn main(): int {\n"
    "  const cells: ImportedCells = amendCells(createCells());\n"
    "  const alias = pass(cells);\n"
    "  const boxes = store.mutateBoxes(store.makeBoxes());\n"
    "  const nested = store.makeNested();\n"
    "  const names = store.extendNames(store.makeNames());\n"
    "  const marks = store.makeMarks();\n"
    "  return sumCells(alias) + boxes[0].value + store.readNested(nested) + names.length() + store.scoreMarks(marks);\n"
    "}\n";

bool ruda_test_imported_arrays_init(
    RudaTestImportedArrays *test,
    RudaAllocator allocator
) {
    const RudaTestImportSource sources[] = {
        {"acme/app", "app/main.ruda", ruda_imported_arrays_application},
        {"acme/relay", "relay/api.ruda", ruda_imported_arrays_relay},
        {"acme/store", "store/api.ruda", ruda_imported_arrays_producer}
    };
    size_t index = 0;
    memset(test, 0, sizeof(*test));
    test->entry_index = SIZE_MAX;
    if (!ruda_checker_import_fixture_init(
            &test->fixture, sources, sizeof(sources) / sizeof(sources[0])
        ) || ruda_checker_import_fixture_check(
            &test->fixture, "acme/app", allocator,
            ruda_check_graph_default_options(), &test->checked
        ) != RUDA_CHECK_OK || !test->checked.initialized
        || test->checked.diagnostics.length != 0
        || test->checked.functions.length
            > RUDA_TEST_IMPORTED_ARRAY_MAX_FUNCTIONS) {
        ruda_test_imported_arrays_dispose(test);
        return false;
    }
    test->entry = ruda_checker_import_function(
        &test->checked, "main", &test->entry_index
    );
    if (test->entry == NULL) {
        ruda_test_imported_arrays_dispose(test);
        return false;
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &test->checked, index
        );
        if (function == NULL
            || ruda_mir_verify(&function->mir) != RUDA_MIR_VERIFY_OK) {
            ruda_test_imported_arrays_dispose(test);
            return false;
        }
        test->functions[index] = &function->mir;
    }
    test->initialized = true;
    return true;
}

void ruda_test_imported_arrays_dispose(RudaTestImportedArrays *test) {
    if (test == NULL) return;
    if (test->checked.initialized) {
        ruda_check_result_dispose(&test->checked);
    }
    ruda_checker_import_fixture_dispose(&test->fixture);
    memset(test, 0, sizeof(*test));
}

RudaMirBytecodeLowerStatus ruda_test_imported_arrays_lower(
    const RudaTestImportedArrays *test,
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
