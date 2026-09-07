# CLI0: первый цикл принятого маршрута

Дата: 2026-09-07. Исходный код: `2d9cc83537d88b02a7079d4e366b297b9d7bf75f`.
Область: сверка реализации и остаточных доказательств, чтение текущего CI.
Пользователь принял [маршрут](../../research/cli0-route-analysis-2026-09-07.md);
его исполняемый контракт находится в [ledger](../checkpoints.md#accepted-cli0-execution-route).
CP42 и первый цикл остаются открытыми. Это не новое прохождение всех тестов.

Последующий [shared harness result](cli0-shared-acceptance-2026-09-07.md)
закрывает общую process/build/parity подготовку на двух реальных config
adapters; описанный ниже исходный список сохраняет границы аудита.

## Все 42 назначения

[Машинный снимок](cli0-baseline-audit-2026-09-07.json) содержит каждую строку,
состояние owner, ссылки на его имеющееся evidence и классификацию остатка.
[Матрица CLI0](../cli0-closure-matrix.tsv) сохраняет прежние requirements,
owners, platform scopes и permitted release claims. Изменены формулировки
26 реализованных семейств и двух install assignments; scope не повышен.

| Owner / число строк | Что установлено | Что действительно осталось |
|---|---|---|
| CP30 / 4 | Dynamic strings, interpolation и formatting/parsing имеют verified checkpoint и публичные fixtures. | Exact-candidate покрытие соответствующих требований и полная платформенная приёмка. |
| CP31 / 3 | Args/controlled exit реализованы и проходят через engines. | Та же платформенная сверка; исторические allocation failures требуют объяснения. |
| CP32 / 3 | stdin/stdout/stderr/flush реализованы. | Полная platform/console/pipe приёмка итогового кандидата. |
| CP33 / 3 | Text/Bytes/builders реализованы. | Полная platform/UTF-8/resource приёмка кандидата. |
| CP34 / 2 | Paths/filesystem/capabilities реализованы. | Полная native/VM host-adapter приёмка, включая реальные ошибки ОС. |
| CP35 / 2 | Явный environment/process context реализован. | Платформенное поведение, absent/denied и redaction в итоговом портфеле. |
| CP36 / 4 | Map/Set/iterators реализованы; CP42 изменил representation и traversal. | Приёмка актуального layout/cursors, ресурсов, общего API и всех нужных hosts. |
| CP37 / 1 | Первая std/cli реализация есть. | Exact-candidate CLI/host/workload evidence. |
| CP38 / 4 | JSON codecs/transformer и CP42 automatic codecs существуют. | Неподтверждённая причина run 205 B0004, итоговые resource/platform/app доказательства. |
| CP39 / 2 | Native run/init/doctor, packaging, relocation и distribution lifecycle имеют production/test owners. | Final-artifact clean images, PATH и полная заявленная platform matrix. |
| CP40 / 3 | Это интеграционные назначения resources, native Tier 1 и workloads. | Аудит кандидата и прямые результаты каждого требования; наличие тестов не закрывает его. |
| N0 / 3 | Внешние identity/legal и PATH обязательства явно записаны. | Прямые внешние evidence и соответствующее решение; установка частично использует общие CP39 tests. |
| A0 / 1 | Механический architecture gate действует. | Полная заявленная Tier 1 ownership/architecture evidence. |
| I0 / 2 | Bootstrap и memory tests/CI существуют. | Полный compiler/host/memory-tool пакет; TSan run 206 не выполнялся. |
| I1 / 2 | Syntax registry и sustained fuzz работают. | Полное requirement/fixture и all-host fuzz evidence с требуемыми метаданными. |
| I2-BASELINE, I2-SERVICE, I3-BUNDLES / 3 | В матрице явно `excluded-from-cli0`. | Сохраняются за будущими gates; не становятся недостающими возможностями CLI0. |

Итого: 26 реализованных языковых назначений + 2 install + 3 интеграционных
+ 3 N0 + 1 A0 + 2 I0 + 2 I1 + 3 исключённых = 42. Это классификация строк,
а не оценка процента готовности. CP30–CP38 исторически verified; родительские
gates и изменившийся кандидат не наследуют all-host verification автоматически.

## Уточнение run 206

[Run 206](http://ci.example:3000/Marimay/marimay/actions/runs/206)
исполнял точный код `2d9cc83`. Статусы job прочитаны отдельно от полных логов.
JSON содержит line-numbered excerpts, hashes полученных логов, времена и URLs.

| Job | Наблюдаемый результат |
|---|---|
| Clang Debug, 1739 | Job success; лог содержит 73/73 passed. |
| Clang Release, 1740 | Job success; в полученном логе есть passed CP38, distribution и packaging. Финальная суммарная строка в полученном снимке отсутствует. |
| GCC Debug, 1741 | Job success; в полученном логе есть passed CP38 и distribution. Финальная суммарная строка в полученном снимке отсутствует. |
| GCC Release, 1742 | CTest 74/74 passed; job failure после пяти HTTP 500 при создании artifact container. |
| ASan/UBSan/LSan, 1743 | CTest 73/73 passed, включая CP38; job failure после пяти HTTP 500 при загрузке diagnostics. |
| TSan, 1749 | Skipped из-за failed dependency; это отсутствие проверки. |

Разница 73/74 связана с profile-specific набором: packaging отдельно
зарегистрирован для Release. Эти числа не следует складывать в число
уникальных языковых тестов. Успешный sanitizer run не является утверждением
об отсутствии всех возможных утечек.

Четыре fuzz job successful: bytecode 23 241 929 executions, formatter
1 104 935, frontend 1 084 086, JSON 21 591. Каждый libFuzzer напечатал
601 second(s). Это длительность по инструменту; отдельного учёта CPU времени
в этих выдержках нет. Остальные платформы и полная campaign metadata остаются
частью gate. Корпуса и параметры берутся из точной ревизии workflow.

API `/runs/205/artifacts` и `/runs/206/artifacts` при чтении вернул пустые
списки. Поэтому фактическое сохранение требуемых distribution/diagnostic
artifacts не подтверждено, несмотря на выполнение тестов и части upload jobs.
Pipeline в целом остаётся failed. Нельзя исправить отчёт изменением checks
на `continue-on-error` или объявить этот кандидат полностью зелёным.

## Разделённые блокеры и владельцы

| Блокер | Следующий проверяемый результат |
|---|---|
| Generated C: run 205 B0004 | Получить/воспроизвести exact failing program, source/runtime/toolchain и missing declaration. CP38 passed в run 206 не объясняет причину; parser recovery patch не выдаётся за исправление emitter. Owner: native emitter/build и focused CP38 harness. |
| CI artifact HTTP 500 | Установить причину artifact-service failure и подтвердить сохранение/чтение нужных artifacts. Это отдельная инфраструктурная проблема; административные изменения сервера не выполнялись. |
| TSan и старые allocation failures | Получить реальное прохождение TSan и адресовать сохранённые ошибки runs 195–197 с точными входами/индексами/окружением. |
| Общая прикладная приёмка | Выделить версионируемый process/build/fixture слой из config scripts и проверить на реальном config; остальные приложения сохраняют свои сценарии. |
| Startup/API/portfolio | Текущий config startup budget, общие API и восемь новых приложений остаются за CP42 по принятому маршруту. |
| Host/N0 evidence | Зафиксировать доступность clean install и PMU окружений, затем собрать результаты и внешние подтверждения. |

## Платформы и границы результата

| Target | Доступное свидетельство в текущем пакете | Чего оно не доказывает |
|---|---|---|
| Linux x86_64 | Gitea run 206 и предыдущие runs. | Пригодность этого host для выделенных стабильных PMU измерений, полный зелёный CI и чистые финальные release images. |
| macOS arm64 | Локальный M2, Release 74/74 и focused sanitizer/app evidence предыдущих CP42 отчётов. | Выделенный performance host и clean-host полный release lifecycle. |
| Linux arm64 | Свежий direct run в этом пакете отсутствует. | Никакая матрица не считается выполненной по переносимости исходников. |
| macOS x86_64 | Свежий direct run в этом пакете отсутствует. | То же. |
| Windows x86_64 | Свежий direct run в этом пакете отсутствует. | То же, включая MSVC/console/path/installer особенности. |

Инвентаризация отсутствующего evidence не означает техническую невозможность
запуска. D057 и N0 сохранены. Отчёт фиксирует прогресс первого цикла:
назначения сверены, текущие CI отказы классифицированы, но надёжная база,
общая прикладная механика и полная внешняя приёмка ещё не закрыты.
