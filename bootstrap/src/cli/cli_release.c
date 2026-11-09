#include "cli_release.h"

#include <stddef.h>
#include <string.h>

#include "mmay/build_config.h"

#define MMAY_STRINGIFY_INNER(value) #value
#define MMAY_STRINGIFY(value) MMAY_STRINGIFY_INNER(value)

static const MarimayCliTaskHelp mmay_release_tasks[] = {
    {"add", "Add a package dependency.", "add module[@version]", "mmay add marimay.example/postgres@1.4", "I7", false},
    {"audit", "Audit dependencies, capabilities, unsafe code, and provenance.", "audit", "mmay audit", "I8", false},
    {"bench", "Run first-party benchmarks.", "bench [filter]", "mmay bench --compare base.json", "I8", false},
    {"build", "Build a host-native executable for the implemented preview surface.", "build [target] [--allow-env name] [--allow-read path] [--allow-write path] [--max-heap-bytes bytes] [options]", "mmay build . --allow-env HOME --release", "CP35", true},
    {"check", "Check the implemented local preview surface and verified typed MIR.", "check [target] [--all-targets]", "mmay check --all-targets", "CP29", true},
    {"doc", "Build or check package documentation.", "doc [options]", "mmay doc --check", "I8", false},
    {"doctor", "Diagnose the installed host toolchain and native runtimes.", "doctor [--json]", "mmay doctor --json", "CP39", true},
    {"exec", "Verify and execute a portable .myb bundle.", "exec bundle.myb [options]", "mmay exec app.myb", "I3", false},
    {"explain", "Explain a diagnostic or inferred compiler behavior.", "explain code|topic", "mmay explain T0017", "I8", false},
    {"fix", "Apply compiler-authored safe fixes.", "fix [target] [options]", "mmay fix", "I8", false},
    {"fmt", "Canonically format Marimay source and manifests.", "fmt [target] [options]", "mmay fmt --check", "CP29", true},
    {"fuzz", "Run a first-party fuzz target.", "fuzz target [options]", "mmay fuzz \"decoder never panics\"", "I8", false},
    {"generate", "Run explicitly configured external generation.", "generate [target]", "mmay generate", "I8", false},
    {"help", "Show task-oriented command help.", "help [command]", "mmay help build", "CP29", true},
    {"init", "Create a local canonical Marimay module.", "init [path]", "mmay init ./orders", "CP39", true},
    {"list", "List package dependencies.", "list", "mmay list", "I7", false},
    {"lsp", "Serve basic compiler-backed editor features over standard input/output.", "lsp", "mmay lsp", "I8", true},
    {"outdated", "Report available dependency updates.", "outdated", "mmay outdated", "I7", false},
    {"remove", "Remove a package dependency.", "remove module", "mmay remove marimay.example/postgres", "I7", false},
    {"run", "Run the implemented preview surface with the VM or native engine.", "run [target] [--engine vm|native] [--allow-env name] [--allow-read path] [--allow-write path] [--max-heap-bytes bytes] [-- arguments]", "mmay run --engine native . --allow-env HOME", "CP39", true},
    {"test", "Run built-in preview tests with VM/native comparison available.", "test [target] [--engine vm|native|both] [--allow-env name] [--allow-read path] [--allow-write path] [--max-heap-bytes bytes] [-- arguments]", "mmay test . --engine both --allow-env MMAY_ALPHA_TEST", "CP35", true},
    {"update", "Update locked package versions.", "update [module]", "mmay update", "I7", false},
    {"vendor", "Vendor immutable locked dependencies.", "vendor", "mmay vendor", "I7", false},
    {"version", "Show the versioned release contract and build identity.", "version [--json]", "mmay version --json", "CP29", true},
    {"vet", "Run first-party correctness checks.", "vet [target] [options]", "mmay vet --strict", "I8", false},
    {"why", "Explain why a dependency is present.", "why module", "mmay why marimay.example/crypto", "I7", false},
};

static size_t mmay_release_minimum3(
    size_t first,
    size_t second,
    size_t third
) {
    size_t result = first < second ? first : second;
    return result < third ? result : third;
}

static size_t mmay_release_edit_distance(const char *left, const char *right) {
    enum { MMAY_SUGGESTION_LIMIT = 64 };
    size_t previous[MMAY_SUGGESTION_LIMIT + 1];
    size_t current[MMAY_SUGGESTION_LIMIT + 1];
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t left_index = 0;
    size_t right_index = 0;

    if (left_length > MMAY_SUGGESTION_LIMIT
        || right_length > MMAY_SUGGESTION_LIMIT) {
        return MMAY_SUGGESTION_LIMIT;
    }
    for (right_index = 0; right_index <= right_length; right_index += 1) {
        previous[right_index] = right_index;
    }
    for (left_index = 1; left_index <= left_length; left_index += 1) {
        current[0] = left_index;
        for (right_index = 1; right_index <= right_length; right_index += 1) {
            size_t substitution = previous[right_index - 1]
                + (left[left_index - 1] == right[right_index - 1] ? 0U : 1U);
            current[right_index] = mmay_release_minimum3(
                previous[right_index] + 1,
                current[right_index - 1] + 1,
                substitution
            );
        }
        for (right_index = 0; right_index <= right_length; right_index += 1) {
            previous[right_index] = current[right_index];
        }
    }
    return previous[right_length];
}

const MarimayCliTaskHelp *mmay_cli_release_find_task(const char *name) {
    size_t index = 0;
    for (index = 0;
        index < sizeof(mmay_release_tasks) / sizeof(mmay_release_tasks[0]);
        index += 1) {
        if (strcmp(mmay_release_tasks[index].name, name) == 0) {
            return &mmay_release_tasks[index];
        }
    }
    return NULL;
}

const char *mmay_cli_release_suggest_task(const char *name) {
    const char *suggestion = NULL;
    size_t best = 4;
    size_t index = 0;
    for (index = 0;
        index < sizeof(mmay_release_tasks) / sizeof(mmay_release_tasks[0]);
        index += 1) {
        size_t distance = mmay_release_edit_distance(
            name,
            mmay_release_tasks[index].name
        );
        if (distance < best) {
            best = distance;
            suggestion = mmay_release_tasks[index].name;
        }
    }
    return suggestion;
}

bool mmay_cli_release_write_top_help(MarimayWriter writer) {
    static const char help[] =
        "Marimay toolchain (Stage 0 private preview)\n"
        "\n"
        "Current claim: local checker-backed slice; not a public alpha.\n"
        "Usage: mmay <command> [options]\n"
        "\n"
        "Available workflow:\n"
        "  fmt       Canonically format source and manifests\n"
        "  check     Check the implemented source and typed MIR slice\n"
        "  test      Run built-in tests (VM by default)\n"
        "  run       Run a program with the VM or native engine\n"
        "  build     Build a host-native executable\n"
        "  init      Create a local canonical module\n"
        "  lsp       Start the basic language server over stdio\n"
        "\n"
        "Available inspection:\n"
        "  version   Show release contract and build identity\n"
        "  doctor    Diagnose host compiler and runtime profiles\n"
        "  help      Show help for a recognized command\n"
        "\n"
        "Staged (recognized but not executable):\n"
        "  add audit bench doc exec explain fix fuzz generate list\n"
        "  outdated remove update vendor vet why\n"
        "\n"
        "Global options:\n"
        "  --help\n"
        "  --version\n"
        "  --color auto|always|never\n"
        "  --diagnostic human|short|json\n"
        "  --quiet\n"
        "  --verbose\n"
        "\n"
        "Run 'mmay help <command>' for availability, owner, and examples.\n";
    return mmay_writer_write_cstr(writer, help);
}

bool mmay_cli_release_write_task_help(
    MarimayWriter writer,
    const MarimayCliTaskHelp *task
) {
    if (!mmay_writer_write_cstr(writer, "Usage: mmay ")
        || !mmay_writer_write_cstr(writer, task->usage)
        || !mmay_writer_write_cstr(writer, "\n\n")
        || !mmay_writer_write_cstr(writer, task->summary)
        || !mmay_writer_write_cstr(writer, "\n\nExample:\n  ")
        || !mmay_writer_write_cstr(writer, task->example)
        || !mmay_writer_write_cstr(writer, "\n\nAvailability: ")) {
        return false;
    }
    if (task->implemented) {
        return mmay_writer_write_cstr(writer, "available (private preview)\n");
    }
    return mmay_writer_write_cstr(writer, "staged; owner ")
        && mmay_writer_write_cstr(writer, task->owner)
        && mmay_writer_write_cstr(
            writer,
            "; invoking it returns U0003 in this binary.\n"
        );
}

static bool mmay_release_write_command_array(
    MarimayWriter writer,
    bool implemented
) {
    size_t index = 0;
    bool first = true;
    if (!mmay_writer_write_cstr(writer, "[")) return false;
    for (index = 0;
        index < sizeof(mmay_release_tasks) / sizeof(mmay_release_tasks[0]);
        index += 1) {
        if (mmay_release_tasks[index].implemented != implemented) continue;
        if ((!first && !mmay_writer_write_cstr(writer, ","))
            || !mmay_writer_write_cstr(writer, "\"")
            || !mmay_writer_write_cstr(writer, mmay_release_tasks[index].name)
            || !mmay_writer_write_cstr(writer, "\"")) {
            return false;
        }
        first = false;
    }
    return mmay_writer_write_cstr(writer, "]");
}

static bool mmay_release_write_command_words(
    MarimayWriter writer,
    bool implemented
) {
    size_t index = 0;
    bool first = true;
    for (index = 0;
        index < sizeof(mmay_release_tasks) / sizeof(mmay_release_tasks[0]);
        index += 1) {
        if (mmay_release_tasks[index].implemented != implemented) continue;
        if ((!first && !mmay_writer_write_cstr(writer, " "))
            || !mmay_writer_write_cstr(writer, mmay_release_tasks[index].name)) {
            return false;
        }
        first = false;
    }
    return true;
}

static bool mmay_release_write_version_json(MarimayWriter writer) {
    static const char prefix[] =
        "{\"schema\":\"" MMAY_VERSION_SCHEMA "\","
        "\"compiler\":\"" MMAY_COMPILER_VERSION "\","
        "\"runtime\":\"" MMAY_RUNTIME_VERSION "\","
        "\"language\":\"" MMAY_LANGUAGE_VERSION "\","
        "\"release\":{\"channel\":\"" MMAY_RELEASE_CHANNEL "\","
        "\"claim\":\"" MMAY_RELEASE_CLAIM "\","
        "\"publicationReady\":" MMAY_RELEASE_PUBLICATION_READY_JSON "},"
        "\"bytecode\":{\"version\":\"" MMAY_BYTECODE_VERSION "\","
        "\"compatibility\":\"" MMAY_BYTECODE_COMPATIBILITY "\"},"
        "\"standardLibrary\":{\"version\":\""
            MMAY_STANDARD_LIBRARY_VERSION "\","
        "\"scope\":\"" MMAY_STANDARD_LIBRARY_SCOPE "\","
        "\"compatibility\":\"" MMAY_STANDARD_LIBRARY_COMPATIBILITY "\"},"
        "\"bootstrapStage\":" MMAY_STRINGIFY(MMAY_BOOTSTRAP_STAGE) ","
        "\"host\":{\"system\":\"" MMAY_HOST_SYSTEM "\","
        "\"processor\":\"" MMAY_HOST_PROCESSOR "\"},"
        "\"targets\":[\"native-host\"],"
        "\"commands\":{\"available\":";
    return mmay_writer_write_cstr(writer, prefix)
        && mmay_release_write_command_array(writer, true)
        && mmay_writer_write_cstr(writer, ",\"staged\":")
        && mmay_release_write_command_array(writer, false)
        && mmay_writer_write_cstr(writer, "}}\n");
}

bool mmay_cli_release_write_version(MarimayWriter writer, bool json) {
    if (json) return mmay_release_write_version_json(writer);
    return mmay_writer_write_cstr(writer, "Marimay compiler ")
        && mmay_writer_write_cstr(writer, MMAY_COMPILER_VERSION)
        && mmay_writer_write_cstr(writer, "\nrelease-channel ")
        && mmay_writer_write_cstr(writer, MMAY_RELEASE_CHANNEL)
        && mmay_writer_write_cstr(writer, "\nrelease-claim ")
        && mmay_writer_write_cstr(writer, MMAY_RELEASE_CLAIM)
        && mmay_writer_write_cstr(writer, "\npublication-ready ")
        && mmay_writer_write_cstr(writer, MMAY_RELEASE_PUBLICATION_READY_JSON)
        && mmay_writer_write_cstr(writer, "\nruntime ")
        && mmay_writer_write_cstr(writer, MMAY_RUNTIME_VERSION)
        && mmay_writer_write_cstr(writer, "\nlanguage ")
        && mmay_writer_write_cstr(writer, MMAY_LANGUAGE_VERSION)
        && mmay_writer_write_cstr(writer, "\nbytecode ")
        && mmay_writer_write_cstr(writer, MMAY_BYTECODE_VERSION)
        && mmay_writer_write_cstr(writer, " (")
        && mmay_writer_write_cstr(writer, MMAY_BYTECODE_COMPATIBILITY)
        && mmay_writer_write_cstr(writer, ")\nstandard-library ")
        && mmay_writer_write_cstr(writer, MMAY_STANDARD_LIBRARY_VERSION)
        && mmay_writer_write_cstr(writer, " (")
        && mmay_writer_write_cstr(writer, MMAY_STANDARD_LIBRARY_SCOPE)
        && mmay_writer_write_cstr(writer, "; ")
        && mmay_writer_write_cstr(writer, MMAY_STANDARD_LIBRARY_COMPATIBILITY)
        && mmay_writer_write_cstr(writer, ")\nbootstrap-stage ")
        && mmay_writer_write_cstr(writer, MMAY_STRINGIFY(MMAY_BOOTSTRAP_STAGE))
        && mmay_writer_write_cstr(writer, "\nhost ")
        && mmay_writer_write_cstr(writer, MMAY_HOST_SYSTEM)
        && mmay_writer_write_cstr(writer, "/")
        && mmay_writer_write_cstr(writer, MMAY_HOST_PROCESSOR)
        && mmay_writer_write_cstr(writer, "\ntargets native-host\navailable-commands ")
        && mmay_release_write_command_words(writer, true)
        && mmay_writer_write_cstr(writer, "\nstaged-commands ")
        && mmay_release_write_command_words(writer, false)
        && mmay_writer_write_cstr(writer, "\n");
}
