# CLI0: общая механика приёмки внешних приложений

Дата: 2026-09-07. Первый цикл [принятого маршрута](../checkpoints.md#accepted-cli0-execution-route).
База языка: `1bb592d`; compiler/runtime/source semantics не изменены.

Из config scripts выделен [драйвер версии 1](../../../tools/cli-acceptance/mmay_acceptance.py):
выбор обычного `MMAY`/PATH, запуск с явным cwd/timeout, VM и две native-сборки
с одинаковыми grants, точное сравнение status/stdout/stderr, подготовка fixture
и callback проверки после каждого engine. Metadata содержит version/hash
драйвера и hashes фактических executable. Он не читает ambient environment
для отчёта и не реализует прикладную логику.

Оба config adapters используют одну идентичную локальную копию module.
Репозиторий приложения работает без импорта из checkout компилятора;
[инструкция](../../../tools/cli-acceptance/README.md) описывает явное обновление
version/hash. Схемы JSON, генерация входов, budgets, сообщения, atomic/denied/
symlink/broken-pipe проверки и ожидаемые результаты остаются в приложении.
Новые отчёты получают новые имена; прежние baseline-файлы не перезаписываются.

## Проверки

| Проверка | Результат |
|---|---|
| `python3 tools/cli-acceptance/test_driver.py` | 7/7 прошли. |
| CTest `mmay_cli_acceptance_driver`, Release | 1/1 прошёл, 0,42 с; запускает те же семь проверок. |
| Внешний `python3 tools/acceptance.py` | Все 32 exact-parity cases × VM/native Debug/Release, дополнительные atomic/denied/broken-pipe assertions прошли. |
| Внешний `python3 tools/acceptance_files.py` | Все 23 filesystem cases × три engines; реальные POSIX permission failures, статические symlinks, FIFO, сохранение исходного файла и atomic success прошли. |

Собственные тесты драйвера доказывают обнаружение расхождений exit/stdout/stderr,
общего неверного exit и общего неверного output через app callback, сохранение
raw bytes/argv/cwd, reset и verify для каждого engine, отказ при пустом наборе
engines, отключённых assertions и timeout. Проверки не ограничиваются
положительным сравнением одинаковых результатов.

Новый CTest включён в существующую Python-enabled ветку CMake и будет выполнен
обычной compiler CI matrix. Полный C17 suite локально повторно не запускался:
изменены только Python acceptance infrastructure, регистрация теста и документы.
Новый тест, реальные оба adapters и обязательные repository gates проверяются
для этой правки; новые Linux/Windows результаты появятся только после CI/host run.

[Пакет воспроизводимости](cli0-shared-acceptance-2026-09-07.json) содержит точные
source/fixture hashes и тексты синтетического config-приложения, миграционные
diffs adapters, dataset generator и оба сырых результата. У config пока нет
app Git commit/remote; SHA приложения не выдумывается. Архив не содержит личной
базы notes. Авторитетный исходник драйвера находится в tools, а его hash в
пакете позволяет проверить совпадение с копией приложения.

Прикладные проверки запускались одновременно: их elapsed durations — время
correctness run, а не performance baseline. Ни startup budget config, ни
CP42 acceptance, ни исторический CI native defect этим изменением не закрыты.
Shared mechanics теперь готовы для следующих приложений; timing budgets и
их доменные сценарии сохраняются отдельными обязанностями.
