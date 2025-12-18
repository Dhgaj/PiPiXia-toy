#!/bin/bash

# 批量运行error目录下的错误代码并生成报告
# 每个ppx文件生成一个对应的Markdown报告

# 获取脚本所在目录的上级目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 定义路径
ERROR_DIR="$PROJECT_DIR/error"
REPORT_DIR="$PROJECT_DIR/report/error"
COMPILER="$PROJECT_DIR/compiler"

# 检查编译器是否存在
if [ ! -f "$COMPILER" ]; then
    echo "错误：编译器不存在，请先编译项目"
    exit 1
fi

# 确保报告目录存在
mkdir -p "$REPORT_DIR"

# 清空旧报告
echo "清理旧报告..."
find "$REPORT_DIR" -name "*.md" -type f -delete 2>/dev/null

# 统计变量
total=0
success=0
failed=0

echo "========================================"
echo "PPX 错误代码测试报告生成器"
echo "========================================"
echo ""
echo "错误代码目录: $ERROR_DIR"
echo "报告输出目录: $REPORT_DIR"
echo ""

# 遍历error目录下的所有ppx文件
for ppx_file in "$ERROR_DIR"/*.ppx; do
    # 检查文件是否存在
    if [ ! -f "$ppx_file" ]; then
        echo "警告：没有找到ppx文件"
        continue
    fi
    
    # 获取文件名（不含路径和扩展名）
    filename=$(basename "$ppx_file" .ppx)
    
    # 报告文件路径
    report_file="$REPORT_DIR/${filename}.md"
    
    echo "处理: $filename.ppx"
    
    # 读取ppx文件的第一行作为错误类型
    error_type=$(head -n 1 "$ppx_file" | sed 's/^# *//')
    
    # 读取第二行作为错误描述
    error_desc=$(sed -n '2p' "$ppx_file" | sed 's/^# *//')
    
    # 读取完整的源代码
    source_code=$(cat "$ppx_file")
    
    # 运行编译器并捕获输出
    raw_output=$("$COMPILER" "$ppx_file" 2>&1)
    exit_code=$?
    # 过滤ANSI颜色代码
    compiler_output=$(echo "$raw_output" | sed 's/\x1b\[[0-9;]*m//g')
    
    # 生成Markdown报告
    {
        echo "# $error_type"
        echo ""
        echo "## 基本信息"
        echo ""
        echo "| 项目 | 值 |"
        echo "|------|-----|"
        echo "| **文件名** | \`$filename.ppx\` |"
        echo "| **错误类型** | $error_type |"
        echo "| **退出码** | $exit_code |"
        echo "| **测试时间** | $(date '+%Y-%m-%d %H:%M:%S') |"
        echo ""
        echo "## 错误描述"
        echo ""
        echo "$error_desc"
        echo ""
        echo "## 源代码"
        echo ""
        echo "\`\`\`ppx"
        echo "$source_code"
        echo "\`\`\`"
        echo ""
        echo "## 编译器输出"
        echo ""
        echo "\`\`\`"
        if [ -n "$compiler_output" ]; then
            echo "$compiler_output"
        else
            echo "(无输出)"
        fi
        echo "\`\`\`"
        echo ""
        echo "## 分析"
        echo ""
        if [ $exit_code -ne 0 ]; then
            echo "✅ 编译器正确检测到错误，返回退出码: $exit_code"
            ((success++))
        else
            echo "⚠️ 编译器未检测到错误（退出码为0），可能需要完善错误检测机制"
            ((failed++))
        fi
    } > "$report_file"
    
    ((total++))
    
    echo "  -> 报告已生成: ${filename}.md"
done

echo ""
echo "========================================"
echo "测试完成"
echo "========================================"
echo "总计测试: $total 个文件"
echo "检测到错误: $success 个"
echo "未检测到错误: $failed 个"
echo "报告目录: $REPORT_DIR"
echo ""

# 生成汇总报告
summary_file="$REPORT_DIR/00_summary.md"
{
    echo "# PPX 错误测试汇总报告"
    echo ""
    echo "生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    echo "## 统计信息"
    echo ""
    echo "| 项目 | 数量 |"
    echo "|------|------|"
    echo "| 总计测试文件 | $total |"
    echo "| 正确检测到错误 | $success |"
    echo "| 未检测到错误 | $failed |"
    echo ""
    echo "## 测试文件列表"
    echo ""
    echo "| 序号 | 文件名 | 错误类型 |"
    echo "|------|--------|----------|"
    
    index=1
    for ppx_file in "$ERROR_DIR"/*.ppx; do
        if [ -f "$ppx_file" ]; then
            filename=$(basename "$ppx_file" .ppx)
            error_type=$(head -n 1 "$ppx_file" | sed 's/^# *//')
            echo "| $index | [\`$filename.ppx\`](./${filename}.md) | $error_type |"
            ((index++))
        fi
    done
    
    echo ""
    echo "## 错误类型分类"
    echo ""
    echo "### 语法错误"
    echo "- 缺少括号（左/右括号、花括号、方括号）"
    echo "- 缺少关键字（func、冒号等）"
    echo "- 字符串/注释未闭合"
    echo "- 无效的运算符或关键字"
    echo ""
    echo "### 语义错误"
    echo "- 未定义的变量或函数"
    echo "- 类型不匹配"
    echo "- 重复定义"
    echo "- 常量重新赋值"
    echo "- 参数数量错误"
    echo ""
    echo "### 控制流错误"
    echo "- break/continue在循环外使用"
    echo "- 缺少main函数"
    echo "- 缺少return语句"
} > "$summary_file"

echo "汇总报告已生成: 00_summary.md"
