# Спецификация языка LORSC

## 1. Общая модель
Язык LORSC — минимальный учебный язык с Go-style синтаксисом блоков:
- блоки в `{ ... }`
- ключевые слова `func`, `var`, `if`, `else`, `for`, `return`
- обязательные `;` для простых statement

Программа состоит только из объявлений функций.

## 2. Типы
- `int` (целое, в LLVM: `i64`)
- `float` (вещественное, в LLVM: `double`)
- `bool` (`true` / `false`, в LLVM: `i1`)
- `void` (только для возвращаемого типа функции)

## 3. Грамматика (упрощённо)
```ebnf
program      := { function_decl } ;
function_decl:= "func" ident "(" [ params ] ")" type block ;
params       := param { "," param } ;
param        := ident type ;
type         := "int" | "float" | "bool" | "void" ;

block        := "{" { statement } "}" ;
statement    := var_decl ";"
              | assign ";"
              | expr ";"
              | return_stmt ";"
              | if_stmt
              | for_stmt
              | block ;

var_decl     := "var" ident type [ "=" expr ] ;
assign       := ident "=" expr ;
return_stmt  := "return" [ expr ] ;

if_stmt      := "if" expr block [ "else" block ] ;
for_stmt     := "for" expr block
              | "for" [var_decl|assign] ";" expr ";" [assign] block ;

expr         := ... (см. таблицу приоритетов)
```

## 4. Приоритеты и ассоциативность операторов
От меньшего приоритета к большему:
1. `?:` (право-ассоциативный)
2. `||` (лево-ассоциативный)
3. `&&` (лево-ассоциативный)
4. `==`, `!=` (лево-ассоциативный)
5. `<`, `<=`, `>`, `>=` (лево-ассоциативный)
6. `+`, `-` (лево-ассоциативный)
7. `*`, `/`, `%` (лево-ассоциативный)
8. унарные `!`, `-` (право-ассоциативный)
9. первичные: литералы, идентификаторы, вызовы, скобки

## 5. Правила типов
Политика неявных преобразований:
- разрешено только `int -> float`
- `float -> int` неявно запрещено

### 5.1 Арифметика
- `int op int -> int`
- `float op float -> float`
- `int` с `float` -> `float` (подъём `int`)
- `%` только для `int`

### 5.2 Сравнения
- Числовые сравнения разрешены для `int/float` (с подъёмом `int`)
- Результат сравнения всегда `bool`
- `bool` разрешён только в `==`, `!=` с `bool`

### 5.3 Логические операции
- `&&`, `||`, `!` работают только с `bool`
- `&&` и `||` имеют short-circuit семантику

### 5.4 Условия
- условие `if` и `for` обязано иметь тип `bool`

### 5.5 Присваивание и инициализация
- RHS должен иметь тот же тип, что LHS, либо `int -> float`

### 5.6 Return
- `void`-функция: только `return;`
- не-`void`: `return expr;` совместимого типа

### 5.7 Тернарный оператор
`cond ? a : b`:
- `cond` обязан быть `bool`
- ветви совместимы:
  - одинаковые типы, или
  - `int` и `float` в любом порядке
- итоговый тип:
  - общий тип ветвей
  - `float` для пары `int/float`

## 6. Области видимости
- Используется стек scope (вектор таблиц символов)
- Каждый блок `{}` создаёт новый scope
- Повторное объявление имени в одном scope запрещено
- Внутренний scope может затенять имя внешнего scope

## 7. Диагностика
Единый формат ошибок:
```text
<file>:<line>:<col>: error: <message>
```

Примеры сообщений:
- `redefinition of variable 'x' in the same scope`
- `use of undeclared variable 'x'`
- `if condition must have type bool`
- `return type mismatch: expected int, got bool`


