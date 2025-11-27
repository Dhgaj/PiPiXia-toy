#!/bin/bash
# PPX 语法测试脚本

echo "PPX 语法教程测试验证"
echo ""

TOTAL=0
PASSED=0
FAILED=0

# 测试所有 code 目录下的文件
for file in code/[0-9][0-9]_*.ppx; do
    if [ -f "$file" ]; then
        TOTAL=$((TOTAL + 1))
        filename=$(basename "$file" .ppx)
        
        # 编译文件
        if ./compiler "$file" > /dev/null 2>&1; then
            PASSED=$((PASSED + 1))
            echo "✓ $filename"
        else
            FAILED=$((FAILED + 1))
            echo "✗ $filename - 编译失败"
        fi
    fi
done

echo ""
echo "  测试总结"
echo "总计: $TOTAL"
echo "通过: $PASSED"
echo "失败: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo ""
    echo "🎉 所有测试通过！"
    exit 0
else
    echo ""
    echo "⚠️  有 $FAILED 个测试失败"
    exit 1
fi
