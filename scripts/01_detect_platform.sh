#!/bin/bash

# PiPiXia 编译器 - 平台检测脚本
# 自动检测操作系统和架构，设置相应的编译配置

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 已测试版本
# Ubuntu 24.04
UBUNTU_TESTED_GCC="13.3.0"
UBUNTU_TESTED_CLANG="18.1.8"
UBUNTU_TESTED_BISON="3.8.2"
UBUNTU_TESTED_FLEX="2.6.4"
UBUNTU_TESTED_MAKE="4.3"
UBUNTU_TESTED_LLVM="21.0.0"

# MacOS 15.5 (Apple Silicon)
MACOS_TESTED_GCC="17.0.0"
MACOS_TESTED_CLANG="21.1.5"
MACOS_TESTED_BISON="3.8.2"
MACOS_TESTED_FLEX="2.6.4"
MACOS_TESTED_MAKE="3.81"
MACOS_TESTED_LLVM="21.1.4"

# 版本不匹配警告列表
VERSION_WARNINGS=() 

echo -e "${BLUE}=== PiPiXia 编译器平台检测 ===${NC}"
echo ""

# 提取版本号的辅助函数
extract_version() {
    local version_string=$1
    # 尝试提取x.y.z格式的版本号
    echo "$version_string" | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -n 1
}

# 检测操作系统
OS="$(uname -s)"
case "${OS}" in
    Linux*)     OS_TYPE=Linux;;
    Darwin*)    OS_TYPE=Mac;;
    CYGWIN*)    OS_TYPE=Cygwin;;
    MINGW*)     OS_TYPE=MinGw;;
    *)          OS_TYPE="UNKNOWN:${OS}"
esac

echo -e "${GREEN}操作系统:${NC} ${OS_TYPE}"

# 检测架构
ARCH="$(uname -m)"
case "${ARCH}" in
    x86_64)     ARCH_TYPE=x86_64;;
    arm64)      ARCH_TYPE=ARM64;;
    aarch64)    ARCH_TYPE=ARM64;;
    armv7l)     ARCH_TYPE=ARMv7;;
    i686)       ARCH_TYPE=x86;;
    *)          ARCH_TYPE="UNKNOWN:${ARCH}"
esac

echo -e "${GREEN}系统架构:${NC} ${ARCH_TYPE}"

# 检测LLVM配置
if [ "${OS_TYPE}" == "Mac" ]; then
    # macOS (Homebrew)
    if [ -f "/opt/homebrew/opt/llvm/bin/llvm-config" ]; then
        LLVM_CONFIG="/opt/homebrew/opt/llvm/bin/llvm-config"
        echo -e "${GREEN}LLVM路径:${NC} ${LLVM_CONFIG} (Homebrew Apple Silicon)"
    elif [ -f "/usr/local/opt/llvm/bin/llvm-config" ]; then
        LLVM_CONFIG="/usr/local/opt/llvm/bin/llvm-config"
        echo -e "${GREEN}LLVM路径:${NC} ${LLVM_CONFIG} (Homebrew Intel)"
    else
        echo -e "${RED}错误: 未找到LLVM，请运行 'brew install llvm'${NC}"
        exit 1
    fi
elif [ "${OS_TYPE}" == "Linux" ]; then
    # Ubuntu
    if command -v llvm-config &> /dev/null; then
        LLVM_CONFIG="$(command -v llvm-config)"
        echo -e "${GREEN}LLVM路径:${NC} ${LLVM_CONFIG}"
    elif [ -f "/usr/bin/llvm-config" ]; then
        LLVM_CONFIG="/usr/bin/llvm-config"
        echo -e "${GREEN}LLVM路径:${NC} ${LLVM_CONFIG}"
    else
        echo -e "${RED}错误: 未找到LLVM，请运行 'sudo apt install llvm llvm-dev'${NC}"
        exit 1
    fi
else
    echo -e "${RED}错误: 不支持的操作系统 ${OS_TYPE}${NC}"
    exit 1
fi

# 获取LLVM版本并检查
LLVM_VERSION=$("${LLVM_CONFIG}" --version 2>/dev/null)
LLVM_VERSION_NUM=$(extract_version "$LLVM_VERSION")

# 比较LLVM版本
if [ "${OS_TYPE}" == "Mac" ]; then
    TESTED_LLVM="${MACOS_TESTED_LLVM}"
elif [ "${OS_TYPE}" == "Linux" ]; then
    TESTED_LLVM="${UBUNTU_TESTED_LLVM}"
else
    TESTED_LLVM=""
fi

if [ -n "$TESTED_LLVM" ] && [ -n "$LLVM_VERSION_NUM" ]; then
    if [ "$LLVM_VERSION_NUM" != "$TESTED_LLVM" ]; then
        echo -e "${GREEN}LLVM版本:${NC} ${LLVM_VERSION} ${YELLOW}(已测试版本: ${TESTED_LLVM})${NC}"
        VERSION_WARNINGS+=("LLVM: 当前 ${LLVM_VERSION_NUM}，已测试 ${TESTED_LLVM}")
    else
        echo -e "${GREEN}LLVM版本:${NC} ${LLVM_VERSION}"
    fi
else
    echo -e "${GREEN}LLVM版本:${NC} ${LLVM_VERSION}"
fi

# 检查必要的工具
echo ""
echo -e "${BLUE}检查编译工具...${NC}"

# 比较版本号函数
check_tool_version() {
    local tool=$1
    local name=$2
    local tested_version=$3
    
    if command -v "${tool}" &> /dev/null; then
        local full_version=$("${tool}" --version 2>&1 | head -n 1)
        local current_version=$(extract_version "$full_version")
        
        if [ -n "$tested_version" ] && [ -n "$current_version" ]; then
            if [ "$current_version" != "$tested_version" ]; then
                echo -e "  ${YELLOW}⚠${NC} ${name}: ${current_version} ${YELLOW}(已测试版本: ${tested_version})${NC}"
                VERSION_WARNINGS+=("${name}: 当前 ${current_version}，已测试 ${tested_version}")
            else
                echo -e "  ${GREEN}✓${NC} ${name}: ${current_version}"
            fi
        else
            echo -e "  ${GREEN}✓${NC} ${name}: ${full_version}"
        fi
        return 0
    else
        echo -e "  ${RED}✗${NC} ${name}: 未安装"
        return 1
    fi
}

ALL_GOOD=true

# 根据平台选择已测试版本
if [ "${OS_TYPE}" == "Mac" ]; then
    check_tool_version "g++" "G++" "${MACOS_TESTED_GCC}" || ALL_GOOD=false
    check_tool_version "flex" "Flex" "${MACOS_TESTED_FLEX}" || ALL_GOOD=false
    check_tool_version "bison" "Bison" "${MACOS_TESTED_BISON}" || ALL_GOOD=false
    check_tool_version "make" "Make" "${MACOS_TESTED_MAKE}" || ALL_GOOD=false
elif [ "${OS_TYPE}" == "Linux" ]; then
    check_tool_version "g++" "G++" "${UBUNTU_TESTED_GCC}" || ALL_GOOD=false
    check_tool_version "flex" "Flex" "${UBUNTU_TESTED_FLEX}" || ALL_GOOD=false
    check_tool_version "bison" "Bison" "${UBUNTU_TESTED_BISON}" || ALL_GOOD=false
    check_tool_version "make" "Make" "${UBUNTU_TESTED_MAKE}" || ALL_GOOD=false
else
    # 未知平台，不进行版本比较
    check_tool_version "g++" "G++" "" || ALL_GOOD=false
    check_tool_version "flex" "Flex" "" || ALL_GOOD=false
    check_tool_version "bison" "Bison" "" || ALL_GOOD=false
    check_tool_version "make" "Make" "" || ALL_GOOD=false
fi

echo ""

# 输出环境信息到文件
cat > .platform_config << EOF
# 自动生成的平台配置
OS_TYPE=${OS_TYPE}
ARCH_TYPE=${ARCH_TYPE}
LLVM_CONFIG=${LLVM_CONFIG}
LLVM_VERSION=${LLVM_VERSION}
EOF

echo -e "${GREEN}平台配置已保存到 .platform_config${NC}"

# 显示版本警告（如果有）
if [ ${#VERSION_WARNINGS[@]} -gt 0 ]; then
    echo ""
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}⚠  版本差异提醒${NC}"
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${CYAN}检测到以下工具版本与 README.md 中已测试版本不一致：${NC}"
    echo ""
    for warning in "${VERSION_WARNINGS[@]}"; do
        echo -e "  ${YELLOW}•${NC} ${warning}"
    done
    echo ""
    echo -e "${CYAN}说明：${NC}"
    echo -e "  - 版本不一致${YELLOW}可能${NC}导致编译或运行时问题"
    echo -e "  - 如遇到问题，建议使用已测试版本"
    echo -e "  - 或根据实际情况修改代码以适配当前版本"
    echo ""
    if [ "${OS_TYPE}" == "Mac" ]; then
        echo -e "${CYAN}macOS 版本管理：${NC}"
        echo -e "  ${BLUE}brew install llvm@21${NC}    # 安装特定版本LLVM"
        echo -e "  ${BLUE}brew switch llvm 21.1.4${NC} # 切换到特定版本（如果已安装）"
    elif [ "${OS_TYPE}" == "Linux" ]; then
        echo -e "${CYAN}Ubuntu 版本管理：${NC}"
        echo -e "  ${BLUE}sudo apt install llvm-21-dev${NC}  # 安装特定版本LLVM"
    fi
    echo -e "${YELLOW}----------------------------------------${NC}"
fi

# 检查结果
echo ""
if [ "${ALL_GOOD}" = true ]; then
    echo -e "${GREEN}✓ 所有必需工具已安装${NC}"
    if [ ${#VERSION_WARNINGS[@]} -eq 0 ]; then
        echo -e "${GREEN}✓ 所有版本与已测试版本一致${NC}"
    fi
    echo -e "${GREEN}✓ 可以开始编译${NC}"
    echo ""
    echo -e "运行以下命令开始编译："
    echo -e "  ${BLUE}make${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ 某些工具未安装，请先安装缺失的工具${NC}"
    echo ""
    if [ "${OS_TYPE}" == "Mac" ]; then
        echo -e "macOS 安装命令:"
        echo -e "  ${BLUE}brew install llvm flex bison gcc make${NC}"
    elif [ "${OS_TYPE}" == "Linux" ]; then
        echo -e "Ubuntu/Debian 安装命令:"
        echo -e "  ${BLUE}sudo apt update${NC}"
        echo -e "  ${BLUE}wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -${NC}"
        echo -e "  ${BLUE}sudo add-apt-repository \"deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-21 main\"${NC}"
        echo -e "  ${BLUE}sudo apt update${NC}"
        echo -e "  ${BLUE}sudo apt install bison flex llvm-21-dev liblld-21-dev clang-21 g++ make${NC}"
    fi
    exit 1
fi
