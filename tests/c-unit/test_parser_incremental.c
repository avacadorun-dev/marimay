#include "test_parser_support.h"

void mmay_test_parser_incremental_reuse(void) {
    static const char initial[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 2; }\n"
        "fn last(): int { return 3; }\n";
    static const char same_length[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 4; }\n"
        "fn last(): int { return 3; }\n";
    static const char shifted[] =
        "fn first(): int { return 1; }\n"
        "fn middle(): int { return 200; }\n"
        "fn last(): int { return 3; }\n";
    size_t edit_start = (size_t)(strstr(initial, "return 2") - initial)
        + strlen("return ");
    size_t failure_index = 0;

    {
        MarimaySourceManager sources;
        const MarimaySourceSnapshot *initial_snapshot = NULL;
        const MarimaySourceSnapshot *overlay = NULL;
        MarimayLexResult initial_lexed;
        MarimayLexResult overlay_lexed;
        MarimayParseResult initial_parsed;
        MarimayParseResult overlay_parsed;
        MarimayIncrementalParseStats stats;
        const MarimayGreenNode *first = NULL;
        const MarimayGreenNode *middle = NULL;
        const MarimayGreenNode *last = NULL;
        MMAY_EXPECT(mmay_source_manager_init(
            &sources,
            mmay_system_allocator()
        ));
        MMAY_EXPECT(mmay_source_manager_add(
            &sources,
            "incremental.my",
            strlen("incremental.my"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            initial_snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &initial_lexed
        ) == MMAY_LEX_OK);
        MMAY_EXPECT(mmay_parse_file(
            &initial_lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &initial_parsed
        ) == MMAY_PARSE_OK);
        first = mmay_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        );
        middle = mmay_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            1
        );
        last = mmay_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        );
        MMAY_EXPECT(first != NULL && middle != NULL && last != NULL);
        MMAY_EXPECT(mmay_source_manager_overlay(
            &sources,
            mmay_source_file_id(initial_snapshot),
            same_length,
            strlen(same_length),
            &overlay
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            overlay,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &overlay_lexed
        ) == MMAY_LEX_OK);
        MMAY_EXPECT(mmay_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == MMAY_PARSE_OK);
        MMAY_EXPECT(stats.reused_top_level_nodes == 2);
        MMAY_EXPECT(mmay_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        MMAY_EXPECT(mmay_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            1
        ) != middle);
        MMAY_EXPECT(mmay_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        ) == last);
        mmay_parse_result_dispose(&initial_parsed);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&overlay_parsed.tree));
        mmay_parse_result_dispose(&overlay_parsed);
        mmay_lex_result_dispose(&overlay_lexed);
        mmay_lex_result_dispose(&initial_lexed);
        mmay_source_manager_dispose(&sources);
    }

    {
        MarimaySourceManager sources;
        const MarimaySourceSnapshot *initial_snapshot = NULL;
        const MarimaySourceSnapshot *overlay = NULL;
        MarimayLexResult initial_lexed;
        MarimayLexResult overlay_lexed;
        MarimayParseResult initial_parsed;
        MarimayParseResult overlay_parsed;
        MarimayIncrementalParseStats stats;
        const MarimayGreenNode *first = NULL;
        const MarimayGreenNode *last = NULL;
        bool reached_success = false;
        MMAY_EXPECT(mmay_source_manager_init(
            &sources,
            mmay_system_allocator()
        ));
        MMAY_EXPECT(mmay_source_manager_add(
            &sources,
            "incremental-shift.my",
            strlen("incremental-shift.my"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            initial_snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &initial_lexed
        ) == MMAY_LEX_OK);
        MMAY_EXPECT(mmay_parse_file(
            &initial_lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &initial_parsed
        ) == MMAY_PARSE_OK);
        first = mmay_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        );
        last = mmay_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        );
        MMAY_EXPECT(mmay_source_manager_overlay(
            &sources,
            mmay_source_file_id(initial_snapshot),
            shifted,
            strlen(shifted),
            &overlay
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            overlay,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &overlay_lexed
        ) == MMAY_LEX_OK);
        MMAY_EXPECT(mmay_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 3,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == MMAY_PARSE_OK);
        MMAY_EXPECT(stats.reused_top_level_nodes == 1);
        MMAY_EXPECT(mmay_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        MMAY_EXPECT(mmay_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            MMAY_SYNTAX_FUNCTION_DECL,
            2
        ) != last);
        mmay_parse_result_dispose(&overlay_parsed);

        memset(&overlay_parsed, 0, sizeof(overlay_parsed));
        MMAY_EXPECT(mmay_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == MMAY_PARSE_INVALID_ARGUMENT);
        MMAY_EXPECT(!overlay_parsed.initialized);

        for (failure_index = 0; failure_index < 256; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            MarimayParseStatus status;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            allocator = mmay_fail_allocator(&state);
            status = mmay_parse_file_incremental(
                &initial_parsed,
                &overlay_lexed,
                edit_start,
                edit_start + 1,
                edit_start + 3,
                allocator,
                mmay_parse_default_options(),
                &overlay_parsed,
                &stats
            );
            if (status == MMAY_PARSE_OK) {
                reached_success = true;
                MMAY_EXPECT(stats.reused_top_level_nodes == 1);
                mmay_parse_result_dispose(&overlay_parsed);
            } else {
                MMAY_EXPECT(status == MMAY_PARSE_ALLOCATION_FAILED);
                MMAY_EXPECT(!overlay_parsed.initialized);
                MMAY_EXPECT(stats.reused_top_level_nodes == 0);
                MMAY_EXPECT(stats.reparsed_bytes == 0);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
        mmay_parse_result_dispose(&initial_parsed);
        mmay_lex_result_dispose(&overlay_lexed);
        mmay_lex_result_dispose(&initial_lexed);
        mmay_source_manager_dispose(&sources);
    }
}

void mmay_test_parser_limits_and_allocation(void) {
    static const char source[] =
        "fn main(): void { const value = (((((((((((1))))))))))); }";
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayParseOptions options = mmay_parse_default_options();
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
    MMAY_EXPECT(mmay_source_manager_add(
        &sources,
        "limits.my",
        strlen("limits.my"),
        source,
        strlen(source),
        &snapshot
    ) == MMAY_SOURCE_OK);
    MMAY_EXPECT(mmay_lex_snapshot(
        snapshot,
        mmay_system_allocator(),
        mmay_lex_default_options(),
        &lexed
    ) == MMAY_LEX_OK);

    options.max_events = 8;
    MMAY_EXPECT(mmay_parse_file(
        &lexed,
        mmay_system_allocator(),
        options,
        &parsed
    ) == MMAY_PARSE_LIMIT_REACHED);
    MMAY_EXPECT(!parsed.initialized);

    options = mmay_parse_default_options();
    options.max_nesting = 8;
    MMAY_EXPECT(mmay_parse_file(
        &lexed,
        mmay_system_allocator(),
        options,
        &parsed
    ) == MMAY_PARSE_LIMIT_REACHED);
    MMAY_EXPECT(!parsed.initialized);

    options = mmay_parse_default_options();
    options.max_diagnostics = 1;
    {
        static const char diagnostic_source[] =
            "fn broken(value: Name...): void {}";
        MarimaySourceManager diagnostic_sources;
        const MarimaySourceSnapshot *diagnostic_snapshot = NULL;
        MarimayLexResult diagnostic_lexed;
        MMAY_EXPECT(mmay_source_manager_init(
            &diagnostic_sources,
            mmay_system_allocator()
        ));
        MMAY_EXPECT(mmay_source_manager_add(
            &diagnostic_sources,
            "diagnostic-limit.my",
            strlen("diagnostic-limit.my"),
            diagnostic_source,
            strlen(diagnostic_source),
            &diagnostic_snapshot
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            diagnostic_snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &diagnostic_lexed
        ) == MMAY_LEX_OK);
        MMAY_EXPECT(mmay_parse_file(
            &diagnostic_lexed,
            mmay_system_allocator(),
            options,
            &parsed
        ) == MMAY_PARSE_LIMIT_REACHED);
        MMAY_EXPECT(!parsed.initialized);
        mmay_lex_result_dispose(&diagnostic_lexed);
        mmay_source_manager_dispose(&diagnostic_sources);
    }

    {
        static const unsigned char fuzz_prefix[] =
            "fn broken(valu, broken(v";
        static const unsigned char fuzz_suffix[] = "e int): Res";
        static const unsigned char import_prefix[] = "import ";
        unsigned char fuzz_regression[254];
        size_t offset = 0;
        MarimaySourceManager fuzz_sources;
        const MarimaySourceSnapshot *fuzz_snapshot = NULL;
        MarimayLexResult fuzz_lexed;
        memcpy(fuzz_regression + offset, fuzz_prefix, sizeof(fuzz_prefix) - 1);
        offset += sizeof(fuzz_prefix) - 1;
        memset(fuzz_regression + offset, '.', 128);
        offset += 128;
        memset(fuzz_regression + offset, ',', 57);
        offset += 57;
        memcpy(fuzz_regression + offset, fuzz_suffix, sizeof(fuzz_suffix) - 1);
        offset += sizeof(fuzz_suffix) - 1;
        memset(fuzz_regression + offset, 0, 25);
        offset += 25;
        memcpy(
            fuzz_regression + offset,
            import_prefix,
            sizeof(import_prefix) - 1
        );
        offset += sizeof(import_prefix) - 1;
        memset(fuzz_regression + offset, 0, 2);
        offset += 2;
        MMAY_EXPECT(offset == sizeof(fuzz_regression));
        MMAY_EXPECT(mmay_source_manager_init(
            &fuzz_sources,
            mmay_system_allocator()
        ));
        MMAY_EXPECT(mmay_source_manager_add(
            &fuzz_sources,
            "frontend-fuzz-regression.my",
            strlen("frontend-fuzz-regression.my"),
            fuzz_regression,
            sizeof(fuzz_regression),
            &fuzz_snapshot
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(mmay_lex_snapshot(
            fuzz_snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fuzz_lexed
        ) == MMAY_LEX_OK);
        options = mmay_parse_default_options();
        options.max_events = 400000;
        options.max_diagnostics = 128;
        options.max_nesting = 128;
        MMAY_EXPECT(mmay_parse_file(
            &fuzz_lexed,
            mmay_system_allocator(),
            options,
            &parsed
        ) == MMAY_PARSE_LIMIT_REACHED);
        MMAY_EXPECT(!parsed.initialized);
        mmay_lex_result_dispose(&fuzz_lexed);
        mmay_source_manager_dispose(&fuzz_sources);
    }

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayParseStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_parse_file(
            &lexed,
            allocator,
            mmay_parse_default_options(),
            &parsed
        );
        if (status == MMAY_PARSE_OK) {
            reached_success = true;
            MMAY_EXPECT(mmay_syntax_tree_is_lossless(&parsed.tree));
            mmay_parse_result_dispose(&parsed);
        } else {
            MMAY_EXPECT(status == MMAY_PARSE_ALLOCATION_FAILED);
            MMAY_EXPECT(!parsed.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_lex_result_dispose(&lexed);
    mmay_source_manager_dispose(&sources);
}

void mmay_test_parser_all_single_bytes(void) {
    unsigned int value = 0;
    for (value = 0; value <= 255; value += 1) {
        unsigned char byte = (unsigned char)value;
        MarimayParseFixture fixture;
        bool initialized = mmay_parse_fixture_init_bytes(&fixture, &byte, 1);
        MMAY_EXPECT(initialized);
        if (initialized) {
            MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
            MMAY_EXPECT(fixture.parsed.tree.root->text_length == 1);
            mmay_parse_fixture_dispose(&fixture);
        }
    }
}
