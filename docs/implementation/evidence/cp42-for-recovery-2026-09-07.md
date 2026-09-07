# CP42: восстановление после неверного compound-binding в for

Дата: 2026-09-07. База `7ff26f856372ff16825e62de365db5f691a005c2` плюс
локальный parser-кандидат; хеши исходников, compiler и сырые результаты —
[в JSON](cp42-for-recovery-2026-09-07.json). Это локальное исправление
обязательства из collection-results/handoff, не закрытие CP42 или CLI0.

## Воспроизведение и причина

`for (key, value) in values {}` не соответствует
[D068](../../design/decisions.md#d068-one-for-name-in-iterable-model-with-closed-integer-ranges):
binding должен быть одним identifier. Parser диагностировал отсутствие имени,
но затем разбирал tuple как iterable, терял настоящее `in` и границу тела.
Следующие функции попадали внутрь ошибочного блока. Малый пример давал 13
диагностик; 30 следующих корректных функций исчерпывали бюджет, и fmt показывал
только `F0003: formatter parser exhausted a defensive limit`.

Declaration owner теперь сохраняет неподдерживаемый compound-binding в
lossless error node и синхронизируется перед `in`, фигурной границей либо EOF.
Сканирование последовательное, без рекурсивного разбора неверного pattern;
allocation/event limits остаются действующими. Следующие объявления сохраняются,
а исходник остаётся ошибочным с одной `P0001` в заголовке. Formatter получает
обычную parser-диагностику. Грамматика не расширена: destructuring выполняется
через существующий match внутри тела. D068 и нормативный текст это уточняют.

## Проверки

- Десять вариантов: tuple/array, вложенные и пустые формы, trailing comma,
  отсутствующие/неверные закрывающие скобки, Unicode и комментарий с `in }`.
  В каждом сохранены 31 функция, тело цикла, точный offset и все source bytes;
  достаточно бюджета одной диагностики.
- Все достижимые allocation-failure indices успешного recovery и material
  event/nesting-limit путей; после teardown `live_allocations == 0` и
  `live_bytes == 0`. Formatter отвергает diagnosed tree без выделения памяти.
- Process driver проверяет одинаковые status/stdout/stderr для fmt check/write,
  check, VM/native run, Debug/Release build; source SHA-256 сохраняется,
  executable не публикуется. Оба fuzz-корпуса получили сохранённые случаи.
- Focused Release **2/2**, 0,07 с; Debug **2/2**, 1,04 с;
  ASan+UBSan **2/2**, 2,15 с. Полный Release **74/74**, 232,61 с.
  Это проверки macOS arm64, не полная Tier 1 матрица.
- Все девять repository gates прошли; `git diff --check` чистый.

Предыдущий push `7ff26f8` запустил
[CI №205](http://ci.example:3000/Marimay/marimay/actions/runs/205).
Его sanitizer job 1730 отверг generated C теста `mmay_cp38_json` из-за
необъявленной строковой константы; загрузка диагностических артефактов отдельно
получила HTTP 500. Причина generated-C дефекта остаётся открытой; parser repair
не объявляется его исправлением. Локальная зелёная проверка не заменяет этот CI.

Идеи книги Владимирова здесь не применялись: исправление следует D068 и
воспроизведённому нарушению recovery. Книжные страницы ему не приписываются.
