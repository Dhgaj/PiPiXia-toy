#!/bin/bash
# AST 可视化脚本
# 用途：将 code/ 目录下的所有 .ppx 文件转换为 PNG 格式的 AST 树状图

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 工具路径
COMPILER="$PROJECT_ROOT/compiler"
VISUALIZER="$SCRIPT_DIR/ast_visualizer.py"
CODE_DIR="$PROJECT_ROOT/code"
OUTPUT_DIR="$PROJECT_ROOT/output/ast_visualized"

# 检查依赖
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到 python3"
    exit 1
fi

if ! command -v dot &> /dev/null || ! python3 -c "import graphviz" 2>/dev/null; then
    echo "缺少依赖，请安装:"
    echo "  pip3 install graphviz"
    exit 1
fi

# 检查编译器
if [ ! -f "$COMPILER" ]; then
    echo "错误: 编译器不存在，请先运行: make"
    exit 1
fi

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 查找所有 .ppx 文件
ppx_files=("$CODE_DIR"/*.ppx)

if [ ! -e "${ppx_files[0]}" ]; then
    echo "错误: code/ 目录中未找到 .ppx 文件"
    exit 1
fi

total=${#ppx_files[@]}
success=0
failed=0

echo -e "开始处理 code/ 目录 (共 $total 个文件)"
echo ""

# 处理每个文件
for ppx_file in "${ppx_files[@]}"; do
    filename=$(basename "$ppx_file" .ppx)
    current=$((success + failed + 1))
    
    echo -e "[$current/$total] $filename.ppx"
    
    # 生成 AST 到 code/ 目录
    ast_file="$CODE_DIR/${filename}.ast"
    if ! "$COMPILER" "$ppx_file" -ast &> /dev/null; then
        echo "  ✗ 编译失败"
        failed=$((failed + 1))
        continue
    fi
    
    # 转换为 PNG，输出到 ast_visualized/ 目录
    png_output="$OUTPUT_DIR/${filename}.png"
    if ! python3 "$VISUALIZER" "$ast_file" "$png_output" &> /dev/null; then
        echo "  ✗ 可视化失败"
        failed=$((failed + 1))
        continue
    fi
    
    echo -e "  ✓ 成功"
    success=$((success + 1))
done

echo ""
echo -e "处理完成"
echo -e "成功: $success  失败: $failed  总计: $total"
echo ""
echo "输出目录: $OUTPUT_DIR/"
