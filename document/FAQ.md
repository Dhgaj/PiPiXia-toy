# PiPiXia 常见问题 (FAQ)

本文档收集了 PiPiXia 编译器使用过程中的常见问题和解决方案。

## 📚 目录

- [编译相关](#编译相关)
- [语法相关](#语法相关)
- [运行相关](#运行相关)
- [性能相关](#性能相关)
- [调试相关](#调试相关)

---

## 编译相关

### Q: 编译时出现 "LLVM not found" 错误？

**A**: 确保已安装 LLVM 并正确配置环境变量。

**macOS**:
```bash
# 安装 LLVM
brew install llvm

# 配置环境变量（添加到 ~/.zshrc 或 ~/.bash_profile）
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"
export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"

# 重新加载配置
source ~/.zshrc
```

**Ubuntu**:
```bash
# 安装 LLVM 21
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-21 main"
sudo apt update
sudo apt install llvm-21-dev liblld-21-dev

# 验证安装
llvm-config --version
```

---

### Q: 编译时出现 "flex: command not found"？

**A**: 需要安装 Flex 词法分析器生成工具。

**macOS**:
```bash
brew install flex
```

**Ubuntu**:
```bash
sudo apt install flex
```

**验证安装**:
```bash
flex --version
```

---

### Q: make 失败，提示找不到 bison？

**A**: 需要安装 Bison 语法分析器生成工具。

**macOS**:
```bash
# 安装 Bison
brew install bison

# 配置环境变量
export PATH="/opt/homebrew/opt/bison/bin:$PATH"
```

**Ubuntu**:
```bash
sudo apt install bison
```

**验证安装**:
```bash
bison --version
```

---

### Q: 编译器构建后运行失败？

**A**: 可能的原因和解决方法：

1. **缺少依赖库**：
   ```bash
   # 检查依赖
   ldd ./compiler  # Linux
   otool -L ./compiler  # macOS
   ```

2. **权限问题**：
   ```bash
   chmod +x ./compiler
   ```

3. **环境变量未配置**：
   ```bash
   # 运行平台检测脚本
   ./scripts/01_detect_platform.sh
   ```

---

## 语法相关

### Q: Switch 语句中如何实现 fall-through？

**A**: PPX 的 switch 默认**不支持** fall-through，每个 case 执行完会自动退出。

**正确用法**：
```ppx
switch value {
    case 1: { print("One") }
    case 2: { print("Two") }
    case 3: { print("Three") }
    default: { print("Other") }
}
```

**如果需要相同处理**，每个 case 都需要有自己的代码块：
```ppx
switch value {
    case 1: { print("Small") }
    case 2: { print("Small") }  # 需要重复代码
    case 10: { print("Big") }
}
```

---

### Q: 为什么整除要用 `//` 而不是 `/`？

**A**: PPX 区分**浮点除法**和**整除**：

- `/` 是**浮点除法**，返回 `double` 类型
- `//` 是**整除**，返回 `int` 类型

**示例**：
```ppx
let a: int = 10 / 3      # ❌ 错误！类型不匹配
let b: double = 10 / 3   # ✅ 正确，结果 3.333...
let c: int = 10 // 3     # ✅ 正确，结果 3
```

**类型转换**：
```ppx
let x: int = 10
let y: int = 3
let result1: double = x / y      # 3.333...
let result2: int = x // y        # 3
```

---

### Q: 如何使用字符串插值？

**A**: 使用 `${}` 语法在字符串中嵌入表达式。

**基本用法**：
```ppx
let name: string = "Alice"
let age: int = 25
print("${name} is ${age} years old")
# 输出: Alice is 25 years old
```

**表达式插值**：
```ppx
let x: int = 10
let y: int = 20
print("${x} + ${y} = ${x + y}")
# 输出: 10 + 20 = 30
```

**复杂表达式**：
```ppx
let price: double = 99.99
let quantity: int = 3
print("Total: $${price * quantity}")
# 输出: Total: $299.97
```

---

### Q: 数组如何初始化？

**A**: PPX 支持多种数组初始化方式。

**显式初始化**：
```ppx
let arr: int[5] = [1, 2, 3, 4, 5]
```

**默认初始化**：
```ppx
let arr: int[10]  # 所有元素初始化为 0
```

**动态大小（使用变量模拟）**：
```ppx
let size: int = 5
let arr0: int = 0
let arr1: int = 0
let arr2: int = 0
let arr3: int = 0
let arr4: int = 0
```

---

## 运行相关

### Q: 程序编译成功但运行时崩溃？

**A**: 常见原因和解决方法：

**1. 数组越界访问**：
```ppx
let arr: int[5] = [1, 2, 3, 4, 5]
print(arr[10])  # ❌ 越界！
```

**2. 除零错误**：
```ppx
let x: int = 10
let y: int = 0
let result: int = x / y  # ❌ 除零！
```

**3. 未初始化的变量**：
```ppx
let x: int
print(x)  # ❌ 未初始化！
```

**调试方法**：
```bash
# 使用 verbose 模式查看详细信息
./compiler your_program.ppx -v -o output

# 查看 LLVM IR
./compiler your_program.ppx -llvm
```

---

### Q: 如何查看编译的中间结果？

**A**: 使用编译器提供的各种输出选项。

**查看 Token 流**：
```bash
./compiler program.ppx -tokens
# 生成: program.tokens
```

**查看 AST**：
```bash
./compiler program.ppx -ast
# 生成: program.ast

# 可视化 AST
./scripts/06_visualize_ast.sh
```

**查看符号表**：
```bash
./compiler program.ppx -symbols
# 生成: program.symbols
# 显示全局变量、函数、参数、局部变量
```

**查看三地址码**：
```bash
./compiler program.ppx -tac
# 生成: program.tac
# 显示 SSA 风格的中间代码
```

**查看 LLVM IR**：
```bash
./compiler program.ppx -llvm
# 生成: program.ll
```

**查看所有中间步骤**：
```bash
./compiler program.ppx -v
# Verbose 模式，显示完整编译过程
```

---

### Q: 异常处理不工作？

**A**: 当前版本的异常处理有一些限制。

**支持的用法**：
```ppx
try {
    throw "发生错误"
} catch (e) {
    print("捕获异常: ${e}")
}
```

**限制**：
- 只支持 try 块内**直接抛出**的异常
- 函数调用中的异常传播功能待完善

**参考示例**：
```bash
# 查看完整的异常处理示例
cat code/54_try_catch.ppx
./compiler code/54_try_catch.ppx
./code/54_try_catch
```

---

### Q: PPX 有哪些内置函数？

**A**: PPX 提供以下内置函数：

| 函数 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `print(value)` | 任意类型 | int | 打印值并换行 |
| `print(value, nowrap)` | 任意类型 | int | 打印值不换行 |
| `input()` | 无 | string | 读取一行输入 |
| `input(prompt)` | string | string | 显示提示后读取输入 |
| `len(str)` | string | int | 获取字符串长度 |
| `to_int(value)` | string/double | int | 转换为整数 |
| `to_double(value)` | string/int | double | 转换为浮点数 |
| `to_string(value)` | 任意类型 | string | 转换为字符串 |
| `pow(base, exp)` | number, number | double | 幂运算 |
| `free(ptr)` | string | int | 释放动态内存 |

**示例**：
```ppx
let name: string = input("请输入姓名: ")
let age: int = to_int(input("请输入年龄: "))
let result: double = pow(2, 10)  # 1024
print("姓名长度: ${len(name)}")
```

---

### Q: 如何计算幂运算？

**A**: 使用 `pow()` 内置函数。

```ppx
# 基本用法
let result: double = pow(2, 3)    # 2³ = 8.0
let square: double = pow(5, 2)    # 5² = 25.0

# 开方运算
let sqrt2: double = pow(4, 0.5)   # √4 = 2.0

# 负指数
let half: double = pow(2, -1)     # 2⁻¹ = 0.5
```

---

### Q: 输入函数 input() 如何使用？

**A**: 使用 `input()` 函数读取用户输入。

**基本用法**：
```ppx
let name: string = input()
print("Hello, ${name}!")
```

**带提示的输入**：
```ppx
let name: string = input("请输入你的名字: ")
```

**数字输入**：
```ppx
let num: int = to_int(input("请输入一个数字: "))
let pi: double = to_double(input("请输入一个小数: "))
```

---

## 性能相关

### Q: 如何优化编译速度？

**A**: 几种提升编译速度的方法。

**1. 并行编译**：
```bash
make -j4  # 使用 4 个线程并行编译
```

**2. 增量编译**：
```bash
# 只修改部分文件时使用 make（不要 make clean）
make
```

**3. 避免重复生成**：
```bash
# 如果只修改了 main.cc，不需要重新生成词法/语法分析器
make main.o
make compiler
```

---

### Q: 生成的可执行文件太大？

**A**: 使用 strip 命令去除调试符号。

**基本用法**：
```bash
strip output/exec/your_program
```

**查看文件大小**：
```bash
# 优化前
ls -lh your_program
# 例如: -rwxr-xr-x  1 user  staff   1.2M  your_program

# 优化后
strip your_program
ls -lh your_program
# 例如: -rwxr-xr-x  1 user  staff   800K  your_program
```

---

### Q: 如何提升程序运行性能？

**A**: PPX 编译器生成的代码已经过 LLVM 优化，但你可以：

**1. 使用合适的数据类型**：
```ppx
# 使用 int 而不是 double（如果不需要小数）
let count: int = 0  # 快
let count: double = 0.0  # 慢
```

**2. 减少不必要的类型转换**：
```ppx
# 避免频繁的类型转换
let x: int = 10
let y: int = 20
let result: int = x + y  # 好

# 而不是
let result: double = x + y  # 需要类型转换
```

**3. 优化循环**：
```ppx
# 缓存数组长度
let arr: int[1000]
let len: int = 1000
for i in 0..len {
    # 使用 arr[i]
}
```

---

## 调试相关

### Q: 如何调试我的 PPX 程序？

**A**: 几种常用的调试方法。

**1. 使用 print() 语句**：
```ppx
func calculate(x: int): int {
    print("计算输入: ${x}")  # 调试输出
    let result: int = x * 2
    print("计算结果: ${result}")  # 调试输出
    return result
}
```

**2. 查看符号表**：
```bash
./compiler program.ppx -symbols
# 查看所有变量、函数定义
```

**3. 查看三地址码**：
```bash
./compiler program.ppx -tac
# 查看中间代码表示
```

**4. 查看 LLVM IR**：
```bash
./compiler program.ppx -llvm
cat program.ll  # 查看生成的中间代码
```

**5. 使用 verbose 模式**：
```bash
./compiler program.ppx -v
# 查看完整的编译过程：
# - 词法分析统计
# - AST 解析细节
# - LLVM IR 生成步骤
# - 编译链接命令
```

**4. 分段测试**：
```ppx
# 将复杂代码拆分成小函数，逐个测试
func test_part1(): int {
    # 测试第一部分
    return 0
}

func test_part2(): int {
    # 测试第二部分
    return 0
}

func main(): int {
    test_part1()
    test_part2()
    return 0
}
```

---

### Q: 语法错误信息不明确？

**A**: Bison 会给出错误行号，按以下步骤检查：

**1. 检查括号匹配**：
```ppx
# 错误示例
if (x > 0 {  # ❌ 缺少右括号
    print("正数")
}

# 正确示例
if (x > 0) {  # ✅
    print("正数")
}
```

**2. 检查语句完整性**：
```ppx
# 错误示例
let x: int  # ❌ 缺少初始值

# 正确示例
let x: int = 0  # ✅
```

**3. 检查类型声明**：
```ppx
# 错误示例
let x int = 10  # ❌ 缺少冒号

# 正确示例
let x: int = 10  # ✅
```

**4. 检查关键字拼写**：
```ppx
# 错误示例
func main() int {  # ❌ 缺少冒号

# 正确示例
func main(): int {  # ✅
    return 0
}
```

---

### Q: 如何查看编译器版本信息？

**A**: 查看编译器和依赖工具的版本。

```bash
# 查看编译器帮助信息
./compiler -h

# 查看依赖工具版本
flex --version
bison --version
llvm-config --version
g++ --version
make --version

# 一键查看所有信息
make info
```

---

## 更多帮助

如果以上 FAQ 没有解决你的问题，可以：

- 📖 查看 [故障排除.md](./故障排除.md) 文档
- 📚 阅读 [PPX语法教程.md](./PPX语法教程.md)
- 🔧 运行 `./scripts/01_detect_platform.sh` 检查环境
- 💬 在 GitHub Issues 提问
- 📧 联系维护者：sifanlian@gmail.com

---

**最后更新**: 2025-12-19
