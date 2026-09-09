# Архитектурный разбор пути к паритету производительности

Дата: 2026-09-09. Контрольная ревизия компилятора: `9fa416a`; SHA-256
установленного `mmay`: `15e6a5077e6b18bfc56908563f19cb08461b12244bf8ebd69561a197e8dd45b4`.
Авторитетный локальный отчёт:
`algorithm-benchmarks/benchmarks/results/comprehensive-local.json`, SHA-256
`732e069f9cd51e6df67230d3547ebe3fd53a26a11ba5ecb387380a0a96f9c3ee`.
Он содержит 32 неизменённых workload, 3 process warmup и 11 samples, все
checksum прошли. Среда Apple M2/macOS, изоляция хоста `local-unverified`.

Это рабочая архитектура исполнения, а не ослабление CP42/CP41, платформ,
лимитов или семантики. Цель остаётся: VM на уровне Bun в этой матрице и native
на уровне Go/Rust при тех же входах. Один локальный хост не закрывает Tier 1.

## Что доказывает полная матрица

| Срез | Сумма медиан, ms | Геометрическое отношение |
|---|---:|---:|
| Marimay native | 1 034,26 | 2,02× Go; 2,19× Rust |
| Go native | 548,33 | — |
| Rust native | 454,77 | — |
| Marimay VM source run | 36 655,01 | 15,54× Bun; 20,73× Marimay native |
| Bun / TypeScript | 1 173,52 | — |

VM-разрыв имеет два масштаба. У коротких прикладных задач VM занимает
115–580 ms, а Bun 17–50 ms: здесь заметны загрузка/проверка/lowering и запуск.
У алгоритмических циклов разрыв остаётся после подготовки: DSU 98,3× Bun,
BFS 72,2×, insertion 70,1×, min-heap 48,5×. Поэтому кэш одной подготовки
не решит длинные нагрузки, а ускорение только dispatch не решит короткие.

Счётчик D104/D106 после упрощения MIR фиксирует 1 331 188 719 исполняемых
bytecode-инструкций для insertion, 524 872 376 для DSU и 502 462 668 для
Dijkstra. Для insertion обязательные алгоритмические итерации превращаются
примерно в восемь VM-инструкций каждая. Даже нулевая стоимость source pipeline
оставляет миллиарды чтений 88-байтового `MarimayBytecodeInstruction`, выборов
handler и обновлений cursor/fuel. Это исключает ставку на одну локальную
оптимизацию runtime-вызова как путь к Bun.

CPU samples помещают почти всё оставшееся время длинных алгоритмов
непосредственно в `mmay_bytecode_run_hot`. Эксперимент с общим record-регионом
дал отрицательный результат: семь чередующихся запусков ухудшили
`record-reference-copy` на 3,4%, `record-create` на 2,0%,
`catalog-interface` на 1,5% и Dijkstra на 1,1%. Совмещение compact и predecoded
helpers в одном owner дополнительно изменило inlining и регрессировало
`heap-sort`. После удаления D107 и выделения `vm_image_run.c` машинный размер
compact loop вернулся ровно к прежним 2 404 байтам, а BFS/Dijkstra — к паритету
с контрольной сборкой. Это измеренно запрещает один разрастающийся universal
dispatch-loop; families должны оставаться отдельными компактными tiers.

## Выбранная VM-архитектура

### 1. Verified execution image

После проверки bytecode строится ограниченный внутренний execution image.
Он не меняет wire-формат и не становится новым источником семантики. Для каждой
функции он хранит плоский плотный поток typed operations, прямые targets веток,
индексы уже проверенных callees/layouts и связь каждой операции с исходной
инструкцией/span. Registers, blocks и metadata уже ограничены verifier; любое
непредставимое значение отклоняет image и использует существующий dispatcher.

Image строится транзакционно через центральный allocator. Отказ памяти/лимита
оставляет исходный verified program доступным для точного fallback. Удаление
image освобождает все bytes; allocation-failure sweep обязан пройти каждый
достижимый индекс и получить `live_allocations == 0`, `live_bytes == 0`.

Первый measured gate: отдельные compact runners для scalar/control, scalar
arrays и полностью совпавших составных shapes. Их кодовые размеры фиксируются в evidence. Family,
которая не используется функцией, не раздувает её steady-state loop. Direct
callee/layout binding устраняет повторный symbol/vector поиск, но не пропускает
проверку hostile bytecode: binding создаётся только после полного verify.

### 2. Семантические superinstructions

Image объединяет только доказанные последовательности общего typed MIR:
require-index+get/set, compare+branch, arithmetic+copy и induction update+jump.
Каждая составная операция хранит число исходных bytecode steps и карту границ.
При недостатке per-frame/shared fuel она исполняет только разрешённый префикс
или возвращается в точную исходную позицию; trap span и момент публикации
остаются прежними. Potentially failing, allocating, host, call, GC и panic
операции не переходят границу без отдельного доказательства.

Gate этого уровня — уменьшение числа dispatch iterations на фактических
insertion/DSU/Dijkstra при неизменном числе логических bytecode steps и полном
совпадении finite-fuel результатов на каждой границе. Если paired 11-sample
серия не даёт существенного общего выигрыша без регрессий, слой пересматривается,
а не объявляется достаточным.

### 3. Hot-loop compilation tier

Миллиарды логических операций указывают на большой расход исполнения, но не
доказывают необходимость собственного JIT. После проверки переносимости этот
этап остаётся кандидатом: его нужно сравнить с общими преобразованиями MIR,
дешёвыми активациями и переносимой специализацией интерпретатора. Выбор
собственного машинного emitter или готового backend требует отдельного
обоснования. Если выбирается компиляция горячих scalar/control/array traces,
она остаётся tier той же VM над тем же typed MIR, без распознавания benchmark
или вызова алгоритма из runtime.

Tier допускается только для verified operations и обязан иметь side exit на
каждой failing/allocating/call/GC/host boundary. Он получает явный fuel budget,
публикует register/heap state в bytecode order и возвращает исходный pc/span.
Неподдерживаемый host или исчерпанный code budget использует compact interpreter.
Code cache ограничен, принадлежит execution, не является W^X-исключением без
платформенного owner и не входит в wire bundle. Перед выбором emitter фиксируются
доступные Tier 1 механизмы W^X и signed-runtime policy; внешний C compiler и
скрытая подмена `--engine vm` native executable не считаются готовым решением.

Acceptance: все 32 workload, исходные параметры, VM source-run boundary,
3+11 observations и checksum; paired old/new series для самых длинных и всех
регрессировавших cells; exact finite fuel, panic spans, allocation failure,
forced GC, alias/rebinding, sanitizer и architecture gates. Цель этого уровня —
VM/Bun ≤ 1,25× по каждой длинной CPU-bound нагрузке и отсутствие ухудшения
коротких задач; общий вердикт не выводится из среднего.

### 4. Подготовка и кэш

Source checking/lowering/verify остаются отдельным измеряемым слоем. После
execution-image контракта можно хранить verified image по полному ключу из
compiler/language/runtime revision, source/module/lock hashes, target profile,
permissions и resource-sensitive flags. Повреждение, неизвестная версия или
несовпадение любого ключа ведёт к полной проверке. Кэш не меняет первый запуск,
не скрывает compilation в warmup и получает cold/warm столбцы отдельно.

## Выбранная native-архитектура

Native уже укладывается рядом с Go в heap-sort, stack, queue, min-heap,
Dijkstra и близок в простых integer arrays. Переписывать общий integer runtime
без профиля не нужно. Разрыв сгруппирован:

| Семейство | Marimay/Go | Подтверждённый слой |
|---|---:|---|
| word-frequency | 7,16× | owned split result и строки; ранее 54,9% sampled stacks |
| normalize-title | 6,26× | grapheme result 26,3% и Option construction 28,4% |
| title-search-100k | 5,13× | построение/сравнение owned dynamic titles |
| catalog-pipeline | 4,66× | end-to-end records + strings + Map |
| map-string-book | 4,45× | dynamic keys, lookup result, field reads |
| set-string-churn | 4,36× | create/remove/reinsert/retained key storage |
| record-create | 3,13× | escaping managed allocation |
| parse-result | 2,96× | format/parse + transient Result |

Работа native выполняется одной волной через borrowed UTF-8 ranges/iterators
внутри std/runtime, unboxed internal lookup status и escape-aware allocation.
Публичные `string`, `Option`, `Result`, Map/Set semantics не меняются: owned
значение материализуется только на действительно escaping boundary. Generated
C не должен создавать временный managed object для внутреннего hit/miss/tag,
когда typed MIR доказывает немедленный exhaustive match. Любой такой proof
проверяется против alias, mutation, GC, defer/panic и allocation failures.

Порядок native: отдельно измерить split/grapheme/lookup/result/allocation;
реализовать borrowed iteration; затем Map/Set result transport; затем record
allocation path. После каждого общего изменения запускаются paired affected
cells и полная 32-cell native matrix. `catalog-pipeline` остаётся обязательным
интеграционным guard. Цель — не только геометрическое среднее: каждая standard
native workload должна быть ≤ 1,25× более быстрого из Go/Rust либо иметь
отдельно принятую, измеренную семантическую причину; сейчас исключений нет.

## Порядок исполнения

1. Завершить execution-image owner: компактное представление, прямые branch,
   callee и layout bindings, rollback/failure tests.
2. Добавить family runners и superinstructions; повторить 32-cell VM matrix.
3. По оставшемуся разрыву сравнить общий compiler tier и дальнейшие переносимые
   изменения. Проверять стоимость source-run и все Tier 1 targets; собственный
   JIT не является заранее обязательным решением. VM/Bun — промежуточный gate,
   цель пользователя остаётся паритетом с Go/Rust на всех 32 workloads.
4. Параллельно по независимому native профилю выполнить borrowed-text и
   result-transport волну; затем escaping records.
5. Полная локальная матрица 3+11, Tier 1 CI/perf, CP42 → CP41 → CP39 → CP40/N0.

## Книга

Новая идея из книги Владимирова в этом анализе не использована. Выводы выше
получены из полной матрицы, динамических instruction counts, CPU samples и
контрактов Marimay. Ранее реально применённые идеи и страницы остаются в
`docs/research/vladimirov-optimizing-compilers-2026-09-07.md`. Исходный PDF
по переданному пути на момент этого анализа отсутствует; поэтому новые ссылки
по сохранённым фрагментам без повторной сверки с книгой не добавляются.

## Первый execution-image предел

На неизменённом `insertion` финальная парная серия из пяти запусков дала
6 062,67 ms для установленного baseline и 965,82 ms для execution-image
кандидата: 6,28× при совпадающем `insertion 999956059`. На `DSU` та же серия
дала 5 478,70 против 3 846,05 ms: 1,42× при совпадающем `dsu 21343152`.
`heap-sort` ускорился на 2,2%, а BFS и Dijkstra отличаются от контроля лишь на
0,21% и 0,08%. Общий image-dispatch до
избирательного включения ухудшал соседние алгоритмы примерно на 5–15%, поэтому
он теперь применяется только к функции с доказанной специализированной трассой;
остальные функции сохраняют компактный D104 runner.

После этого CPU sample ускоренного `DSU` всё ещё распределён между
`mmay_bytecode_run_hot`, scalar/array handlers, program/frame boundaries и
execution-image runner. Следовательно, superinstructions доказали направление,
но одновременно измерили предел интерпретатора: `insertion` остаётся примерно
11× медленнее Bun из контрольной полной матрицы, `DSU` — примерно 64×. Следующий
уровень из выбранного маршрута — ограниченный hot-loop compilation tier с теми
же side exits; добавление всё более широкого universal switch отклонено
измерением. Новая идея из книги для этой реализации не использовалась.

## Второй execution-image предел: batching полной итерации

D109 проверяет первую часть выбранного hot-loop уровня без генерации машинного
кода: точная nonallocating array-shift trace исполняет несколько полных
итераций, удерживая scalar registers, stable RAW64 storage и mutation
generation в локальных переменных C. Перед любой неполной итерацией состояние
публикуется на последней завершённой границе, а обычный D108 runner выполняет
первую непройденную инструкцию. Frame/shared fuel списывает ровно 17 исходных
операций на завершённую итерацию.

В одиннадцати чередующихся full-size запусках immutable `9fb7523` control без
D109 дал медиану 980,44 ms, D109 candidate — 234,36 ms, то есть 4,18×. От
первоначальных 6 142,94 ms это примерно 26,2×; checksum во всех наблюдениях
равен `insertion 999956059`. До контрольного Bun 87,63 ms остаётся 2,67×, куда
входит source loading/checking/lowering и запуск процесса. Финальная VM-only
матрица 3+11 прошла все 32 checksum и дала 230,19 ms для `insertion`. Сумма
медиан относительно D108 изменилась 28 305,37 → 27 977,31 ms; остальные 31
ячейка остались в диапазоне -5,9%..+3,5%.

Этот результат подтверждает пользу удержания состояния на доказанной trace и
одновременно сужает следующий шаг. DSU, min-heap и BFS требуют отдельных
общих function/call/control/array trace families; коротким прикладным сценариям
нужен измеренный source-pipeline cache. Расширять один universal dispatcher по
прежнему запрещает отрицательное D107 измерение. Новая идея из книги
Владимирова для D109 не использовалась.
