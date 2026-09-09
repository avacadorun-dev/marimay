# Marimay execution and runtime specification — design baseline 0.1

Status: normative design baseline.  
Applies to: bytecode VM, C17 AOT backend, runtime, and standard-library I/O.

## 1. Central invariant

A Marimay source program has one language meaning. The VM and native backend may differ in startup time, throughput, memory layout, optimization, and deployment shape. They must not differ in accepted syntax, static types, evaluation order, numeric behavior, panic rules, task structure, standard-library contracts, or safe-code results.

The engine is not observable through a safe standard API. A program cannot branch on “VM versus native”. Engine-specific diagnostics and profiler metadata are outside program semantics.

## 2. Compilation pipeline

Both engines consume the same front half:

```text
.my source
  -> UTF-8 source manager and tokens
  -> lossless syntax tree
  -> resolved HIR
  -> type, interface, exhaustiveness, and effect checking
  -> typed HIR
  -> control-flow and cleanup lowering
  -> typed MIR
      -> verified register bytecode -> .myb -> C17 VM
      -> portable generated C17 -> host C compiler -> native executable
```

Typed MIR is the semantic convergence point. A backend may optimize MIR only with transformations proven or tested to preserve this specification. Backend-specific acceptance or type repair is forbidden.

Every MIR function contains:

- a stable symbol identifier;
- typed parameters, locals, temporaries, and result place;
- explicit basic blocks and left-to-right evaluation temporaries;
- explicit checked arithmetic and bounds operations, plus verifier-gated
  in-bounds consumers of an explicit checked capability;
- explicit enum tag tests and payload projections;
- explicit nominal-record identity comparison over stable managed handles, not
  backend object addresses;
- explicit cleanup edges for `defer`, task scope, and managed roots;
- a compiler-inferred suspension bit and suspension-point table;
- source spans for every user-observable operation.

`mmay build --emit mir` is a diagnostic format, not a stable public ABI. A versioned MIR test serializer is maintained internally for golden and differential tests.

## 3. Required execution engines

### 3.1 Typed bytecode VM

The VM is required in every standard Marimay installation. It is the default engine for `mmay run`, `mmay test`, the REPL, debugger evaluation, compile-time execution, and sandboxed `.myb` execution.

The baseline VM is an interpreter. A JIT is not required for 1.0 and cannot become a semantic dependency.

### 3.2 Native AOT

The native backend lowers typed MIR to portable C17 and invokes a supported host C toolchain. It is the default engine for `mmay build`. Generated C and the C runtime form an implementation boundary, not a stable foreign API.

A native program is standalone except for documented operating-system libraries, the platform C runtime, and explicitly requested dynamic FFI dependencies. The Marimay runtime is statically linked by default.

### 3.3 Scenario defaults

| Scenario | Default | Reason |
|---|---|---|
| edit/run | VM | minimum feedback latency |
| unit and integration tests | VM | fast process reuse and isolation |
| semantic release gate | both | differential conformance |
| REPL/debug evaluation | VM | inspectable frames and sandboxing |
| distributable service/CLI | native | standalone startup and throughput |
| portable plugin/script artifact | `.myb` | validated target-neutral bundle |

Every default can be made explicit with `--engine vm|native|both` where the command supports it. `both` is a verification mode, not a third engine.

## 4. Bytecode contract

### 4.1 Instruction model

Marimay bytecode is register-based. Each function declares typed virtual registers, constants, control-flow blocks, root maps, and suspension metadata. Instructions never infer operand types at runtime; the verifier confirms the declared type state before execution.

Instruction families include constants and moves, checked numeric operations, record/enum construction and projection, direct/interface/closure/extern calls, typed branches, verified collection intrinsics, task operations, root registration, panic, cleanup transfer, and return. First-parameter dot calls are resolved before MIR and therefore require no separate call instruction.

There is no instruction that treats arbitrary data as a type, jumps to an unchecked byte offset, accesses a field without a layout descriptor, or dereferences a raw pointer in verified safe bytecode.

### 4.2 `.myb` format

A portable bundle contains, in canonical order:

1. magic, format version, and feature bitmap;
2. module identity, language version, and dependency hashes;
3. string and constant pools;
4. nominal type and interface descriptors;
5. function signatures and effect metadata;
6. bytecode bodies and exception-free cleanup tables;
7. GC root maps;
8. source/debug tables, optionally stripped;
9. declared runtime capabilities;
10. whole-file cryptographic digest and optional publisher signature.

All integers in the file format use a specified little-endian variable-length encoding independent of host endianness. Unknown mandatory features cause loader rejection. Unknown optional debug sections may be skipped.

A portable `.myb` contains no native machine code. An FFI import marks a bundle platform-constrained; the loader validates ABI, library identity, and granted capability before linking it.

#### 4.2.1 Generic array type table

New bundles that contain `[]T` use a canonical array-type table in the
program type section. Each row stores the bytecode element type tag and either
`NO_LAYOUT` for scalar elements and the built-in canonical string descriptor,
or the index of the exact array, record, enum, or interface layout for another
managed reference element. Array construction, access, and growth instructions carry
the row index through their verified function metadata. A source `MarimayTypeId`,
allocator address, descriptor pointer, or other host pointer is never written
to the wire. The loader rejects an out-of-range row, a mismatched element
layout, a nominal/reference row without its required exact layout, or a row
that claims a scalar or string element with a layout-table index.

The row and every referenced nominal layout denote the same canonical,
fully-qualified instantiated type in every function that transports it. A
package call, result, public alias, or forwarding wrapper does not create a
fresh array representation. Verification compares the complete recursive
element/layout identity across call boundaries before execution.

The two fixed f64 array tags are retained only as a compatibility decoder path
for legacy f64-only preview bundles. They are not the representation of a new
generic array bundle.

#### 4.2.2 Result-bearing collection operations

Under D079, collection operations may construct their typed source result
directly, without a separate membership query. Wire opcodes 154
`collection_get_option`, 155 `collection_remove_option`, and 157
`collection_add_result` have destination, collection, and key registers.
Opcode 156 `collection_put_result` additionally has a fourth value register.
Earlier opcode IDs, including 153 `test_assert`, retain their meaning.

The destination has an exact enum layout: Map get/remove require the
`Some(V), None` shape with the collection's exact value layout; put/add require
`Ok(bool), Err(CollectionGrowthError)`, with the error carrying an `int`
length limit. MIR also verifies the corresponding prelude nominal identities.
Bytecode verifies the instantiated destination and recursive payload layouts;
source-level generic base symbols are not substituted for wire type identities.
Map/Set receiver kind, key and value types, and all input register assignments
are verified before execution. These operations are allocation safepoints;
removed reference payloads remain rooted while their result is constructed.

These instructions preserve D079 order, mutation generation, and result values.
Only the language length limit becomes a growth error. Configured execution
limits and allocation failures remain execution outcomes under D055. Hash and
probe counts are internal: storage growth or compaction may require reprobing.

#### 4.2.3 Ordered collection cursors

D089 appends wire opcodes 158 `collection_next`, 159 `collection_cursor_key`,
and 160 `collection_cursor_value`. Each has destination, collection and cursor
register operands. The cursor and NEXT destination are exact `int` scalars;
key/value destinations match the receiver's exact recursive layout. VALUE
requires a Map. All inputs must be definitely assigned. IDs 151/152 continue
to read active insertion-order ordinals, independently of these operations.

Cursor zero starts a traversal. NEXT scans from the supplied nonnegative
storage offset and returns one past the next active position, or zero at
exhaustion. A position beyond storage is rejected. KEY/VALUE require a positive
cursor naming an active slot and reject zero, removed or out-of-range slots.
Negative cursors are rejected before host-size conversion. Invalid dynamic
cursor values produce an invalid-reference VM execution outcome (the shared
runtime reports bounds), not a source panic without a panic payload. A cursor grants no
unchecked storage access and carries no pointer. Each runtime operation checks
the complete receiver handle and Map/Set kind. These operations allocate no
managed state and are not allocation safepoints; reference results participate
in the ordinary engine rooting/liveness contract.

Source loops own independent cursors and check the captured mutation generation
before every NEXT, even the final exhaustion check. Break performs no further
advance; continue reaches the next generation check. D079 insertion order,
G4005 messages and exact source spans remain unchanged. A complete traversal
without mutation scans each storage position at most once; key/value reads
at a returned cursor take constant time. Internal cursors and storage offsets
are not source-level indices, capacity promises or a public iterator API.

### 4.3 Verification

Under D080, opcode 11 `i64_equal` compares two registers of the same integer
type or two exact `rune` registers and writes `bool`. Rune equality compares
the scalar value without conversion. Other integer opcodes do not acquire
rune operands from this rule. The opcode ID and operand encoding are unchanged.

The loader verifies a bundle before any module code becomes executable. Verification checks at least section bounds and overflow, UTF-8, symbol identity, control-flow targets, register definite assignment and types, call signatures, enum consistency, root maps, task-scope nesting, unsafe opcode placement, capabilities, and resource limits.

Verification failure is a loader diagnostic and must not crash or partially initialize the runtime. The bytecode verifier is fuzzed as an untrusted-input boundary. An encoded in-bounds array operation is accepted only when verification reconstructs a matching dominating `ARRAY_REQUIRE_INDEX` capability for the same carrier and index. MIR-only static range assumptions lower to that checked bytecode barrier rather than claiming a serialized range fact. Calls, interface calls, stores, and every array mutation/resize operation invalidate capabilities. A verifier allocation or work-limit failure rejects the bundle; it never treats a missing proof as trusted.

Under D062, wire opcodes 98 `i64_add_proven`, 99
`i64_subtract_proven`, and 100 `i64_multiply_proven` have
`destination`, `left`, and `right` register operands and no `may_panic` flag.
All three registers must be exact `i64`. Before accepting any such opcode, the
verifier reconstructs the bounded signed/unsigned interval dataflow, including
join, loop-header widening followed by bounded narrowing, conversion,
remainder, and comparison-edge rules, and proves the result representable at
that exact instruction. Serialized bounds
or producer trust cannot substitute for reconstruction. Missing proof,
allocation failure, or work-limit exhaustion rejects the bundle before VM
execution.

D062 loop-header narrowing tightens only endpoints still at the declared type
extrema; already finite endpoints are retained if recomputation would tighten
them further. A recomputed expansion is never suppressed. This makes narrowing
converge without walking a signed 64-bit interval one integer at a time, while
retaining conservative ranges and the independent work-limit boundary. The MIR
optimizer and both proof verifiers use the same interval rules.

Before backend lowering, D106 permits bounded, transactional scalar/control
simplification of verified typed MIR. It preserves observable effects, checks,
panic spans and managed lifetimes; an unused result does not justify removing
a potentially trapping or allocating operation. Pure scalar literals may move
to entry only when their unique definition and unchanged value are established.
Local copy facts expire on redefinition, and exact array/index proof operands
are not independently substituted. CFG simplification preserves reachable
effects and divergence. All MIR proofs are reverified before publication.
Resource failure leaves the original checked MIR intact; invalid transformed
MIR is a compiler failure. D087 fuel applies to supplied verified bytecode,
not to a stable instruction count for source code across compiler revisions.

## 5. C17 AOT contract

### 5.1 Generated C safety

Generated C must not depend on C undefined behavior to implement safe Marimay. In particular:

- checked signed arithmetic uses range checks or unsigned-domain techniques before conversion;
- verifier-proven D062 `i64` add/subtract/multiply uses unsigned-domain
  evaluation followed by portable two's-complement conversion and can execute
  only after the same interval proof is reconstructed;
- signed division and remainder implement D042 truncation-toward-zero and
  dividend-sign remainder rules, checking zero and `MIN / -1` or `MIN % -1`
  before evaluating any host C division or remainder operator;
- shifts validate their count, checked left shifts reject an unrepresentable
  result, and signed right shifts implement explicit sign extension;
- memory reads respect alignment and effective type, using `memcpy` where necessary;
- generated checked array access checks bounds before address arithmetic; a
  verifier-gated in-bounds access may address storage only through its exact
  checked capability and never across a call or mutation;
- enum payload access follows a checked tag edge in MIR;
- evaluation order is explicit through temporaries;
- integer-to-pointer operations exist only in unsafe FFI lowering;
- aliasing assumptions are conservative unless proven from MIR;
- default compiler flags preserve strict floating-point semantics.

For D045 indexed assignment, generated code evaluates the receiver and signed
index, validates `0 <= index < length`, and only then evaluates the right-hand
side or performs address arithmetic. Compound indexed assignment reads the old
element after the check and before its right-hand side. Bounds failure publishes
no destination or mutation and produces the exact `G4002` panic contract.

For D044 `f64`, a native target is conforming only when its `double` is IEC
60559/IEEE 754 binary64 and generated C preserves binary64
round-to-nearest-ties-to-even semantics without fast-math, reassociation,
contraction, excess precision, or locale-dependent literal conversion. The
backend rejects an unsupported host before publishing a native artifact; it
does not silently substitute host floating-point behavior. Native lowering
canonicalizes NaNs at every `f64` arithmetic result boundary to
`0x7ff8000000000000`, while unary negation is an exact sign-bit toggle.

The C backend test suite runs generated code with strong warnings, AddressSanitizer, UndefinedBehaviorSanitizer, and platform equivalents.

### 5.1.1 Generated identifiers

User-controlled Marimay identifiers are never copied verbatim into C identifiers.
The backend assigns every generated function, type, field accessor, local,
temporary, descriptor, and helper an injective compiler-owned spelling from
its stable symbol identity and ordinal. Generated spellings must be valid C17,
must not be C17 keywords, must not enter any implementation-reserved C
namespace (including double-underscore and underscore-plus-uppercase forms),
and must not collide with runtime helpers, C library external identifiers, or
one another after case-insensitive filesystem partitioning. Unicode spelling,
normalization distinctions, and source names that happen to be C keywords do
not alter this contract. Human-readable source names may appear only in
escaped comments or diagnostic/debug metadata and cannot determine linkage.

An explicit `extern "c"` declaration is different: its validated foreign link
name denotes the external ABI symbol rather than a generated C identifier.
The FFI specification and I9 gate own platform spelling, linkage, decoration,
and library-namespace validation; the native emitter must not silently mangle
that external contract.

### 5.2 ABI categories

The internal native ABI has two call categories:

1. **direct** — a statically known function proven not to suspend;
2. **resumable** — a function or dynamic call that may suspend and therefore exchanges a scheduler/frame result.

Interface calls and general closure calls use the resumable ABI so an open implementation set does not expose suspension in source types. A non-suspending implementation receives a generated adapter. Direct statically resolved calls remain ordinary C calls when safe.

Changing a function from direct to resumable changes package build metadata and forces dependent native recompilation, but is not a source API break because the declared Marimay signature is unchanged.

### 5.3 Rooting

Generated native functions register managed references in a precise shadow-root frame whenever they can allocate, call an allocating function, or suspend. Resumable function locals live in typed heap frames with generated trace descriptors. Root registration is balanced on every normal, error, panic, and cancellation edge.

Conservative scanning of arbitrary C stack bytes is not a conforming primary strategy.

### 5.4 Emitted C

`mmay build --emit c` may emit human-readable C for compiler debugging and bootstrap inspection. Symbol names, file partitioning, helper signatures, and formatting are unstable. User code must use the documented C FFI surface instead of including generated headers.

### 5.5 Native executable entry

D050's public executable adapter is distinct from the internal backend test
harness. It accepts only the D049/D066 checked `main` forms and calls only symbols
from the same canonical verified typed-MIR program. It grants D046 stdout,
prints no `ok:*` or `trap:*` envelope, and maps `void`, `Result`, panic, and
internal runtime outcomes to the same stdout bytes, stderr diagnostic identity,
and exit status as the VM contract.

The adapter strictly decodes host arguments into D066 UTF-8 values, constructs
the exact entry `[]string` when requested, and excludes the executable name.
The array and every string are ordinary managed values rooted before the root
call. A selected `int` status is range-checked after the call and all lexical
cleanup; it is never cast or truncated into a host exit type before validation.

For `Result.Err`, the adapter keeps both the returned `Result` and extracted
error payload in exact managed roots while it calls the checker-selected
`message(E): string`. It writes counted message bytes, including embedded NUL,
before popping roots. Root frames are balanced and the managed heap is disposed
on success, expected error, panic, allocation/resource failure, invalid runtime
layout, and host-rejection edges. A release optimizer may remove a check only
after proving it redundant under Marimay semantics.

### 5.6 Native runtime profiles

Under D052, debug and release native outputs link distinct version-matched
runtime archives. Debug runtime objects use `-O0` or `/Od`; release runtime
objects use `-O2` or `/O2`. These profile flags apply to every first-party
runtime translation unit independently of the build configuration of the
invoking compiler. Both profiles preserve the same checked arithmetic, bounds,
panic, floating-point, rooting, cleanup, and D046 behavior.

The toolchain validates the selected archive's manifest and embedded profile
and runtime-version markers before C emission. It hashes the archive contents
into the native cache key. An archive path is distribution metadata, not a
semantic cache input, so a complete installed toolchain remains relocatable.
Missing, stale, partial, or wrong-profile runtime artifacts are rejected before
host linking and cannot be reclassified as a generated-C failure.

## 6. Direct-style suspension

### 6.1 Source rule

A function's declared return type describes its completed result, not its execution container:

```marimay
fn loadUser(id: int): Result<User, Error> {
  const response = http.get($"/users/{id}")?;
  return json.decode<User>(response.body);
}
```

If `http.get` is not ready, only the current lightweight task is parked. The isolate scheduler continues other runnable tasks. There is no `async` declaration, `await` operator, `Future<T>`, or user-selectable executor.

Calling the same function without `spawn` remains sequential: the caller cannot continue until the call produces its declared result. `spawn` is the operation that introduces concurrent progress.

### 6.2 Effect inference

The compiler assigns each function an internal effect `maySuspend: bool`. It is true when a function directly invokes a suspending runtime operation, calls a suspending function, performs an open interface/closure call, waits on a task/channel/timer/isolate operation, or invokes `@blocking` FFI through the runtime adapter.

Inference is a least fixed point over strongly connected components of the call graph. Imported package metadata supplies effect summaries and causal source spans. Unsafe code cannot lie about the effect of an ordinary Marimay function.

The effect is not considered when checking source-level function compatibility. It is observable to tooling:

```text
mmay explain suspension loadDashboard

loadDashboard may suspend
└─ calls loadUser at src/dashboard.my:12
   └─ calls http.get at src/users.my:8
      └─ runtime network wait
```

Editors should mark suspension-capable calls in an optional gutter view. Diagnostics must show the effect path when a non-suspending boundary rejects a call.

### 6.3 Lowering

For the VM, the current typed bytecode frame contains the resumption instruction and live registers. Suspending transfers the frame from `running` to a wait queue without unwinding it.

For native AOT, the compiler transforms a suspending function into a generated heap-frame state machine. Each suspension point stores the next state, live typed locals, active cleanup stack, parent continuation, source span, task context, and precise GC descriptor.

Conceptually:

```c
switch (frame->state) {
case 0:
    frame->state = 1;
    return mmay_http_get_or_suspend(frame, request);
case 1:
    response = mmay_resume_value(frame);
    /* continue typed MIR */
}
```

Actual lowering is generated from MIR and must not expose a C stack address across suspension. Recursion allocates distinct resumable frames and therefore does not retain unbounded native C stack across a wait.

### 6.4 Non-suspending boundaries

Synchronous C callbacks, constant evaluation, GC tracing helpers, low-level signal bridges, and scheduler-critical runtime sections require a statically proven non-suspending call graph. Rejection includes the causal effect chain.

## 7. Tasks and structured concurrency

### 7.1 Terms

- A **task** is a lightweight resumable Marimay execution context.
- A **scope** is the lexical lifetime owner created by `scope`.
- A **child** is a task created by `spawn` in that scope.
- The **owner** is the task executing the `scope` block.
- A **safepoint** is a MIR location where scheduling, cancellation, or GC may occur.

Every program starts in a root task. Standard servers create request tasks under a server-owned structured scope. The runtime maintains a task tree; it does not expose a flat bag of detached work.

### 7.2 Creating tasks

```marimay
scope {
  const userTask = spawn loadUser(id);
  const ordersTask = spawn loadOrders(id);

  const user = userTask.wait()?;
  const orders = ordersTask.wait()?;

  return Ok(Dashboard { user, orders });
}
```

The callee expression and arguments of `spawn` are evaluated left to right in the owner before the child becomes runnable. Failure or panic during argument evaluation means no child was created. Once created, the child receives a stable monotonically increasing task identifier within its isolate.

`spawn call()` returns a compiler-known `Task<T>` where `T` is the completed return type of `call`. `Task<T>` is affine and scope-bound:

- it cannot be copied;
- `wait()` consumes the handle and returns `T`;
- it cannot be returned, stored in a managed object/container, sent through a channel, or crossed into another scope;
- it cannot be captured by a closure that may escape the scope;
- it may be left unused, in which case scope-exit rules still own the child.

These restrictions are checked statically without general ownership or lifetime syntax.

### 7.3 Scope exit

Before control can leave a `scope` block, all of its children are terminal. The rule applies to normal fallthrough, `return`, propagation by `?`, outward `break`/`continue`, panic, and owner cancellation.

On exit, the runtime:

1. requests cancellation of each unfinished child;
2. continues scheduling until those children acknowledge cancellation and complete cleanup;
3. releases task frames and scope resources;
4. resumes the original control transfer.

A task therefore cannot accidentally outlive the block that created it. Normal fallthrough does not mean detach; an operation intended to finish must be explicitly waited or placed under a longer scope owned by a server/supervisor API.

### 7.4 Waiting and child results

`task.wait()` suspends the owner when the child is not terminal. When the child:

- returns `T`, `wait()` produces that `T`;
- returns `Result<T,E>`, `wait()` produces that exact `Result<T,E>` and the caller may apply `?`;
- panics, the panic is re-raised in the owner with an attached child-task trace;
- is cancelled because its ancestor is leaving, no source result is produced because the owner is already following that exit path.

Expected failure is not automatically a task failure. A child returning `Err` is an ordinary completed value until the owner chooses to propagate it.

### 7.5 Child panic

A child panic marks its scope failed and requests cancellation of unfinished siblings. The owner observes it at its next wait, scheduler safepoint, or scope exit. Cleanup runs before the panic crosses the scope boundary.

When multiple children panic before cancellation is observed, the runtime reports one primary panic and attaches the others as suppressed task failures. Conformance tests use a deterministic scheduler and choose the smallest creation identifier as primary. Production scheduling may affect which failure is first; panic selection cannot be used as business logic.

### 7.6 Cancellation

Cancellation is a runtime control signal, not a catchable exception and not a `Result` variant injected into arbitrary signatures. It propagates from a scope to all descendant tasks.

The compiler inserts cancellation/scheduler polls at function entries on resumable paths, loop backedges, allocation slow paths, task/channel/timer operations, external blocking adapters, and explicit `task.yield()`.

Cancellation becomes effective at a safepoint. Between safepoints, safe code may run a bounded straight-line computation. Compiler transformations must preserve a finite poll path for every source loop unless the loop is proven finite and bounded.

During cancellation cleanup, further cancellation is masked while lexical `defer` calls and nested child joins run. Cleanup may suspend. Runtimes expose operational shutdown deadlines, but forced process termination is outside recoverable language semantics.

### 7.7 Timeouts and races

Timeout, structured first-result racing, and readiness selection are standard compiler-aware library operations, not syntax in 0.1:

```marimay
const result = task.withTimeout(2.seconds, (): Result<User, Error> => {
  return loadUser(id);
});
```

`withTimeout<T>` creates a nested scope and returns `Result<T, Timeout>`. On expiry it cancels and joins the nested work before returning `Err(Timeout)`.

Under D029, 1.0 also provides two distinct structured policies. A first-completion race owns its alternatives in a nested scope, returns the tagged winning result only after cancelling and joining losers, and propagates cleanup/panic according to the ordinary scope rules. A readiness selection registers a finite set of task-completion, bounded-channel, timer, or cancellation-aware I/O alternatives, returns one tagged ready case, and unregisters every losing wait before returning. It does not cancel a task merely because that independently owned task was not selected.

If multiple alternatives are ready at the same logical scheduler step, deterministic test mode selects the first declared alternative and records the selection event for VM/native replay. Production code cannot use incidental host poller order as business logic. Registration, winner commit, losing-registration removal, cancellation, and scope exit are one race-safe runtime protocol; no losing callback may resume a completed selection. Exact first-party API names are frozen by the I6 prototype, without adding `select` syntax to the 0.1 grammar. Ad-hoc detached races are not permitted.

### 7.8 Channels

`Channel<T>` is a typed bounded queue for tasks in one isolate. Sending and receiving may suspend. Closing a sender produces an explicit receive result rather than a sentinel. Sending after close returns a typed error; it does not panic. Unbounded channels are absent from the safe 1.0 API.

Channel endpoints must remain within a scope that owns all participating tasks. The compiler rejects an endpoint captured by work that could escape its owner.

### 7.9 Scheduling

Each isolate runs at most one of its Marimay tasks at a time. Scheduling is preemptive only at compiler-defined safepoints and cooperative between them. The required policy is fair: a continuously runnable task cannot be postponed forever by other runnable tasks.

The exact interleaving of independent tasks is not ordinary program semantics. Mutable shared objects inside one isolate are free of simultaneous data races but can still have order-dependent logical races. Programs requiring a deterministic order use channels, explicit waits, or a single owner task.

Conformance mode uses a deterministic scheduler keyed by task creation identifiers and a recorded event stream. VM and native differential tests execute against the same logical schedule.

## 8. Isolates and CPU parallelism

### 8.1 Heap isolation

An isolate owns one managed heap, scheduler/task tree, package runtime state, capability set, resource table, and logical event loop.

Different isolates may execute in parallel on different OS threads. Safe Marimay cannot hold a mutable reference into another isolate's heap. There are no user-visible shared-memory threads, mutexes, atomics, or data-race-prone global variables in 1.0.

### 8.2 Running CPU work

The core parallel API is a suspending standard operation:

```marimay
fn hashChunk(chunk: Bytes): Digest {
  return crypto.hash(chunk);
}

fn hashBoth(a: Bytes, b: Bytes): (Digest, Digest) {
  scope {
    const left = spawn isolate.run(a, hashChunk);
    const right = spawn isolate.run(b, hashChunk);

    return [left.wait(), right.wait()];
  }
}
```

`isolate.run(input, operation)` executes a statically named, non-capturing function in a runtime-managed worker isolate and suspends until its result returns. It composes with scoped `spawn`; users do not manage a detached OS thread pool.

The compiler checks that input and result are sendable. Sendable values are primitives, strings, immutable enums, and immutable record graphs whose reachable fields are sendable. Mutable records and containers are deep-copied as an independent snapshot when explicitly accepted by the API. Opaque resources are non-sendable unless their API defines transfer.

Selected buffer/resource types support explicit transfer. A transferred handle becomes statically unusable at the sender through a narrow compiler-known affine rule. This rule does not introduce general move semantics into Marimay values.

### 8.3 Service workers

Server libraries may run multiple isolated accept/request workers to use multiple CPU cores. Each worker constructs its own application state or receives an immutable configuration snapshot. Cross-worker coordination uses external services or isolate channels; a global mutable heap is not silently shared.

Operational worker counts are runtime configuration and must not change request-handler source types.

## 9. Managed memory

### 9.1 Required guarantees

Safe Marimay automatically manages records, strings, arrays, maps, closures, task frames, interface boxes, and runtime metadata. It prevents use-after-free, double-free, null dereference, uninitialized reads, and type confusion.

Objects retain stable reference identity for 1.0. The Stage 0 collector is a precise non-moving tracing collector. A later moving or generational collector may be introduced only if identity, FFI handles, suspension frames, and observable semantics remain unchanged.

### 9.2 Per-isolate collection

Collection stops tasks in one isolate, not the whole process. Roots include VM registers, native shadow-root frames, resumable task frames, cleanup stacks, package constants, runtime handles, channel buffers, pending I/O registrations, and explicit FFI handle scopes.

Root descriptors are precise. Cycles are collectible. Collection order and timing are not observable in safe source.

Runtime string values in both engines participate in this managed graph and
the D055 live-heap byte budget, including strings produced by interpolation,
concatenation, formatting, and text operations. Replacing a string value does
not retain every previous value until execution teardown: values no longer
reachable from roots are collectible before a heap-budget rejection. Copies
and container cells preserve immutable string values through managed
references. A cumulative count of discarded runtime string bytes must not
replace the configured live-heap budget. Encoded bytecode string constants
remain bounded artifact metadata, separate from runtime string objects.

Managed array objects are non-moving and participate in the same precise root
graph. An array of managed references traces each initialized element; scalar
array storage is not conservatively scanned. Array growth is failure-atomic,
and a collection attempt precedes a heap-limit outcome when reclaimable
objects could satisfy the operation. Capacity and collection timing remain
unobservable.

Under D053, pop/truncate/clear remove cells from the initialized prefix and
from precise tracing before a following collection can observe the array.
Reserve roots the array across collection/reallocation and is failure-atomic:
an unsuccessful operation changes neither cells, length, reserved storage, nor
mutation generation. Capacity remains unobservable and is never serialized in
portable bytecode.

Each array carries a non-wrapping internal mutation generation. VM and native
increment it only after a successful initialized-sequence mutation commits.
Array iteration captures it once and checks it before every later loop test or
yield. A mismatch produces the exact `G4005` panic contract. Iterators resolve
the array handle for each check/access and do not retain a raw cell pointer,
so a successful `reserve` neither invalidates them nor exposes relocation.

Under D059, generated C performs exact managed-array length and scalar-cell
access through the version-matched runtime array-access ABI. One ABI call
resolves the handle once and validates its immutable descriptor; checked mode
also observes the length and performs the signed D045 bounds comparison,
while in-bounds mode consumes a D058 verifier capability. An immediately
matching checked barrier may be fused into that access only when its G4002
span and evaluation point are preserved. A barrier separated from its
consumer remains observable at its original point. Neither mode retains a
resolved runtime pointer, length, or capacity after the call, and the caller's
shadow-root frame owns the array reference for the complete call.

Under D060, a verified raw scalar element type selects dense byte-addressed
array storage with an exact 1-, 2-, 4-, or 8-byte stride; reference elements
retain `MarimayManagedCell` stride. The runtime validates the descriptor tag,
stride, and raw/reference class before publishing or accessing storage. Raw
loads and stores use byte copies through aligned fixed-width locals, preserve
the canonical low payload bits, and are never traced as references. Newly
reserved storage is zero-initialized, only the initialized reference prefix is
traced, and live-byte accounting charges the exact reserved stride bytes.
Checked size arithmetic and budget preflight precede allocation, resizing, and
address calculation, and a failed operation leaves identity, contents, length,
capacity, mutation generation, and accounting unchanged. This internal layout
does not change the portable bytecode array-type table or expose stride or
capacity to source code.

Under D061, managed-array construction whose element count is known allocates
exactly `count * descriptor.stride` storage bytes and publishes capacity equal
to count. Unknown-size growth uses the single runtime capacity planner: zero
capacity starts at four and later capacity doubles, clamped once to the
configured maximum. `reserve` submits `length + additional` to that planner
once; a covered request performs no allocation. All checked planning and
stride arithmetic precede D055 budget preflight and host allocation, and a
collection-triggered retry re-resolves the rooted handle and recomputes the
same plan. VM and native use this identical path. These rules neither expose
capacity nor change the established budget bytes, allocation request, failure
precedence, mutation generation, or portable bytecode.

Under D063, generated native C may bind an exact array or record reference to
a non-escaping activation view after validating its managed handle and
immutable descriptor. Managed objects do not move. Array views retain field
locations for length, mutation generation, and the current storage pointer;
they do not retain the storage pointer value across growth. Record views retain
the fixed field-array location. Copying an exact reference copies its companion
view, while every other reference-producing definition clears it. Exact array
fields of a descriptor-checked record parameter may be bound eagerly and are
kept alive through the rooted record trace edge. Activations containing a call
or array-valued record write compare the current field identity before reuse;
call-free/read-only activations cannot replace that field and omit the guard.

A view is not a bounds proof. Checked native access still performs the signed
G4002 check at the original source span, and unchecked access still requires a
D058 verifier capability with the same invalidation rules. `RAW64` native
loads and stores may use C17 `memcpy` directly through the bound storage field;
stores preserve D053 generation saturation and commit order. Reference layouts
retain exact type validation, and nested aliases, collection, growth, iterator
G4005 behavior, panic identity, and VM observations are unchanged.

Under D104, a fully verified VM program may execute supported instructions in
a nonallocating region using the same runtime array-view contract. Complete
managed identity and expected layout are validated before view reuse. The
region cannot allocate, collect, call, suspend, perform I/O or invoke host code;
every view expires before returning to ordinary dispatch. Descriptor misses,
unsupported instructions and observable failures return at the original
instruction without register/array mutation or fuel consumption. Successful
instructions consume exactly the original D087 per-frame/shared fuel. The
ordinary dispatcher retains error construction, spans, cleanup and resource
classification; D053 generations and D058 bounds proofs are unchanged.
Scalar views may access every D060 storage width with host-order typed copies.
Narrow stores must validate the complete scalar bits before narrowing and
before modifying storage or the mutation generation. Boolean conversion is
canonical; floating-point access preserves the stored bits, including negative
zero and NaN payloads. These accesses introduce no allocation or safepoint.

Under D108, a VM invocation may prepare an internal execution image only after
the complete bytecode program passes verification. The image may flatten
instructions, resolve direct branch/call targets and compose verified opcode
sequences, but each operation remains mapped to its original block, instruction
and span. Composed execution consumes the exact original per-frame and shared
fuel and publishes effects in the same order. Insufficient fuel or failure
returns at the first uncommitted original instruction. Unsupported image shapes
use ordinary dispatch; central-allocation failure remains observable.

A nested call-free function may borrow its caller's linked managed roots only
when every reachable instruction is nonallocating and cannot collect, invoke a
host, suspend, call or grow managed storage. The caller retains every argument
for the whole activation. Entry frames always link their own roots. Cached views
must compare full managed identity before reuse and cannot turn a view into a
bounds, type, mutability or generation proof.

Under D105, one program invocation may reuse inactive VM activation storage
per call depth. Returning unlinks the callee's root frame after publishing its
result to the parent. Inactive storage never roots managed values; every managed
location is invalidated before arguments are installed and checked roots are
linked for reuse. Only the same verified function layout reuses a slot; a
different layout replaces it. Teardown releases all active and inactive storage
on success and failure. Call-depth/fuel limits, source values, panic/defer order
and D055 managed-heap accounting remain unchanged.

Under D076, `Bytes`, both bounded builders, `Bytes.Iterator`, and every owned
text collection are ordinary managed record/array/string graphs. A text or
binary operation that may construct an owned result is a native safepoint and
roots every live carrier, separator, builder, intermediate string, and result
array until publication. `Bytes` mutation delegates to the D053 array owner;
copy/from-array/to-array/slice and builder `finish` construct independent
storage, while ordinary record copies preserve alias identity. Limit checks
precede output construction, and a failed declared limit publishes no managed
prefix. Host allocation and managed-budget failures retain D055 classification
and do not become a source `BoundaryError`, `LimitError`, `SplitError`,
`BuildError`, or `Utf8Error`.

Generated native code links a precise shadow-root frame whenever the function
or any reachable direct callee may allocate, collect, suspend, dispatch through
an unknown interface, or otherwise reach a managed safepoint. A conservative
transitive proof may omit that frame only when no such event is reachable.
Normal builds keep small root-location tables in fixed activation storage;
deterministic allocation-injection builds preserve the allocating path and its
failure order. Portable static-inline checked/proven `i64` helpers may remove a
separately linked call but retain D035/D062 arithmetic and panic semantics.

### 9.3 Resource lifetime

Garbage collection is not resource management. Safe APIs for files, sockets, transactions, and foreign handles expose explicit `close`/`finish` operations intended for lexical `defer`. User-defined finalizers are not supported in 1.0. Internal emergency cleanup for leaked OS handles must be reported and cannot define correctness.

D077 file handles are execution-local resources. VM, MIR-oracle, and native
execution contexts own one bounded handle table and best-effort close every
still-open entry during complete teardown, including `Err`, panic, and internal
resource failure. Source code still owns correctness through explicit
`close`/`closeBestEffort`; teardown is a leak backstop and cannot convert or
hide the already selected source outcome.

### 9.4 Limits and allocation failure

Embedded VM runs may set heap, task, instruction, and wall-time limits.
Under D087 ordinary local `mmay run` and `mmay test` have no instruction-count
or elapsed-time cutoff in either engine. Their caller must interrupt a
nonterminating computation; CI timeouts are explicit host policy. This does
not remove memory, call-stack, capability, verification or allocation limits.
The bytecode embedding API retains its finite default fuel. `max_steps ==
SIZE_MAX` explicitly leaves per-frame instructions unmetered without counter
wraparound; a supplied shared instruction budget is still consumed exactly.
Finite budgets retain their exact exhaustion boundary; zero shared fuel
exhausts immediately and a zero per-frame maximum is an invalid API argument.

D055's
managed-heap byte budget counts published managed object payloads, reserved
array cell storage, and managed slot-table storage. It does not count compiler
or VM metadata, bytecode/native code, call stacks, capture buffers, or OS
resources, which remain bounded by their own owners. The default is exactly
`67108864` bytes per root execution/native process.

All accounting arithmetic is checked. `live + requested == limit` succeeds.
Before a budget result, the heap performs the D045/D053 collection attempt and
recomputes `live` and the net additional accounted `requested`; a final
`live + requested > limit` rejects the operation atomically without attempting
that host allocation. The result is `I0002`/exit `70` and records those three
byte counts without an address. A host allocator failure after successful
preflight is the distinct host-allocation `I0002`/exit `70`. Source-semantic
array growth checks precede both and retain `ArrayGrowthError.LengthLimit` or
`NegativeAdditional` rather than becoming execution failures.

Every test and isolate starts with a fresh counter and failure record. A future
child isolate inherits the configured ceiling value, not its parent's live
usage, unless an explicit isolate API selects another ceiling. Exceeding another
sandbox limit terminates the affected root task or isolate with a classified
runtime-limit diagnostic after bounded cleanup when possible. Process-level
out-of-memory that prevents runtime bookkeeping is fatal and not catchable.

## 10. I/O runtime

The standard runtime owns one portable I/O abstraction. It selects platform mechanisms internally (`epoll`, `kqueue`, IOCP, or a supported fallback). Packages do not choose competing executors.

Standard file, socket, DNS, timer, process, and HTTP APIs use direct-style signatures. When an operation cannot complete immediately, it registers interest, stores the current task continuation, and returns control to the scheduler.

Potentially blocking operations without a portable non-blocking primitive run on a bounded blocking pool. Saturation suspends callers rather than creating unbounded OS threads. Completion re-enters the owning isolate through its event queue; foreign worker threads never mutate the isolate heap directly.

### 10.1 Bounded typed standard streams

D075 supersedes D046's stdout-only preview. `std/io` owns three independent
program streams and exact capabilities: `readLine` uses `io.stdin`, output and
flush operations use either `io.stdout` or `io.stderr`, and tool/compiler
diagnostics are not part of any program stream. Each output stream is ordered;
no cross-stream merge order is promised. `print`/`eprint` write exact counted
UTF-8 bytes, `println`/`eprintln` append one LF, and none flushes implicitly.
The two explicit flush operations affect only their selected stream.

`readLine(limit)` accepts `0..1048576`, returns `Ok(None)` only for EOF before
bytes, returns an unterminated final line as `Some`, strips LF and a directly
preceding CR, retains NUL, and validates the complete returned bytes as UTF-8.
The byte limit is measured after terminator removal. Invalid signed limits win
over capability denial and return `LineTooLong` with the original value. No
read error publishes a partial source value.

D076 adds `readBytes(limit)`, `writeBytes`, and `ewriteBytes` to these same
streams. `readBytes` accepts `0..1048576`, reads exact bytes through EOF, and
returns an owned empty `Bytes` at immediate EOF. The first byte beyond the
limit returns `InputTooLong { limit }`; an invalid signed limit wins over
capability denial and carries the original value, and no prefix is published.
Binary output preserves embedded NUL and arbitrary byte values, selects exactly
the requested stdout/stderr capability, performs the same ordered short-write
and retry protocol, and does not flush implicitly. Text and binary calls on one
stream share one program order and the same test capture owner.

The Stage 0 blocking adapter completes positive short transfers in order and
allows at most 1024 consecutive zero-progress retries after the initial host
attempt. A write or flush failure can occur after a prefix became visible, but
the source error reveals neither a partial count nor host error text. On POSIX,
the CLI and generated executable suppress process termination by `SIGPIPE` so
a closed reader reaches this typed boundary. Internal allocation/resource
failure stays an `I0002` runtime outcome.

Until I6 parks I/O, this blocking adapter is permitted only while the bytes,
EOF, error, capability, retry-bound, and separate-stream observations remain
identical. It is a removal-gated implementation detail, not a second stream
semantics. The internal harness spelling `ok:f64:` followed by hexadecimal
bits is not program output.

### 10.2 Capability-confined files

D077 file operations share one closed runtime request model across the MIR
oracle, verified bytecode VM, and native C17. A request carries a normalized
typed path plus its exact read/write operation. The engine resolves relative
paths against the captured launch directory, performs a component-boundary
capability match, and only then invokes the symlink-free platform adapter.
Denied requests produce no target metadata/open/read/write/directory call.

Read handles consume partial transfers in order and allow no zero-progress
success. Bounded whole-file reads publish only after EOF and successful close;
input beyond the caller limit publishes no prefix. Atomic writes create one
exclusive sibling temporary, consume positive short writes, sync, close, and
atomically replace. A failure before replace leaves the previous destination
observable and attempts temporary removal. Directory results are fully owned
and unsigned-UTF-8 sorted before publication. The Stage 0 adapters may block;
I6 owns parking without changing this request/result/capability trace.

D101 adds `std/fs.readLine(file, limit)` over the same authorized handle.
It returns `Result<Option<string>, fs.Error>` with D075's LF/CRLF, lone-CR,
NUL, final-line, and empty-line rules. The inclusive `0..1048576` byte bound
applies after terminator removal to each line, independently of total file
size. `InvalidUtf8.offset` is relative to that line. Invalid limits, excessive
lines, and malformed text retain D077's existing `InvalidLimit`, `LimitExceeded`,
and `InvalidUtf8` variants. No failing line is published; consumed input is
not rewound. All aliases and mixtures of `read`/`readLine` share one cursor,
including bytes prefetched by the runtime. Closing releases the handle's
buffer. The public closed-File guard precedes the raw request as for `read`.

### 10.3 Environment and process context

D078 environment reads use one closed request carrying only the requested
source string. The runtime first validates the portable name, then matches the
named grant, and only then invokes the host or injected lookup adapter. Denial
therefore produces no host query. A successful adapter response is copied,
bounded to 1048576 bytes, and strictly UTF-8 validated before construction of
`Some(string)`; missing constructs `None`, while no failure publishes a value.
POSIX names are byte-exact. Windows name matching and duplicate detection fold
ASCII case only. Environment enumeration has no request operation.

D078 process-context reads use zero-argument current-directory and executable-
path requests. A fresh execution captures or injects their absolute UTF-8
spellings before source code runs. Each response must normalize as an absolute
D077 path before publication. Platform failures, malformed encoding, and
invalid paths remain closed typed errors and expose no platform code or text.
The VM, MIR oracle, and native runtime consume the same injected context in
differential tests. A real VM run and a real native artifact may have different
executable paths because those are different controlled process inputs, not an
engine-identity operation.

## 11. FFI interaction

### 11.1 Calls from Marimay to C

An ordinary extern call executes on the isolate's carrier thread and must not block or retain managed pointers after returning. `@blocking` dispatches the call through the bounded blocking pool with marshalled arguments.

Managed objects are not raw C pointers. FFI uses scoped handles, copied C strings/buffers, opaque pointers, or explicit borrowed views whose lifetime cannot cross a suspension point. The compiler rejects a live scoped C view at a possible suspension.

### 11.2 Calls from C to Marimay

A synchronous C callback may target only a function proven non-suspending. A foreign thread entering Marimay must attach through a runtime API and schedule work onto an isolate rather than directly accessing its heap.

For asynchronous C libraries, the callback records an opaque completion event and resumes a Marimay task through the runtime event queue. It does not call an arbitrary suspending frame on the foreign stack.

### 11.3 Unsafe containment

An FFI declaration and wrapper live in a package that declares the `ffi` capability. Packages containing unsafe code are reported by `mmay audit`. Unsafe operations cannot bypass bytecode task-scope verification or obtain engine identity.

## 12. Capabilities and sandboxing

The root application declares the ambient capabilities it requires, such as filesystem roots, network endpoints, environment names, subprocess execution, clock/randomness, dynamic FFI libraries, and resource ceilings.

Libraries cannot broaden the root set. Standard APIs check the current runtime capability context. Tests replace capabilities with deterministic in-memory implementations.

For D077, read and write roots are separate, repeatable, absolute normalized
component prefixes with a maximum of 64 roots per class and 4096 bytes per
path. A write root authorizes the same-directory temporary needed for atomic
publication but does not authorize source reads. Existing symbolic-link or
reparse-point components are rejected after the lexical capability match and
are never followed.

For D078, `env.read` grants are separate named capabilities with at most 64
portable names. The runtime holds only the accepted names, never an ambient
environment snapshot. No environment value changes compiler/runtime policy,
and an ungranted value cannot enter a semantic result, cache key, trace, or
diagnostic. Process current-directory and executable-path context is explicit
execution input but does not itself grant filesystem access.

For `.myb` execution, capabilities are denied unless granted by the invoking command or accepted root manifest. The VM verifier and runtime enforce them. Native binaries perform the same standard-library checks, but unsafe native FFI can escape process-level enforcement; production hard isolation also uses operating-system sandboxing.

Package build and constant evaluation have no ambient runtime capabilities and cannot access the network or filesystem outside declared source inputs.

## 13. VM/native equivalence

### 13.1 Observable equivalence

With the same source, dependency graph, language version, input capabilities, and controlled nondeterministic event stream, VM and native executions must agree on:

- returned value or root exit status;
- stdout/stderr bytes and program-ordered output;
- panic code, primary source location, and source frame sequence;
- integer and floating-point result bits;
- `match`, equality, string, array, and insertion-ordered map behavior;
- standard-library requests and responses;
- task creation tree, explicit waits, cancellation, and cleanup events;
- capability denials and resource-limit classification.

For D053 arrays this agreement includes `Option`/`Result` removal and sizing
cases, exact post-operation length/elements, mutation-generation invalidation,
removed-reference reachability, reserve failure atomicity, and `G4005` panic
identity.

For D076 text and bytes, agreement additionally includes exact UTF-8 validation
offsets, byte/rune/alpha-grapheme counts and partitions, substring boundaries,
split trailing empties, owned-copy versus alias mutation, builder snapshots and
limit failures, iterator invalidation, and binary stdin/stdout/stderr bytes.
The MIR oracle, verified bytecode VM, and generated C may not select different
normalization, locale, grapheme, host-string, or partial-publication behavior.

For D078 environment and process context, agreement additionally includes
missing versus empty values, the first invalid UTF-8 offset, limit and host-
failure classification, exact capability denial, requested-name trace,
Windows ASCII folding, and normalized current-directory/executable paths for
the same injected context.

For D079 maps and sets, agreement additionally includes exact key eligibility,
equality/hash coherence, collision behavior, insertion order across resize and
delete/reinsert, put/add insertion booleans, absence/removal results,
non-observable capacity and seed, mutation-generation changes, `G4005` panic
identity, and precise tracing of every live key and value. A different hash
seed may change only internal placement.

D058/D059 optimized accesses additionally preserve the same G4002 code,
primary span, observed failing index and length, result bits, alias-visible
mutation, and stale/wrong-descriptor rejection as the checked VM path. Link
mode, runtime archive profile, or availability of LTO cannot select a different
native safety boundary.

D060 dense raw storage additionally preserves exact integer/floating payload
bits, live-byte failure classification, reference tracing, mutation generation,
and VM/native observations. Storage tag, stride, padding, and backing addresses
are not safe-source observations and are absent from portable bytecode.

Addresses, GC timing, backend helper frames, native symbol names, throughput, and independent-task scheduling are not observable guarantees.

For the implemented D044 `f64` slice, comparison includes raw binary64 bits at
constant, copy, call-argument, call-result, return, and arithmetic-result
boundaries. Fixtures cover decimal nearest-even conversion, finite values,
subnormals, both zero signs, infinities, canonical NaN, IEEE divide-by-zero,
unary negation of raw NaN bits, and ordered/unordered comparison behavior. No
textual floating-point rendering protocol is implied by bitwise result
comparison. Public user stdout uses D046 `println` bytes and
`formatF32`/`formatF64` spellings; the internal `ok:f64:` hexadecimal harness
is not that contract and is not compared as user-visible output.

Construction traces preserve source field-initializer order even when MIR
stores the resulting values in declaration-layout order. Cleanup traces
preserve D041 LIFO order, primary/suppressed panic precedence, and the original
outcome when an infallible best-effort resource fallback runs.

String literal-pattern selection compares the complete decoded UTF-8 byte
sequence, including embedded zero bytes. Engines cannot delegate that operation
to a NUL-terminated C string API, locale collation, case folding, or Unicode
normalization.

D065 interpolation lowers to typed primitive-format and counted-string
concatenation operations. Each engine observes the same left-to-right,
once-only source evaluation and the same fragment boundary before proceeding
to the next expression. Boolean, integer, floating, parsing, and concatenation
helpers are locale-independent and length-aware. String-byte arithmetic is
checked before allocation; allocation or managed-budget failure exposes no
partial result. MIR, VM, and native comparison includes the exact resulting
bytes, including embedded NUL and non-ASCII UTF-8.

For an explicit source `panic(message)`, that observable tuple begins with code
`G4000`, the complete call-expression location, and the exact runtime string
value. MIR represents this as a typed panic terminator rather than a return or
backend-specific trap.

A call whose resolved result type is `never` is a typed terminating-call edge
in MIR. Its arguments retain ordinary left-to-right evaluation, but it has no
normal successor and produces no register value. An execution engine that
observes such a callee return normally must reject the program or artifact as
invalid rather than continue with a fabricated value.

### 13.2 Differential harness

`mmay test --engine both` runs each test in an isolated deterministic environment:

1. execute VM with a recorded scheduler/I/O/clock/random seed;
2. execute native against the same logical event recording;
3. normalize documented non-semantic metadata;
4. compare result, output, panic, capability trace, and task trace;
5. report the first semantic divergence with both MIR/backend locations.

Tests intentionally exercising real nondeterminism may opt out of byte-for-byte trace comparison but must assert properties independently on both engines. An opt-out cannot hide deterministic core-language coverage.

D051's checkpoint-11 implementation applies this contract to the currently
executable deterministic subset before the I6 scheduler, clock, random, and
temporary-filesystem surfaces exist. D077 extends the record with normalized
filesystem requests, structured results, and cleanup events. Its record is the exact termination
classification plus separate counted stdout and stderr byte streams. Every VM
test receives a new bytecode execution and managed heap; every native test is
a separately invoked process. Both executions consume the same verified typed
MIR symbol and fixed D046 capture grant. A native signal, exit-code difference,
stdout byte difference, or panic/stderr byte difference is `B0007`; matching
panics and expectation failures are ordinary test failures rather than
divergence. Later task/capability event fields extend this record without
weakening these byte-exact fields.

### 13.3 Compiler differential testing

The implementation suite additionally interprets unoptimized MIR as a reference oracle, compares optimized/unoptimized MIR, compares bytecode/generated C, varies supported C compilers and optimization levels, fuzz-generates well-typed programs, and minimizes divergences into checked fixtures.

## 14. Observability

The runtime exposes source-oriented task trees rather than raw backend frames. A task dump includes isolate/task identifiers, parent and children, state, wait reason, source suspension point, elapsed/CPU time, retained-frame summary, and capability/resource waits.

Native state-machine helpers and VM dispatch frames are hidden by default. Debuggers may reveal them in an implementation view.

Tracing uses a stable event schema for task start/end, scope enter/exit, suspension/resume, cancellation, panic, GC pause, blocking-pool wait, and external I/O. The schema includes engine only as diagnostic metadata; application logic cannot consume it as a capability.

## 15. Security invariants

The following are release blockers:

- malformed `.myb` causes memory corruption, verifier bypass, or partial execution;
- safe source reaches C undefined behavior;
- a managed pointer is retained by C beyond its handle lifetime;
- a task escapes its lexical scope;
- cancellation skips required cleanup;
- one isolate directly mutates another isolate's heap;
- VM and native disagree on a deterministic safe program;
- package build executes undeclared code or accesses undeclared network state;
- resource limits overflow counters or allocate unbounded metadata before rejection.

## 16. Required prototype gates before syntax freeze

The direct-style model is accepted in intent but must prove feasibility through these executable prototypes:

1. nested suspending calls across at least ten stack frames in VM and generated C;
2. recursion with live managed references across suspension;
3. interface and closure dispatch to suspending and non-suspending implementations;
4. `return`, `?`, panic, and cancellation through nested `defer` and task `scope` blocks;
5. non-blocking TCP echo and HTTP request fan-out with no carrier-thread blockage;
6. blocking C FFI dispatch with backpressure and cancellation;
7. deterministic VM/native task-trace comparison;
8. task dump and source stack reconstruction from native frames;
9. one million parked tasks within the published memory budget target;
10. isolate CPU parallelism with copy and transfer paths.

Failure of a gate requires revisiting the implementation or decisions D010/D012 explicitly. It must not be hidden by introducing public `async` types in only one backend.
