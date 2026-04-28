# Тестирование LORSC

## 1. Цели тестирования
- Проверить корректность frontend (lexer/parser)
- Проверить семантические правила
- Проверить генерацию LLVM IR и объектного файла
- Проверить интеграционный сценарий линковки с runtime

## 2. Категории тестов

### 2.1 Позитивные (`tests/positive`)
- `arithmetic_precedence.lors`  
  Проверка приоритетов `+` и `*`.
- `float_mix.lors`  
  Проверка `int -> float` и сравнений float.
- `bool_logic.lors`  
  Проверка `bool`, `!`, `&&`, `if`.
- `for_loop.lors`  
  Проверка `for cond {}`.
- `for_full_form.lors`  
  Проверка `for init; cond; post {}`.
- `ternary.lors`  
  Проверка `?:`.
- `short_circuit.lors`  
  Проверка short-circuit в `&&`/`||`.
- `functions.lors`  
  Проверка функций и вызова.

### 2.2 Негативные синтаксиса (`tests/negative/syntax`)
- `missing_semicolon.lors`
- `missing_rparen.lors`
- `bad_expression.lors`

Ожидание: parser ошибка в формате diagnostics.

### 2.3 Негативные семантики (`tests/negative/semantic`)
- `redeclare.lors` — redeclaration в одном scope
- `undeclared.lors` — использование необъявленного имени
- `assign_type.lors` — запрещённое `float -> int`
- `return_type.lors` — неверный тип return
- `call_arity.lors` — неверное число аргументов
- `if_not_bool.lors` — условие не `bool`
- `ternary_branch_mismatch.lors` — несовместимые ветви `?:`

### 2.4 Интеграционные (`tests/integration`)
- `demo_entry.lors`  
  Полный путь: `.lors -> .ll/.o -> link runtime/main.c -> qemu run`.
- `full_feature_showcase.lors`
  Интеграционный сценарий на смеси функций, циклов, `bool` и `float`.
- `prime_count.lors`
  Подсчёт количества простых чисел от `0` до заданной границы с проверкой реального результата выполнения.

## 3. Автоматизация
Скрипт: `scripts/run_tests.sh`.

Запуск:
```bash
bash scripts/run_tests.sh ./build/lorsc
```

Скрипт:
- компилирует позитивные примеры (ожидает успех)
- компилирует негативные примеры (ожидает ошибку с `error:`)
- прогоняет интеграционные примеры через `clang` + `qemu-riscv64`
- проверяет, что для integration-кейсов генерируется asm-файл
- выводит итоговые счётчики pass/fail

## 4. Критерии успешности
- Все позитивные тесты: успешная генерация `.o` (и `.ll` где задано).
- Все негативные тесты: компилятор завершился с ошибкой и выдал diagnostic.
- Интеграционные тесты: успешная генерация `.ll/.s/.o`, линковка и запуск под `qemu-riscv64` (если доступен в системе).

## 5. Ручные проверки для защиты
1. Показать ошибку parser на синтаксически неверном файле.
2. Показать ошибку sema на типовой несовместимости.
3. Сгенерировать `.ll` и продемонстрировать `PHI` для `?:`.
4. Сгенерировать `asm` через `--emit-asm` и показать преподавателю итоговый низкоуровневый код.
5. Сгенерировать `.o` под RISC-V и показать линковку с `runtime/main.c`.
