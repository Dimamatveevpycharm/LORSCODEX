# Архитектура компилятора LORSC

## 1. Фазы компиляции
Компилятор выполняет строго последовательные фазы:
1. Лексический анализ (`src/lexer.l`)
2. Синтаксический анализ + AST (`src/parser.y`)
3. Семантический анализ (`src/Sema.cpp`)
4. Генерация LLVM IR (`src/Codegen.cpp`)
5. Эмиссия `.ll` и/или `.o` (`src/Codegen.cpp`)

Если в parser или sema есть ошибки, codegen не запускается.

## 2. Модули
- `include/lorsc/AST.h`, `src/AST.cpp`  
  Узлы AST: выражения, statements, функции, программа.
- `include/lorsc/Diagnostics.h`, `src/Diagnostics.cpp`  
  Централизованные diagnostics в едином формате.
- `src/parser.y`, `src/lexer.l`, `src/Parser.cpp`  
  Flex/Bison frontend и API `parseFile(...)`.
- `include/lorsc/Sema.h`, `src/Sema.cpp`  
  Проверки имён, типов, return, вызовов, тернарного оператора.
- `include/lorsc/Codegen.h`, `src/Codegen.cpp`  
  LLVM backend: IR, verifyFunction, verifyModule, object emission.
- `src/main.cpp`  
  CLI-драйвер пайплайна.

## 3. AST и владение памятью
- Базовые классы: `Node`, `Expr`, `Stmt`.
- В AST владение реализовано через `std::unique_ptr`.
- Parser создаёт узлы и передаёт владение в AST-контейнеры.
- Каждое выражение хранит `inferredType`, выставляемый на sema-фазе.

## 4. Семантический анализ
### 4.1 Таблицы символов
- Функции: `unordered_map<string, FunctionSymbol>` (глобально)
- Переменные: `vector<unordered_map<string, TypeKind>>` (стек scope)

### 4.2 Проходы
1. Регистрация сигнатур функций (проверка дубликатов)
2. Проверка тел функций:
   - объявления/использования
   - типы выражений
   - присваивания
   - вызовы функций
   - return
   - `if`/`for` условия
   - `?:` правила

### 4.3 Правило неявных преобразований
Единственное разрешённое: `int -> float`.

## 5. LLVM codegen
### 5.1 Типы
- `int -> i64`
- `float -> double`
- `bool -> i1`
- `void -> void`

### 5.2 Переменные
- Для локальных переменных используется `alloca` в entry-блоке.
- Чтение/запись: `load`/`store`.

### 5.3 Управляющий поток
- `if/else`: отдельные basic blocks + merge
- `for`: `cond/body/post/exit`
- `return`: явная генерация `ret`

### 5.4 Выражения
- `&&`, `||`: short-circuit через ветвление + `PHI`
- `?:`: then/else/merge + `PHI`
- `int -> float`: `sitofp` при необходимости

### 5.5 Верификация
- `verifyFunction` после генерации каждой функции
- `verifyModule` перед выводом

## 6. Эмиссия объектного файла
- По умолчанию target triple: `riscv64-unknown-linux-gnu`.
- Можно переопределить: `--target <triple>`.
- Объектный файл создаётся через `TargetMachine::addPassesToEmitFile`.

## 7. Принятые архитектурные решения
1. Минимальный язык без лишних конструкций (для надёжной защиты).
2. Жёсткое разделение parser/sema/codegen.
3. Единый формат диагностик и единые типовые правила в sema.
4. LLVM используется как backend-инфраструктура без ручной генерации RISC-V инструкций.

