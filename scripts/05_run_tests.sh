#!/bin/bash

# PiPiXia 测试运行脚本
# 自动检测 test 目录下的所有 .ppx 文件并编译测试
# 生成测试报告到 report 目录

echo "  PiPiXia 编译器测试套件"
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 获取项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$PROJECT_ROOT/test"
OUTPUT_DIR="$PROJECT_ROOT/output"
REPORT_DIR="$PROJECT_ROOT/report"

# 确保输出目录存在
mkdir -p "$OUTPUT_DIR/exec"
mkdir -p "$OUTPUT_DIR/ast"
mkdir -p "$OUTPUT_DIR/ast_visualized"
mkdir -p "$OUTPUT_DIR/llvm"
mkdir -p "$OUTPUT_DIR/token"
mkdir -p "$REPORT_DIR"

# 报告文件
REPORT_FILE="${REPORT_DIR}/test_report_$(date +%Y%m%d_%H%M%S).md"

# 统计
TOTAL=0
PASSED=0
FAILED=0

echo -e "${BLUE}正在扫描 test 目录...${NC}"
echo ""

# 自动检测所有 .ppx 文件（排除库文件和模块文件）
TEST_FILES=()
while IFS= read -r -d '' file; do
    filename="$(basename "$file")"
    # 跳过库文件（以 _lib.ppx 结尾的文件）和模块文件（以 _module.ppx 结尾的文件）
    if [[ ! "$filename" =~ _lib\.ppx$ ]] && [[ ! "$filename" =~ _module\.ppx$ ]]; then
        TEST_FILES+=("$filename")
    fi
done < <(find "$TEST_DIR" -maxdepth 1 -name "*.ppx" -type f -print0 | sort -z)

if [ ${#TEST_FILES[@]} -eq 0 ]; then
    echo -e "${RED}错误: 未找到任何测试文件${NC}"
    exit 1
fi

echo -e "${BLUE}找到 ${#TEST_FILES[@]} 个测试文件${NC}"
echo ""

# 初始化报告文件
echo "# PiPiXia Test 测试报告" > "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "**测试时间**: $(date '+%Y-%m-%d %H:%M:%S')" >> "${REPORT_FILE}"
echo "**测试目录**: ${TEST_DIR}" >> "${REPORT_FILE}"
echo "**测试文件数**: ${#TEST_FILES[@]}" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "---" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"

# 函数：为需要输入的测试提供预定义输入数据
get_test_input() {
    local test_file="$1"
    
    case "$test_file" in
        "27_input.ppx")
            echo -e "测试用户\n25\n北京"
            ;;
        "27_input_test.ppx")
            echo -e "liansifan\n18\nMeizhou"
            ;;
        *)
            echo ""
            ;;
    esac
}

echo "【开始测试】"
echo ""

# 运行所有测试
for test_file in "${TEST_FILES[@]}"; do
    TOTAL=$((TOTAL + 1))
    test_name="${test_file%.ppx}"
    
    echo -e "${CYAN}[$TOTAL/${#TEST_FILES[@]}] 测试文件: ${test_file}${NC}"
    echo "## 测试 ${TOTAL}: ${test_file}" >> "${REPORT_FILE}"
    echo "" >> "${REPORT_FILE}"
    
    # 编译并生成所有输出（文件名以test_开头）
    echo -e "  ${BLUE}→ 编译中...${NC}"
    
    # 1. 生成AST输出
    "$PROJECT_ROOT/compiler" "$TEST_DIR/$test_file" -ast -o "$OUTPUT_DIR/ast/test_${test_name}.ast" > /dev/null 2>&1
    # 生成 AST 可视化图（如果可用）
    if [ -f "$OUTPUT_DIR/ast/test_${test_name}.ast" ]; then
        python3 "$SCRIPT_DIR/ast_visualizer.py" "$OUTPUT_DIR/ast/test_${test_name}.ast" "$OUTPUT_DIR/ast_visualized/test_${test_name}.png" > /dev/null 2>&1 || true
    fi
    
    # 2. 生成LLVM IR输出
    "$PROJECT_ROOT/compiler" "$TEST_DIR/$test_file" -llvm -o "$OUTPUT_DIR/llvm/test_${test_name}.ll" > /dev/null 2>&1
    
    # 3. 生成Token输出
    "$PROJECT_ROOT/compiler" "$TEST_DIR/$test_file" -tokens -o "$OUTPUT_DIR/token/test_${test_name}.tokens" > /dev/null 2>&1
    
    # 4. 编译生成可执行文件
    compile_output=$("$PROJECT_ROOT/compiler" "$TEST_DIR/$test_file" -o "$OUTPUT_DIR/exec/test_${test_name}" 2>&1)
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
            PASSED=$((PASSED + 1))
        else
            # 运行可执行文件
            echo -e "  ${BLUE}→ 运行中...${NC}"
            
            # 检查是否需要提供输入数据
            test_input=$(get_test_input "$test_file")
            if [[ -n "$test_input" ]]; then
                echo -e "  ${YELLOW}→ 使用预定义输入数据${NC}"
                run_output=$(echo -e "$test_input" | "$OUTPUT_DIR/exec/test_${test_name}" 2>&1)
                run_status=$?
            else
                run_output=$("$OUTPUT_DIR/exec/test_${test_name}" 2>&1)
                run_status=$?
            fi
            
            if [ ${run_status} -eq 0 ]; then
                echo -e "  ${GREEN}✓ 运行成功${NC}"
                echo "### 运行结果" >> "${REPORT_FILE}"
                echo "✅ **运行成功**" >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                
                # 如果使用了输入数据，记录到报告中
                if [[ -n "$test_input" ]]; then
                    echo "**预定义输入**:" >> "${REPORT_FILE}"
                    echo '```' >> "${REPORT_FILE}"
                    echo -e "$test_input" >> "${REPORT_FILE}"
                    echo '```' >> "${REPORT_FILE}"
                    echo "" >> "${REPORT_FILE}"
                fi
                
                echo "**输出内容**:" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                # 输出完整内容
                echo "${run_output}" >> "${REPORT_FILE}"
                echo '```' >> "${REPORT_FILE}"
                echo "" >> "${REPORT_FILE}"
                
                PASSED=$((PASSED + 1))
                
                # 显示输出预览
                echo -e "  ${BLUE}输出预览:${NC}"
                echo "${run_output}" | head -n 10 | while IFS= read -r line; do
                    echo "    $line"
                done
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
                
                FAILED=$((FAILED + 1))
            fi
        fi
        
        # 显示文件大小
        if [ -f "$OUTPUT_DIR/exec/test_${test_name}" ]; then
            file_size=$(ls -lh "$OUTPUT_DIR/exec/test_${test_name}" | awk '{print $5}')
            echo -e "  ${BLUE}→ 可执行文件大小: ${file_size}${NC}"
        fi
        
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
        
        FAILED=$((FAILED + 1))
    fi
    
    echo "----------*****----------"
    echo "---" >> "${REPORT_FILE}"
    echo "" >> "${REPORT_FILE}"
done

# 生成测试总结
echo "## 测试总结" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"
echo "| 项目 | 数量 |" >> "${REPORT_FILE}"
echo "|------|------|" >> "${REPORT_FILE}"
echo "| 总测试数 | ${TOTAL} |" >> "${REPORT_FILE}"
echo "| ✅ 通过 | ${PASSED} |" >> "${REPORT_FILE}"
echo "| ❌ 失败 | ${FAILED} |" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"

if [ ${FAILED} -eq 0 ]; then
    echo "**结论**: 🎉 所有测试通过！" >> "${REPORT_FILE}"
else
    echo "**结论**: ⚠️  有 ${FAILED} 个测试失败" >> "${REPORT_FILE}"
fi

echo ""
echo "-----------"
echo "  测试完成"
echo "-----------"
echo ""
echo "总测试数: $TOTAL"
echo -e "成功: ${GREEN}$PASSED${NC}"
echo -e "失败: ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}🎉 所有测试通过！${NC}"
else
    echo -e "${YELLOW}⚠️  有 $FAILED 个测试失败${NC}"
fi

echo ""
echo -e "详细报告已保存到: ${BLUE}${REPORT_FILE}${NC}"
echo -e "输出文件位于:"
echo -e "  - ${CYAN}${OUTPUT_DIR}/exec${NC} (可执行文件)"
echo -e "  - ${CYAN}${OUTPUT_DIR}/token${NC} (Token 流)"
echo -e "  - ${CYAN}${OUTPUT_DIR}/ast${NC} (抽象语法树)"
echo -e "  - ${CYAN}${OUTPUT_DIR}/ast_visualized${NC} (AST Visualized)"
echo -e "  - ${CYAN}${OUTPUT_DIR}/llvm${NC} (LLVM IR)"
echo ""

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
