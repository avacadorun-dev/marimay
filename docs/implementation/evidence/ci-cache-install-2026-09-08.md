# Run 212 and Python-cache installation boundary

Run 212 on `d0d69eb0dd97c3385335b3b9809ca0956078a310` completed all four
Clang/GCC Debug/Release jobs and all four 600-second fuzz campaigns. The
ASan/UBSan/LSan job ran 76 tests: 75 passed; `mmay_cp39_distribution` failed
because the staged installation contained an unlisted
`tools/ci-diagnostics/__pycache__` directory. TSan was skipped by the existing
dependency. This is not a complete sanitizer acceptance result.

`test_collect.py` imports its sibling module, which can create a bytecode cache
in the source directory while CTest stages the distribution. CMake's
`FILES_MATCHING` excludes the `.pyc` file but can still copy its empty directory.
The strict installed-file inventory correctly rejected that directory.

The fix sets `PYTHONDONTWRITEBYTECODE=1` on both Python tool tests, inherited by
the acceptance driver's child Python process. The install rule also excludes
`__pycache__`, covering caches left by earlier local tooling. The installed
inventory remains strict; no test, artifact step or sanitizer is disabled.

Local verification passed the two tool tests and CP39 distribution (3/3,
15.11 seconds) with a deliberately planted synthetic `.pyc` under the source
tools cache. The staged installation passed the unchanged inventory check.
That synthetic file was then removed. This local check ran alongside ongoing
D097 work; it establishes the packaging boundary, not acceptance of that new
language feature. A fresh pushed CI result is still required.

The infrastructure agent observed stable services throughout run 212: one
runner, four build jobs and two sanitizer test jobs; 4,932 one-second samples
had a maximum runnable queue of 9, unused swap and no new RCU/OOM/watchdog/I/O
error. Previous stall and transient compiler-failure causes remain unknown.
The [raw observations](ci-run-212-2026-09-08.json) preserve job outcomes and
log hashes. Full sanitized logs/artifacts remain in the ignored
`build/gitea-recovery-2026-09-07/` directory. No book idea was used here.
