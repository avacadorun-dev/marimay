# Общий контракт коллекций для config и notes

Статус на 2026-09-08: D096 и D097 приняты; scoped Option bindings и
copy/removeAt реализованы. Predicate search остаётся проектом. Это не новая
очередь работ и не нормативная спецификация.
Перед реализацией каждой части её окончательный контракт должен войти в
`docs/design/decisions.md` и `docs/spec/`. Основание — обязательства 2–5
[CP42](../implementation/checkpoints.md#cp42-notes-driven-cli-ergonomics).

## Что показали реальные программы

В исходном baseline notes `add` создаёт новый внешний массив, сохраняя identity записей
и массивов тегов. `remove` сдвигает элементы исходного массива; все его aliases
видят удаление. Эти различия теперь закреплены отдельными source tests,
чтобы миграция меняла контракт явно. Поиск ID использует цикл и sentinel `-1`.
После миграции D097 add/remove возвращают новую базу; сохранение старых
aliases и разделение объектов проверяются явно. Цикл поиска пока остаётся.
Config уже использует `Option` для выбора назначения вывода и `Result` для
ошибок чтения, проверки и атомарной записи. Обеим программам нужен понятный
способ отделить отсутствие значения от сбоя операции.

Не всякий `const _ =` доказывает недостаток API. D092 уже позволяет void match
без фиктивных `true`/`false`, а D074 разрешает выражение после явного `?`:

```marimay
const inserted = ids.add(note.id).mapError(collectionGrowthError)?;
if !inserted {
  return Err("В базе повторяется ID заметки");
}
```

Это выражение уже работает: оно сохраняет ошибку роста и использует ответ
вставки для проверки дубликата. Отдельный `contains` не нужен. Это устранение
повторной операции, не измеренный выигрыш скорости. В notes такая миграция
выполнена; исходные ошибки и процессное поведение проверены повторно.

## Предлагаемая единая поверхность

Для массива предлагаются три операции с free/dot selection по точному
receiver, как у D043/D053. `copy`/`removeAt` приняты в D097, callable values —
в D098, `findIndex` с проверками generation вокруг callback — в D099.
Все три операции теперь реализованы; результаты проверки находятся в
[пакете D099](../implementation/evidence/cp42-array-search-2026-09-08.md):

```marimay
public fn copy<T>(values: []T): []T;
public fn findIndex<T>(values: []T, predicate: (T) => bool): Option<int>;
public fn removeAt<T>(values: []T, index: int): Option<T>;
```

| Операция | Предлагаемый наблюдаемый контракт |
|---|---|
| `copy` | Новый внешний массив даже для пустого входа, исходный порядок и точные значения элементов. Записи, вложенные массивы и другие reference values остаются общими. Исходник и его итераторы не меняются. Начальная generation нового массива — 0. |
| `findIndex` | Первый индекс с истинным predicate либо `None`. Обход слева направо, каждый посещённый элемент передаётся один раз, после первого совпадения следующих вызовов нет; для пустого массива callback не вызывается. |
| `removeAt` | При `0 <= index < length` возвращает точный удалённый элемент в `Some`, сохраняет относительный порядок остальных элементов и меняет существующий массив. Отрицательный индекс, `length` и более далёкий индекс дают `None` без изменений. |

`removeAt` выбирает отсутствие элемента через `Option`, аналогично `pop` и
`Map.remove`; это не изменение panic-контракта обычного `values[index]`.
Результат сохраняет reference identity удалённого элемента. Удалённый слот
перестаёт удерживать объект; capacity не уменьшается автоматически. Успешное
удаление увеличивает generation ровно один раз; `None` не инвалидирует
итератор. Все проверки и возможная подготовка результата предшествуют commit.
При allocation/resource/generation failure массив остаётся прежним.

D099 принимает для `findIndex` проверку generation перед каждым callback и
после его нормального возврата, включая возврат `true`. Иначе callback может
удалить текущий элемент и вернуть уже устаревший индекс. Мутация самого
массива через alias должна давать G4005; изменение поля записи или вложенного
массива не меняет generation внешнего массива. `reserve` разрешён по D053.
Если callback сам паникует, возвращается его исходная panic. Это намеренно
более строгая проверка на выходе, чем `for` с немедленным `break`/`return`;
различие явно принято и зафиксировано D099.

Predicate принимает обычную функцию или closure с аннотированными
типами. Исходная проверка 2026-09-08 обнаружила I0001 при сохранении closure;
теперь D098 реализует общий callable contract через typed MIR interfaces и
managed environments/cells. [Локальная проверка](../implementation/evidence/cp42-function-values-2026-09-08.md)
охватывает захват ID, совместную мутацию, escape lifetime и VM/native поведение.
Это снимает зависимость поиска от отсутствующего представления callback.
Nonfatal warning об escaping mutable capture остаётся отдельным removal gate
CP42; полная callback acceptance и claimed-platform evidence ещё открыты.
findIndex использует этот общий owner, без C-only predicate или AST expansion.
Контекстный вывод типов для обычных callback не входит в эту часть:
D094 специально ограничивает существующее послабление `Result.mapError`.
Поиск через predicate покрывает ID записей без добавления равенства по identity
или требования `@derive(Eq)`. `indexOf`, сортировка и семейство `removeAll`
не нужны для устранения текущего ручного цикла.

## Результаты мутаций и Option

Предлагается сохранить `push/reserve: Result<void, ...>` и
`Map.put/Set.add: Result<bool, CollectionGrowthError>`. Заменять их на `void`
нельзя: предел длины остаётся ожидаемой ошибкой, а bool уже устраняет повторный
поиск. Когда bool не нужен, `values.put(key, value)?;` явно обрабатывает ошибку
и не требует discard binding. При другом типе ошибки приложение использует
`mapError`. Best-effort печать сообщения после уже установленного ненулевого
exit status — отдельный осознанный discard, не повод ослаблять D074.

Для условий принят D096 — Option binding с существующими `const`/`let`:

```marimay
if const index = database.notes.findIndex((note: Note): bool => note.id == id) {
  // index: int; только в этой ветке
}
while const item = pending.pop() {
  // item — значение очередной Some; None завершает цикл
}
```

Синтаксис реализован отдельным D096 owner; `findIndex` в первом примере
реализован D099, а `pop` уже существует.
Обычное условие по-прежнему требует bool. RHS binding должен быть точно
`Option<T>`: `Result`, bool и похожий пользовательский enum не конвертируются.
В `if` RHS вычисляется один раз; в `while` заново перед каждой попыткой входа
и после `continue`. `Some` привязывает T, `None` выбирает `else`/выход. Новое
имя не видно в RHS, `else` и после конструкции; shadowing внешнего имени
допустим по обычным правилам. `const` запрещает перепривязку, `let` разрешает;
никакой глубокой копии или глубокой неизменяемости не появляется.

Первая реализация должна поддерживать один identifier с выводимым T, без
составного pattern и цепочки binding/`&&`. Существующий `match` остаётся
способом исчерпывающего разбора enum и Result; Option binding не отменяет
проверку exhaustiveness в match. Уточнение после сверки с D072 и разделом 2.3
спецификации: `const`/`let` — hard keywords. Первоначальное предположение этого
пакета о contextual names было неверным; обычные функции с такими именами не
допускаются, и новая реализация не вводит исключения.

## Модель notes после появления операций

Миграция D097 сохраняет функциональную модель существующего `add` и приводит
к ней `remove`: обе операции возвращают новую Database и внешний массив;
исходная Database не меняется. Удаление отсутствующего ID возвращает явную
прикладную ошибку; `nextId` не уменьшается и не переиспользуется. Валидация
входа выполняется перед изменением. Внутри новой копии можно применять
`removeAt`; это не делает опубликованную старую базу изменяемой.

Такой выбор продолжает уже существующий контракт добавления и позволяет
сохранить старую модель до успешной записи файла. Он требует O(n) нового
внешнего хранения; принятие этого расхода требует измерения на исходном
предельном notes dataset. `copy` остаётся shallow: изменение самой общей
записи будет видно в обеих версиях. Называть результат immutable snapshot
нельзя. `remove` мигрирован на `Result<Database, string>`; CLI сохраняет
возвращённое значение. 16 source tests проверяют обе модели copy/identity,
отказы, порядок и монотонность ID. Парный замер использует синтетическую базу
6 098 записей / 1 047 786 байт, зафиксированную до изменения потребителя.
Это отдельное evidence стоимости миграции, не полная performance acceptance.

Успешная атомарная запись и последующий stdout — разные эффекты. Закрытый
канал подтверждения даёт ненулевой exit, но не откатывает уже сохранённую
заметку. Это правило проверено на add/remove; автоматический повтор такой
команды может повторить прикладной эффект.

## Границы реализации и обязательные различающие проверки

`copy`/`removeAt` требуют narrow typed MIR operations и одинаковых runtime
контрактов VM/native. Простой перенос циклов приложения в checker или C
backend не подходит: repeated indexed stores меняют generation несколько
раз и могут остановиться после частичного сдвига. Копирование должно
сохранять rooting, поддерживать raw/reference layouts, резервировать известную
длину без роста по одному элементу и публиковать только полный результат.

Receiver selection остаётся у `checker_receiver_calls.c`; семантическая
проверка нового семейства — у отдельного checker owner. Runtime storage и
failure-atomic commit используют существующие managed-array boundaries.
Для поиска нужен отдельный callback/iteration owner, а Option binding —
свой owner поверх существующего CFG, cleanup и enum lowering. MIR verifier,
hostile bytecode и оба backend должны принимать уже проверенный контракт.
Не расширять `array.c` до нового смешанного координатора.

| Граница | Обязательные проверки перед закрытием соответствующей части |
|---|---|
| Copy | Пустой/new identity, raw/reference/nested values, independent outer mutation, shared inner identity, source iterator preservation, GC rooting, все allocation-failure indices. |
| Search | Нет совпадений/первое/последнее/duplicates, счётчик callback, alias mutation при true и false, nested mutation, reserve, исходная panic callback, тип/arity/ordinary-name collision. |
| Removal | -1, INT_MIN, length, INT_MAX, empty/single/middle/end; exact returned identity, stable order, aliases, generation exhaustion, no-op iterator, released reference slots. |
| Binding | Some/None, nested Option, RHS once/repeated loop, shadowing, immutable/mutable, scope leakage, break/continue/return/defer, definite initialization, Result rejection, malformed-header recovery, formatter idempotence. |
| Consumers | Исходные config/notes сценарии и ошибки, функциональные add/remove, JSON bytes, сохранение файла при неуспехе записи, memory/latency на прежних datasets. |

Для новых allocation-capable C API: successful/error teardown и каждый
достижимый deterministic failure index с `live_allocations == 0` и
`live_bytes == 0`; для семантики — VM/native differential и sanitizer CI.
Пакет тестов и документации добавляется с каждой реализацией, не после неё.
All-ten-apps, CP41, Tier 1, install и N0 остаются независимыми обязательствами.

Новых идей из книги Владимирова при подготовке этого пакета не использовано;
источники — действующие контракты репозитория, focused owners и реальные
программы. Прежние заимствования с номерами страниц остаются в журнале книги.


D100 завершает перенос `Result.mapError` на общий callable boundary; Notes
сохраняет collection-growth mapper перед циклом ID, config повторно использует
один output mapper. [Локальное evidence](../implementation/evidence/cp42-mapper-values-2026-09-08.md)
содержит прикладные проверки и исправление lifetime native roots. Общий enum
transport для `Result<T, never>` и nonfatal warning escaping mutable capture
остаются открытыми, с явными removal gates.
