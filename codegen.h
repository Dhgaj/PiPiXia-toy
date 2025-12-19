#ifndef CODEGEN_H
#define CODEGEN_H

#include <map>
#include <set>
#include <string>
#include <memory>

#include "node.h"
#include "error.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>

// 外部变量声明
extern bool g_verbose;  // 全局日志控制变量

// 设置源文件路径（用于错误报告显示源代码上下文）
void setSourceFilePath(const std::string& path);

// 常量定义
namespace CodeGenConstants {
    // 缓冲区大小配置
    const size_t INPUT_BUFFER_SIZE = 256;           // input() 函数输入缓冲区
    const size_t STRING_CONVERT_BUFFER_SIZE = 64;   // 数值转字符串缓冲区
    const size_t EXCEPTION_MSG_BUFFER_SIZE = 256;   // 异常消息缓冲区
    const size_t JMP_BUF_SIZE = 200;                // setjmp/longjmp 缓冲区
}

// LLVM 代码生成器类
class CodeGenerator {
private:
    // LLVM 核心组件
    std::unique_ptr<llvm::LLVMContext> context;                     // LLVM 上下文，管理类型和常量
    std::unique_ptr<llvm::Module> module;                           // LLVM 模块，包含所有函数和全局变量
    std::unique_ptr<llvm::IRBuilder<>> builder;                     // IR 构建器，用于生成 LLVM 指令
    
    // 编译状态（错误计数使用error.h中的全局变量）
    llvm::Function* currentFunction;                                // 当前正在编译的函数
    int currentFunctionLineNumber;                                  // 当前函数声明的行号
    
    // 符号表
    std::map<std::string, llvm::AllocaInst*> namedValues;          // 局部变量符号表
    std::map<std::string, llvm::GlobalVariable*> globalValues;      // 全局变量符号表
    std::map<std::string, llvm::Function*> functions;               // 函数符号表
    std::map<std::string, std::string> variableTypes;               // 变量类型映射表
    std::set<std::string> localConstVariables;                      // 局部常量变量集合
    std::set<std::string> failedDeclarations;                       // 声明失败的变量（用于抑制级联错误）
    std::set<std::string> usedVariables;                            // 已使用的变量（用于未使用变量警告）
    std::map<std::string, int> declaredVariables;                   // 已声明的变量及其行号
    
    // 全局变量动态初始化
    struct GlobalInitializer {
        llvm::GlobalVariable* variable;                             // 全局变量指针
        ExprNode* initializer;                                      // 初始化表达式
    };
    std::vector<GlobalInitializer> globalInitializers;              // 需要动态初始化的全局变量列表
    
    // 控制流上下文
    struct LoopContext {
        llvm::BasicBlock* continueBlock;                            // continue 跳转的目标块
        llvm::BasicBlock* breakBlock;                               // break 跳转的目标块
    };
    std::vector<LoopContext> loopContextStack;                      // 循环上下文栈（支持嵌套循环）
    std::vector<llvm::AllocaInst*> exceptionContextStack;           // 异常上下文栈（支持嵌套 try-catch）
    llvm::GlobalVariable* currentExceptionMsg;                      // 当前异常消息的全局变量
    
    // 内存管理
    std::vector<llvm::Value*> tempMemoryStack;                      // 临时内存栈（用于自动释放）
    std::map<std::string, llvm::Value*> ownedStringMemory;          // 变量拥有的动态字符串内存
    
    // 模块管理
    std::set<std::string> loadedModules;                            // 已加载的模块集合
    std::string currentDirectory;                                   // 当前工作目录
    std::string sourceDirectory;                                    // 源文件所在目录
    std::map<std::string, std::string> moduleAliases;               // 模块别名映射（import as）
    std::map<std::string, std::map<std::string, llvm::Function*>> moduleFunctions;       // 模块函数表
    std::map<std::string, std::map<std::string, llvm::GlobalVariable*>> moduleGlobals;   // 模块全局变量表
    
    // 类型系统
    llvm::Type* getType(const std::string& typeName);                               // 将类型名转换为 LLVM 类型
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function,              // 在函数入口块创建局部变量
                                              const std::string& varName,
                                              llvm::Type* type);
    
    // 内置函数
    void declareBuiltinFunctions();                                                 // 声明所有内置函数
    llvm::Function* getPrintfFunction();                                            // 获取 printf 函数
    llvm::Function* getScanfFunction();                                             // 获取 scanf 函数
    llvm::Function* getStrlenFunction();                                            // 获取 strlen 函数
    
    // 内存管理辅助函数
    void pushTempMemory(llvm::Value* ptr);                                          // 将指针加入临时内存栈
    void removeTempMemory(llvm::Value* ptr);                                        // 从临时内存栈移除指针
    void clearTempMemory();                                                         // 清理当前作用域的临时内存
    void trackOwnedString(const std::string& varName, llvm::Value* ptr);           // 跟踪变量拥有的字符串内存
    void freeOwnedString(const std::string& varName);                               // 释放变量拥有的字符串内存
    
    // 模块管理辅助函数
    std::string findModuleFile(const std::string& moduleName);                      // 查找模块文件路径
    bool loadModule(const std::string& moduleName);                                 // 加载模块
    llvm::Function* findModuleFunction(const std::string& moduleName, const std::string& funcName);        // 在模块中查找函数
    llvm::GlobalVariable* findModuleGlobal(const std::string& moduleName, const std::string& varName);     // 在模块中查找全局变量
    void codegenImport(ImportNode* node);                                           // 生成 import 语句代码
    
    // 异常处理辅助函数
    void declareExceptionHandlingFunctions();                                       // 声明异常处理相关函数
    llvm::Function* getSetjmpFunction();                                            // 获取 setjmp 函数
    llvm::Function* getLongjmpFunction();                                           // 获取 longjmp 函数
    llvm::GlobalVariable* getOrCreateExceptionMsgGlobal();                          // 获取或创建异常消息全局变量
    
    // 全局变量初始化
    void createGlobalConstructor();                                                 // 创建全局构造函数（用于初始化全局变量）
    
    // 类型转换辅助函数
    llvm::Value* convertToType(llvm::Value* value, llvm::Type* targetType);        // 将值转换为目标类型
    llvm::Value* convertToString(llvm::Value* value);                               // 将值转换为字符串
    std::string getFormatSpecForType(llvm::Type* type);                             // 获取类型对应的 printf 格式符
    
    // 安全检查辅助函数
    llvm::Value* createDivisionWithZeroCheck(llvm::Value* left, llvm::Value* right,    // 创建带除零检查的除法
                                             const std::string& errorMsg, 
                                             bool isIntegerDivision = false);
    llvm::Value* createModuloWithZeroCheck(llvm::Value* left, llvm::Value* right,      // 创建带除零检查的取模
                                           const std::string& errorMsg);
    
    // 表达式代码生成
    llvm::Value* codegenExpr(ExprNode* node);                                       // 表达式代码生成入口
    llvm::Value* codegenIntLiteral(IntLiteralNode* node);                           // 生成整数字面量
    llvm::Value* codegenDoubleLiteral(DoubleLiteralNode* node);                     // 生成浮点数字面量
    llvm::Value* codegenStringLiteral(StringLiteralNode* node);                     // 生成字符串字面量
    llvm::Value* codegenInterpolatedString(InterpolatedStringNode* node);           // 生成插值字符串
    llvm::Value* codegenCharLiteral(CharLiteralNode* node);                         // 生成字符字面量
    llvm::Value* codegenBoolLiteral(BoolLiteralNode* node);                         // 生成布尔字面量
    llvm::Value* codegenArrayLiteral(ArrayLiteralNode* node);                       // 生成数组字面量
    llvm::Value* codegenIdentifier(IdentifierNode* node);                           // 生成标识符引用
    llvm::Value* codegenArrayAccess(ArrayAccessNode* node);                         // 生成数组访问
    llvm::Value* codegenMemberAccess(MemberAccessNode* node);                       // 生成成员访问
    llvm::Value* codegenBinaryOp(BinaryOpNode* node);                               // 生成二元运算
    llvm::Value* codegenLogicalOp(BinaryOpNode* node);                              // 生成逻辑运算（短路求值）
    llvm::Value* codegenUnaryOp(UnaryOpNode* node);                                 // 生成一元运算
    llvm::Value* codegenFunctionCall(FunctionCallNode* node);                       // 生成函数调用
    
    // 语句代码生成
    void codegenStmt(StmtNode* node);                                               // 语句代码生成入口
    void codegenVarDecl(VarDeclNode* node);                                         // 生成变量声明
    void codegenAssignment(AssignmentNode* node);                                   // 生成赋值语句
    void codegenFunctionDecl(FunctionDeclNode* node);                               // 生成函数声明
    void codegenBlock(BlockNode* node);                                             // 生成代码块
    void codegenIfStmt(IfStmtNode* node);                                           // 生成 if 语句
    void codegenWhileStmt(WhileStmtNode* node);                                     // 生成 while 循环
    void codegenForStmt(ForStmtNode* node);                                         // 生成 for 循环
    void codegenSwitchStmt(SwitchStmtNode* node);                                   // 生成 switch 语句
    void codegenReturnStmt(ReturnStmtNode* node);                                   // 生成 return 语句
    void codegenBreakStmt(BreakStmtNode* node);                                     // 生成 break 语句
    void codegenContinueStmt(ContinueStmtNode* node);                               // 生成 continue 语句
    void codegenTryCatch(TryCatchNode* node);                                       // 生成 try-catch 语句
    void codegenThrow(ThrowStmtNode* node);                                         // 生成 throw 语句
    void codegenExprStmt(ExprStmtNode* node);                                       // 生成表达式语句
    
public:
    // 构造函数：初始化代码生成器，创建 LLVM 模块
    CodeGenerator(const std::string& moduleName);
    // 析构函数：清理资源
    ~CodeGenerator();
    
    // 配置
    void setSourceDirectory(const std::string& dir) { sourceDirectory = dir; }  // 设置源文件目录（用于模块查找）
    
    // 错误管理（使用error.h中的全局函数和变量）
    bool hasErrors() const { return g_errorCount > 0; }             // 检查是否有错误
    int getErrorCount() const { return g_errorCount; }              // 获取错误数量
    int getWarningCount() const { return g_warningCount; }          // 获取警告数量
    
    // 代码生成
    bool generate(ProgramNode* root);                               // 主入口：从 AST 生成 LLVM IR
    
    // 输出
    void printIR();                                                 // 打印 LLVM IR 到控制台
    bool writeIRToFile(const std::string& filename);                // 将 LLVM IR 写入文件
    bool compileToObjectFile(const std::string& filename);          // 编译为目标文件 (.o)
    bool compileToExecutable(const std::string& filename);          // 编译为可执行文件
    
    // 符号表和三地址码
    void printSymbolTable();                                        // 打印符号表到控制台
    bool writeSymbolTableToFile(const std::string& filename);       // 将符号表写入文件
    void printThreeAddressCode();                                   // 打印三地址码到控制台
    bool writeThreeAddressCodeToFile(const std::string& filename);  // 将三地址码写入文件
    
    // 访问器
    llvm::Module* getModule() { return module.get(); }              // 获取生成的 LLVM 模块
};

#endif // CODEGEN_H
