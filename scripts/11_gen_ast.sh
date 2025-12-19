#!/bin/bash
# 生成AST文件和可视化
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/09_run_build_all.sh" -a "$@"
