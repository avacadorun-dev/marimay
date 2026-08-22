# Preview 0.1-dev checkpoints

Порядок обязательный. Не переставлять и не сливать соседние вехи.

Текущая база после нормативного D046 (`0d08dd2` и later main):
- package-local non-generic D003 resolver уже есть; его не переписывать;
- record/enum живут в checker и MIR oracle, но VM/native их ещё отвергают;
- есть bounded `Array<f64>` / `Array<Array<f64>>`, не общий `Array<T>`;
- user stdout контракт принят, движки и `std/io` не реализованы;
- `ruda fmt` есть; публичные `check` / `run` / `build` / `test` ещё нет.

Каждый checkpoint — отдельная веха: полный validation loop, evidence/roadmap,
commit и немедленный push. I2–I5 не помечать `verified`. I6 оставить
`not-started`. Public 0.1.0 не объявлять.
После любой feature implementation и перед evidence/commit каждого checkpoint
обязательно запускай `./tools/check-architecture`; красный gate блокирует
дальнейшую feature работу.

------

## 0. Architecture and bounded context

Это обязательный блокирующий checkpoint перед любым feature checkpoint ниже.
Он меняет физическую архитектуру bootstrap, а не семантику Ruda. Пока он не
закрыт, checkpoint 1 остаётся припаркованным и новая функциональность не
добавляется.

Authority: `AGENTS.md`, `docs/implementation/compiler-architecture.md`,
`docs/spec/execution.md`, C17 ownership contract и текущий public API.

Требуемый результат:

- каждый handwritten production/test `.c` не больше 1,500 физических строк,
  target 500–1,200; owner-private `.h` не больше 400, target 50–250, а
  owner API header не больше 160;
- `checker.c` становится только package/pass coordinator; declarations,
  expressions/calls, patterns/match, statements/CFG и package publication
  живут в независимо компилируемых owner modules;
- MIR разделён на representation/builders, verifier и oracle/execution;
- bytecode разделён на model/builders, codec, verifier, lowering и VM;
- parser, formatter, CLI, VM/native/runtime и тестовые hotspots разделены по
  реальным semantic owners, а не по случайным диапазонам строк;
- private contracts узкие и const-oriented; нет handwritten `.inc`, включения
  `.c`, include-facade или общего заголовка со всем mutable state;
- focused test owners зеркалят production owners и сохраняют все существующие
  assertions, fault injection и порядок наблюдаемых проверок;
- `tools/check-architecture` проверяет hard limit, исключения, запрещённые
  owner dependencies, `.inc`/`.c` include tricks и регистрацию production/test
  translation units в CMake;
- архитектурный гейт запускается в CI на каждый push и pull request и входит в
  acceptance/evidence каждого последующего checkpoint.

Временное исключение допустимо только через
`docs/implementation/architecture-exceptions.tsv`: exact path, owner, explicit
ceiling, reason и removal checkpoint. В исключённый файл нельзя добавлять
feature logic. Checkpoint 0 не считается закрытым, если старые hotspots просто
занесены в исключения.

Acceptance:

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

Запиши before/after inventory, module ownership, exact commands and known
limitations в evidence; обнови roadmap. Сделай отдельный commit и немедленный
push. Только после этого восстанови checkpoint 1 и продолжай его вертикальный
engine slice.

------

## 1. Record / enum / non-generic interface engines

Доведи уже существующий managed nominal slice до VM и native. Это текущий
V0 engine-equivalence blocker. Не откладывай его после generics или Array<T>.

Полностью прочитай goal-objective.md и authority. Особо: language.md §§5.4–5.5,
§5.8, §9; execution.md §§2, 5, 9, 13; D004, D005, D007, D040; diagnostics.md
§§6–7, 9; test-strategy record/enum/identity families.

Требуемый вертикальный результат:
- record construction, field projection, mutable fields;
- `identity.same` только для одного nominal record type;
- algebraic enum construction и nested exhaustive match;
- useful missing/redundant witnesses на уже принятом pattern subset;
- non-generic structural interface values и dispatch через тот же D003
  resolver, без второго method table;
- D040 source-order construction с effectful fields вне declaration order;
- checker → verified MIR → encoded VM → generated C17 → одинаковый результат.

Продолжай существующую реализацию. Не переписывай managed heap. Не требуй
generic payloads — они входят в checkpoint 4.

Acceptance:
- runnable record mutation / `identity.same` program;
- exhaustive enum match program;
- D040 effect-order program;
- non-generic interface call с free/dot equivalence.

Добавь ownership/provenance/rooting tests, allocation failures, malformed
verifier tests, formatter fixtures, public examples и VM/native differential.
Обнови evidence/roadmap, оставив I2/I3/I4/I5 in-progress. После полной
проверки commit и сразу push.

------

## 2. Local packages / imports / D043

Реализуй deterministic local package/import resolution и non-generic D043
import-form invariance поверх уже существующего shared D003 resolver.

Полностью прочитай goal-objective.md и authority. Особо: language.md §3, §5.8,
§6.2; D003 и D043; diagnostics.md §7; test-strategy import matrix;
compiler-architecture.md; evidence iteration 39.

Требуемый вертикальный результат:
- multi-file local packages;
- public/private visibility;
- named type-only import;
- named type+function import;
- namespace import;
- одинаковый owned public dot-call set при всех нормативных формах импорта;
- deterministic symbols независимо от file/import order;
- structural non-generic interface conformance через тот же call resolver;
- точные diagnostics для visibility/ownership/conflicts;
- одинаковый typed MIR и VM/native результат на record/enum/interface path
  из checkpoint 1.

Не переписывай package-local resolver. Не реализуй package manager/network
или general overload resolution. Не подменяй D043 фильтрацией готовых
function vectors в compiler service.

Добавь parser/checker/formatter/import fixtures, negative matrix,
allocation/limit tests и VM/native differential. Обнови roadmap/evidence,
оставив I2 in-progress. После полной проверки commit и сразу push.

------

## 3. Compiler service: occurrence facts и completion

Доведи compiler service до внутреннего I2 editor foundation, не подключая LSP.

Поверх уже стабильных definition facts реализуй:
- owned occurrence/reference facts;
- definition-from-reference;
- deterministic package symbol search;
- exact dot-call completion, использующее тот же resolver, что checker;
- equivalent available free spelling;
- snapshot/result-scoped query APIs;
- stale-result safety;
- bounded facts and cancellation;
- public/private/import-aware invalidation после checkpoint 2.

Completion не должна повторно вычислять кандидатов фильтрацией
`checked.functions`. `RudaTypeId` нельзя смешивать между result versions.
Это ещё не production LSP и не публичный `ruda check`.

Добавь reverse-order, overlay, stale snapshot, cancellation,
fail-every-allocation, exact visibility/import и completion==resolver tests.
Обнови architecture/evidence/roadmap. Commit и сразу push после полного
validation loop.

------

## 4. Generic Option / Result / ? / mapError

Реализуй ограниченный, но настоящий generic vertical slice, достаточный для:
- пользовательских generic functions;
- `Option<T>`;
- `Result<T,E>`;
- postfix `?`;
- explicit `mapError`;
- generic record/enum payloads на уже существующем engine path;
- обязательного `fs.readText(...).mapError(...)?` compile fixture без
  фиктивной реализации I/O.

Следуй language.md §5.2, §5.7, §10.1, standard-library.md §2, D006, accepted
decisions и test-strategy. При конфликте сначала decision/spec.

Нужны:
- canonical generic identity/substitution;
- bounded inference и deterministic diagnostics;
- monomorphized либо иной нормативно совместимый typed MIR;
- verifier support;
- encoded bytecode/VM;
- generated C17/native;
- managed rooting для generic managed payloads;
- positive, negative, recursion/depth/size/fail-allocation tests;
- VM/native differential для Option/Result/mapError/?;
- formatter и user example.

Не заявляй полноценные collections или I2/I5 verified. Не заводи второй
completion engine: service из checkpoint 3 может научиться generic cases,
но lookup остаётся checker-owned. Обнови evidence/roadmap, проверь
sanitizers и portability, commit и сразу push.

------

## 5. Preview Array<T>

Расширь текущий bounded `Array<f64>` / `Array<Array<f64>>` slice до preview
`Array<T>`. Делай это только после checkpoint 4.

Authority: D045, language Array/indexing rules, standard-library collections,
execution managed-memory/rooting contracts.

Требуется:
- canonical serialized program type table вместо двух fixed backend tags;
- `Array<T>` в checker, MIR, bytecode codec/verifier/VM и generated C17;
- `arrayOf` / `arrayOf<T>`;
- free/dot `push`;
- `length`;
- индексирование и indexed compound assignment;
- нормативная iteration surface;
- bounds/resource/allocation classification parity;
- precise roots для nested managed arrays;
- sorting/searching `Array<int>` acceptance;
- существующая f64 matrix остаётся зелёной.

Проверь malformed bytecode, descriptor/provenance, collect-before-allocation,
fail-every-allocation, G4002 extrema и VM/native differential.

Не добавляй raw TypeId в wire format и raw pointers в generated C.
Обнови formatter fixtures, examples, evidence/roadmap. Commit и сразу push.

------

## 6. Numeric widths, conversions, strings, D046 formatters

Закрой обязательную числовую и текстовую поверхность 0.1-dev вертикально.

Требуется:
- все нормативные integer widths;
- checked, wrapping и saturating operations;
- explicit numeric conversions;
- D035 shifts для всех integer widths;
- D042 division/remainder для всех integer widths;
- уже принятый D044 `f64`;
- strings;
- `std/convert.formatF32` и `formatF64` по D046;
- одинаковые VM/native result bits и panic identity.

`f32` arithmetic, literals и NaN canonicalization нельзя вывести из D044.
Если этот checkpoint включает исполняемый `f32` кроме formatter bits,
сначала закрой отдельное accepted decision, потом specs/tests, потом
реализацию. Иначе реализуй только D046 `formatF32` над raw binary32 bits.

Interpolation не является D046 stdout и не специфицирована. Не реализуй
произвольный «standard string representation». Либо сначала отдельное
decision, либо ограничься интерполяцией уже-`string` выражений и явно
зафиксируй это в evidence.

Не реализуй `println`, capability `"io.stdout"` и user write path — это
checkpoint 9. Не делай внутренний `ok:f64:` hex protocol публичным API и
не используй host `printf` как formatter.

Добавь scalar boundary programs, iterative Fibonacci/factorial, conversion
negatives, `MIN/-1`, divide by zero, shift boundaries, formatter fixtures,
codec/verifier corruptions и generated-C O0/O2 differential.

Нельзя наследовать signed overflow/shift/FP contraction от host C.
Debug/Release/sanitizers/strict C17, обнови evidence/roadmap, commit и
немедленный push.

------

## 7. Lexical defer, D041

Реализуй доступную до task runtime часть lexical defer строго по D041.

Покрыть:
- normal fallthrough;
- return;
- break/continue;
- panic;
- nested scopes;
- LIFO;
- exactly once;
- запрет неподдерживаемых non-void/`Result` cleanup calls;
- явную panic/failure policy;
- verifier CFG invariants;
- одинаковый VM/native cleanup outcome.

Не реализуй scheduler/tasks и не расширяй defer beyond accepted semantics.
Добавь parser/checker/formatter, MIR negatives, VM/native differential,
fail-allocation и D041 acceptance fixture.

Обнови evidence/roadmap, оставив I6 not-started. После sanitizers и полного
CTest сделай commit и сразу push.

------

## 8. `ruda check`

Подключи authoritative `ruda check` к compiler service.

Требования toolchain.md §1, §2 и §6.1:
- root/module/package discovery;
- `file.ruda`, package и module targets;
- полный deterministic package check;
- human stderr diagnostics;
- newline-delimited versioned JSON diagnostics;
- нормативные exit codes;
- no program stdout pollution;
- overlays/service ownership не должны менять batch semantics;
- unsupported accepted syntax не маскировать как success.

Команду можно включить только если обязательные source surfaces checkpoint
1–7 реально проходят checker. Не делай subset-check, который возвращает
success для непроверенного кода.

Добавь CLI snapshots, clean-project fixtures, multi-file imports, invalid
manifests, allocation/I/O failures и no-change checks. Обнови examples,
roadmap/evidence. Debug/Release/sanitizers, commit и немедленный push.

------

## 9. `ruda run` и D046 stdout

Подключи `ruda run` с VM по умолчанию к canonical checker → typed MIR →
verified bytecode pipeline и реализуй принятый D046 user stdout.

Требования:
- normative root discovery;
- validate exactly `fn main(): void` или `fn main(): Result<void, E>`;
- выбрать canonical entry symbol;
- encode/decode/verify перед VM execution;
- `std/io.println(text: string): Result<void, io.Error>`;
- capability `"io.stdout"` с default grant для local `ruda run`;
- UTF-8 + один LF, line-atomic publish-before-`Ok`;
- program args/capabilities в утверждённом минимальном объёме;
- точные exit codes;
- stdout/stderr separation;
- bounds/arithmetic/explicit panic identity.

Acceptance:
- scalar;
- Fibonacci/factorial;
- arrays/sort;
- records/enums;
- Option/Result/?;
- matrix multiplication через `println(formatF64(...))?`, ожидаемые байты
  `415.0\n`;
- negative G4002/arithmetic panic.

Не печатай внутренний `ok:f64:` harness envelope. Не добавляй `print`,
`eprintln`, general `Display` или полный `std/io`. Blocking host write до I6
допустим только если observable D046 contract сохраняется.

Добавь CLI/process fixtures и sanitizer differential. Evidence/roadmap,
commit и сразу push.

------

## 10. `ruda build`

Подключи `ruda build` с native C17 backend по умолчанию.

Следуй execution.md §5 и toolchain.md §6.3:
- canonical typed MIR shared с VM;
- generated portable C17;
- runtime link ownership;
- root cleanup на всех exits;
- supported host toolchain discovery;
- `--output`;
- `--release`;
- `--emit c`;
- precise user diagnostics вместо raw compiler/linker errors;
- no network during build;
- тот же D046 stdout, что у `ruda run`.

Из чистого проекта собранный executable должен выдавать те же stdout/stderr,
exit code, f64 bits и panic identity, что `ruda run` VM.

Обязательно выполнить matrix и panic differential, O0/O2, fail-allocation,
strict C17, GCC/Clang и доступную Windows/MSVC compile/run проверку.
Обнови evidence/roadmap, commit и немедленный push.

------

## 11. `ruda test`

Реализуй минимальный обязательный встроенный test loop:
- discovery `@test fn(): void` в `*_test.ruda`;
- package-private и integration test boundaries;
- VM default;
- `--engine native`;
- `--engine both`;
- deterministic source-order reporting;
- per-test isolation;
- captured stdout/stderr без interleaving;
- `expect.output` сравнивает точные D046 байты;
- exact result/output/panic differential;
- normative exit codes.

Не подменяй `--engine both` двумя несвязанными success assertions:
используй execution equivalence/record-replay contract.

Добавь matrix test, panic parity, records, Option/Result, match, imports,
D040 и D041 fixtures. Неподдерживаемые filters/watch/cache должны честно
диагностироваться, а не молча игнорироваться.

Обнови toolchain evidence/roadmap, полный validation, commit и сразу push.

------

## 12. Final preview integration

Проведи финальную интеграцию Ruda local/private developer preview 0.1-dev.

Не добавляй новую семантику, кроме исправлений подтверждённых расхождений.
Из чистого checkout реально выполни обязательную матрицу:
- `ruda fmt --check`;
- `ruda check`;
- `ruda run`;
- `ruda test`;
- `ruda build`;
- `ruda test --engine both`;
- запуск собранного native executable.

Обязательные runnable programs:
1. scalar arithmetic/boundary panics;
2. Fibonacci/factorial;
3. sorting/searching `Array<int>`;
4. f64 matrix multiplication с D046 stdout `415.0\n`;
5. records/mutation/`identity.same`;
6. generic Option/Result/`mapError`/`?`;
7. exhaustive match;
8. multi-file imports;
9. D043 import-form/dot-call/interface;
10. D040 construction order;
11. D041 cleanup.

Сравни VM/native stdout, stderr, exit codes, f64 bits и panic
classification. Выполни Debug, Release, ASan/UBSan, Linux leak evidence,
Valgrind/nightly policy, fuzz campaigns, malformed MIR/bytecode,
generated freshness, docs, formatter idempotence и Tier 1 CI.

Обнови final evidence и roadmap. Помечай I2/I3/I4 verified только если
каждый их gate действительно выполнен на Tier 1. Не объявляй public 0.1.0.
После зелёного final audit сделай milestone commit, немедленно push и дай
честный финальный отчёт с SHA, командами, exit codes, ограничениями и рисками.
