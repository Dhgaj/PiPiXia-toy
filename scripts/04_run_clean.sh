#!/bin/bash

# PiPiXia 项目清理脚本
# 清理编译生成的文件和临时文件

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

# 切换到项目根目录
cd "${PROJECT_ROOT}"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PiPiXia 项目清理${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 清理选项
CLEAN_TYPE="${1:-normal}"  # normal, all, deep

case "${CLEAN_TYPE}" in
    "normal"|"")
        echo -e "${CYAN}清理模式: 标准清理${NC}"
        echo "清理编译中间文件和输出文件..."
        echo ""
        
        # 清理目标文件
        if [ -f "lexical.o" ] || [ -f "syntax.o" ] || [ -f "main.o" ] || [ -f "codegen.o" ]; then
            echo -e "  ${YELLOW}→ 清理目标文件 (.o)${NC}"
            rm -f lexical.o syntax.o main.o codegen.o
        fi
        
        # 清理生成的源文件
        if [ -f "lexical.cc" ] || [ -f "syntax.cc" ] || [ -f "syntax.hh" ]; then
            echo -e "  ${YELLOW}→ 清理生成的源文件 (lexical.cc, syntax.cc, syntax.hh)${NC}"
            rm -f lexical.cc syntax.cc syntax.hh
        fi
        
        # 清理 Bison 生成的辅助文件
        if [ -f "syntax.output" ] || [ -f "location.hh" ] || [ -f "position.hh" ] || [ -f "stack.hh" ]; then
            echo -e "  ${YELLOW}→ 清理 Bison 辅助文件${NC}"
            rm -f syntax.output location.hh position.hh stack.hh
        fi
        
        # 清理输出目录
        if [ -d "output/ast" ] && [ "$(ls -A output/ast 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 AST 文件${NC}"
            rm -f output/ast/*.ast
        fi
        
        if [ -d "output/llvm" ] && [ "$(ls -A output/llvm 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 LLVM IR 文件${NC}"
            rm -f output/llvm/*.ll
        fi
        
        if [ -d "output/token" ] && [ "$(ls -A output/token 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 Token 文件${NC}"
            rm -f output/token/*.tokens
        fi
        
        if [ -d "output/exec" ] && [ "$(ls -A output/exec 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理可执行文件${NC}"
            rm -f output/exec/*
        fi
        
        # 清理测试报告（各子目录）
        if [ -d "report/code" ] && [ "$(ls -A report/code 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 code 测试报告${NC}"
            find report/code -name "*.md" -type f -exec trash {} \; 2>/dev/null
        fi
        
        if [ -d "report/test" ] && [ "$(ls -A report/test 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 test 测试报告${NC}"
            find report/test -name "*.md" -type f -exec trash {} \; 2>/dev/null
        fi
        
        if [ -d "report/ast" ] && [ "$(ls -A report/ast 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 AST 报告${NC}"
            find report/ast -name "*.md" -type f -exec trash {} \; 2>/dev/null
        fi
        
        if [ -d "report/error" ] && [ "$(ls -A report/error 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理 error 测试报告${NC}"
            find report/error -name "*.md" -type f -exec trash {} \; 2>/dev/null
        fi
        
        # 清理根目录下的旧报告
        if [ "$(find report -maxdepth 1 -name "*.md" -type f 2>/dev/null)" ]; then
            echo -e "  ${YELLOW}→ 清理根目录报告${NC}"
            find report -maxdepth 1 -name "*.md" -type f -exec trash {} \; 2>/dev/null
        fi
        
        # 清理平台配置文件
        if [ -f ".platform_config" ]; then
            echo -e "  ${YELLOW}→ 清理平台配置文件${NC}"
            rm -f .platform_config
        fi

        echo ""
        echo -e "${GREEN}✓ 标准清理完成${NC}"
        ;;
        
    "all"|"distclean")
        echo -e "${CYAN}清理模式: 完全清理 (包括编译器可执行文件)${NC}"
        echo "清理所有生成的文件..."
        echo ""
        
        # 执行标准清理
        "${0}" normal
        
        # 清理编译器可执行文件
        if [ -f "compiler" ]; then
            echo -e "  ${RED}→ 删除编译器可执行文件${NC}"
            rm -f compiler
        fi
        
        # 清理平台配置文件
        if [ -f ".platform_config" ]; then
            echo -e "  ${YELLOW}→ 删除平台配置文件 (.platform_config)${NC}"
            rm -f .platform_config
        fi
        
        echo ""
        echo -e "${GREEN}✓ 完全清理完成${NC}"
        ;;
        
    "deep")
        echo -e "${CYAN}清理模式: 深度清理 (包括临时文件和缓存)${NC}"
        echo "执行深度清理..."
        echo ""
        
        # 执行完全清理
        "${0}" all
        
        # 清理其他临时文件
        echo -e "  ${YELLOW}→ 清理临时文件${NC}"
        
        # 清理 macOS 的 .DS_Store 文件
        find . -name ".DS_Store" -type f -delete 2>/dev/null
        
        # 清理编辑器临时文件
        find . -name "*~" -type f -delete 2>/dev/null
        find . -name "*.swp" -type f -delete 2>/dev/null
        find . -name "*.swo" -type f -delete 2>/dev/null
        
        # 清理 Python 缓存（如果有）
        find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null
        find . -name "*.pyc" -type f -delete 2>/dev/null
        
        # 清理空目录
        find output -type d -empty -delete 2>/dev/null
        
        echo ""
        echo -e "${GREEN}✓ 深度清理完成${NC}"
        ;;
        
    "help"|"-h"|"--help")
        echo "用法: $0 [选项]"
        echo ""
        echo "清理选项:"
        echo "  normal      标准清理（默认）- 清理编译中间文件和输出文件"
        echo "  all         完全清理 - 包括编译器可执行文件和配置文件"
        echo "  distclean   同 'all'"
        echo "  deep        深度清理 - 包括临时文件和缓存"
        echo "  help        显示此帮助信息"
        echo ""
        echo "示例:"
        echo "  $0              # 标准清理"
        echo "  $0 normal       # 标准清理"
        echo "  $0 all          # 完全清理"
        echo "  $0 deep         # 深度清理"
        exit 0
        ;;
        
    *)
        echo -e "${RED}错误: 未知的清理选项 '${CLEAN_TYPE}'${NC}"
        echo "使用 '$0 help' 查看可用选项"
        exit 1
        ;;
esac

# 显示清理统计
echo ""
echo -e "${BLUE}-----------${NC}"
echo -e "${BLUE}  清理统计${NC}"
echo -e "${BLUE}-----------${NC}"

# 检查目录状态
echo ""
echo "目录状态:"

check_dir_empty() {
    local dir=$1
    local desc=$2
    if [ -d "$dir" ]; then
        local count=$(ls -A "$dir" 2>/dev/null | wc -l | tr -d ' ')
        if [ "$count" -eq 0 ]; then
            echo -e "  ${GREEN}✓${NC} $desc: 空"
        else
            echo -e "  ${YELLOW}→${NC} $desc: $count 个文件"
        fi
    else
        echo -e "  ${CYAN}·${NC} $desc: 目录不存在"
    fi
}

check_dir_empty "output/ast" "AST 输出"
check_dir_empty "output/llvm" "LLVM IR 输出"
check_dir_empty "output/token" "Token 输出"
check_dir_empty "output/exec" "可执行文件"
check_dir_empty "report/code" "Code 报告"
check_dir_empty "report/test" "Test 报告"
check_dir_empty "report/ast" "AST 报告"
check_dir_empty "report/error" "Error 报告"

# 检查编译器状态
echo ""
if [ -f "compiler" ]; then
    echo -e "  ${CYAN}→${NC} 编译器: 存在"
else
    echo -e "  ${GREEN}✓${NC} 编译器: 已清理"
fi

echo ""
echo -e "${GREEN}清理任务完成！${NC}"
echo ""
