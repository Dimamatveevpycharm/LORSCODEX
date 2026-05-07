# LORSC (`lorsc`)

Учебный мини-компилятор: `source (.lors) -> Flex/Bison frontend -> AST -> Sema -> LLVM IR -> RISC-V object`.

## Ключевые возможности

- Go-style синтаксис блоков и функций.
- Типы: `int`, `float`, `bool`, `void`.
- Конструкции: `if/else`, `for`, `return`, вызовы функций.
- Выражения с приоритетами, `?:`, short-circuit для `&&`/`||`.
- Отдельная семантическая фаза (`Sema`) с диагностикой ошибок.
- Генерация `LLVM IR` (`--emit-llvm`), asm-листинга (`--emit-asm`) и `.o` под `riscv64-unknown-linux-gnu`.

## Архитектура проекта

Компилятор разделён на независимые этапы:

1. Лексический анализ: `src/lexer.l`
2. Синтаксический анализ и AST: `src/parser.y`, `src/Parser.cpp`
3. Семантический анализ: `src/Sema.cpp`
4. Codegen и эмиссия артефактов: `src/Codegen.cpp`
5. CLI-драйвер: `src/main.cpp`

Подробно: [docs/architecture.md](docs/architecture.md)

## Требования к окружению

Рекомендуемая среда: `WSL Ubuntu` (сборка и запуск выполняются в Linux toolchain).

Минимальные зависимости:

- `build-essential`
- `cmake`
- `flex`
- `bison`
- `llvm-dev`
- `clang`
- `lld`
- `qemu-user`

Пример установки:

```bash
sudo apt update
sudo apt install -y build-essential cmake flex bison llvm-dev clang lld qemu-user
```

## Quick Start

```bash
cmake -S . -B build
cmake --build build -j

./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o \
  --emit-llvm build/demo_entry.ll \
  --emit-asm build/demo_entry.s
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
qemu-riscv64 build/demo_riscv
```

Ожидаемый вывод для demo:

```text
entry() = 8
```

## Сборка

Ручной способ:

```bash
cmake -S . -B build
cmake --build build -j
```

Скриптом:

```bash
bash scripts/build.sh
```

После сборки бинарник компилятора: `build/lorsc`.

## Пример использования

Компиляция в объектный файл:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o
```

Компиляция с выгрузкой LLVM IR:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll
```

Выгрузка asm-листинга для показа преподавателю:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-asm build/demo_entry.s
```

Проверка целевого triple в IR:

```bash
grep -n "target triple" build/demo_entry.ll
```

Проверка объекта:

```bash
file build/demo_entry.o
```

## Тестирование

Автотесты (positive + negative + integration):

```bash
bash scripts/run_tests.sh ./build/lorsc
```

Интеграционный прогон demo с `.ll/.s/.o`:

```bash
bash scripts/demo_run.sh
```

Подробно: [docs/testing.md](docs/testing.md)

## Структура репозитория

- `src/` — реализация компилятора
- `include/lorsc/` — публичные заголовки
- `tests/` — positive/negative/integration тесты
- `runtime/` — C runtime (`main.c`)
- `scripts/` — build/demo/tests скрипты
- `docs/` — спецификация языка, архитектура, тестирование и команды запуска

## Документация

- Шпаргалка по командам запуска: [docs/commands.md](docs/commands.md)
- Спецификация языка: [docs/language_spec.md](docs/language_spec.md)
- Архитектура: [docs/architecture.md](docs/architecture.md)
- Тестирование: [docs/testing.md](docs/testing.md)

## Особенности линковки под RISC-V в текущей WSL-среде

В текущей проверенной среде линковка командой без `lld` может падать с:

```text
unrecognised emulation mode: elf64lriscv
```

Рабочий вариант:

```bash
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
```

`-fuse-ld=lld` нужен для корректной RISC-V линковки, `-static` помогает запуску в `qemu-riscv64` без внешнего dynamic loader.

## Лицензия

Лицензия в репозитории явно не задана.

