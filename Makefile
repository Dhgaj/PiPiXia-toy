# PiPiXia Language Compiler Makefile
# 需要：flex, bison, g++, llvm, make

# 编译器和工具
CXX = g++
FLEX = flex
BISON = bison

# 运行00脚本后会生成.platform_config文件，然后尝试从该文件读取LLVM路径
-include .platform_config

# 如果没有配置文件，使用默认值
ifndef LLVM_CONFIG
  # 自动根据系统判断 LLVM_CONFIG
  ifeq ($(shell uname), Darwin)
    LLVM_CONFIG = /opt/homebrew/opt/llvm/bin/llvm-config
  else
    LLVM_CONFIG = /usr/bin/llvm-config
  endif
endif

# LLVM标志
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core support)

# 编译器标志
CXXFLAGS = -std=c++17 -Wall $(LLVM_CXXFLAGS)
LDFLAGS = $(LLVM_LDFLAGS)

# 额外的编译选项（可通过命令行传入）
# 例如: make EXTRA_CXXFLAGS="-fsanitize=address -g"
ifdef EXTRA_CXXFLAGS
CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_CXXFLAGS)
endif

# AddressSanitizer 快捷目标
SANITIZER_FLAGS = -fsanitize=address -g -fno-omit-frame-pointer

# 目标可执行文件
TARGET = compiler

# 源文件
LEXER_SRC = lexical.l
PARSER_SRC = syntax.y
MAIN_SRC = main.cc
CODEGEN_SRC = codegen.cc
ERROR_SRC = error.cc
HEADER = node.h codegen.h error.h

# 生成的文件
LEXER_OUT = lexical.cc
PARSER_OUT = syntax.cc
PARSER_HEADER = syntax.hh

# 目标文件
OBJS = lexical.o syntax.o main.o codegen.o error.o

# 默认目标
all: $(TARGET)

# 使用 AddressSanitizer 编译
sanitizer:
	@echo "Building with AddressSanitizer..."
	@$(MAKE) clean
	@$(MAKE) EXTRA_CXXFLAGS="$(SANITIZER_FLAGS)" all
	@echo "Build with AddressSanitizer complete!"

# 构建编译器
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "Build complete! Compiler: $(TARGET)"
	@echo "皮皮虾编译器构建成功！"

# 从flex文件生成词法分析器
$(LEXER_OUT): $(LEXER_SRC) $(PARSER_HEADER)
	@echo "Generating lexer from $(LEXER_SRC)..."
	$(FLEX) -o $(LEXER_OUT) $(LEXER_SRC)

# 从bison文件生成语法分析器
$(PARSER_OUT) $(PARSER_HEADER): $(PARSER_SRC) $(HEADER)
	@echo "Generating parser from $(PARSER_SRC)..."
	$(BISON) -d -Wno-conflicts-sr -o $(PARSER_OUT) $(PARSER_SRC)

# 编译词法分析器
lexical.o: $(LEXER_OUT) $(PARSER_HEADER) $(HEADER)
	@echo "Compiling lexer..."
	$(CXX) $(CXXFLAGS) -Wno-unused-function -c $(LEXER_OUT) -o lexical.o

# 编译语法分析器
syntax.o: $(PARSER_OUT) $(PARSER_HEADER) $(HEADER)
	@echo "Compiling parser..."
	$(CXX) $(CXXFLAGS) -c $(PARSER_OUT) -o syntax.o

# 编译主程序
main.o: $(MAIN_SRC) $(HEADER) $(PARSER_HEADER)
	@echo "Compiling main..."
	$(CXX) $(CXXFLAGS) -c $(MAIN_SRC) -o main.o

# 编译代码生成器
codegen.o: $(CODEGEN_SRC) codegen.h node.h error.h
	@echo "Compiling LLVM code generator..."
	$(CXX) $(CXXFLAGS) -c $(CODEGEN_SRC) -o codegen.o

# 编译错误处理模块
error.o: $(ERROR_SRC) error.h
	@echo "Compiling error handler..."
	$(CXX) $(CXXFLAGS) -c $(ERROR_SRC) -o error.o

# 测试 - 运行统一测试脚本
test: $(TARGET)
	@echo "运行代码测试..."
	@bash scripts/03_run_code.sh

# 运行指定的.ppx文件
run: $(TARGET)
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make run FILE=<filename.ppx>"; \
		echo "Example: make run FILE=code/main.ppx"; \
	else \
		./$(TARGET) $(FILE) -ast; \
	fi

# 清理生成的文件
clean:
	@bash scripts/04_run_clean.sh normal

# 清理所有文件包括可执行文件
distclean:
	@bash scripts/04_run_clean.sh all

# 卸载
uninstall:
	@echo "Uninstalling $(TARGET)..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstall complete!"

# 帮助
help:
	@echo "PiPiXia (皮皮虾) 语言编译器 - Makefile 帮助"
	@echo ""
	@echo "可用目标："
	@echo "  make              - 构建编译器"
	@echo "  make all          - 同 'make'"
	@echo "  make sanitizer    - 使用 AddressSanitizer 构建（内存检测）"
	@echo "  make test         - 运行所有测试文件"
	@echo "  make run FILE=... - 构建并对指定文件运行编译器"
	@echo "  make clean        - 标准清理（中间文件和输出）"
	@echo "  make distclean    - 完全清理（包括编译器和配置文件）"
	@echo "  make install      - 安装编译器到 /usr/local/bin（需要 sudo）"
	@echo "  make uninstall    - 从 /usr/local/bin 卸载编译器（需要 sudo）"
	@echo "  make info         - 显示构建配置信息"
	@echo "  make help         - 显示此帮助信息"
	@echo ""
	@echo "脚本工具："
	@echo "  ./scripts/01_platform.sh              # 检测平台并生成配置"
	@echo "  ./scripts/02_build.sh                 # 自动构建脚本"
	@echo "  ./scripts/03_code.sh                  # 运行code目录测试"
	@echo "  ./scripts/04_clean.sh [normal|all]   # 项目清理"
	@echo "  ./scripts/05_test.sh                  # 运行test目录测试"
	@echo "  ./scripts/06_ast.sh                   # 生成AST可视化报告"
	@echo "  ./scripts/08_error.sh                 # 运行错误测试"
	@echo "  ./scripts/09_gen_all.sh [-t|-a|-l|-e|-s|--tac]  # 批量生成"
	@echo "  ./scripts/10_token.sh                 # 生成Token文件"
	@echo "  ./scripts/11_ast_file.sh              # 生成AST文件"
	@echo "  ./scripts/12_llvm.sh                  # 生成LLVM IR文件"
	@echo "  ./scripts/13_exec.sh                  # 生成可执行文件"
	@echo "  ./scripts/14_symbols.sh               # 生成符号表文件"
	@echo "  ./scripts/15_tac.sh                   # 生成三地址码文件"
	@echo ""
	@echo "平台检测："
	@echo "  首次编译前建议运行: ./scripts/01_platform.sh"
	@echo "  该脚本会自动检测系统并生成 .platform_config 文件"
	@echo "  Makefile 会自动使用该配置文件中的 LLVM 路径"
	@echo ""
	@echo "示例："
	@echo "  ./scripts/01_platform.sh  # 检测平台配置"
	@echo "  make info                        # 查看配置信息"
	@echo "  make                             # 构建编译器"
	@echo "  make test                        # 运行所有测试"
	@echo "  make run FILE=code/addition.ppx  # 运行指定文件"
	@echo "  ./compiler code/addition.ppx -ast"
	@echo "  ./compiler code/import.ppx -o output/exec/import"
	@echo ""

# 伪目标
.PHONY: all test run clean distclean install uninstall help info sanitizer

# 显示构建信息
info:
	@echo "=== Build Information ==="
	@echo "Compiler: $(CXX)"
	@echo "Flex: $(FLEX)"
	@echo "Bison: $(BISON)"
	@echo "LLVM Config: $(LLVM_CONFIG)"
	@if [ -f ".platform_config" ]; then \
		echo "配置来源: .platform_config (自动检测)"; \
	else \
		echo "配置来源: Makefile默认值"; \
	fi
	@echo "Flags: $(CXXFLAGS)"
	@echo "Target: $(TARGET)"
	@echo ""
	@echo "Source files:"
	@echo "  - $(LEXER_SRC)"
	@echo "  - $(PARSER_SRC)"
	@echo "  - $(MAIN_SRC)"
	@echo "  - $(HEADER)"
	@echo ""
