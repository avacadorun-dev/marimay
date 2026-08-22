# Checkpoints до первой публичной CLI-alpha Marimay

Статус: предлагаемый post-checkpoint-28 delivery plan. Этот файл не является
normative language specification и сам по себе не меняет состояния roadmap
gates. Перед началом реализации его нужно связать с
`docs/implementation/roadmap.md`; любое изменение source semantics сначала
проходит обычный decision/spec protocol.

Целевой результат — первая честно ограниченная публичная версия вида
`0.1.0-alpha.1`, на которой можно писать, тестировать и распространять
небольшие локальные консольные программы. Это не стабильная `0.1.0`, не P0 и
не утверждение о готовности networking, structured concurrency, package
registry, FFI, portable `.myb`, self-hosting или production workloads.

Checkpoint 28 остаётся algorithm/performance integration baseline.
Checkpoints 29–40 добавляют недостающий прикладной CLI vertical slice и
distribution gate. Нумерация не разрешает пропускать более ранние roadmap
dependencies: N0, I0–I5, architecture, memory-safety и Tier 1 gates сохраняют
свою силу.

## Условие публичного релиза

До checkpoint 40 запрещено:

- публиковать indexed release, installer, package или registry namespace;
- создавать впечатление, что весь язык из `docs/spec/` реализован;
- называть сборку стабильной `0.1.0`;
- отмечать N0, I0–I6, I8 или P0 `verified` на основании этого bounded slice;
- скрывать отсутствующие возможности за молчаливым fallback или host-specific
  поведением.

Финальный release claim ограничен следующим:

> Marimay 0.1.0-alpha.1 позволяет создавать локальные CLI-программы с
> аргументами, предсказуемыми exit codes, stdin/stdout/stderr, форматированием,
> UTF-8 text/bytes, файлами, paths, environment, базовыми collections,
> command-line parsing и typed JSON. `fmt`, `check`, VM `run`/`test`, native
> `run`/`build` и VM/native differential доступны на перечисленных и реально
> проверенных Tier 1 hosts.

Все остальные ограничения перечисляются в release notes и installed offline
documentation.

## Общие правила каждого checkpoint

1. Начинать только с зелёного `main` и закрытого обязательного predecessor.
2. Перед изменением subsystem прочитать
   `docs/implementation/compiler-architecture.md`, focused production owners и
   focused tests. Выполнить `./tools/check-architecture` и проверить line
   ceilings.
3. Semantic/API change в одном checkpoint включает accepted decision или
   amendment, normative spec, diagnostics, parser/checker/runtime coverage,
   VM/native differential, formatter fixtures и public examples.
4. Новый runtime intrinsic или compiler-known standard API проходит typed MIR,
   MIR verification, bytecode encode/decode/verify, VM, generated C17 и native
   execution. Signature-only extern не является реализацией.
5. Нельзя чинить source semantics в lowering, VM или C emitter. Checker и
   verified typed MIR остаются единственным semantic authority.
6. Каждый allocation-capable C API покрывает success, material errors и каждый
   reachable deterministic allocation-failure index. После teardown обязательны
   `live_allocations == 0` и `live_bytes == 0`.
7. Host I/O не вызывается через shell. Bounds, partial reads/writes,
   interruption, invalid UTF-8, Windows paths/console behavior и capability
   denial имеют точные contracts и tests.
8. Blocking Stage 0 I/O до I6 допустимо только как документированный internal
   adapter с removal gate. Оно не создаёт вторую публичную семантику и не
   разрешает claim о task-aware I/O.
9. Checkpoint получает evidence file, roadmap link, один reviewable commit и
   немедленный push. `implemented` или `verified` запрещены при незакрытом
   focused test, sanitizer, Tier 1 или allocation gate.
10. Публикация тега, release artifacts, домена или публичного namespace —
    отдельное user-authorized действие только после checkpoint 40.

Для каждого checkpoint после focused acceptance обязателен полный validation
tail:

```sh
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-generated
./tools/check-doc-links
./tools/check-spec-terms
./tools/check-example-syntax
./tools/check-syntax-coverage
cmake --build --preset debug --parallel
ctest --preset debug --output-on-failure
cmake --build --preset release --parallel
ctest --preset release --output-on-failure
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan --output-on-failure
```

Изменения runtime scheduling, synchronization или shared capture дополнительно
требуют TSan. Изменения parser/formatter/bytecode требуют соответствующей
sustained fuzz campaign. Linux release evidence требует LSan; scheduled/manual
Valgrind evidence учитывается отдельно и не заменяется allocator counters.

## Issue-to-checkpoint traceability

| Gap до публичной CLI-alpha | Primary checkpoint |
|---|---:|
| Честная alpha/version/compatibility/documentation граница | 29 |
| Dynamic strings, interpolation и primitive formatting | 30 |
| Source-visible arguments и controlled exit status | 31 |
| stdin/stdout/stderr, print/eprint/read/flush | 32 |
| UTF-8 text operations, Bytes и bounded builders | 33 |
| Исполняемые path/filesystem APIs | 34 |
| Environment, cwd и executable context | 35 |
| Map/Set и production iteration core | 36 |
| First-party command-line option parser | 37 |
| Typed JSON для первого data-transform CLI | 38 |
| Native run, init, doctor и installable toolchain artifacts | 39 |
| N0/license/security/Tier 1/public workload/release closure | 40 |

Dependency graph:

```text
28 -> 29 -> 30 -> 31 -> 32
              \-> 33 -> 34 -> 35
                    \-> 36 -> 37
                         \-> 38
31..38 -----------------------> 39 -> 40
```

Checkpoint 18 должен уже определить canonical array spelling до 29. Если его
migration window не закрыт, checkpoint 29 обязан либо закрыть точный removal
gate, либо явно исключить source-compatibility claim из alpha.

---

## 29. Публичный release contract и честная implemented surface

**User-visible outcome:** version/help/README/reference однозначно отделяют
реально исполнимый alpha subset от target 1.0 design и не предлагают
неработающие программы как quick start.

- **Зависимость и граница:** зависит от checkpoint 28. Это release semantics,
  versioning и documentation checkpoint; он не публикует release и не добавляет
  runtime features.
- **Authority / decision gate:** принять решение о `0.1.0-alpha.N`, source
  compatibility до beta, diagnostic stability, bytecode non-compatibility,
  standard-library versioning и removal policy. Обновить toolchain §10 и
  roadmap release ladder. N0 остаётся независимым hard blocker.
- **Scope:** `mmay version` human/JSON, help, README, examples index, installed
  version metadata, known-limitations format, release-channel names и exact
  implemented-command inventory. `bytecode` может честно оставаться
  `unavailable`; работающий bounded `std` не может называться целиком
  `unavailable` без поясняющей version classification.
- **Required corrections:** создать runnable quick start, проходящий `fmt`,
  `check`, VM `run`, native `build` и `test --engine both`; target-design
  examples пометить и тестировать отдельно. `mmay help` не должен выдавать
  unimplemented commands без явной маркировки. `mmay explain` либо становится
  рабочим для alpha diagnostics, либо удаляется из everyday help до своего
  checkpoint.
- **Tests:** human/JSON version snapshots; help snapshots для каждого
  implemented/staged command; extracted README quick start; mismatch tests
  между version metadata, installed paths и release notes; no-network and
  relocation checks.
- **Focused acceptance:**

  ```sh
  build/debug/mmay version
  build/debug/mmay version --json
  build/debug/mmay help
  build/debug/mmay fmt tests/public-alpha/cp29-quick-start --check
  build/debug/mmay check tests/public-alpha/cp29-quick-start
  build/debug/mmay test tests/public-alpha/cp29-quick-start --engine both
  ```

- **Docs/evidence:** accepted release/version decision, toolchain and roadmap
  update, executable README fixture, `cp29-public-contract` evidence.
- **Completion:** metadata и every public statement совпадают с executable
  evidence. Нельзя завершать с `0.0.0-dev` в binary и заявленной другой alpha
  version в документации.

---

## 30. Dynamic strings, interpolation и primitive formatting

**User-visible outcome:** программа строит строки из runtime values и печатает
не только literals или отдельно отформатированный `f64`.

- **Зависимость и граница:** зависит от 29 и закрытых numeric/string foundations
  checkpoint 6. Не включает stdin, files или general reflection.
- **Authority / decision gate:** принять точное interpolation/formatting rule.
  Решение перечисляет поддержанные primitive types, user-defined structural
  formatting contract, evaluation order, allocation/resource failures,
  locale independence и interaction с `println(string)`. Не добавлять `any`,
  implicit conversion graph или host `printf` semantics.
- **Scope:** interpolated-expression checker typing; exact once-only
  left-to-right evaluation; integer/bool/f64 formatting; user-owned visible
  formatting function/interface, если он принят; bounded string construction;
  MIR/bytecode/VM/native operations; diagnostic and formatter display.
- **Required API:** exact signatures всех integer format/parse functions и
  optional radix/sign/padding policy фиксируются decision. Нельзя публиковать
  частично названную family. `formatF32` сохраняет D046 raw-binary32 contract.
- **Tests:** all integer extrema and bases, bool, both zero signs, infinities,
  NaNs, embedded NUL, Unicode, empty interpolation, nested expressions,
  side-effect order, user formatter ambiguity/visibility, output limits,
  malformed bytecode и fail-allocation sweep. VM/native bytes должны совпадать.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp30-formatting --check
  build/debug/mmay check tests/public-alpha/cp30-formatting
  build/debug/mmay run tests/public-alpha/cp30-formatting
  build/debug/mmay test tests/public-alpha/cp30-formatting --engine both
  build/debug/mmay build tests/public-alpha/cp30-formatting --release --output build/debug/cp30-formatting
  build/debug/cp30-formatting
  ```

- **Docs/evidence/removal:** decision, language/stdlib/execution updates,
  formatter fixtures, runnable tour fragment, checkpoint evidence. README
  interpolation example становится compile-and-run fixture здесь.
- **Completion:** ни один formatter не зависит от host locale или undefined C
  conversion; every accepted formatted type проходит full VM/native path.

---

## 31. Аргументы программы и controlled exit status

**User-visible outcome:** Marimay CLI читает переданные после `--` аргументы и
возвращает caller-selected portable success/failure code без обхода `defer`.

- **Зависимость и граница:** зависит от 30 и cross-package `Array<string>`
  correctness checkpoint 14. Не включает subprocess spawning.
- **Authority / decision gate:** выбрать один canonical source contract:
  допустимую `main` signature с arguments либо `std/process`/`std/cli`
  accessor. Зафиксировать наличие/отсутствие executable name, invalid host
  encoding, Windows quoting, maximum count/bytes, test injection и capability
  classification. Exit API обязана выполнять lexical cleanup; immediate
  process termination, пропускающая `defer`, не является safe default.
- **Scope:** CLI `--` transport, VM options, native executable adapter, test
  fixture injection, typed exit code validation and mapping. Preserve exact
  G4000–G4003/I0002 precedence.
- **Tests:** zero/many/empty/Unicode/NUL-rejected arguments, leading dashes,
  `--`, spaces, Windows quoting corpus, byte/count limits, invalid encoding,
  normal/Err/panic/defer outcomes, invalid exit codes и VM/native parity.
- **Focused acceptance:**

  ```sh
  build/debug/mmay run tests/public-alpha/cp31-args -- alpha "two words"
  build/debug/mmay test tests/public-alpha/cp31-args --engine both
  build/debug/mmay build tests/public-alpha/cp31-args --release --output build/debug/cp31-args
  build/debug/cp31-args alpha "two words"
  ```

- **Docs/evidence:** language entry contract, toolchain run/build/test syntax,
  diagnostics, examples and checkpoint evidence.
- **Completion:** argument bytes and exit classification agree across direct VM,
  native process and test isolate; cleanup always runs exactly once.

---

## 32. Полные стандартные потоки для synchronous CLI-alpha

**User-visible outcome:** программы читают stdin и независимо пишут stdout и
stderr через typed, bounded APIs: `print`/`println`, `eprint`/`eprintln`,
`flush`, line/read-to-limit input и explicit EOF.

- **Зависимость и граница:** зависит от 30–31. Это synchronous Stage 0 CLI
  surface с documented I6 removal gate, не task-aware direct I/O claim.
- **Authority / decision gate:** расширить D046 или принять новую decision с
  exact signatures, buffering, partial write/read, line ending, EOF, invalid
  UTF-8, maximum bytes, flush and error precedence. Разделить capabilities
  `io.stdin`, `io.stdout`, `io.stderr`; runtime diagnostics не являются
  program stderr.
- **Scope:** physical/embedded `std/io`, checker ownership, MIR instructions,
  bytecode codec/verifier, VM sinks/sources, generated C17, native adapters,
  captured test records и CLI grants. Binary I/O опирается на checkpoint 33 и
  может быть включено там, но text API не должно притворяться byte-safe.
- **Tests:** empty input, EOF before/after data, LF/CRLF policy, embedded NUL,
  invalid UTF-8, partial host operations, EINTR-equivalent retry policy,
  denied capability, closed/broken pipe, flush failure, stdout/stderr ordering
  records, no diagnostic pollution, allocation failures and VM/native parity.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp32-stdio --check
  build/debug/mmay check tests/public-alpha/cp32-stdio
  build/debug/mmay test tests/public-alpha/cp32-stdio --engine both
  build/debug/mmay build tests/public-alpha/cp32-stdio --release --output build/debug/cp32-stdio
  ```

  Process fixtures передают exact input bytes и отдельно сравнивают stdout,
  stderr и exit code VM/native executables.
- **Docs/evidence/removal:** stdlib/execution/toolchain/capability specs,
  diagnostic registry, examples, evidence и explicit I6 parking removal gate.
- **Completion:** line/text semantics, error classes и captures одинаковы на
  Tier 1; никакой host `errno` или locale spelling не входит в public output.

---

## 33. UTF-8 text, Bytes и bounded builders

**User-visible outcome:** прочитанные данные можно безопасно разбирать,
собирать и преобразовывать без quadratic literal-only workarounds.

- **Зависимость и граница:** зависит от 30; binary stdio integration зависит от
  32. Не включает regex или locale-dependent collation.
- **Authority / decision gate:** принять exact `string` byte/rune/grapheme view
  subset для alpha, `Bytes` identity/mutation rules, UTF-8 conversion failures,
  slicing boundaries и builder limit/failure behavior. Никакого integer string
  indexing.
- **Scope:** byte length, rune iteration, bounded substring/view/copy,
  contains/prefix/suffix, split/join/trim with explicit Unicode policy,
  `Bytes` construction/index/push/append/slice/copy, text/byte builders,
  encode/decode UTF-8. Не добавлять permanent borrowed view до доказанного
  suspension/lifetime contract.
- **Tests:** ASCII, multibyte scalars, combining marks, invalid UTF-8, empty and
  maximum inputs, boundary slices, alias mutation, builder growth/overflow,
  reference rooting, iteration invalidation, randomized round trips, malformed
  bytecode and allocation sweeps.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp33-text-bytes --check
  build/debug/mmay check tests/public-alpha/cp33-text-bytes
  build/debug/mmay run tests/public-alpha/cp33-text-bytes
  build/debug/mmay test tests/public-alpha/cp33-text-bytes --engine both
  build/debug/mmay build tests/public-alpha/cp33-text-bytes --release --output build/debug/cp33-text-bytes
  build/debug/cp33-text-bytes
  ```

- **Docs/evidence:** standard-library text/bytes section, language types,
  execution roots/layout, formatter examples and checkpoint evidence.
- **Completion:** exact UTF-8 and memory behavior доказано на VM/native; input
  limit нельзя обойти через repeated builder operations.

---

## 34. Paths и исполняемый filesystem slice

**User-visible outcome:** CLI читает и атомарно пишет файлы, получает metadata
и перечисляет directory entries через portable typed paths и capabilities.

- **Зависимость и граница:** зависит от 32–33 и D041 cleanup. Это local
  filesystem alpha slice, не watcher, mmap, permissions management или network
  filesystem guarantee.
- **Authority / decision gate:** утвердить alpha subset `std/path` и `std/fs`:
  path representation, relative/absolute rules, lexical normalization,
  symlink policy, open/read/write/close, bounded `readText`/`readBytes`, atomic
  `writeText`/`writeBytes`, metadata, deterministic directory iteration and
  stable error cases. Capability checks происходят после operation-appropriate
  safe canonicalization.
- **Scope:** `--allow-read`, `--allow-write`; VM capability matcher; native
  POSIX/Windows adapters; temporary test filesystem; explicit cleanup and
  writer failure atomicity. Existing platform helpers toolchain не становятся
  user API автоматически.
- **Tests:** Unicode/spaces/long paths, relative roots, `..`, symlinks, missing/
  denied/not-file/not-directory, read-only targets, short reads/writes,
  interruption, replacement failure, file-size limits, invalid UTF-8,
  directory ordering, close failure, cleanup panic precedence, capability
  escape attempts, allocation failures and exact VM/native errors.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp34-files --check
  build/debug/mmay check tests/public-alpha/cp34-files
  build/debug/mmay test tests/public-alpha/cp34-files --engine both
  build/debug/mmay run tests/public-alpha/cp34-files --allow-read tests/public-alpha/cp34-files/fixtures
  build/debug/mmay build tests/public-alpha/cp34-files --release --output build/debug/cp34-files
  ```

- **Docs/evidence/removal:** fs/path/capability decisions and specs, diagnostics,
  examples, evidence. Signature-only `std/fs` checkpoint-4 fixture заменяется
  real package/runtime conformance либо явно остаётся isolated negative fixture.
- **Completion:** VM/native temporary-filesystem records совпадают; denied roots
  не вызывают host operation; every opened resource закрывается на всех exits.

---

## 35. Environment и process context

**User-visible outcome:** программа получает cwd, executable identity и только
явно разрешённые environment variables с portable encoding/error semantics.

- **Зависимость и граница:** зависит от 31, 33 и path contract 34. Не включает
  subprocess execution, signals или dynamic libraries.
- **Authority / decision gate:** принять `std/env` и process-context API:
  named lookup, missing versus invalid encoding, enumeration as separate grant,
  cwd/executable path guarantees, test injection and cache/nondeterminism rules.
- **Scope:** `--allow-env NAME`, optional separately named enumeration flag,
  VM/native capability transport, Windows case folding policy, deterministic
  test environment and redacted diagnostics. Hidden environment overrides для
  runtime semantics запрещены.
- **Tests:** missing/empty/Unicode/invalid encoding, duplicate/case variants,
  denied lookup, enumeration denial, changed cwd, relocated executable,
  spaces/Unicode, deterministic tests, no environment leakage and allocation
  failures.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp35-environment --check
  build/debug/mmay check tests/public-alpha/cp35-environment
  build/debug/mmay test tests/public-alpha/cp35-environment --engine both
  build/debug/mmay run tests/public-alpha/cp35-environment --allow-env MMAY_ALPHA_TEST
  ```

- **Docs/evidence:** standard-library, execution capabilities, toolchain flags,
  diagnostics, examples and checkpoint evidence.
- **Completion:** undeclared environment никогда не влияет на observable
  program result или semantic build cache; VM/native behavior совпадает.

---

## 36. Map, Set и production iterator core

**User-visible outcome:** CLI хранит lookup tables, counters и unique values в
first-party collections без parallel-array workarounds.

- **Зависимость и граница:** зависит от 33 и Array mutation contract 17. Не
  включает concurrent collections или custom allocator APIs.
- **Authority / decision gate:** зафиксировать Map/Set equality/hash eligibility,
  insertion order, update/delete/reinsert position, generation invalidation,
  growth errors, capacity non-observability and deterministic test seeding.
  Hash randomization не может менять iteration order.
- **Scope:** generic type checking, dot/free calls, structural constraints,
  typed MIR layouts, bytecode tables/opcodes, VM/native managed storage,
  precise tracing, iterators and standard error cases. Все public operations
  должны быть перечислены; half-surface не публикуется.
- **Tests:** primitive/string/nominal keys where allowed, collisions, update,
  remove/reinsert, empty/large maps, mutation during iteration, equality/hash
  laws, nested managed values/cycles, package boundaries, malformed bytecode,
  collect-at-every-allocation, randomized heap graphs and fail-allocation.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp36-map-set --check
  build/debug/mmay check tests/public-alpha/cp36-map-set
  build/debug/mmay run tests/public-alpha/cp36-map-set
  build/debug/mmay test tests/public-alpha/cp36-map-set --engine both
  build/debug/mmay build tests/public-alpha/cp36-map-set --release --output build/debug/cp36-map-set
  build/debug/cp36-map-set
  ```

- **Docs/evidence:** D007/D013 consequences where applicable, language/stdlib/
  execution specs, formatter fixtures, examples and evidence.
- **Completion:** hash/equality laws and insertion order property tests зелёные;
  GC roots survive cycles and every VM/native observation matches.

---

## 37. First-party `std/cli` option parser

**User-visible outcome:** приложение декларативно разбирает flags, options и
positionals, печатает deterministic help и возвращает typed usage errors.

- **Зависимость и граница:** зависит от 31, 33 и 36. Это library API, не новый
  compiler syntax и не shell parser.
- **Authority / decision gate:** утвердить bounded `std/cli` model: long/short
  names, boolean flags, required/optional values, repeated options,
  positionals, `--`, subcommands only if justified, help order, duplicate
  declarations, unknown options, Unicode and exit recommendation. Shell
  expansion/quoting не выполняется библиотекой.
- **Scope:** records/enums/builders/collections implementation through ordinary
  Marimay-visible symbols where possible; compiler intrinsic допускается только
  для уже утверждённого argument transport. API не использует `any` или runtime
  reflection.
- **Tests:** empty args, clusters if accepted, `--name=value`, separated values,
  negative numeric values, repeated/unknown/missing options, `--`, Unicode,
  help width/order, limit/OOM, property generation and VM/native exact bytes.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp37-cli-parser --check
  build/debug/mmay check tests/public-alpha/cp37-cli-parser
  build/debug/mmay test tests/public-alpha/cp37-cli-parser --engine both
  build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --help
  build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --count 3 input.txt
  ```

- **Docs/evidence:** stdlib API, user guide, help/error snapshots, examples and
  checkpoint evidence.
- **Completion:** parser package itself uses only public alpha language/runtime
  contracts; real CLI fixture has no benchmark-only or host-C parsing bypass.

---

## 38. Typed JSON codec для data-transform CLI

**User-visible outcome:** alpha включает реальную data utility, которая читает
bounded JSON, декодирует typed records/enums/collections, преобразует данные и
детерминированно кодирует JSON обратно.

- **Зависимость и граница:** зависит от 33, 36 и filesystem/stdio 32/34. Не
  включает schema registry, arbitrary reflection или streaming network body.
- **Authority / decision gate:** утвердить bounded typed codec model: explicit
  codec values или compiler derives, field naming, unknown/missing/duplicate
  fields, enum representation, number ranges, Unicode escapes, ordering,
  nesting/bytes limits and error spans. Private fields не доступны через
  unrestricted reflection.
- **Scope:** tokenizer/parser/encoder packages, typed codec generation or
  explicit builders, Array/Map integration, text/bytes I/O and deterministic
  errors. Если derives добавляют source syntax/attributes, checkpoint включает
  полный semantic change protocol и formatter fixtures.
- **Tests:** RFC-compatible valid corpus as applicable, invalid/truncated data,
  duplicate keys, nesting/size/number limits, Unicode/surrogate cases,
  deterministic output, unknown-field policies, private fields, enum variants,
  round trips, fuzzing, allocation failures and VM/native equality.
- **Focused acceptance:**

  ```sh
  build/debug/mmay fmt tests/public-alpha/cp38-json --check
  build/debug/mmay check tests/public-alpha/cp38-json
  build/debug/mmay test tests/public-alpha/cp38-json --engine both
  build/debug/mmay run tests/public-alpha/cp38-json -- tests/public-alpha/cp38-json/input.json
  build/debug/mmay build tests/public-alpha/cp38-json --release --output build/debug/cp38-json
  build/debug/cp38-json tests/public-alpha/cp38-json/input.json
  ```

- **Docs/evidence:** stdlib JSON contract, security/limit guidance, formatter
  fixtures if needed, runnable transformer example and checkpoint evidence.
- **Completion:** public transformer использует только shipped APIs; parser and
  codec fuzz campaigns не имеют unresolved crash/hang/OOM amplification.

---

## 39. Native run, project init, doctor и installable alpha artifacts

**User-visible outcome:** fresh supported host устанавливает Marimay, создаёт
локальный проект, диагностирует native prerequisites и одинаково запускает VM
или native программу без ручного выбора compiler/linker paths per project.

- **Зависимость и граница:** зависит от 29–38 и existing native build/cache
  checkpoints 10/13. Не включает dependency resolver, registry или cross
  compilation.
- **Authority / decision gate:** уточнить D030 alpha distribution contract,
  install layout, relocatability, upgrade/uninstall, host-toolchain ownership,
  `mmay init`, `mmay doctor`, native run cache/argument/capability transport and
  artifact naming. N0 должен быть готов к закрытию, но публикация остаётся 40.
- **Scope:** `mmay run --engine native`; local-only `mmay init`; human/JSON
  `doctor`; version-matched debug/release runtimes; installed `std` and offline
  docs; source archive plus per-supported-host archive/package; checksums,
  provenance/SBOM generation; paths with spaces/Unicode; atomic install/update
  where format supports it.
- **Tests:** clean image install, relocate prefix, bare `PATH` invocation,
  init/fmt/check/run/test/build workflow, offline repeat, missing/stale/wrong
  compiler/runtime, read-only prefix/project, interrupted install/build, upgrade
  and uninstall without deleting user projects, command collision and no former
  `ma` alias. Exact machine-readable doctor/version schemas обязательны.
- **Focused acceptance:**

  ```sh
  mmay version --json
  mmay doctor --json
  mmay init alpha-smoke
  cd alpha-smoke
  mmay fmt --check
  mmay check
  mmay run --engine vm . -- --help
  mmay run --engine native . -- --help
  mmay test --engine both
  mmay build --release --output dist/alpha-smoke
  ```

- **Docs/evidence:** install/upgrade/uninstall and native-prerequisite guides,
  toolchain/roadmap updates, artifact manifest and checkpoint evidence for each
  claimed host.
- **Completion:** every claimed host artifact проходит clean-image lifecycle.
  Нельзя объявлять platform support по configured workflow без completed run.

---

## 40. Финальная публичная CLI-alpha интеграция

**User-visible outcome:** один versioned, installable и independently auditable
release candidate доказывает, что Marimay годится для небольших локальных CLI, и
честно перечисляет всё, чего в alpha нет.

- **Зависимость и граница:** зависит от всех checkpoints 29–39 и не вводит
  новые semantics. Любой найденный semantic/API defect возвращается в owning
  checkpoint или получает новый atomic checkpoint; final integration не
  становится bug-fix monolith.
- **Authority / decision gate:** governing authority — accepted decisions,
  normative specs, N0, roadmap release policy и evidence предыдущих
  checkpoints. Checkpoint 40 может уточнять только release metadata и
  documentation; новая source/runtime семантика требует отдельного decision и
  checkpoint.
- **External hard gates:** N0 verified с accepted follow-up naming decision;
  выбранная и добавленная repository license; контролируемые public repository,
  documentation/security contacts и signing identity; command/PATH collision
  matrix; явное user authorization на publication.
- **Repository release files:** `LICENSE`, `SECURITY.md`, contribution/build
  guide, changelog/release notes, support scope, compatibility/migration policy,
  known limitations, artifact checksums, SBOM/provenance and exact source
  revision. Placeholder domains/namespaces отсутствуют в published contract.
- **Required public workload corpus:**
  1. stdin line filter с Unicode, stdout/stderr и broken-pipe behavior;
  2. `wc`/search-like file utility с options, multiple inputs, typed errors и
     meaningful exit codes;
  3. bounded typed JSON transformer с file/stdin modes.

  Все три используют reusable packages, не inline compiler-specific
  workarounds, и проходят VM/native differential.
- **Focused acceptance / command matrix:**

  ```sh
  mmay fmt --check
  mmay check --all-targets
  mmay test --engine both
  mmay run --engine vm examples/public/line-filter -- --help
  mmay run --engine native examples/public/line-filter -- --help
  mmay build examples/public/file-count --release --output dist/file-count
  mmay build examples/public/json-transform --release --output dist/json-transform
  mmay version --json
  mmay doctor --json
  ```

- **Exact comparison:** для workload matrix сравнить stdout bytes, stderr bytes,
  exit codes, diagnostics, file outputs, allocation/resource classification,
  panic identity and source spans. Debug/Release, VM/native, installed/build-tree
  and all claimed Tier 1 hosts must agree where contract требует equality.
- **Quality gates:** full Debug/Release; ASan/UBSan/LSan; Windows ASan; TSan if
  touched code applies; scheduled Valgrind; parser/formatter/bytecode/JSON fuzz
  campaigns; allocator sweeps; architecture/memory/generated/docs freshness;
  clean checkout and clean installed-prefix tests; reproducible source archive.
- **Release audit:** map every public-alpha claim and every requirement assigned
  to checkpoints 29–40 to direct evidence. Full 1.0-only requirements remain
  explicitly deferred, not falsely marked verified.
- **Docs/evidence:** final public-alpha evidence report, release notes, known
  limitations, install/security/support documents, artifact manifest and
  direct links to every required CI run for the exact release SHA.
- **Completion:** final evidence commit is pushed and all required CI runs for
  that exact SHA are green. После этого подготовленные artifacts могут быть
  опубликованы только по явному подтверждению пользователя. Release называется
  `0.1.0-alpha.1` либо более поздним alpha номером, но не stable `0.1.0`.

## Явно отложено после публичной CLI-alpha

Следующее не блокирует checkpoint 40, если отсутствующие claims явно указаны:

- direct-style tasks, scheduler, cancellation, channels and non-blocking I/O
  из I6;
- network, DNS, TLS, HTTP client/server;
- subprocess execution and signals;
- dependency resolver, `mmay add/update/vendor/audit` и public registry из I7;
- production LSP/editor integration, watch/cache/property/fuzz user commands из
  I8;
- portable public `.myb`, untrusted `mmay exec` and cross compilation;
- C FFI/unsafe platform contract;
- self-hosting and bootstrap reproducibility;
- beta compatibility/security promises, production reference service и 1.0.

Эти exclusions не разрешают shipping заглушек под теми же именами. Команда или
API либо работает в заявленном alpha contract, либо документированно отсутствует
и выдаёт точную staged/unsupported diagnostic.
