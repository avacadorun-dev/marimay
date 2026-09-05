#include "test_checker_imports_support.h"
#include "mmay/generated/diagnostic_codes.h"

static const char mmay_record_import_types[] =
    "public type Item { public value: int, public label: string, }\n"
    "public type Alias = Item;\n"
    "public type Box<T> { public value: T, }\n"
    "public type IntBox = Box<int>;\n"
    "public type Empty {}\n"
    "public type Locked { secret: int, }\n"
    "type Hidden { public value: int, }\n";

/* Sweep successful construction and diagnostic paths through complete graph
 * teardown. The source/AST fixture itself uses its independent allocator. */
static void mmay_record_import_case(const char *source, const char *code) {
    const MarimayTestImportSource sources[] = {
        {"app", "app/main.my", source},
        {"model", "model/types.my", mmay_record_import_types}
    };
    MarimayCheckerImportFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, sources, 2));
    for (failure_index = 0; failure_index < 8192; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_checker_import_fixture_check(&fixture, "app",
            mmay_fail_allocator(&state), mmay_check_graph_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            for (size_t index = 0; index < checked.diagnostics.length; index += 1) {
                const MarimayCheckDiagnostic *item = mmay_check_diagnostic(&checked, index);
                if (item != NULL && (code == NULL || strcmp(item->code, code) != 0)) {
                    fprintf(stderr, "record import: expected %s, got %s at %zu\n%s\n",
                        code == NULL ? "success" : code, item->code, item->start, source);
                }
            }
            MMAY_EXPECT(checked.diagnostics.length == (code == NULL ? 0U : 1U));
            if (code != NULL) {
                const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(&checked, 0);
                MMAY_EXPECT(diagnostic != NULL);
                if (diagnostic != NULL) MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
            } else {
                size_t main_index = 0;
                MarimayMirValue value;
                const MarimayCheckedFunction *main_function =
                    mmay_checker_import_function(&checked, "main", &main_index);
                MMAY_EXPECT(main_function != NULL);
                if (main_function != NULL) {
                    /* The sweep covers checking, not allocations by this
                     * independent verifier/interpreter after checking. */
                    state.failure_index = SIZE_MAX;
                    MMAY_EXPECT(mmay_mir_verify(&main_function->mir) == MMAY_MIR_VERIFY_OK);
                    memset(&value, 0, sizeof(value));
                    MMAY_EXPECT(mmay_check_interpret_function(&checked, main_index,
                        NULL, 0, mmay_system_allocator(),
                        mmay_check_interpret_default_options(), &value) == MMAY_MIR_INTERPRET_OK);
                    MMAY_EXPECT(value.type == MMAY_TYPE_I64 && value.as.i64 == 21);
                }
            }
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_checker_record_imports(void) {
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "import { Item as Renamed, Alias } from \"model\";\n"
        "type LocalAlias = m.Item;\n"
        "fn main(): int {\n"
        " const label = \"note\";\n"
        " const a = m.Item { label, value: 1 };\n"
        " const b = Renamed { value: 2, label };\n"
        " const c = Alias { value: 3, label };\n"
        " const d = LocalAlias { value: 4, label };\n"
        " const e: m.Box<int> = m.Box { value: 5 };\n"
        " const f = m.IntBox { value: 6 };\n"
        " const empty = m.Empty {};\n"
        " if !identity.same(empty, empty) || !identity.same(e, e) { return 0; }\n"
        " if identity.same(a, b) { return 0; }\n"
        " return a.value + b.value + c.value + d.value + e.value + f.value;\n"
        "}\n", NULL);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Empty {}; const b = m.Item { value: 1, label: \"x\" };\n"
        " const same = identity.same(a, b); return 0; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Hidden { value: 1 }; return 0; }\n",
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Locked { secret: 1 }; return 0; }\n",
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Item { value: 1 }; return 0; }\n",
        MMAY_DIAGNOSTIC_MISSING_FIELD);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Item { value: 1, value: 2 }; return 0; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_FIELD);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Item { other: 1 }; return 0; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_FIELD);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a: m.Box<int> = m.Box { value: \"x\" }; return 0; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_record_import_case(
        "import * as m from \"model\";\n"
        "fn main(): int { const a = m.Box { value: 1 }; return 0; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH);
}
