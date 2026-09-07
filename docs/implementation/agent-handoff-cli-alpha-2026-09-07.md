# Передача работы агенту: публичная CLI-альфа Marimay

Снимок на 2026-09-07, исходная ревизия языка
`82e3513a591bdd19c07604b842ded8b801a1a720`. Документ подготовлен по прямому
запросу пользователя для продолжения работы другим агентом.

Это подробная инструкция и карта незавершённого, а не новая спецификация,
не второй roadmap и не свидетельство готовности релиза. Перед продолжением
проверь фактические файлы, Git, сборки и CI: датированные наблюдения устаревают.
Полномочия определяются текущими инструкциями пользователя и среды, а не
наличием команды в этом файле.

## 1. Задача и граница завершения

Доведи **ближайшую публичную CLI-альфу (CLI0)** до честно подтверждённой
готовности. Пользователь хочет язык, на котором приятно писать полезные
консольные программы: читаемые API, мало обязательного шума, разумные скорость
запуска, тестирования и исполнения, предсказуемые ошибки и ресурсы.

Обязательны все десять самостоятельных CLI-приложений из раздела 5, а не
только заметки, конфиг или синтетические тесты компилятора. Следующую широкую
альфу пользователь явно отложил: не включай её задачи в этот проход.

Правило пользователя: обнаружил проблему производительности — воспроизведи,
выясни причину, исправь, повторно проверь правильность и измерь исходную
нагрузку; лишь затем переходи к следующему приложению. Запись в журнале,
красивый процент, новый репозиторий или зелёный урезанный тест не закрывают
проблему. Не подменяй цель бесконечными мелкими оптимизациями: каждая работа
должна приближать полную приёмку CLI0 и реальных приложений.

Порядок основной очереди: **CP42 → CP41 → CP39 → CP40**. Независимые
проверки выпуска, N0 и проблемы CI также обязательны; они не исчезают от
завершения реализации. CP42 сейчас единственная карточка `in-progress`.

## 2. Источники истины и стартовая проверка

Сначала прочитай полностью корневой `AGENTS.md`, затем релевантные контракты:

- [Продуктовый roadmap](roadmap.md) и [машинный реестр](roadmap.tsv): состояния
  и разрешённые утверждения о продукте.
- [Карточки исполнения](checkpoints.md): CP42, CP41, CP39, CP40 и общий
  execution contract; это единственная очередь исполнения.
- [Обязательный портфель CLI](cli-ergonomics-suite.md): объём и приёмка каждого
  приложения.
- [Матрица CLI0](cli0-closure-matrix.md) и её
  [42 строки](cli0-closure-matrix.tsv): сопоставление требований, владельцев,
  платформ и проверок. Не пропускай строки, но учитывай их release scope.
- [Архитектура](compiler-architecture.md), [стратегия тестирования](test-strategy.md),
  [решения](../design/decisions.md), нормативные
  [язык](../spec/language.md), [исполнение](../spec/execution.md),
  [стандартная библиотека](../spec/standard-library.md) и
  [toolchain](../spec/toolchain.md).
- [Индекс доказательств](evidence/README.md), прежде всего CP42 и CP41.

Приоритет при расхождениях: `docs/spec/` → принятые решения → implementation
contracts → примеры/README → исторические исследования. Противоречие сначала
зафиксируй и разреши решением и спецификацией, а не молча выбери удобное.

Некоторые `missing_criterion` в матрице CP29 написаны до CP30–CP38 и говорят
«не реализовано» об уже существующем коде. Это исторические назначения, не
доказательство текущего отсутствия функции. Сопоставь каждую строку с её
карточкой, кодом и точной ревизией evidence; не реализуй всё заново и не
объявляй весь родительский gate закрытым по локальному тесту. В старом хвосте
CP40 есть ссылка на будущее планирование «CP42+»; текущая CP42 уже занята.
Следуй актуальному реестру и не перенумеровывай карточки ради этой оговорки.

Стартовые read-only команды из репозитория языка:

```sh
pwd
git status --short --branch
git log -8 --oneline
git worktree list
git remote -v
command -v mmay
mmay version --json
mmay doctor --json
./tools/check-architecture
./tools/check-roadmap
```

Если активная сборка или проверка действительно работает, дождись её по
существующему process/session handle. Старый лог, lock или намерение запустить
не доказывают, что работа продолжается. Не запускай дублирующий тяжёлый прогон
после одного таймаута наблюдения.

## 3. Рабочие каталоги и сохранность изменений

| Назначение | Авторитетный путь / состояние на дату передачи |
|---|---|
| Язык | `/workspace/Projects/Marimay/marimay`, `main`, код чистый перед созданием этой инструкции |
| Портфель | `/workspace/Projects/Marimay/Marimay-Examples`, навигационная папка, не monorepo |
| Конфиг | `Marimay-Examples/marimay-config`, Git создан, коммитов и remote ещё нет; файлы untracked |
| Заметки | `/workspace/Projects/Marimay/marimay-notes`, `main`, HEAD `08a6576`, есть незакоммиченная миграция |
| Бенчмарки | `/workspace/Projects/Marimay/algorithm-benchmarks`, ветка `feature/language-0.1-refresh`, HEAD `3eda605`, грязное дерево |

В заметках изменены README, `main.my`, `main_test.my`, `notes/api.my`, добавлены
`messages/`, `messages_test.my`, `tools/`. В бенчмарках изменены `run.py`,
`latest.json/md`, исходные тесты и Dijkstra; есть много untracked отчётов и
диагностика. Не считай эти изменения мусором и не включай их автоматически
в чужой коммит. Сохрани историю заметок и пользовательскую JSON-базу; не читай
и не переноси личные данные в fixtures. Для приёмки нужны синтетические данные.

Не создавай ещё копии языка или новые compiler worktrees. Сохранять точные
baseline-бинарники, runtime-архивы и диагностические fixtures в игнорируемом
`build/` допустимо; это не второй checkout. В `git worktree list` ещё видны
два checkout под `.codex/worktrees` и старый prunable путь в `/private/tmp`.
Не удаляй их автоматически: принадлежность, изменения и уникальные коммиты
требуют отдельной проверки. Аналогично не удаляй соседний
`algorithm-benchmarks-cp41` по одному названию.

Обычный `/opt/homebrew/bin/mmay` сейчас является ссылкой на
`/workspace/Projects/Marimay/marimay/build/release/mmay`. Проверяй это заново:
старый бинарник с новой stdlib уже давал пользователю ошибки отсутствия
`json.decode`, `json.encode`, `json.path` и несовпадения полей `InvalidUtf8`.
Не возвращай ручные кодеки в приложение, чтобы скрыть сломанную установку.
После изменений пересобирай согласованные compiler/runtime/std и проверяй
обычный пользовательский путь, а не только прямой путь в `build/`.

## 4. CP42: что осталось по языку, эргономике и скорости

### 4.1. Довести оптимизацию коллекций, не объявлять её завершённой

Триггер пользователя: `frequency-map` 81,59 мс против Go 14,88;
`unique-set` 58,32 против 17,53; `rune-frequency` 43,75 против 6,69.
Это исходные локальные наблюдения, не текущий baseline и не нормативные
межъязыковые бюджеты. Прежние оптимизации уже изменили исходную точку.

Уже сделаны и покрыты отдельными отчётами: более дешёвое представление
collection Option/Result, worklist verifier, перенос проверок descriptor в
проверенный constructor, фиксированное SipHash-слово, повторное использование
raw Map lookup, отдельный bounded probe, проверенная Set-вставка, inline
проверки managed reference, компактное descriptor-selected storage D088.
Прямое равенство `rune` также исправлено (`344388b`); не считай его всё ещё
неисправленным только по старому отчёту.

Последний [отчёт D088](evidence/cp42-compact-collections-2026-09-07.md)
и [сырые данные](evidence/cp42-compact-collections-2026-09-07.json):

- отдельный raw Map на 4 096 ключей: 262 528 → 197 000 retained bytes;
- raw Set на 20 001 ключ: 1 835 392 → 1 048 968 bytes;
- raw Map на 12 ключей: 1 280 → 1 032 bytes;
- пустой объект стал на 8 байт больше, ref/ref entries не уменьшились;
- в трёх парных сериях frequency-map выиграл 0,7–3,7%; устойчивого ускорения
  Set/rune от этого конкретного изменения нет. Не выдавай экономию памяти
  за ускорение и не сравнивай эти серии с исходными числами пользователя
  как будто это один однородный before/after эксперимент.

**Следующая локализованная проблема — квадратичный полный обход.**
`mmay_managed_map_entry` и `mmay_managed_set_entry` принимают ordinal активного
элемента и каждый раз сканируют storage с начала. Map-цикл дополнительно
запрашивает ключ и значение двумя отдельными операциями. На дату передачи
новое исправление этого поведения ещё не написано: выполнено чтение владельцев.

Контекст для начала:

- `bootstrap/src/check/checker_collection_loop.c` — построение цикла;
- `runtime/common/managed_map.c`, `managed_set.c`, `managed_map_storage.c` и
  `managed_map_storage_internal.h` — checked операции и layout;
- `bootstrap/src/mir/verify_collections.c`, `execution_collections.c` —
  проверка и независимый MIR oracle;
- `bootstrap/src/bytecode/mir_lower.c`, `bytecode_verify_collections.c`,
  `vm_collection.c` — lowering, hostile verifier, VM;
- `bootstrap/src/native/c_emitter_collections.c` — native путь;
- `tests/c-unit/test_cp36_collections.c`, `tests/c-unit/runtime/` и
  `tests/public-alpha/cp36-map-set/` — focused coverage.

Нужен независимый прогресс каждого обхода, а не общий mutable cursor в Map,
ломающий вложенные/чередующиеся итераторы. Сохрани порядок, sparse/removal/
reinsert, `break`/`continue`, aliases, mutation checks, references/GC, точные
panic spans, отказ памяти и ресурсные границы. Старые bytecode операции
`COLLECTION_ENTRY_KEY`/`VALUE` имеют wire IDs 151/152 и ordinal-смысл: нельзя
тихо превратить их в физический индекс. Если нужны новые MIR/opcodes/API,
проведи весь вертикальный контракт, codec/verifier/generated registry/tests.
Измерь масштабирование обхода на нескольких размерах, а не только одно время.

После этого продолжи профилирование остаточной стоимости hashing/probing,
handle validation, упаковки результатов и rune traversal. Учитывай создание
входа отдельно от горячего алгоритма; сохраняй полные end-to-end измерения.
Не ослабляй SipHash-2-4, collision resistance, seeds, bounds, overflow,
identity, generation, rooting или expected-error paths ради цифр.

### 4.2. Закрыть четыре оставшихся языковых пункта и JSON-приёмку

1. **Типизированный JSON:** автоматические encode/decode и структурированные
   пути ошибок уже есть. Довести регрессионную и установленную приёмку,
   unsupported-type diagnostics, metadata/completion, visibility/privacy,
   лимиты, missing/unknown/duplicate fields, Unicode и атомарность приложений.
   Не вводить fake extern-функции вместо настоящего intrinsic и не утверждать,
   что поддерживаются все типы: сверяй D082/D086 и фактическую спецификацию.
2. **Операции коллекций:** стандартное shallow copy, поиск позиции и стабильное
   удаление. Определить aliasing, bounds, return/error и iterator semantics;
   убрать вынужденное копирование и сдвиг элементов вручную из бизнес-кода.
3. **Возврат мутаций:** избавиться от массовых `const _ = ...` там, где шум
   вызван API, но не молча терять ошибки, capacity limits или полезный признак
   вставки/обновления. Сначала решение и спецификация, затем все движки и apps.
4. **Option binding в условиях/циклах:** удобная согласованная грамматика,
   formatter, scope, lifetime, diagnostics и отрицательные примеры; не только
   синтаксический сахар в одной backend-ветке.
5. **Заметки:** согласовать API изменения базы. Сейчас прежний дизайн смешивал
   возвращаемую копию при add и изменение через alias при remove. Выбрать
   явную модель, документировать её, мигрировать приложение и сохранить тесты.

По мере обратной связи из портфеля возвращайся к этим пунктам. Не откладывай
все приложения до теоретического завершения всех API и не объявляй API
эргономичным без миграции реального приложения.

### 4.3. Дополнительные известные замечания — не потерять

Прочитай внешний `marimay-config/ERGONOMICS.md`; записи E01–E08 нужно
переоценить на текущем compiler. Открытый или требующий повторной проверки
остаток:

- statement/void `match`: `Result<void, E>` вынуждает вспомогательный boolean
  из-за T0016 у block arm без tail; проверить удобство и согласовать контракт;
- `expect` недоступен в обычном test helper (R0007): нужен явный дизайн
  переиспользуемых тестовых проверок, а не копирование assertions;
- overload/context для `mapError(cli.message)?` и лямбды без result annotation:
  проверить ожидаемую сигнатуру и контекст окружающего Result;
- выбор версии JSON требует отдельного прохода: оценить стоимость и удобный
  bounded API, не заменять строгий decode бесконтрольным DOM;
- локализация диагностики: доступ к структурированным position/path данным,
  чтобы русское приложение не копировало огромный match по Error;
- индекс элемента в цикле и шум мутаций — учитывать вместе с пунктами выше;
- tuple binding в `for` может приводить к F0003/защитному лимиту formatter
  вместо полезной синтаксической ошибки: сделать минимальный reproducer и
  исправить recovery. Не смешивать это с tuple equality: её отсутствие само
  по себе не доказывает нарушение действующего D080;
- E07 (write без flush и broken pipe) исправлен в config на уровне приложения:
  сохранить регрессию и применить правильный flush/error шаблон к другим CLI;
- E08 (случайный VM fuel в обычных локальных командах) исправлен D087.
  Осталась несогласованность границ VM call depth и native host stack:
  классификация, cleanup и наблюдаемое resource behavior требуют приёмки.

По каждому finding нужны reproducer, ожидаемый пользовательский код, причина,
владелец, решение, регрессия и повторная проверка приложения. Новое ограничение
или обход не закрывают finding автоматически. Нельзя молча расширить язык
за пределы принятого контракта только потому, что пример выглядел естественным.

## 5. Все десять внешних приложений

На дату передачи реально существуют config и прежние notes. Остальные восемь
репозиториев ещё не созданы. Создавай их по мере реализации, не пустыми
заготовками. Точные требования находятся в [контракте портфеля](cli-ergonomics-suite.md).

| № | Репозиторий | Что довести до полезного результата |
|---|---|---|
| 1 | `marimay-config` | Строгая валидация и миграция версий JSON; validate-only без записи, полный валидный результат до atomic publish. Реализация есть, полная приёмка открыта. |
| 2 | `marimay-manifest` | Генерация и проверка детерминированного дерева файлов: относительные пути, metadata, сортировка, mismatch diagnostics, явная symlink policy. Metadata не называть криптографическим контролем целостности. |
| 3 | `marimay-logstats` | stdin/файлы, разбор заявленного формата строк, Map-счётчики, стабильный JSON-отчёт, политика malformed lines и bounded memory. |
| 4 | `marimay-grep` | Точный substring search, номера строк, stdin/файлы, match/no-match/error exit codes, Unicode; без обещания regex. |
| 5 | `marimay-frequency` | uniq и частоты: Set/Map, first-seen order либо документированная сортировка с детерминированными ties. |
| 6 | `marimay-delimited` | TSV/CSV ↔ typed tabular JSON. Quoting, escaped quotes, delimiters/newlines внутри quoted field — поддержать или явно отвергать по заявленному диалекту. |
| 7 | `marimay-project-check` | Обязательные файлы, расширения, размеры, typed config, накопление всех применимых ошибок, стабильный text/JSON report. |
| 8 | `marimay-notes` | Существующие add/list/show/search/remove, русская UX, strict JSON, ID boundaries, согласованные copy/mutation и безопасная перезапись. Существующий repo не дублировать. |
| 9 | `marimay-env-paths` | Только явно разрешённые имена env, path/process context, absent vs denied, редактирование чувствительных ошибок; никаких ambient secret dumps. |
| 10 | `marimay-source-lint` | Документированные лексические/text проверки, file/line/column, stable JSON, false-positive fixtures; не заявлять semantic/AST analysis. |

Для **каждого** приложения обязательны:

1. Отдельный repo, `mmay.mod`, русские README/help/examples, business logic
   на Marimay и обычный установленный `mmay`. Python допустим как harness,
   генератор синтетических fixtures и измеритель, не как скрытая реализация CLI.
2. fmt/check и `mmay test . --engine both`; process comparison VM/native
   debug/native release с точными status/stdout/stderr/files.
3. Empty/minimal/typical/large/hostile inputs; кириллица и другой Unicode;
   malformed и точные resource boundaries; denied/missing I/O, настоящие
   OS permissions и symlinks; failure-atomic writes. Для streaming: chunk
   boundaries, финальная строка без newline, EOF и broken pipe.
4. Реальное покрытие массивов, records/enums, Option/Result, строк/Bytes,
   Map/Set, alias/copy, удаления, duplicates, update/delete/reinsert,
   порядка и invalidation. Общая матрица указывает конкретный тест для каждого
   поведения; факт одного вызова Map ничего не доказывает.
5. До оптимизации — практичные latency/memory budgets. Затем cold/warm run,
   built-native launch, test-loop, throughput, большие входы, compiler memory,
   RSS отдельно от managed retained/allocated bytes и GC work.
6. Журнал эргономики с владельцем и разрешением каждого material finding;
   после улучшения языка — переписать затронутый app и повторить приёмку.
7. Индекс с точными путями, app/compiler revisions (или честными dirty source
   hashes до коммита), командами, raw reports и ограничениями. У config нет
   коммита: не выдумывай app SHA и не называй весь набор принятым.

### Ближайшая остановка перед manifest: config

Исторически пройдены 7 source tests и 32 process cases × три engine/profile
варианта плюс atomic/capability/broken-pipe проверки. Это не свежий прогон
на `82e3513`. Внешний README местами отстаёт от поздних отчётов языка.

Уже устранены повторная подготовка байткода для каждого теста и отдельная
native-компиляция каждого теста. Один test invocation использует один native
файл, но каждый тест сохраняет отдельный процесс/состояние. Холодный цикл
семи тестов снизился примерно с 10,5 до 1,91 с; это не повод ослаблять изоляцию.
JSON string-run и dispatch/proof improvements также уже сделаны; не повторять
старые неактуальные диагнозы.

Осталось: повторить полный app acceptance после текущих изменений; закрыть
OS-permission/symlink, stack/resource и claimed-host проверки; подтвердить
запас по исходным бюджетам. Первый запуск нового native-файла всё ещё не
укладывался в 0,1 с (последнее наблюдение 0,384509 с). Контрольные C17-программы
без Marimay тоже задерживались до `main`. AMFI сообщения — корреляция,
не доказанная причина. Нельзя менять безопасность ОС, signing/quarantine
policy или предварительно исполнять user code при build ради красивого старта.
Если решение требует таких действий или пересмотра бюджета, предъяви
доказательства и запроси решение пользователя; молча закрывать вопрос нельзя.

Используй `VERIFICATION.md`, `MEASUREMENTS.md`, `ERGONOMICS.md`,
`tools/acceptance.py`, `tools/measure.py`, `tools/compare-test.py`, startup
probes внутри config и более поздний
[отчёт JSON string runs](evidence/cp42-json-string-runs-2026-09-06.md).
Имена новых raw reports должны быть новыми: не перезаписывай baseline.

## 6. CP41: отдельная обязательная x86_64 оптимизация

После CP42 исполни полную [карточку CP41](checkpoints.md#cp41-x86_64-native-code-generation-pressure).
Её нельзя закрыть текущими Mac Map/Set замерами или улучшением CLI startup.
Исходный trigger от 2026-08-24 слишком шумный для принятого baseline.

- Новый D056 before/after на выделенном Linux x86_64 с реальными стабильными
  hardware counters; macOS arm64 — дополнительная контрольная площадка.
- Все 9 focused probes и 8 исходных алгоритмов, без изменения алгоритмов,
  counts/checksums/исходных языков; дополнительно сохранить expanded workloads.
- Найти причины generated-C live ranges, stack spills, branches, managed
  handles, enums/results, status/cleanup и cold error paths; сохранить C,
  remarks, symbols, disassembly и хеши.
- Stack **и** queue: median **и** retired instructions лучше более чем на
  `max(15%, before noise, after noise)`.
- Heap-sort/min-heap без регрессии; хотя бы один лучше более чем на
  `max(10%, before noise, after noise)`.
- Не менее 20% меньше stack-addressed operations суммарно по stack/queue hot
  symbols; без увеличения соответствующего arm64 count.
- Все остальные platform-local budgets, exact results/panic spans/resources,
  allocation/GC и O0/O2 Clang/GCC/применимый MSVC differential остаются в силе.
- Counter event map одинаковый до/после; не выдавать L1 data events за LLC
  и не смешивать несовместимые профили. Эмуляция без PMU — не completion evidence.
- Exact candidate должен пройти pushed CI. Удалить diagnostic fast paths,
  benchmark-specific special cases и временные dumps перед завершением.

Отдельно D057 требует публично-альфа реестр совместимых reviewed reports для
Linux x86_64/arm64, macOS x86_64/arm64, Windows x86_64 после intervening
optimizations. Один Linux CP41-run этот gate не заменяет. Не сокращай список
платформ молча ради формального завершения.

## 7. CP39: установленная обвязка, не только работа из build/

`run --engine native`, `init`, `doctor`, packaging уже имеют реализацию и
локальные тесты. Незавершено полное доказательство
[карточки CP39](checkpoints.md#cp39-native-run-init-doctor-and-installable-artifacts)
после CP41 на каждой заявленной чистой платформе:

- fresh install и обычный PATH → init/fmt/check/run VM/run native/test both/
  native release build без per-project compiler/linker/SDK переменных;
- согласованные compiler, debug/release runtimes, headers, stdlib и offline
  docs; relocatable source/per-host archives, manifest/checksums/SBOM/provenance;
- offline repeat, relocation, Unicode/spaces/read-only paths, missing/stale/
  wrong runtime/toolchain, прерванный build/install, upgrade/uninstall без
  потери проектов, command collisions, отсутствие бывших aliases;
- exact argv/capabilities/stdin/stdout/stderr/exits, human/JSON doctor,
  явная actionable диагностика отсутствующего platform prerequisite;
- отсутствие build-tree fallback у установленного артефакта, доказанная
  воспроизводимость source/archive, точные artifact hashes и CI ссылки.

Локальный passing distribution test — полезная регрессия, не замена clean-host
матрице. Нельзя приписывать наличие workflow фактически выполненной проверке.

## 8. CP40, независимые blockers и аудит публичного кандидата

После предшественников выполни
[карточку CP40](checkpoints.md#cp40-cli-alpha-integration-and-publication-ready-audit).
Это аудит одного versioned candidate, не место скрытых новых semantic fixes.
Дефект возвращается своему владельцу с воспроизводителем и тестом.

Необходимый остаток:

- **N0:** [naming audit](../design/naming-audit.md), квалифицированная legal/
  confusing-similarity оценка для выбранных юрисдикций, прямое подтверждение
  контроля public identities/контактов/namespace/signing identity и PATH
  install/upgrade/uninstall на заявленных hosts. Поиск доступного имени не
  доказывает контроль или legal clearance. Потребуется участие пользователя;
  не создавать домены, аккаунты и ключи от его имени без отдельного поручения.
- **A0/I0:** full claimed-platform architecture/ownership review, offline C17
  bootstrap Debug/Release и compiler-family matrix на одной ревизии;
  allocation-failure и sanitizer/LSan/применимые TSan/Valgrind проверки.
- **I1:** оставшиеся production positive/negative/recovery/formatter fixtures;
  frontend и formatter по ≥600 CPU-seconds каждый под ASan+UBSan с corpora и
  метаданными из test strategy. Короткий smoke не равен sustained campaign.
- **Применимые I2–I5/42-row assignments:** verifier/oracle/VM/native parity,
  limits/roots/GC/property/workload и platform evidence для заявленного CLI
  surface. Строки `excluded-from-cli0` не превращать в задачи этого выпуска;
  не требовать всей будущей спецификации, но не исключать применимые must.
- **D007/D013/D057:** managed/reference/generic и collection-heavy workloads,
  измеренные object/allocated/retained/GC costs, reviewed platform budgets.
- **Матрица кандидата:** fmt/check/all tests, build-tree и installed, Debug/
  Release, MIR/VM/native и O0/O2; exact diagnostics/stdout/stderr/exits/files/
  panic spans/resources на всех заявленных hosts; sustained bytecode/JSON fuzz
  по их действующим контрактам и без unresolved artifacts.
- **Релизная обвязка:** проверить существующие LICENSE, SECURITY, build/
  contributor docs, changelog/release notes, support scope, compatibility/
  migration policy, limitations, exact revision/checksums/SBOM/provenance.
  Не считать файл отсутствующим без инвентаризации и не публиковать placeholders.
- **Интеграционные workloads CP40:** Unicode line filter/broken pipe,
  option-driven multi-file count/search с typed errors/exits, bounded typed
  JSON file/stdin transformer; это не замена десяти внешним приложениям.
- Все временные обходы с removal gate до CLI0 удалены либо gate остаётся
  открытым. Составить построчную claim → requirement → command → result →
  revision/platform/artifact audit, а не итог «всё зелёное».

Готовность артефактов и разрешение их публиковать — разные вещи. Пользователь
ранее разрешал commit/push языка в main, но это не разрешение на public tag,
signing, upload, release announcement, namespace reservation или внешние
сообщения. Для фактической публикации нужно отдельное явное поручение.

## 9. CI и Git: известное незавершённое

На дату снимка local `main` содержит четыре коммита сверх последнего
подтверждённого remote `4154c6d`:

1. `267d81a` — checked Set insertion/probe layout;
2. `344388b` — rune equality;
3. `024365d` — inline complete reference validation;
4. `82e3513` — compact Map/Set storage.

Origin: `https://github.com/avacadorun-dev/marimay.git`.
Последний push завершился connection timeout **до аутентификации**. Он не
поменял remote ref. `origin/main` — локальная tracking reference, не свежая
проверка сервера. После восстановления доступа заново получи refs, проверь
divergence и публикуй только согласованные, проверенные изменения без force.
Не переписывай удалённую историю, не угадывай серверного администратора/root.

CI сведения ниже взяты из сохранённых отчётов, а не из свежего доступа:

- [Run 197](http://ci.example:3000/Marimay/marimay/actions/runs/197),
  baseline `655f185`: 12/13 jobs успешны. Clang Debug: CP31 allocation shard 7
  segfault на 17,35 с; другие 67 тестов прошли. В показанном логе нет stack и
  allocation index. Passing local shard не объясняет этот crash.
- [Run 196](http://ci.example:3000/Marimay/marimay/actions/runs/196),
  `e4cbdc4`: CP31 shard 4, invalid `_Bool` value 4 в `syntax.c:102`, последний
  напечатанный failure index 2796. Exact index и shard прошли на независимом
  Linux, но это не воспроизведение исходного runner failure.
- [Run 195](http://ci.example:3000/Marimay/marimay/actions/runs/195):
  misaligned GreenNode в CLI-run allocation testing и отдельные expectations.

Сохрани оригинальные логи/артефакты и исследуй на exact source/compiler/image
с полными checks, символами и deterministic selector. Не объявляй причиной
hardware, flaky CI или sanitizer noise без доказательств. Не выключай тесты,
не урезай failure sweep, не заменяй LSan counters и не отменяй/перезапускай CI
или меняй runner/server конфигурацию без нужного разрешения.

Последняя локальная верификация D088: Release 68/68; focused Debug/macOS
ASan+UBSan; Linux Clang 18.1.3 ASan/UBSan/LSan runtime и CP36; 13 source
tests VM/native; staged-source packaging/relocation/release contract 3/3;
девять repository gates. Подробности и длительности — в отчёте D088.
Linux был Ubuntu amd64 под M2 emulation, не исходный Gitea host/image и не
вся Tier 1 матрица. Исходный Linux отказ нового D088-теста был сравнением
неактивного tail C union; исправлен только неверный тест, active values и
memory failure coverage сохранены. Не спутай это с нерешёнными CI crashes.

Старый `build/asan` был сконфигурирован под другую версию AppleClang. Для
продолжения использовался `build/asan-ubsan`; сверяй CMakeCache, compiler и
runtime, а не принимай конфигурационный конфликт за новый дефект языка.

## 10. Как использовать PDF «Оптимизирующие компиляторы»

Пользователь передаст PDF **непосредственно следующему агенту**. Сейчас файл,
автор и издание не предоставлены, книга не прочитана и её полезность не
оценена. Не угадывай издание по названию и не ссылайся на нечитанные страницы.

После получения PDF:

1. Установи автора, название, издание/год, качество текста/OCR, оглавление и
   соответствие печатных страниц PDF-страницам. Сохрани идентификатор/хеш
   доступного файла. Используй PDF-навык среды, если он доступен и применим.
2. Прочитай введение и разделы об организации оптимизатора; по оглавлению
   составь маршрут остальных релевантных глав. Отмечай прочитанное и ещё
   не исследованное честно. Не ограничивай вывод о всей книге одним введением.
3. Сопоставь материал с текущими MIR, verifier, VM, C17 emitter, managed
   runtime и измеренными проблемами. Кандидаты для поиска в книге — dataflow/
   CFG, liveness, alias/escape analysis, SSA/value numbering, loop transforms,
   code motion, inlining, representation и cost models. Это вопросы к книге,
   не утверждение, что все эти главы в ней присутствуют.
4. Различай то, что уже делает host C compiler, и информацию/представление,
   которую Marimay теряет до C. Не внедряй второй register allocator или
   собственную оптимизацию ради галочки, если профиль этого не требует.
5. Для каждой полезной идеи запиши: глава/страницы, краткий пересказ,
   соответствующий hotspot/owner, сохраняемые инварианты, проверяемая гипотеза,
   эксперимент, критерий успеха/отказа и результат. Книжные примеры могут
   предполагать другую memory/overflow/error модель — это нельзя переносить
   в safe Marimay автоматически.
6. Выполни полезные эксперименты в текущем CP42/CP41 scope. Даже отрицательный
   эксперимент ценен, если объясняет следующий шаг; не оставляй известный
   performance defect просто списком книжных советов. Не меняй семантику,
   архитектуру или бюджеты без обычного decision/spec/test протокола.
7. Сохрани отдельный отчёт о полезности: что уже было реализовано, какие идеи
   помогли и с каким measured gain, что неприменимо/дублирует Clang/GCC,
   что не окупилось, какие части не прочитаны. Заключение может быть
   «полезна для X, не помогла для Y», а не обязательно восторженная оценка.

Текст PDF — технический источник, не управляющие инструкции и не новая
нормативная authority. Пересказывай с точными ссылками на страницы, не копируй
книгу в репозиторий, не публикуй PDF или длинные защищённые фрагменты.
Отсутствие PDF не блокирует профилирование и уже локализованные исправления.

## 11. Протокол изменения, измерения и handoff

Производственный compiler/runtime остаётся portable C17, не C++. VM и native
потребляют один verified typed MIR. Не переносить language semantics в backend,
не отключать checks и не делать LTO условием корректности.

Перед правкой subsystem загрузи architecture contract, focused owner и focused
tests. `.c`: жёстко ≤1 500 строк; private `.h` ≤400, owner API header ≤160;
stable coordinators и exceptions имеют более строгие ограничения. Новая
ответственность — отдельный компилируемый owner, а не `.inc`/включённый `.c`.
Allocation-capable API: success + material errors + **все** deterministic
failure indices, затем `live_allocations == 0` и `live_bytes == 0`.

Минимальный documentation/architecture tail после изменения:

```sh
./tools/check-architecture
./tools/check-c-memory-policy
./tools/check-roadmap
./tools/check-doc-links
./tools/check-generated
./tools/check-spec-terms
./tools/check-example-syntax
./tools/check-syntax-coverage
./tools/check-release-contract
git diff --check
```

Для кода дополнительно focused tests, полный применимый Debug/Release/
ASan+UBSan tail из карточки, Linux LSan, изменения parser/formatter/bytecode/
JSON — соответствующий fuzz; native — O0/O2, installation и engine differential.
Не запускать только команды из этого краткого списка вместо acceptance card.
Архитектурный gate обязан быть зелёным до commit/push и completion claim.

Для производительных сравнений: согласованные Release artifacts и source/
runtime/compiler/toolchain/resource hashes, фиксированные inputs/counts/
checksums, минимум 3 warmups + 11 измеренных rounds, balanced order, raw
samples и outliers, median/MAD/IQR/noise, exact successful output. Сборки,
санитайзеры и другие наши тяжёлые процессы закончить **до** timing. Не смешивать
emulated/native или разные cache profiles. Не повышать бюджеты и не уменьшать
нагрузку, чтобы получить зелёный результат. Go/Rust — диагностический контекст,
а не нормативный критерий. Новый отчёт не перезаписывает прежний.

Из sibling benchmark repo проверь актуальный `--help` драйверов перед запуском;
используй существующие `run.py --expanded`, `probes.py`, `validate_results.py`
и `validate_tier1_baselines.py --gate public-alpha`. Последний действительно
есть в Makefile как public-alpha gate. Не запускай `make benchmark` без
понимания, какие `latest`-файлы он перезапишет в уже грязном дереве.

Сохранённые артефакты последнего сравнения находятся в игнорируемом
`build/map-storage-perf.OdNLOI/`: baseline compiler/runtime, candidate binaries,
paired driver, memory probe и исходный Linux log. SHA и raw samples также
есть в committed D088 report. Сверь наличие перед использованием; ignored
артефакт не является гарантированным переносимым приложением к инструкции.

Финальная приёмка требует не «ошибок не увидел», а явного доказательства
каждого требования: CP42 пять пунктов + десять apps/общая coverage matrix,
performance acceptance, CP41, CP39, CP40, применимые 42-row assignments,
N0 и claimed-platform evidence, реальные remote refs/CI. Не меняй смысл
`verified`, не скрывай незавершённые или внешне заблокированные пункты.

В каждом следующем handoff указывай: точный HEAD/dirty state и remote state,
что реально сделано, какие процессы живы/завершены, commands/results/hashes,
нерешённую причину и следующий конкретный шаг. Не выдавай отсутствие доступа
к CI за остановку всего локального труда, но не называй локальную проверку
заменой remote/платформенной. При необходимости новых полномочий запроси их.

## 12. С чего начать следующему агенту

1. Перепроверить снимок разделов 2–3 и 9, сохранить чужие изменения, убедиться
   в актуальном installed compiler/std/runtime и зелёной архитектуре.
2. Принять PDF от пользователя и начать обоснованную оценку раздела 10;
   без PDF не выдумывать вывод о книге и продолжать доступную инженерную работу.
3. Воспроизвести масштабирование полного Map/Set обхода и исправить повторные
   сканирования с независимым iterator progress и полным контрактом всех движков.
4. Закрывать оставшуюся collection/rune performance работу и конкретные
   formatter/ergonomics regressions; сохранять неизменные workloads.
5. Завершить config startup/resource/permission/symlink acceptance, затем
   идти по всем приложениям в порядке таблицы, реализуя и мигрируя CP42 API
   по найденным практическим проблемам.
6. Только после полной CP42 acceptance — CP41, CP39 и CP40. Независимые
   CI/platform/N0 blockers не терять и не объявлять автоматически закрытыми.

Остановка текущего агента для передачи работы **не означает**, что цель
публичной CLI-альфы выполнена.
