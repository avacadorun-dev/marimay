# Contributing to Marimay

Marimay is a personal learning project, shared for curiosity and fun.
If you would like to join in, start with a focused bug,
test, documentation improvement, or an issue proposing a design change.
Large language changes need agreement on the contract before implementation.

## Set up and validate

Use CMake 3.25+, a C17 compiler, and Python 3.10+. The default bootstrap needs
no package manager or network fetch.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
./tools/check-example-syntax
./tools/check-syntax-coverage
./tools/check-c-memory-policy
./tools/check-architecture
./tools/check-roadmap
./tools/check-release-contract
./tools/check-publication
```

Use `asan-ubsan` for memory and undefined-behavior checks. Linux LeakSanitizer
and every claimed Tier 1 platform remain required for milestone verification;
a local macOS run cannot replace them. See [CI](docs/ci.md) for the full matrix.

## Follow the contracts

[AGENTS.md](AGENTS.md) applies to every contribution:

1. Read the relevant spec, accepted decision, architecture owner, and focused
   tests before editing. Specifications take precedence over implementation.
2. For a semantic change, include the decision, normative spec, focused
   parser/checker/runtime tests, VM/native differential coverage, formatter
   fixtures, and public examples in the same change.
3. Keep C owners narrow and independently compiled. Respect architecture and
   line-count gates; do not grow a broad coordinator.
4. Allocation-capable C APIs require cleanup and deterministic failure-index
   coverage, with zero live allocations **and** zero live bytes after teardown.
5. Preserve unrelated work. Do not weaken tests or claim open roadmap gates
   are complete to make a contribution pass.

Use descriptive branches such as `fix/formatter-layout`, `feature/...`, or
`chore/...`. Do not use a `codex/` branch prefix.

## Send a pull request

Describe the problem, resulting behavior, relevant decision/spec, and checks
you actually ran. State platform limits and unresolved failures. Keep unrelated
refactors separate. Agent-assisted work has the same review requirements;
see [the workflow](docs/agent-workflow.md).

Before sharing code, logs, screenshots, or evidence, remove credentials,
personal emails and paths, private hostnames/remotes, environment dumps, and
unrelated process information. Use a GitHub no-reply commit address to keep
your email private. Report vulnerabilities using [SECURITY.md](SECURITY.md).

By contributing original work, you agree to license it under the repository's
[MIT License](LICENSE). Preserve applicable third-party notices. Be respectful,
discuss technical choices with evidence, and protect others' private information.
