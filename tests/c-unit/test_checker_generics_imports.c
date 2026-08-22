#include "test_checker_imports_support.h"

static const char mmay_generic_fs_source[] =
    "public enum Error { Unavailable, }\n"
    "public extern \"c\" fn readText(path: string, limit: int): "
    "Result<string, Error>;\n";

static const char mmay_generic_fs_application[] =
    "import * as fs from \"std/fs\";\n"
    "enum ConfigError { Invalid, }\n"
    "fn configError(error: fs.Error): ConfigError {\n"
    "  return ConfigError.Invalid;\n"
    "}\n"
    "unsafe fn load(filePath: string, limit: int): Result<string, ConfigError> {\n"
    "  const value = fs.readText(filePath, limit).mapError(configError)?;\n"
    "  return Ok(value);\n"
    "}\n"
    "fn main(): int { return 0; }\n";

void mmay_test_checker_generics_imports(void) {
    const MarimayTestImportSource sources[] = {
        {"app", "app/main.my", mmay_generic_fs_application},
        {"std/fs", "fs/api.my", mmay_generic_fs_source}
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *external = NULL;
    const MarimayCheckedFunction *load = NULL;
    MarimayCheckStatus status = MMAY_CHECK_INVALID_ARGUMENT;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    status = mmay_checker_import_fixture_check(
        &fixture,
        "app",
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    );
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (status == MMAY_CHECK_OK && checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.enums.length == 2);
        MMAY_EXPECT(checked.functions.length == 4);
        external = mmay_checker_import_function(
            &checked, "readText", NULL
        );
        load = mmay_checker_import_function(&checked, "load", NULL);
        MMAY_EXPECT(external != NULL && external->is_external);
        MMAY_EXPECT(load != NULL);
        if (load != NULL) {
            MMAY_EXPECT(load->mir.calls.length == 2);
            MMAY_EXPECT(mmay_mir_verify(&load->mir) == MMAY_MIR_VERIFY_OK);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}
