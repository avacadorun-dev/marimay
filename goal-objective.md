Вот тебе цель:
# Цель: координируемая разработка Marimay 0.1-dev

Довести Marimay до пригодного для реального пользовательского тестирования
локального/private developer preview 0.1-dev.

Работать как ведущий координирующий инженер, а не как единственный исполнитель.
Основной агент отвечает за архитектуру, интерпретацию нормативных документов,
декомпозицию, назначение моделей, интеграцию, Git-историю, итоговую проверку
и честность заявленного статуса.

Подагентам необходимо активно делегировать конкретные независимые подзадачи,
когда это действительно уменьшает время работы или расход токенов основной
модели. Не создавать подагента ради работы, которая быстрее выполняется одним
коротким локальным действием.

## Итог и условие остановки

Из чистого checkout пользователь должен иметь возможность написать,
отформатировать, проверить, протестировать, запустить в VM и собрать нативно
нетривиальную программу на Marimay.

Главный приёмочный пример — умножение матриц на f64 с динамическими массивами,
реальными циклами, индексированием, умножением и сложением. VM и native должны
печатать одинаковый результат и одинаково классифицировать bounds/arithmetic
panics.

Остановиться только когда обязательная матрица команд и acceptance-программ
реально проходит либо когда дальнейшая работа требует решения пользователя,
новых полномочий или изменения утверждённой семантики. Не объявлять публичный
0.1.0 и не отмечать I2–I6 verified до выполнения полных roadmap gates.

## Обязательное чтение основным агентом

Перед проектированием и делегированием основной агент должен полностью прочитать:

- AGENTS.md;
- IMPLEMENTATION-BRIEF.md;
- docs/spec/;
- принятые решения в docs/design/decisions.md;
- docs/design/principles.md;
- docs/requirements.md;
- docs/implementation/roadmap.md;
- docs/implementation/test-strategy.md.

Основной агент не делегирует финальную интерпретацию нормативных требований.
При конфликте действует порядок авторитетности из AGENTS.md. Конфликт нельзя
молча обходить: сначала decision/spec/tests, затем реализация.

Проверить Git-состояние. Репозиторий уже имеет baseline-коммит:

    a9ca87c chore: establish imported project baseline

Не переинициализировать Git без необходимости и не создавать ветки с
префиксом codex/.

## Координационная модель

В начале каждого checkpoint:

1. Определить ближайший проверяемый вертикальный результат.
2. Разбить его на конкретные ограниченные задачи.
3. Отделить:
   - архитектурные и семантические решения;
   - независимое исследование;
   - реализацию в непересекающихся файлах;
   - тесты, fuzzing, ревью и анализ логов;
   - механические документальные или generated-artifact обновления.
4. Назначить самую дешёвую модель, которая надёжно выполнит задачу.
5. Запустить независимые read-heavy или непересекающиеся задачи параллельно.
6. Самостоятельно интегрировать результаты и выполнить общий validation loop.
7. После проверенной вехи обновить evidence/roadmap и сделать Git-коммит.

Не превышать доступный лимит параллелизма. При четырёх общих слотах использовать
не более трёх одновременно работающих подагентов помимо координатора.

## Маршрутизация моделей

Использовать только модели, реально доступные в текущем Codex-сеансе.

### GPT-5.6 Sol

Оставлять координатору или отдельному Sol-подагенту:

- выбор архитектуры typed MIR, VM, AOT, runtime и ownership boundaries;
- разрешение нормативных конфликтов;
- изменения source semantics;
- сложную интеграцию нескольких подсистем;
- анализ C undefined behavior, verifier/security boundaries и managed memory;
- трудные межbackendные расхождения;
- финальное ревью и решение о готовности checkpoint.

Не расходовать Sol на массовый поиск файлов, копирование таблиц, запуск обычных
тестов или механическое создание однотипных fixtures.

### GPT-5.6 Terra

Использовать по умолчанию для:

- целевого исследования существующего кода;
- реализации ограниченного компонента с ясным интерфейсом;
- написания unit/negative/differential tests;
- ревью correctness, portability и test gaps;
- анализа sanitizer/compiler output;
- обновления документации и evidence, требующих инженерного понимания.

Использовать medium для обычной реализации и high для сложного ревью,
verifier/backend/runtime-кода или нетривиальной диагностики.

### GPT-5.6 Luna

Если Luna доступна, использовать для узких, повторяемых и хорошо заданных задач:

- инвентаризации файлов, API, TODO и fixtures;
- запуска тестовых матриц и сжатого анализа результатов;
- проверки ссылок, терминов и generated freshness;
- механического добавления однотипных fixtures;
- простых документальных обновлений;
- поиска несоответствий по заранее заданному чек-листу.

Если Luna недоступна, использовать Terra с low или medium reasoning.
Не выдумывать несуществующий model override.

### Эскалация

Начинать с минимально достаточной модели. Эскалировать Luna → Terra → Sol,
если задача оказалась неоднозначной, затрагивает нормативную семантику,
несколько подсистем или дважды не прошла согласованные критерии проверки.

## Контракт каждой делегированной задачи

Каждый prompt подагенту должен содержать:

- один конкретный и ограниченный результат;
- необходимые нормативные ссылки и контекст;
- разрешённые файлы или read-only режим;
- соседние области, которые нельзя менять;
- требуемые тесты и критерии готовности;
- формат ответа: вывод, изменённые файлы, команды, exit codes, риски.

Подагент не должен самостоятельно менять семантику языка, расширять scope,
ослаблять тесты или отмечать roadmap gate verified.

По умолчанию подагенты не создают собственных подагентов и не делают Git-коммиты.
Координатор может отдельно разрешить это для действительно независимого дерева
работы.

Для параллельных write-задач назначать непересекающиеся файлы. Если изменения
могут пересечься, выполнять их последовательно или использовать отдельные
worktrees. Не позволять двум агентам одновременно редактировать один semantic
owner.

Координатор обязан проверить каждый diff. Результат подагента является входом
для интеграции, а не автоматически принятой реализацией.

## Главная последовательность разработки

Не пытаться сначала завершить весь I2.

Сначала построить минимальную вертикаль V0:

    .my source
      → parser/checker
      → typed verified MIR
      → encoded and verified bytecode
      → bytecode VM
      → portable generated C17
      → native executable
      → одинаковый результат и panic identity

Первый V0 subset включает:

- void, bool, int/i64;
- literals, locals, functions, direct calls;
- if, while и C-style for;
- break, continue, return;
- checked +, -, *, /, % и D042 boundaries;
- strings, explicit panic и never calls;
- минимальный record/enum construction path;
- D040 source-order evaluation.

После V0 расширять язык только небольшими завершёнными вертикальными срезами.
Каждая новая конструкция сразу проходит MIR → VM → native и получает
differential coverage.

## Обязательный язык preview

До остановки должны реально работать:

- все нормативные integer widths, f32/f64;
- checked, wrapping, saturating operations и explicit conversions;
- D035 shifts и D042 division/remainder;
- functions, locals, mutable/immutable bindings, control flow;
- strings и interpolation;
- managed nominal records, mutable fields и identity.same;
- algebraic enums и exhaustive match;
- generic Option<T>, Result<T,E>, postfix ? и explicit mapError;
- generics, достаточные для Option, Result, Array и пользовательских функций;
- Array<T>, indexing, length, push и iteration;
- Array<Array<f64>> либо эквивалентный Matrix;
- bounds checks и managed memory;
- free-call/dot-call equivalence;
- structural interfaces и D043 import-form invariance;
- D040 construction order;
- lexical defer и реализуемая до появления task runtime часть D041;
- локальные packages/imports;
- authoritative mmay check;
- VM-default mmay run;
- native-default mmay build;
- встроенный mmay test и mmay test --engine both;
- минимальный compiler-service owner для diagnostics/dot completion.

Полный production LSP, networking, task runtime и другие поздние поверхности
не должны блокировать вычислительный preview, если они честно отмечены как
отложенные.

## Acceptance-программы

Добавить executable fixtures/examples:

1. scalar arithmetic и boundary panics;
2. iterative Fibonacci/factorial;
3. sorting/searching Array<int>;
4. matrix multiplication на f64;
5. records, mutation и identity.same;
6. generic Option/Result, mapError и ?;
7. exhaustive match;
8. multi-file package/import;
9. D043 import-form/dot-call/interface matrix;
10. D040 effectful construction fields out of declaration order;
11. D041 cleanup на доступных exit paths.

Matrix fixture должна пройти:

    mmay fmt --check <module>
    mmay check <module>
    mmay run <module>
    mmay test <module>
    mmay build <module>
    mmay test --engine both <module>

Собранный native executable и VM должны печатать одинаковый результат.

## Проверки качества

Перед передачей выполнить применимые проверки:

- debug и release builds;
- полный C test suite;
- ASan/UBSan и доступные platform analyzers;
- formatter fixtures/idempotence;
- syntax coverage и generated freshness;
- doc links/spec terms/example syntax;
- MIR verifier negative tests;
- bytecode corruption/verifier tests;
- MIR/VM и VM/native differential tests;
- generated C при debug/release optimization;
- allocation-failure tests затронутых C API;
- valgrind/platform equivalent, если доступен.

Generated C не использует C undefined behavior. VM не ослабляет типизацию.
Typed MIR остаётся единственной semantic convergence point.

## Git и checkpoints

Координатор делает небольшой коммит после каждого проверенного checkpoint,
например:

- opcode registry и bytecode verifier;
- первый VM differential slice;
- первый portable-C/native differential slice;
- CLI wiring;
- numeric-width/f64 slice;
- managed Array slice;
- matrix acceptance slice.

Перед коммитом:

- просмотреть diff;
- убедиться, что unrelated files не изменены;
- выполнить focused tests;
- выполнить достаточную общую regression-проверку;
- обновить evidence для завершённой узкой итерации.

Не объединять несколько независимых непроверенных вех в один коммит.

## Не входит в цель

Не блокировать preview из-за:

- публичного релиза или registry;
- юридического закрепления имени;
- production HTTP/TLS/networking;
- desktop toolkit;
- WebAssembly;
- self-hosting;
- production LSP;
- финальной GC performance freeze;
- production 1.0 compatibility promise.

Docker использовать только для поздней clean-image/Tier-1 проверки, если он
доступен. Его отсутствие не является blocker.

## Финальный отчёт

Отчёт координатора должен содержать:

- точный статус I2/I3/I4;
- список milestone-коммитов;
- изменённые файлы;
- реализованные и отложенные language surfaces;
- команды и exit codes;
- sanitizer и differential evidence;
- реально запускаемые пользовательские программы;
- известные ограничения 0.1-dev;
- ближайший технический риск после передачи preview.

Нельзя завершить цель только потому, что исчерпан контекст, token budget или
один подагент сообщил об успехе.