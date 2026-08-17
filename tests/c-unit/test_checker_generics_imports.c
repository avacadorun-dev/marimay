#include "test_checker_imports_support.h"

static const char ruda_generic_fs_source[] =
    "public enum Error { Unavailable, }\n"
    "public extern \"c\" fn readText(path: string, limit: int): "
    "Result<string, Error>;\n";

static const char ruda_generic_fs_application[] =
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

void ruda_test_checker_generics_imports(void) {
    const RudaTestImportSource sources[] = {
        {"app", "app/main.ruda", ruda_generic_fs_application},
        {"std/fs", "fs/api.ruda", ruda_generic_fs_source}
    };
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *external = NULL;
    const RudaCheckedFunction *load = NULL;
    RudaCheckStatus status = RUDA_CHECK_INVALID_ARGUMENT;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    status = ruda_checker_import_fixture_check(
        &fixture,
        "app",
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    );
    RUDA_EXPECT(status == RUDA_CHECK_OK);
    if (status == RUDA_CHECK_OK && checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 2);
        RUDA_EXPECT(checked.functions.length == 4);
        external = ruda_checker_import_function(
            &checked, "readText", NULL
        );
        load = ruda_checker_import_function(&checked, "load", NULL);
        RUDA_EXPECT(external != NULL && external->is_external);
        RUDA_EXPECT(load != NULL);
        if (load != NULL) {
            RUDA_EXPECT(load->mir.calls.length == 2);
            RUDA_EXPECT(ruda_mir_verify(&load->mir) == RUDA_MIR_VERIFY_OK);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}
