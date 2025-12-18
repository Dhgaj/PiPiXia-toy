#!/bin/bash

# PiPiXia AST可视化报告生成器
# 为code目录下的每个ppx文件生成AST可视化的Markdown报告

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
VISUALIZER="${SCRIPT_DIR}/ast_visualizer.py"
CODE_DIR="${PROJECT_ROOT}/code"
REPORT_DIR="${PROJECT_ROOT}/report/ast"
OUTPUT_DIR="${PROJECT_ROOT}/output"
OUTPUT_AST_DIR="${OUTPUT_DIR}/ast"
OUTPUT_AST_VIS_DIR="${OUTPUT_DIR}/ast_visualized"

# 检查依赖
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}错误: 未找到 python3${NC}"
    exit 1
fi

if ! command -v dot &> /dev/null || ! python3 -c "import graphviz" 2>/dev/null; then
    echo -e "${YELLOW}警告: 缺少graphviz依赖，AST可视化图片将无法生成${NC}"
    echo "请安装: pip3 install graphviz"
    HAS_GRAPHVIZ=false
else
    HAS_GRAPHVIZ=true
fi

# 检查编译器
if [ ! -f "${COMPILER}" ]; then
    echo -e "${RED}错误: 编译器不存在，请先运行 'make' 构建编译器${NC}"
    exit 1
fi

# 创建目录
mkdir -p "${REPORT_DIR}" "${OUTPUT_AST_DIR}" "${OUTPUT_AST_VIS_DIR}"

# 清理旧报告
echo "清理旧报告..."
find "${REPORT_DIR}" -name "*.md" -type f -delete 2>/dev/null

# 统计变量
TOTAL=0
SUCCESS=0
FAILED=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PiPiXia AST 可视化报告生成器${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "代码目录: ${CODE_DIR}"
echo "报告目录: ${REPORT_DIR}"
echo ""

# 查找所有ppx文件
ppx_files=("${CODE_DIR}"/*.ppx)

if [ ! -e "${ppx_files[0]}" ]; then
    echo -e "${YELLOW}警告: code目录下没有找到ppx文件${NC}"
    exit 0
fi

for ppx_file in "${ppx_files[@]}"; do
    if [ ! -f "${ppx_file}" ]; then
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    basename=$(basename "${ppx_file}" .ppx)
    ast_file="${OUTPUT_AST_DIR}/${basename}.ast"
    png_file="${OUTPUT_AST_VIS_DIR}/${basename}.png"
    report_file="${REPORT_DIR}/${basename}.md"
    
    echo -e "${CYAN}[${TOTAL}] 处理: ${basename}.ppx${NC}"
    
    # 读取源代码
    source_code=$(cat "${ppx_file}")
    
    # 生成AST
    ast_output=$("${COMPILER}" "${ppx_file}" -ast -o "${ast_file}" 2>&1)
    ast_status=$?
    
    # 读取AST内容
    if [ -f "${ast_file}" ]; then
        ast_content=$(cat "${ast_file}")
    else
        ast_content="(AST生成失败)"
    fi
    
    # 生成可视化图片
    vis_status=1
    vis_message=""
    if [ "${HAS_GRAPHVIZ}" = true ] && [ -f "${ast_file}" ]; then
        vis_output=$(python3 "${VISUALIZER}" "${ast_file}" "${png_file}" 2>&1)
        vis_status=$?
        if [ ${vis_status} -ne 0 ]; then
            vis_message="${vis_output}"
        fi
    fi
    
    # 生成报告
    {
        echo "# ${basename} AST 分析报告"
        echo ""
        echo "## 基本信息"
        echo ""
        echo "| 项目 | 值 |"
        echo "|------|-----|"
        echo "| **文件名** | \`${basename}.ppx\` |"
        echo "| **生成时间** | $(date '+%Y-%m-%d %H:%M:%S') |"
        echo "| **AST生成** | $([ ${ast_status} -eq 0 ] && echo '✅ 成功' || echo '❌ 失败') |"
        echo "| **可视化** | $([ ${vis_status} -eq 0 ] && echo '✅ 成功' || echo '❌ 失败') |"
        echo ""
        echo "## 源代码"
        echo ""
        echo "\`\`\`ppx"
        echo "${source_code}"
        echo "\`\`\`"
        echo ""
        echo "## 抽象语法树 (AST)"
        echo ""
        
        if [ ${ast_status} -eq 0 ]; then
            echo "### AST 文本表示"
            echo ""
            echo "\`\`\`"
            echo "${ast_content}"
            echo "\`\`\`"
            echo ""
            
            if [ ${vis_status} -eq 0 ] && [ -f "${png_file}" ]; then
                echo "### AST 可视化图"
                echo ""
                echo "![AST Visualization](../../output/ast_visualized/${basename}.png)"
                echo ""
                echo "图片路径: \`output/ast_visualized/${basename}.png\`"
            else
                echo "### AST 可视化"
                echo ""
                echo "⚠️ **可视化生成失败**"
                if [ -n "${vis_message}" ]; then
                    echo ""
                    echo "\`\`\`"
                    echo "${vis_message}"
                    echo "\`\`\`"
                fi
            fi
        else
            echo "❌ **AST生成失败**"
            echo ""
            echo "\`\`\`"
            echo "${ast_output}"
            echo "\`\`\`"
        fi
        
        echo ""
        echo "## 文件路径"
        echo ""
        echo "| 类型 | 路径 |"
        echo "|------|------|"
        echo "| 源文件 | \`code/${basename}.ppx\` |"
        echo "| AST文件 | \`output/ast/${basename}.ast\` |"
        echo "| 可视化图 | \`output/ast_visualized/${basename}.png\` |"
    } > "${report_file}"
    
    if [ ${ast_status} -eq 0 ]; then
        SUCCESS=$((SUCCESS + 1))
        echo -e "  ${GREEN}✓ 报告已生成${NC}"
    else
        FAILED=$((FAILED + 1))
        echo -e "  ${YELLOW}⚠ 报告已生成（AST生成失败）${NC}"
    fi
done

# 生成汇总报告
summary_file="${REPORT_DIR}/00_summary.md"
{
    echo "# AST 可视化汇总报告"
    echo ""
    echo "生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "## 统计信息"
    echo ""
    echo "| 项目 | 数量 |"
    echo "|------|------|"
    echo "| 总计文件 | ${TOTAL} |"
    echo "| AST生成成功 | ${SUCCESS} |"
    echo "| AST生成失败 | ${FAILED} |"
    echo ""
    echo "## 文件列表"
    echo ""
    echo "| 序号 | 文件名 | AST状态 | 可视化 |"
    echo "|------|--------|---------|--------|"
    
    index=1
    for ppx_file in "${ppx_files[@]}"; do
        if [ -f "${ppx_file}" ]; then
            basename=$(basename "${ppx_file}" .ppx)
            ast_file="${OUTPUT_AST_DIR}/${basename}.ast"
            png_file="${OUTPUT_AST_VIS_DIR}/${basename}.png"
            
            if [ -f "${ast_file}" ]; then
                ast_status="✅"
            else
                ast_status="❌"
            fi
            
            if [ -f "${png_file}" ]; then
                vis_status="✅"
            else
                vis_status="❌"
            fi
            
            echo "| ${index} | [\`${basename}.ppx\`](./${basename}.md) | ${ast_status} | ${vis_status} |"
            ((index++))
        fi
    done
    
    echo ""
    echo "## 输出目录"
    echo ""
    echo "- AST文本文件: \`output/ast/\`"
    echo "- AST可视化图: \`output/ast_visualized/\`"
} > "${summary_file}"

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  处理完成${NC}"
echo -e "${BLUE}========================================${NC}"
echo "总计: ${TOTAL} | 成功: ${SUCCESS} | 失败: ${FAILED}"
echo "报告目录: ${REPORT_DIR}"
echo "汇总报告: 00_summary.md"
