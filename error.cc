/**
 * error.cc
 * PiPiXia 编译器错误处理模块实现
 * 
 * 模块结构：
 * 1. 全局变量定义
 * 2. 源文件管理
 * 3. 辅助函数
 * 4. 错误信息翻译
 * 5. 修复建议生成
 * 6. 错误报告输出
 * 7. 警告控制
 */

#include "error.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

//  * 全局变量定义
// ANSI 终端颜色代码
namespace ErrorColors {
    const char* RED    = "\033[1;31m";  // 红色（错误）
    const char* YELLOW = "\033[1;33m";  // 黄色（警告）
    const char* CYAN   = "\033[36m";    // 青色（提示）
    const char* BOLD   = "\033[1m";     // 粗体
    const char* RESET  = "\033[0m";     // 重置样式
}

// 源代码缓存
std::vector<std::string> g_sourceLines;
std::string g_sourceFilePath;

// 错误统计
int g_errorCount = 0;
int g_warningCount = 0;
int g_syntaxErrorCount = 0;

// 警告控制选项
bool g_enableAllWarnings = false;
bool g_warningsAsErrors = false;
bool g_suppressWarnings = false;
bool g_enableUnusedWarnings = true;
bool g_enableDeadCodeWarnings = true;
bool g_enableMissingReturnWarnings = true;
bool g_enableShadowWarnings = false;  // 需要 -Wall 或 -Wshadow 启用


// 源文件管理
// 加载源文件到内存缓存
void loadSourceFile(const std::string& filename) {
    g_sourceFilePath = filename;
    g_sourceLines.clear();
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            g_sourceLines.push_back(line);
        }
        file.close();
    }
}

// 获取指定行的源代码（行号从1开始）
std::string getSourceLine(int lineNum) {
    if (lineNum > 0 && lineNum <= static_cast<int>(g_sourceLines.size())) {
        return g_sourceLines[lineNum - 1];
    }
    return "";
}

// 重置错误和警告计数器
void resetErrorCounts() {
    g_errorCount = 0;
    g_warningCount = 0;
    g_syntaxErrorCount = 0;
}


// 辅助函数
namespace {
// 去除字符串首尾空白
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

// 检查字符串是否以指定前缀开头
bool startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.substr(0, prefix.length()) == prefix;
}

// 检查字符串是否以指定后缀结尾
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.substr(str.length() - suffix.length()) == suffix;
}

// 统计源代码中未匹配的括号数量（跳过注释和字符串）
void countBrackets(int& braces, int& brackets, int& parens) {
    braces = brackets = parens = 0;
    for (const auto& line : g_sourceLines) {
        bool inString = false;
        bool inComment = false;
        for (size_t i = 0; i < line.length(); i++) {
            char c = line[i];
            if (c == '#' && !inString) inComment = true;
            if (inComment) continue;
            if (c == '"' && (i == 0 || line[i-1] != '\\')) {
                inString = !inString;
                continue;
            }
            if (inString) continue;
            
            switch (c) {
                case '{': braces++;   break;
                case '}': braces--;   break;
                case '[': brackets++; break;
                case ']': brackets--; break;
                case '(': parens++;   break;
                case ')': parens--;   break;
            }
        }
    }
}

// 显示源代码上下文（不显示列指向）
void displaySourceContext(int line, int column, bool isError) {
    (void)column;  // 不再使用列号
    if (line <= 0 || g_sourceLines.empty()) return;
    
    std::cerr << std::endl;
    int startLine = std::max(1, line - 2);
    
    for (int i = startLine; i <= line; i++) {
        std::string srcLine = getSourceLine(i);
        if (i == line) {
            // 高亮当前行
            std::cerr << (isError ? ErrorColors::RED : ErrorColors::YELLOW) 
                      << " >> " << ErrorColors::RESET;
            std::cerr << ErrorColors::CYAN << std::setw(4) << i << " | " 
                      << ErrorColors::RESET;
            std::cerr << ErrorColors::BOLD << srcLine << ErrorColors::RESET 
                      << std::endl;
        } else {
            std::cerr << "    ";
            std::cerr << ErrorColors::CYAN << std::setw(4) << i << " | " 
                      << ErrorColors::RESET;
            std::cerr << srcLine << std::endl;
        }
    }
}

} // anonymous namespace

// 错误信息翻译
// Token 翻译表（按长度降序排列，避免部分匹配问题）
static const struct { const char* from; const char* to; } tokenTranslations[] = {
    // 字面量（先处理长的）
    {"DOUBLE_LITERAL", "浮点数"},
    {"STRING_LITERAL", "字符串"},
    {"CHAR_LITERAL", "字符"},
    {"BOOL_LITERAL", "布尔值"},
    {"INT_LITERAL", "整数"},
    {"IDENTIFIER", "标识符"},
    
    // 复合运算符
    {"PLUS_ASSIGN", "'+='"},
    {"MINUS_ASSIGN", "'-='"},
    {"MULT_ASSIGN", "'*='"},
    {"DIV_ASSIGN", "'/='"},
    {"MOD_ASSIGN", "'%='"},
    
    // 分隔符
    {"RPAREN", "')' (右括号)"},
    {"LPAREN", "'(' (左括号)"},
    {"RBRACE", "'}' (右花括号)"},
    {"LBRACE", "'{' (左花括号)"},
    {"RBRACKET", "']' (右方括号)"},
    {"LBRACKET", "'[' (左方括号)"},
    {"SEMICOLON", "';'"},
    {"DOTDOT", "'..'"},
    {"COMMA", "','"},
    {"COLON", "':'"},
    {"DOT", "'.'"},
    
    // 关键字
    {"CONTINUE", "'continue'"},
    {"DEFAULT", "'default'"},
    {"RETURN", "'return'"},
    {"IMPORT", "'import'"},
    {"SWITCH", "'switch'"},
    {"WHILE", "'while'"},
    {"BREAK", "'break'"},
    {"CATCH", "'catch'"},
    {"THROW", "'throw'"},
    {"CONST", "'const'"},
    {"ELSE", "'else'"},
    {"CASE", "'case'"},
    {"FUNC", "'func'"},
    {"FOR", "'for'"},
    {"TRY", "'try'"},
    {"LET", "'let'"},
    {"IN", "'in'"},
    {"IF", "'if'"},
    
    // 运算符
    {"MULTIPLY", "'*'"},
    {"DIVIDE", "'/'"},
    {"MODULO", "'%'"},
    {"ASSIGN", "'='"},
    {"MINUS", "'-'"},
    {"PLUS", "'+'"},
    {"NOT", "'!'"},
    {"AND", "'&&'"},
    {"OR", "'||'"},
    {"NE", "'!='"},
    {"LE", "'<='"},
    {"GE", "'>='"},
    {"EQ", "'=='"},
    {"LT", "'<'"},
    {"GT", "'>'"},
    {"TYPE", "类型"},
    
    // 错误描述
    {"syntax error", "语法错误"},
    {"unexpected", "遇到意外的"},
    {"expecting", "期望"},
    {"$end", "文件结尾"},
    {nullptr, nullptr}
};

// 翻译语法错误消息（英文 -> 中文）
std::string translateErrorMessage(const char* msg) {
    std::string result = msg;
    
    // 应用Token翻译表
    for (int i = 0; tokenTranslations[i].from != nullptr; i++) {
        size_t pos = 0;
        while ((pos = result.find(tokenTranslations[i].from, pos)) != std::string::npos) {
            result.replace(pos, strlen(tokenTranslations[i].from), tokenTranslations[i].to);
            pos += strlen(tokenTranslations[i].to);
        }
    }
    
    // 清理残留的引号格式问题
    size_t pos = 0;
    while ((pos = result.find("''", pos)) != std::string::npos) {
        result.replace(pos, 2, "'");
    }
    
    return result;
}

// 翻译语义错误消息为中文
std::string translateSemanticError(const std::string& msg) {
    // 未定义变量
    if (msg.find("Undefined variable") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            return "未定义的变量 '" + name + "'";
        }
        return "未定义的变量";
    }
    
    // 未定义函数
    if (msg.find("Undefined function") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            return "未定义的函数 '" + name + "'";
        }
        return "未定义的函数";
    }
    
    // 类型不匹配
    if (msg.find("Type mismatch") != std::string::npos) {
        if (msg.find("cannot assign") != std::string::npos) {
            size_t toPos = msg.find(" to ");
            if (toPos != std::string::npos) {
                std::string fromType = msg.substr(msg.find("assign ") + 7, toPos - msg.find("assign ") - 7);
                std::string toType = msg.substr(toPos + 4);
                // 移除引号
                if (toType.front() == '\'') toType = toType.substr(1);
                if (toType.back() == '\'') toType = toType.substr(0, toType.length() - 1);
                return "类型不匹配: 无法将 " + fromType + " 赋值给 '" + toType + "' 类型";
            }
        }
        return "类型不匹配";
    }
    
    // 重复定义
    if (msg.find("already defined") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            if (msg.find("variable") != std::string::npos || msg.find("Variable") != std::string::npos)
                return "变量 '" + name + "' 已定义";
            if (msg.find("function") != std::string::npos || msg.find("Function") != std::string::npos)
                return "函数 '" + name + "' 已定义";
            return "'" + name + "' 已定义";
        }
        return "标识符已定义";
    }
    
    // 参数数量错误
    if (msg.find("expects") != std::string::npos && msg.find("argument") != std::string::npos) {
        return "函数参数数量不正确";
    }
    
    // 除零错误
    if (msg.find("Division by zero") != std::string::npos)
        return "除数为零";
    if (msg.find("Modulo by zero") != std::string::npos)
        return "取模运算除数为零";
    if (msg.find("Integer division by zero") != std::string::npos)
        return "整数除法除数为零";
    
    // 常量重新赋值
    if (msg.find("Cannot reassign") != std::string::npos && msg.find("const") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            return "无法重新赋值常量 '" + name + "'";
        }
        return "无法重新赋值常量";
    }
    
    // 变量遮蔽
    if (msg.find("shadows") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            if (msg.find("Parameter") != std::string::npos)
                return "参数 '" + name + "' 遮蔽了全局变量";
            return "变量 '" + name + "' 遮蔽了全局变量";
        }
    }
    
    // 未使用变量
    if (msg.find("Unused variable") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            return "未使用的变量 '" + name + "'";
        }
        return "未使用的变量";
    }
    
    // break/continue 不在循环中
    if (msg.find("'break' statement not in loop") != std::string::npos)
        return "'break' 语句不在循环或 switch 中";
    if (msg.find("'continue' statement not in loop") != std::string::npos)
        return "'continue' 语句不在循环中";
    
    // void 函数返回值
    if (msg.find("Cannot return a value from void function") != std::string::npos)
        return "void 函数不能返回值";
    if (msg.find("return value from void") != std::string::npos)
        return "void 函数不能返回值";
    
    // 数组相关
    if (msg.find("Array size mismatch") != std::string::npos) {
        if (msg.find("declared size") != std::string::npos)
            return "数组大小不匹配: 声明的大小与初始化元素数量不一致";
        return "数组大小不匹配";
    }
    if (msg.find("Undefined array variable") != std::string::npos) {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string name = msg.substr(start + 1, end - start - 1);
            return "未定义的数组变量 '" + name + "'";
        }
        return "未定义的数组变量";
    }
    if (msg.find("Array index must be integer") != std::string::npos)
        return "数组索引必须是整数类型";
    
    // 缺少 main 函数
    if (msg.find("No 'main' function defined") != std::string::npos)
        return "未定义 'main' 函数，程序需要入口点";
    
    // 字符串插值错误
    if (msg.find("未闭合的 ${}") != std::string::npos)
        return msg;  // 已经是中文
    
    // 未知字符
    if (msg.find("Unknown character") != std::string::npos)
        return msg;  // 保留原始消息
    
    return msg;  // 无法翻译时返回原文
}

// 修复建议生成

// 根据语法错误生成修复建议
std::string generateSyntaxHint(const std::string& errorMsg, int lineNum) {
    std::string currentLine = trim(getSourceLine(lineNum));
    std::string prevLine = (lineNum > 1) ? trim(getSourceLine(lineNum - 1)) : "";
    std::string lineStr = std::to_string(lineNum);
    std::string prevLineStr = std::to_string(lineNum - 1);
    
    // 当前行错误模式检查
    if (!currentLine.empty()) {
        if (currentLine == "if" || currentLine == "if {")
            return "提示: 第 " + lineStr + " 行的 'if' 语句缺少条件表达式";
        if (currentLine == "while" || currentLine == "while {")
            return "提示: 第 " + lineStr + " 行的 'while' 语句缺少条件表达式";
        if (currentLine == "switch" || currentLine == "switch {")
            return "提示: 第 " + lineStr + " 行的 'switch' 语句缺少匹配表达式";
        if (currentLine == "case" || currentLine == "case:")
            return "提示: 第 " + lineStr + " 行的 'case' 子句缺少匹配值";
        
        if (startsWith(currentLine, "for ") || currentLine == "for") {
            if (currentLine.find(" in ") == std::string::npos)
                return "提示: 第 " + lineStr + " 行的 'for' 语句格式不完整";
            if (currentLine.find("..") == std::string::npos)
                return "提示: 第 " + lineStr + " 行的 'for' 语句缺少范围运算符 '..'";
        }
        
        if (startsWith(currentLine, "func ")) {
            if (currentLine.find("(") == std::string::npos)
                return "提示: 第 " + lineStr + " 行的函数定义缺少参数列表 '()'";
            if (currentLine.find("{") == std::string::npos && currentLine.find(")") != std::string::npos) {
                size_t pos = currentLine.find(")");
                if (trim(currentLine.substr(pos + 1)).empty())
                    return "提示: 第 " + lineStr + " 行的函数定义缺少函数体 '{}'";
            }
        }
    }
    
    // 前一行错误模式检查
    if (!prevLine.empty()) {
        if (startsWith(prevLine, "let ") || startsWith(prevLine, "const ")) {
            if (endsWith(prevLine, "=") || endsWith(prevLine, "= "))
                return "提示: 第 " + prevLineStr + " 行的变量声明缺少初始值";
            if (prevLine.find(":") == std::string::npos && prevLine.find("=") != std::string::npos)
                return "提示: 第 " + prevLineStr + " 行的变量声明可能缺少类型注解";
        }
        
        if (endsWith(prevLine, "(") || endsWith(prevLine, "( "))
            return "提示: 第 " + prevLineStr + " 行可能缺少参数和右括号 ')'";
        if (endsWith(prevLine, "["))
            return "提示: 第 " + prevLineStr + " 行的数组访问不完整，缺少索引和 ']'";
        if (endsWith(prevLine, ":") || endsWith(prevLine, ": "))
            return "提示: 第 " + prevLineStr + " 行可能缺少类型声明";
        
        char last = prevLine.back();
        if (last == '+' || last == '-' || last == '*' || last == '/' || last == '%')
            return "提示: 第 " + prevLineStr + " 行的表达式不完整";
        
        if (endsWith(prevLine, "==") || endsWith(prevLine, "!=") || 
            endsWith(prevLine, "<=") || endsWith(prevLine, ">=") ||
            endsWith(prevLine, "&&") || endsWith(prevLine, "||"))
            return "提示: 第 " + prevLineStr + " 行的表达式不完整，缺少右侧操作数";
        
        if (startsWith(prevLine, "if ") && prevLine.find("{") == std::string::npos)
            return "提示: 第 " + prevLineStr + " 行的 'if' 语句缺少 '{'";
        if (startsWith(prevLine, "while ") && prevLine.find("{") == std::string::npos)
            return "提示: 第 " + prevLineStr + " 行的 'while' 语句缺少 '{'";
        if (startsWith(prevLine, "for ") && prevLine.find("{") == std::string::npos)
            return "提示: 第 " + prevLineStr + " 行的 'for' 语句缺少 '{'";
    }
    
    // 意外遇到右括号 -> 缺少对应的左括号，分析当前行找出应该添加的位置
    if (errorMsg.find("意外的 ')'") != std::string::npos) {
        // 检查当前行是否有 if/while/for 语句
        if (currentLine.find("if ") != std::string::npos || currentLine.find("if(") != std::string::npos) {
            return "提示: 'if' 后缺少左括号 '('，正确格式: if (条件) { }";
        }
        if (currentLine.find("while ") != std::string::npos || currentLine.find("while(") != std::string::npos) {
            return "提示: 'while' 后缺少左括号 '('，正确格式: while (条件) { }";
        }
        if (currentLine.find("for ") != std::string::npos || currentLine.find("for(") != std::string::npos) {
            return "提示: 'for' 后缺少左括号 '('，正确格式: for (变量 in 范围) { }";
        }
        return "提示: 遇到多余的 ')'，请检查前面是否缺少 '('";
    }
    if (errorMsg.find("意外的 '}'") != std::string::npos)
        return "提示: 遇到多余的 '}'，请检查前面是否缺少 '{'";
    if (errorMsg.find("意外的 ']'") != std::string::npos)
        return "提示: 遇到多余的 ']'，请检查前面是否缺少 '['";
    
    // 期望右括号 -> 缺少对应的右括号
    if (errorMsg.find("期望") != std::string::npos) {
        if (errorMsg.find("')'") != std::string::npos)
            return "提示: 缺少右括号 ')'，请检查括号是否匹配";
        if (errorMsg.find("'}'") != std::string::npos)
            return "提示: 缺少右花括号 '}'，请检查括号是否匹配";
        if (errorMsg.find("']'") != std::string::npos)
            return "提示: 缺少右方括号 ']'，请检查括号是否匹配";
    }
    
    // 文件结尾错误 - 智能检测缺少的括号或关键字
    if (errorMsg.find("文件结尾") != std::string::npos || 
        errorMsg.find("end of file") != std::string::npos) {
        // 先检查是否是缺少关键字的情况
        if (currentLine.find("():") != std::string::npos || 
            currentLine.find("(): ") != std::string::npos)
            return "提示: 函数定义缺少 'func' 关键字，正确格式: func 函数名(): 返回类型 { }";
        if (currentLine.find("/#/") != std::string::npos)
            return "提示: 多行注释未正确闭合，格式为 /#/ 注释内容 /#/";
        
        // 检查括号匹配
        int braces, brackets, parens;
        countBrackets(braces, brackets, parens);
        if (braces > 0)   return "提示: 缺少右花括号 '}'，请检查括号是否匹配";
        if (brackets > 0) return "提示: 缺少右方括号 ']'，请检查括号是否匹配";
        if (parens > 0)   return "提示: 缺少右括号 ')'，请检查括号是否匹配";
        return "提示: 语法错误，请检查代码结构";
    }
    
    // 期望左括号
    if (errorMsg.find("期望 '{'") != std::string::npos)
        return "提示: 缺少左花括号 '{'，请检查语法格式";
    if (errorMsg.find("期望 '['") != std::string::npos)
        return "提示: 缺少左方括号 '['，请检查语法格式";
    if (errorMsg.find("期望 '('") != std::string::npos)
        return "提示: 缺少左括号 '('，请检查语法格式";
    
    // 期望冒号（类型声明）
    if (errorMsg.find("期望 ':'") != std::string::npos)
        return "提示: 缺少冒号 ':'，变量声明格式为 'let 变量名: 类型 = 值'";
    
    // 期望等号
    if (errorMsg.find("期望 '='") != std::string::npos)
        return "提示: 缺少赋值符号 '='，请检查变量声明或赋值语句";
    
    // 期望 case/default
    if (errorMsg.find("期望 'case'") != std::string::npos || 
        errorMsg.find("期望 'default'") != std::string::npos)
        return "提示: switch 语句内部需要 'case' 或 'default' 子句";
    
    // 意外的符号
    if (errorMsg.find("意外的 类型") != std::string::npos)
        return "提示: 类型声明位置不正确，请检查语法格式";
    if (errorMsg.find("意外的 ','") != std::string::npos)
        return "提示: 逗号位置不正确，请检查参数列表或变量声明";
    if (errorMsg.find("意外的 '.'") != std::string::npos)
        return "提示: 点号位置不正确，请检查范围运算符 '..' 的使用";
    if (errorMsg.find("意外的 '*'") != std::string::npos)
        return "提示: 运算符位置不正确，请检查表达式语法";
    if (errorMsg.find("意外的 整数") != std::string::npos)
        return "提示: 数字位置不正确，请检查语法格式";
    if (errorMsg.find("意外的 标识符") != std::string::npos)
        return "提示: 标识符位置不正确，请检查语法格式";
    if (errorMsg.find("意外的 ':'") != std::string::npos) {
        // 检查是否使用了无效关键字 var
        if (currentLine.find("var ") != std::string::npos)
            return "提示: 'var' 不是有效关键字，请使用 'let' 声明变量";
        return "提示: 冒号位置不正确，请检查语法格式";
    }
    
    // 词法错误（ERR token 表示词法分析器遇到错误）
    if (errorMsg.find("ERR") != std::string::npos)
        return "提示: 存在词法错误，请检查字符串或字符是否正确闭合";
    
    // 字符串插值错误
    if (errorMsg.find("插值") != std::string::npos || errorMsg.find("${}") != std::string::npos)
        return "提示: 字符串插值语法错误，格式为 \"text ${expression} text\"";
    
    return "";
}

// 根据语义错误生成修复建议
std::string generateSemanticHint(const std::string& message, int line) {
    // 提取引号中的名称
    auto extractName = [](const std::string& msg) -> std::string {
        size_t start = msg.find("'");
        size_t end = msg.rfind("'");
        if (start != std::string::npos && end != std::string::npos && start < end)
            return msg.substr(start + 1, end - start - 1);
        return "";
    };
    
    if (message.find("Undefined variable") != std::string::npos) {
        std::string name = extractName(message);
        if (!name.empty())
            return "提示: 变量 '" + name + "' 未声明，请先使用 'let " + name + ": 类型 = 值' 声明";
        return "提示: 请检查变量是否已声明，注意拼写是否正确";
    }
    
    if (message.find("Undefined function") != std::string::npos) {
        std::string name = extractName(message);
        if (!name.empty())
            return "提示: 函数 '" + name + "' 未定义，请先使用 'func " + name + "() { }' 定义";
        return "提示: 请检查函数是否已定义，注意拼写是否正确";
    }
    
    if (message.find("Type mismatch") != std::string::npos || 
        message.find("type mismatch") != std::string::npos)
        return "提示: 请检查赋值和运算两侧的类型是否一致";
    
    if (message.find("already defined") != std::string::npos ||
        message.find("redefinition") != std::string::npos)
        return "提示: 该标识符已在当前作用域中定义，请使用不同的名称";
    
    if (message.find("argument") != std::string::npos && 
        (message.find("expected") != std::string::npos || message.find("too") != std::string::npos))
        return "提示: 请检查函数调用时传入的参数数量是否正确";
    
    if (message.find("Division by zero") != std::string::npos || 
        message.find("division by zero") != std::string::npos ||
        message.find("Modulo by zero") != std::string::npos)
        return "提示: 除数不能为0，请检查除法和取模运算的右侧表达式";
    
    if (message.find("out of bounds") != std::string::npos)
        return "提示: 数组索引超出范围，请确保索引值在 0 到 (数组长度-1) 之间";
    if (message.find("Array index must be integer") != std::string::npos)
        return "提示: 数组索引必须是整数类型，不能使用浮点数作为索引";
    
    // 常量重新赋值
    if (message.find("Cannot reassign") != std::string::npos || 
        message.find("reassign") != std::string::npos)
        return "提示: 常量一旦赋值就不能修改，如需修改请使用 'let' 声明变量";
    
    // void 函数返回值
    if (message.find("void") != std::string::npos && message.find("return") != std::string::npos)
        return "提示: 函数缺少返回类型声明，请在函数参数列表后添加 ': 返回类型'（如 `: int`）";
    
    // 数组大小不匹配
    if (message.find("Array size") != std::string::npos)
        return "提示: 数组声明的大小与初始化元素数量不一致";
    
    // 未定义数组
    if (message.find("Undefined array") != std::string::npos)
        return "提示: 请先声明数组变量";
    
    // 缺少 main 函数
    if (message.find("main") != std::string::npos && message.find("defined") != std::string::npos)
        return "提示: 程序需要一个 'main' 函数作为入口点";
    
    // break/continue 不在循环中
    if (message.find("break") != std::string::npos && message.find("loop") != std::string::npos)
        return "提示: 'break' 语句只能在循环或 switch 语句中使用";
    if (message.find("continue") != std::string::npos && message.find("loop") != std::string::npos)
        return "提示: 'continue' 语句只能在循环语句中使用";
    
    // 参数数量错误
    if (message.find("expects") != std::string::npos || message.find("argument") != std::string::npos)
        return "提示: 请检查函数调用时传入的参数数量";
    
    return "";
}

// 错误报告输出

// 报告语义错误（带源代码上下文和修复建议）
void reportError(const std::string& message, int line, int column) {
    (void)column;  // 不再使用列号
    
    // 翻译错误消息为中文
    std::string translatedMsg = translateSemanticError(message);
    
    // 输出位置信息（显示行号）
    std::cerr << ErrorColors::BOLD;
    if (!g_sourceFilePath.empty()) std::cerr << g_sourceFilePath << ":";
    if (line > 0) std::cerr << line << ": ";
    
    // 输出错误信息
    std::cerr << ErrorColors::RED << "error: " << ErrorColors::RESET;
    std::cerr << ErrorColors::BOLD << translatedMsg << ErrorColors::RESET << std::endl;
    
    // 显示上下文（不带列指向）
    displaySourceContext(line, 0, true);
    std::string hint = generateSemanticHint(message, line);
    if (!hint.empty())
        std::cerr << ErrorColors::CYAN << hint << ErrorColors::RESET << std::endl;
    
    std::cerr << std::endl;
    g_errorCount++;
}

// 报告警告信息
void reportWarning(const std::string& message, int line, int column) {
    (void)column;  // 不再使用列号
    
    if (g_suppressWarnings) return;
    if (g_warningsAsErrors) { reportError(message, line, column); return; }
    
    // 翻译警告消息为中文
    std::string translatedMsg = translateSemanticError(message);
    
    // 输出位置信息（只显示行号）
    std::cerr << ErrorColors::BOLD;
    if (!g_sourceFilePath.empty()) std::cerr << g_sourceFilePath << ":";
    if (line > 0) std::cerr << line << ": ";
    
    // 输出警告信息
    std::cerr << ErrorColors::YELLOW << "warning: " << ErrorColors::RESET;
    std::cerr << translatedMsg << std::endl;
    
    // 显示简化的上下文
    if (line > 0 && !g_sourceLines.empty()) {
        std::string srcLine = getSourceLine(line);
        if (!srcLine.empty()) {
            std::cerr << "    " << ErrorColors::CYAN << std::setw(4) << line 
                      << " | " << ErrorColors::RESET << srcLine << std::endl;
        }
    }
    
    g_warningCount++;
}

// 报告语法错误（由 Bison 解析器调用）
void reportSyntaxError(const char* msg, int line, int column) {
    (void)column;  // 不再使用列号
    g_syntaxErrorCount++;
    
    std::string friendlyMsg = translateErrorMessage(msg);
    int errorLine = line;
    
    // 对于缺少符号的错误，判断是否应该指向上一行
    if (line > 1) {
        bool shouldPointToPrev = false;
        
        // 检查是否遇到了意外的闭合括号（多余的括号，保持当前行）
        bool hasUnexpectedClosing = 
            friendlyMsg.find("意外的 ')'") != std::string::npos ||
            friendlyMsg.find("意外的 '}'") != std::string::npos ||
            friendlyMsg.find("意外的 ']'") != std::string::npos;
        
        if (!hasUnexpectedClosing) {
            // 期望闭合括号（上一行缺少）
            if (friendlyMsg.find("期望 ')'") != std::string::npos ||
                friendlyMsg.find("期望 '}'") != std::string::npos ||
                friendlyMsg.find("期望 ']'") != std::string::npos) {
                shouldPointToPrev = true;
            }
            // 期望左花括号（函数/控制语句缺少 {）
            if (friendlyMsg.find("期望 '{'") != std::string::npos) {
                shouldPointToPrev = true;
            }
        }
        
        if (shouldPointToPrev) {
            errorLine = line - 1;
        }
    }
    
    // 输出位置和错误信息（只显示行号）
    std::cerr << ErrorColors::BOLD;
    if (!g_sourceFilePath.empty()) std::cerr << g_sourceFilePath << ":";
    std::cerr << errorLine << ": ";
    std::cerr << ErrorColors::RED << "error: " << ErrorColors::RESET;
    std::cerr << ErrorColors::BOLD << friendlyMsg << ErrorColors::RESET << std::endl;
    std::cerr << std::endl;
    
    // 显示上下文（高亮错误行）
    int startLine = std::max(1, errorLine - 2);
    int endLine = errorLine + 1;
    for (int i = startLine; i <= endLine; i++) {
        std::string srcLine = getSourceLine(i);
        if (srcLine.empty() && i > errorLine) break;
        if (i == errorLine) {
            std::cerr << ErrorColors::RED << " >> " << ErrorColors::RESET;
            std::cerr << ErrorColors::CYAN << std::setw(4) << i << " | " << ErrorColors::RESET;
            std::cerr << ErrorColors::BOLD << srcLine << ErrorColors::RESET << std::endl;
        } else {
            std::cerr << "    " << ErrorColors::CYAN << std::setw(4) << i 
                      << " | " << ErrorColors::RESET << srcLine << std::endl;
        }
    }
    
    // 生成修复建议
    std::string hint = generateSyntaxHint(friendlyMsg, errorLine);
    if (!hint.empty())
        std::cerr << ErrorColors::CYAN << hint << ErrorColors::RESET << std::endl;
    std::cerr << std::endl;
}

// 启用所有警告选项
void enableAllWarnings() {
    g_enableAllWarnings = true;
    g_enableUnusedWarnings = true;
    g_enableDeadCodeWarnings = true;
    g_enableMissingReturnWarnings = true;
    g_enableShadowWarnings = true;
}

// 将警告视为错误
void setWarningsAsErrors(bool enable) {
    g_warningsAsErrors = enable;
}

// 禁用所有警告
void suppressAllWarnings() {
    g_suppressWarnings = true;
    g_enableUnusedWarnings = false;
    g_enableDeadCodeWarnings = false;
    g_enableMissingReturnWarnings = false;
    g_enableShadowWarnings = false;
}

// 检查警告是否启用
bool isWarningEnabled() {
    return !g_suppressWarnings;
}

// 设置警告选项（命令行参数处理）
void setWarningOption(const std::string& option) {
    if (option == "all")             enableAllWarnings();
    else if (option == "error")      setWarningsAsErrors(true);
    else if (option == "no-unused")  g_enableUnusedWarnings = false;
    else if (option == "unused")     g_enableUnusedWarnings = true;
    else if (option == "no-dead-code")      g_enableDeadCodeWarnings = false;
    else if (option == "dead-code")         g_enableDeadCodeWarnings = true;
    else if (option == "no-missing-return") g_enableMissingReturnWarnings = false;
    else if (option == "missing-return")    g_enableMissingReturnWarnings = true;
    else if (option == "shadow")     g_enableShadowWarnings = true;
    else if (option == "no-shadow")  g_enableShadowWarnings = false;
}
