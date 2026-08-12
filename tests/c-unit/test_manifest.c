#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/manifest/manifest.h"
#include "ruda/source/source_manager.h"

typedef struct RudaManifestFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaManifestParseResult parsed;
} RudaManifestFixture;

static bool ruda_manifest_fixture_init(
    RudaManifestFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length,
    RudaManifestKind kind,
    RudaManifestParseOptions options,
    RudaAllocator allocator,
    RudaManifestParseStatus *status
) {
    memset(fixture, 0, sizeof(*fixture));
    *status = RUDA_MANIFEST_PARSE_INVALID_ARGUMENT;
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())
        || ruda_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            bytes,
            length,
            &fixture->snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    *status = ruda_manifest_parse(
        fixture->snapshot,
        kind,
        allocator,
        options,
        &fixture->parsed
    );
    return true;
}

static void ruda_manifest_fixture_dispose(RudaManifestFixture *fixture) {
    ruda_manifest_parse_result_dispose(&fixture->parsed);
    ruda_source_manager_dispose(&fixture->sources);
}

static bool ruda_manifest_has_diagnostic(
    const RudaManifestParseResult *parsed,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < parsed->diagnostics.length; index += 1) {
        const RudaManifestDiagnostic *diagnostic = ruda_manifest_diagnostic(
            parsed,
            index
        );
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

static void ruda_test_manifest_module_round_trip(void) {
    static const char source[] =
        "capability (\r\n"
        "\t\"net.listen\" \"0.0.0.0:8080\"\r\n"
        "  \"env.read\" \"DATABASE_URL\"\r\n"
        ")\r\n"
        "language \"\\u0030.1\"\r\n"
        "module \"example.com/acme/orders\"\r\n"
        "require (\r\n"
        "  \"z.example/pkg\" \"~2.0\"\r\n"
        "  \"a.example/pkg\" \"^1.0\"\r\n"
        ")\r\n"
        "vendor.note \"\\uD83D\\uDE00\"\r\n";
    static const char expected[] =
        "module \"example.com/acme/orders\"\n"
        "\n"
        "language \"0.1\"\n"
        "\n"
        "require (\n"
        "  \"a.example/pkg\" \"^1.0\"\n"
        "  \"z.example/pkg\" \"~2.0\"\n"
        ")\n"
        "\n"
        "capability (\n"
        "  \"env.read\" \"DATABASE_URL\"\n"
        "  \"net.listen\" \"0.0.0.0:8080\"\n"
        ")\n"
        "\n"
        "vendor.note \"😀\"\n";
    RudaManifestFixture fixture;
    RudaManifestFixture formatted_fixture;
    RudaManifestParseStatus parse_status;
    RudaManifestFormatResult first;
    RudaManifestFormatResult second;
    RUDA_EXPECT(ruda_manifest_fixture_init(
        &fixture,
        "ruda.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        RUDA_MANIFEST_MODULE,
        ruda_manifest_parse_default_options(),
        ruda_system_allocator(),
        &parse_status
    ));
    RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.entries.length == 7);
    RUDA_EXPECT(fixture.parsed.has_require_group);
    RUDA_EXPECT(fixture.parsed.has_capability_group);
    RUDA_EXPECT(ruda_manifest_format(
        &fixture.parsed,
        ruda_system_allocator(),
        ruda_manifest_format_default_options(),
        &first
    ) == RUDA_MANIFEST_FORMAT_OK);
    if (!first.initialized) {
        ruda_manifest_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(first.changed);
    RUDA_EXPECT(first.output.length == sizeof(expected) - 1);
    RUDA_EXPECT(first.output.length != sizeof(expected) - 1
        || memcmp(first.output.data, expected, sizeof(expected) - 1) == 0);

    RUDA_EXPECT(ruda_manifest_fixture_init(
        &formatted_fixture,
        "ruda.mod",
        first.output.data,
        first.output.length,
        RUDA_MANIFEST_MODULE,
        ruda_manifest_parse_default_options(),
        ruda_system_allocator(),
        &parse_status
    ));
    RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
    RUDA_EXPECT(formatted_fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_manifest_format(
        &formatted_fixture.parsed,
        ruda_system_allocator(),
        ruda_manifest_format_default_options(),
        &second
    ) == RUDA_MANIFEST_FORMAT_OK);
    if (second.initialized) {
        RUDA_EXPECT(!second.changed);
        RUDA_EXPECT(second.output.length == first.output.length);
        RUDA_EXPECT(second.output.length != first.output.length
            || memcmp(
                second.output.data,
                first.output.data,
                first.output.length
            ) == 0);
        ruda_manifest_format_result_dispose(&second);
    }
    ruda_manifest_fixture_dispose(&formatted_fixture);
    ruda_manifest_format_result_dispose(&first);
    ruda_manifest_fixture_dispose(&fixture);
}

static void ruda_test_manifest_workspace_round_trip(void) {
    static const char source[] =
        "use \"b/./c\"\r\n"
        "use \"a/x/../y\"\r\n";
    static const char expected[] =
        "use \"a/y\"\n"
        "use \"b/c\"\n";
    RudaManifestFixture fixture;
    RudaManifestParseStatus parse_status;
    RudaManifestFormatResult result;
    RUDA_EXPECT(ruda_manifest_fixture_init(
        &fixture,
        "ruda.work",
        (const unsigned char *)source,
        sizeof(source) - 1,
        RUDA_MANIFEST_WORKSPACE,
        ruda_manifest_parse_default_options(),
        ruda_system_allocator(),
        &parse_status
    ));
    RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_manifest_format(
        &fixture.parsed,
        ruda_system_allocator(),
        ruda_manifest_format_default_options(),
        &result
    ) == RUDA_MANIFEST_FORMAT_OK);
    if (result.initialized) {
        RUDA_EXPECT(result.output.length == sizeof(expected) - 1);
        RUDA_EXPECT(result.output.length != sizeof(expected) - 1
            || memcmp(result.output.data, expected, sizeof(expected) - 1) == 0);
        ruda_manifest_format_result_dispose(&result);
    }
    ruda_manifest_fixture_dispose(&fixture);
}

static void ruda_test_manifest_invalid_inputs(void) {
    static const struct {
        const unsigned char *bytes;
        size_t length;
        RudaManifestKind kind;
        const char *code;
    } cases[] = {
        {
            (const unsigned char *)"unknown \"value\"\n",
            sizeof("unknown \"value\"\n") - 1,
            RUDA_MANIFEST_MODULE,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX
        },
        {
            (const unsigned char *)"module \"\\q\"\n",
            sizeof("module \"\\q\"\n") - 1,
            RUDA_MANIFEST_MODULE,
            RUDA_DIAGNOSTIC_MANIFEST_STRING
        },
        {
            (const unsigned char *)"module \"\\uD800\"\n",
            sizeof("module \"\\uD800\"\n") - 1,
            RUDA_MANIFEST_MODULE,
            RUDA_DIAGNOSTIC_MANIFEST_STRING
        },
        {
            (const unsigned char *)"require (\n)\nrequire (\n)\n",
            sizeof("require (\n)\nrequire (\n)\n") - 1,
            RUDA_MANIFEST_MODULE,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX
        },
        {
            (const unsigned char *)"use \"../outside\"\n",
            sizeof("use \"../outside\"\n") - 1,
            RUDA_MANIFEST_WORKSPACE,
            RUDA_DIAGNOSTIC_WORKSPACE_PATH
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaManifestFixture fixture;
        RudaManifestParseStatus parse_status;
        RudaManifestFormatResult formatted;
        RUDA_EXPECT(ruda_manifest_fixture_init(
            &fixture,
            cases[index].kind == RUDA_MANIFEST_MODULE ? "ruda.mod" : "ruda.work",
            cases[index].bytes,
            cases[index].length,
            cases[index].kind,
            ruda_manifest_parse_default_options(),
            ruda_system_allocator(),
            &parse_status
        ));
        RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
        RUDA_EXPECT(ruda_manifest_has_diagnostic(&fixture.parsed, cases[index].code));
        RUDA_EXPECT(ruda_manifest_format(
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_manifest_format_default_options(),
            &formatted
        ) == RUDA_MANIFEST_FORMAT_SOURCE_INVALID);
        RUDA_EXPECT(!formatted.initialized);
        ruda_manifest_fixture_dispose(&fixture);
    }

    {
        static const unsigned char bom[] = {
            0xefU, 0xbbU, 0xbfU, 'm', 'o', 'd', 'u', 'l', 'e', ' ', '"', 'x', '"'
        };
        RudaManifestFixture fixture;
        RudaManifestParseStatus parse_status;
        RUDA_EXPECT(ruda_manifest_fixture_init(
            &fixture,
            "ruda.mod",
            bom,
            sizeof(bom),
            RUDA_MANIFEST_MODULE,
            ruda_manifest_parse_default_options(),
            ruda_system_allocator(),
            &parse_status
        ));
        RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
        RUDA_EXPECT(ruda_manifest_has_diagnostic(
            &fixture.parsed,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX
        ));
        ruda_manifest_fixture_dispose(&fixture);
    }

    {
        static const unsigned char invalid_utf8[] = {
            'm', 'o', 'd', 'u', 'l', 'e', ' ', '"', 0xffU, '"'
        };
        RudaManifestFixture fixture;
        RudaManifestParseStatus parse_status;
        RUDA_EXPECT(ruda_manifest_fixture_init(
            &fixture,
            "ruda.mod",
            invalid_utf8,
            sizeof(invalid_utf8),
            RUDA_MANIFEST_MODULE,
            ruda_manifest_parse_default_options(),
            ruda_system_allocator(),
            &parse_status
        ));
        RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
        RUDA_EXPECT(ruda_manifest_has_diagnostic(
            &fixture.parsed,
            RUDA_DIAGNOSTIC_INVALID_UTF8
        ));
        ruda_manifest_fixture_dispose(&fixture);
    }
}

static void ruda_test_manifest_limits_and_allocation(void) {
    static const char source[] =
        "module \"example/a\"\n"
        "language \"0.1\"\n"
        "require (\n"
        "  \"a\" \"1\"\n"
        ")\n";
    RudaManifestFixture fixture;
    RudaManifestParseStatus parse_status;
    RudaManifestParseOptions parse_options = ruda_manifest_parse_default_options();
    RudaManifestFormatOptions format_options;
    RudaManifestFormatResult formatted;
    size_t failure_index = 0;
    bool reached_success = false;

    parse_options.max_entries = 1;
    RUDA_EXPECT(ruda_manifest_fixture_init(
        &fixture,
        "ruda.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        RUDA_MANIFEST_MODULE,
        parse_options,
        ruda_system_allocator(),
        &parse_status
    ));
    RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_LIMIT_REACHED);
    RUDA_EXPECT(fixture.parsed.initialized);
    ruda_manifest_fixture_dispose(&fixture);

    parse_options = ruda_manifest_parse_default_options();
    RUDA_EXPECT(ruda_manifest_fixture_init(
        &fixture,
        "ruda.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        RUDA_MANIFEST_MODULE,
        parse_options,
        ruda_system_allocator(),
        &parse_status
    ));
    RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_OK);
    format_options = ruda_manifest_format_default_options();
    format_options.max_decoded_bytes = 1;
    RUDA_EXPECT(ruda_manifest_format(
        &fixture.parsed,
        ruda_system_allocator(),
        format_options,
        &formatted
    ) == RUDA_MANIFEST_FORMAT_LIMIT_REACHED);
    format_options = ruda_manifest_format_default_options();
    format_options.max_output_bytes = 1;
    RUDA_EXPECT(ruda_manifest_format(
        &fixture.parsed,
        ruda_system_allocator(),
        format_options,
        &formatted
    ) == RUDA_MANIFEST_FORMAT_LIMIT_REACHED);

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManifestFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_manifest_format(
            &fixture.parsed,
            allocator,
            ruda_manifest_format_default_options(),
            &formatted
        );
        if (status == RUDA_MANIFEST_FORMAT_OK) {
            reached_success = true;
            ruda_manifest_format_result_dispose(&formatted);
        } else {
            RUDA_EXPECT(status == RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!formatted.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_manifest_fixture_dispose(&fixture);

    reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManifestFixture failing;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_manifest_fixture_init(
            &failing,
            "ruda.mod",
            (const unsigned char *)source,
            sizeof(source) - 1,
            RUDA_MANIFEST_MODULE,
            ruda_manifest_parse_default_options(),
            allocator,
            &parse_status
        ));
        if (parse_status == RUDA_MANIFEST_PARSE_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(parse_status == RUDA_MANIFEST_PARSE_ALLOCATION_FAILED);
            RUDA_EXPECT(!failing.parsed.initialized);
        }
        ruda_manifest_fixture_dispose(&failing);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_manifest(void) {
    ruda_test_manifest_module_round_trip();
    ruda_test_manifest_workspace_round_trip();
    ruda_test_manifest_invalid_inputs();
    ruda_test_manifest_limits_and_allocation();
}
