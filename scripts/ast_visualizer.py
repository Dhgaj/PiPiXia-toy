#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AST 可视化工具
将 PiPiXia 编译器生成的 .ast 文件转换为图形化显示

依赖: graphviz (pip install graphviz)
使用: python3 ast_visualizer.py <ast_file> [output_format]
"""

import sys
import re
from pathlib import Path

try:
    from graphviz import Digraph
except ImportError:
    print("错误: 需要安装 graphviz 库")
    print("请运行: pip3 install graphviz")
    sys.exit(1)


class ASTNode:
    """AST节点类"""
    def __init__(self, node_type, value="", indent=0):
        self.node_type = node_type
        self.value = value
        self.indent = indent
        self.children = []
        self.id = None  # 用于Graphviz节点ID
    
    def __repr__(self):
        return f"ASTNode({self.node_type}, {self.value}, indent={self.indent})"


class ASTParser:
    """AST文件解析器"""
    
    def __init__(self, ast_file):
        self.ast_file = ast_file
        self.node_id_counter = 0
        
    def parse(self):
        """解析AST文件，返回根节点"""
        with open(self.ast_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
        
        if not lines:
            return None
        
        # 跳过标题行和空行
        start_idx = 0
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("===") or stripped.startswith("Source:") or not stripped:
                start_idx = i + 1
            else:
                break
        
        lines = lines[start_idx:]
        
        # 解析所有节点
        nodes = []
        for line in lines:
            if not line.strip():
                continue
            
            node = self._parse_line(line)
            if node:
                nodes.append(node)
        
        # 构建树结构
        if not nodes:
            return None
        
        root = nodes[0]
        stack = [root]
        
        for i in range(1, len(nodes)):
            current = nodes[i]
            
            # 找到当前节点的父节点
            while stack and stack[-1].indent >= current.indent:
                stack.pop()
            
            if stack:
                stack[-1].children.append(current)
            
            stack.append(current)
        
        return root
    
    def _parse_line(self, line):
        """解析单行AST"""
        # 计算缩进
        indent = len(line) - len(line.lstrip())
        content = line.strip()
        
        if not content:
            return None
        
        # 解析节点类型和值
        # 格式: "NodeType: value" 或 "NodeType"
        if ':' in content:
            parts = content.split(':', 1)
            node_type = parts[0].strip()
            value = parts[1].strip() if len(parts) > 1 else ""
        else:
            node_type = content
            value = ""
        
        return ASTNode(node_type, value, indent)


class ASTVisualizer:
    """AST可视化器"""
    
    def __init__(self, root, output_file, format='png'):
        self.root = root
        self.output_file = output_file
        self.format = format
        self.node_counter = 0
        
        # 创建Graphviz图
        self.graph = Digraph(comment='Abstract Syntax Tree')
        self.graph.attr(rankdir='TB')  # 从上到下
        self.graph.attr('node', shape='box', style='rounded,filled')
        
        # 设置不同节点类型的颜色
        self.node_colors = {
            'Program': '#e1f5ff',
            'Function': '#fff9c4',
            'Block': '#f3e5f5',
            'IntLiteral': '#c8e6c9',
            'DoubleLiteral': '#c8e6c9',
            'StringLiteral': '#c8e6c9',
            'BoolLiteral': '#c8e6c9',
            'Identifier': '#ffccbc',
            'BinaryOp': '#ffe0b2',
            'UnaryOp': '#ffe0b2',
            'VarDecl': '#b2dfdb',
            'Assignment': '#b2dfdb',
            'IfStmt': '#d1c4e9',
            'WhileStmt': '#d1c4e9',
            'ForStmt': '#d1c4e9',
            'ReturnStmt': '#d1c4e9',
            'Type': '#cfd8dc',
        }
    
    def visualize(self):
        """生成可视化图"""
        if not self.root:
            print("错误: 没有AST根节点")
            return False
        
        # 遍历树并添加节点和边
        self._add_node(self.root)
        
        # 渲染图
        try:
            self.graph.render(self.output_file, format=self.format, cleanup=True)
            print(f"✓ AST可视化图已生成: {self.output_file}.{self.format}")
            return True
        except Exception as e:
            print(f"错误: 生成图形失败 - {e}")
            return False
    
    def _add_node(self, node, parent_id=None):
        """递归添加节点到图中"""
        # 生成唯一节点ID
        node_id = f"node_{self.node_counter}"
        self.node_counter += 1
        
        # 创建节点标签
        if node.value:
            label = f"{node.node_type}\n{node.value}"
        else:
            label = node.node_type
        
        # 选择节点颜色
        color = self.node_colors.get(node.node_type, '#e0e0e0')
        
        # 添加节点
        self.graph.node(node_id, label, fillcolor=color)
        
        # 如果有父节点，添加边
        if parent_id:
            self.graph.edge(parent_id, node_id)
        
        # 递归添加子节点
        for child in node.children:
            self._add_node(child, node_id)
        
        return node_id


def main():
    """主函数"""
    # 解析命令行参数
    if len(sys.argv) < 2:
        print("使用方法: python3 ast_visualizer.py <ast_file> [output_file] [format]")
        print("格式选项: png (默认), svg, pdf, dot")
        sys.exit(1)
    
    ast_file = sys.argv[1]
    
    # 支持两种用法：
    # 1. ast_visualizer.py input.ast format
    # 2. ast_visualizer.py input.ast output.png
    if len(sys.argv) >= 3:
        arg2 = sys.argv[2]
        # 如果第二个参数是格式（没有路径分隔符），则是旧用法
        if arg2 in ['png', 'svg', 'pdf', 'dot']:
            output_format = arg2
            output_file = str(Path(ast_file).with_suffix(''))
        else:
            # 否则是输出文件路径
            output_path = Path(arg2)
            output_file = str(output_path.with_suffix(''))
            output_format = output_path.suffix[1:] if output_path.suffix else 'png'
    else:
        output_format = 'png'
        output_file = str(Path(ast_file).with_suffix(''))
    
    # 检查文件是否存在
    if not Path(ast_file).exists():
        print(f"错误: 文件不存在 - {ast_file}")
        sys.exit(1)
    
    print(f"正在解析 AST 文件: {ast_file}")
    
    # 解析AST
    parser = ASTParser(ast_file)
    root = parser.parse()
    
    if not root:
        print("错误: AST文件为空或格式不正确")
        sys.exit(1)
    
    print(f"✓ AST 解析成功")
    print(f"正在生成可视化图 ({output_format} 格式)...")
    
    # 可视化
    visualizer = ASTVisualizer(root, output_file, output_format)
    success = visualizer.visualize()
    
    if success:
        print(f"\n完成! 请查看: {output_file}.{output_format}")
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
