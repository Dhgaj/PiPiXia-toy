/**
 * main.cc
 * PiPiXia 编译器主程序
 * 负责命令行参数解析、词法分析、语法分析、AST构建和LLVM代码生成的完整编译流程
 * 
 * 编译流程：
 * 1. 命令行参数解析
 * 2. 词法分析
 * 3. 语法分析和AST构建
 * 4. LLVM IR代码生成
 * 5. 生成可执行文件、目标文件或LLVM IR文件
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <cstdio>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>
#include "node.h"
#include "codegen.h"
#include "error.h"
#include "syntax.hh"

// 全局日志控制变量，控制是否输出详细的编译过程信息
bool g_verbose = false;

// 来自 Flex/Bison 的外部声明
extern FILE* yyin;                                  // Flex 输入文件指针
extern int yyparse();                               // Bison 语法分析函数
extern int yylex();                                 // Flex 词法分析函数
extern std::shared_ptr<ProgramNode> root;           // AST 根节点
extern int yylineno;                                // 当前行号
extern union YYSTYPE yylval;                        // Token 值

// 来自 error.h 的源文件加载函数和错误计数器
// loadSourceFile 和 g_syntaxErrorCount 已在 error.h 中声明

// 文件名处理辅助函数：更改文件扩展名
std::string changeExtension(const std::string& filename, const std::string& newExt) {
    size_t dotPos = filename.find_last_of('.');
    std::string base = (dotPos != std::string::npos) ? 
        filename.substr(0, dotPos) : filename;
    return newExt.empty() ? base : base + newExt;
}

// Token名称映射函数
const char* getTokenName(int token) {
    switch(token) {
        // 字面量
        case INT_LITERAL: return "INT_LITERAL";
        case DOUBLE_LITERAL: return "DOUBLE_LITERAL";
        case STRING_LITERAL: return "STRING_LITERAL";
        case CHAR_LITERAL: return "CHAR_LITERAL";
        case BOOL_LITERAL: return "BOOL_LITERAL";
        case IDENTIFIER: return "IDENTIFIER";
        case TYPE: return "TYPE";
        
        // 关键字
        case LET: return "LET";
        case CONST: return "CONST";
        case FUNC: return "FUNC";
        case RETURN: return "RETURN";
        case IF: return "IF";
        case ELSE: return "ELSE";
        case WHILE: return "WHILE";
        case FOR: return "FOR";
        case IN: return "IN";
        case BREAK: return "BREAK";
        case CONTINUE: return "CONTINUE";
        case SWITCH: return "SWITCH";
        case CASE: return "CASE";
        case DEFAULT: return "DEFAULT";
        case IMPORT: return "IMPORT";
        case AS: return "AS";
        case TRY: return "TRY";
        case CATCH: return "CATCH";
        case THROW: return "THROW";
        
        // 运算符
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        case MULTIPLY: return "MULTIPLY";
        case DIVIDE: return "DIVIDE";
        case FLOORDIV: return "FLOORDIV";
        case MODULO: return "MODULO";
        case EQ: return "EQ";
        case NE: return "NE";
        case LT: return "LT";
        case GT: return "GT";
        case LE: return "LE";
        case GE: return "GE";
        case AND: return "AND";
        case OR: return "OR";
        case NOT: return "NOT";
        case ASSIGN: return "ASSIGN";
        case PLUS_ASSIGN: return "PLUS_ASSIGN";
        case MINUS_ASSIGN: return "MINUS_ASSIGN";
        case MULT_ASSIGN: return "MULT_ASSIGN";
        case DIV_ASSIGN: return "DIV_ASSIGN";
        case FLOORDIV_ASSIGN: return "FLOORDIV_ASSIGN";
        case MOD_ASSIGN: return "MOD_ASSIGN";
        
        // 分隔符
        case LPAREN: return "LPAREN";
        case RPAREN: return "RPAREN";
        case LBRACE: return "LBRACE";
        case RBRACE: return "RBRACE";
        case LBRACKET: return "LBRACKET";
        case RBRACKET: return "RBRACKET";
        case COMMA: return "COMMA";
        case COLON: return "COLON";
        case SEMICOLON: return "SEMICOLON";
        case DOT: return "DOT";
        case DOTDOT: return "DOTDOT";
        
        // 特殊
        case ERROR: return "ERROR";
        case 0: return "EOF";
        
        // 最后匹配未知token
        default: return "UNKNOWN";
    }
}

// 对源文件进行词法分析，生成Token
bool tokenizeFile(const std::string& inputFile, const std::string& outputFile = "") {
    std::ofstream outFile;
    
    // 打开文件（如果指定了输出文件）
    if (!outputFile.empty()) {
        outFile.open(outputFile);
        if (!outFile.is_open()) {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": Cannot create output file '" << outputFile << "'" << std::endl;
            return false;
        }
    }
    
    // 输出表头（到控制台和文件）
    auto printHeader = [&]() {
        std::cout << "=== Token Analysis ===" << std::endl;
        std::cout << "Source: " << inputFile << std::endl;
        std::cout << std::endl;
        std::cout << std::left << std::setw(8) << "Line" 
                  << std::setw(20) << "Token Type" 
                  << "Value" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        if (outFile.is_open()) {
            outFile << "=== Token Analysis ===" << std::endl;
            outFile << "Source: " << inputFile << std::endl;
            outFile << std::endl;
            outFile << std::left << std::setw(8) << "Line" 
                    << std::setw(20) << "Token Type" 
                    << "Value" << std::endl;
            outFile << std::string(50, '-') << std::endl;
        }
    };
    
    printHeader();
    
    int token;
    int tokenCount = 0;
    
    while ((token = yylex()) != 0) {
        tokenCount++;
        
        // 准备输出行
        std::stringstream line;
        line << std::left << std::setw(8) << yylineno
             << std::setw(20) << getTokenName(token);
        
        // 输出token值
        switch(token) {
            case INT_LITERAL:
                line << yylval.intVal;
                break;
            case DOUBLE_LITERAL:
                line << yylval.doubleVal;
                break;
            case STRING_LITERAL:
            case IDENTIFIER:
            case TYPE:
                if (yylval.strVal) {
                    line << "\"" << *yylval.strVal << "\"";
                }
                break;
            case CHAR_LITERAL:
                line << "'" << yylval.charVal << "'";
                break;
            case BOOL_LITERAL:
                line << (yylval.boolVal ? "true" : "false");
                break;
            case ERROR:
                line << "<lexical error>";
                break;
            default:
                // 运算符和分隔符通常不需要显示值
                break;
        }
        
        // 同时输出到控制台和文件
        std::cout << line.str() << std::endl;
        if (outFile.is_open()) {
            outFile << line.str() << std::endl;
        }
        
        if (token == ERROR) {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": Lexical analysis stopped due to error" << std::endl;
            return false;
        }
    }
    
    // 输出总结
    std::cout << std::string(30, '-') << std::endl;
    std::cout << "Total tokens: " << tokenCount << std::endl;
    
    if (outFile.is_open()) {
        outFile << std::string(30, '-') << std::endl;
        outFile << "Total tokens: " << tokenCount << std::endl;
        outFile.close();
        std::cout << "\nToken analysis written to: " << outputFile << std::endl;
    }
    
    return true;
}

// 打印编译器使用帮助信息
void printUsage(const char* programName) {
    std::cout << "PiPiXia Language Compiler" << std::endl;
    std::cout << "用法: " << programName << " <输入文件.ppx> [选项]" << std::endl;
    std::cout << "\n选项:" << std::endl;
    std::cout << "  -o <输出>      指定输出文件名" << std::endl;
    std::cout << "  -tokens        输出词法分析结果（.tokens），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -tokens -o <目录/文件.tokens> 指定输出路径" << std::endl;
    std::cout << "  -ast           输出抽象语法树（.ast），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -ast -o <目录/文件.ast> 指定输出路径" << std::endl;
    std::cout << "  -symbols       输出符号表（.symbols），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -symbols -o <目录/文件.symbols> 指定输出路径" << std::endl;
    std::cout << "  -tac           输出三地址码（.tac），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -tac -o <目录/文件.tac> 指定输出路径" << std::endl;
    std::cout << "  -llvm          输出 LLVM IR 文件（.ll），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -llvm -o <目录/文件.ll> 指定输出路径" << std::endl;
    std::cout << "  -c             输出目标文件（.o），不生成可执行文件" << std::endl;
    std::cout << "                 可使用 -c -o <目录/文件.o> 指定输出路径" << std::endl;
    std::cout << "  -v, --verbose  启用详细日志 (AST 解析和 IR 生成)" << std::endl;
    std::cout << "  -Wall          启用所有警告" << std::endl;
    std::cout << "  -Werror        将警告视为错误" << std::endl;
    std::cout << "  -w             禁用所有警告" << std::endl;
    std::cout << "  -Wno-unused    禁用未使用变量警告" << std::endl;
    std::cout << "  -Wshadow       启用变量遮蔽警告" << std::endl;
    std::cout << "  -h, --help     显示此帮助信息" << std::endl;
    std::cout << "\n示例:" << std::endl;
    std::cout << "  " << programName << " code/main.ppx                     # 编译到可执行文件 main" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -o myapp            # 编译到可执行文件 myapp" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -tokens             # 生成 tokens 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -tokens -o my.tok   # 生成 my.tok 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -ast                # 生成 AST 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -ast -o my.ast      # 生成 my.ast 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -symbols            # 生成符号表文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -symbols -o my.sym  # 生成 my.sym 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -tac                # 生成三地址码文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -tac -o my.tac      # 生成 my.tac 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -llvm               # 打印 LLVM IR 到控制台" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -llvm -o my.ll      # 生成 my.ll 文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -c                  # 生成目标文件" << std::endl;
    std::cout << "  " << programName << " code/main.ppx -c -o myobj.o       # 生成 myobj.o 文件" << std::endl;
}

// 主函数
int main(int argc, char** argv) {
    // 检查命令行参数
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // 变量声明
    std::string inputFile;              // 输入文件路径
    std::string outputFile;             // 输出文件路径
    bool printTokens = false;           // 是否生成Token文件
    bool printAST = false;              // 是否生成AST文件
    bool printSymbols = false;          // 是否生成符号表
    bool printTAC = false;              // 是否生成三地址码
    bool generateLLVM = false;          // 是否生成LLVM IR
    bool emitLLVM = false;              // 是否输出LLVM IR到文件
    bool compileToObj = false;          // 是否生成目标文件(.o)
    bool compileToExe = false;          // 是否生成可执行文件

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-tokens") {
            printTokens = true;
        } else if (arg == "-ast") {
            printAST = true;
        } else if (arg == "-symbols") {
            printSymbols = true;
        } else if (arg == "-tac") {
            printTAC = true;
        } else if (arg == "-llvm") {
            generateLLVM = true;
            emitLLVM = false; 
        } else if (arg == "-c") {
            compileToObj = true;
        } else if (arg == "-v" || arg == "--verbose") {
            g_verbose = true;
        } else if (arg == "-Wall") {
            enableAllWarnings();
        } else if (arg == "-Werror") {
            setWarningsAsErrors(true);
        } else if (arg == "-w") {
            suppressAllWarnings();
        } else if (arg.substr(0, 2) == "-W" && arg.length() > 2) {
            // 解析 -Wxx 选项
            setWarningOption(arg.substr(2));
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": -o 选项需要指定输出文件名" << std::endl;
                return 1;
            }
        } else if (arg[0] != '-') {
            inputFile = arg;
        } else {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": 未知选项 '" << arg << "'" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": 未指定输入文件" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // 编译模式设置
    
    // Token分析模式：只进行词法分析，不进行后续的语法分析和代码生成
    if (printTokens) {
        // Token分析模式不进行语法分析和代码生成
        compileToExe = false;
        compileToObj = false;
        generateLLVM = false;
        emitLLVM = false;
    } else if (printAST && !printSymbols && !printTAC && !generateLLVM && !emitLLVM && !compileToObj) {
        // AST模式：只打印AST，不生成可执行文件
        compileToExe = false;
        compileToObj = false;
        generateLLVM = false;
        emitLLVM = false;
    } else if (printSymbols || printTAC) {
        // 符号表或三地址码模式：需要生成IR但不编译成可执行文件
        generateLLVM = true;
        compileToExe = false;
        compileToObj = false;
    } else if (compileToObj) {
        // 目标文件模式：生成.o文件
        compileToExe = false;
        generateLLVM = true;  // 生成目标文件也需要LLVM IR
    } else {
        // 默认行为：如果没有指定 -llvm 或 -c，就编译到可执行文件
        if (!generateLLVM) {
            compileToExe = true;
            generateLLVM = true;  // 编译到可执行文件也需要生成 LLVM IR
        }
    }

    // 检查输入文件是否有.ppx扩展名
    if (inputFile.length() < 4 || inputFile.substr(inputFile.length() - 4) != ".ppx") {
        std::cerr << ErrorColors::YELLOW << "Warning" << ErrorColors::RESET << ": 输入文件应使用 .ppx 扩展名" << std::endl;
    }

    // 文件打开和初始化
    
    // 打开输入文件
    FILE* file = fopen(inputFile.c_str(), "r");
    if (!file) {
        std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": 无法打开文件 '" << inputFile << "'" << std::endl;
        return 1;
    }

    std::cout << "=== PiPiXia Compiler ===" << std::endl;
    std::cout << "Compiling: " << inputFile << std::endl;
    if (g_verbose) {
        std::cout << "Verbose mode: ENABLED" << std::endl;
    }
    std::cout << std::endl;

    // 为词法分析器设置输入
    yyin = file;
    yylineno = 1;
    
    // 加载源文件内容用于错误报告
    loadSourceFile(inputFile);

    // 在verbose模式下，先进行词法分析统计
    if (g_verbose && !printTokens) {
        std::cout << "=== Lexical Analysis Phase ===" << std::endl;
        
        int token;
        int tokenCount = 0;
        std::map<std::string, int> tokenStats;
        
        while ((token = yylex()) != 0) {
            tokenCount++;
            std::string tokenName = getTokenName(token);
            tokenStats[tokenName]++;
        }
        
        std::cout << "[Lexical] Scanned " << tokenCount << " tokens" << std::endl;
        std::cout << "[Lexical] Token types found: " << tokenStats.size() << std::endl;
        
        // 显示token类型统计（前10个最常见的）
        std::vector<std::pair<std::string, int>> sortedStats(tokenStats.begin(), tokenStats.end());
        std::sort(sortedStats.begin(), sortedStats.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << "[Lexical] Most common tokens:" << std::endl;
        int count = 0;
        for (const auto& stat : sortedStats) {
            if (count++ >= 5) break;
            std::cout << "          " << std::left << std::setw(20) << stat.first 
                     << " x " << stat.second << std::endl;
        }
        std::cout << std::endl;
        
        // 重新打开文件进行语法分析
        fclose(file);
        file = fopen(inputFile.c_str(), "r");
        if (!file) {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": 无法重新打开文件进行语法分析" << std::endl;
            return 1;
        }
        yyin = file;
        yylineno = 1;
    }

    // Token分析模式
    if (printTokens) {
        std::string tokenOutput;
        if (!outputFile.empty()) {
            tokenOutput = outputFile;
        } else {
            tokenOutput = changeExtension(inputFile, ".tokens");
        }
        
        bool success = tokenizeFile(inputFile, tokenOutput);
        fclose(file);
        
        std::cout << "\nLexical analysis completed successfully!" << std::endl;
        
        if (success) {
            std::cout << "\n=== Compilation Summary ===" << std::endl;
            std::cout << "Status: SUCCESS" << std::endl;
            std::cout << "Input:  " << inputFile << std::endl;
            std::cout << "Output: " << tokenOutput << std::endl;
        } else {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": Token analysis failed" << std::endl;
            return 1;
        }
        
        return 0;
    }

    // 语法分析和AST构建
    
    if (g_verbose) {
        std::cout << "=== AST Parsing Phase ===" << std::endl;
    }
    
    // 调用Bison生成的解析器进行语法分析
    int parseResult = yyparse();
    fclose(file);

    if (parseResult != 0 || g_syntaxErrorCount > 0) {
        std::cerr << "\nCompilation failed with " << g_syntaxErrorCount << " syntax error(s)." << std::endl;
        return 1;
    }

    std::cout << "Parsing completed successfully!" << std::endl;

    // 编译状态标志
    bool compilationFailed = false;

    // AST输出（可选）
    
    // 如果用户指定了-ast选项，打印AST到控制台
    if (printAST && root) {
        std::cout << "\n=== Abstract Syntax Tree ===" << std::endl;
        std::cout << "Source: " << inputFile << std::endl;
        std::cout << std::endl;
        root->print(0);
        std::cout << std::endl;
    }

    // AST文件输出
    // 如果用户指定了-ast但未指定输出文件名，自动生成文件名
    if (printAST && outputFile.empty() && !emitLLVM && !compileToExe) {
        outputFile = changeExtension(inputFile, ".ast");
    }

    // 将AST内容写入文件（仅当用户指定了-ast选项时）
    if (printAST) {
        std::string astOutput;
        if (!outputFile.empty() && !compileToExe && !emitLLVM) {
            astOutput = outputFile;
        } else {
            astOutput = changeExtension(inputFile, ".ast");
        }
            
        std::ofstream outFile(astOutput);
        if (outFile.is_open()) {
            std::streambuf* coutBuf = std::cout.rdbuf();
            std::cout.rdbuf(outFile.rdbuf());
            
            if (root) {
                std::cout << "=== PiPiXia AST Output ===" << std::endl;
                std::cout << "Source: " << inputFile << std::endl;
                std::cout << std::endl;
                root->print(0);
            }
            
            std::cout.rdbuf(coutBuf);
            outFile.close();
            
            std::cout << "AST written to: " << astOutput << std::endl;
        }
    }

    // 程序验证
    
    // 检查程序中是否定义了main函数
    bool hasMain = false;
    if (root) {
        for (const auto& stmt : root->statements) {
            if (auto funcDecl = std::dynamic_pointer_cast<FunctionDeclNode>(stmt)) {
                if (funcDecl->name == "main") {
                    hasMain = true;
                    break;
                }
            }
        }
    }

    if (!hasMain) {
        std::cerr << ErrorColors::YELLOW << "Warning" << ErrorColors::RESET << ": 程序中未找到 'main' 函数" << std::endl;
    }

    // LLVM模式检查 
    // 如果使用-llvm参数且指定了输出文件，则输出到文件而非控制台
    if (generateLLVM && !compileToExe && !compileToObj && !outputFile.empty()) {
        emitLLVM = true;
    }

    // LLVM IR 代码生成
    if (generateLLVM && root) {
        std::cout << "\n=== LLVM Code Generation ===" << std::endl;
        
        // 设置源文件路径（用于语义错误报告显示源代码上下文）
        setSourceFilePath(inputFile);
        
        CodeGenerator codegen(inputFile);
        
        // 设置源文件目录（用于import查找模块）
        size_t lastSlash = inputFile.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string sourceDir = inputFile.substr(0, lastSlash);
            codegen.setSourceDirectory(sourceDir);
        }
        
        // 生成LLVM IR代码
        if (codegen.generate(root.get())) {
            std::cout << "LLVM IR generation successful!" << std::endl;
            
            // 可执行文件生成
            if (compileToExe) {
                // 编译为可执行文件（默认模式）
                std::cout << "\n=== Compiling to Executable ===" << std::endl;
                
                std::string exeOutput;
                if (!outputFile.empty()) {
                    exeOutput = outputFile;
                } else {
                    // 默认可执行文件名：去掉 .ppx 扩展名
                    exeOutput = changeExtension(inputFile, "");
                }
                
                if (codegen.compileToExecutable(exeOutput)) {
                    std::cout << "Executable generated: " << exeOutput << std::endl;
                } else {
                    std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": Failed to generate executable" << std::endl;
                }
            // 目标文件生成
            } else if (compileToObj) {
                // 生成目标文件（.o）用于链接
                std::cout << "\n=== Generating Object File ===" << std::endl;
                
                std::string objOutput;
                if (!outputFile.empty()) {
                    objOutput = outputFile;
                } else {
                    objOutput = changeExtension(inputFile, ".o");
                }
                
                if (codegen.compileToObjectFile(objOutput)) {
                    std::cout << "Object file generated: " << objOutput << std::endl;
                } else {
                    std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": Failed to generate object file" << std::endl;
                }
            // LLVM IR输出（既显示又保存）
            } else if (printSymbols) {
                // 符号表输出
                std::cout << "\n=== Symbol Table Generation ===" << std::endl;
                codegen.printSymbolTable();
                
                // 保存符号表到文件
                std::string symbolsOutput;
                if (!outputFile.empty()) {
                    symbolsOutput = outputFile;
                } else {
                    symbolsOutput = changeExtension(inputFile, ".symbols");
                }
                
                if (codegen.writeSymbolTableToFile(symbolsOutput)) {
                    std::cout << "\nSymbol table written to: " << symbolsOutput << std::endl;
                }
            } else if (printTAC) {
                // 三地址码输出
                std::cout << "\n=== Three Address Code Generation ===" << std::endl;
                codegen.printThreeAddressCode();
                
                // 保存三地址码到文件
                std::string tacOutput;
                if (!outputFile.empty()) {
                    tacOutput = outputFile;
                } else {
                    tacOutput = changeExtension(inputFile, ".tac");
                }
                
                if (codegen.writeThreeAddressCodeToFile(tacOutput)) {
                    std::cout << "\nThree address code written to: " << tacOutput << std::endl;
                }
            } else {
                // 打印到控制台
                std::cout << "\n=== LLVM IR ===" << std::endl;
                codegen.printIR();
                
                // 同时保存到文件
                std::string llvmOutput;
                if (!outputFile.empty()) {
                    llvmOutput = outputFile;
                } else {
                    llvmOutput = changeExtension(inputFile, ".ll");
                }
                
                if (codegen.writeIRToFile(llvmOutput)) {
                    std::cout << "\nLLVM IR written to: " << llvmOutput << std::endl;
                }
            }
        } else {
            std::cerr << ErrorColors::RED << "Error" << ErrorColors::RESET << ": LLVM IR generation failed" << std::endl;
            compilationFailed = true;
        }
    }

    // 编译结果总结
    std::cout << "\n=== Compilation Summary ===" << std::endl;
    if (compilationFailed) {
        std::cout << "Status: FAILED" << std::endl;
    } else {
        std::cout << "Status: SUCCESS" << std::endl;
    }
    std::cout << "Input:  " << inputFile << std::endl;
    
    if (compileToExe) {
        std::string exeFile;
        if (!outputFile.empty()) {
            exeFile = outputFile;
        } else {
            exeFile = changeExtension(inputFile, "");
        }
        std::cout << "Output: " << exeFile << " (executable)" << std::endl;
    } else if (compileToObj) {
        std::string objFile;
        if (!outputFile.empty()) {
            objFile = outputFile;
        } else {
            objFile = changeExtension(inputFile, ".o");
        }
        std::cout << "Output: " << objFile << " (object file)" << std::endl;
    } else if (printSymbols) {
        // -symbols 模式
        std::string symbolsFile;
        if (!outputFile.empty()) {
            symbolsFile = outputFile;
        } else {
            symbolsFile = changeExtension(inputFile, ".symbols");
        }
        std::cout << "Output: " << symbolsFile << " (Symbol Table)" << std::endl;
    } else if (printTAC) {
        // -tac 模式
        std::string tacFile;
        if (!outputFile.empty()) {
            tacFile = outputFile;
        } else {
            tacFile = changeExtension(inputFile, ".tac");
        }
        std::cout << "Output: " << tacFile << " (Three Address Code)" << std::endl;
    } else if (generateLLVM && !compileToExe && !compileToObj) {
        // -llvm 模式
        std::string llvmFile;
        if (!outputFile.empty()) {
            llvmFile = outputFile;
        } else {
            llvmFile = changeExtension(inputFile, ".ll");
        }
        std::cout << "Output: " << llvmFile << " (LLVM IR)" << std::endl;
    } else if (printAST) {
        // -ast 模式
        std::string astFile;
        if (!outputFile.empty()) {
            astFile = outputFile;
        } else {
            astFile = changeExtension(inputFile, ".ast");
        }
        std::cout << "Output: " << astFile << " (AST)" << std::endl;
    }
    
    if (root) {
        std::cout << "Statements parsed: " << root->statements.size() << std::endl;
    }
    
    // 返回编译结果
    if (compilationFailed) {
        return 1;
    }
    
    return 0;
}
