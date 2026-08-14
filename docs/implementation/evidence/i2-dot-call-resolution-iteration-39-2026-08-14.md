# I2 package-local free/dot-call resolution — iteration 39 — 2026-08-14

Gate: shared package-local non-generic D003 free/dot resolver.
Iteration status: complete; I2 remains `in-progress`.

Baseline: `1ae289a` (`feat: publish stable checker definition facts`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: accepted first-parameter ownership and free/dot equivalence in
[D003](../../design/decisions.md), language [§6.2](../../spec/language.md),
dot-call diagnostics in [diagnostics §7](../../spec/diagnostics.md), and the
checker owner in the [compiler architecture](../compiler-architecture.md).

## Delivered checkpoint

- `resolution.c` owns one package-local callable index and one resolver used by
  both free calls and first-parameter dot calls. Insert, remove, and lookup
  hash a live borrowed declaration-name snapshot plus the exact local nominal
  receiver symbol; they do not read `owned_name` after that buffer is moved
  into the published function.
- The same function name is accepted more than once only when every candidate
  is dot-call eligible and the first-parameter types unwrap, through aliases,
  to distinct locally owned record or enum symbols. A later same-receiver
  declaration, including `User` versus `type Handle = User`, remains a
  duplicate name.
- `user.string()` and `string(user)` lower to one `RudaSymbolId` and one MIR
  callee. The dot receiver is compiled once and before explicit arguments.
  Early call/argument resource-limit preflight and transactional callable-index
  rollback are preserved.
- Diagnostics distinguish unknown name, a single same-name function that is
  not eligible, a receiver that is not a locally owned nominal, an eligible
  function for a different local nominal, and form-specific overloaded free
  calls. Overloaded-free wording states that `value.name()` and `name(value)`
  are one symbol. No diagnostic suggests an `impl` block.

## Verification coverage

- Source checker tests in `test_checker_resolution.c` cover free/dot symbol
  identity, alias-to-record eligibility, enum same-name overloads, receiver
  evaluation order and once-only effects, the negative overload matrix, call
  resource-limit atomicity, and fail-every-allocation with zero live
  allocations and bytes.
- The same source example interprets to `42` on the MIR oracle. VM and native
  still reject record construction as an unsupported type or instruction;
  both engines report that same unsupported class. This is not a completed
  VM/native execution differential.

## Deliberate boundary and removal gates

- The resolver is package-local and non-generic. Imports, D043 import-form
  invariance, generics, interface method tables, function values of overloaded
  names, occurrence facts, completion, and public `ruda check` remain later
  gates.
- Record and enum construction are still MIR-oracle only. Executing the
  free/dot source example on VM and native requires the next I3/I4 record
  lowering slice.
- This checkpoint is not I2 verification and does not change I3/I4 gate state.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| Debug build plus full CTest | 0 | resolver, diagnostic, limit, allocation, and MIR identity tests passed |
| Release build plus full CTest | 0 | optimized suite passed |
| ASan+UBSan build plus full CTest with halt-on-error and Apple leak detection disabled | 0 | no sanitizer report; no global Apple leak claim |
| strict C17 and simulated `_WIN32` compile of changed production/test translation units | 0 | warnings-as-errors compile-only portability check |
| Clang static analysis of `resolution.c` and `checker.c` | 0 | no analyzer finding |
| generated/docs/spec/example/memory-policy checks | 0 | repository consistency gates passed |
| `git diff --check` | 0 | clean |
