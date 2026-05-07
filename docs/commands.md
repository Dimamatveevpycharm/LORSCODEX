# Команды для прогона компилятора LORSC

Ниже собраны основные команды, которые нужны для сборки, запуска и проверки компилятора.
Все команды предполагают запуск из корня репозитория:

```bash
cd /home/matveevda/projects/LORSCODEX
```

## 1. Сборка проекта

Сконфигурировать и собрать проект через CMake:

```bash
cmake -S . -B build
cmake --build build -j
```

Описание:
- создаёт каталог `build/`;
- генерирует служебные файлы сборки;
- собирает исполняемый файл компилятора `build/lorsc`.

Альтернатива через готовый скрипт:

```bash
bash scripts/build.sh
```

Описание:
- выполняет ту же сборку, но одной командой.

## 2. Проверка, что компилятор запускается

```bash
./build/lorsc
```

Описание:
- без аргументов компилятор должен показать `Usage: ...`;
- это быстрый способ понять, что бинарник собрался и запускается.

## 3. Компиляция `.lors` в объектный файл

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o
```

Описание:
- читает исходник `demo_entry.lors`;
- прогоняет parser, sema и codegen;
- создаёт RISC-V объектный файл `build/demo_entry.o`.

## 4. Генерация LLVM IR

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll
```

Описание:
- помимо объектного файла создаёт текстовый LLVM IR;
- удобно для разбора промежуточного представления программы.

## 5. Генерация asm-листинга

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-asm build/demo_entry.s
```

Описание:
- создаёт читаемый RISC-V asm-файл `build/demo_entry.s`;
- этот файл удобно показывать преподавателю как низкоуровневый код, который генерирует компилятор.

## 6. Генерация сразу всех основных артефактов

```bash
./build/lorsc tests/integration/demo_entry.lors -o build/demo_entry.o --emit-llvm build/demo_entry.ll --emit-asm build/demo_entry.s
```

Описание:
- за один прогон создаёт `.o`, `.ll` и `.s`;
- это самый удобный режим для демонстрации проекта.

## 7. Проверка целевого target triple в LLVM IR

```bash
grep -n "target triple" build/demo_entry.ll
```

Описание:
- показывает, под какую архитектуру сгенерирован IR;
- ожидаемое значение: `riscv64-unknown-linux-gnu`.

## 8. Проверка объектного файла

```bash
file build/demo_entry.o
```

Описание:
- показывает тип файла и целевую архитектуру;
- в корректном случае в выводе должен быть `RISC-V`.

## 9. Линковка с runtime

```bash
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/demo_entry.o -o build/demo_riscv
```

Описание:
- линкует сгенерированный объектный файл с `runtime/main.c`;
- создаёт готовый RISC-V исполняемый файл `build/demo_riscv`;
- `-fuse-ld=lld` нужен для корректной линковки в текущей WSL-среде;
- `-static` упрощает запуск под `qemu-riscv64`.

## 10. Запуск результата через QEMU

```bash
qemu-riscv64 build/demo_riscv
```

Описание:
- запускает собранную RISC-V программу;
- для `demo_entry.lors` ожидаемый вывод:

```text
entry() = 8
```

## 11. Полный демо-прогон одной командой

```bash
bash scripts/demo_run.sh
```

Описание:
- компилирует `tests/integration/demo_entry.lors`;
- генерирует `.ll`, `.s`, `.o`;
- линкует результат;
- запускает программу под `qemu-riscv64`.

## 12. Запуск всех автотестов

```bash
bash scripts/run_tests.sh ./build/lorsc
```

Описание:
- прогоняет positive-тесты;
- прогоняет negative syntax/semantic тесты;
- прогоняет integration-тесты;
- проверяет ожидаемый вывод integration-программ.

## 13. Пример интеграционного теста на простые числа

```bash
./build/lorsc tests/integration/prime_count.lors -o build/prime_count.o --emit-llvm build/prime_count.ll --emit-asm build/prime_count.s
clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static runtime/main.c build/prime_count.o -o build/prime_count_riscv
qemu-riscv64 build/prime_count_riscv
```

Описание:
- компилирует программу, считающую количество простых чисел от `0` до заданной границы;
- в текущем тесте используется граница `1000`;
- ожидаемый вывод:

```text
entry() = 168
```

