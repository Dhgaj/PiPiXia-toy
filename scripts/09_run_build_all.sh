#!/bin/bash

# PiPiXia 批量生成脚本
# 为code目录下的每个ppx文件生成token, ast, exec, llvm等文件

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 项目路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
COMPILER="${PROJECT_ROOT}/compiler"
CODE_DIR="${PROJECT_ROOT}/code"

# 输出目录
OUTPUT_DIR="${PROJECT_ROOT}/output"
OUTPUT_TOKEN="${OUTPUT_DIR}/token"
OUTPUT_AST="${OUTPUT_DIR}/ast"
OUTPUT_LLVM="${OUTPUT_DIR}/llvm"
OUTPUT_EXEC="${OUTPUT_DIR}/exec"
OUTPUT_SYMBOLS="${OUTPUT_DIR}/symbols"
OUTPUT_TAC="${OUTPUT_DIR}/tac"
OUTPUT_AST_VIS="${OUTPUT_DIR}/ast_visualized"

# 检查编译器
if [ ! -f "${COMPILER}" ]; then
    echo -e "${RED}错误: 编译器不存在，请先运行 'make' 构建编译器${NC}"
    exit 1
fi

# 创建输出目录
mkdir -p "${OUTPUT_TOKEN}" "${OUTPUT_AST}" "${OUTPUT_LLVM}" "${OUTPUT_EXEC}" \
         "${OUTPUT_SYMBOLS}" "${OUTPUT_TAC}" "${OUTPUT_AST_VIS}"

# 解析参数
GENERATE_TOKEN=false
GENERATE_AST=false
GENERATE_LLVM=false
GENERATE_EXEC=false
GENERATE_SYMBOLS=false
GENERATE_TAC=false
GENERATE_ALL=false
TARGET_FILE=""

print_usage() {
    echo "用法: $0 [选项] [文件]"
    echo ""
    echo "选项:"
    echo "  -t, --token     生成Token文件"
    echo "  -a, --ast       生成AST文件和可视化"
    echo "  -l, --llvm      生成LLVM IR文件"
    echo "  -e, --exec      生成可执行文件"
    echo "  -s, --symbols   生成符号表文件"
    echo "  --tac           生成三地址码文件"
    echo "  -A, --all       生成所有类型文件（默认）"
    echo "  -h, --help      显示帮助"
    echo ""
    echo "示例:"
    echo "  $0              # 为code目录下所有文件生成所有类型"
    echo "  $0 -t           # 只生成Token文件"
    echo "  $0 -a -l        # 生成AST和LLVM IR文件"
    echo "  $0 file.ppx     # 只处理指定文件"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--token)
            GENERATE_TOKEN=true
            shift
            ;;
        -a|--ast)
            GENERATE_AST=true
            shift
            ;;
        -l|--llvm)
            GENERATE_LLVM=true
            shift
            ;;
        -e|--exec)
            GENERATE_EXEC=true
            shift
            ;;
        -s|--symbols)
            GENERATE_SYMBOLS=true
            shift
            ;;
        --tac)
            GENERATE_TAC=true
            shift
            ;;
        -A|--all)
            GENERATE_ALL=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *.ppx)
            TARGET_FILE="$1"
            shift
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

# 默认生成所有类型
if ! $GENERATE_TOKEN && ! $GENERATE_AST && ! $GENERATE_LLVM && ! $GENERATE_EXEC && ! $GENERATE_SYMBOLS && ! $GENERATE_TAC; then
    GENERATE_ALL=true
fi

if $GENERATE_ALL; then
    GENERATE_TOKEN=true
    GENERATE_AST=true
    GENERATE_LLVM=true
    GENERATE_EXEC=true
    GENERATE_SYMBOLS=true
    GENERATE_TAC=true
fi

# 统计变量
TOTAL=0
SUCCESS=0
FAILED=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PiPiXia 批量生成工具${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "代码目录: ${CODE_DIR}"
echo "输出目录: ${OUTPUT_DIR}"
echo ""
echo "生成类型:"
$GENERATE_TOKEN && echo "  - Token文件"
$GENERATE_AST && echo "  - AST文件"
$GENERATE_LLVM && echo "  - LLVM IR文件"
$GENERATE_EXEC && echo "  - 可执行文件"
$GENERATE_SYMBOLS && echo "  - 符号表文件"
$GENERATE_TAC && echo "  - 三地址码文件"
echo ""

# 确定要处理的文件
if [ -n "${TARGET_FILE}" ]; then
    if [ -f "${TARGET_FILE}" ]; then
        PPX_FILES=("${TARGET_FILE}")
    elif [ -f "${CODE_DIR}/${TARGET_FILE}" ]; then
        PPX_FILES=("${CODE_DIR}/${TARGET_FILE}")
    else
        echo -e "${RED}错误: 找不到文件 ${TARGET_FILE}${NC}"
        exit 1
    fi
else
    PPX_FILES=($(find "${CODE_DIR}" -maxdepth 1 -name "*.ppx" -type f ! -name "*_lib.ppx" | sort))
fi

# 处理每个文件
for ppx_file in "${PPX_FILES[@]}"; do
    if [ ! -f "${ppx_file}" ]; then
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    basename=$(basename "${ppx_file}" .ppx)
    has_error=false
    
    echo -e "${CYAN}[${TOTAL}] 处理: ${basename}.ppx${NC}"
    
    # 生成Token文件
    if $GENERATE_TOKEN; then
        token_file="${OUTPUT_TOKEN}/${basename}.tokens"
        if "${COMPILER}" "${ppx_file}" -tokens -o "${token_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} Token: ${basename}.tokens"
        else
            echo -e "  ${YELLOW}⚠${NC} Token生成失败"
            has_error=true
        fi
    fi
    
    # 生成AST文件
    if $GENERATE_AST; then
        ast_file="${OUTPUT_AST}/${basename}.ast"
        if "${COMPILER}" "${ppx_file}" -ast -o "${ast_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} AST: ${basename}.ast"
            
            # 生成AST可视化
            if [ -f "${SCRIPT_DIR}/ast_visualizer.py" ] && command -v python3 &>/dev/null; then
                png_file="${OUTPUT_AST_VIS}/${basename}.png"
                if python3 "${SCRIPT_DIR}/ast_visualizer.py" "${ast_file}" "${png_file}" 2>/dev/null; then
                    echo -e "  ${GREEN}✓${NC} AST可视化: ${basename}.png"
                fi
            fi
        else
            echo -e "  ${YELLOW}⚠${NC} AST生成失败"
            has_error=true
        fi
    fi
    
    # 生成LLVM IR文件
    if $GENERATE_LLVM; then
        llvm_file="${OUTPUT_LLVM}/${basename}.ll"
        if "${COMPILER}" "${ppx_file}" -llvm -o "${llvm_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} LLVM: ${basename}.ll"
        else
            echo -e "  ${YELLOW}⚠${NC} LLVM IR生成失败"
            has_error=true
        fi
    fi
    
    # 生成可执行文件
    if $GENERATE_EXEC; then
        exec_file="${OUTPUT_EXEC}/${basename}"
        if "${COMPILER}" "${ppx_file}" -o "${exec_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} Exec: ${basename}"
        else
            echo -e "  ${YELLOW}⚠${NC} 可执行文件生成失败"
            has_error=true
        fi
    fi
    
    # 生成符号表文件
    if $GENERATE_SYMBOLS; then
        symbols_file="${OUTPUT_SYMBOLS}/${basename}.symbols"
        if "${COMPILER}" "${ppx_file}" -symbols -o "${symbols_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} Symbols: ${basename}.symbols"
        else
            echo -e "  ${YELLOW}⚠${NC} 符号表生成失败"
            has_error=true
        fi
    fi
    
    # 生成三地址码文件
    if $GENERATE_TAC; then
        tac_file="${OUTPUT_TAC}/${basename}.tac"
        if "${COMPILER}" "${ppx_file}" -tac -o "${tac_file}" 2>/dev/null; then
            echo -e "  ${GREEN}✓${NC} TAC: ${basename}.tac"
        else
            echo -e "  ${YELLOW}⚠${NC} 三地址码生成失败"
            has_error=true
        fi
    fi
    
    if $has_error; then
        FAILED=$((FAILED + 1))
    else
        SUCCESS=$((SUCCESS + 1))
    fi
done

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  处理完成${NC}"
echo -e "${BLUE}========================================${NC}"
echo "总计: ${TOTAL} | 成功: ${SUCCESS} | 失败: ${FAILED}"
echo ""
echo "输出目录:"
echo "  Token:   ${OUTPUT_TOKEN}"
echo "  AST:     ${OUTPUT_AST}"
echo "  LLVM:    ${OUTPUT_LLVM}"
echo "  Exec:    ${OUTPUT_EXEC}"
echo "  Symbols: ${OUTPUT_SYMBOLS}"
echo "  TAC:     ${OUTPUT_TAC}"
echo "  可视化:  ${OUTPUT_AST_VIS}"
