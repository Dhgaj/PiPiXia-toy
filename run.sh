#!/bin/bash

# PiPiXia 项目 - 脚本运行菜单
# 为用户提供交互式选择 scripts 目录下的脚本并执行

# 获取项目根目录和 scripts 目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${SCRIPT_DIR}/scripts"

# 检查 scripts 目录是否存在
if [ ! -d "${SCRIPTS_DIR}" ]; then
    echo "错误: 未找到 scripts 目录: ${SCRIPTS_DIR}"
    exit 1
fi

while true; do
    echo " PiPiXia 项目脚本运行菜单"
    echo " scripts 目录: ${SCRIPTS_DIR}"
    echo ""
    # 收集 scripts 目录下的所有 .sh 脚本
    scripts=()
    for script in "${SCRIPTS_DIR}"/*.sh; do
        [ -f "${script}" ] || continue
        scripts+=("${script}")
    done

    if [ ${#scripts[@]} -eq 0 ]; then
        echo "当前 scripts 目录下没有可用的 .sh 脚本"
        exit 0
    fi

    echo "可用脚本列表："
    index=1
    for script in "${scripts[@]}"; do
        name="$(basename "${script}")"
        echo "  ${index}) ${name}"
        index=$((index + 1))
    done
    echo "  0) 退出"
    echo ""

    # 读取用户选择
    read -p "请输入要运行的脚本编号 (0 退出): " choice

    # 处理退出
    if [ "${choice}" = "0" ]; then
        echo "已退出脚本菜单。"
        break
    fi

    # 检查是否为数字
    if ! [[ "${choice}" =~ ^[0-9]+$ ]]; then
        echo "输入无效，请输入数字编号。"
        echo ""
        continue
    fi

    # 检查编号范围
    if [ "${choice}" -lt 1 ] || [ "${choice}" -gt "${#scripts[@]}" ]; then
        echo "编号超出范围，请重新选择。"
        echo ""
        continue
    fi

    # 计算脚本索引并执行
    script_index=$((choice - 1))
    selected_script="${scripts[${script_index}]}"

    echo ""
    echo "------**************------"
    echo "正在运行脚本: $(basename "${selected_script}")"
    echo "脚本路径: ${selected_script}"
    echo "------**************------"

    bash "${selected_script}"
    status=$?

    echo "------**************------"
    echo "脚本执行完成，退出状态码: ${status}"
    echo "------**************------"
    echo ""

    read -p "按 Enter 键返回菜单..." _
    echo ""
done

exit 0

