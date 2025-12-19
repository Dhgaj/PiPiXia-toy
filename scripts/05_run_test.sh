#!/bin/bash

# PiPiXia test目录测试报告生成器
# 为test目录下的每个ppx文件生成单独的Markdown报告

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
TEST_DIR="${PROJECT_ROOT}/test"
REPORT_DIR="${PROJECT_ROOT}/report/test"
OUTPUT_DIR="${PROJECT_ROOT}/output"
OUTPUT_EXEC_DIR="${OUTPUT_DIR}/exec"

# 检查编译器
if [ ! -f "${COMPILER}" ]; then
    echo -e "${RED}错误: 编译器不存在，请先运行 'make' 构建编译器${NC}"
    exit 1
fi

# 创建目录
mkdir -p "${REPORT_DIR}" "${OUTPUT_EXEC_DIR}"

# 清理旧报告
echo "清理旧报告..."
find "${REPORT_DIR}" -name "*.md" -type f -delete 2>/dev/null

# 统计变量
TOTAL=0
SUCCESS=0
FAILED=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PiPiXia Test 目录测试报告生成器${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "测试目录: ${TEST_DIR}"
echo "报告目录: ${REPORT_DIR}"
echo ""

# 函数：获取预定义输入
get_test_input() {
    local test_file="$1"
    case "$test_file" in
        "27_input.ppx") echo -e "测试用户\n25\n北京" ;;
        "27_input_test.ppx") echo -e "liansifan\n18\nMeizhou" ;;
        *) echo "" ;;
    esac
}

# 查找所有ppx文件（排除库文件和模块文件）
TEST_FILES=()
while IFS= read -r -d '' file; do
    filename="$(basename "$file")"
    if [[ ! "$filename" =~ _lib\.ppx$ ]] && [[ ! "$filename" =~ _module\.ppx$ ]]; then
        TEST_FILES+=("$file")
    fi
done < <(find "$TEST_DIR" -maxdepth 1 -name "*.ppx" -type f -print0 | sort -z)

if [ ${#TEST_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}警告: test目录下没有找到ppx文件${NC}"
    exit 0
fi

for ppx_file in "${TEST_FILES[@]}"; do
    if [ ! -f "${ppx_file}" ]; then
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    basename=$(basename "${ppx_file}" .ppx)
    exec_file="${OUTPUT_EXEC_DIR}/test_${basename}"
    report_file="${REPORT_DIR}/${basename}.md"
    
    echo -e "${CYAN}[${TOTAL}] 测试文件: ${basename}.ppx${NC}"
    
    # 读取源代码
    source_code=$(cat "${ppx_file}")
    
    # 编译
    echo -e "  ${BLUE}→ 编译中...${NC}"
    raw_output=$("${COMPILER}" "${ppx_file}" -o "${exec_file}" 2>&1)
    compile_status=$?
    # 过滤ANSI颜色代码
    compile_output=$(echo "$raw_output" | sed 's/\x1b\[[0-9;]*m//g')
    
    if [ ${compile_status} -eq 0 ]; then
        echo -e "  ${GREEN}✓ 编译成功${NC}"
    else
        echo -e "  ${RED}✗ 编译失败${NC}"
    fi
    
    # 开始生成报告
    {
        echo "# ${basename}"
        echo ""
        echo "## 基本信息"
        echo ""
        echo "| 项目 | 值 |"
        echo "|------|-----|"
        echo "| **文件名** | \`${basename}.ppx\` |"
        echo "| **目录** | test |"
        echo "| **测试时间** | $(date '+%Y-%m-%d %H:%M:%S') |"
        echo "| **编译状态** | $([ ${compile_status} -eq 0 ] && echo '✅ 成功' || echo '❌ 失败') |"
        echo ""
        echo "## 源代码"
        echo ""
        echo "\`\`\`ppx"
        echo "${source_code}"
        echo "\`\`\`"
        echo ""
        echo "## 编译结果"
        echo ""
        
        if [ ${compile_status} -eq 0 ]; then
            echo "✅ **编译成功**"
            echo ""
            
            # 运行测试
            echo -e "  ${BLUE}→ 运行中...${NC}" >&2
            filename="$(basename "${ppx_file}")"
            test_input=$(get_test_input "$filename")
            
            if [[ -n "$test_input" ]]; then
                echo -e "  ${YELLOW}→ 使用预定义输入数据${NC}" >&2
                run_output=$(echo -e "$test_input" | "${exec_file}" 2>&1)
                run_status=$?
            else
                run_output=$("${exec_file}" 2>&1)
                run_status=$?
            fi
            
            if [ ${run_status} -eq 0 ]; then
                echo -e "  ${GREEN}✓ 运行成功${NC}" >&2
                # 显示输出预览
                if [ -n "${run_output}" ]; then
                    echo -e "  ${GREEN}输出预览:${NC}" >&2
                    echo "${run_output}" | head -n 5 | sed 's/^/    /' >&2
                    if [ $(echo "${run_output}" | wc -l) -gt 5 ]; then
                        echo "    ..." >&2
                    fi
                fi
            else
                echo -e "  ${RED}✗ 运行失败 (退出码: ${run_status})${NC}" >&2
            fi
            
            echo "## 运行结果"
            echo ""
            
            if [ ${run_status} -eq 0 ]; then
                echo "✅ **运行成功** (退出码: ${run_status})"
            else
                echo "❌ **运行失败** (退出码: ${run_status})"
            fi
            echo ""
            
            if [[ -n "$test_input" ]]; then
                echo "### 预定义输入"
                echo ""
                echo "\`\`\`"
                echo -e "$test_input"
                echo "\`\`\`"
                echo ""
            fi
            
            echo "### 输出内容"
            echo ""
            echo "\`\`\`"
            if [ -n "${run_output}" ]; then
                echo "${run_output}"
            else
                echo "(无输出)"
            fi
            echo "\`\`\`"
        else
            echo "❌ **编译失败**"
            echo ""
            echo "### 错误信息"
            echo ""
            echo "\`\`\`"
            echo "${compile_output}"
            echo "\`\`\`"
        fi
    } > "${report_file}"
    
    if [ ${compile_status} -eq 0 ]; then
        SUCCESS=$((SUCCESS + 1))
    else
        FAILED=$((FAILED + 1))
    fi
    echo -e "  ${BLUE}→ 报告已生成: ${basename}.md${NC}"
    echo ""
done

# 生成汇总报告
summary_file="${REPORT_DIR}/00_summary.md"
{
    echo "# Test 目录测试汇总报告"
    echo ""
    echo "生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "## 统计信息"
    echo ""
    echo "| 项目 | 数量 |"
    echo "|------|------|"
    echo "| 总计测试文件 | ${TOTAL} |"
    echo "| 编译成功 | ${SUCCESS} |"
    echo "| 编译失败 | ${FAILED} |"
    echo ""
    echo "## 测试文件列表"
    echo ""
    echo "| 序号 | 文件名 | 状态 |"
    echo "|------|--------|------|"
    
    index=1
    for ppx_file in "${TEST_FILES[@]}"; do
        if [ -f "${ppx_file}" ]; then
            basename=$(basename "${ppx_file}" .ppx)
            if "${COMPILER}" "${ppx_file}" -o /dev/null 2>/dev/null; then
                status="✅ 成功"
            else
                status="❌ 失败"
            fi
            echo "| ${index} | [\`${basename}.ppx\`](./${basename}.md) | ${status} |"
            ((index++))
        fi
    done
} > "${summary_file}"

# 生成合并报告（包含所有测试结果）
merged_file="${REPORT_DIR}/all_tests_report.md"
{
    echo "# PiPiXia Test 完整测试报告"
    echo ""
    echo "**生成时间**: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "**测试目录**: ${TEST_DIR}"
    echo "**总计**: ${TOTAL} | **成功**: ${SUCCESS} | **失败**: ${FAILED}"
    echo ""
    echo "---"
    echo ""
    
    # 合并所有单独的报告
    for report in "${REPORT_DIR}"/*.md; do
        report_name=$(basename "$report")
        # 跳过汇总文件和合并文件本身
        if [[ "$report_name" != "00_summary.md" ]] && [[ "$report_name" != "all_tests_report.md" ]]; then
            cat "$report"
            echo ""
            echo "---"
            echo ""
        fi
    done
} > "${merged_file}"

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  测试完成${NC}"
echo -e "${BLUE}========================================${NC}"
echo "总计: ${TOTAL} | 成功: ${SUCCESS} | 失败: ${FAILED}"
echo "报告目录: ${REPORT_DIR}"
echo "汇总报告: 00_summary.md"
echo -e "合并报告: ${GREEN}all_tests_report.md${NC}"
