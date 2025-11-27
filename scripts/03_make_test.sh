#!/bin/bash

# PiPiXia 代码测试脚本
# 统一测试 code 目录下的所有测试文件

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
COMPILER="${PROJECT_ROOT}/compiler"
CODE_DIR="${PROJECT_ROOT}/code"
REPORT_DIR="${PROJECT_ROOT}/report"
OUTPUT_DIR="${PROJECT_ROOT}/output"
OUTPUT_EXEC_DIR="${OUTPUT_DIR}/exec"
OUTPUT_AST_DIR="${OUTPUT_DIR}/ast"
OUTPUT_LLVM_DIR="${OUTPUT_DIR}/llvm"
OUTPUT_TOKEN_DIR="${OUTPUT_DIR}/token"
REPORT_FILE="${REPORT_DIR}/code_test_report_$(date +%Y%m%d_%H%M%S).md"

# 统计变量
TOTAL_FILES=0
SUCCESS_FILES=0
FAIL_FILES=0

# 创建输出目录
mkdir -p "${REPORT_DIR}"
mkdir -p "${OUTPUT_EXEC_DIR}" "${OUTPUT_AST_DIR}" "${OUTPUT_LLVM_DIR}" "${OUTPUT_TOKEN_DIR}"
# 检查编译器是否存在
if [ ! -f "${COMPILER}" ]; then
    echo -e "${RED}错误: 编译器不存在，请先运行 'make' 构建编译器${NC}"
    exit 1
fi

# 初始化报告文件
echo "# PiPiXia 代码测试报告" > "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "**测试时间**: $(date '+%Y-%m-%d %H:%M:%S')" >> "${REPORT_FILE}"
echo "**测试目录**: ${CODE_DIR}" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "---" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"

echo -e "${BLUE}------------------------${NC}"
echo -e "${BLUE}  PiPiXia 代码测试${NC}"
echo -e "${BLUE}------------------------${NC}"
echo ""

# 查找所有 .ppx 文件（排除 math_lib.ppx 和 hello_world.ppx）
TEST_FILES=($(find "${CODE_DIR}" -maxdepth 1 -name "*.ppx" -type f ! -name "math_lib.ppx" ! -name "hello_world.ppx" | sort))

# 遍历测试文件
for test_file in "${TEST_FILES[@]}"; do
    ppx_file="${test_file}"
    
    # 跳过不存在的文件
    if [ ! -f "${ppx_file}" ]; then
        echo -e "${YELLOW}跳过: ${test_file} (文件不存在)${NC}"
        continue
    fi
    
    # 获取文件名（不含扩展名和路径）
    basename=$(basename "${test_file}" .ppx)
    # 可执行文件路径
    exec_file="${OUTPUT_EXEC_DIR}/${basename}"
    
    TOTAL_FILES=$((TOTAL_FILES + 1))
    
    echo -e "${CYAN}[${TOTAL_FILES}] 测试文件: ${test_file}${NC}"
    echo "## 测试 ${TOTAL_FILES}: ${test_file}" >> "${REPORT_FILE}"
    echo "" >> "${REPORT_FILE}"
    
    # 编译ppx文件
    echo -e "  ${BLUE}→ 编译中...${NC}"
    compile_output=$("${COMPILER}" "${ppx_file}" -o "${exec_file}" 2>&1)
    compile_status=$?
    
    if [ ${compile_status} -eq 0 ]; then
        echo -e "  ${GREEN}✓ 编译成功${NC}"
        echo "### 编译结果" >> "${REPORT_FILE}"
        echo "✅ **编译成功**" >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
        
        # 对于库文件（如 math_lib.ppx），只编译不运行
        if [[ "${test_file}" == *"_lib.ppx" ]]; then
            echo -e "  ${YELLOW}→ 库文件，跳过运行${NC}"
            echo "### 运行结果" >> "${REPORT_FILE}"
            echo "⏭️  **库文件，仅编译**" >> "${REPORT_FILE}"
            echo "" >> "${REPORT_FILE}"
            SUCCESS_FILES=$((SUCCESS_FILES + 1))
        else
            # 运行可执行文件
            echo -e "  ${BLUE}→ 运行中...${NC}"
            run_output=$("${exec_file}" 2>&1)
            run_status=$?
            
            if [ ${run_status} -eq 0 ]; then
                echo -e "  ${GREEN}✓ 运行成功${NC}"
                echo "### 运行结果" >> "${REPORT_FILE}"
                echo "✅ **运行成功**" >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                echo "**输出内容**:" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                echo "${run_output}" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                
                SUCCESS_FILES=$((SUCCESS_FILES + 1))
                
                # 显示部分输出（前10行）
                echo -e "  ${GREEN}输出预览:${NC}"
                echo "${run_output}" | head -n 10 | sed 's/^/    /'
                if [ $(echo "${run_output}" | wc -l) -gt 10 ]; then
                    echo "    ..."
                fi
            else
                echo -e "  ${RED}✗ 运行失败 (退出码: ${run_status})${NC}"
                echo "### 运行结果" >> "${REPORT_FILE}"
                echo "❌ **运行失败** (退出码: ${run_status})" >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                echo "**错误信息**:" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                echo "${run_output}" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                
                FAIL_FILES=$((FAIL_FILES + 1))
            fi
        fi
        
        # 生成 Token 流、AST 和 LLVM IR
        token_file="${OUTPUT_TOKEN_DIR}/${basename}.tokens"
        ast_file="${OUTPUT_AST_DIR}/${basename}.ast"
        llvm_file="${OUTPUT_LLVM_DIR}/${basename}.ll"
        "${COMPILER}" "${ppx_file}" -tokens -o "${token_file}" >/dev/null 2>&1
        "${COMPILER}" "${ppx_file}" -ast -o "${ast_file}" >/dev/null 2>&1
        "${COMPILER}" "${ppx_file}" -emit-llvm -o "${llvm_file}" >/dev/null 2>&1
        
    else
        echo -e "  ${RED}✗ 编译失败${NC}"
        echo "### 编译结果" >> "${REPORT_FILE}"
        echo "❌ **编译失败**" >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
        echo "**错误信息**:" >> "${REPORT_FILE}"
        echo '```' >> "${REPORT_FILE}"
        echo "${compile_output}" >> "${REPORT_FILE}"
        echo '```' >> "${REPORT_FILE}"
        echo "" >> "${REPORT_FILE}"
        
        FAIL_FILES=$((FAIL_FILES + 1))
    fi
    
    echo ""
    echo "---" >> "${REPORT_FILE}"
    echo "" >> "${REPORT_FILE}"
done

# 生成测试总结
echo -e "${BLUE}------------${NC}"
echo -e "${BLUE}  测试完成${NC}"
echo -e "${BLUE}------------${NC}"
echo ""
echo -e "总文件数: ${TOTAL_FILES}"
echo -e "${GREEN}成功: ${SUCCESS_FILES}${NC}"
echo -e "${RED}失败: ${FAIL_FILES}${NC}"
echo ""

# 写入报告总结
echo "## 测试总结" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "| 项目 | 数量 |" >> "${REPORT_FILE}"
echo "|------|------|" >> "${REPORT_FILE}"
echo "| 总文件数 | ${TOTAL_FILES} |" >> "${REPORT_FILE}"
echo "| ✅ 成功 | ${SUCCESS_FILES} |" >> "${REPORT_FILE}"
echo "| ❌ 失败 | ${FAIL_FILES} |" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"

if [ ${FAIL_FILES} -eq 0 ]; then
    echo "**结论**: 🎉 所有测试通过！" >> "${REPORT_FILE}"
    echo -e "${GREEN}🎉 所有测试通过！${NC}"
else
    echo "**结论**: ⚠️  有 ${FAIL_FILES} 个文件测试失败" >> "${REPORT_FILE}"
    echo -e "${YELLOW}⚠️  有 ${FAIL_FILES} 个文件测试失败${NC}"
fi

echo ""
echo -e "详细报告已保存到: ${BLUE}${REPORT_FILE}${NC}"
echo -e "输出文件位于:"
echo -e "  - ${CYAN}${OUTPUT_EXEC_DIR}${NC} (可执行文件)"
echo -e "  - ${CYAN}${OUTPUT_TOKEN_DIR}${NC} (Token 流)"
echo -e "  - ${CYAN}${OUTPUT_AST_DIR}${NC} (抽象语法树)"
echo -e "  - ${CYAN}${OUTPUT_LLVM_DIR}${NC} (LLVM IR)"
echo ""

# 返回状态码
if [ ${FAIL_FILES} -eq 0 ]; then
    exit 0
else
    exit 1
fi
