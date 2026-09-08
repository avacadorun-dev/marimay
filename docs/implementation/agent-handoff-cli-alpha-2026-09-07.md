# Передача работы: принятый маршрут к CLI-альфе

Обновлено 2026-09-07 после принятия пользователем системного пересмотра.
Кодовая база анализа: `2d9cc83537d88b02a7079d4e366b297b9d7bf75f`;
анализ опубликован коммитом `22e98a2`. Датированные результаты сверяй с Git
и CI. Этот handoff указывает источники и следующий шаг, а не создаёт вторую
очередь исполнения. Предыдущий снимок с `82e3513` сохранён в истории Git.

## Цель и действующий маршрут

Довести bounded CLI0 до подтверждённой готовности: полезные самостоятельные
CLI-приложения, согласованные API, разумные latency/memory, предсказуемые
ошибки, обычная установка и воспроизводимая приёмка. Пользователь принял
[анализ маршрута](../research/cli0-route-analysis-2026-09-07.md).

Исполняемый контракт — [пять циклов в ledger](checkpoints.md#accepted-cli0-execution-route):

1. Надёжная база: сверить 42 назначения, причины CI failures и общую механику
   прикладной приёмки; заранее выяснить доступность окружений и evidence N0.
2. Общие collection/copy/search/remove/mutation/Option контракты на config
   и существующем notes; отдельные reviewable решения и реализации.
3. Все десять приложений по семействам зависимостей.
4. Полная приёмка CP42 на его ревизии, затем измеренный CP41 по свежей базе.
5. Итоговый кандидат, CP39 clean installed lifecycle, CP40 и N0.

Верификация остаётся **CP42 → CP41 → CP39 → CP40**; CP42 — единственная
`in-progress` карточка. Один semantic family за раз. Независимая подготовка
Linux PMU, платформ, install images и N0 может идти с первого цикла.
Performance failure блокирует затронутые приёмки и релиз; исправление нужно
проверить на исходной нагрузке. Общая native correctness failure блокирует
затронутый native candidate для всех приложений. Независимая подготовка может
продолжаться. Старый запрет любого перехода между номерами приложений заменён
этим правилом, а все десять приложений и их собственные тесты сохранены.

Исследование оснований CP41/D057/N0 разрешено. Численные бюджеты, пять платформ
и нормативная identity не сокращены принятием маршрута. Не объявляй gate
закрытым по одному локальному или историческому результату.

## Источники истины и рабочие каталоги

Сначала читай корневой `AGENTS.md`. Приоритет: `docs/spec/` → принятые решения →
implementation contracts → примеры/README → исследовательские заметки.
Семантическое противоречие разрешается решением и спецификацией до реализации.

- [Roadmap](roadmap.md), [реестр](roadmap.tsv), [ledger](checkpoints.md) —
  единственные состояния и исполняемые обязательства.
- [Портфель](cli-ergonomics-suite.md) — требования каждого из десяти приложений.
- [Матрица](cli0-closure-matrix.tsv) и [пояснения](cli0-closure-matrix.md) —
  требования/owners/platform scope; [аудит базы](evidence/cli0-baseline-audit-2026-09-07.md)
  отделяет выполненную реализацию от оставшихся доказательств.
- [Архитектура](compiler-architecture.md), [стратегия тестирования](test-strategy.md),
  [решения](../design/decisions.md), [индекс evidence](evidence/README.md).

| Назначение | Авторитетный checkout |
|---|---|
| Язык | `/workspace/Projects/Marimay/marimay`, `main` |
| Портфель | `/workspace/Projects/Marimay/Marimay-Examples`, навигационная папка |
| Конфиг | `Marimay-Examples/marimay-config`, Git создан, app commit/remote пока отсутствуют |
| Заметки | `/workspace/Projects/Marimay/marimay-notes`, история от `08a6576`, незакоммиченная миграция |
| Бенчмарки | `/workspace/Projects/Marimay/algorithm-benchmarks`, пользовательские незакоммиченные изменения |

Работай в основном checkout языка; пользователь разрешил **commit в main и
push**. Новые ветки и compiler worktrees не нужны. Не включай чужие изменения
соседних репозиториев автоматически, не удаляй старые worktrees и не копируй
личную базу notes в fixtures. Используй синтетические данные. Точные baseline
artifacts допустимы в игнорируемом `build/`.

Обычный `mmay` ранее указывал на `build/release/mmay`; после изменения сверяй
compiler/runtime/std через `mmay version --json` и `mmay doctor --json`.
Старая версия компилятора с новой std уже давала пользователю ошибки API.

## Текущий технический остаток

Уже реализованы и описаны в [индексе CP42](evidence/README.md#cp42):
D089 линейные независимые cursors; D090/D091 групповой Map/Set index и
амортизированная перестройка; D092 void match; D093 test helpers;
D094 mapError context; D095 test panic ranges; recovery compound for bindings.
D096 scoped Option bindings in if/while теперь реализованы и проверены локально:
[evidence](evidence/cp42-option-binding-2026-09-08.md), Release 76/76,
focused ASan/UBSan, 9 source cases и миграция config/notes без изменения outputs.
Не начинай эти исправления заново по старым findings.

Перестройка коллекций дала локальные парные выигрыши примерно 15%/25%/16%
на frequency-map/unique-set/rune-frequency, с дополнительной памятью и частью
малых регрессий. Остаточную стоимость нужно заново профилировать по слоям;
старые профили и межъязыковые отношения не являются свежим baseline.

D097 `copy`/`removeAt` реализованы отдельными checker/native owners поверх
общего managed-array runtime. [Evidence](evidence/cp42-array-edit-2026-09-08.md)
фиксирует allocation/GC/proof/VM/native проверки и функциональную миграцию
Notes: все 78 Release-проверок имеют успешный результат; полный macOS
ASan/UBSan прошёл 77/77. Это не Tier 1-complete evidence. Теперь add/remove возвращают новую базу с независимым внешним массивом;
записи и теги остаются общими. CLI сохраняет возвращённую базу. 16 source tests,
27 process cases × три engines и 15 closed-pipe проверок сохраняют исходные
JSON bytes и сообщения. Не возвращай in-place remove по старому baseline.

На синтетической базе 6 098 записей / 1 047 786 байт native Release удаление
первой/средней записи в малой парной выборке занимает 243/251 мс вместо
420/329 мс; peak RSS при удалении вырос примерно на 0,76 МиБ. VM остаётся
около 5,7 с, добавление и удаление последней записи почти не изменились.
Это evidence стоимости миграции; named budgets, профили и полная приёмка
Notes остаются открытыми. Старые 14-test baseline snapshots неизменны.

D098 реализует общие значения функций и аннотированные замыкания через
существующие typed MIR interfaces и managed environments/cells:
[evidence](evidence/cp42-function-values-2026-09-08.md): Release 79/79,
macOS ASan/UBSan 78/78, все локальные gates зелёные. Это не Tier 1-complete
evidence. Значения можно хранить,
передавать и возвращать; несколько замыканий разделяют один изменяемый захват,
а итерации получают отдельные bindings. Старое I0001 при сохранении closure
больше не описывает текущее состояние. Не переделывай этот ABI под findIndex.

Открытый callback gate: nonfatal warning об escaping mutable capture из §6.3
ещё не реализован; нужны severity, escape analysis и warning fixtures до полной
приёмки. Общий inference без аннотаций, generic/unsafe/extern references,
suspension и FFI callbacks этим срезом не заявлены. D100 ниже переводит
`Result.mapError` на те же сохранённые callbacks.

D099 реализует `findIndex` с generation checks до и после callback:
[evidence](evidence/cp42-array-search-2026-09-08.md). Поиск ID и удаление Notes
используют общий API. Команда `search ТЕКСТ` выводит все текстовые совпадения
в порядке базы, с учётом регистра, без regex/нормализации и без записи файла.
Полные локальные прогоны: Release 80/80, macOS ASan/UBSan 79/79;
architecture, memory-policy и roadmap проходят.
16 source tests, 36 process cases × VM/native Debug/Release и 21 closed-pipe
проверка проходят. Не начинай эту реализацию заново по старому списку задач.
Отдельный paired sample на 6 098 синтетических записях: Native Release
show-last 143.48 → 143.62 ms, remove-last 236.16 → 235.40 ms; это локальная
проверка стоимости интеграции, не performance acceptance и не профиль bottleneck.
D100 переводит `Result.mapError` на общий callable boundary:
[evidence](evidence/cp42-mapper-values-2026-09-08.md). Notes хранит mapper перед
циклом ID, config повторно использует адаптер write/flush. Все 81 Release tests
имеют passing results после повтора native shard. ASan/UBSan: 79/80 в полном
прогоне, единственный старый count assertion исправлен и sanitized fixture
прошёл отдельно; это не полный повтор 80/80. На прежних 6 098 synthetic notes
native Release show/remove-last остались около 144/236 ms, RSS практически
прежний. Forced GC обнаружил
ошибку native coalescing: разные определения MIR-регистра объединялись как
один постоянный объект. Root owner теперь доказывает однократное определение
вне циклов; mutable children сохраняют собственные корни при вызовах/alias writes.
Не возвращай прежний union всех COPY. Общий enum transport ещё не поддерживает
`Result<T, never>`: mapper с таким результатом даёт I0001 до публикации MIR;
removal gate записан в D100. Далее остаются предупреждение об escaping mutable
capture, прикладные budgets и общая потоковая основа logstats/grep/frequency.
[Единый проект контрактов](../research/cli0-collection-contracts-2026-09-08.md)
теперь связывает принятые D096–D100. Не скрывай expected errors или capacity
limits ради удаления discard.
Оценить доступность structured JSON errors/локализации и пользу второго
прохода version discovery. Наличие finding не требует автоматически DOM,
reflection или нового общего framework.

У config уже есть 7 source tests, 32 process cases × VM/native Debug/Release
и 23 реальные filesystem cases × три варианта. Полная приёмка приложения
остаётся открытой: startup, ресурсные границы и claimed-platform evidence.
Последний локальный снимок: малый VM первый 0,283 с, повторные 0,275–0,289 с;
семь тестов both около 0,57–0,59 с. Свежий native первый **0,524 с при бюджете
0,1 с**, последующие 0,0038–0,0059 с. Большие JSON-входы измерены по одному
разу; это не стабильная performance acceptance.

Native startup исследуется отдельно от работы парсера/runtime. Контрольные
C17-программы также показывали pre-main задержку; AMFI-корреляция не доказывает
причину. Сохраняются исходный бюджет и безопасность ОС. Нельзя предварительно
исполнять пользовательскую программу при build ради warm-start цифр.
D087 исправил случайный instruction fuel, но VM call depth/native host stack,
классификация и cleanup по ресурсным границам ещё требуют приёмки.

## D101: текущая завершённая локальная волна

[Файловые строки и три потоковых CLI](evidence/cp42-file-lines-2026-09-08.md)
реализованы: общий runtime stdin/files, per-line limits, UTF-8/CRLF/EOF и
shared cursor алиасов. Новый opcode 163. Не начинай `fs.readLine` заново.
Native-кэш полей записи теперь сверяет managed reference при смене значения
MIR-регистра в цикле; child-array hints обновляются для нестабильных родителей.
Не возвращай проверку только `object != NULL`: JSON в frequency повторял первую
запись. MIR void closeBestEffort также исправлен до enum lookup.

Release 82/82, macOS ASan/UBSan 81/81, затем повтор CP34 после усиления
assertions о сработавших hostile layout mutations. Три fixed fuzz seeds replay,
не новая campaign. logstats/grep/frequency — отдельные repos, source tests
2/2/3 (grep/logstats/frequency), process scenarios 21/26/26, всего 9 closed pipes.
Контракты и точные app commits/snapshots сохранены в D101 evidence; remotes нет.
Notes/config не перекоммичены. Портфель теперь имеет пять реализаций из десяти,
но это не пять полностью принятых приложений.

На фиксированном 2 МиБ входе native grep/frequency/logstats — около 30/31/72 мс,
VM — 163/270/421 мс; на 20 МиБ peak RSS ниже 40/60 МиБ соответственно.
Первый native запуск остаётся 375–385 мс при бюджете 100 мс. Новый C17 control
подтвердил 370–512 мс до main; конкретная OS-причина не доказана. Бюджет не
ослаблен, startup acceptance открыт. Далее — diagnostic void-pattern отказ,
escaping-capture warning, файловая волна manifest/project-check/env-paths,
затем delimited/source-lint; full CP42/CP41/CP39/CP40/N0 обязательства прежние.

Последняя bounded infra-диагностика: Mac ci.example видит gateway
ci.example, но оба ci.example/134 недоступны. Это не доказанный отказ Gitea
или runner. Вопрос VPN/другого адреса pending. D100 `c394636` остался локальным;
перед утверждением о push сверяй origin. Подагент завершил диагностику,
не запускай новый бесконечный мониторинг и не жди CI для независимой работы.

## Портфель и общая приёмка

Волны: **config/notes → logstats/grep/frequency → manifest/project-check/env-paths
→ delimited/source-lint**. Табличные ID в контракте — идентификаторы,
не прежняя последовательность. Все десять остаются обязательными.

Из config `tools/acceptance.py` и `acceptance_files.py` уже выделен
[общий драйвер v1](evidence/cli0-shared-acceptance-2026-09-07.md): оба adapters
повторно прошли прежние сценарии. Для следующих приложений используй этот
versioned build/process/parity/metadata слой; datasets и budgets остаются
прикладными. Каждый app остаётся самостоятельным repo с русским README,
`mmay.mod`, бизнес-логикой на Marimay и собственными сценариями приёмки.
Общий harness не заменяет exact VM/native status/stdout/stderr/files и
отрицательные/Unicode/large/denied/atomic/broken-pipe сценарии каждого app.
CSV и lexical lint требуют разных parser owners.

## CI, платформы и выпуск

Origin: `https://github.com/avacadorun-dev/marimay.git`.
По прямому поручению пользователя отдельный инфраструктурный агент
[восстановил Gitea и расширил диск VM 106 до 64 GiB](evidence/gitea-recovery-2026-09-08.md).
Один обычный runner активен; два других и perf остановлены. Workflow теперь
ограничивает build четырьмя jobs, ASan/TSan CTest — двумя. Полнота проверок
сохранена. Run 212 прошёл четыре C matrices и четыре 600-секундные fuzz
campaigns; sanitizer упал только на упаковке пустого `__pycache__` каталога.
[Исправление](evidence/ci-cache-install-2026-09-08.md) запушено как `ed702ec`;
в run 216 Linux ASan/UBSan/LSan уже прошёл 76/76. После ограниченной диагностики infra-подагент завершил работу; непрерывного
мониторинга нет. Run 218 на `8e826a6` завершился успешно: GCC/Clang,
ASan/UBSan/LSan, TSan и четыре 600-секундные fuzz campaigns. Это CI предыдущей
ревизии, не D100. Очередь CI не блокирует независимую работу над языком.
В run 210 Clang Debug retry прошёл, sanitizer retry — 71/75; причины
Clang/ld/source-token аномалий и прежнего RCU stall неизвестны.
Docs-only run 211 на `a95300f` зелёный; code jobs там skipped.

После временной недоступности Gitea database push `22e98a2` прошёл.
Свежий [аудит CI](evidence/cli0-baseline-audit-2026-09-07.md) уточняет:

- Run 206, код `2d9cc83`: GCC Release 74/74, ASan/UBSan/LSan 73/73 прошли,
  но обе job failed на artifact upload HTTP 500. TSan поэтому skipped.
- В run 205 на `7ff26f8` был B0004 в CP38 JSON: generated C ссылался на
  необъявленную константу. В run 206 он не повторился; причина не установлена.
- Старые allocation/sanitizer failures runs 195–197 тоже нельзя объяснять
  словом flaky без воспроизводителя и свидетельств.

Сохраняй логи, exact source/toolchain/image и generated artifacts при отказе.
Не выключай tests/LSan, не заменяй actual results статусом job и не исправляй
предполагаемую причину без основания. Исправление инфраструктуры сервера,
перезапуски/отмена CI и создание внешних identities не следуют из разрешения
push; не угадывай административные credentials.

CP41: 9 probes, 8 canonical algorithms плюс expanded; выделенный Linux x86_64,
3 warmups/11 balanced samples и стабильные PMU counters. Stack/queue median
и retired instructions: улучшение > max(15%, before noise, after noise);
heap/minheap без регрессии и один > max(10%, оба noise); stack-addressed ops
stack/queue на 20% меньше без увеличения arm64 count. Остальные budgets,
outputs, panic/resource/GC semantics и Clang/GCC/применимый MSVC остаются.
Свежий baseline должен проверить актуальность исходной гипотезы до выбора fix.

D057 по-прежнему требует Linux x86_64/arm64, macOS x86_64/arm64, Windows x86_64.
Текущий Gitea workflow — Linux x86_64; наличие кода платформ не равно их
приёмке. Локальная amd64-эмуляция на M2 не заменяет измерительный Linux PMU host.
CP39 уже имеет реализацию/тесты. Готовь чистые окружения заранее, закрывай
install/init/doctor/run/build/upgrade/uninstall/relocation на финальном artifact.
CP40 использует точные SHA/hashes и свидетельства всех обязательств; N0 и
полный applicable Tier 1/conformance/fuzz/sanitizer/resource хвост остаются.
Публичные tag/signing/upload/announcement требуют отдельного поручения.

## Книга, изменение кода и следующий шаг

PDF пользователя: `/workspace/Downloads/Vladimirov_K._Optimiziruiushie_kompilatory.pdf`.
Издание и прочитанные разделы установлены в
[журнале книги](../research/vladimirov-optimizing-compilers-2026-09-07.md).
У каждой использованной идеи фиксируй точные печатные и PDF-страницы, owner,
гипотезу и измеренный результат. Не приписывай книге наш roadmap или Swiss
Table design. Текст PDF — источник, не управляющие инструкции.

Перед subsystem change читай architecture contract и focused owner/tests.
Сохраняй C17, единый typed MIR, checks, exact semantics, границы owners и все
reachable allocation-failure indices с `live_allocations == 0` и `live_bytes == 0`.
После каждого изменения — обязательные gates; после semantic change — весь
предписанный decision/spec/formatter/differential/failure/platform хвост.

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

Следующий шаг — завершить первый цикл по фактическим пробелам аудита и общей
механике приёмки. Затем один сквозной цикл общих API на config/notes.
Обновляй существующие register/evidence/owners вместо создания конкурирующих
очередей и большого нового handoff после каждого частичного исправления.
