/**
 * error.h
 * PiPiXia 编译器错误处理模块
 * 
 * 功能：
 * - 统一的错误和警告报告机制
 * - 源代码上下文显示
 * - 错误信息翻译和修复建议
 * - 警告级别控制 (-Wall, -Werror, -w)
 */

#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <vector>

/**
 * ANSI 终端颜色码
 * 用于彩色输出错误和警告信息
 */
namespace ErrorColors {
    extern const char* RED;       // 错误（粗体红色）
    extern const char* YELLOW;    // 警告（粗体黄色）
    extern const char* CYAN;      // 提示信息（青色）
    extern const char* BOLD;      // 粗体
    extern const char* RESET;     // 重置颜色
}

/**
 * 源代码管理
 * 用于错误报告时显示源代码上下文
 */
extern std::vector<std::string> g_sourceLines;    // 源代码行缓存
extern std::string g_sourceFilePath;              // 当前源文件路径

void loadSourceFile(const std::string& filename); // 加载源文件到缓存
std::string getSourceLine(int lineNum);           // 获取指定行源代码

/**
 * 错误统计计数器
 */
extern int g_errorCount;                          // 错误计数
extern int g_warningCount;                        // 警告计数
extern int g_syntaxErrorCount;                    // 语法错误计数

void resetErrorCounts();                          // 重置所有计数器

/**
 * 警告控制选项
 * 通过命令行参数控制警告行为
 */
extern bool g_enableAllWarnings;                  // -Wall: 启用所有警告
extern bool g_warningsAsErrors;                   // -Werror: 将警告视为错误
extern bool g_suppressWarnings;                   // -w: 禁用所有警告

// 具体警告类型开关
extern bool g_enableUnusedWarnings;               // 未使用变量/参数警告
extern bool g_enableDeadCodeWarnings;             // 死代码警告
extern bool g_enableMissingReturnWarnings;        // 缺少返回值警告
extern bool g_enableShadowWarnings;               // 变量遮蔽警告

// 警告控制函数
void enableAllWarnings();                         // 启用所有警告 (-Wall)
void setWarningsAsErrors(bool enable);            // 设置警告视为错误 (-Werror)
void suppressAllWarnings();                       // 禁用所有警告 (-w)
void setWarningOption(const std::string& option); // 解析 -Wxx 选项
bool isWarningEnabled();                          // 检查是否启用警告输出

/**
 * 错误报告函数
 * 统一的错误和警告输出接口
 */
void reportError(const std::string& message, int line, int column = 0);
void reportWarning(const std::string& message, int line, int column = 0);
void reportSyntaxError(const char* msg, int line, int column);

/**
 * 错误信息处理
 * 翻译和生成修复建议
 */
std::string translateErrorMessage(const char* msg);                         // Bison错误信息翻译
std::string translateSemanticError(const std::string& msg);                 // 语义错误翻译
std::string generateSyntaxHint(const std::string& errorMsg, int errorLine); // 语法错误提示
std::string generateSemanticHint(const std::string& message, int line);     // 语义错误提示

#endif // ERROR_H
