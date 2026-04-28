# READMEchild.md: очень подробный гайд по проекту `lorsc`

## 1) Что это за проект

`lorsc` — это учебный мини-компилятор.

Он берёт исходный файл на небольшом учебном языке (`.lors`) и проходит несколько этапов:

1. `Flex`-лексер разбивает текст на токены.
2. `Bison`-парсер строит AST (дерево программы).
3. Семантический анализ (`Sema`) проверяет типы, области видимости, `return`, вызовы функций и т.д.
4. LLVM Codegen строит `LLVM IR`.
5. LLVM backend может дополнительно выгрузить читаемый `asm`-листинг.
6. LLVM backend делает объектный файл `.o` под `RISC-V`.

Дальше объектный файл можно слинковать с `runtime/main.c`, получить RISC-V исполняемый файл и запустить его через `qemu-riscv64`.

## 2) Для кого эта инструкция

Эта инструкция написана для начинающего студента, который:

- только начинает работать с `WSL`;
- неуверенно пользуется `CMake`, `LLVM`, `Flex`, `Bison`, `QEMU`;
- хочет повторить рабочий сценарий шаг за шагом.

Важно: команды ниже нужно выполнять **внутри Linux-окружения WSL**, а не в обычном `cmd.exe` и не в обычном Windows PowerShell без WSL.

## 3) Что нужно установить

Минимум инструментов:

- `cmake`
- `clang`
- `llvm-config` (часть LLVM dev-пакета)
- `flex`
- `bison`
- `qemu-riscv64` (из `qemu-user`)
- стандартные инструменты сборки (`gcc/g++`, `make`)

Пример установки в Ubuntu (WSL):

```bash
sudo apt update
sudo apt install -y build-essential cmake flex bison llvm-dev clang qemu-user lld
```

Проверка, что инструменты доступны:

```bash
cmake --version
clang --version
llvm-config --version
flex --version
bison --version
qemu-riscv64 --version
```

В проверенной среде проекта выводились версии:

- `cmake 3.28.3`
- `clang 18.1.3`
- `llvm-config 18.1.3`
- `flex 2.6.4`
- `bison 3.8.2`
- `qemu-riscv64 8.2.2`

## 4) Где должен лежать проект

Лучший вариант: хранить и собирать проект **в Linux-файловой системе WSL**.

Пример пути в WSL:

```text
/home/matveevda/projects/LORSCODEX
```

Из Windows этот же путь можно открыть через:

```text
\\wsl$\Ubuntu-24.04\home\matveevda\projects\LORSCODEX
```

Почему так лучше:

- меньше проблем с правами и путями;
- стабильнее работают сборка и bash-скрипты;
- проще запускать Linux toolchain (`clang --target=...`, `qemu-riscv64`).

## 5) Структура проекта

Основные каталоги:

- `src/` — исходники компилятора (`main.cpp`, `Sema.cpp`, `Codegen.cpp`, `parser.y`, `lexer.l`).
- `include/lorsc/` — заголовочные файлы (AST, Sema, Codegen, Diagnostics).
- `runtime/` — минимальный runtime, файл `main.c`, который вызывает `entry()`.
- `tests/` — тесты языка:
  - `positive/` — программы, которые должны компилироваться;
  - `negative/syntax/` — синтаксические ошибки;
  - `negative/semantic/` — семантические ошибки;
  - `integration/` — интеграционные программы (`demo_entry.lors`, `full_feature_showcase.lors`, `prime_count.lors`).
- `scripts/` — вспомогательные скрипты:
  - `build.sh` — сборка через CMake;
  - `demo_run.sh` — полный демо-сценарий с генерацией `.ll/.s/.o` до запуска в QEMU;
  - `run_tests.sh` — автопрогон positive/negative/integration тестов.
- `docs/` — спецификация и архитектурные документы.
- `CMakeLists.txt` — конфигурация сборки.
- `README.md` — основной краткий README.
- `docs/commands.md` — компактный список команд для сборки, прогона и проверки компилятора.

## 6) Сборка проекта

Перейдите в корень репозитория:

```bash
cd /home/matveevda/projects/LORSCODEX
```

Сконфигурируйте проект:

```bash
cmake -S . -B build
```

Соберите:

```bash
cmake --build build -j
```

После успешной сборки:

- появится каталог `build/`;
- появится исполняемый файл компилятора `build/lorsc`;
- при первом конфиге будут сгенерированы файлы парсера/лексера в `build/generated/`.

Альтернатива тем же шагам:

```bash
bash scripts/build.sh
```

## 7) Быстрая проверка после сборки

Проверьте, что бинарник запускается:

```bash
./build/lorsc
```

Ожидаемое поведение: компилятор печатает строку `Usage: ...` в `stderr` и завершается с ошибкой аргументов (это нормально, потому что входной файл не передан).

## 8) Как скомпилировать программу на языке проекта

Пример на реальном файле:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o
```

Что произойдёт:

- `tests/integration/demo_entry.lors` прочитается и пройдет parser + sema;
- будет создан `build/demo_entry.o` (RISC-V объектный файл).

## 9) Как сгенерировать LLVM IR

Рабочая команда:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll
```

После выполнения появятся:

- `build/demo_entry.ll` — текстовый LLVM IR;
- `build/demo_entry.o` — объектный файл.

Проверка `target triple` в IR:

```bash
grep -n "target triple" build/demo_entry.ll
```

Ожидается строка вида:

```text
target triple = "riscv64-unknown-linux-gnu"
```

## 10) Как получить asm-листинг

Рабочая команда:

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-asm build/demo_entry.s
```

После выполнения появятся:

- `build/demo_entry.s` — текстовый RISC-V asm;
- `build/demo_entry.o` — объектный файл.

Этот файл удобно показывать преподавателю: он человекочитаемее, чем бинарный `.o`, и напрямую отражает низкоуровневый код, который генерирует компилятор.

## 11) Как проверить объектный файл

Команда:

```bash
file build/demo_entry.o
```

Ожидаемый смысл вывода:

- файл `ELF 64-bit relocatable`;
- архитектура `RISC-V`;
- ABI (в проверенной среде было `double-float ABI`).

Если вы видите `RISC-V`, значит кодогенерация в нужную архитектуру прошла.

## 12) Как выполнить линковку с `runtime/main.c`

Рабочая команда для текущей WSL-среды:

```bash
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
```

Почему так:

- `-fuse-ld=lld` нужен, потому что в этой среде стандартный системный линкер может не поддерживать нужную RISC-V emulation.
- без `lld` наблюдалась ошибка:
  - `unrecognised emulation mode: elf64lriscv`
- `-static` позволяет избежать зависимости от RISC-V dynamic loader в системе при запуске под QEMU.

Коротко: именно эта команда реально прошла в текущей рабочей среде этого репозитория.

## 13) Как запустить через QEMU

Команда:

```bash
qemu-riscv64 build/demo_riscv
```

Ожидаемый вывод для демо-примера:

```text
entry() = 8
```

Что это означает:

- QEMU успешно запустил RISC-V бинарник;
- runtime вызвал функцию `entry()` из скомпилированного `.lors` файла;
- программа корректно вернула результат.

## 14) Как запустить все тесты

Команда:

```bash
bash scripts/run_tests.sh ./build/lorsc
```

Скрипт делает следующее:

- компилирует все `tests/positive/*.lors` и ждёт успех;
- компилирует все `tests/negative/syntax/*.lors` и ждёт ошибку;
- компилирует все `tests/negative/semantic/*.lors` и ждёт ошибку;
- прогоняет интеграционные программы через `.ll/.s/.o -> clang -> qemu`;
- считает pass/fail.

Успешный прогон выглядит как:

- `[PASS] ...` по всем тестам;
- `Total failed: 0`.

Примечание: integration-примеры теперь тоже входят в `scripts/run_tests.sh`, а вручную их всё ещё можно прогонять через `scripts/demo_run.sh`.

## 15) Полный сценарий от нуля до результата

Ниже единая последовательность команд:

```bash
cd /home/matveevda/projects/LORSCODEX

cmake -S . -B build
cmake --build build -j

./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll --emit-asm build/demo_entry.s

grep -n "target triple" build/demo_entry.ll
file build/demo_entry.o

clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
qemu-riscv64 build/demo_riscv

bash scripts/run_tests.sh ./build/lorsc
```

## 16) Типичные ошибки и их решение

### `cmake: command not found`

Причина: CMake не установлен в WSL.

Решение:

```bash
sudo apt update
sudo apt install -y cmake
```

### `flex: command not found`

Причина: отсутствует Flex.

Решение:

```bash
sudo apt install -y flex
```

### `bison: command not found`

Причина: отсутствует Bison.

Решение:

```bash
sudo apt install -y bison
```

### Проблемы с LLVM/clang

Симптомы:

- CMake не находит LLVM;
- не находится `llvm-config`;
- ошибки линковки LLVM-библиотек.

Базовое решение:

```bash
sudo apt install -y llvm-dev clang lld
```

Проверьте:

```bash
llvm-config --version
clang --version
```

### `unrecognised emulation mode: elf64lriscv`

Причина: используется линкер, который не умеет нужную RISC-V emulation.

Решение: используйте `lld`:

```bash
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
```

### `qemu-riscv64: command not found`

Причина: не установлен пакет `qemu-user`.

Решение:

```bash
sudo apt install -y qemu-user
```

### `No such file or directory` (файл не найден)

Частые причины:

- вы не в корне проекта;
- опечатка в пути;
- файл ещё не сгенерирован.

Проверка текущей директории:

```bash
pwd
ls -la
```

### Проект открыт не в той директории

Если `cmake -S . -B build` сообщает, что нет `CMakeLists.txt`, значит вы не в корне репозитория.

Нужно перейти в каталог проекта:

```bash
cd /home/matveevda/projects/LORSCODEX
```

### Команды запускаются не в WSL

Признаки:

- пути вида `C:\...`;
- отсутствуют Linux-инструменты;
- не работает `qemu-riscv64`.

Решение: откройте WSL-терминал Ubuntu и повторите команды там.

### Бинарник не запускается в QEMU

Частый случай:

```text
Could not open '/lib/ld-linux-riscv64-lp64d.so.1'
```

Решение:

- собрать статически (как в этой инструкции): добавить `-static`;
- либо устанавливать и настраивать RISC-V sysroot/runtime (сложнее).

### Тесты падают

Что делать:

1. Сначала убедитесь, что проект пересобран.
2. Запустите `bash scripts/run_tests.sh ./build/lorsc`.
3. Посмотрите имя первого `[FAIL]` теста.
4. Скомпилируйте конкретный `.lors` вручную, чтобы увидеть диагностику.
5. Сверьтесь с `docs/language_spec.md` и `docs/testing.md`.

## 16) FAQ

### Зачем нужен WSL?

Проект использует Linux toolchain (`flex`, `bison`, `clang`, `qemu-riscv64`), поэтому WSL даёт стабильную рабочую среду на Windows.

### Почему лучше хранить проект в Linux workspace?

Так меньше проблем с правами, путями и производительностью I/O по сравнению с работой через внешние Windows-диски.

### Зачем нужен LLVM IR?

Это промежуточное представление, которое удобно читать, проверять и отлаживать до генерации машинного кода.

### Почему `.o` нельзя запустить напрямую?

`.o` — это объектный файл (часть программы), а не финальный исполняемый файл. Его нужно слинковать.

### Зачем нужен `runtime/main.c`?

Он даёт точку входа `main()`, которая вызывает функцию `entry()` из вашей программы.

### Что делает QEMU?

`qemu-riscv64` эмулирует RISC-V CPU на вашей машине и позволяет запустить собранный RISC-V бинарник.

## 17) Раздел для защиты/отчёта

Что удобно показать преподавателю:

1. Успешную сборку:
   - `cmake -S . -B build`
   - `cmake --build build -j`
2. Генерацию артефактов:
   - `./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll`
3. Проверку IR/объекта:
   - `grep -n "target triple" build/demo_entry.ll`
   - `file build/demo_entry.o`
4. Линковку и запуск:
   - `clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv`
   - `qemu-riscv64 build/demo_riscv`
5. Прогон тестов:
   - `bash scripts/run_tests.sh ./build/lorsc`

Какие файлы открыть на защите:

- `src/lexer.l`
- `src/parser.y`
- `src/Sema.cpp`
- `src/Codegen.cpp`
- `build/demo_entry.ll`
- `runtime/main.c`
- `scripts/run_tests.sh`

Какие артефакты приложить к отчёту:

- лог успешной сборки;
- `build/demo_entry.ll`;
- вывод `file build/demo_entry.o`;
- вывод `qemu-riscv64 build/demo_riscv` (`entry() = ...`);
- лог `run_tests.sh` с `Total failed: 0`.
