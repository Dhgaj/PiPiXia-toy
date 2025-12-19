#!/bin/bash
# 生成符号表文件
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${SCRIPT_DIR}/09_run_build_all.sh" -s "$@"
