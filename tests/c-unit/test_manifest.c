#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/manifest/manifest.h"
#include "mmay/source/source_manager.h"

typedef struct MarimayManifestFixture {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot;
    MarimayManifestParseResult parsed;
} MarimayManifestFixture;

static bool mmay_manifest_fixture_init(
    MarimayManifestFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length,
    MarimayManifestKind kind,
    MarimayManifestParseOptions options,
    MarimayAllocator allocator,
    MarimayManifestParseStatus *status
) {
    memset(fixture, 0, sizeof(*fixture));
    *status = MMAY_MANIFEST_PARSE_INVALID_ARGUMENT;
    if (!mmay_source_manager_init(&fixture->sources, mmay_system_allocator())
        || mmay_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            bytes,
            length,
            &fixture->snapshot
        ) != MMAY_SOURCE_OK) {
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    *status = mmay_manifest_parse(
        fixture->snapshot,
        kind,
        allocator,
        options,
        &fixture->parsed
    );
    return true;
}

static void mmay_manifest_fixture_dispose(MarimayManifestFixture *fixture) {
    mmay_manifest_parse_result_dispose(&fixture->parsed);
    mmay_source_manager_dispose(&fixture->sources);
}

static bool mmay_manifest_has_diagnostic(
    const MarimayManifestParseResult *parsed,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < parsed->diagnostics.length; index += 1) {
        const MarimayManifestDiagnostic *diagnostic = mmay_manifest_diagnostic(
            parsed,
            index
        );
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

static void mmay_test_manifest_module_round_trip(void) {
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
    MarimayManifestFixture fixture;
    MarimayManifestFixture formatted_fixture;
    MarimayManifestParseStatus parse_status;
    MarimayManifestFormatResult first;
    MarimayManifestFormatResult second;
    MMAY_EXPECT(mmay_manifest_fixture_init(
        &fixture,
        "mmay.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        MMAY_MANIFEST_MODULE,
        mmay_manifest_parse_default_options(),
        mmay_system_allocator(),
        &parse_status
    ));
    MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.entries.length == 7);
    MMAY_EXPECT(fixture.parsed.has_require_group);
    MMAY_EXPECT(fixture.parsed.has_capability_group);
    MMAY_EXPECT(mmay_manifest_format(
        &fixture.parsed,
        mmay_system_allocator(),
        mmay_manifest_format_default_options(),
        &first
    ) == MMAY_MANIFEST_FORMAT_OK);
    if (!first.initialized) {
        mmay_manifest_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(first.changed);
    MMAY_EXPECT(first.output.length == sizeof(expected) - 1);
    MMAY_EXPECT(first.output.length != sizeof(expected) - 1
        || memcmp(first.output.data, expected, sizeof(expected) - 1) == 0);

    MMAY_EXPECT(mmay_manifest_fixture_init(
        &formatted_fixture,
        "mmay.mod",
        first.output.data,
        first.output.length,
        MMAY_MANIFEST_MODULE,
        mmay_manifest_parse_default_options(),
        mmay_system_allocator(),
        &parse_status
    ));
    MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
    MMAY_EXPECT(formatted_fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_manifest_format(
        &formatted_fixture.parsed,
        mmay_system_allocator(),
        mmay_manifest_format_default_options(),
        &second
    ) == MMAY_MANIFEST_FORMAT_OK);
    if (second.initialized) {
        MMAY_EXPECT(!second.changed);
        MMAY_EXPECT(second.output.length == first.output.length);
        MMAY_EXPECT(second.output.length != first.output.length
            || memcmp(
                second.output.data,
                first.output.data,
                first.output.length
            ) == 0);
        mmay_manifest_format_result_dispose(&second);
    }
    mmay_manifest_fixture_dispose(&formatted_fixture);
    mmay_manifest_format_result_dispose(&first);
    mmay_manifest_fixture_dispose(&fixture);
}

static void mmay_test_manifest_workspace_round_trip(void) {
    static const char source[] =
        "use \"b/./c\"\r\n"
        "use \"a/x/../y\"\r\n";
    static const char expected[] =
        "use \"a/y\"\n"
        "use \"b/c\"\n";
    MarimayManifestFixture fixture;
    MarimayManifestParseStatus parse_status;
    MarimayManifestFormatResult result;
    MMAY_EXPECT(mmay_manifest_fixture_init(
        &fixture,
        "mmay.work",
        (const unsigned char *)source,
        sizeof(source) - 1,
        MMAY_MANIFEST_WORKSPACE,
        mmay_manifest_parse_default_options(),
        mmay_system_allocator(),
        &parse_status
    ));
    MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_manifest_format(
        &fixture.parsed,
        mmay_system_allocator(),
        mmay_manifest_format_default_options(),
        &result
    ) == MMAY_MANIFEST_FORMAT_OK);
    if (result.initialized) {
        MMAY_EXPECT(result.output.length == sizeof(expected) - 1);
        MMAY_EXPECT(result.output.length != sizeof(expected) - 1
            || memcmp(result.output.data, expected, sizeof(expected) - 1) == 0);
        mmay_manifest_format_result_dispose(&result);
    }
    mmay_manifest_fixture_dispose(&fixture);
}

static void mmay_test_manifest_invalid_inputs(void) {
    static const struct {
        const unsigned char *bytes;
        size_t length;
        MarimayManifestKind kind;
        const char *code;
    } cases[] = {
        {
            (const unsigned char *)"unknown \"value\"\n",
            sizeof("unknown \"value\"\n") - 1,
            MMAY_MANIFEST_MODULE,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX
        },
        {
            (const unsigned char *)"module \"\\q\"\n",
            sizeof("module \"\\q\"\n") - 1,
            MMAY_MANIFEST_MODULE,
            MMAY_DIAGNOSTIC_MANIFEST_STRING
        },
        {
            (const unsigned char *)"module \"\\uD800\"\n",
            sizeof("module \"\\uD800\"\n") - 1,
            MMAY_MANIFEST_MODULE,
            MMAY_DIAGNOSTIC_MANIFEST_STRING
        },
        {
            (const unsigned char *)"require (\n)\nrequire (\n)\n",
            sizeof("require (\n)\nrequire (\n)\n") - 1,
            MMAY_MANIFEST_MODULE,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX
        },
        {
            (const unsigned char *)"use \"../outside\"\n",
            sizeof("use \"../outside\"\n") - 1,
            MMAY_MANIFEST_WORKSPACE,
            MMAY_DIAGNOSTIC_WORKSPACE_PATH
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayManifestFixture fixture;
        MarimayManifestParseStatus parse_status;
        MarimayManifestFormatResult formatted;
        MMAY_EXPECT(mmay_manifest_fixture_init(
            &fixture,
            cases[index].kind == MMAY_MANIFEST_MODULE ? "mmay.mod" : "mmay.work",
            cases[index].bytes,
            cases[index].length,
            cases[index].kind,
            mmay_manifest_parse_default_options(),
            mmay_system_allocator(),
            &parse_status
        ));
        MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
        MMAY_EXPECT(mmay_manifest_has_diagnostic(&fixture.parsed, cases[index].code));
        MMAY_EXPECT(mmay_manifest_format(
            &fixture.parsed,
            mmay_system_allocator(),
            mmay_manifest_format_default_options(),
            &formatted
        ) == MMAY_MANIFEST_FORMAT_SOURCE_INVALID);
        MMAY_EXPECT(!formatted.initialized);
        mmay_manifest_fixture_dispose(&fixture);
    }

    {
        static const unsigned char bom[] = {
            0xefU, 0xbbU, 0xbfU, 'm', 'o', 'd', 'u', 'l', 'e', ' ', '"', 'x', '"'
        };
        MarimayManifestFixture fixture;
        MarimayManifestParseStatus parse_status;
        MMAY_EXPECT(mmay_manifest_fixture_init(
            &fixture,
            "mmay.mod",
            bom,
            sizeof(bom),
            MMAY_MANIFEST_MODULE,
            mmay_manifest_parse_default_options(),
            mmay_system_allocator(),
            &parse_status
        ));
        MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
        MMAY_EXPECT(mmay_manifest_has_diagnostic(
            &fixture.parsed,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX
        ));
        mmay_manifest_fixture_dispose(&fixture);
    }

    {
        static const unsigned char invalid_utf8[] = {
            'm', 'o', 'd', 'u', 'l', 'e', ' ', '"', 0xffU, '"'
        };
        MarimayManifestFixture fixture;
        MarimayManifestParseStatus parse_status;
        MMAY_EXPECT(mmay_manifest_fixture_init(
            &fixture,
            "mmay.mod",
            invalid_utf8,
            sizeof(invalid_utf8),
            MMAY_MANIFEST_MODULE,
            mmay_manifest_parse_default_options(),
            mmay_system_allocator(),
            &parse_status
        ));
        MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
        MMAY_EXPECT(mmay_manifest_has_diagnostic(
            &fixture.parsed,
            MMAY_DIAGNOSTIC_INVALID_UTF8
        ));
        mmay_manifest_fixture_dispose(&fixture);
    }
}

static void mmay_test_manifest_limits_and_allocation(void) {
    static const char source[] =
        "module \"example/a\"\n"
        "language \"0.1\"\n"
        "require (\n"
        "  \"a\" \"1\"\n"
        ")\n";
    MarimayManifestFixture fixture;
    MarimayManifestParseStatus parse_status;
    MarimayManifestParseOptions parse_options = mmay_manifest_parse_default_options();
    MarimayManifestFormatOptions format_options;
    MarimayManifestFormatResult formatted;
    size_t failure_index = 0;
    bool reached_success = false;

    parse_options.max_entries = 1;
    MMAY_EXPECT(mmay_manifest_fixture_init(
        &fixture,
        "mmay.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        MMAY_MANIFEST_MODULE,
        parse_options,
        mmay_system_allocator(),
        &parse_status
    ));
    MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_LIMIT_REACHED);
    MMAY_EXPECT(fixture.parsed.initialized);
    mmay_manifest_fixture_dispose(&fixture);

    parse_options = mmay_manifest_parse_default_options();
    MMAY_EXPECT(mmay_manifest_fixture_init(
        &fixture,
        "mmay.mod",
        (const unsigned char *)source,
        sizeof(source) - 1,
        MMAY_MANIFEST_MODULE,
        parse_options,
        mmay_system_allocator(),
        &parse_status
    ));
    MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_OK);
    format_options = mmay_manifest_format_default_options();
    format_options.max_decoded_bytes = 1;
    MMAY_EXPECT(mmay_manifest_format(
        &fixture.parsed,
        mmay_system_allocator(),
        format_options,
        &formatted
    ) == MMAY_MANIFEST_FORMAT_LIMIT_REACHED);
    format_options = mmay_manifest_format_default_options();
    format_options.max_output_bytes = 1;
    MMAY_EXPECT(mmay_manifest_format(
        &fixture.parsed,
        mmay_system_allocator(),
        format_options,
        &formatted
    ) == MMAY_MANIFEST_FORMAT_LIMIT_REACHED);

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManifestFormatStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_manifest_format(
            &fixture.parsed,
            allocator,
            mmay_manifest_format_default_options(),
            &formatted
        );
        if (status == MMAY_MANIFEST_FORMAT_OK) {
            reached_success = true;
            mmay_manifest_format_result_dispose(&formatted);
        } else {
            MMAY_EXPECT(status == MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED);
            MMAY_EXPECT(!formatted.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_manifest_fixture_dispose(&fixture);

    reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManifestFixture failing;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_manifest_fixture_init(
            &failing,
            "mmay.mod",
            (const unsigned char *)source,
            sizeof(source) - 1,
            MMAY_MANIFEST_MODULE,
            mmay_manifest_parse_default_options(),
            allocator,
            &parse_status
        ));
        if (parse_status == MMAY_MANIFEST_PARSE_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(parse_status == MMAY_MANIFEST_PARSE_ALLOCATION_FAILED);
            MMAY_EXPECT(!failing.parsed.initialized);
        }
        mmay_manifest_fixture_dispose(&failing);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_manifest(void) {
    mmay_test_manifest_module_round_trip();
    mmay_test_manifest_workspace_round_trip();
    mmay_test_manifest_invalid_inputs();
    mmay_test_manifest_limits_and_allocation();
}
