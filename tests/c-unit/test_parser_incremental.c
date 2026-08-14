#include "test_parser_support.h"

void ruda_test_parser_incremental_reuse(void) {
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
        RudaSourceManager sources;
        const RudaSourceSnapshot *initial_snapshot = NULL;
        const RudaSourceSnapshot *overlay = NULL;
        RudaLexResult initial_lexed;
        RudaLexResult overlay_lexed;
        RudaParseResult initial_parsed;
        RudaParseResult overlay_parsed;
        RudaIncrementalParseStats stats;
        const RudaGreenNode *first = NULL;
        const RudaGreenNode *middle = NULL;
        const RudaGreenNode *last = NULL;
        RUDA_EXPECT(ruda_source_manager_init(
            &sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &sources,
            "incremental.ruda",
            strlen("incremental.ruda"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            initial_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &initial_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &initial_lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &initial_parsed
        ) == RUDA_PARSE_OK);
        first = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        );
        middle = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            1
        );
        last = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        );
        RUDA_EXPECT(first != NULL && middle != NULL && last != NULL);
        RUDA_EXPECT(ruda_source_manager_overlay(
            &sources,
            ruda_source_file_id(initial_snapshot),
            same_length,
            strlen(same_length),
            &overlay
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            overlay,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &overlay_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_OK);
        RUDA_EXPECT(stats.reused_top_level_nodes == 2);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            1
        ) != middle);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) == last);
        ruda_parse_result_dispose(&initial_parsed);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&overlay_parsed.tree));
        ruda_parse_result_dispose(&overlay_parsed);
        ruda_lex_result_dispose(&overlay_lexed);
        ruda_lex_result_dispose(&initial_lexed);
        ruda_source_manager_dispose(&sources);
    }

    {
        RudaSourceManager sources;
        const RudaSourceSnapshot *initial_snapshot = NULL;
        const RudaSourceSnapshot *overlay = NULL;
        RudaLexResult initial_lexed;
        RudaLexResult overlay_lexed;
        RudaParseResult initial_parsed;
        RudaParseResult overlay_parsed;
        RudaIncrementalParseStats stats;
        const RudaGreenNode *first = NULL;
        const RudaGreenNode *last = NULL;
        bool reached_success = false;
        RUDA_EXPECT(ruda_source_manager_init(
            &sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &sources,
            "incremental-shift.ruda",
            strlen("incremental-shift.ruda"),
            initial,
            strlen(initial),
            &initial_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            initial_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &initial_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &initial_lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &initial_parsed
        ) == RUDA_PARSE_OK);
        first = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        );
        last = ruda_syntax_direct_nth_kind(
            initial_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        );
        RUDA_EXPECT(ruda_source_manager_overlay(
            &sources,
            ruda_source_file_id(initial_snapshot),
            shifted,
            strlen(shifted),
            &overlay
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            overlay,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &overlay_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 3,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_OK);
        RUDA_EXPECT(stats.reused_top_level_nodes == 1);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            0
        ) == first);
        RUDA_EXPECT(ruda_syntax_direct_nth_kind(
            overlay_parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL,
            2
        ) != last);
        ruda_parse_result_dispose(&overlay_parsed);

        memset(&overlay_parsed, 0, sizeof(overlay_parsed));
        RUDA_EXPECT(ruda_parse_file_incremental(
            &initial_parsed,
            &overlay_lexed,
            edit_start,
            edit_start + 1,
            edit_start + 1,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &overlay_parsed,
            &stats
        ) == RUDA_PARSE_INVALID_ARGUMENT);
        RUDA_EXPECT(!overlay_parsed.initialized);

        for (failure_index = 0; failure_index < 256; failure_index += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaParseStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            allocator = ruda_fail_allocator(&state);
            status = ruda_parse_file_incremental(
                &initial_parsed,
                &overlay_lexed,
                edit_start,
                edit_start + 1,
                edit_start + 3,
                allocator,
                ruda_parse_default_options(),
                &overlay_parsed,
                &stats
            );
            if (status == RUDA_PARSE_OK) {
                reached_success = true;
                RUDA_EXPECT(stats.reused_top_level_nodes == 1);
                ruda_parse_result_dispose(&overlay_parsed);
            } else {
                RUDA_EXPECT(status == RUDA_PARSE_ALLOCATION_FAILED);
                RUDA_EXPECT(!overlay_parsed.initialized);
                RUDA_EXPECT(stats.reused_top_level_nodes == 0);
                RUDA_EXPECT(stats.reparsed_bytes == 0);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
        ruda_parse_result_dispose(&initial_parsed);
        ruda_lex_result_dispose(&overlay_lexed);
        ruda_lex_result_dispose(&initial_lexed);
        ruda_source_manager_dispose(&sources);
    }
}

void ruda_test_parser_limits_and_allocation(void) {
    static const char source[] =
        "fn main(): void { const value = (((((((((((1))))))))))); }";
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaParseOptions options = ruda_parse_default_options();
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "limits.ruda",
        strlen("limits.ruda"),
        source,
        strlen(source),
        &snapshot
    ) == RUDA_SOURCE_OK);
    RUDA_EXPECT(ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        ruda_lex_default_options(),
        &lexed
    ) == RUDA_LEX_OK);

    options.max_events = 8;
    RUDA_EXPECT(ruda_parse_file(
        &lexed,
        ruda_system_allocator(),
        options,
        &parsed
    ) == RUDA_PARSE_LIMIT_REACHED);
    RUDA_EXPECT(!parsed.initialized);

    options = ruda_parse_default_options();
    options.max_nesting = 8;
    RUDA_EXPECT(ruda_parse_file(
        &lexed,
        ruda_system_allocator(),
        options,
        &parsed
    ) == RUDA_PARSE_LIMIT_REACHED);
    RUDA_EXPECT(!parsed.initialized);

    options = ruda_parse_default_options();
    options.max_diagnostics = 1;
    {
        static const char diagnostic_source[] = "a; b; c;";
        RudaSourceManager diagnostic_sources;
        const RudaSourceSnapshot *diagnostic_snapshot = NULL;
        RudaLexResult diagnostic_lexed;
        RUDA_EXPECT(ruda_source_manager_init(
            &diagnostic_sources,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_source_manager_add(
            &diagnostic_sources,
            "diagnostic-limit.ruda",
            strlen("diagnostic-limit.ruda"),
            diagnostic_source,
            strlen(diagnostic_source),
            &diagnostic_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(ruda_lex_snapshot(
            diagnostic_snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &diagnostic_lexed
        ) == RUDA_LEX_OK);
        RUDA_EXPECT(ruda_parse_file(
            &diagnostic_lexed,
            ruda_system_allocator(),
            options,
            &parsed
        ) == RUDA_PARSE_LIMIT_REACHED);
        RUDA_EXPECT(!parsed.initialized);
        ruda_lex_result_dispose(&diagnostic_lexed);
        ruda_source_manager_dispose(&diagnostic_sources);
    }

    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaParseStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_parse_file(
            &lexed,
            allocator,
            ruda_parse_default_options(),
            &parsed
        );
        if (status == RUDA_PARSE_OK) {
            reached_success = true;
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&parsed.tree));
            ruda_parse_result_dispose(&parsed);
        } else {
            RUDA_EXPECT(status == RUDA_PARSE_ALLOCATION_FAILED);
            RUDA_EXPECT(!parsed.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_lex_result_dispose(&lexed);
    ruda_source_manager_dispose(&sources);
}

void ruda_test_parser_all_single_bytes(void) {
    unsigned int value = 0;
    for (value = 0; value <= 255; value += 1) {
        unsigned char byte = (unsigned char)value;
        RudaParseFixture fixture;
        bool initialized = ruda_parse_fixture_init_bytes(&fixture, &byte, 1);
        RUDA_EXPECT(initialized);
        if (initialized) {
            RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
            RUDA_EXPECT(fixture.parsed.tree.root->text_length == 1);
            ruda_parse_fixture_dispose(&fixture);
        }
    }
}
