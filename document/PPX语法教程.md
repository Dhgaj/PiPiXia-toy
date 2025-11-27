# PiPiXia (PPX) 编程语言语法教程

> PiPiXia 是一个现代化的静态类型编程语言，具有简洁的语法和强大的类型系统。

---

## 📚 目录

### [第一章：语言概述](#第一章语言概述)
- [1.1 什么是 PiPiXia](#11-什么是-pipixia)
- [1.2 语言特性](#12-语言特性)
- [1.3 Hello World](#13-hello-world)
- [1.4 编译和运行](#14-编译和运行)

### [第二章：词法规则](#第二章词法规则)
- [2.1 注释](#21-注释)
  - 2.1.1 单行注释
  - 2.1.2 多行注释
- [2.2 标识符](#22-标识符)
  - 2.2.1 命名规则
  - 2.2.2 保留关键字
- [2.3 字面量](#23-字面量)
  - 2.3.1 整数字面量
  - 2.3.2 浮点数字面量
  - 2.3.3 字符串字面量
  - 2.3.4 插值字符串
  - 2.3.5 字符字面量
  - 2.3.6 布尔字面量
  - 2.3.7 数组字面量
- [2.4 转义序列](#24-转义序列)

### [第三章：数据类型](#第三章数据类型)
- [3.1 基本类型](#31-基本类型)
  - 3.1.1 整数类型 (int)
  - 3.1.2 浮点类型 (double)
  - 3.1.3 字符类型 (char)
  - 3.1.4 布尔类型 (bool)
  - 3.1.5 字符串类型 (string)
- [3.2 数组类型](#32-数组类型)
  - 3.2.1 一维数组
  - 3.2.2 多维数组
  - 3.2.3 数组字面量
  - 3.2.4 空数组
- [3.3 类型转换](#33-类型转换)
  - 3.3.1 隐式类型提升
  - 3.3.2 混合类型运算

### [第四章：变量和常量](#第四章变量和常量)
- [4.1 变量声明](#41-变量声明)
  - 4.1.1 let 关键字
  - 4.1.2 类型标注
  - 4.1.3 变量初始化
  - 4.1.4 变量赋值
- [4.2 常量声明](#42-常量声明)
  - 4.2.1 const 关键字
  - 4.2.2 常量的不可变性
- [4.3 变量作用域](#43-变量作用域)
  - 4.3.1 全局作用域
  - 4.3.2 函数作用域
  - 4.3.3 块级作用域

### [第五章：运算符](#第五章运算符)
- [5.1 算术运算符](#51-算术运算符)
  - 5.1.1 加法和减法 (+, -)
  - 5.1.2 乘法和除法 (*, /)
  - 5.1.3 整除运算 (//)
  - 5.1.4 取模运算 (%)
- [5.2 比较运算符](#52-比较运算符)
  - 5.2.1 相等性比较 (==, !=)
  - 5.2.2 关系比较 (<, >, <=, >=)
- [5.3 逻辑运算符](#53-逻辑运算符)
  - 5.3.1 逻辑与 (&&)
  - 5.3.2 逻辑或 (||)
  - 5.3.3 逻辑非 (!)
- [5.4 赋值运算符](#54-赋值运算符)
  - 5.4.1 简单赋值 (=)
  - 5.4.2 复合赋值 (+=, -=, *=, /=, //=, %=)
- [5.5 一元运算符](#55-一元运算符)
  - 5.5.1 负号运算符 (-)
  - 5.5.2 逻辑非运算符 (!)
- [5.6 运算符优先级](#56-运算符优先级)

### [第六章：表达式](#第六章表达式)
- [6.1 算术表达式](#61-算术表达式)
- [6.2 比较表达式](#62-比较表达式)
- [6.3 逻辑表达式](#63-逻辑表达式)
- [6.4 字符串插值表达式](#64-字符串插值表达式)
  - 6.4.1 插值语法 ${}
  - 6.4.2 表达式插值
  - 6.4.3 算术表达式插值
  - 6.4.4 比较表达式插值

### [第七章：控制流语句](#第七章控制流语句)
- [7.1 条件语句](#71-条件语句)
  - 7.1.1 if 语句
  - 7.1.2 if-else 语句
  - 7.1.3 if-else if-else 链
- [7.2 循环语句](#72-循环语句)
  - 7.2.1 while 循环
  - 7.2.2 for 循环（范围循环）
  - 7.2.3 循环变量作用域
- [7.3 跳转语句](#73-跳转语句)
  - 7.3.1 break 语句
  - 7.3.2 continue 语句
  - 7.3.3 return 语句
- [7.4 分支语句](#74-分支语句)
  - 7.4.1 switch 语句
  - 7.4.2 case 子句
  - 7.4.3 default 子句

### [第八章：函数](#第八章函数)
- [8.1 函数定义](#81-函数定义)
  - 8.1.1 func 关键字
  - 8.1.2 函数签名
  - 8.1.3 参数列表
  - 8.1.4 返回类型
  - 8.1.5 函数体
- [8.2 函数调用](#82-函数调用)
  - 8.2.1 调用语法
  - 8.2.2 参数传递
  - 8.2.3 返回值接收
- [8.3 函数参数](#83-函数参数)
  - 8.3.1 单参数函数
  - 8.3.2 多参数函数
  - 8.3.3 无参数函数
- [8.4 函数返回值](#84-函数返回值)
  - 8.4.1 有返回值函数
  - 8.4.2 无返回值函数
  - 8.4.3 提前返回
- [8.5 递归函数](#85-递归函数)
- [8.6 main 函数](#86-main-函数)

### [第九章：数组操作](#第九章数组操作)
- [9.1 数组声明和初始化](#91-数组声明和初始化)
  - 9.1.1 固定大小数组
  - 9.1.2 数组字面量
  - 9.1.3 空数组
- [9.2 数组访问](#92-数组访问)
  - 9.2.1 索引访问
  - 9.2.2 多维数组访问
  - 9.2.3 边界检查
- [9.3 数组赋值](#93-数组赋值)
- [9.4 数组遍历](#94-数组遍历)

### [第十章：字符串操作](#第十章字符串操作)
- [10.1 字符串字面量](#101-字符串字面量)
- [10.2 转义字符](#102-转义字符)
- [10.3 字符串插值](#103-字符串插值)
  - 10.3.1 基本插值
  - 10.3.2 表达式插值
  - 10.3.3 复杂表达式插值
- [10.4 字符串操作](#104-字符串操作)
  - 10.4.1 字符串拼接
  - 10.4.2 字符串访问

### [第十一章：异常处理](#第十一章异常处理)
- [11.1 Try-Catch 语句](#111-try-catch-语句)
  - 11.1.1 try 块
  - 11.1.2 catch 块
  - 11.1.3 异常变量
- [11.2 Throw 语句](#112-throw-语句)
  - 11.2.1 抛出字符串异常
  - 11.2.2 异常传播
- [11.3 嵌套异常处理](#113-嵌套异常处理)

### [第十二章：模块系统](#第十二章模块系统)
- [12.1 Import 语句](#121-import-语句)
  - 12.1.1 基本导入
  - 12.1.2 模块别名 (as)
- [12.2 模块使用](#122-模块使用)
  - 12.2.1 调用模块函数
  - 12.2.2 模块命名空间
- [12.3 创建模块](#123-创建模块)
  - 12.3.1 模块文件结构
  - 12.3.2 函数导出

### [第十三章：内置函数](#第十三章内置函数)
- [13.1 输入输出函数](#131-输入输出函数)
  - 13.1.1 print() 函数
  - 13.1.2 print(value, nowrap) 连续输出
  - 13.1.3 input() 函数
- [13.2 字符串函数](#132-字符串函数)

### [第十四章：编程实践](#第十四章编程实践)
- [14.1 命名约定](#141-命名约定)
- [14.2 代码格式](#142-代码格式)
- [14.3 注释规范](#143-注释规范)
- [14.4 错误处理](#144-错误处理)
- [14.5 代码组织](#145-代码组织)

### [第十五章：常见问题](#第十五章常见问题)
- [15.1 类型错误](#151-类型错误)
- [15.2 Switch 语句问题](#152-switch-语句问题)
- [15.3 数组越界](#153-数组越界)
- [15.4 常量修改](#154-常量修改)

### [附录](#附录)
- [附录 A：完整示例程序](#附录-a完整示例程序)
- [附录 B：保留关键字列表](#附录-b保留关键字列表)
- [附录 C：运算符优先级表](#附录-c运算符优先级表)
- [附录 D：转义序列表](#附录-d转义序列表)

---

## 第一章：语言概述

### 1.1 什么是 PiPiXia

PiPiXia (PPX) 是一个现代化的静态类型编程语言，设计目标是提供简洁的语法和强大的类型系统。PPX 编译器将源代码编译为高效的机器码，支持多种编程范式。

**主要特点**：
- 静态类型系统，提供编译期类型检查
- 简洁清晰的语法，易于学习和使用
- 支持函数式编程特性
- 强大的字符串插值功能
- 完善的异常处理机制
- 模块化的代码组织

### 1.2 语言特性

PPX 支持以下核心特性：

#### 数据类型
- 基本类型：int, double, string, char, bool
- 数组类型：一维和多维数组
- 类型推断：自动类型提升

#### 控制结构
- 条件语句：if, if-else, if-else if-else
- 循环语句：while, for (范围循环)
- 跳转语句：break, continue, return
- 分支语句：switch-case

#### 函数特性
- 函数定义和调用
- 参数传递
- 返回值
- 递归函数

#### 高级特性
- 字符串插值（支持复杂表达式）
- 异常处理（try-catch-throw）
- 模块系统（import, as）
- 运算符重载（算术、比较、逻辑）

### 1.3 Hello World

最简单的 PPX 程序：

```ppx
# 我的第一个 PiPiXia 程序

func main(): int {
    print("Hello, World!")
    return 0
}
```

**代码说明**：
- `#` 开头的是注释
- `func main(): int` 定义主函数，返回整数类型
- `print()` 用于输出文本
- `return 0` 表示程序正常结束

### 1.4 编译和运行

#### 编译源文件

```bash
# 编译 PPX 源文件
./compiler hello.ppx

# 编译成功后会生成可执行文件
# 默认输出文件名与源文件同名
```

#### 运行程序

```bash
# 运行编译后的可执行文件
./hello

# 输出：
# Hello, World!
```

#### 查看中间结果

```bash
# 查看 Token 流
./compiler hello.ppx -tokens

# 查看抽象语法树
./compiler hello.ppx -ast

# 查看 LLVM IR
./compiler hello.ppx -llvm
```

---

## 第二章：词法规则

### 2.1 注释

PPX 支持两种注释方式。

#### 2.1.1 单行注释

使用 `#` 符号开始单行注释：

```ppx
# 这是单行注释

func main(): int {
    print("Hello")  # 行尾注释
    return 0
}
```

#### 2.1.2 多行注释

使用 `/#/` ... `/#/` 包围多行注释：

```ppx
/#/
这是多行注释
可以跨越多行
用于说明复杂的功能
/#/

func calculate() {
    # 函数实现
}
```

### 2.2 标识符

#### 2.2.1 命名规则

标识符（变量名、函数名等）必须遵循以下规则：

- **开头**：字母（a-z, A-Z）或下划线（_）
- **后续**：字母、数字（0-9）或下划线
- **大小写敏感**：`name` 和 `Name` 是不同的标识符

**合法标识符示例**：
```ppx
userName
user_name
_temp
value123
calculateTotal
```

**非法标识符示例**：
```ppx
123user      # 不能以数字开头
user-name    # 不能包含连字符
user name    # 不能包含空格
```

#### 2.2.2 保留关键字

以下关键字不能用作标识符：

```
let         const       func        return      if
else        while       for         in          import
as          try         catch       throw       break
continue    switch      case        default     int
double      string      bool        char        true
false
```

### 2.3 字面量

#### 2.3.1 整数字面量

```ppx
let decimal: int = 42        # 十进制
let negative: int = -100     # 负整数
let zero: int = 0            # 零
let large: int = 1234567890  # 大整数
```

#### 2.3.2 浮点数字面量

```ppx
let pi: double = 3.14159
let negative: double = -2.71828
let scientific: double = 1.23    # 支持小数点表示
```

#### 2.3.3 字符串字面量

使用双引号包围：

```ppx
let message: string = "Hello, World!"
let chinese: string = "你好，世界！"
let empty: string = ""
let multiline: string = "Line 1\nLine 2"  # 使用转义字符
```

#### 2.3.4 插值字符串

使用 `${}` 在字符串中嵌入表达式：

```ppx
let name: string = "Alice"
let age: int = 25

let greeting: string = "Hello, ${name}!"
let info: string = "${name} is ${age} years old"
let calc: string = "2 + 3 = ${2 + 3}"  # 支持表达式
```

#### 2.3.5 字符字面量

使用单引号包围单个字符：

```ppx
let grade: char = 'A'
let symbol: char = '@'
let newline: char = '\n'  # 转义字符
let tab: char = '\t'
```

#### 2.3.6 布尔字面量

只有两个值：

```ppx
let isTrue: bool = true
let isFalse: bool = false
```

#### 2.3.7 数组字面量

使用方括号表示数组：

```ppx
let numbers: int[5] = [1, 2, 3, 4, 5]
let matrix: int[2][3] = [[1, 2, 3], [4, 5, 6]]
let empty: int[0] = []  # 空数组
```

### 2.4 转义序列

PPX 支持以下转义字符：

| 转义序列 | 含义 | 示例 |
|---------|------|------|
| `\n` | 换行符 | `"Line1\nLine2"` |
| `\t` | 制表符 | `"Name:\tValue"` |
| `\r` | 回车符 | `"CR\rLF"` |
| `\\` | 反斜杠 | `"C:\\Path"` |
| `\"` | 双引号 | `"He said \"Hi\""` |
| `\'` | 单引号 | `'It\'s'` |
| `\0` | 空字符 | `"\0"` |
| `\b` | 退格符 | `"\b"` |
| `\f` | 换页符 | `"\f"` |
| `\v` | 垂直制表符 | `"\v"` |

**示例**：
```ppx
let path: string = "C:\\Users\\Name\\Documents"
let quote: string = "He said: \"Hello!\""
let multiline: string = "First\nSecond\nThird"
let tabbed: string = "Name:\tAlice\nAge:\t25"
```

## 第三章：数据类型

PPX 是一个静态类型语言，每个变量和表达式都有明确的类型。

### 3.1 基本类型

#### 3.1.1 整数类型 (int)

```ppx
let age: int = 25
let negative: int = -100
let zero: int = 0
```

#### 3.1.2 浮点数类型 (double)

```ppx
let pi: double = 3.14159
let temperature: double = -15.5
let price: double = 99.99
```

#### 3.1.3 字符类型 (char)

```ppx
let grade: char = 'A'
let symbol: char = '@'
let newline: char = '\n'
```

#### 3.1.4 布尔类型 (bool)

```ppx
let isValid: bool = true
let hasError: bool = false
```

#### 3.1.5 字符串类型 (string)

```ppx
let name: string = "Alice"
let message: string = "你好，世界！"
let empty: string = ""
```

### 3.2 数组类型

#### 3.2.1 一维数组

数组是相同类型元素的集合，使用方括号声明大小：

```ppx
let numbers: int[5] = [10, 20, 30, 40, 50]
let scores: double[3] = [95.5, 87.0, 92.5]
```

#### 3.2.2 多维数组

支持多维数组（矩阵）：

```ppx
let matrix: int[3][3] = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]
```

#### 3.2.3 数组字面量

使用方括号表示数组字面量：

```ppx
let arr: int[3] = [1, 2, 3]
let nested: int[2][2] = [[1, 2], [3, 4]]
```

#### 3.2.4 空数组

支持声明空数组：

```ppx
let empty: int[0] = []
```

### 3.3 类型转换

#### 3.3.1 隐式类型提升

PPX 支持混合类型运算时的自动类型提升：

```ppx
let a: int = 10
let b: double = 3.5
let result: double = a + b  # int 自动提升为 double，结果为 13.5
```

#### 3.3.2 混合类型运算

在算术运算中，如果操作数类型不同，会自动将较小的类型提升为较大的类型：

```ppx
let x: int = 5
let y: double = 2.5
let sum: double = x + y      # 7.5
let product: double = x * y  # 12.5
```

---

## 第四章：变量和常量

### 变量声明 (let)

使用 `let` 声明可变变量：

```ppx
let x: int = 10
x = 20  # 可以重新赋值

let name: string = "Alice"
name = "Bob"  # 可以修改
```

### 常量声明 (const)

使用 `const` 声明不可变常量：

```ppx
const PI: double = 3.14159
# PI = 3.14  # 错误！常量不能重新赋值

const MAX_SIZE: int = 100
```

### 变量作用域

```ppx
let global: int = 100  # 全局作用域

func example() {
    let local: int = 50  # 局部作用域（仅在函数内）
    
    {
        let block: int = 25  # 块作用域（仅在块内）
        print(block)  # ✓ 可访问
    }
    
    # print(block)  # ✗ 错误！block 不可访问
}
```

---

## 运算符

### 算术运算符

```ppx
let a: int = 10
let b: int = 3

let sum: int = a + b         # 加法: 13
let diff: int = a - b        # 减法: 7
let product: int = a * b     # 乘法: 30
let quotient: double = a / b # 浮点除法: 3.333...
let intDiv: int = a // b     # 整除: 3
let remainder: int = a % b   # 取模: 1
```

### 比较运算符

```ppx
let x: int = 10
let y: int = 20

let eq: bool = x == y   # 等于: false
let ne: bool = x != y   # 不等于: true
let lt: bool = x < y    # 小于: true
let gt: bool = x > y    # 大于: false
let le: bool = x <= y   # 小于等于: true
let ge: bool = x >= y   # 大于等于: false
```

### 逻辑运算符

```ppx
let a: bool = true
let b: bool = false

let andResult: bool = a && b  # 逻辑与: false
let orResult: bool = a || b   # 逻辑或: true
let notResult: bool = !a      # 逻辑非: false
```

### 复合赋值运算符

```ppx
let x: int = 10

x += 5   # x = x + 5  → 15
x -= 3   # x = x - 3  → 12
x *= 2   # x = x * 2  → 24
x /= 4   # x = x / 4  → 6
x //= 2  # x = x // 2 → 3
x %= 2   # x = x % 2  → 1
```

### 运算符优先级

从高到低：
1. `!` (逻辑非), `-` (负号)
2. `*` `/` `//` `%` (乘除取模)
3. `+` `-` (加减)
4. `<` `>` `<=` `>=` (比较)
5. `==` `!=` (相等)
6. `&&` (逻辑与)
7. `||` (逻辑或)
8. `=` `+=` `-=` 等 (赋值)

使用括号改变优先级：

```ppx
let result1: int = 2 + 3 * 4     # 14 (先乘后加)
let result2: int = (2 + 3) * 4   # 20 (先加后乘)
```

---

## 控制流

### If 语句

```ppx
let score: int = 85

if (score >= 90) {
    print("优秀")
} else if (score >= 80) {
    print("良好")
} else if (score >= 60) {
    print("及格")
} else {
    print("不及格")
}
```

简单条件：

```ppx
let age: int = 18

if (age >= 18) {
    print("成年人")
}
```

### While 循环

```ppx
let i: int = 0

while (i < 5) {
    print(i)
    i = i + 1
}
# 输出: 0 1 2 3 4
```

### For 循环

使用范围语法 `start..end`（不包括 end）：

```ppx
# 从 0 到 4
for i in 0..5 {
    print(i)
}
# 输出: 0 1 2 3 4

# 从 1 到 10
for num in 1..11 {
    print(num)
}
```

### Break 和 Continue

#### Break - 退出循环

```ppx
for i in 0..10 {
    if (i == 5) {
        break  # 遇到 5 就退出
    }
    print(i)
}
# 输出: 0 1 2 3 4
```

#### Continue - 跳过当前迭代

```ppx
for i in 0..10 {
    if (i % 2 == 0) {
        continue  # 跳过偶数
    }
    print(i)
}
# 输出: 1 3 5 7 9 (只打印奇数)
```

### Switch 语句

```ppx
let day: int = 3

switch day {
    case 1: {
        print("星期一")
    }
    case 2: {
        print("星期二")
    }
    case 3: {
        print("星期三")
    }
    case 4: {
        print("星期四")
    }
    case 5: {
        print("星期五")
    }
    default: {
        print("周末")
    }
}
# 输出: 星期三
```

**注意：** PPX 的 switch 默认不会 fall-through，每个 case 执行完自动退出（与 C/C++ 不同）。

使用整除进行分类：

```ppx
let score: int = 85

switch (score // 10) {  # 注意：必须使用 // 整除，不能用 /
    case 10:
    case 9: {
        print("优秀")
    }
    case 8: {
        print("良好")
    }
    case 7: {
        print("中等")
    }
    case 6: {
        print("及格")
    }
    default: {
        print("不及格")
    }
}
```

---

## 函数

### 函数定义

#### 无返回值函数

```ppx
func greet() {
    print("你好！")
}

func sayHello(name: string) {
    print("Hello, ", nowrap)
    print(name)
}
```

#### 有返回值函数

```ppx
func add(a: int, b: int): int {
    return a + b
}

func multiply(x: double, y: double): double {
    return x * y
}
```

### 函数调用

```ppx
greet()                    # 调用无参数函数
sayHello("Alice")          # 调用单参数函数

let sum: int = add(10, 20)           # 调用并接收返回值
let product: double = multiply(3.5, 2.0)
```

### 函数参数

```ppx
func calculateArea(width: int, height: int): int {
    return width * height
}

func printInfo(name: string, age: int, score: double) {
    print("姓名: ", nowrap)
    print(name)
    print("年龄: ", nowrap)
    print(age)
    print("分数: ", nowrap)
    print(score)
}
```

### 递归函数

```ppx
# 计算阶乘
func factorial(n: int): int {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

let result: int = factorial(5)  # 120

# 斐波那契数列
func fibonacci(n: int): int {
    if (n <= 1) {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
```

---

## 数组

### 数组声明

```ppx
# 声明固定大小的数组
let numbers: int[5] = [10, 20, 30, 40, 50]
let scores: double[3] = [95.5, 87.0, 92.5]
let names: string[2] = ["Alice", "Bob"]
```

### 数组访问

```ppx
let arr: int[5] = [1, 2, 3, 4, 5]

let first: int = arr[0]   # 第一个元素: 1
let last: int = arr[4]    # 最后一个元素: 5

arr[2] = 100              # 修改第三个元素
```

### 数组遍历

```ppx
let numbers: int[5] = [10, 20, 30, 40, 50]

# 使用 for 循环遍历
for i in 0..5 {
    print(numbers[i])
}
```

### 多维数组

```ppx
# 二维数组（矩阵）
let matrix: int[3][3] = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]

let element: int = matrix[1][2]  # 访问第2行第3列: 6
matrix[0][0] = 10                # 修改元素
```

### 数组边界检查

PPX 会在运行时检查数组访问是否越界：

```ppx
let arr: int[5] = [1, 2, 3, 4, 5]

let x: int = arr[10]  # 运行时错误：数组越界
```

---

## 字符串

### 字符串字面量

```ppx
let message: string = "Hello, World!"
let chinese: string = "你好，世界！"
let empty: string = ""
```

### 转义字符

```ppx
let newline: string = "第一行\n第二行"
let tab: string = "姓名:\t张三"
let quote: string = "他说：\"你好！\""
let backslash: string = "路径: C:\\Users\\Name"
```

支持的转义序列：
- `\n` - 换行
- `\t` - 制表符
- `\r` - 回车
- `\\` - 反斜杠
- `\"` - 双引号
- `\'` - 单引号
- `\0` - 空字符

### 字符串插值

使用 `${}` 在字符串中插入表达式：

```ppx
let name: string = "Alice"
let age: int = 25

# 基本插值
let greeting: string = "Hello ${name}"
print(greeting)  # 输出: Hello Alice

# 表达式插值
let info: string = "${name} is ${age} years old"
print(info)  # 输出: Alice is 25 years old

# 算术表达式
let x: int = 10
let y: int = 20
let result: string = "${x} + ${y} = ${x + y}"
print(result)  # 输出: 10 + 20 = 30
```

### 字符串操作

```ppx
# 字符串访问（作为字符数组）
let str: string = "Hello"
let firstChar: char = str[0]  # 'H'

# 字符串长度（使用 strlen 函数）
# 注意：需要通过内置函数获取
```

---

## 异常处理

### Try-Catch 语句

```ppx
try {
    # 可能抛出异常的代码
    let result: int = 10 / 0  # 除零错误
} catch (e: string) {
    # 异常处理代码
    print("捕获异常: ", nowrap)
    print(e)
}
```

### Throw 语句

```ppx
func checkAge(age: int) {
    if (age < 0) {
        throw "年龄不能为负数"
    }
    if (age > 150) {
        throw "年龄不合理"
    }
    print("年龄有效")
}

try {
    checkAge(-5)
} catch (error: string) {
    print("错误: ", nowrap)
    print(error)
}
```

### 异常传播

```ppx
func divide(a: int, b: int): int {
    if (b == 0) {
        throw "除数不能为零"
    }
    return a / b
}

func calculate() {
    try {
        let result: int = divide(10, 0)
        print(result)
    } catch (e: string) {
        print("计算错误: ", nowrap)
        print(e)
    }
}
```

---

## 模块系统

### Import 语句

导入其他 PPX 文件作为模块：

```ppx
# 基本导入
import math_module

# 使用别名
import math_module as math
```

### 使用导入的模块

```ppx
import math_module as math

func main(): int {
    # 调用模块中的函数
    let result: int = math.square(5)
    print(result)  # 25
    
    let cube: int = math.cube(3)
    print(cube)    # 27
    
    return 0
}
```

### 创建模块

**math_module.ppx**:
```ppx
# 数学工具模块

func square(x: int): int {
    return x * x
}

func cube(x: int): int {
    return x * x * x
}

func abs(x: int): int {
    if (x < 0) {
        return -x
    }
    return x
}

func max(a: int, b: int): int {
    if (a > b) {
        return a
    }
    return b
}
```

**main.ppx**:
```ppx
import math_module

func main(): int {
    let a: int = math_module.square(4)
    let b: int = math_module.abs(-10)
    let c: int = math_module.max(a, b)
    
    print(c)
    return 0
}
```

---

## 最佳实践

### 1. 命名约定

```ppx
# 变量和函数：使用驼峰命名或下划线
let userName: string = "Alice"
let user_name: string = "Bob"

func calculateTotal() { }
func calculate_total() { }

# 常量：使用大写字母
const MAX_SIZE: int = 100
const PI: double = 3.14159
```

### 2. 代码格式

```ppx
# 好的格式
func add(a: int, b: int): int {
    let result: int = a + b
    return result
}

# 在运算符两侧添加空格
let sum: int = a + b
let product: int = x * y

# 在逗号后添加空格
func greet(name: string, age: int) { }
```

### 3. 注释使用

```ppx
# 为复杂逻辑添加注释
func isPrime(n: int): bool {
    # 小于 2 的数不是素数
    if (n < 2) {
        return false
    }
    
    # 检查是否能被 2 到 n-1 的数整除
    for i in 2..n {
        if (n % i == 0) {
            return false
        }
    }
    
    return true
}
```

### 4. 错误处理

```ppx
# 使用 try-catch 处理潜在错误
func safeDivide(a: int, b: int): int {
    try {
        if (b == 0) {
            throw "除数不能为零"
        }
        return a / b
    } catch (e: string) {
        print("错误: ", nowrap)
        print(e)
        return 0
    }
}
```

### 5. 避免魔法数字

```ppx
# 不好的做法
if (age >= 18) { }

# 好的做法
const ADULT_AGE: int = 18
if (age >= ADULT_AGE) { }
```

### 6. 函数分解

```ppx
# 将复杂功能分解为小函数
func calculateGrade(score: int): string {
    if (isExcellent(score)) {
        return "优秀"
    } else if (isGood(score)) {
        return "良好"
    }
    return "及格"
}

func isExcellent(score: int): bool {
    return score >= 90
}

func isGood(score: int): bool {
    return score >= 80
}
```

---

## 常见错误与解决方案

### 1. 类型不匹配

```ppx
# 错误
let x: int = 3.14  # double 不能直接赋值给 int

# 正确
let x: double = 3.14
```

### 2. Switch 类型错误

```ppx
# 错误
switch (score / 10) { }  # / 产生 double，但 case 是 int

# 正确
switch (score // 10) { }  # 使用整除 //
```

### 3. 数组越界

```ppx
let arr: int[5] = [1, 2, 3, 4, 5]

# 错误
let x: int = arr[10]  # 越界

# 正确：检查索引范围
for i in 0..5 {
    print(arr[i])
}
```

### 4. 常量修改

```ppx
# 错误
const PI: double = 3.14
PI = 3.14159  # 常量不能修改

# 正确：使用变量
let pi: double = 3.14
pi = 3.14159
```

---

## 完整示例程序

### 计算器程序

```ppx
func main(): int {
    print("===== 简单计算器 =====")
    
    let a: int = 10
    let b: int = 5
    
    print("数字 A: ", nowrap)
    print(a)
    print("数字 B: ", nowrap)
    print(b)
    print("")
    
    print("加法: ", nowrap)
    print(a + b)
    
    print("减法: ", nowrap)
    print(a - b)
    
    print("乘法: ", nowrap)
    print(a * b)
    
    print("除法: ", nowrap)
    print(a / b)
    
    return 0
}
```

### 素数判断程序

```ppx
func isPrime(n: int): bool {
    if (n < 2) {
        return false
    }
    
    for i in 2..n {
        if (i >= n) {
            break
        }
        if (n % i == 0) {
            return false
        }
    }
    
    return true
}

func main(): int {
    print("前20个素数：")
    
    let count: int = 0
    let num: int = 2
    
    while (count < 20) {
        if (isPrime(num)) {
            print(num)
            count = count + 1
        }
        num = num + 1
    }
    
    return 0
}
```

---

## 快速参考卡片

### 语法速查表

#### 变量和常量
```ppx
let x: int = 10              # 变量
const PI: double = 3.14159   # 常量
```

#### 函数定义
```ppx
func name(): int { }         # 有返回值
func name() { }              # 无返回值
func name(x: int): int { }   # 带参数
```

#### 控制流
```ppx
if (condition) { }
while (condition) { }
for i in 0..10 { }
switch value { case 1: { } }
```

#### 数组操作
```ppx
let arr: int[5] = [1, 2, 3, 4, 5]
let value: int = arr[0]
arr[0] = 100
```

#### 字符串插值
```ppx
let name: string = "Alice"
print("Hello, ${name}!")
print("2 + 3 = ${2 + 3}")
```

### 运算符速查

| 类型 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `//` `%` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` |
| 逻辑 | `&&` `\|\|` `!` |
| 赋值 | `=` `+=` `-=` `*=` `/=` `//=` `%=` |

### 类型速查

| 类型 | 示例 | 说明 |
|------|------|------|
| `int` | `42` | 整数 |
| `double` | `3.14` | 浮点数 |
| `string` | `"Hello"` | 字符串 |
| `char` | `'A'` | 字符 |
| `bool` | `true` / `false` | 布尔值 |
| `int[5]` | `[1,2,3,4,5]` | 数组 |

### 关键字速查

```
let      const    func     return   if       else
while    for      in       import   as       try
catch    throw    break    continue switch   case
default  int      double   string   bool     char
true     false
```

### 常用模式

#### 循环累加
```ppx
let sum: int = 0
for i in 0..10 {
    sum += i
}
```

#### 条件判断
```ppx
if (x > 0) {
    print("正数")
} else if (x < 0) {
    print("负数")
} else {
    print("零")
}
```

#### 异常处理
```ppx
try {
    if (b == 0) {
        throw "除数不能为零"
    }
} catch (e: string) {
    print("错误: ${e}")
}
```

---

## 下一步学习

1. **阅读示例代码**：查看 `code/` 目录中的示例
2. **查看测试用例**：参考 `test/` 目录中的测试文件
3. **学习编译器实现**：阅读 `document/词法分析.md` 和 `markdown/语法分析.md`
4. **参与贡献**：查看 `.github/CONTRIBUTING.md`

---

## 参考资源

- [README.md](../README.md) - 项目概述和快速开始
- [词法分析文档](../document/词法分析.md) - 词法分析器详解
- [语法分析文档](./document/语法分析.md) - 语法分析器详解
- [示例代码目录](../code/) - 实用代码示例
- [测试用例目录](../test/) - 完整的测试用例

---

**PiPiXia, Launch!** 🚀

*最后更新: 2025-11-26*
