# Обязательные последующие checkpoints после preview 0.1-dev

Этот файл продолжает исторический план из `checkpoints.md`. Checkpoint 12
остаётся завершённым integration baseline; checkpoints 13–28 — обязательные
последующие этапы, обнаруженные при проверке реальных алгоритмов. Они не
переопределяют checkpoints 0–12 и не разрешают помечать roadmap milestone как
verified без всех уже установленных Tier 1 gates.

Каждый этап начинается с зелёного main. Перед изменением production/test owner
нужно перечитать его architecture contract и focused tests, выполнить
`./tools/check-architecture` и проверить фактическое число строк. Нельзя
добавлять feature logic в excepted/over-limit coordinator: сначала создаётся
узкий независимо компилируемый owner и регистрируется в CMake. Для любого нового
или изменённого allocation-capable C API обязательны cleanup на success и всех
существенных error paths, перебор каждого детерминированного fail-allocation
index и финальные `live_allocations == 0`, `live_bytes == 0`.

## Подтверждённая исходная evidence

- Все текущие Ruda/Go/Rust benchmark runs проверяются по одним и тем же
  checksum constants.
- Все три текущие DSU реализации используют path compression и union by rank.
- Последний отчёт называет `build/release/ruda` фактическим Ruda compiler.
- Harness сначала собирает программы, затем измеряет native executables;
  компиляция и `ruda run`/VM в native timing не входят.
- Timed Ruda corpus ещё содержит inline/package-local реализации и местами
  несопоставимый data layout; равенство checksum само по себе это не закрывает.
- Архивные debug-runtime результаты фиксируют конфигурационную ловушку и не
  являются release acceptance values.

## Issue-to-checkpoint traceability

У каждого inventory item ровно один primary checkpoint. Checkpoint 28 повторно
валидирует остальные результаты, но primary является только для явно
назначенных ему строк.

| Inventory item | Primary checkpoint | Почему это atomic outcome |
|---|---:|---|
| Output-profile/runtime-archive coherence | 13 | Один контракт выбора native artifact. |
| Cross-package `Array<T>` и `Array<Array<T>>` execution | 14 | Одна canonical instantiated-array identity через все слои. |
| Imported generic `Option`/`Result` constructors и enum results | 15 | Использует transport из 14, но имеет отдельный enum constructor/result path. |
| Receiver-directed collection names | 16 | Изолированное D003/D043 conformance исправление. |
| `pop`, `truncate`, `clear`, non-observable `reserve` | 17 | Один Array mutation/invalidation/failure contract. |
| `[]int`/`[][]int` как compact managed-array spelling | 18 | Все четыре ergonomics-пункта — одна canonical type-spelling семантика. |
| Явное отличие `[]T` от `Slice<T>` | 18 | Та же type-identity decision. |
| Formatter/diagnostic/IDE spelling и pre-1.0 migration | 18 | Неотделимо от canonical spelling; formatter-only slice запрещён. |
| Контекстное `let rows: [][]int = []` | 18 | Уже допустимая D045 construction через ту же type identity, без новой inference. |
| Configurable heap/isolate budget и actionable `I0002` | 19 | Отдельный execution/resource-policy contract. |
| Reusable-package и equivalent-work benchmark fidelity | 20 | Возможен только после correctness/usability 13–19. |
| Focused probes, metadata, noise envelope и baseline до budgets | 21 | Measurement infrastructure раньше speed acceptance. |
| MIR range facts и safe bounds-check elimination/hoisting | 22 | Одна proof-carrying MIR transformation для обоих backend. |
| Redundant type/handle/length/bounds work | 23 | Не устраняется безопасно без выбора generated-C/runtime boundary. |
| Measured narrow-fast-path или LTO decision | 23 | Одна native access boundary, без двух постоянных конкурирующих paths. |
| Dense native raw-scalar storage | 24 | Изолированное representation-only изменение после access path. |
| Non-observable capacity planning и exact/reserved construction | 25 | Опирается на публичный contract 17 и representation 24. |
| Nested-array traversal cost | 26 | Измеряется только после общих scalar primitives 22–25. |
| Proof-based checked-integer elimination | 27 | Переиспользует range facts 22, не меняя overflow semantics. |
| Final release-native Tier 1 rebaseline и regression policy | 28 | Post-optimization evidence, не локальные три M2 samples. |
| Полная reusable algorithm command matrix | 28 | Финальный запуск faithful corpus через все tools/engines. |
| Exact checksum/stdout/panic/exit equivalence | 28 | Финальный aggregate equivalence gate. |
| Allocator/sanitizer/architecture/generated/Tier 1 closure | 28 | Финальный aggregate integrity/evidence gate. |

Dependency graph направлен только вперёд: 13 исправляет release artifacts;
14–19 закрывают toolchain, package, collection, syntax и resource correctness;
20 делает workload честным; 21 устанавливает методику и budgets; 22–27 идут в
порядке MIR proof → scalar access → scalar storage → capacity → nested access →
checked arithmetic; 28 удаляет оставшиеся workarounds и выполняет rebaseline.

------

## 13. Согласованный release runtime для `ruda build`

**User-visible outcome:** `ruda build --release` детерминированно создаёт
настоящий release artifact или выдаёт точную ошибку; он не может молча
подлинковать debug runtime из build tree вызывающего compiler.

- **Зависимость и граница:** зависит только от checkpoint 12. Это correctness
  native-build artifact selection, не array optimization и не performance
  budget.
- **Normative authority / decision gate:** `toolchain.md` §6.3 и
  `execution.md` §5 задают output profile и runtime ownership. До code change
  принять decision: profile-specific shipped artifacts, explicit runtime
  rebuild contract либо точный profile-mismatch rejection. Decision также
  фиксирует installed-tree lookup, relocation и cache-key inputs.
- **Подтверждённое ограничение:** `native/build.c` передаёт release flags для
  generated C, но `RUDA_NATIVE_RUNTIME_ARCHIVE` вшит из CMake configuration
  самого `ruda`. Смена только `build/debug/ruda` на `build/release/ruda`
  существенно изменила timings. Это доказывает incoherence, но не общий speed
  ratio языка.
- **Scope:** runtime artifact discovery/validation, build-cache identity,
  build/install tree, GCC/Clang/MSVC command formation, precise diagnostics,
  no-network. Не входят LTO, array fast paths и source semantics.
- **Owners:** повторно проверить `bootstrap/src/native/build.c`, private build
  API/config, CMake runtime targets и `bootstrap/src/cli/cli_build*.c`. При
  смешении ответственности выделить новый native artifact-selection owner.
  Тесты — новый `tests/c-unit/test_cli_build_profiles.c`, а не broad aggregator.
- **Поведение и tests:** debug compiler + debug output, debug compiler +
  `--release`, release compiler + оба output profiles, installed relocation,
  stale/missing/wrong-profile archive, cache invalidation по runtime
  content/profile, `--emit c`, `--output`, paths with spaces. Test-only marker
  доказывает выбранный runtime; executable сохраняет VM stdout/stderr/exit/panic
  equivalence. Negative diagnostics не пропускают raw linker output. Все path/
  command allocations получают fail-index coverage и zero counters.
- **Focused acceptance:**

  ```sh
  cmake --build --preset debug --target ruda_c_unit --parallel
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cmake --build --preset release --target ruda_c_unit --parallel
  ctest --preset release -R '^ruda_c_unit$' --output-on-failure
  build/debug/ruda build tests/run/cp9 --release --output build/debug/cp13-release
  build/debug/cp13-release
  ```

- **Docs/evidence/removal:** обновить accepted decision, toolchain/native
  execution specs, build examples, roadmap и checkpoint-13 evidence с compiler
  path, runtime path/hash, flags, link classification и exit codes. Silent
  single-archive fallback удалить в этом checkpoint.
- **Completion:** один малый commit и немедленный push. Нельзя писать
  implemented/verified без build-tree и installed-tree evidence на всех Tier 1
  host/toolchains.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 14. Исполнение managed arrays между пакетами

**User-visible outcome:** public/private package functions принимают,
возвращают, создают, изменяют и вкладывают `Array<T>`/`Array<Array<T>>` с
одинаковым VM/native поведением.

- **Зависимость и граница:** зависит от 12 и 13. Закрывает transport canonical
  instantiated array type через package boundary; generic enums остаются в 15.
- **Authority / gate:** D002, D013, D043, D045 и normative types/generics/
  imports/typed-MIR/bytecode-array-table уже требуют это поведение. Это
  conformance defect, не новая семантика. Если обнаружится конфликт canonical
  type identity, сначала decision/spec amendment; package conversions запрещены.
- **Evidence:** dependency-ordered checker и exported surfaces существуют, но
  imported nominal resolution не всегда переносит generic arguments в
  executable layout identity. Checkpoint-12 покрывает scalar nominals, а
  reusable array packages check проходят и execution ломают.
- **Scope:** canonical qualified instantiated types, exported metadata, aliases,
  calls/returns/index/iteration, typed MIR layouts, bytecode array tables +
  encode/decode/verify, VM и native. Не входят compact spelling, imported enum
  constructors, новые Array APIs и optimization.
- **Owners:** проверить `checker_generic_types.c`,
  `checker_imported_nominal.c`, `checker_lookup_imports.c`, `package_graph.c`,
  focused MIR type/layout, `bytecode/mir_lower*.c`, array-table codec/verifier,
  VM nominal/array и native import emitters. Не добавлять family в near-ceiling
  `array.c`, `mir.c` и broad verifier; при необходимости создать package-
  instantiation owners. Tests: отдельные checker/MIR/bytecode/native imported-
  array owners.
- **Поведение и tests:** scalar/record/enum/generic/nested element types;
  direct/selective/aliased imports; public aliases; mutation через stable
  identity; length/index/push/iteration; re-export chains; одинаковые short names
  из разных packages; corrupt metadata. Bytecode обязательно encode → decode →
  verify → execute. Сравнить VM/native stdout, stderr, exit, G4002 message/span и
  mutations. Exhaust checker/metadata/MIR/bytecode/VM/native allocations.
- **Focused acceptance:**

  ```sh
  build/debug/ruda fmt tests/post-preview/cp14-imported-arrays --check
  build/debug/ruda check tests/post-preview/cp14-imported-arrays
  build/debug/ruda run tests/post-preview/cp14-imported-arrays
  build/debug/ruda test tests/post-preview/cp14-imported-arrays --engine both
  build/debug/ruda build tests/post-preview/cp14-imported-arrays --output build/debug/cp14-imported-arrays
  build/debug/cp14-imported-arrays
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** уточнить spec canonical instantiated package type,
  обновить examples/roadmap/evidence. Benchmark alias/lowering workaround
  удалить в 20 после перехода corpus на reusable packages.
- **Completion:** один vertical commit и push. Без bytecode round-trip,
  differential, fail-allocation, sanitizer и Tier 1 package evidence этап не
  verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 15. Конструкторы и результаты импортированных generic enums

**User-visible outcome:** imported `Option<T>`/`Result<T,E>` constructors,
patterns, returns и postfix `?` работают между packages без local wrapper enums.

- **Зависимость и граница:** зависит от canonical package-instantiation
  transport из 14. Enum variant lookup, payload substitution, exhaustiveness и
  result propagation — отдельный vertical path.
- **Authority / gate:** D013 и normative Option/Result/match/import/`?` уже
  требуют это. Никакой новой absence syntax или nominal erasure. Конфликт
  prelude ownership/import scope сначала фиксируется decision/spec.
- **Evidence:** local generic enum tests существуют; imported fixture проверяет
  внешнюю `Result` signature, но не ordinary imported constructors/returned enum
  values. Benchmark использует `IntOption`/`SearchResult` workaround.
- **Scope:** qualified/selective variants, explicit/inferred type args, imported
  returns, pattern payloads, exhaustiveness, `mapError`, `?`, MIR identity,
  bytecode nominal tables/provenance, VM/native/diagnostics. Generic
  specialization и новая syntax не входят.
- **Owners:** проверить prelude/generic/import lookup, enum/match checker, MIR
  enums/calls, bytecode nominal/provenance, native generic/nominal emitters.
  Создать focused imported-generic-enum test owners, не расширять aggregators.
- **Поведение и tests:** `Some`/`None`, `Ok`/`Err`, qualified/selective/re-export,
  nested `Result<Option<Array<T>>,E>`, public/private, success/error `?`, wrong
  arity/type, ambiguity, non-exhaustive match. Bytecode round-trip + verify;
  exact VM/native values/stdout/stderr/exit/panic. Exhaust lookup/substitution/
  nominal-table/decode/execution allocations и zero counters.
- **Focused acceptance:**

  ```sh
  build/debug/ruda fmt tests/post-preview/cp15-imported-results --check
  build/debug/ruda check tests/post-preview/cp15-imported-results
  build/debug/ruda run tests/post-preview/cp15-imported-results
  build/debug/ruda test tests/post-preview/cp15-imported-results --engine both
  build/debug/ruda build tests/post-preview/cp15-imported-results --output build/debug/cp15-imported-results
  build/debug/cp15-imported-results
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** уточнить normative import/generic text только при
  необходимости; обновить examples/roadmap/evidence. В 20 удалить
  `IntOption`/`SearchResult` и эквивалентные wrappers.
- **Completion:** один vertical commit и push. Imported negative tests,
  bytecode verification, allocator exhaustion, engine differential и Tier 1
  evidence обязательны.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 16. Разрешение имён коллекций по типу receiver

**User-visible outcome:** ordinary nominal `push(stack, value)`/`pop(stack)`
идентичны `stack.push(value)`/`stack.pop()`, даже если prelude содержит
compiler-known Array operations с теми же именами.

- **Зависимость и граница:** зависит от 12 и выполняется после 15. Это D003/D043
  name-resolution conformance, не special methods и не новый Array API.
- **Authority / gate:** D003/D043 требуют exact-owned-nominal receiver и import-
  form invariance. Compiler-known prelude не получает reserved precedence,
  противоречащий этой модели.
- **Evidence:** Array recognition находится в specialized checker paths;
  benchmark переименовал stack/heap operations из-за collision `push`/`pop`.
- **Scope:** receiver candidate collection, prelude/package ordering, free/dot
  symbol identity, import forms, interface dot-call sets, diagnostics,
  occurrence/completion facts. Не входят general overloads, extension methods и
  semantics Array `pop` из 17.
- **Owners:** проверить checker lookup/import/resolution/dot-call/interface и
  compiler-service facts. Если lookup coordinator смешивает responsibilities —
  выделить receiver-candidate owner. Tests — отдельный receiver-collection-name
  owner и service fixture.
- **Поведение и tests:** nominal Stack/Queue/Heap functions `push`, `pop`,
  `length`, `clear`; same-package и все imports; Array prelude calls; interface
  satisfaction; hidden/wrong/ambiguous/approximate receiver diagnostics. VM/
  native fixture доказывает execution выбранного symbol. Exhaust candidate и
  diagnostic allocations.
- **Focused acceptance:**

  ```sh
  build/debug/ruda fmt tests/post-preview/cp16-receiver-names --check
  build/debug/ruda check tests/post-preview/cp16-receiver-names
  build/debug/ruda test tests/post-preview/cp16-receiver-names --engine both
  build/debug/ruda build tests/post-preview/cp16-receiver-names --output build/debug/cp16-receiver-names
  build/debug/cp16-receiver-names
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** уточнить implementation/spec candidate precedence,
  не меняя D003/D043; обновить examples/roadmap/evidence. В 20 удалить
  benchmark-only `add`/`take`/`insert`/`removeMinimum` names.
- **Completion:** один conformance commit и push. Import invariance, facts,
  interfaces, diagnostics, fail-allocation, engines и Tier 1 обязательны.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 17. Исполняемые операции размера и capacity для Array

**User-visible outcome:** stack, queue и heap используют ordinary free/dot-call
`pop`, `truncate`, `clear`, `reserve` без shadow logical size и устаревших
initialized elements; capacity остаётся ненаблюдаемой.

- **Зависимость и граница:** зависит от 16. Это один Array mutation surface;
  type shorthand и heap configuration остаются отдельными.
- **Authority / decision gate:** до code принять amendment D045 или новую
  collection decision и обновить language/stdlib/execution. Зафиксировать exact
  signatures/cases: empty pop, invalid/expanding truncate, negative/too-large
  reserve, `ArrayGrowthError`, return type removal, destruction of removed
  reference elements, generation increments и failure atomicity. Нельзя просто
  копировать Rust/Go semantics.
- **Evidence:** D045 сейчас задаёт construction/length/index/set/iteration/push.
  Structures сохраняют старые cells и отдельный logical size; runtime capacity
  растёт геометрически без public reserve contract.
- **Scope:** prelude + dot calls, checker generics, typed MIR, bytecode codec/
  verifier, VM/native, tracing/root release, iterator invalidation, diagnostics,
  examples. Не входят capacity getter, slices, deque, implicit shrink и
  optimization beyond specified operation.
- **Owners:** проверить `checker_prelude.c`, `array.c`, focused MIR/bytecode/VM/
  native array owners, `managed.c`, `managed_internal.c`, runtime `array.h`.
  `array.c` уже велик — новая checking logic идёт в focused owner. Runtime
  mutation также выделить из managed coordinator. Создать focused tests на
  каждом затронутом слое.
- **Поведение и tests:** raw/reference/nested arrays; empty/one/partial/all;
  reserve below/equal/above; no-op; `int::MAX`/size overflow; active iterator;
  alias identity; reference reachability; D040 order; неизменный G4002.
  Malformed bytecode rejection и round-trip. Exact VM/native state/stdout/
  stderr/exit/panic. Все allocation/resize failure indexes, atomic reserve и
  zero counters.
- **Focused acceptance:**

  ```sh
  build/debug/ruda fmt tests/post-preview/cp17-array-mutation --check
  build/debug/ruda check tests/post-preview/cp17-array-mutation
  build/debug/ruda test tests/post-preview/cp17-array-mutation --engine both
  build/debug/ruda build tests/post-preview/cp17-array-mutation --output build/debug/cp17-array-mutation
  build/debug/cp17-array-mutation
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** decision, normative specs, generated artifacts,
  formatter fixtures, examples, roadmap, checkpoint-17 evidence. Shadow-size/
  stale-cell benchmark workarounds удалить в 20; compiler-known name bypass не
  должен пережить 17.
- **Completion:** один vertical API/engine commit и push. Любая незакрытая
  signature decision, cleanup path, allocation index, engine, sanitizer или
  Tier 1 gate запрещает implemented/verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```
------

## 18. Каноническое написание `[]T` для managed arrays

**User-visible outcome:** `[]int` и `[][]int` становятся canonical source,
formatter, diagnostic и IDE spelling существующих `Array<int>` и
`Array<Array<int>>`; работает `let rows: [][]int = []`.

- **Зависимость и граница:** зависит от 14 и 17. Один semantic vertical change;
  запрещено разносить parser, checker, formatter и migration по отдельным
  checkpoints.
- **Authority / decision gate:** proposal non-normative. До code принять новую
  decision или amendment D045 и обновить language/stdlib/diagnostics/tooling.
  Плановый end state — pre-1.0 replacement: ссылки, разрешённые как prelude
  managed array, пишутся `[]T`; prelude `Array<T>` получает targeted migration
  diagnostic и не остаётся вторым permanent spelling. User/package nominal type
  с именем `Array` остаётся ordinary. Если будет выбран one-preview window,
  перед 28 нужно вставить отдельный numbered removal checkpoint.
- **Evidence:** `[T, U]` уже tuple type syntax, но empty tuple type `[]`
  недопустим, поэтому prefix `[]T` различим в type position. Expression `[]`
  уже contextual array literal. `Slice<T>` — отдельный non-owning scope-bounded
  view и не получает managed identity/spelling. Formatter coordinator почти
  достиг своего path ceiling.
- **Scope:** nested/qualified/generic/tuple/function element grammar и recovery;
  exact existing managed-array identity; aliases/imports; contextual empty
  literal; formatter bytes; diagnostics/fixes; service hover/signature/
  completion; repository migration; VM/native proof отсутствия layout change.
  Не входят Slice conversion, `T?`, postfix array syntax, новая inference,
  runtime/ABI tag или capacity semantics.
- **Owners:** проверить parser type owners, generic checker/imports, formatter
  collect/layout, diagnostics и compiler-service display. До logic в near-limit
  `formatter.c` создать focused type-format owner. Не дублировать semantic Array
  identity в AST. Добавить focused parser/checker/service tests, formatter
  fixtures и engine differential.
- **Поведение и tests:** `[]int`, `[][]int`, `[]pkg.Edge`, `Option<[]int>`,
  `Result<[][]Edge,E>`, tuple/function/generic elements, aliases/import forms,
  whitespace/comments. Deterministic recovery для `[]`, `[][]`, `[][`,
  incomplete generic. `Slice<T>` остаётся distinct. Old resolved prelude form
  отклоняется с exact migration diagnostic, unrelated user `Array` проходит.
  Formatter idempotence, canonical IDE output, VM/native identity/mutation/
  stdout/exit/G4002 span и allocation failures обязательны.
- **Focused acceptance:**

  ```sh
  build/debug/ruda fmt tests/post-preview/cp18-array-spelling --check
  build/debug/ruda check tests/post-preview/cp18-array-spelling
  build/debug/ruda test tests/post-preview/cp18-array-spelling --engine both
  build/debug/ruda build tests/post-preview/cp18-array-spelling --output build/debug/cp18-array-spelling
  build/debug/cp18-array-spelling
  build/debug/ruda fmt tests/format --check
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** accepted decision, все normative texts, grammar/
  diagnostic/generated artifacts, formatter fixtures, IDE rules, migrated
  examples/source, roadmap/evidence. При плановом replacement prelude
  `Array<T>` source compatibility удаляется в 18; internal C/runtime name
  `Array` допустимо оставить.
- **Completion:** один semantic commit и push. Без принятой migration decision,
  syntax/import/formatter/diagnostic/IDE/differential/allocation/Tier 1 evidence
  этап не implemented/verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 19. Настраиваемые managed-heap limits и точный `I0002`

**User-visible outcome:** пользователь задаёт документированный per-VM/native-
isolate live-byte budget, а failure различает semantic array length,
configured budget и host allocation exhaustion со стабильными diagnostics и
exit behavior.

- **Зависимость и граница:** зависит от 17, потому что reserve/growth обязаны
  использовать одну resource classification. Это correctness/resource policy
  до benchmark fidelity и performance budgets.
- **Authority / decision gate:** D045 уже отделяет
  `ArrayGrowthError.LengthLimit`; execution/toolchain задают resource limits и
  exit 70, diagnostics — `I0002`. Принять amendment с configuration surface и
  precedence. Конкретная цель: `--max-heap-bytes <positive-int>` для `run`,
  `test`, `build`; build встраивает default в executable. Зафиксировать units,
  min/max, duplicates, test isolation и future isolate inheritance. Hidden
  environment-only override запрещён.
- **Evidence:** runtime default — 64 MiB, capacity растёт геометрически. При
  16-byte cells рост к пяти миллионам требует 128 MiB allocation и попадает в
  broad `I0002`; это не `int::MAX` и не доказательство правильности default.
- **Scope:** CLI config propagation, VM options, native emitted setup,
  deterministic accounting, три stable classifications, structured diagnostics
  и exact exit codes. Budget diagnostic указывает live/requested/limit без host
  addresses; host allocation failure distinct. Не входят новый default из
  benchmark, capacity optimization, OS hard limits и GC redesign.
- **Owners:** проверить CLI run/test/build options/reports, `execution_heap.c`,
  bytecode VM options, native setup, runtime `heap.h`, managed accounting и
  diagnostic generation. Создать focused config/reporting owners до роста
  coordinators; отдельные CLI/VM/native/accounting tests.
- **Поведение и tests:** unchanged default; exact boundary; invalid zero/
  negative/overflow/non-number/repeated options; isolated tests; nested/raw/
  reference arrays; reserve/push; cleanup. Semantic length возвращает
  `ArrayGrowthError.LengthLimit`; budget emits specific `I0002` + exit 70; host
  allocator failure — distinct `I0002` + exit 70. Exact VM/native stderr/exit/
  class. Exhaust config/report/heap allocations и zero counters.
- **Focused acceptance:**

  ```sh
  build/debug/ruda run tests/post-preview/cp19-resource-limits --max-heap-bytes 1048576
  build/debug/ruda test tests/post-preview/cp19-resource-limits --engine both --max-heap-bytes 1048576
  build/debug/ruda build tests/post-preview/cp19-resource-limits --max-heap-bytes 1048576 --output build/debug/cp19-resource-limits
  build/debug/cp19-resource-limits
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  ```

- **Docs/evidence/removal:** decision, execution/toolchain/diagnostics/stdlib
  specs, CLI help/examples, roadmap/evidence с exact stderr/JSON/exits. Generic
  “resource, integrity, or host requirement” fallback удалить для этих cases;
  broad I0002 оставить только для действительно unclassified paths.
- **Completion:** один resource-policy commit и push. Без accepted semantics,
  negative diagnostics, VM/native parity, allocator failures, sanitizers и
  Tier 1 evidence этап не закрыт.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 20. Эквивалентный reusable algorithm corpus

**User-visible outcome:** timed и correctness Ruda corpus вызывает reusable
sorting/stack/queue/heap/DSU/BFS/Dijkstra packages и выполняет algorithmic work/
data layout, эквивалентные Go/Rust corpus.

- **Зависимость и граница:** зависит от всех 13–19. Меняет evidence workload,
  не compiler semantics, и завершается до performance budget.
- **Authority / gate:** normative import/collection contracts и test strategy.
  До изменений review machine-readable equivalence manifest: operations, input
  generator, checks, representation, checksum. Если equivalence требует новой
  semantics сверх 13–19 — добавить новый semantic checkpoint, не ослаблять
  workload другого языка.
- **Evidence:** checksums и DSU совпадают, но timed Ruda inlines algorithms;
  Go/Rust вызывают packages. Ruda stack/queue/heap используют inline state,
  Dijkstra — parallel target/weight arrays вместо edge records; validation work
  местами различается.
- **Scope:** переключить timed entrypoints на reusable Ruda APIs; выровнять
  structures/graph layout и checks; сохранить LCG, sizes/counts, Dijkstra
  `O(V^2)`, checksums и stdout line; добавить equivalence manifest и main-repo
  regressions. Не входят timings budgets, algorithm substitutions, tuning и
  compiler optimization.
- **Owners:** main repo — focused post-preview package/CLI fixtures. Sibling —
  reusable Ruda packages, benchmark entrypoints, `verify.sh`, `run.py`, Go/Rust
  sources. `benchmarks/ruda/core/benchmarks.ruda` только orchestrates package
  owners, не становится semantic aggregator.
- **Поведение и tests:** все 8 algorithms через imports, public aliases,
  Option/Result, dot calls, nested/record arrays. `fmt`, `check`, VM run/test,
  debug/release native, `--engine both` дают exact bytes. Manifest проверяет
  workload/checksum constants. Negative invalid input либо эквивалентен, либо
  одинаково исключён. Timing acceptance в этом этапе отсутствует.
- **Focused acceptance:**

  ```sh
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/debug/ruda" ./verify.sh
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/run.py --languages ruda,go,rust --samples 1 --warmups 0 --output benchmarks/results/cp20-fidelity-smoke.md
  (cd go && go test ./...)
  cargo test --manifest-path rust/Cargo.toml
  ```

- **Docs/evidence/removal:** sibling methodology/manifest/raw smoke и main
  examples/roadmap/evidence с обоими SHAs. Удалить timed inline copies,
  `IntOption`/`SearchResult`, shadow sizes, renamed operations, parallel-edge
  layout и package bypass. Любой untimed adapter перечислить exact path с
  removal checkpoint 28.
- **Completion:** по одному малому linked commit в каждом изменённом repo,
  немедленно push, записать SHAs. Все 8 workloads должны реально использовать
  reusable surface; speed claim запрещён.
- **Полный validation tail в Ruda repo:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 21. Воспроизводимый Tier 1 performance baseline для arrays

**User-visible outcome:** focused array probes и faithful algorithm corpus имеют
Tier 1 raw samples, metadata, noise envelopes и reviewed workload-specific
regression budgets до начала optimizations.

- **Зависимость и граница:** зависит от 13, 19, 20. Только measurement
  infrastructure/evidence — никакой optimization или semantic change.
- **Authority / decision gate:** test strategy и Tier 1 roadmap. Принять
  measurement protocol: host isolation, warmups/samples/order, timeout,
  compiler/runtime hashes, generated-C compiler/linker versions/flags, resource
  budget, raw schema, median, MAD/IQR, retry и regression adjudication. Numeric
  budgets выводятся из Tier 1 noise envelope; три M2 samples и debug archive в
  hard thresholds не входят.
- **Evidence:** current report хранит raw min/max и compiler path, но имеет один
  host/3 samples и не изолирует access/growth/storage/nested/checked costs.
- **Scope:** deterministic probes scalar get/set, proven/unproven bounds,
  growth/reserve, density, nested traversal, checked loops; faithful native
  corpus; schema validation; hashes; Tier 1 baseline/budgets. Не входят
  cross-language ranking, universal claims и optimization.
- **Owners:** focused tools/schema tests в sibling `benchmarks/`, не раздувать
  `run.py`. Main native-build tests предоставляют artifact identity без
  unstable public flag.
- **Поведение и tests:** reject wrong profile, checksum drift, insufficient
  samples, incomplete metadata, non-finite data, incompatible baseline. Rotate
  order; publish every sample/statistic. Separate budgets per workload/platform.
- **Focused acceptance на каждом Tier 1 runner:**

  ```sh
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --samples 11 --warmups 3 --output benchmarks/results/cp21-probes-PLATFORM.json
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/run.py --languages ruda,go,rust --samples 11 --warmups 3 --output benchmarks/results/cp21-algorithms-PLATFORM.md
  ./benchmarks/validate_results.py benchmarks/results/cp21-probes-PLATFORM.json benchmarks/results/cp21-algorithms-PLATFORM.json
  ```

- **Docs/evidence/removal:** protocol, schemas, raw samples, medians,
  dispersion/noise envelope, artifact paths/hashes, tools, reviewed budgets,
  roadmap/evidence. Temporary instrumentation удалить; оставшийся test-only
  counter должен иметь checkpoint-28 removal.
- **Completion:** linked commits/evidence сразу push. Пока нет всех Tier 1
  baselines/review, checkpoint не complete и 22–27 не начинаются.
- **Полный validation tail в Ruda repo:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 22. Проверяемые MIR range facts и bounds checks

**User-visible outcome:** typed MIR хранит verified range facts и безопасно
eliminate/hoist повторные array bounds checks, сохраняя observable panic и
evaluation order в VM/native.

- **Зависимость и граница:** зависит от 17 и 21. Shrink mutations уже известны,
  measurement/budgets приняты. MIR analysis идёт раньше backend shortcuts.
- **Authority / decision gate:** D033, D040, D045, language indexing,
  execution equivalence и G4002 неизменны. Принять internal MIR decision:
  lattice, dominance/proof tokens, mutation/call invalidation, loop fixpoint,
  verifier и allocation-failure fallback. При отсутствии proof всегда остаётся
  checked MIR.
- **Evidence/hypothesis:** каждый current access проверяет bounds, counted loops
  дают вероятные facts. Материальность overhead доказывает probe 21. Current
  `execution.c` ровно на hard ceiling 1500, `mir.c` почти на coordinator ceiling;
  feature logic туда запрещена.
- **Scope:** новые focused range/optimizer owners; explicit bounds-check +
  in-bounds access; alias/call/push/pop/truncate/clear invalidation; overflow-
  aware comparisons; verifier; bytecode encode/decode/verify; VM/native lowering;
  deterministic fallback. Не входят storage layout, raw-pointer caching и
  general checked arithmetic elimination.
- **Owners:** сначала `execution.c` делегирует existing array execution новому
  independently compiled owner и уменьшается без exception. Создать MIR fact/
  pass, focused verifier/bytecode/VM/emitter owners; не добавлять в near-limit
  files. Отдельные range/proof/allocation tests.
- **Поведение и tests:** counted/nested/affine loops, literal lengths;
  negative/overflow/uncertain остаются checked. Facts kill на alias/call/
  mutation/join/exit. Первый failing full `array[index]` span сохраняется.
  Corrupt proof/bytecode rejected before execution. Optimized/unoptimized MIR,
  VM, O0/O2 native exact differential. Exhaust pass/verifier/serialization
  allocations и prove checked fallback + zero counters.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe bounds --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp22-bounds-PLATFORM.json
  ./benchmarks/validate_results.py benchmarks/results/cp22-bounds-PLATFORM.json
  ```

- **Docs/evidence/removal:** MIR decision/architecture, execution/bytecode spec
  при serialized proof, generated opcode, roadmap/evidence с dynamic check
  counts и budget comparison. Test-only force-proof удалить; documented internal
  optimizer-disable test mode допустим.
- **Completion:** один semantics-preserving optimizer commit и push. Без
  malformed-proof rejection, full differential, allocation fallback,
  architecture, performance evidence, sanitizers и Tier 1 — не verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 23. Измеренная граница native array access

**User-visible outcome:** hot exact scalar-array access проходит одну
документированную generated-C/runtime boundary, а не повторяет managed resolve,
descriptor, length, bounds, status и cell conversion.

- **Зависимость и граница:** зависит от 21 и 22. Range proof остаётся в MIR;
  этот этап выбирает native ABI/optimization boundary и убирает duplicate work.
  Два inventory items объединены, потому что безопасно устранить дублирование
  при неопределённой cross-archive boundary невозможно.
- **Authority / decision gate:** D040/D045, native equivalence, portable C17,
  stable managed identity и root lifetime неизменны. По probes 21 сравнить узкий
  typed access API с поддерживаемым LTO path на каждом Tier 1 compiler; принять
  ровно одну internal decision. Нельзя оставить случайный profile/host-dependent
  выбор.
- **Evidence/hypothesis:** emitter сначала проверяет exact descriptor/length,
  затем runtime get/set снова resolves/checks/maps `RudaManagedCell`. Runtime —
  отдельный archive, обычный generated-C `-O2` не inline через него. Sequence
  подтверждён; его isolated timing contribution ещё hypothesis.
- **Scope:** один internal exact typed checked/proof-in-bounds API; single
  resolve/descriptor validation на access или verifier-safe region; status/
  panic mapping; alias/growth/collection invalidation; root lifetime; C17 ABI;
  O0/O2 и selected link mode. Не входят source pointers, capacity exposure,
  dense storage и nested specialization.
- **Owners:** проверить `c_emitter*.c`, runtime `array.h`, managed private owners
  и native link config. Предпочесть новый runtime array-access translation unit;
  public header ≤160 lines. Добавить focused runtime-access/native differential
  tests, не расширять near-limit array aggregator.
- **Поведение и tests:** raw scalar + reference regressions; negative/upper
  index; wrong/stale handle/descriptor; aliases; mutations и safepoints;
  source spans. Test counters/generated-C inspection подтверждают one surviving
  check. Exact optimized/unoptimized MIR, VM, native O0/O2/link-mode stdout/
  stderr/exit/G4002/overflow/mutation differential. Новые allocation paths —
  полный fail-index и zero counters.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe scalar-access --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp23-scalar-access-PLATFORM.json
  ./benchmarks/validate_results.py benchmarks/results/cp23-scalar-access-PLATFORM.json
  ```

- **Docs/evidence/removal:** internal boundary decision, native/runtime ABI
  docs, generated declarations, roadmap/evidence с measurements обоих
  prototypes, operation counts, code size/link data и budget comparison.
  Проигравший prototype и old duplicate helper chain удалить до completion.
- **Completion:** один focused commit и push. Если Tier 1 compiler не
  поддерживает выбранный contract, differential не зелёный или accepted budget
  не выполнен — этап не implemented/verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 24. Плотное native-хранилище raw scalar arrays

**User-visible outcome:** native raw-scalar arrays используют descriptor-
selected dense stride вместо reference-width union cells, сохраняя source
identity, tracing, failures и VM behavior.

- **Зависимость и граница:** зависит от 21 и 23. Access boundary стабилизируется
  раньше internal storage; capacity и nested references остаются отдельными.
- **Authority / decision gate:** D045 non-moving identity, execution tracing/
  accounting, bytecode array tables и VM/native equivalence неизменны. Принять
  internal layout/ABI decision: список raw scalar kinds, width/alignment/bits,
  descriptor tag/stride, zero-init, compatibility и отсутствие bundle change.
  Это не source-level D013 specialization.
- **Evidence/hypothesis:** на measured 64-bit target `RudaManagedCell` 16 bytes,
  raw64 — 8; collector уже различает raw/reference kinds. Cache density и
  capacity improvement вероятны, но подтверждаются probes 21.
- **Scope:** native allocation/access/copy/growth raw descriptors; checked size/
  alignment math; tracing skips raw payload; exact live-byte accounting;
  f64/integer bit preservation; portable C17; reference fallback. Не входят VM
  encoding change, packed bool, vectorization, public stride/capacity, moving.
- **Owners:** runtime core/array/heap headers, private managed representation,
  growth/trace, emitter conversions, bytecode descriptor mapping. Создать
  focused managed-array-storage owner вместо monolith `managed.c`. Добавить
  layout assertions и raw-storage/tracing/native/fail-allocation tests.
- **Поведение и tests:** все raw scalar min/max/bits, f64 `-0.0`/NaN payload,
  alignment/odd lengths, empty/large, growth/reserve, set/get, alias identity,
  GC, mixed nested arrays. Corrupt descriptor/stride rejected, bytecode
  unchanged, exact VM/native outputs/panics/exits. Exhaust allocation/resize,
  prove atomicity/accounting и zero counters.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe scalar-storage --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp24-scalar-storage-PLATFORM.json
  ./benchmarks/validate_results.py benchmarks/results/cp24-scalar-storage-PLATFORM.json
  ```

- **Docs/evidence/removal:** runtime layout decision, execution/ABI/accounting
  docs, roadmap/evidence с stride assertions, max successful elements при том же
  budget, raw samples/dispersion/comparison. Raw-scalar union compatibility path
  удалить; union оставить только для descriptors, которым он нужен.
- **Completion:** один representation commit и push. Bit preservation, tracing,
  accounting, failure, sanitizer, differential и Tier 1 measurements обязательны.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 25. Планирование exact/reserved capacity для arrays

**User-visible outcome:** known-size construction выделяет exact internal
storage, а `reserve` планирует non-observable capacity без повторных geometric
reallocations и изменения source behavior.

- **Зависимость и граница:** зависит от 17, 21, 24. Public reserve contract и
  final scalar stride должны существовать раньше capacity math. Resource budget
  и capacity observability не меняются.
- **Authority / decision gate:** D040/D045 и decision 17 фиксируют evaluation,
  identity, length, failure atomicity/Error/invalidation. Принять internal
  policy: exact construction, reserve rounding, overflow, slack, accounting и
  deterministic fallback. Политика может меняться только без observable
  resource/failure-order изменений.
- **Evidence/hypothesis:** capacity сейчас стартует с 4 и doubles; literal/
  variadic count известен, structures часто знают target. Меньше realloc/unused
  bytes — hypothesis до growth probes.
- **Scope:** exact allocation для literals/`arrayOf`; reserve через MIR,
  bytecode, VM/native; stride-aware checked arithmetic; no-op reserve; accounting.
  Не входят capacity getter, shrink-to-fit, static/stack arrays и изменение
  argument evaluation.
- **Owners:** checker/MIR construction metadata, bytecode tables, VM heap,
  native construction, managed growth/runtime API. Capacity math — focused
  owner, не MIR/managed coordinator. Tests считают allocations/resizes.
- **Поведение и tests:** counts 0/1/3/4/5/powers, nested/reference/raw, no-op/
  huge reserve, overflow, D040 effects, allocation failure ordering, aliases,
  iterator и budget-vs-host class. Bytecode round-trip и exact differential.
  Каждый allocate/resize failure atomic, counters zero.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe capacity --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp25-capacity-PLATFORM.json
  ./benchmarks/validate_results.py benchmarks/results/cp25-capacity-PLATFORM.json
  ```

- **Docs/evidence/removal:** internal capacity policy, execution/runtime docs,
  roadmap/evidence с allocation counts, peak/live bytes, raw samples и budget
  comparison. Old geometric construction удалить там, где exact count известен;
  documented fallback — только для unknown sizes.
- **Completion:** один capacity commit и push. Observable/failure-order drift,
  uncovered allocation или missing Tier 1 evidence блокируют completion.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 26. Оптимизация обхода nested arrays

**User-visible outcome:** nested managed-array traversal использует измеренные
scalar/reference primitives без repeated outer/inner descriptor work, сохраняя
safe aliasing, tracing и panic behavior.

- **Зависимость и граница:** зависит от 20 и 22–25. Faithful graph layout и
  general scalar primitives стабилизированы до graph/nested optimization.
- **Authority / decision gate:** D040/D045, record identity, tracing, fact
  invalidation и equivalence фиксированы. По nested probe выявить remaining
  cost. Internal decision принимается только для измеренного reusable path; если
  distinct bottleneck нет, этап честно закрывается evidence без optimization.
- **Evidence/hypothesis:** BFS/Dijkstra проходят nested adjacency arrays, но
  initial Dijkstra layout был несопоставим, а scalar costs доминировали. Отдельный
  nested cause пока не доказан.
- **Scope:** reusable outer-reference/inner-descriptor reuse, proof-safe bounds,
  loop-local rooted handles, record-edge arrays, mutation/safepoint invalidation,
  generic nested depths. Не входят graph algorithm change, source flattening,
  slices, unrooted pointer across GC и graph-specific opcode.
- **Owners:** MIR range/access, bytecode layout/provenance, VM nominal/array,
  emitter, managed descriptor/root. Новый nested owner только при подтверждённой
  отдельной ответственности. Tests — nested/graph differentials, не scalar
  aggregator.
- **Поведение и tests:** 2D/3D raw, record/enum, empty/ragged, aliases, inner
  mutations, outer replacement, calls/GC, wrong descriptor, bounds каждого
  уровня. Сохраняется первый panic и full span. Optimized/unoptimized MIR, VM,
  native O0/O2 и faithful BFS/Dijkstra exact checksum differential. Новые
  cache/root allocations exhaust, counters zero.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe nested-traversal --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp26-nested-PLATFORM.json
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/run.py --languages ruda --samples 11 --warmups 3 --output benchmarks/results/cp26-graphs-PLATFORM.md
  ./benchmarks/validate_results.py benchmarks/results/cp26-nested-PLATFORM.json benchmarks/results/cp26-graphs-PLATFORM.json
  ```

- **Docs/evidence/removal:** internal decision только при accepted path;
  roadmap/evidence отдельно показывает scalar/nested counts и raw graph data.
  Test caches/graph prototypes удалить. Evidence-backed no-change outcome
  допустим.
- **Completion:** measured optimization либо честный no-op closure — один commit
  и push. Optimization без semantics/sanitizer/allocator/Tier 1/budget gates не
  verified.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 27. Удаление checked integer operations по доказанным facts

**User-visible outcome:** MIR удаляет только checked integer operations с
доказанной невозможностью overflow; release overflow behavior остаётся
идентичным в VM/native.

- **Зависимость и граница:** зависит от 21 и 22. Переиспользует range lattice,
  но остаётся arithmetic optimization, не bounds pass/backend shortcut.
- **Authority / decision gate:** numeric widths/overflow, D040, MIR verification
  и equivalence фиксированы. Расширить MIR decision: signed/unsigned intervals,
  transfer/widening, proof tokens, replacement и fallback. Запрещены C signed
  overflow, benchmark assumptions и debug-only checks.
- **Evidence/hypothesis:** generated loops сохраняют checked operations даже
  при вероятно safe bounds. Материальный вклад доказывает checked-loop probe.
- **Scope:** proofs add/subtract/multiply/conversions для measured loops;
  verifier-checked unchecked form/constant fold; joins/widening; bytecode/VM/
  native; checked fallback. Не входят wrapping arithmetic, fast-math, width
  changes и division/shift без отдельного proof rule.
- **Owners:** focused MIR range/pass из 22, checked-integer MIR/bytecode
  verifier, VM arithmetic, native emitter, generated opcodes. Не добавлять logic
  в full `execution.c`, `mir.c` и broad verifier. Отдельные arithmetic/proof
  corruption tests.
- **Поведение и tests:** min/max/zero/negative, nested loops, branches,
  conversions, multiplication, still-overflow cases. Первый failure/message/
  span и side-effect order сохраняются. Corrupt proof rejected. Exact optimized/
  unoptimized MIR, VM, native O0/O2 differential. Analysis/verifier allocation
  failure → checked fallback, counters zero.
- **Focused acceptance:**

  ```sh
  ctest --preset debug -R '^ruda_c_unit$' --output-on-failure
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --probe checked-integer --samples 11 --warmups 3 --baseline benchmarks/results/cp21-probes-PLATFORM.json --output benchmarks/results/cp27-checked-integer-PLATFORM.json
  ./benchmarks/validate_results.py benchmarks/results/cp27-checked-integer-PLATFORM.json
  ```

- **Docs/evidence/removal:** MIR proof/architecture, serialized bytecode spec,
  opcodes, roadmap/evidence с removed/retained counts, overflow negatives, raw
  samples/budget. Test-only proof injection удалить.
- **Completion:** один arithmetic commit и push. Release overflow differential,
  corrupt-proof rejection, allocation fallback, sanitizers, architecture и Tier
  1 measurement обязательны.
- **Полный validation tail:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```

------

## 28. Финальная интеграция и release rebaseline

**User-visible outcome:** clean Tier 1 audit доказывает exact VM/native behavior
faithful reusable algorithm corpus и preview suite после всех follow-ups, затем
публикует true-release rebaseline и честные remaining limitations.

- **Зависимость и граница:** зависит от всех 13–27 и не вводит semantics,
  resource policy или optimization. Новая semantic need становится будущим
  checkpoint, а не скрытым fix внутри audit.
- **Authority / gate:** все accepted decisions/specs, definition of done, test
  strategy и Tier 1 roadmap. Единственный performance authority — protocol/
  budgets 21. Debug archive и broad language claims запрещены.
- **Evidence:** initial latest report уже использует release compiler/checksums,
  но это один host/3 samples и pre-fidelity Ruda boundaries. Каждый 13–27 должен
  иметь focused evidence до aggregate run.
- **Scope:** удалить оставшиеся aliases/wrappers/inline copies/renamed ops/shadow
  sizes/parallel-edge layouts/adapters/test counters/proof injection/old source
  spelling; clean `fmt`/`check`/VM run/test/native build/run/`--engine both`;
  checksums/stdout/stderr/panics/exits; Debug, true Release, ASan/UBSan, Linux
  LeakSanitizer, Tier 1 toolchains; raw rebaseline и regression comparison. Не
  входят new features, benchmark-driven semantics, public 1.0 и budget rewrite.
- **Owners:** integration fixtures только orchestrate focused owners. Проверить
  line counts, exception registry, CMake registration, sibling benchmark/CI/
  evidence. Не расширять `test_final_preview.c`; новый regression family получает
  отдельный owner.
- **Поведение и tests:** reusable insertion, heap sort, stack, queue, min-heap,
  compressed/rank DSU, BFS, `O(V^2)` Dijkstra через package boundaries; для
  каждого canonical fmt/check, VM root, VM/native/both tests, debug/release
  executable и exact checksum. Повторить checkpoint-12 scalar, Fibonacci/
  factorial, matrix `415.0\n`, records/enums/identity, Option/Result/`?`, match,
  imports/D043, D040/D041. Corrupt MIR/bytecode, formatter idempotence, fuzz,
  all fail-allocation indexes, zero counters, generated/docs/architecture.
- **Focused acceptance на каждом Tier 1:**

  ```sh
  cd ../ruda-algotithms
  RUDA="$(cd ../Ma && pwd)/build/debug/ruda" ./verify.sh
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/probes.py --samples 11 --warmups 3 --output benchmarks/results/cp28-probes-PLATFORM.json
  RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/run.py --languages ruda,go,rust --samples 11 --warmups 3 --output benchmarks/results/cp28-release-PLATFORM.md
  ./benchmarks/validate_results.py benchmarks/results/cp28-probes-PLATFORM.json benchmarks/results/cp28-release-PLATFORM.json --baseline benchmarks/results/cp21-algorithms-PLATFORM.json
  ```

  Записать absolute compiler, runtime archive/content hash, generated-C
  compiler/linker и final executable paths.
- **Docs/evidence/removal:** raw JSON, median/dispersion, artifact metadata,
  exact differential records, Debug/Release/sanitizer/Tier 1 exits, budget
  comparison и limitations; roadmap не преувеличивает unrun platforms. Exact-
  path workaround inventory 20–27 должен быть пуст.
- **Completion:** final integration/evidence commit и linked sibling commit при
  необходимости, немедленный push и оба SHAs. Milestone verified только при
  полном existing definition of done; missing Tier 1/raw/allocator/sanitizer/
  equivalence/budget evidence явно перечисляется и оставляет этап incomplete.
- **Полный validation tail плюс focused commands:**

  ```sh
  ./tools/check-architecture
  ./tools/check-c-memory-policy
  ./tools/check-generated
  ./tools/check-doc-links
  ./tools/check-spec-terms
  cmake --build --preset debug --parallel
  ctest --preset debug --output-on-failure
  cmake --build --preset release --parallel
  ctest --preset release --output-on-failure
  cmake --build --preset asan --parallel
  ctest --preset asan --output-on-failure
  cmake --build --preset asan-ubsan --parallel
  ctest --preset asan-ubsan --output-on-failure
  ```
