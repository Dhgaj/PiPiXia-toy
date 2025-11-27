#!/bin/bash

# PiPiXia 编译器 - 自动构建脚本
# 自动检测平台并使用正确的配置进行编译

set -e  # 遇到错误立即退出

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

cd "${PROJECT_ROOT}"

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE} PiPiXia 编译器自动构建 ${NC}"
echo ""

# 运行平台检测
if [ -f "scripts/01_detect_platform.sh" ]; then
    bash scripts/01_detect_platform.sh
    if [ $? -ne 0 ]; then
        echo -e "${YELLOW}平台检测失败，请检查依赖安装${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}警告: 未找到平台检测脚本，使用默认配置${NC}"
fi

echo ""
echo -e "${BLUE}开始编译...${NC}"
echo ""

# 加载平台配置
if [ -f ".platform_config" ]; then
    source .platform_config
    echo -e "${GREEN}使用平台配置:${NC}"
    echo -e "  OS: ${OS_TYPE}"
    echo -e "  ARCH: ${ARCH_TYPE}"
    echo -e "  LLVM: ${LLVM_CONFIG}"
    echo ""
fi

# 执行编译
if [ "${OS_TYPE}" == "Mac" ] && [ -n "${LLVM_CONFIG}" ]; then
    # macOS 使用检测到的LLVM路径
    make LLVM_CONFIG="${LLVM_CONFIG}"
elif [ "${OS_TYPE}" == "Linux" ] && [ -n "${LLVM_CONFIG}" ]; then
    # Linux 使用检测到的LLVM路径
    make LLVM_CONFIG="${LLVM_CONFIG}"
else
    # 使用默认配置
    make
fi

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ 编译成功！${NC}"
    echo ""
    echo -e "可执行文件: ${BLUE}./compiler${NC}"
    echo ""
    echo -e "使用示例:"
    echo -e "  ${BLUE}./compiler code/make_test/main.ppx -o output/exec/main${NC}"
    echo -e "  ${BLUE}./output/exec/main${NC}"
else
    echo ""
    echo -e "${YELLOW}✗ 编译失败${NC}"
    exit 1
fi
