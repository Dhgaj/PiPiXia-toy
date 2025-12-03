#include "codegen.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <llvm/IR/Constants.h>
#include <memory>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <regex>

// 外部声明
extern FILE *yyin;
extern int yyparse();
extern std::shared_ptr<ProgramNode> root;

// 构造和析构函数
CodeGenerator::CodeGenerator(const std::string &moduleName) {
    // 初始化LLVM
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    // 创建LLVM上下文和模块
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>(moduleName, *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    // 初始化错误管理
    errorCount = 0;
    warningCount = 0;
    currentFunction = nullptr;
    // 异常消息全局变量初始化
    currentExceptionMsg = nullptr;  
    // 初始化当前目录为当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        currentDirectory = std::string(cwd);
    } else {
        currentDirectory = ".";
    }
    // 声明内置函数
    declareBuiltinFunctions();
}

CodeGenerator::~CodeGenerator() {}

// ANSI 转义码定义
static const char* ANSI_RED = "\033[1;31m";     // Error 时粗体红色
static const char* ANSI_YELLOW = "\033[1;33m";  // Warning 时粗体黄色
static const char* ANSI_RESET = "\033[0m";      // 重置颜色

// 安全执行外部命令（避免命令注入攻击）使用 fork/exec 代替 system()，不经过 shell 解释
static int safeExecuteCommand(const std::vector<std::string>& args, bool verbose = false) {
    if (args.empty()) {
        return -1;
    }
    
    if (verbose) {
        std::cout << "[Compile] Running:";
        for (const auto& arg : args) {
            std::cout << " " << arg;
        }
        std::cout << std::endl;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        // fork 失败
        std::cerr << "Error: Failed to fork process" << std::endl;
        return -1;
    }
    
    if (pid == 0) {
        // 子进程：执行命令
        // 构建 C 风格的参数数组
        std::vector<char*> c_args;
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);
        
        // 执行命令（不经过 shell）
        execvp(c_args[0], c_args.data());
        
        // 如果 execvp 返回，说明执行失败
        std::cerr << "Error: Failed to execute " << args[0] << std::endl;
        _exit(127);
    }
    
    // 父进程：等待子进程完成
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    
    return -1;
}

// 验证文件路径是否安全（不包含危险字符）
static bool isValidFilePath(const std::string& path) {
    // 检查空路径
    if (path.empty()) {
        return false;
    }
    
    // 禁止的字符模式（shell 特殊字符）
    static const std::regex dangerousPattern(R"([;&|`$(){}'\"\\\n\r])");
    
    if (std::regex_search(path, dangerousPattern)) {
        return false;
    }
    
    // 禁止以 - 开头（可能被解释为命令行选项）
    if (path[0] == '-') {
        return false;
    }
    
    return true;
}

// 错误和警告管理
void CodeGenerator::reportError(const std::string& message, int line) {
    if (line > 0) {
        std::cerr << ANSI_RED << "Error" << ANSI_RESET 
                  << " at line " << line << ": " << message << std::endl;
    } else {
        std::cerr << ANSI_RED << "Error" << ANSI_RESET 
                  << ": " << message << std::endl;
    }
    errorCount++;
}

void CodeGenerator::reportWarning(const std::string& message, int line) {
    if (line > 0) {
        std::cerr << ANSI_YELLOW << "Warning" << ANSI_RESET 
                  << " at line " << line << ": " << message << std::endl;
    } else {
        std::cerr << ANSI_YELLOW << "Warning" << ANSI_RESET 
                  << ": " << message << std::endl;
    }
    warningCount++;
}

// 临时内存管理
void CodeGenerator::pushTempMemory(llvm::Value *ptr) {
    if (!ptr || !ptr->getType()->isPointerTy())
        return;
    tempMemoryStack.push_back(ptr);
}

void CodeGenerator::removeTempMemory(llvm::Value *ptr) {
    if (!ptr)
        return;
    // 从临时内存栈中移除指定指针，当它被赋值给变量时，所有权转移
    auto it = std::find(tempMemoryStack.begin(), tempMemoryStack.end(), ptr);
    if (it != tempMemoryStack.end()) {
        tempMemoryStack.erase(it);
    }
}

void CodeGenerator::clearTempMemory() {
    if (tempMemoryStack.empty())
        return;

    // 获取free函数
    llvm::Function *freeFunc = module->getFunction("free");
    if (!freeFunc) {
        std::cerr << "Warning: free function not found, cannot auto-release "
                     "temp memory"
                  << std::endl;
        tempMemoryStack.clear();
        return;
    }

    // 逆序释放所有临时内存
    for (auto it = tempMemoryStack.rbegin(); it != tempMemoryStack.rend();
         ++it) {
        builder->CreateCall(freeFunc, {*it});
    }

    tempMemoryStack.clear();
}

void CodeGenerator::trackOwnedString(const std::string& varName, llvm::Value* ptr) {
    if (!ptr || !ptr->getType()->isPointerTy())
        return;
    
    // 如果变量之前拥有字符串，先释放旧的
    freeOwnedString(varName);
    
    // 跟踪新的字符串
    ownedStringMemory[varName] = ptr;
}

void CodeGenerator::freeOwnedString(const std::string& varName) {
    auto it = ownedStringMemory.find(varName);
    if (it == ownedStringMemory.end())
        return;
    
    // 获取free函数
    llvm::Function *freeFunc = module->getFunction("free");
    if (!freeFunc) {
        std::cerr << "Warning: free function not found" << std::endl;
        ownedStringMemory.erase(it);
        return;
    }
    
    // 释放旧的字符串内存
    builder->CreateCall(freeFunc, {it->second});
    
    // 从跟踪中移除
    ownedStringMemory.erase(it);
}

// 模块管理函数
std::string CodeGenerator::findModuleFile(const std::string &moduleName) {
    if (g_verbose) {
        std::cout << "[Module] Searching for module: " << moduleName
                  << std::endl;
    }

    // 处理相对路径或绝对路径
    std::string modulePath = moduleName;
    
    // 如果没有 .ppx 扩展名，添加它
    if (modulePath.length() < 4 ||
        modulePath.substr(modulePath.length() - 4) != ".ppx") {
        modulePath += ".ppx";
    }

    struct stat buffer;

    // 1. 直接检查文件是否存在（支持相对路径和绝对路径）
    if (stat(modulePath.c_str(), &buffer) == 0) {
        if (g_verbose) {
            std::cout << "[Module] Found module file: " << modulePath
                      << std::endl;
        }
        return modulePath;
    }

    // 2. 尝试在源文件目录查找
    if (!sourceDirectory.empty() && modulePath[0] != '/') {
        std::string fullPath = sourceDirectory + "/" + modulePath;
        if (stat(fullPath.c_str(), &buffer) == 0) {
            if (g_verbose) {
                std::cout << "[Module] Found module file in source directory: "
                          << fullPath << std::endl;
            }
            return fullPath;
        }
    }
    
    // 3. 尝试在当前工作目录查找
    if (modulePath[0] != '/') {
        std::string fullPath = currentDirectory + "/" + modulePath;
        if (stat(fullPath.c_str(), &buffer) == 0) {
            if (g_verbose) {
                std::cout << "[Module] Found module file in current directory: "
                          << fullPath << std::endl;
            }
            return fullPath;
        }
    }
    // 未找到
    return "";
}

// 加载模块
bool CodeGenerator::loadModule(const std::string &moduleName) {
    if (g_verbose) {
        std::cout << "[Module] Loading module: " << moduleName << std::endl;
    }

    // 检查是否已加载
    if (loadedModules.find(moduleName) != loadedModules.end()) {
        if (g_verbose) {
            std::cout << "[Module] Module already loaded: " << moduleName
                      << std::endl;
        }
        return true;
    }

    // 查找模块文件
    std::string moduleFile = findModuleFile(moduleName);
    if (moduleFile.empty()) {
        std::cerr << "Error: Module '" << moduleName << "' not found"
                  << std::endl;
        return false;
    }

    // 保存当前的 yyin 和 root
    FILE *oldYyin = yyin;
    std::shared_ptr<ProgramNode> oldRoot = root;

    // 打开模块文件
    yyin = fopen(moduleFile.c_str(), "r");
    if (!yyin) {
        std::cerr << "Error: Cannot open module file: " << moduleFile
                  << std::endl;
        yyin = oldYyin;  // 恢复原始状态
        return false;
    }

    // 解析模块
    root = nullptr;
    int parseResult = yyparse();
    // 保存模块的AST
    std::shared_ptr<ProgramNode> moduleRoot = root; 

    // 立即恢复全局状态
    fclose(yyin);
    yyin = oldYyin;
    root = oldRoot;

    if (parseResult != 0 || !moduleRoot) {
        std::cerr << "Error: Failed to parse module: " << moduleName
                  << std::endl;
        return false;
    }

    // 编译模块（生成函数和全局变量）
    if (g_verbose) {
        std::cout << "[Module] Compiling module: " << moduleName << std::endl;
    }

    // 创建模块的命名空间（如果不存在）
    if (moduleFunctions.find(moduleName) == moduleFunctions.end()) {
        moduleFunctions[moduleName] = std::map<std::string, llvm::Function*>();
        moduleGlobals[moduleName] = std::map<std::string, llvm::GlobalVariable*>();
    }

    for (auto &stmt : moduleRoot->statements) {
        // 跳过空指针（防御性编程）
        if (!stmt) {
            continue;
        }
        
        // 只处理函数声明和全局变量声明
        if (auto funcDecl = dynamic_cast<FunctionDeclNode *>(stmt.get())) {
            codegenFunctionDecl(funcDecl);
            // 将函数添加到模块命名空间
            llvm::Function* func = module->getFunction(funcDecl->name);
            if (func) {
                moduleFunctions[moduleName][funcDecl->name] = func;
                if (g_verbose) {
                    std::cout << "[Module] Registered function: " << moduleName 
                              << "." << funcDecl->name << std::endl;
                }
            }
        } else if (auto varDecl = dynamic_cast<VarDeclNode *>(stmt.get())) {
            codegenVarDecl(varDecl);
            // 将全局变量添加到模块命名空间
            auto it = globalValues.find(varDecl->name);
            if (it != globalValues.end()) {
                moduleGlobals[moduleName][varDecl->name] = it->second;
                if (g_verbose) {
                    std::cout << "[Module] Registered global: " << moduleName 
                              << "." << varDecl->name << std::endl;
                }
            }
        }
    }

    // 标记为已加载
    loadedModules.insert(moduleName);
    // moduleRoot 是 shared_ptr，会自动管理内存
    if (g_verbose) {
        std::cout << "[Module] Module loaded successfully: " << moduleName
                  << std::endl;
    }

    return true;
}

// 在模块中查找函数
llvm::Function* CodeGenerator::findModuleFunction(const std::string& moduleName, const std::string& funcName) {
    auto moduleIt = moduleFunctions.find(moduleName);
    if (moduleIt == moduleFunctions.end()) {
        return nullptr;
    }
    
    auto funcIt = moduleIt->second.find(funcName);
    if (funcIt == moduleIt->second.end()) {
        return nullptr;
    }
    
    return funcIt->second;
}

// 在模块中查找全局变量
llvm::GlobalVariable* CodeGenerator::findModuleGlobal(const std::string& moduleName, const std::string& varName) {
    auto moduleIt = moduleGlobals.find(moduleName);
    if (moduleIt == moduleGlobals.end()) {
        return nullptr;
    }
    
    auto varIt = moduleIt->second.find(varName);
    if (varIt == moduleIt->second.end()) {
        return nullptr;
    }
    
    return varIt->second;
}

// 处理 import 语句
void CodeGenerator::codegenImport(ImportNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Import statement: " << node->moduleName;
        if (!node->alias.empty()) {
            std::cout << " as " << node->alias;
        }
        std::cout << std::endl;
    }

    // 加载模块
    if (!loadModule(node->moduleName)) {
        std::cerr << "Error: Failed to import module: " << node->moduleName
                  << std::endl;
        return;
    }

    // 处理别名：保存别名到模块名的映射
    if (!node->alias.empty()) {
        // 使用用户指定的别名
        moduleAliases[node->alias] = node->moduleName;
        if (g_verbose) {
            std::cout << "[Module] Registered alias: " << node->alias 
                      << " -> " << node->moduleName << std::endl;
        }
    } else {
        // 没有别名时，模块名本身也可以作为访问路径
        // 这样 import math_module 后可以使用 math_module.function()
        moduleAliases[node->moduleName] = node->moduleName;
        if (g_verbose) {
            std::cout << "[Module] Module accessible as: " << node->moduleName << std::endl;
        }
    }
}

// 类型系统辅助函数
llvm::Type *CodeGenerator::getType(const std::string &typeName) {
    if (typeName == "int") {
        return llvm::Type::getInt32Ty(*context);
    } else if (typeName == "double") {
        return llvm::Type::getDoubleTy(*context);
    } else if (typeName == "bool") {
        return llvm::Type::getInt1Ty(*context);
    } else if (typeName == "char") {
        return llvm::Type::getInt8Ty(*context);
    } else if (typeName == "string") {
        // 为LLVM 21+使用不透明指针类型
        return llvm::PointerType::get(*context, 0);
    } else if (typeName == "void") {
        return llvm::Type::getVoidTy(*context);
    } else {
        std::cerr << "Warning: Unknown type '" << typeName
                  << "', using void type" << std::endl;
        return llvm::Type::getVoidTy(*context);
    }
}

// 在入口块中创建alloca指令
llvm::AllocaInst *CodeGenerator::createEntryBlockAlloca(
    llvm::Function *function, const std::string &varName, llvm::Type *type) {

    llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(),
                                 function->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, varName);
}

// 声明内置函数
void CodeGenerator::declareBuiltinFunctions() {
    // printf函数
    std::vector<llvm::Type *> printfArgs;
    printfArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), printfArgs, true);
    llvm::Function::Create(printfType, llvm::Function::ExternalLinkage,
                           "printf", module.get());

    // strlen 函数
    std::vector<llvm::Type *> strlenArgs;
    strlenArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *strlenType = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(*context), strlenArgs, false);
    llvm::Function::Create(strlenType, llvm::Function::ExternalLinkage,
                           "strlen", module.get());

    // malloc 函数
    std::vector<llvm::Type *> mallocArgs;
    mallocArgs.push_back(llvm::Type::getInt64Ty(*context));
    llvm::FunctionType *mallocType = llvm::FunctionType::get(
        llvm::PointerType::get(*context, 0), mallocArgs, false);
    llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage,
                           "malloc", module.get());

    // strcpy 函数
    std::vector<llvm::Type *> strcpyArgs;
    strcpyArgs.push_back(llvm::PointerType::get(*context, 0));
    strcpyArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *strcpyType = llvm::FunctionType::get(
        llvm::PointerType::get(*context, 0), strcpyArgs, false);
    llvm::Function::Create(strcpyType, llvm::Function::ExternalLinkage,
                           "strcpy", module.get());

    // strcat 函数
    std::vector<llvm::Type *> strcatArgs;
    strcatArgs.push_back(llvm::PointerType::get(*context, 0));
    strcatArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *strcatType = llvm::FunctionType::get(
        llvm::PointerType::get(*context, 0), strcatArgs, false);
    llvm::Function::Create(strcatType, llvm::Function::ExternalLinkage,
                           "strcat", module.get());

    // scanf 函数
    std::vector<llvm::Type *> scanfArgs;
    scanfArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *scanfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), scanfArgs, true);
    llvm::Function::Create(scanfType, llvm::Function::ExternalLinkage, "scanf",
                           module.get());

    // getchar 函数
    llvm::FunctionType *getcharType =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), {}, false);
    llvm::Function::Create(getcharType, llvm::Function::ExternalLinkage,
                           "getchar", module.get());

    // atoi 函数
    std::vector<llvm::Type *> atoiArgs;
    atoiArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *atoiType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), atoiArgs, false);
    llvm::Function::Create(atoiType, llvm::Function::ExternalLinkage, "atoi",
                           module.get());

    // atof 函数
    std::vector<llvm::Type *> atofArgs;
    atofArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *atofType = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(*context), atofArgs, false);
    llvm::Function::Create(atofType, llvm::Function::ExternalLinkage, "atof",
                           module.get());

    // sprintf 函数
    std::vector<llvm::Type *> sprintfArgs;
    sprintfArgs.push_back(llvm::PointerType::get(*context, 0));
    sprintfArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *sprintfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), sprintfArgs, true);
    llvm::Function::Create(sprintfType, llvm::Function::ExternalLinkage,
                           "sprintf", module.get());

    // exit 函数
    std::vector<llvm::Type *> exitArgs;
    exitArgs.push_back(llvm::Type::getInt32Ty(*context));
    llvm::FunctionType *exitType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), exitArgs, false);
    llvm::Function::Create(exitType, llvm::Function::ExternalLinkage, "exit",
                           module.get());

    // free 函数
    std::vector<llvm::Type *> freeArgs;
    freeArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *freeType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), freeArgs, false);
    llvm::Function::Create(freeType, llvm::Function::ExternalLinkage, "free",
                           module.get());
    
    // 声明异常处理相关函数
    declareExceptionHandlingFunctions();
}

// 获取 printf 函数
llvm::Function *CodeGenerator::getPrintfFunction() {
    return module->getFunction("printf");
}

// 获取 scanf 函数
llvm::Function *CodeGenerator::getScanfFunction() {
    return module->getFunction("scanf");
}

// 获取 strlen 函数
llvm::Function *CodeGenerator::getStrlenFunction() {
    return module->getFunction("strlen");
}

// 表达式代码生成 - 字面量
// 生成整数字面量
llvm::Value *CodeGenerator::codegenIntLiteral(IntLiteralNode *node) {
    return llvm::ConstantInt::get(*context, llvm::APInt(32, node->value, true));
}

// 生成浮点数字面量
llvm::Value *CodeGenerator::codegenDoubleLiteral(DoubleLiteralNode *node) {
    return llvm::ConstantFP::get(*context, llvm::APFloat(node->value));
}

// 生成字符串字面量
llvm::Value *CodeGenerator::codegenStringLiteral(StringLiteralNode *node) {
    return builder->CreateGlobalString(node->value, "", 0, module.get());
}

// 字符串插值代码生成

llvm::Value *CodeGenerator::codegenInterpolatedString(InterpolatedStringNode *node) {
    
    if (node->expressions.empty()) {
        // 没有插值表达式，返回普通字符串
        std::string combined;
        for (const auto& part : node->stringParts) {
            combined += part;
        }
        return builder->CreateGlobalString(combined, "", 0, module.get());
    }
    
    // 获取 snprintf 函数（用于计算所需缓冲区大小）
    llvm::Function* snprintfFunc = module->getFunction("snprintf");
    if (!snprintfFunc) {
        std::vector<llvm::Type*> snprintfArgs;
        snprintfArgs.push_back(llvm::PointerType::get(*context, 0));  
        snprintfArgs.push_back(llvm::Type::getInt64Ty(*context));     
        snprintfArgs.push_back(llvm::PointerType::get(*context, 0));  
        llvm::FunctionType* snprintfType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            snprintfArgs,
            true  // 可变参数
        );
        snprintfFunc = llvm::Function::Create(
            snprintfType, llvm::Function::ExternalLinkage, "snprintf", module.get()
        );
    }
    
    // 获取 malloc 函数
    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        llvm::FunctionType* mallocType = llvm::FunctionType::get(
            llvm::PointerType::get(*context, 0),
            {llvm::Type::getInt64Ty(*context)},
            false
        );
        mallocFunc = llvm::Function::Create(
            mallocType, llvm::Function::ExternalLinkage, "malloc", module.get()
        );
    }
    
    // 生成所有表达式的值并确定格式符
    std::vector<llvm::Value*> exprValues;
    std::vector<std::string> formatSpecs;
    
    for (size_t i = 0; i < node->expressions.size(); i++) {
        const auto& expr = node->expressions[i];
        llvm::Value* exprValue = codegenExpr(expr.get());
        if (!exprValue) {
            std::cerr << "Error: Failed to generate code for expression in interpolated string" << std::endl;
            return nullptr;
        }
        
        // 根据类型确定格式符
        llvm::Type* exprType = exprValue->getType();
        std::string formatSpec;
        
        // 检查是否有自定义格式说明符
        if (i < node->formatSpecs.size() && !node->formatSpecs[i].empty()) {
            // 使用自定义格式（需要添加 % 前缀）
            formatSpec = "%" + node->formatSpecs[i];
        } else {
            // 使用默认格式
            formatSpec = getFormatSpecForType(exprType);
        }
        
        // 布尔值需要特殊处理转为字符串
        if (exprType->isIntegerTy(1)) {
            llvm::Value *trueStr = builder->CreateGlobalString("true", "", 0, module.get());
            llvm::Value *falseStr = builder->CreateGlobalString("false", "", 0, module.get());
            exprValue = builder->CreateSelect(exprValue, trueStr, falseStr, "bool_str");
        }
        
        exprValues.push_back(exprValue);
        formatSpecs.push_back(formatSpec);
    }
    
    // 构建格式字符串
    std::string formatStr;
    for (size_t i = 0; i < node->stringParts.size(); i++) {
        // 转义字符串部分中的 % 符号，防止被 printf 误解释
        std::string part = node->stringParts[i];
        std::string escapedPart;
        for (char ch : part) {
            if (ch == '%') {
                escapedPart += "%%";  
            } else {
                escapedPart += ch;
            }
        }
        formatStr += escapedPart;
        if (i < formatSpecs.size()) {
            formatStr += formatSpecs[i];
        }
    }
    
    llvm::Value* formatGlobal = builder->CreateGlobalString(formatStr, "", 0, module.get());
    
    // 第一步：调用 snprintf(NULL, 0, ...) 计算所需缓冲区大小
    std::vector<llvm::Value*> sizeCalcArgs;
    sizeCalcArgs.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0))); 
    sizeCalcArgs.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0))); 
    sizeCalcArgs.push_back(formatGlobal);
    for (auto exprValue : exprValues) {
        sizeCalcArgs.push_back(exprValue);
    }
    
    // snprintf 返回所需的字符数（不包括结尾符）
    llvm::Value* requiredSize = builder->CreateCall(snprintfFunc, sizeCalcArgs, "required_size");
    
    // 加 1 用于结尾符，并转换为 i64
    llvm::Value* requiredSize64 = builder->CreateSExt(requiredSize, llvm::Type::getInt64Ty(*context), "size64");
    llvm::Value* bufferSize = builder->CreateAdd(
        requiredSize64,
        llvm::ConstantInt::get(*context, llvm::APInt(64, 1)),
        "buffer_size"
    );
    
    // 第二步：分配精确大小的缓冲区
    llvm::Value* buffer = builder->CreateCall(mallocFunc, {bufferSize});
    buffer = builder->CreatePointerCast(buffer, llvm::PointerType::get(*context, 0));
    pushTempMemory(buffer);
    
    // 第三步：调用 snprintf 填充缓冲区
    std::vector<llvm::Value*> snprintfArgs;
    snprintfArgs.push_back(buffer);
    snprintfArgs.push_back(bufferSize);
    snprintfArgs.push_back(formatGlobal);
    for (auto exprValue : exprValues) {
        snprintfArgs.push_back(exprValue);
    }
    
    builder->CreateCall(snprintfFunc, snprintfArgs);
    
    return buffer;
}

// 生成字符字面量
llvm::Value *CodeGenerator::codegenCharLiteral(CharLiteralNode *node) {
    return llvm::ConstantInt::get(*context, llvm::APInt(8, node->value, false));
}

// 生成布尔字面量
llvm::Value *CodeGenerator::codegenBoolLiteral(BoolLiteralNode *node) {
    return llvm::ConstantInt::get(*context,
                                  llvm::APInt(1, node->value ? 1 : 0, false));
}

// 生成标识符引用
llvm::Value *CodeGenerator::codegenIdentifier(IdentifierNode *node) {
    // 先查找局部变量
    llvm::AllocaInst *alloca = namedValues[node->name];
    if (alloca) {
        return builder->CreateLoad(alloca->getAllocatedType(), alloca,
                                   node->name.c_str());
    }

    // 再查找全局变量
    llvm::GlobalVariable *globalVar = globalValues[node->name];
    if (globalVar) {
        return builder->CreateLoad(globalVar->getValueType(), globalVar,
                                   node->name.c_str());
    }

    if (node->lineNumber > 0) {
        std::cerr << ANSI_RED << "Error" << ANSI_RESET 
                  << " at line " << node->lineNumber 
                  << ": Undefined variable '" << node->name << "'"
                  << std::endl;
    } else {
        std::cerr << ANSI_RED << "Error" << ANSI_RESET 
                  << ": Undefined variable '" << node->name << "'"
                  << std::endl;
    }
    errorCount++;
    return nullptr;
}

// 二元运算
llvm::Value *CodeGenerator::codegenBinaryOp(BinaryOpNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Binary operation: " << node->op << std::endl;
    }

    // 逻辑运算符需要特殊处理（短路求值），不能提前求值右侧
    if (node->op == "&&" || node->op == "||") {
        return codegenLogicalOp(node);
    }

    llvm::Value *left = codegenExpr(node->left.get());
    llvm::Value *right = codegenExpr(node->right.get());
    if (!left || !right)
        return nullptr;

    // 类型提升：bool(i1) -> int(i32)
    if (left->getType()->isIntegerTy(1)) {
        left = builder->CreateZExt(left, llvm::Type::getInt32Ty(*context),
                                   "bool_to_int");
    }
    if (right->getType()->isIntegerTy(1)) {
        right = builder->CreateZExt(right, llvm::Type::getInt32Ty(*context),
                                    "bool_to_int");
    }

    // 类型提升：char(i8) -> int(i32)
    if (left->getType()->isIntegerTy(8)) {
        left = builder->CreateSExt(left, llvm::Type::getInt32Ty(*context),
                                   "char_to_int");
    }
    if (right->getType()->isIntegerTy(8)) {
        right = builder->CreateSExt(right, llvm::Type::getInt32Ty(*context),
                                    "char_to_int");
    }

    // 类型提升：i16 -> i32
    if (left->getType()->isIntegerTy(16)) {
        left = builder->CreateSExt(left, llvm::Type::getInt32Ty(*context),
                                   "i16_to_i32");
    }
    if (right->getType()->isIntegerTy(16)) {
        right = builder->CreateSExt(right, llvm::Type::getInt32Ty(*context),
                                    "i16_to_i32");
    }

    // 类型提升：int -> double（如果需要）
    if (left->getType()->isDoubleTy() && right->getType()->isIntegerTy()) {
        right = builder->CreateSIToFP(right, llvm::Type::getDoubleTy(*context),
                                      "int_to_double");
    } else if (right->getType()->isDoubleTy() &&
               left->getType()->isIntegerTy()) {
        left = builder->CreateSIToFP(left, llvm::Type::getDoubleTy(*context),
                                     "int_to_double");
    }

    // 字符串拼接处理
    if (node->op == "+" && left->getType()->isPointerTy() &&
        right->getType()->isPointerTy()) {
        // 调用 strlen 获取两个字符串的长度
        llvm::Function *strlenFunc = module->getFunction("strlen");
        llvm::Value *len1 = builder->CreateCall(strlenFunc, {left}, "len1");
        llvm::Value *len2 = builder->CreateCall(strlenFunc, {right}, "len2");

        // 计算新字符串需要的总长度 (len1 + len2 + 1 for null terminator)
        llvm::Value *totalLen = builder->CreateAdd(len1, len2, "totallen");
        totalLen = builder->CreateAdd(
            totalLen, llvm::ConstantInt::get(*context, llvm::APInt(64, 1)),
            "totallen_plus1");

        // 分配内存
        llvm::Function *mallocFunc = module->getFunction("malloc");
        llvm::Value *newStr =
            builder->CreateCall(mallocFunc, {totalLen}, "newstr");

        // 复制第一个字符串
        llvm::Function *strcpyFunc = module->getFunction("strcpy");
        builder->CreateCall(strcpyFunc, {newStr, left});

        // 拼接第二个字符串
        llvm::Function *strcatFunc = module->getFunction("strcat");
        builder->CreateCall(strcatFunc, {newStr, right});

        // 追踪临时内存，稍后自动释放
        pushTempMemory(newStr);

        return newStr;
    }

    bool isFloat = left->getType()->isDoubleTy();

    if (node->op == "+")
        return isFloat ? builder->CreateFAdd(left, right, "addtmp")
                       : builder->CreateAdd(left, right, "addtmp");
    if (node->op == "-")
        return isFloat ? builder->CreateFSub(left, right, "subtmp")
                       : builder->CreateSub(left, right, "subtmp");
    if (node->op == "*")
        return isFloat ? builder->CreateFMul(left, right, "multmp")
                       : builder->CreateMul(left, right, "multmp");

    // / 运算符: 总是返回浮点数
    if (node->op == "/") {
        // 如果操作数不是浮点数,先转换为浮点数
        if (!left->getType()->isDoubleTy()) {
            left = builder->CreateSIToFP(
                left, llvm::Type::getDoubleTy(*context), "conv_left");
        }
        if (!right->getType()->isDoubleTy()) {
            right = builder->CreateSIToFP(
                right, llvm::Type::getDoubleTy(*context), "conv_right");
        }

        // 使用辅助函数进行除零检查
        return createDivisionWithZeroCheck(left, right, "Runtime Error: Division by zero\n", false);
    }

    // // 运算符: 整除,总是返回整数
    if (node->op == "//") {
        // 如果操作数是浮点数,先转换为整数
        if (left->getType()->isDoubleTy()) {
            left = builder->CreateFPToSI(left, llvm::Type::getInt32Ty(*context),
                                         "conv_left");
        }
        if (right->getType()->isDoubleTy()) {
            right = builder->CreateFPToSI(
                right, llvm::Type::getInt32Ty(*context), "conv_right");
        }

        // 使用辅助函数进行除零检查
        return createDivisionWithZeroCheck(left, right, "Runtime Error: Integer division by zero\n", true);
    }

    if (node->op == "%") {
        // 使用辅助函数进行除零检查
        return createModuloWithZeroCheck(left, right, "Runtime Error: Modulo by zero\n");
    }
    if (node->op == "==")
        return isFloat ? builder->CreateFCmpOEQ(left, right, "eqtmp")
                       : builder->CreateICmpEQ(left, right, "eqtmp");
    if (node->op == "!=")
        return isFloat ? builder->CreateFCmpONE(left, right, "netmp")
                       : builder->CreateICmpNE(left, right, "netmp");
    if (node->op == "<")
        return isFloat ? builder->CreateFCmpOLT(left, right, "lttmp")
                       : builder->CreateICmpSLT(left, right, "lttmp");
    if (node->op == ">")
        return isFloat ? builder->CreateFCmpOGT(left, right, "gttmp")
                       : builder->CreateICmpSGT(left, right, "gttmp");
    if (node->op == "<=")
        return isFloat ? builder->CreateFCmpOLE(left, right, "letmp")
                       : builder->CreateICmpSLE(left, right, "letmp");
    if (node->op == ">=")
        return isFloat ? builder->CreateFCmpOGE(left, right, "getmp")
                       : builder->CreateICmpSGE(left, right, "getmp");

    return nullptr;
}

// 生成逻辑运算符（短路求值）
llvm::Value *CodeGenerator::codegenLogicalOp(BinaryOpNode *node) {
    // 先求值左侧
    llvm::Value *left = codegenExpr(node->left.get());
    if (!left)
        return nullptr;

    // 逻辑运算符 - 实现短路求值
    if (node->op == "&&") {
        // 转换左侧为布尔值
        llvm::Value *leftCond = left;
        if (!left->getType()->isIntegerTy(1)) {
            if (left->getType()->isDoubleTy()) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                leftCond = builder->CreateFCmpONE(left, zero, "tobool");
            } else {
                leftCond = builder->CreateICmpNE(
                    left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
            }
        }

        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock *rhsBB =
            llvm::BasicBlock::Create(*context, "and_rhs", function);
        llvm::BasicBlock *mergeBB =
            llvm::BasicBlock::Create(*context, "and_merge");

        // 如果左侧为假，直接跳到merge（结果为false）；否则求值右侧
        builder->CreateCondBr(leftCond, rhsBB, mergeBB);
        llvm::BasicBlock *leftBB = builder->GetInsertBlock();

        // 右侧求值块
        builder->SetInsertPoint(rhsBB);
        llvm::Value *rightVal = codegenExpr(node->right.get());
        if (!rightVal)
            return nullptr;

        // 转换右侧为布尔值
        llvm::Value *rightCond = rightVal;
        if (!rightVal->getType()->isIntegerTy(1)) {
            if (rightVal->getType()->isDoubleTy()) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                rightCond = builder->CreateFCmpONE(rightVal, zero, "tobool");
            } else {
                rightCond = builder->CreateICmpNE(
                    rightVal, llvm::ConstantInt::get(rightVal->getType(), 0),
                    "tobool");
            }
        }
        builder->CreateBr(mergeBB);
        rhsBB = builder->GetInsertBlock();

        // 合并块 - 使用PHI节点
        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context),
                                                2, "and_result");
        phi->addIncoming(llvm::ConstantInt::getFalse(*context),
                         leftBB);           // 左侧为假，结果为false
        phi->addIncoming(rightCond, rhsBB); // 左侧为真，结果取决于右侧

        return phi;
    }

    if (node->op == "||") {
        // 转换左侧为布尔值
        llvm::Value *leftCond = left;
        if (!left->getType()->isIntegerTy(1)) {
            if (left->getType()->isDoubleTy()) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                leftCond = builder->CreateFCmpONE(left, zero, "tobool");
            } else {
                leftCond = builder->CreateICmpNE(
                    left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
            }
        }

        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock *rhsBB =
            llvm::BasicBlock::Create(*context, "or_rhs", function);
        llvm::BasicBlock *mergeBB =
            llvm::BasicBlock::Create(*context, "or_merge");

        // 如果左侧为真，直接跳到merge（结果为true）；否则求值右侧
        builder->CreateCondBr(leftCond, mergeBB, rhsBB);
        llvm::BasicBlock *leftBB = builder->GetInsertBlock();

        // 右侧求值块
        builder->SetInsertPoint(rhsBB);
        llvm::Value *rightVal = codegenExpr(node->right.get());
        if (!rightVal)
            return nullptr;

        // 转换右侧为布尔值
        llvm::Value *rightCond = rightVal;
        if (!rightVal->getType()->isIntegerTy(1)) {
            if (rightVal->getType()->isDoubleTy()) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                rightCond = builder->CreateFCmpONE(rightVal, zero, "tobool");
            } else {
                rightCond = builder->CreateICmpNE(
                    rightVal, llvm::ConstantInt::get(rightVal->getType(), 0),
                    "tobool");
            }
        }
        builder->CreateBr(mergeBB);
        rhsBB = builder->GetInsertBlock();

        // 合并块 - 使用PHI节点
        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi =
            builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "or_result");
        phi->addIncoming(llvm::ConstantInt::getTrue(*context),
                         leftBB);           // 左侧为真，结果为true
        phi->addIncoming(rightCond, rhsBB); // 左侧为假，结果取决于右侧

        return phi;
    }

    return nullptr;
}

// 生成一元运算符
llvm::Value *CodeGenerator::codegenUnaryOp(UnaryOpNode *node) {
    llvm::Value *operand = codegenExpr(node->operand.get());
    if (!operand) {
        std::cerr << "Error: Invalid operand for unary operator '" << node->op
                  << "'" << std::endl;
        return nullptr;
    }

    if (node->op == "-") {
        return operand->getType()->isDoubleTy()
                   ? builder->CreateFNeg(operand, "negtmp")
                   : builder->CreateNeg(operand, "negtmp");
    }
    if (node->op == "!")
        return builder->CreateNot(operand, "nottmp");

    std::cerr << "Error: Unknown unary operator '" << node->op << "'"
              << std::endl;
    return nullptr;
}

// 生成函数调用
llvm::Value *CodeGenerator::codegenFunctionCall(FunctionCallNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Function call: " << node->functionName << "()"
                  << std::endl;
    }

    // 如果有object字段，这是成员函数调用（如 module.function()）
    if (node->object) {
        // 检查是否是模块函数调用
        if (auto identNode = dynamic_cast<IdentifierNode*>(node->object.get())) {
            std::string moduleName = identNode->name;
            
            // 检查是否是模块别名
            auto aliasIt = moduleAliases.find(moduleName);
            if (aliasIt != moduleAliases.end()) {
                moduleName = aliasIt->second;  // 使用实际模块名
                if (g_verbose) {
                    std::cout << "[IR Gen] Resolved alias " << identNode->name 
                              << " to module " << moduleName << std::endl;
                }
            }
            
            // 在模块命名空间中查找函数
            llvm::Function* moduleFunc = findModuleFunction(moduleName, node->functionName);
            if (moduleFunc) {
                if (g_verbose) {
                    std::cout << "[IR Gen] Calling module function: " << moduleName 
                              << "." << node->functionName << "()" << std::endl;
                }
                
                // 准备参数
                std::vector<llvm::Value*> args;
                for (size_t i = 0; i < node->arguments.size(); i++) {
                    llvm::Value* argVal = codegenExpr(node->arguments[i].get());
                    if (!argVal) return nullptr;
                    
                    // 类型检查和转换
                    if (i < moduleFunc->arg_size()) {
                        llvm::Type* expectedType = moduleFunc->getFunctionType()->getParamType(i);
                        if (argVal->getType() != expectedType) {
                            argVal = convertToType(argVal, expectedType);
                        }
                    }
                    args.push_back(argVal);
                }
                
                // 调用模块函数
                if (moduleFunc->getReturnType()->isVoidTy()) {
                    return builder->CreateCall(moduleFunc, args);
                }
                return builder->CreateCall(moduleFunc, args, "module_call");
            } else {
                std::cerr << "Error: Function '" << node->functionName 
                          << "' not found in module '" << moduleName << "'" << std::endl;
                return nullptr;
            }
        }
        
        // 其他类型的对象成员访问暂不支持
        std::cerr << "Warning: Non-module member access is not yet supported"
                  << std::endl;
    }

    // print() 函数 - 打印不换行
    if (node->functionName == "print") {
        llvm::Function *printfFunc = getPrintfFunction();
        if (node->arguments.empty()) {
            // 没有参数的 print() - 打印换行符
            std::vector<llvm::Value *> args;
            args.push_back(
                builder->CreateGlobalString("\n", "", 0, module.get()));
            return builder->CreateCall(printfFunc, args, "printcall");
        }

        llvm::Value *arg = codegenExpr(node->arguments[0].get());
        if (!arg)
            return nullptr;

        // 检查是否有第二个参数（nowrap）
        bool nowrap = false;
        if (node->arguments.size() > 1) {
            // 第二个参数应该是标识符 "nowrap"
            if (auto ident =
                    dynamic_cast<IdentifierNode *>(node->arguments[1].get())) {
                if (ident->name == "nowrap") {
                    nowrap = true;
                }
            }
        }

        std::vector<llvm::Value *> args;

        // 根据类型确定格式字符串和值
        if (arg->getType()->isIntegerTy(32)) {
            args.push_back(builder->CreateGlobalString(nowrap ? "%d" : "%d\n",
                                                       "", 0, module.get()));
            args.push_back(arg);
        } else if (arg->getType()->isDoubleTy()) {
            args.push_back(builder->CreateGlobalString(nowrap ? "%f" : "%f\n",
                                                       "", 0, module.get()));
            args.push_back(arg);
        } else if (arg->getType()->isPointerTy()) {
            args.push_back(builder->CreateGlobalString(nowrap ? "%s" : "%s\n",
                                                       "", 0, module.get()));
            args.push_back(arg);
        } else if (arg->getType()->isIntegerTy(8)) {
            args.push_back(builder->CreateGlobalString(nowrap ? "%c" : "%c\n",
                                                       "", 0, module.get()));
            args.push_back(arg);
        } else if (arg->getType()->isIntegerTy(1)) {
            // 布尔值：转换为字符串
            if (nowrap) {
                llvm::Value *trueStr =
                    builder->CreateGlobalString("true", "", 0, module.get());
                llvm::Value *falseStr =
                    builder->CreateGlobalString("false", "", 0, module.get());
                llvm::Value *str =
                    builder->CreateSelect(arg, trueStr, falseStr, "boolstr");
                args.push_back(
                    builder->CreateGlobalString("%s", "", 0, module.get()));
                args.push_back(str);
            } else {
                llvm::Value *trueStr =
                    builder->CreateGlobalString("true\n", "", 0, module.get());
                llvm::Value *falseStr =
                    builder->CreateGlobalString("false\n", "", 0, module.get());
                llvm::Value *str =
                    builder->CreateSelect(arg, trueStr, falseStr, "boolstr");
                args.push_back(
                    builder->CreateGlobalString("%s", "", 0, module.get()));
                args.push_back(str);
            }
        } else {
            args.push_back(builder->CreateGlobalString(
                nowrap ? "(unknown type)" : "(unknown type)\n", "", 0,
                module.get()));
        }

        return builder->CreateCall(printfFunc, args, "printcall");
    }

    // input() 函数
    if (node->functionName == "input") {
        llvm::Function *printfFunc = getPrintfFunction();
        llvm::Function *getcharFunc = module->getFunction("getchar");
        llvm::Function *mallocFunc = module->getFunction("malloc");

        // 如果有参数,先打印提示信息
        if (!node->arguments.empty()) {
            llvm::Value *prompt = codegenExpr(node->arguments[0].get());
            if (prompt && prompt->getType()->isPointerTy()) {
                std::vector<llvm::Value *> printArgs;
                printArgs.push_back(
                    builder->CreateGlobalString("%s", "", 0, module.get()));
                printArgs.push_back(prompt);
                builder->CreateCall(printfFunc, printArgs);
            }
        }

        // 分配缓冲区用于存储输入
        llvm::Value *bufferSize =
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), CodeGenConstants::INPUT_BUFFER_SIZE);
        llvm::Value *buffer =
            builder->CreateCall(mallocFunc, {bufferSize}, "input_buffer");

        // 创建索引变量
        llvm::AllocaInst *indexAlloca = createEntryBlockAlloca(
            currentFunction, "input_index", llvm::Type::getInt32Ty(*context));
        builder->CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0),
            indexAlloca);

        // 创建循环读取字符
        llvm::BasicBlock *loopBB =
            llvm::BasicBlock::Create(*context, "input_loop", currentFunction);
        llvm::BasicBlock *storeBB =
            llvm::BasicBlock::Create(*context, "store_char", currentFunction);
        llvm::BasicBlock *afterBB =
            llvm::BasicBlock::Create(*context, "after_input", currentFunction);

        builder->CreateBr(loopBB);

        // 循环体
        builder->SetInsertPoint(loopBB);
        llvm::Value *ch = builder->CreateCall(getcharFunc, {}, "ch");

        // 检查是否为换行符或 EOF
        llvm::Value *isNewline = builder->CreateICmpEQ(
            ch, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), '\n'),
            "is_newline");
        llvm::Value *isEOF = builder->CreateICmpEQ(
            ch, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), -1),
            "is_eof");
        llvm::Value *shouldStop =
            builder->CreateOr(isNewline, isEOF, "should_stop");

        // 添加缓冲区溢出检查：确保索引不超过255
        llvm::Value *currentIndex = builder->CreateLoad(
            llvm::Type::getInt32Ty(*context), indexAlloca, "check_index");
        llvm::Value *isOverflow = builder->CreateICmpSGE(
            currentIndex,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 255),
            "is_overflow");
        llvm::Value *shouldStopFull =
            builder->CreateOr(shouldStop, isOverflow, "should_stop_full");

        builder->CreateCondBr(shouldStopFull, afterBB, storeBB);

        // 存储字符
        builder->SetInsertPoint(storeBB);
        llvm::Value *index = builder->CreateLoad(
            llvm::Type::getInt32Ty(*context), indexAlloca, "index");
        llvm::Value *charPtr = builder->CreateGEP(
            llvm::Type::getInt8Ty(*context), buffer, index, "char_ptr");
        llvm::Value *charVal = builder->CreateTrunc(
            ch, llvm::Type::getInt8Ty(*context), "char_val");
        builder->CreateStore(charVal, charPtr);

        // 增加索引
        llvm::Value *nextIndex = builder->CreateAdd(
            index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1),
            "next_index");
        builder->CreateStore(nextIndex, indexAlloca);
        builder->CreateBr(loopBB);

        // 循环结束后,添加 null 终止符
        builder->SetInsertPoint(afterBB);
        llvm::Value *finalIndex = builder->CreateLoad(
            llvm::Type::getInt32Ty(*context), indexAlloca, "final_index");
        llvm::Value *nullPtr = builder->CreateGEP(
            llvm::Type::getInt8Ty(*context), buffer, finalIndex, "null_ptr");
        builder->CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), 0),
            nullPtr);

        // 追踪临时内存
        pushTempMemory(buffer);

        // 返回缓冲区指针 (字符串类型)
        return buffer;
    }

    // len() 函数
    if (node->functionName == "len") {
        if (node->arguments.empty()) {
            return nullptr;
        }

        llvm::Value *str = codegenExpr(node->arguments[0].get());
        if (!str || !str->getType()->isPointerTy()) {
            // 参数必须是字符串(指针类型)
            return nullptr;
        }

        // 调用 strlen 函数
        llvm::Function *strlenFunc = module->getFunction("strlen");
        llvm::Value *length64 =
            builder->CreateCall(strlenFunc, {str}, "strlen");

        // strlen 返回 i64，转换为 i32
        llvm::Value *length32 = builder->CreateTrunc(
            length64, llvm::Type::getInt32Ty(*context), "len");

        return length32;
    }

    // to_int() 函数
    if (node->functionName == "to_int") {
        if (node->arguments.empty()) {
            return nullptr;
        }

        llvm::Value *str = codegenExpr(node->arguments[0].get());
        if (!str)
            return nullptr;

        // 如果参数已经是整数,直接返回
        if (str->getType()->isIntegerTy(32)) {
            return str;
        }

        // 如果是字符串(指针类型),调用 atoi()
        if (str->getType()->isPointerTy()) {
            llvm::Function *atoiFunc = module->getFunction("atoi");
            return builder->CreateCall(atoiFunc, {str}, "to_int");
        }

        // 如果是浮点数,转换为整数
        if (str->getType()->isDoubleTy()) {
            return builder->CreateFPToSI(str, llvm::Type::getInt32Ty(*context),
                                         "to_int");
        }

        // 其他类型暂不支持
        return nullptr;
    }

    // to_double() 函数
    if (node->functionName == "to_double") {
        if (node->arguments.empty()) {
            return nullptr;
        }

        llvm::Value *val = codegenExpr(node->arguments[0].get());
        if (!val)
            return nullptr;

        // 如果参数已经是浮点数,直接返回
        if (val->getType()->isDoubleTy()) {
            return val;
        }

        // 如果是字符串(指针类型),调用 atof()
        if (val->getType()->isPointerTy()) {
            llvm::Function *atofFunc = module->getFunction("atof");
            return builder->CreateCall(atofFunc, {val}, "to_double");
        }

        // 如果是整数,转换为浮点数
        if (val->getType()->isIntegerTy(32)) {
            return builder->CreateSIToFP(val, llvm::Type::getDoubleTy(*context),
                                         "to_double");
        }

        // 其他类型暂不支持
        return nullptr;
    }

    // to_string() 函数
    if (node->functionName == "to_string") {
        if (node->arguments.empty()) {
            // to_string() 需要一个参数
            return nullptr;
        }

        llvm::Value *val = codegenExpr(node->arguments[0].get());
        if (!val)
            return nullptr;

        // 如果参数已经是字符串,直接返回
        if (val->getType()->isPointerTy()) {
            return val;
        }

        // 分配缓冲区用于存储结果字符串
        llvm::Function *mallocFunc = module->getFunction("malloc");
        llvm::Value *bufferSize =
            llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), CodeGenConstants::STRING_CONVERT_BUFFER_SIZE);
        llvm::Value *buffer =
            builder->CreateCall(mallocFunc, {bufferSize}, "str_buffer");

        llvm::Function *sprintfFunc = module->getFunction("sprintf");

        if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(1) &&
            !val->getType()->isIntegerTy(8)) {
            // 整数转字符串
            llvm::Value *format =
                builder->CreateGlobalString("%d", "", 0, module.get());
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            pushTempMemory(buffer); // 追踪临时内存
            return buffer;
        } else if (val->getType()->isDoubleTy()) {
            // 浮点数转字符串
            llvm::Value *format =
                builder->CreateGlobalString("%g", "", 0, module.get());
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            pushTempMemory(buffer); // 追踪临时内存
            return buffer;
        } else if (val->getType()->isIntegerTy(1)) {
            // 布尔值转字符串 - 为保持一致性，也使用动态内存分配
            llvm::Value *trueStr =
                builder->CreateGlobalString("true", "", 0, module.get());
            llvm::Value *falseStr =
                builder->CreateGlobalString("false", "", 0, module.get());
            
            // 选择对应的字符串
            llvm::Value *selectedStr = 
                builder->CreateSelect(val, trueStr, falseStr, "bool_str");
            
            // 使用 sprintf 的 %s 格式将其复制到动态分配的缓冲区
            llvm::Value *format = 
                builder->CreateGlobalString("%s", "", 0, module.get());
            builder->CreateCall(sprintfFunc, {buffer, format, selectedStr});
            pushTempMemory(buffer); // 追踪临时内存，保持一致性
            return buffer;
        } else if (val->getType()->isIntegerTy(8)) {
            // 字符转字符串
            llvm::Value *format =
                builder->CreateGlobalString("%c", "", 0, module.get());
            builder->CreateCall(sprintfFunc, {buffer, format, val});
            pushTempMemory(buffer); // 追踪临时内存
            return buffer;
        }

        // 其他类型暂不支持
        return nullptr;
    }

    // free() 函数 - 释放malloc分配的内存
    if (node->functionName == "free") {
        if (node->arguments.empty()) {
            std::cerr << "Error: free() requires one argument" << std::endl;
            return nullptr;
        }

        llvm::Value *ptr = codegenExpr(node->arguments[0].get());
        if (!ptr)
            return nullptr;

        // 检查参数是否为指针类型
        if (!ptr->getType()->isPointerTy()) {
            std::cerr << "Error: free() argument must be a pointer (string)"
                      << std::endl;
            return nullptr;
        }

        // 调用系统的free函数
        llvm::Function *freeFunc = module->getFunction("free");
        builder->CreateCall(freeFunc, {ptr});

        // free()返回void，但为了兼容性返回0
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
    }

    llvm::Function *calleeFunc = module->getFunction(node->functionName);
    if (!calleeFunc) {
        std::cerr << "Error: Unknown function '" << node->functionName << "'"
                  << std::endl;
        return nullptr;
    }

    // 检查参数数量（对可变参数函数需要特殊处理）
    if (!calleeFunc->isVarArg()) {
        // 普通函数：参数数量必须完全匹配
        if (calleeFunc->arg_size() != node->arguments.size()) {
            std::cerr << "Error: Function '" << node->functionName << "' expects "
                      << calleeFunc->arg_size() << " arguments but got "
                      << node->arguments.size() << std::endl;
            return nullptr;
        }
    } else {
        // 可变参数函数：至少要有固定参数数量
        if (node->arguments.size() < calleeFunc->arg_size()) {
            std::cerr << "Error: Variadic function '" << node->functionName 
                      << "' expects at least " << calleeFunc->arg_size() 
                      << " arguments but got " << node->arguments.size() << std::endl;
            return nullptr;
        }
    }

    std::vector<llvm::Value *> args;
    unsigned idx = 0;
    for (auto &arg : node->arguments) {
        // 特殊处理：如果参数是数组标识符且期望指针类型，传递数组地址而不是加载值
        llvm::Value *argVal = nullptr;
        
        // 检查是否期望指针类型（对于数组参数）
        bool expectsPointer = false;
        if (idx < calleeFunc->arg_size()) {
            llvm::Type *expectedType = calleeFunc->getFunctionType()->getParamType(idx);
            expectsPointer = expectedType->isPointerTy();
        }
        
        // 如果参数是标识符且期望指针
        if (expectsPointer) {
            if (auto identNode = dynamic_cast<IdentifierNode*>(arg.get())) {
                auto it = namedValues.find(identNode->name);
                if (it != namedValues.end()) {
                    llvm::AllocaInst *alloca = it->second;
                    // 检查是否是数组类型
                    if (alloca->getAllocatedType()->isArrayTy()) {
                        // 对于数组，使用 GEP 获取第一个元素的指针
                        std::vector<llvm::Value*> indices;
                        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
                        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
                        argVal = builder->CreateInBoundsGEP(
                            alloca->getAllocatedType(), alloca, indices, "array_param");
                    }
                }
            }
        }
        
        // 如果没有特殊处理，正常生成表达式
        if (!argVal) {
            argVal = codegenExpr(arg.get());
        }
        
        if (!argVal)
            return nullptr;

        // 对于可变参数函数，只有固定参数需要类型检查
        // 可变参数部分直接传递
        if (idx >= calleeFunc->arg_size()) {
            // 这是可变参数部分，直接添加
            args.push_back(argVal);
            idx++;
            continue;
        }

        // 获取期望的参数类型
        llvm::Type *expectedType =
            calleeFunc->getFunctionType()->getParamType(idx);

        // 检查并转换类型
        if (argVal->getType() != expectedType) {
            llvm::Value* originalArgVal = argVal;
            
            // 使用统一的类型转换函数
            argVal = convertToType(argVal, expectedType);
            
            // 检查转换是否成功（类型是否匹配）
            if (argVal->getType() != expectedType && 
                !(expectedType->isPointerTy() && argVal->getType()->isPointerTy())) {
                std::cerr << "Error: Type mismatch for argument " << idx
                          << " in function '" << node->functionName << "'"
                          << std::endl;
                std::cerr << "  Expected: ";
                expectedType->print(llvm::errs());
                std::cerr << ", Got: ";
                originalArgVal->getType()->print(llvm::errs());
                std::cerr << std::endl;
                return nullptr;
            }
        }

        args.push_back(argVal);
        idx++;
    }

    // void 函数不应该有返回值名称
    if (calleeFunc->getReturnType()->isVoidTy()) {
        return builder->CreateCall(calleeFunc, args);
    }
    return builder->CreateCall(calleeFunc, args, "calltmp");
}

// 生成数组访问
llvm::Value *CodeGenerator::codegenArrayAccess(ArrayAccessNode *node) {
    llvm::Value *index = codegenExpr(node->index.get());
    if (!index) {
        return nullptr;
    }

    // 获取数组变量或子数组
    llvm::Value *arrayPtr = nullptr;
    llvm::Type *arrayType = nullptr;
    llvm::Type *elementType = nullptr;
    bool isFromVariable = false;
    
    if (auto identNode = dynamic_cast<IdentifierNode*>(node->array.get())) {
        // 直接访问变量
        std::string arrayVarName = identNode->name;
        
        auto it = namedValues.find(arrayVarName);
        if (it == namedValues.end()) {
            std::cerr << "Error: Undefined array variable '" << arrayVarName << "'" << std::endl;
            return nullptr;
        }
        
        arrayPtr = it->second;
        isFromVariable = true;
        
        if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(arrayPtr)) {
            arrayType = allocaInst->getAllocatedType();
            
            if (arrayType->isArrayTy()) {
                llvm::ArrayType *arrType = llvm::cast<llvm::ArrayType>(arrayType);
                elementType = arrType->getElementType();
            } else if (arrayType->isPointerTy()) {
                // 数组参数（作为指针传递）
                // 元素类型通过加载指针获得
                // 对于不透明指针，我们需要知道元素类型，暂时假设为 i32
                // TODO: 需要更好的类型跟踪系统
                elementType = llvm::Type::getInt32Ty(*context);
            } else {
                elementType = llvm::Type::getInt8Ty(*context);
            }
        } else {
            std::cerr << "Error: Not an array variable" << std::endl;
            return nullptr;
        }
    } else if (dynamic_cast<ArrayAccessNode*>(node->array.get())) {
        // 链式访问：matrix[0][1] 或 cube[0][0][0]
        // 使用辅助函数递归收集所有索引
        std::vector<llvm::Value*> allIndices;
        std::string baseVarName;
        
        // 递归收集索引
        ArrayAccessNode* current = node;
        while (current) {
            llvm::Value *idx = codegenExpr(current->index.get());
            if (!idx) return nullptr;
            allIndices.insert(allIndices.begin(), idx);  // 前插
            
            if (auto innerAccess = dynamic_cast<ArrayAccessNode*>(current->array.get())) {
                current = innerAccess;
            } else if (auto ident = dynamic_cast<IdentifierNode*>(current->array.get())) {
                baseVarName = ident->name;
                break;
            } else {
                std::cerr << "Error: Unsupported nested array access" << std::endl;
                return nullptr;
            }
        }
        
        // 获取基础数组
        auto it = namedValues.find(baseVarName);
        if (it == namedValues.end()) {
            std::cerr << "Error: Undefined array variable '" << baseVarName << "'" << std::endl;
            return nullptr;
        }
        
        llvm::Value *basePtr = it->second;
        llvm::Type *currentType = nullptr;
        
        if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(basePtr)) {
            currentType = allocaInst->getAllocatedType();
        } else {
            std::cerr << "Error: Not an array variable" << std::endl;
            return nullptr;
        }
        
        // 逐层访问
        llvm::Value *currentPtr = basePtr;
        for (size_t i = 0; i < allIndices.size() - 1; i++) {
            if (!currentType->isArrayTy()) {
                std::cerr << "Error: Dimension mismatch in array access" << std::endl;
                return nullptr;
            }
            
            llvm::ArrayType *arrType = llvm::cast<llvm::ArrayType>(currentType);
            std::vector<llvm::Value*> indices;
            indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
            indices.push_back(allIndices[i]);
            
            currentPtr = builder->CreateGEP(currentType, currentPtr, indices, "sub_ptr");
            currentType = arrType->getElementType();
        }
        
        // 设置最终访问
        arrayPtr = currentPtr;
        arrayType = currentType;
        
        if (arrayType->isArrayTy()) {
            llvm::ArrayType *arrType = llvm::cast<llvm::ArrayType>(arrayType);
            elementType = arrType->getElementType();
        } else {
            elementType = arrayType;
        }
        
        // 使用收集的最后一个索引
        index = allIndices.back();
        isFromVariable = false;
    } else {
        std::cerr << "Error: Unsupported array access pattern" << std::endl;
        return nullptr;
    }
    
    // 尝试从array表达式获取变量名（如果是标识符）
    std::string arrayVarName;
    bool isStringType = false;

    if (auto identNode = dynamic_cast<IdentifierNode *>(node->array.get())) {
        arrayVarName = identNode->name;

        // 从类型信息表查询变量类型
        auto typeIt = variableTypes.find(arrayVarName);
        if (typeIt != variableTypes.end()) {
            std::string varType = typeIt->second;

            // 如果是字符串类型(string/char*)，元素类型是i8
            if (varType == "string" || varType == "char*") {
                elementType = llvm::Type::getInt8Ty(*context);
                isStringType = true;
            }
            // 如果是int数组，元素类型是i32
            else if (varType == "int" || varType == "int*") {
                elementType = llvm::Type::getInt32Ty(*context);
            }
            // 如果是double数组，元素类型是double
            else if (varType == "double" || varType == "double*") {
                elementType = llvm::Type::getDoubleTy(*context);
            }
            // 其他类型默认i8
            else {
                elementType = llvm::Type::getInt8Ty(*context);
            }
        }
    }

    // 运行时边界检查
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *checkBB =
        llvm::BasicBlock::Create(*context, "bounds_check", function);
    llvm::BasicBlock *errorBB =
        llvm::BasicBlock::Create(*context, "bounds_error");
    llvm::BasicBlock *accessBB =
        llvm::BasicBlock::Create(*context, "array_access");
    llvm::BasicBlock *mergeBB =
        llvm::BasicBlock::Create(*context, "bounds_merge");

    builder->CreateBr(checkBB);
    builder->SetInsertPoint(checkBB);

    // 检查1: 索引是否为负数
    llvm::Value *zero = llvm::ConstantInt::get(index->getType(), 0);
    llvm::Value *isNegative =
        builder->CreateICmpSLT(index, zero, "is_negative");

    llvm::Value *isOutOfBounds = isNegative;

    // 检查2: 对于字符串类型，检查索引是否超出长度
    if (isStringType) {
        llvm::Function *strlenFunc = module->getFunction("strlen");
        if (strlenFunc) {
            llvm::Value *length64 =
                builder->CreateCall(strlenFunc, {arrayPtr}, "strlen");
            llvm::Value *length32 = builder->CreateTrunc(
                length64, llvm::Type::getInt32Ty(*context), "len32");

            // 将index转换为i32（如果需要）
            llvm::Value *index32 = index;
            if (index->getType() != llvm::Type::getInt32Ty(*context)) {
                if (index->getType()->isIntegerTy() &&
                    index->getType()->getIntegerBitWidth() < 32) {
                    index32 = builder->CreateSExt(
                        index, llvm::Type::getInt32Ty(*context), "index32");
                } else if (index->getType()->isIntegerTy() &&
                           index->getType()->getIntegerBitWidth() > 32) {
                    index32 = builder->CreateTrunc(
                        index, llvm::Type::getInt32Ty(*context), "index32");
                }
            }

            llvm::Value *isOverflow =
                builder->CreateICmpSGE(index32, length32, "is_overflow");
            isOutOfBounds = builder->CreateOr(isOutOfBounds, isOverflow,
                                              "is_out_of_bounds");
        }
    }

    builder->CreateCondBr(isOutOfBounds, errorBB, accessBB);

    // 错误分支：打印错误信息并返回默认值
    function->insert(function->end(), errorBB);
    builder->SetInsertPoint(errorBB);
    llvm::Function *printfFunc = getPrintfFunction();
    llvm::Value *errorMsg = builder->CreateGlobalString(
        "Runtime Error: Array index out of bounds\n", "", 0, module.get());
    builder->CreateCall(printfFunc, {errorMsg});

    // 返回默认值（0或0.0）
    llvm::Value *defaultVal = llvm::Constant::getNullValue(elementType);
    builder->CreateBr(mergeBB);
    llvm::BasicBlock *errorExitBB = builder->GetInsertBlock();

    // 正常访问分支
    function->insert(function->end(), accessBB);
    builder->SetInsertPoint(accessBB);
    
    llvm::Value *ptr = nullptr;
    if (arrayType && arrayType->isArrayTy() && isFromVariable) {
        // 从变量直接访问数组：使用 GEP(array, 0, index)
        std::vector<llvm::Value*> indices;
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        indices.push_back(index);
        ptr = builder->CreateGEP(arrayType, arrayPtr, indices, "arrayptr");
    } else if (arrayType && arrayType->isArrayTy() && !isFromVariable) {
        // 从子数组访问（链式访问）：arrayPtr 已经是子数组指针
        std::vector<llvm::Value*> indices;
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        indices.push_back(index);
        ptr = builder->CreateGEP(arrayType, arrayPtr, indices, "arrayptr");
    } else if (arrayType && arrayType->isPointerTy()) {
        // 数组参数（指针类型）：需要先加载指针值，然后使用 GEP
        llvm::Value *loadedPtr = builder->CreateLoad(arrayType, arrayPtr, "loaded_ptr");
        ptr = builder->CreateGEP(elementType, loadedPtr, index, "arrayptr");
    } else {
        // 指针类型（字符串等）：使用 GEP(ptr, index)
        ptr = builder->CreateGEP(elementType, arrayPtr, index, "arrayptr");
    }
    
    llvm::Value *loadedVal = builder->CreateLoad(elementType, ptr, "arrayval");
    builder->CreateBr(mergeBB);
    llvm::BasicBlock *accessExitBB = builder->GetInsertBlock();

    // 合并分支
    function->insert(function->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
    llvm::PHINode *phi = builder->CreatePHI(elementType, 2, "array_result");
    phi->addIncoming(defaultVal, errorExitBB);
    phi->addIncoming(loadedVal, accessExitBB);

    return phi;
}

// 成员访问代码生成
// 处理 object.member 语法
llvm::Value *CodeGenerator::codegenMemberAccess(MemberAccessNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Member access: ." << node->memberName << std::endl;
    }

    // 获取对象表达式
    if (!node->object) {
        std::cerr << "Error: Member access without object" << std::endl;
        errorCount++;
        return nullptr;
    }

    // 检查对象是否是标识符（可能是模块名或变量名）
    if (auto identNode = dynamic_cast<IdentifierNode*>(node->object.get())) {
        std::string objectName = identNode->name;
        
        // 检查是否是模块访问（查找模块全局变量）
        // 首先检查是否是模块别名
        auto aliasIt = moduleAliases.find(objectName);
        if (aliasIt != moduleAliases.end()) {
            objectName = aliasIt->second;  // 使用实际模块名
            if (g_verbose) {
                std::cout << "[IR Gen] Resolved alias " << identNode->name 
                          << " to module " << objectName << std::endl;
            }
        }
        
        // 尝试在模块命名空间中查找全局变量
        llvm::GlobalVariable* moduleVar = findModuleGlobal(objectName, node->memberName);
        if (moduleVar) {
            if (g_verbose) {
                std::cout << "[IR Gen] Accessing module global variable: " << objectName 
                          << "." << node->memberName << std::endl;
            }
            
            // 加载全局变量的值
            llvm::Type* varType = moduleVar->getValueType();
            return builder->CreateLoad(varType, moduleVar, "module_global");
        }
        
        // 如果不是模块访问，可能是对象成员访问
        // 检查变量是否存在
        auto varIt = namedValues.find(objectName);
        if (varIt != namedValues.end()) {
            // 变量存在，但PiPiXia目前不支持结构体/类
            std::cerr << "Error: Member access on object '" << objectName 
                      << "' is not supported. PiPiXia currently does not support "
                      << "structures or classes." << std::endl;
            std::cerr << "Note: Only module member access (e.g., module.function()) "
                      << "is currently supported." << std::endl;
            errorCount++;
            return nullptr;
        }
        
        // 变量不存在
        std::cerr << "Error: Undefined variable or module '" << objectName << "'" << std::endl;
        errorCount++;
        return nullptr;
    }
    
    // 对于复杂表达式的成员访问（如 func().member 或 array[0].member）
    // 当前不支持，因为没有结构体系统
    std::cerr << "Error: Member access on complex expressions is not supported. "
              << "PiPiXia currently does not support structures or classes." << std::endl;
    std::cerr << "Note: Only module member access (e.g., module.function() or module.variable) "
              << "is currently supported." << std::endl;
    errorCount++;
    return nullptr;
}

// 表达式代码生成 - 分发器
llvm::Value *CodeGenerator::codegenExpr(ExprNode *node) {
    if (!node) {
        std::cerr << "Error: Null expression node" << std::endl;
        return nullptr;
    }

    if (auto intLit = dynamic_cast<IntLiteralNode *>(node))
        return codegenIntLiteral(intLit);
    if (auto doubleLit = dynamic_cast<DoubleLiteralNode *>(node))
        return codegenDoubleLiteral(doubleLit);
    if (auto stringLit = dynamic_cast<StringLiteralNode *>(node))
        return codegenStringLiteral(stringLit);
    if (auto interpolStr = dynamic_cast<InterpolatedStringNode *>(node))
        return codegenInterpolatedString(interpolStr);
    if (auto charLit = dynamic_cast<CharLiteralNode *>(node))
        return codegenCharLiteral(charLit);
    if (auto boolLit = dynamic_cast<BoolLiteralNode *>(node))
        return codegenBoolLiteral(boolLit);
    if (auto arrayLit = dynamic_cast<ArrayLiteralNode *>(node))
        return codegenArrayLiteral(arrayLit);
    if (auto ident = dynamic_cast<IdentifierNode *>(node))
        return codegenIdentifier(ident);
    if (auto binOp = dynamic_cast<BinaryOpNode *>(node))
        return codegenBinaryOp(binOp);
    if (auto unaryOp = dynamic_cast<UnaryOpNode *>(node))
        return codegenUnaryOp(unaryOp);
    if (auto funcCall = dynamic_cast<FunctionCallNode *>(node))
        return codegenFunctionCall(funcCall);
    if (auto arrayAccess = dynamic_cast<ArrayAccessNode *>(node))
        return codegenArrayAccess(arrayAccess);
    if (auto memberAccess = dynamic_cast<MemberAccessNode *>(node))
        return codegenMemberAccess(memberAccess);

    std::cerr << "Error: Unknown expression node type" << std::endl;
    return nullptr;
}

// 表达式代码生成 - 数组字面量

llvm::Value *CodeGenerator::codegenArrayLiteral(ArrayLiteralNode *node) {
    // 处理空数组：生成默认类型（int）的空数组
    if (node->elements.empty()) {
        llvm::Type *elemType = llvm::Type::getInt32Ty(*context);  // 默认使用 int 类型
        llvm::ArrayType *arrayType = llvm::ArrayType::get(elemType, 0);  // 大小为 0
        
        // 在函数入口块分配空数组
        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
        llvm::AllocaInst *arrayAlloca = tmpBuilder.CreateAlloca(arrayType, nullptr, "empty_array");
        
        // 返回数组首元素指针
        std::vector<llvm::Value*> indices;
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        return builder->CreateGEP(arrayType, arrayAlloca, indices, "empty_array_ptr");
    }
    
    // 检查是否是嵌套数组（多维）
    bool isNestedArray = dynamic_cast<ArrayLiteralNode*>(node->elements[0].get()) != nullptr;
    
    if (isNestedArray) {
        // 多维数组：递归处理
        // 先生成第一个子数组确定类型
        auto firstSubArray = dynamic_cast<ArrayLiteralNode*>(node->elements[0].get());
        llvm::Value *firstSubValue = codegenArrayLiteral(firstSubArray);
        if (!firstSubValue) return nullptr;
        
        // 获取子数组类型（这是一个指针，指向子数组的第一个元素）
        // 我们需要重建完整的子数组类型
        llvm::Value *firstSubElem = codegenExpr(firstSubArray->elements[0].get());
        if (!firstSubElem) {
            std::cerr << "Error: Failed to generate code for first sub-array element" << std::endl;
            return nullptr;
        }
        llvm::Type *firstSubElemType = firstSubElem->getType();
        llvm::ArrayType *subArrayType = llvm::ArrayType::get(firstSubElemType, firstSubArray->elements.size());
        
        size_t outerSize = node->elements.size();
        llvm::ArrayType *arrayType = llvm::ArrayType::get(subArrayType, outerSize);
        
        // 在函数入口块分配数组
        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
        llvm::AllocaInst *arrayAlloca = tmpBuilder.CreateAlloca(arrayType, nullptr, "multi_array");
        
        // 初始化每个子数组
        for (size_t i = 0; i < outerSize; i++) {
            auto subArrayNode = dynamic_cast<ArrayLiteralNode*>(node->elements[i].get());
            if (!subArrayNode) {
                std::cerr << "Error: Inconsistent array dimensions" << std::endl;
                return nullptr;
            }
            
            // 逐个复制子数组元素到目标位置
            for (size_t j = 0; j < subArrayNode->elements.size(); j++) {
                llvm::Value *elem = codegenExpr(subArrayNode->elements[j].get());
                if (!elem) return nullptr;
                
                // 计算目标元素的位置：array[i][j]
                std::vector<llvm::Value*> elemIndices;
                elemIndices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
                elemIndices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, i)));
                elemIndices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, j)));
                llvm::Value *elemPtr = builder->CreateGEP(arrayType, arrayAlloca, elemIndices, "elem_ptr");
                builder->CreateStore(elem, elemPtr);
            }
        }
        
        // 返回数组首元素指针
        std::vector<llvm::Value*> indices;
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        return builder->CreateGEP(arrayType, arrayAlloca, indices, "array_ptr");
        
    } else {
        // 一维数组：原有逻辑
        llvm::Value *firstElem = codegenExpr(node->elements[0].get());
        if (!firstElem) return nullptr;
        
        llvm::Type *elemType = firstElem->getType();
        size_t arraySize = node->elements.size();
        
        llvm::ArrayType *arrayType = llvm::ArrayType::get(elemType, arraySize);
        
        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
        llvm::AllocaInst *arrayAlloca = tmpBuilder.CreateAlloca(arrayType, nullptr, "array_lit");
        
        for (size_t i = 0; i < arraySize; i++) {
            llvm::Value *elem = (i == 0) ? firstElem : codegenExpr(node->elements[i].get());
            if (!elem) return nullptr;
            
            std::vector<llvm::Value*> indices;
            indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
            indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, i)));
            
            llvm::Value *elemPtr = builder->CreateGEP(arrayType, arrayAlloca, indices, "elem_ptr");
            builder->CreateStore(elem, elemPtr);
        }
        
        std::vector<llvm::Value*> indices;
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
        return builder->CreateGEP(arrayType, arrayAlloca, indices, "array_ptr");
    }
}

// 语句代码生成实现
void CodeGenerator::codegenVarDecl(VarDeclNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Variable declaration: " << node->name << " : "
                  << node->type->typeName << std::endl;
    }

    // 获取类型，支持多维数组
    llvm::Type *type = nullptr;
    bool isArrayType = false;
    
    if (!node->type->arrayDimensions.empty()) {
        // 数组类型（一维或多维）
        llvm::Type *baseType = getType(node->type->typeName);
        if (!baseType) {
            std::cerr << "Error: Unknown type '" << node->type->typeName << "'" << std::endl;
            return;
        }
        
        // 从最内层向外构建数组类型
        // 例如: int[3][4] -> [3 x [4 x i32]]
        type = baseType;
        for (int i = node->type->arrayDimensions.size() - 1; i >= 0; i--) {
            type = llvm::ArrayType::get(type, node->type->arrayDimensions[i]);
        }
        isArrayType = true;
    } else {
        // 普通类型
        type = getType(node->type->typeName);
        if (!type) {
            std::cerr << "Error: Unknown type '" << node->type->typeName << "'" << std::endl;
            return;
        }
    }

    // 检查是否在函数内
    if (!currentFunction) {
        // 检查全局变量是否已定义
        if (globalValues.find(node->name) != globalValues.end()) {
            std::cerr << "Error: Global variable '" << node->name
                      << "' is already defined" << std::endl;
            return;
        }

        // 全局变量/常量：创建全局变量
        llvm::Constant *initVal = nullptr;

        if (node->initializer) {
            // 对于全局变量，初始化器必须是常量表达式
            if (auto intLit =
                    dynamic_cast<IntLiteralNode *>(node->initializer.get())) {
                // 整数字面量
                initVal = llvm::ConstantInt::get(type, intLit->value);
            } else if (auto doubleLit = dynamic_cast<DoubleLiteralNode *>(
                           node->initializer.get())) {
                // 浮点数字面量
                initVal = llvm::ConstantFP::get(type, doubleLit->value);
            } else if (auto boolLit = dynamic_cast<BoolLiteralNode *>(
                           node->initializer.get())) {
                // 布尔字面量
                initVal = llvm::ConstantInt::get(type, boolLit->value ? 1 : 0);
            } else if (auto charLit = dynamic_cast<CharLiteralNode *>(
                           node->initializer.get())) {
                // 字符字面量
                initVal = llvm::ConstantInt::get(type, (uint8_t)charLit->value);
            } else if (auto stringLit = dynamic_cast<StringLiteralNode *>(
                           node->initializer.get())) {
                // 字符串字面量 - 创建全局字符串常量
                llvm::Constant *strConstant =
                    llvm::ConstantDataArray::getString(*context,
                                                       stringLit->value, true);
                auto globalStr =
                    new llvm::GlobalVariable(*module, strConstant->getType(),
                                             true, // 字符串是常量
                                             llvm::GlobalValue::PrivateLinkage,
                                             strConstant, node->name + ".str");
                // 返回指向字符串的指针
                initVal = llvm::ConstantExpr::getPointerCast(globalStr, type);
            } else if (auto binOp = dynamic_cast<BinaryOpNode *>(
                           node->initializer.get())) {
                // 尝试计算常量表达式（如：1 + 2）
                // 只支持简单的字面量运算
                auto leftInt =
                    dynamic_cast<IntLiteralNode *>(binOp->left.get());
                auto rightInt =
                    dynamic_cast<IntLiteralNode *>(binOp->right.get());

                if (leftInt && rightInt) {
                    int result = 0;
                    if (binOp->op == "+")
                        result = leftInt->value + rightInt->value;
                    else if (binOp->op == "-")
                        result = leftInt->value - rightInt->value;
                    else if (binOp->op == "*")
                        result = leftInt->value * rightInt->value;
                    else if (binOp->op == "/")
                        result = rightInt->value != 0
                                     ? leftInt->value / rightInt->value
                                     : 0;
                    else if (binOp->op == "%")
                        result = rightInt->value != 0
                                     ? leftInt->value % rightInt->value
                                     : 0;
                    else {
                        std::cerr << "Warning: Unsupported constant expression "
                                     "operator '"
                                  << binOp->op << "' for global variable '"
                                  << node->name << "', using zero" << std::endl;
                        initVal = llvm::Constant::getNullValue(type);
                    }
                    if (!initVal) {
                        initVal = llvm::ConstantInt::get(type, result);
                    }
                } else {
                    // 浮点数常量表达式
                    auto leftDouble =
                        dynamic_cast<DoubleLiteralNode *>(binOp->left.get());
                    auto rightDouble =
                        dynamic_cast<DoubleLiteralNode *>(binOp->right.get());

                    if (leftDouble && rightDouble) {
                        double result = 0.0;
                        if (binOp->op == "+")
                            result = leftDouble->value + rightDouble->value;
                        else if (binOp->op == "-")
                            result = leftDouble->value - rightDouble->value;
                        else if (binOp->op == "*")
                            result = leftDouble->value * rightDouble->value;
                        else if (binOp->op == "/")
                            result =
                                rightDouble->value != 0.0
                                    ? leftDouble->value / rightDouble->value
                                    : 0.0;
                        else {
                            std::cerr << "Warning: Unsupported constant "
                                         "expression operator '"
                                      << binOp->op << "' for global variable '"
                                      << node->name << "', using zero"
                                      << std::endl;
                            initVal = llvm::Constant::getNullValue(type);
                        }
                        if (!initVal) {
                            initVal = llvm::ConstantFP::get(type, result);
                        }
                    } else {
                        // 不是简单的常量表达式，使用动态初始化
                        initVal = llvm::Constant::getNullValue(type);
                        
                        // 创建全局变量
                        auto globalVar = new llvm::GlobalVariable(
                            *module, type, node->isConst, llvm::GlobalValue::InternalLinkage,
                            initVal, node->name);
                        globalValues[node->name] = globalVar;
                        
                        // 添加到动态初始化列表
                        globalInitializers.push_back({globalVar, node->initializer.get()});
                        
                        if (g_verbose) {
                            std::cout << "[IR Gen] Global variable '" << node->name 
                                      << "' will be initialized dynamically (complex binary expression)" << std::endl;
                        }
                        return;
                    }
                }
            } else if (auto unaryOp = dynamic_cast<UnaryOpNode *>(
                           node->initializer.get())) {
                // 处理一元运算符（如：-5）
                if (unaryOp->op == "-") {
                    if (auto intLit = dynamic_cast<IntLiteralNode *>(
                            unaryOp->operand.get())) {
                        initVal = llvm::ConstantInt::get(type, -intLit->value);
                    } else if (auto doubleLit =
                                   dynamic_cast<DoubleLiteralNode *>(
                                       unaryOp->operand.get())) {
                        initVal =
                            llvm::ConstantFP::get(type, -doubleLit->value);
                    } else {
                        std::cerr
                            << "Warning: Global variable '" << node->name
                            << "' has non-constant unary expression, using zero"
                            << std::endl;
                        initVal = llvm::Constant::getNullValue(type);
                    }
                } else {
                    std::cerr << "Warning: Unsupported unary operator '"
                              << unaryOp->op << "' for global variable '"
                              << node->name << "', using zero" << std::endl;
                    initVal = llvm::Constant::getNullValue(type);
                }
            } else {
                // 其他类型的初始化器不是常量表达式
                // 使用零初始化，稍后在全局构造函数中进行实际初始化
                initVal = llvm::Constant::getNullValue(type);
                
                // 创建全局变量
                auto globalVar = new llvm::GlobalVariable(
                    *module, type, node->isConst, llvm::GlobalValue::InternalLinkage,
                    initVal, node->name);
                globalValues[node->name] = globalVar;
                
                // 添加到动态初始化列表
                globalInitializers.push_back({globalVar, node->initializer.get()});
                
                if (g_verbose) {
                    std::cout << "[IR Gen] Global variable '" << node->name 
                              << "' will be initialized dynamically" << std::endl;
                }
                return;
            }
        } else {
            // 没有初始化器，使用零初始化
            initVal = llvm::Constant::getNullValue(type);
        }

        auto globalVar = new llvm::GlobalVariable(
            *module, type, node->isConst, llvm::GlobalValue::InternalLinkage,
            initVal, node->name);
        globalValues[node->name] = globalVar;
        return;
    }

    // 检查局部变量是否已定义
    if (namedValues.find(node->name) != namedValues.end()) {
        std::cerr << "Error: Local variable '" << node->name
                  << "' is already defined in this scope" << std::endl;
        return;
    }

    // 局部变量：创建 alloca
    llvm::AllocaInst *alloca =
        createEntryBlockAlloca(currentFunction, node->name, type);

    if (node->initializer) {
        if (isArrayType && dynamic_cast<ArrayLiteralNode*>(node->initializer.get())) {
            // 数组字面量初始化（支持多维）
            auto arrayLit = dynamic_cast<ArrayLiteralNode*>(node->initializer.get());
            llvm::ArrayType *arrType = llvm::cast<llvm::ArrayType>(type);
            
            // 检查是否是嵌套数组
            bool isNested = !arrayLit->elements.empty() && 
                           dynamic_cast<ArrayLiteralNode*>(arrayLit->elements[0].get()) != nullptr;
            
            if (isNested) {
                // 多维数组：使用辅助函数递归初始化
                std::function<void(ArrayLiteralNode*, std::vector<llvm::Value*>)> initNestedArray;
                
                initNestedArray = [&](ArrayLiteralNode* arrLit, std::vector<llvm::Value*> currentIndices) {
                    for (size_t i = 0; i < arrLit->elements.size(); i++) {
                        std::vector<llvm::Value*> newIndices = currentIndices;
                        newIndices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, i)));
                        
                        if (auto subArrayLit = dynamic_cast<ArrayLiteralNode*>(arrLit->elements[i].get())) {
                            // 仍然是嵌套数组，继续递归
                            initNestedArray(subArrayLit, newIndices);
                        } else {
                            // 到达最深层，存储元素
                            llvm::Value *elem = codegenExpr(arrLit->elements[i].get());
                            if (!elem) continue;
                            
                            // 计算完整索引：[0, i1, i2, ..., in]
                            std::vector<llvm::Value*> fullIndices;
                            fullIndices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
                            fullIndices.insert(fullIndices.end(), newIndices.begin(), newIndices.end());
                            
                            llvm::Value *elemPtr = builder->CreateGEP(arrType, alloca, fullIndices, "elem_ptr");
                            builder->CreateStore(elem, elemPtr);
                        }
                    }
                };
                
                // 开始递归初始化
                std::vector<llvm::Value*> baseIndices;
                initNestedArray(arrayLit, baseIndices);
            } else {
                // 一维数组：原有逻辑
                for (size_t i = 0; i < arrayLit->elements.size() && i < arrType->getNumElements(); i++) {
                    llvm::Value *elem = codegenExpr(arrayLit->elements[i].get());
                    if (!elem) continue;
                    
                    std::vector<llvm::Value*> indices;
                    indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
                    indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, i)));
                    
                    llvm::Value *elemPtr = builder->CreateGEP(arrType, alloca, indices, "arr_elem");
                    builder->CreateStore(elem, elemPtr);
                }
            }
        } else {
            // 普通变量初始化
            llvm::Value *initVal = codegenExpr(node->initializer.get());
            if (initVal) {
                if (initVal->getType() != type) {
                    initVal = convertToType(initVal, type);
                }
                builder->CreateStore(initVal, alloca);

                // 如果初始化值是临时内存，从临时栈中移除并跟踪所有权
                if (initVal->getType()->isPointerTy()) {
                    // 检查是否是从临时内存转移所有权
                    auto it = std::find(tempMemoryStack.begin(), tempMemoryStack.end(), initVal);
                    if (it != tempMemoryStack.end()) {
                        // 是临时内存，转移所有权给变量
                        removeTempMemory(initVal);
                        trackOwnedString(node->name, initVal);
                    }
                }
            }
        }
    }

    namedValues[node->name] = alloca;

    // 保存变量类型信息（用于数组访问）
    if (node->type) {
        variableTypes[node->name] = node->type->typeName;
    }
    
    // 清理变量声明中产生的临时内存
    clearTempMemory();
}

void CodeGenerator::codegenAssignment(AssignmentNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Assignment operation" << std::endl;
    }

    // 检查是否是数组元素赋值
    auto arrayAccess = dynamic_cast<ArrayAccessNode *>(node->target.get());
    if (arrayAccess) {
        // 处理数组元素赋值: arr[index] = value
        llvm::Value *index = codegenExpr(arrayAccess->index.get());
        if (!index) {
            std::cerr << "Error: Invalid index in array assignment" << std::endl;
            return;
        }

        // 获取数组变量
        llvm::Value *arrayPtr = nullptr;
        llvm::Type *arrayType = nullptr;
        llvm::Type *elementType = nullptr;
        
        if (auto identNode = dynamic_cast<IdentifierNode *>(arrayAccess->array.get())) {
            std::string arrayVarName = identNode->name;
            
            // 从符号表中查找
            auto it = namedValues.find(arrayVarName);
            if (it == namedValues.end()) {
                std::cerr << "Error: Undefined array variable '" << arrayVarName << "'" << std::endl;
                return;
            }
            
            arrayPtr = it->second;
            
            // 获取分配的类型
            if (auto allocaInst = llvm::dyn_cast<llvm::AllocaInst>(arrayPtr)) {
                arrayType = allocaInst->getAllocatedType();
                
                // 判断是数组、数组参数还是字符串
                if (arrayType->isArrayTy()) {
                    llvm::ArrayType *arrType = llvm::cast<llvm::ArrayType>(arrayType);
                    elementType = arrType->getElementType();
                } else if (arrayType->isPointerTy()) {
                    // 数组参数（作为指针传递）
                    // 对于不透明指针，我们需要知道元素类型，暂时假设为 i32
                    // TODO: 需要更好的类型跟踪系统
                    elementType = llvm::Type::getInt32Ty(*context);
                } else {
                    // 字符串或其他指针
                    elementType = llvm::Type::getInt8Ty(*context);
                }
            } else {
                std::cerr << "Error: Not an array variable" << std::endl;
                return;
            }
        } else {
            std::cerr << "Error: Array assignment target must be a variable" << std::endl;
            return;
        }

        // 计算赋值表达式的值
        llvm::Value *value = codegenExpr(node->value.get());
        if (!value) {
            std::cerr << "Error: Invalid assignment value for array element" << std::endl;
            return;
        }

        // 创建GEP获取元素指针
        llvm::Value *ptr = nullptr;
        if (arrayType && arrayType->isArrayTy()) {
            // 数组类型：使用 GEP(array, 0, index)
            std::vector<llvm::Value*> indices;
            indices.push_back(llvm::ConstantInt::get(*context, llvm::APInt(64, 0)));
            indices.push_back(index);
            ptr = builder->CreateGEP(arrayType, arrayPtr, indices, "arrayptr");
        } else if (arrayType && arrayType->isPointerTy()) {
            // 数组参数（指针类型）：需要先加载指针值，然后使用 GEP
            llvm::Value *loadedPtr = builder->CreateLoad(arrayType, arrayPtr, "loaded_ptr");
            ptr = builder->CreateGEP(elementType, loadedPtr, index, "arrayptr");
        } else {
            // 指针类型（字符串等）：使用 GEP(ptr, index)
            ptr = builder->CreateGEP(elementType, arrayPtr, index, "arrayptr");
        }

        // 处理复合赋值操作符
        if (node->op != "=") {
            // 先加载当前值
            llvm::Value *oldVal = builder->CreateLoad(elementType, ptr, "oldval");
            bool isFloat = oldVal->getType()->isDoubleTy();

            // 类型匹配
            if (value->getType() != oldVal->getType()) {
                value = convertToType(value, oldVal->getType());
            }

            // 执行复合操作
            if (node->op == "+=") {
                value = isFloat
                            ? builder->CreateFAdd(oldVal, value, "addassign")
                            : builder->CreateAdd(oldVal, value, "addassign");
            } else if (node->op == "-=") {
                value = isFloat
                            ? builder->CreateFSub(oldVal, value, "subassign")
                            : builder->CreateSub(oldVal, value, "subassign");
            } else if (node->op == "*=") {
                value = isFloat
                            ? builder->CreateFMul(oldVal, value, "mulassign")
                            : builder->CreateMul(oldVal, value, "mulassign");
            } else if (node->op == "/=") {
                value = isFloat
                            ? builder->CreateFDiv(oldVal, value, "divassign")
                            : builder->CreateSDiv(oldVal, value, "divassign");
            } else if (node->op == "//=") {
                // 整除赋值：先转换为整数再执行整除
                llvm::Value *leftInt = oldVal;
                llvm::Value *rightInt = value;
                if (oldVal->getType()->isDoubleTy()) {
                    leftInt = builder->CreateFPToSI(
                        oldVal, llvm::Type::getInt32Ty(*context),
                        "floordiv_left");
                }
                if (value->getType()->isDoubleTy()) {
                    rightInt = builder->CreateFPToSI(
                        value, llvm::Type::getInt32Ty(*context),
                        "floordiv_right");
                }
                value =
                    builder->CreateSDiv(leftInt, rightInt, "floordivassign");
            } else if (node->op == "%=") {
                value = isFloat
                            ? builder->CreateFRem(oldVal, value, "modassign")
                            : builder->CreateSRem(oldVal, value, "modassign");
            }
        }

        if (value->getType() != elementType) {
            value = convertToType(value, elementType);
        }

        // 存储值到数组元素
        builder->CreateStore(value, ptr);
        return;
    }

    // 处理普通变量赋值
    auto ident = dynamic_cast<IdentifierNode *>(node->target.get());
    if (!ident) {
        std::cerr << "Error: Assignment target must be a variable identifier "
                     "or array element"
                  << std::endl;
        return;
    }

    // 先检查局部变量
    llvm::AllocaInst *alloca = namedValues[ident->name];

    // 如果不是局部变量，检查全局变量
    if (!alloca) {
        llvm::GlobalVariable *globalVar = globalValues[ident->name];
        if (!globalVar) {
            std::cerr << "Error: Undefined variable '" << ident->name << "'"
                      << std::endl;
            return;
        }

        // 处理全局变量赋值
        llvm::Value *value = codegenExpr(node->value.get());
        if (!value) {
            std::cerr << "Error: Invalid assignment value for variable '"
                      << ident->name << "'" << std::endl;
            return;
        }

        // 处理复合赋值操作符（全局变量）
        if (node->op != "=") {
            llvm::Value *oldVal = builder->CreateLoad(globalVar->getValueType(),
                                                      globalVar, "oldval");
            bool isFloat = oldVal->getType()->isDoubleTy();

            // 类型匹配：确保value的类型与oldVal一致
            if (value->getType() != oldVal->getType()) {
                value = convertToType(value, oldVal->getType());
            }

            if (node->op == "+=") {
                value = isFloat
                            ? builder->CreateFAdd(oldVal, value, "addassign")
                            : builder->CreateAdd(oldVal, value, "addassign");
            } else if (node->op == "-=") {
                value = isFloat
                            ? builder->CreateFSub(oldVal, value, "subassign")
                            : builder->CreateSub(oldVal, value, "subassign");
            } else if (node->op == "*=") {
                value = isFloat
                            ? builder->CreateFMul(oldVal, value, "mulassign")
                            : builder->CreateMul(oldVal, value, "mulassign");
            } else if (node->op == "/=") {
                // 除零检查
                llvm::Function *function =
                    builder->GetInsertBlock()->getParent();
                if (isFloat) {
                    llvm::Value *zero = llvm::ConstantFP::get(
                        llvm::Type::getDoubleTy(*context), 0.0);
                    llvm::Value *isZero =
                        builder->CreateFCmpOEQ(value, zero, "iszero");

                    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                        *context, "divassign_error", function);
                    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                        *context, "divassign_compute", function);
                    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                        *context, "divassign_merge", function);

                    builder->CreateCondBr(isZero, errorBB, computeBB);

                    builder->SetInsertPoint(errorBB);
                    llvm::Function *printfFunc = getPrintfFunction();
                    llvm::Value *errorMsg = builder->CreateGlobalString(
                        "Runtime Error: Division by zero in /= operator\n", "",
                        0, module.get());
                    builder->CreateCall(printfFunc, {errorMsg});
                    llvm::Value *nan = llvm::ConstantFP::getNaN(
                        llvm::Type::getDoubleTy(*context));
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(computeBB);
                    llvm::Value *divResult =
                        builder->CreateFDiv(oldVal, value, "divassign");
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(mergeBB);
                    llvm::PHINode *phi =
                        builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2,
                                           "divassign_result");
                    phi->addIncoming(nan, errorBB);
                    phi->addIncoming(divResult, computeBB);
                    value = phi;
                } else {
                    llvm::Value *zero = llvm::ConstantInt::get(
                        llvm::Type::getInt32Ty(*context), 0);
                    llvm::Value *isZero =
                        builder->CreateICmpEQ(value, zero, "iszero");

                    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                        *context, "divassign_error", function);
                    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                        *context, "divassign_compute", function);
                    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                        *context, "divassign_merge", function);

                    builder->CreateCondBr(isZero, errorBB, computeBB);

                    builder->SetInsertPoint(errorBB);
                    llvm::Function *printfFunc = getPrintfFunction();
                    llvm::Value *errorMsg = builder->CreateGlobalString(
                        "Runtime Error: Division by zero in /= operator\n", "",
                        0, module.get());
                    builder->CreateCall(printfFunc, {errorMsg});
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(computeBB);
                    llvm::Value *divResult =
                        builder->CreateSDiv(oldVal, value, "divassign");
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(mergeBB);
                    llvm::PHINode *phi =
                        builder->CreatePHI(llvm::Type::getInt32Ty(*context), 2,
                                           "divassign_result");
                    phi->addIncoming(zero, errorBB);
                    phi->addIncoming(divResult, computeBB);
                    value = phi;
                }
            } else if (node->op == "//=") {
                // 整除赋值：先转换为整数再执行整除
                llvm::Value *leftInt = oldVal;
                llvm::Value *rightInt = value;
                if (oldVal->getType()->isDoubleTy()) {
                    leftInt = builder->CreateFPToSI(
                        oldVal, llvm::Type::getInt32Ty(*context),
                        "floordiv_left");
                }
                if (value->getType()->isDoubleTy()) {
                    rightInt = builder->CreateFPToSI(
                        value, llvm::Type::getInt32Ty(*context),
                        "floordiv_right");
                }

                // 除零检查
                llvm::Function *function =
                    builder->GetInsertBlock()->getParent();
                llvm::Value *zero =
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
                llvm::Value *isZero =
                    builder->CreateICmpEQ(rightInt, zero, "iszero");

                llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                    *context, "floordivassign_error", function);
                llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                    *context, "floordivassign_compute", function);

                builder->CreateCondBr(isZero, errorBB, computeBB);

                // 错误分支：打印错误信息并终止程序
                builder->SetInsertPoint(errorBB);
                llvm::Function *printfFunc = getPrintfFunction();
                llvm::Value *errorMsg = builder->CreateGlobalString(
                    "Runtime Error: Integer division by zero in //= operator\n",
                    "", 0, module.get());
                builder->CreateCall(printfFunc, {errorMsg});
                // 调用 exit(1) 终止程序
                llvm::Function *exitFunc = module->getFunction("exit");
                builder->CreateCall(exitFunc,
                                    {llvm::ConstantInt::get(
                                        llvm::Type::getInt32Ty(*context), 1)});
                builder->CreateUnreachable();

                // 计算分支
                builder->SetInsertPoint(computeBB);
                llvm::Value *floordivResult =
                    builder->CreateSDiv(leftInt, rightInt, "floordivassign");
                value = floordivResult;
            } else if (node->op == "%=") {
                // 模运算除零检查
                llvm::Function *function =
                    builder->GetInsertBlock()->getParent();
                if (isFloat) {
                    llvm::Value *zero = llvm::ConstantFP::get(
                        llvm::Type::getDoubleTy(*context), 0.0);
                    llvm::Value *isZero =
                        builder->CreateFCmpOEQ(value, zero, "iszero");

                    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                        *context, "modassign_error", function);
                    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                        *context, "modassign_compute", function);
                    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                        *context, "modassign_merge", function);

                    builder->CreateCondBr(isZero, errorBB, computeBB);

                    builder->SetInsertPoint(errorBB);
                    llvm::Function *printfFunc = getPrintfFunction();
                    llvm::Value *errorMsg = builder->CreateGlobalString(
                        "Runtime Error: Modulo by zero in %= operator\n", "", 0,
                        module.get());
                    builder->CreateCall(printfFunc, {errorMsg});
                    llvm::Value *nan = llvm::ConstantFP::getNaN(
                        llvm::Type::getDoubleTy(*context));
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(computeBB);
                    llvm::Value *modResult =
                        builder->CreateFRem(oldVal, value, "modassign");
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(mergeBB);
                    llvm::PHINode *phi =
                        builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2,
                                           "modassign_result");
                    phi->addIncoming(nan, errorBB);
                    phi->addIncoming(modResult, computeBB);
                    value = phi;
                } else {
                    llvm::Value *zero =
                        llvm::ConstantInt::get(oldVal->getType(), 0);
                    llvm::Value *isZero =
                        builder->CreateICmpEQ(value, zero, "iszero");

                    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                        *context, "modassign_error", function);
                    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                        *context, "modassign_compute", function);
                    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                        *context, "modassign_merge", function);

                    builder->CreateCondBr(isZero, errorBB, computeBB);

                    builder->SetInsertPoint(errorBB);
                    llvm::Function *printfFunc = getPrintfFunction();
                    llvm::Value *errorMsg = builder->CreateGlobalString(
                        "Runtime Error: Modulo by zero in %= operator\n", "", 0,
                        module.get());
                    builder->CreateCall(printfFunc, {errorMsg});
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(computeBB);
                    llvm::Value *modResult =
                        builder->CreateSRem(oldVal, value, "modassign");
                    builder->CreateBr(mergeBB);

                    builder->SetInsertPoint(mergeBB);
                    llvm::PHINode *phi = builder->CreatePHI(
                        oldVal->getType(), 2, "modassign_result");
                    phi->addIncoming(zero, errorBB);
                    phi->addIncoming(modResult, computeBB);
                    value = phi;
                }
            }
        }

        builder->CreateStore(value, globalVar);

        // 如果赋值的是临时内存，从临时栈中移除（转移所有权）
        if (value->getType()->isPointerTy()) {
            removeTempMemory(value);
        }
        return;
    }

    // 处理局部变量赋值
    llvm::Value *value = codegenExpr(node->value.get());
    if (!value) {
        std::cerr << "Error: Invalid assignment value for variable '"
                  << ident->name << "'" << std::endl;
        return;
    }

    // 处理复合赋值操作符（局部变量）
    if (node->op != "=") {
        llvm::Value *oldVal =
            builder->CreateLoad(alloca->getAllocatedType(), alloca, "oldval");
        bool isFloat = oldVal->getType()->isDoubleTy();

        // 类型匹配：确保value的类型与oldVal一致
        if (value->getType() != oldVal->getType()) {
            value = convertToType(value, oldVal->getType());
        }

        if (node->op == "+=") {
            value = isFloat ? builder->CreateFAdd(oldVal, value, "addassign")
                            : builder->CreateAdd(oldVal, value, "addassign");
        } else if (node->op == "-=") {
            value = isFloat ? builder->CreateFSub(oldVal, value, "subassign")
                            : builder->CreateSub(oldVal, value, "subassign");
        } else if (node->op == "*=") {
            value = isFloat ? builder->CreateFMul(oldVal, value, "mulassign")
                            : builder->CreateMul(oldVal, value, "mulassign");
        } else if (node->op == "/=") {
            // 除零检查（局部变量）
            llvm::Function *function = builder->GetInsertBlock()->getParent();
            if (isFloat) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                llvm::Value *isZero =
                    builder->CreateFCmpOEQ(value, zero, "iszero");

                llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                    *context, "divassign_error", function);
                llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                    *context, "divassign_compute", function);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                    *context, "divassign_merge", function);

                builder->CreateCondBr(isZero, errorBB, computeBB);

                builder->SetInsertPoint(errorBB);
                llvm::Function *printfFunc = getPrintfFunction();
                llvm::Value *errorMsg = builder->CreateGlobalString(
                    "Runtime Error: Division by zero in /= operator\n", "", 0,
                    module.get());
                builder->CreateCall(printfFunc, {errorMsg});
                llvm::Value *nan =
                    llvm::ConstantFP::getNaN(llvm::Type::getDoubleTy(*context));
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(computeBB);
                llvm::Value *divResult =
                    builder->CreateFDiv(oldVal, value, "divassign");
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(mergeBB);
                llvm::PHINode *phi = builder->CreatePHI(
                    llvm::Type::getDoubleTy(*context), 2, "divassign_result");
                phi->addIncoming(nan, errorBB);
                phi->addIncoming(divResult, computeBB);
                value = phi;
            } else {
                llvm::Value *zero =
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
                llvm::Value *isZero =
                    builder->CreateICmpEQ(value, zero, "iszero");

                llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                    *context, "divassign_error", function);
                llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                    *context, "divassign_compute", function);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                    *context, "divassign_merge", function);

                builder->CreateCondBr(isZero, errorBB, computeBB);

                builder->SetInsertPoint(errorBB);
                llvm::Function *printfFunc = getPrintfFunction();
                llvm::Value *errorMsg = builder->CreateGlobalString(
                    "Runtime Error: Division by zero in /= operator\n", "", 0,
                    module.get());
                builder->CreateCall(printfFunc, {errorMsg});
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(computeBB);
                llvm::Value *divResult =
                    builder->CreateSDiv(oldVal, value, "divassign");
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(mergeBB);
                llvm::PHINode *phi = builder->CreatePHI(
                    llvm::Type::getInt32Ty(*context), 2, "divassign_result");
                phi->addIncoming(zero, errorBB);
                phi->addIncoming(divResult, computeBB);
                value = phi;
            }
        } else if (node->op == "//=") {
            // 整除赋值（局部变量）：先转换为整数再执行整除
            llvm::Value *leftInt = oldVal;
            llvm::Value *rightInt = value;
            if (oldVal->getType()->isDoubleTy()) {
                leftInt = builder->CreateFPToSI(
                    oldVal, llvm::Type::getInt32Ty(*context), "floordiv_left");
            }
            if (value->getType()->isDoubleTy()) {
                rightInt = builder->CreateFPToSI(
                    value, llvm::Type::getInt32Ty(*context), "floordiv_right");
            }

            // 除零检查
            llvm::Function *function = builder->GetInsertBlock()->getParent();
            llvm::Value *zero =
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
            llvm::Value *isZero =
                builder->CreateICmpEQ(rightInt, zero, "iszero");

            llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                *context, "floordivassign_error", function);
            llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                *context, "floordivassign_compute", function);

            builder->CreateCondBr(isZero, errorBB, computeBB);

            // 错误分支：打印错误信息并终止程序
            builder->SetInsertPoint(errorBB);
            llvm::Function *printfFunc = getPrintfFunction();
            llvm::Value *errorMsg = builder->CreateGlobalString(
                "Runtime Error: Integer division by zero in //= operator\n", "",
                0, module.get());
            builder->CreateCall(printfFunc, {errorMsg});
            // 调用 exit(1) 终止程序
            llvm::Function *exitFunc = module->getFunction("exit");
            builder->CreateCall(
                exitFunc,
                {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1)});
            builder->CreateUnreachable();

            // 计算分支
            builder->SetInsertPoint(computeBB);
            llvm::Value *floordivResult =
                builder->CreateSDiv(leftInt, rightInt, "floordivassign");
            value = floordivResult;
        } else if (node->op == "%=") {
            // 模运算除零检查（局部变量）
            llvm::Function *function = builder->GetInsertBlock()->getParent();
            if (isFloat) {
                llvm::Value *zero = llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(*context), 0.0);
                llvm::Value *isZero =
                    builder->CreateFCmpOEQ(value, zero, "iszero");

                llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                    *context, "modassign_error", function);
                llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                    *context, "modassign_compute", function);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                    *context, "modassign_merge", function);

                builder->CreateCondBr(isZero, errorBB, computeBB);

                builder->SetInsertPoint(errorBB);
                llvm::Function *printfFunc = getPrintfFunction();
                llvm::Value *errorMsg = builder->CreateGlobalString(
                    "Runtime Error: Modulo by zero in %= operator\n", "", 0,
                    module.get());
                builder->CreateCall(printfFunc, {errorMsg});
                llvm::Value *nan =
                    llvm::ConstantFP::getNaN(llvm::Type::getDoubleTy(*context));
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(computeBB);
                llvm::Value *modResult =
                    builder->CreateFRem(oldVal, value, "modassign");
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(mergeBB);
                llvm::PHINode *phi = builder->CreatePHI(
                    llvm::Type::getDoubleTy(*context), 2, "modassign_result");
                phi->addIncoming(nan, errorBB);
                phi->addIncoming(modResult, computeBB);
                value = phi;
            } else {
                llvm::Value *zero =
                    llvm::ConstantInt::get(oldVal->getType(), 0);
                llvm::Value *isZero =
                    builder->CreateICmpEQ(value, zero, "iszero");

                llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(
                    *context, "modassign_error", function);
                llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(
                    *context, "modassign_compute", function);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(
                    *context, "modassign_merge", function);

                builder->CreateCondBr(isZero, errorBB, computeBB);

                builder->SetInsertPoint(errorBB);
                llvm::Function *printfFunc = getPrintfFunction();
                llvm::Value *errorMsg = builder->CreateGlobalString(
                    "Runtime Error: Modulo by zero in %= operator\n", "", 0,
                    module.get());
                builder->CreateCall(printfFunc, {errorMsg});
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(computeBB);
                llvm::Value *modResult =
                    builder->CreateSRem(oldVal, value, "modassign");
                builder->CreateBr(mergeBB);

                builder->SetInsertPoint(mergeBB);
                llvm::PHINode *phi = builder->CreatePHI(oldVal->getType(), 2,
                                                        "modassign_result");
                phi->addIncoming(zero, errorBB);
                phi->addIncoming(modResult, computeBB);
                value = phi;
            }
        }
    }

    builder->CreateStore(value, alloca);

    // 如果赋值的是临时内存（如字符串拼接结果），从临时栈中移除并跟踪所有权
    // 因为现在变量拥有这块内存的所有权
    if (value->getType()->isPointerTy()) {
        // 检查是否是从临时内存转移所有权
        auto it = std::find(tempMemoryStack.begin(), tempMemoryStack.end(), value);
        if (it != tempMemoryStack.end()) {
            // 是临时内存，转移所有权给变量
            removeTempMemory(value);
            trackOwnedString(ident->name, value);
        }
    }
    
    // 清理赋值语句中产生的其他临时内存
    clearTempMemory();
}

void CodeGenerator::codegenBlock(BlockNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Block" << std::endl;
    }

    for (auto &stmt : node->statements) {
        codegenStmt(stmt.get());
    }
    
    // 清理代码块中产生的临时内存
    clearTempMemory();
}

void CodeGenerator::codegenIfStmt(IfStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] If statement" << std::endl;
    }

    llvm::Value *condVal = codegenExpr(node->condition.get());
    if (!condVal)
        return;

    if (!condVal->getType()->isIntegerTy(1)) {
        condVal = builder->CreateICmpNE(
            condVal, llvm::ConstantInt::get(condVal->getType(), 0), "ifcond");
    }

    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *thenBB =
        llvm::BasicBlock::Create(*context, "then", function);
    llvm::BasicBlock *elseBB =
        node->elseBranch ? llvm::BasicBlock::Create(*context, "else") : nullptr;
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "ifcont");

    builder->CreateCondBr(condVal, thenBB, elseBB ? elseBB : mergeBB);

    builder->SetInsertPoint(thenBB);
    codegenStmt(node->thenBranch.get());
    // 检查当前插入点的块是否有终止符
    bool thenHasTerminator =
        builder->GetInsertBlock()->getTerminator() != nullptr;
    if (!thenHasTerminator) {
        builder->CreateBr(mergeBB);
    }

    bool elseHasTerminator = false;
    if (elseBB) {
        function->insert(function->end(), elseBB);
        builder->SetInsertPoint(elseBB);
        codegenStmt(node->elseBranch.get());
        // 检查当前插入点的块是否有终止符（可能已经不是 elseBB 了）
        elseHasTerminator =
            builder->GetInsertBlock()->getTerminator() != nullptr;
        if (!elseHasTerminator) {
            builder->CreateBr(mergeBB);
        }
    }

    // mergeBB 在以下情况需要插入：
    // 1. then 分支没有终止符（会跳转到 mergeBB）
    // 2. else 分支存在且没有终止符（会跳转到 mergeBB）
    // 3. 没有 else 分支（条件为 false 时直接跳到 mergeBB）
    bool needMergeBB =
        !thenHasTerminator || !elseBB || (elseBB && !elseHasTerminator);

    if (needMergeBB) {
        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);
    } else {
        // 两个分支都有终止符，mergeBB 不会被使用
        // 不要删除，而是插入一个不可达块，让 LLVM 优化掉
        function->insert(function->end(), mergeBB);
        builder->SetInsertPoint(mergeBB);
        // 添加一个 unreachable 指令
        builder->CreateUnreachable();
    }
}

void CodeGenerator::codegenWhileStmt(WhileStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] While loop" << std::endl;
    }

    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *condBB =
        llvm::BasicBlock::Create(*context, "whilecond", function);
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*context, "whilebody");
    llvm::BasicBlock *afterBB =
        llvm::BasicBlock::Create(*context, "afterwhile");

    // 添加循环上下文（用于 break/continue）
    LoopContext loopCtx;
    loopCtx.continueBlock = condBB;  // continue 跳转到条件检查
    loopCtx.breakBlock = afterBB;    // break 跳转到循环后
    loopContextStack.push_back(loopCtx);

    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    // 求值条件表达式
    llvm::Value *condVal = codegenExpr(node->condition.get());
    if (!condVal) {
        loopContextStack.pop_back();
        return;
    }

    if (!condVal->getType()->isIntegerTy(1)) {
        condVal = builder->CreateICmpNE(
            condVal, llvm::ConstantInt::get(condVal->getType(), 0),
            "whilecond");
    }
    
    // 清理条件表达式求值产生的临时内存
    // 这对于长循环非常重要，避免每次迭代都累积临时内存
    clearTempMemory();

    builder->CreateCondBr(condVal, bodyBB, afterBB);

    function->insert(function->end(), bodyBB);
    builder->SetInsertPoint(bodyBB);
    codegenStmt(node->body.get());

    // 只有当循环体没有终止符时才添加跳转
    if (!builder->GetInsertBlock()->getTerminator()) {
        // 在跳回条件块前清理循环体产生的临时内存
        clearTempMemory();
        builder->CreateBr(condBB);
    }

    function->insert(function->end(), afterBB);
    builder->SetInsertPoint(afterBB);
    
    // 弹出循环上下文
    loopContextStack.pop_back();
}

void CodeGenerator::codegenForStmt(ForStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] For loop: " << node->variable << " in range"
                  << std::endl;
    }

    llvm::Function *function = builder->GetInsertBlock()->getParent();

    // 检查循环变量名是否与已存在的局部变量冲突
    if (namedValues.find(node->variable) != namedValues.end()) {
        std::cerr << "Error: For loop variable '" << node->variable
                  << "' shadows an existing local variable" << std::endl;
        return;
    }

    llvm::AllocaInst *loopVar = createEntryBlockAlloca(
        function, node->variable, llvm::Type::getInt32Ty(*context));
    namedValues[node->variable] = loopVar;

    llvm::Value *startVal = codegenExpr(node->start.get());
    builder->CreateStore(startVal, loopVar);
    
    // 清理起始值求值产生的临时内存
    clearTempMemory();

    // 在循环开始前计算并存储结束值，避免每次迭代重复计算
    llvm::Value *endVal = codegenExpr(node->end.get());
    llvm::AllocaInst *endVar = createEntryBlockAlloca(
        function, node->variable + "_end", llvm::Type::getInt32Ty(*context));
    builder->CreateStore(endVal, endVar);
    
    // 清理结束值求值产生的临时内存
    clearTempMemory();

    llvm::BasicBlock *condBB =
        llvm::BasicBlock::Create(*context, "forcond", function);
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(*context, "forbody");
    llvm::BasicBlock *incrBB = llvm::BasicBlock::Create(*context, "forincr");
    llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(*context, "afterfor");

    // 添加循环上下文（用于 break/continue）
    LoopContext loopCtx;
    loopCtx.continueBlock = incrBB;  // continue 跳转到递增块
    loopCtx.breakBlock = afterBB;    // break 跳转到循环后
    loopContextStack.push_back(loopCtx);

    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    llvm::Value *currentVal =
        builder->CreateLoad(llvm::Type::getInt32Ty(*context), loopVar, "i");
    llvm::Value *loadedEndVal =
        builder->CreateLoad(llvm::Type::getInt32Ty(*context), endVar, "end");
    llvm::Value *condVal =
        builder->CreateICmpSLT(currentVal, loadedEndVal, "forcond");

    builder->CreateCondBr(condVal, bodyBB, afterBB);

    function->insert(function->end(), bodyBB);
    builder->SetInsertPoint(bodyBB);
    codegenStmt(node->body.get());

    // 只有当循环体没有终止符时才添加跳转到递增块
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(incrBB);
    }
    
    // 递增块
    function->insert(function->end(), incrBB);
    builder->SetInsertPoint(incrBB);
    
    // 在跳回条件块前清理循环体产生的临时内存
    clearTempMemory();
    
    llvm::Value *nextVal = builder->CreateAdd(
        currentVal, llvm::ConstantInt::get(*context, llvm::APInt(32, 1)),
        "nextvar");
    builder->CreateStore(nextVal, loopVar);
    builder->CreateBr(condBB);

    function->insert(function->end(), afterBB);
    builder->SetInsertPoint(afterBB);
    
    // 弹出循环上下文
    loopContextStack.pop_back();

    // 清理循环变量，使其作用域仅限于循环内
    namedValues.erase(node->variable);
}

void CodeGenerator::codegenReturnStmt(ReturnStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Return statement" << std::endl;
    }

    if (node->value) {
        llvm::Value *retVal = codegenExpr(node->value.get());
        if (!retVal) {
            std::cerr << "Error: Invalid return value" << std::endl;
            return;
        }

        // 获取当前函数的返回类型
        llvm::Type *expectedRetType = currentFunction->getReturnType();

        // 检查返回类型是否匹配
        if (retVal->getType() != expectedRetType) {
            // 尝试类型转换
            if (expectedRetType->isVoidTy()) {
                std::cerr << "Error: Cannot return a value from void function"
                          << std::endl;
                return;
            }

            llvm::Value *converted = convertToType(retVal, expectedRetType);
            if (converted->getType() != expectedRetType) {
                std::cerr << "Warning: Return type mismatch, expected ";
                expectedRetType->print(llvm::errs());
                std::cerr << ", got ";
                retVal->getType()->print(llvm::errs());
                std::cerr << std::endl;
            }
            retVal = converted;
        }

        // 如果返回值是临时内存，从临时栈中移除（转移所有权给调用者）
        if (retVal->getType()->isPointerTy()) {
            removeTempMemory(retVal);
        }
        
        // 在返回前清理其他临时内存
        // 这对于提前返回的函数非常重要，防止内存泄漏
        clearTempMemory();
        
        builder->CreateRet(retVal);
    } else {
        // 检查void返回是否匹配
        if (currentFunction && !currentFunction->getReturnType()->isVoidTy()) {
            std::cerr << "Warning: Empty return in non-void function"
                      << std::endl;
        }
        
        // 即使是void返回，也要清理临时内存
        clearTempMemory();
        
        builder->CreateRetVoid();
    }
}

void CodeGenerator::codegenExprStmt(ExprStmtNode *node) {
    codegenExpr(node->expression.get());
    // 清理表达式语句产生的临时内存
    clearTempMemory();
}

void CodeGenerator::codegenFunctionDecl(FunctionDeclNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Generating function: " << node->name << " -> "
                  << (node->returnType ? node->returnType->typeName : "void")
                  << std::endl;
    }

    // 检查函数是否已定义
    if (module->getFunction(node->name)) {
        std::cerr << "Error: Function '" << node->name << "' is already defined"
                  << std::endl;
        return;
    }

    llvm::Type *retType = node->returnType ? getType(node->returnType->typeName)
                                           : llvm::Type::getVoidTy(*context);

    std::vector<llvm::Type *> paramTypes;
    for (auto &param : node->parameters) {
        llvm::Type *paramType = getType(param->type->typeName);
        
        // 处理数组参数：数组作为参数时传递指针
        if (!param->type->arrayDimensions.empty()) {
            // 对于数组参数，传递指向元素类型的指针
            // 例如：int[5] -> i32*
            paramType = llvm::PointerType::get(*context, 0);
        }
        
        paramTypes.push_back(paramType);
    }

    llvm::FunctionType *funcType =
        llvm::FunctionType::get(retType, paramTypes, false);
    llvm::Function *function = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, node->name, module.get());

    unsigned idx = 0;
    for (auto &arg : function->args()) {
        arg.setName(node->parameters[idx++]->name);
    }

    llvm::BasicBlock *entryBB =
        llvm::BasicBlock::Create(*context, "entry", function);
    builder->SetInsertPoint(entryBB);

    llvm::Function *prevFunction = currentFunction;
    currentFunction = function;
    namedValues.clear();
    variableTypes.clear(); // 清理类型信息，避免函数间类型污染

    // 为每个参数创建alloca并存储参数值
    for (auto &arg : function->args()) {
        llvm::Type *allocaType = arg.getType();
        
        // 对于数组参数（传递为指针），直接使用指针类型的alloca
        // 不需要特殊处理，因为指针已经可以用于数组访问
        
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, std::string(arg.getName()), allocaType);
        builder->CreateStore(&arg, alloca);
        namedValues[std::string(arg.getName())] = alloca;
    }

    if (node->body) {
        codegenBlock(node->body.get());
    }

    // 在函数结束前清理临时内存
    // 如果函数没有显式 return 语句，需要在这里清理临时内存
    if (!builder->GetInsertBlock()->getTerminator()) {
        // 清理函数体执行过程中产生的临时内存
        clearTempMemory();
        
        if (retType->isVoidTy()) {
            builder->CreateRetVoid();
        } else {
            builder->CreateRet(llvm::Constant::getNullValue(retType));
        }
    }

    llvm::verifyFunction(*function, &llvm::errs());
    currentFunction = prevFunction;
    functions[node->name] = function;
}

void CodeGenerator::codegenStmt(StmtNode *node) {
    if (auto varDecl = dynamic_cast<VarDeclNode *>(node))
        codegenVarDecl(varDecl);
    else if (auto assignment = dynamic_cast<AssignmentNode *>(node))
        codegenAssignment(assignment);
    else if (auto blockNode = dynamic_cast<BlockNode *>(node))
        codegenBlock(blockNode);
    else if (auto ifStmt = dynamic_cast<IfStmtNode *>(node))
        codegenIfStmt(ifStmt);
    else if (auto whileStmt = dynamic_cast<WhileStmtNode *>(node))
        codegenWhileStmt(whileStmt);
    else if (auto forStmt = dynamic_cast<ForStmtNode *>(node))
        codegenForStmt(forStmt);
    else if (auto returnStmt = dynamic_cast<ReturnStmtNode *>(node))
        codegenReturnStmt(returnStmt);
    else if (auto breakStmt = dynamic_cast<BreakStmtNode *>(node))
        codegenBreakStmt(breakStmt);
    else if (auto continueStmt = dynamic_cast<ContinueStmtNode *>(node))
        codegenContinueStmt(continueStmt);
    else if (auto switchStmt = dynamic_cast<SwitchStmtNode *>(node))
        codegenSwitchStmt(switchStmt);
    else if (auto exprStmt = dynamic_cast<ExprStmtNode *>(node))
        codegenExprStmt(exprStmt);
    else if (auto funcDecl = dynamic_cast<FunctionDeclNode *>(node))
        codegenFunctionDecl(funcDecl);
    else if (auto tryCatch = dynamic_cast<TryCatchNode *>(node))
        codegenTryCatch(tryCatch);
    else if (auto throwStmt = dynamic_cast<ThrowStmtNode *>(node))
        codegenThrow(throwStmt);
    else if (auto importStmt = dynamic_cast<ImportNode *>(node))
        codegenImport(importStmt);
    else {
        std::cerr << "Error: Unknown statement type" << std::endl;
    }
}

llvm::Value *CodeGenerator::convertToType(llvm::Value *value,
                                          llvm::Type *targetType) {
    if (!value || !targetType)
        return value;
    
    llvm::Type *sourceType = value->getType();
    
    // 类型相同，无需转换
    if (sourceType == targetType)
        return value;
    
    // 1. 整数类型之间的转换（包括 bool, char）
    if (sourceType->isIntegerTy() && targetType->isIntegerTy()) {
        unsigned sourceBits = sourceType->getIntegerBitWidth();
        unsigned targetBits = targetType->getIntegerBitWidth();
        
        if (sourceBits < targetBits) {
            // 扩展：i1/i8 -> i32, i32 -> i64
            // bool (i1) 和 char (i8) 使用零扩展
            if (sourceBits == 1 || sourceBits == 8) {
                return builder->CreateZExt(value, targetType, "zext");
            } else {
                return builder->CreateSExt(value, targetType, "sext");
            }
        } else if (sourceBits > targetBits) {
            // 截断：i32 -> i8, i64 -> i32
            return builder->CreateTrunc(value, targetType, "trunc");
        }
        // 位宽相同但可能是不同的整数类型，使用 bitcast
        return builder->CreateBitCast(value, targetType, "bitcast");
    }
    
    // 2. 整数 -> 浮点数
    if (sourceType->isIntegerTy() && targetType->isDoubleTy()) {
        // bool (i1) 和 char (i8) 视为无符号
        if (sourceType->getIntegerBitWidth() == 1 || 
            sourceType->getIntegerBitWidth() == 8) {
            return builder->CreateUIToFP(value, targetType, "uitofp");
        }
        // 其他整数类型视为有符号
        return builder->CreateSIToFP(value, targetType, "sitofp");
    }
    
    // 3. 浮点数 -> 整数
    if (sourceType->isDoubleTy() && targetType->isIntegerTy()) {
        // 浮点数转整数使用带符号转换
        llvm::Value *intVal = builder->CreateFPToSI(
            value, 
            llvm::Type::getInt32Ty(*context), 
            "fptosi"
        );
        
        // 如果目标不是 i32，需要进一步转换
        if (targetType != llvm::Type::getInt32Ty(*context)) {
            return convertToType(intVal, targetType);
        }
        return intVal;
    }
    
    // 4. 布尔类型特殊处理
    if (targetType->isIntegerTy(1)) {
        // 转换为 bool：非零值为 true
        if (sourceType->isIntegerTy()) {
            llvm::Value *zero = llvm::ConstantInt::get(sourceType, 0);
            return builder->CreateICmpNE(value, zero, "tobool");
        } else if (sourceType->isDoubleTy()) {
            llvm::Value *zero = llvm::ConstantFP::get(sourceType, 0.0);
            return builder->CreateFCmpONE(value, zero, "tobool");
        }
    }
    
    // 5. 字符类型特殊处理（i8）
    if (targetType->isIntegerTy(8)) {
        if (sourceType->isIntegerTy()) {
            // 整数 -> char：截断到 8 位
            return builder->CreateTrunc(value, targetType, "tochar");
        } else if (sourceType->isDoubleTy()) {
            // 浮点 -> char：先转 int32 再截断
            llvm::Value *intVal = builder->CreateFPToSI(
                value, 
                llvm::Type::getInt32Ty(*context), 
                "fptosi"
            );
            return builder->CreateTrunc(intVal, targetType, "tochar");
        }
    }
    
    // 6. 指针类型不转换（字符串等）
    if (sourceType->isPointerTy() || targetType->isPointerTy()) {
        // 指针类型保持原样或使用 bitcast（谨慎使用）
        if (sourceType->isPointerTy() && targetType->isPointerTy()) {
            return builder->CreateBitCast(value, targetType, "ptrcast");
        }
        return value;
    }
    
    // 默认：无法转换，返回原值
    return value;
}

// 获取类型对应的printf格式符
std::string CodeGenerator::getFormatSpecForType(llvm::Type* type) {
    if (type->isIntegerTy(32)) {
        return "%d";
    } else if (type->isIntegerTy(64)) {
        return "%lld";
    } else if (type->isDoubleTy()) {
        return "%.15g";  // 使用 %.15g 保留15位有效数字，自动去除尾随零
    } else if (type->isPointerTy()) {
        return "%s";
    } else if (type->isIntegerTy(8)) {
        return "%c";
    } else if (type->isIntegerTy(1)) {
        return "%s";  // 布尔值需要特殊处理转为字符串
    } else {
        return "%d";  // 默认格式
    }
}

llvm::Value *CodeGenerator::convertToString(llvm::Value *value) {
    if (!value)
        return nullptr;
    
    // 如果已经是字符串（指针类型），直接返回
    if (value->getType()->isPointerTy()) {
        return value;
    }

    // 分配缓冲区用于存储结果字符串 (64 字节足够)
    llvm::Function *mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        std::cerr << "Error: malloc function not found" << std::endl;
        return nullptr;
    }
    
    llvm::Value *bufferSize =
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 64);
    llvm::Value *buffer =
        builder->CreateCall(mallocFunc, {bufferSize}, "str_buffer");

    llvm::Function *sprintfFunc = module->getFunction("sprintf");
    if (!sprintfFunc) {
        std::cerr << "Error: sprintf function not found" << std::endl;
        return nullptr;
    }

    // 根据类型进行转换
    if (value->getType()->isIntegerTy() && !value->getType()->isIntegerTy(1) &&
        !value->getType()->isIntegerTy(8)) {
        // 整数转字符串
        llvm::Value *format =
            builder->CreateGlobalString("%d", "", 0, module.get());
        builder->CreateCall(sprintfFunc, {buffer, format, value});
        pushTempMemory(buffer); // 追踪临时内存
        return buffer;
    } else if (value->getType()->isDoubleTy()) {
        // 浮点数转字符串
        llvm::Value *format =
            builder->CreateGlobalString("%g", "", 0, module.get());
        builder->CreateCall(sprintfFunc, {buffer, format, value});
        pushTempMemory(buffer); // 追踪临时内存
        return buffer;
    } else if (value->getType()->isIntegerTy(1)) {
        // 布尔值转字符串
        llvm::Value *trueStr =
            builder->CreateGlobalString("true", "", 0, module.get());
        llvm::Value *falseStr =
            builder->CreateGlobalString("false", "", 0, module.get());
        
        llvm::Value *selectedStr = 
            builder->CreateSelect(value, trueStr, falseStr, "bool_str");
        
        llvm::Value *format = 
            builder->CreateGlobalString("%s", "", 0, module.get());
        builder->CreateCall(sprintfFunc, {buffer, format, selectedStr});
        pushTempMemory(buffer); // 追踪临时内存
        return buffer;
    } else if (value->getType()->isIntegerTy(8)) {
        // 字符转字符串
        llvm::Value *format =
            builder->CreateGlobalString("%c", "", 0, module.get());
        builder->CreateCall(sprintfFunc, {buffer, format, value});
        pushTempMemory(buffer); // 追踪临时内存
        return buffer;
    }

    // 不支持的类型，返回错误消息
    llvm::Value *errorMsg =
        builder->CreateGlobalString("<unsupported type>", "", 0, module.get());
    
    // 将错误消息复制到动态分配的缓冲区
    llvm::Value *format = 
        builder->CreateGlobalString("%s", "", 0, module.get());
    builder->CreateCall(sprintfFunc, {buffer, format, errorMsg});
    pushTempMemory(buffer); // 追踪临时内存
    return buffer;
}

// 除零检查辅助函数 - 除法
llvm::Value *CodeGenerator::createDivisionWithZeroCheck(llvm::Value *left, llvm::Value *right,
                                                        const std::string &errorMsg,
                                                        bool isIntegerDivision) {
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(*context, "div_error", function);
    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(*context, "div_compute", function);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "div_merge", function);
    
    llvm::Value *isZero;
    llvm::Value *errorValue;
    llvm::Value *divResult;
    
    if (isIntegerDivision) {
        // 整数除法
        llvm::Value *zero = llvm::ConstantInt::get(right->getType(), 0);
        isZero = builder->CreateICmpEQ(right, zero, "is_zero");
        
        builder->CreateCondBr(isZero, errorBB, computeBB);
        
        // 错误分支
        builder->SetInsertPoint(errorBB);
        llvm::Function *printfFunc = getPrintfFunction();
        llvm::Value *errorMsgVal = builder->CreateGlobalString(errorMsg, "", 0, module.get());
        builder->CreateCall(printfFunc, {errorMsgVal});
        
        // 整数除零返回 0
        errorValue = llvm::ConstantInt::get(left->getType(), 0);
        builder->CreateBr(mergeBB);
        
        // 计算分支
        builder->SetInsertPoint(computeBB);
        divResult = builder->CreateSDiv(left, right, "div_result");
        builder->CreateBr(mergeBB);
        
        // 合并分支
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder->CreatePHI(left->getType(), 2, "div_phi");
        phi->addIncoming(errorValue, errorBB);
        phi->addIncoming(divResult, computeBB);
        
        return phi;
    } else {
        // 浮点数除法
        llvm::Value *zero = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), 0.0);
        isZero = builder->CreateFCmpOEQ(right, zero, "is_zero");
        
        builder->CreateCondBr(isZero, errorBB, computeBB);
        
        // 错误分支
        builder->SetInsertPoint(errorBB);
        llvm::Function *printfFunc = getPrintfFunction();
        llvm::Value *errorMsgVal = builder->CreateGlobalString(errorMsg, "", 0, module.get());
        builder->CreateCall(printfFunc, {errorMsgVal});
        
        // 浮点除零返回 NaN
        errorValue = llvm::ConstantFP::getNaN(llvm::Type::getDoubleTy(*context));
        builder->CreateBr(mergeBB);
        
        // 计算分支
        builder->SetInsertPoint(computeBB);
        divResult = builder->CreateFDiv(left, right, "div_result");
        builder->CreateBr(mergeBB);
        
        // 合并分支
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2, "div_phi");
        phi->addIncoming(errorValue, errorBB);
        phi->addIncoming(divResult, computeBB);
        
        return phi;
    }
}

// 除零检查辅助函数 - 取模
llvm::Value *CodeGenerator::createModuloWithZeroCheck(llvm::Value *left, llvm::Value *right,
                                                      const std::string &errorMsg) {
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *errorBB = llvm::BasicBlock::Create(*context, "mod_error", function);
    llvm::BasicBlock *computeBB = llvm::BasicBlock::Create(*context, "mod_compute", function);
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*context, "mod_merge", function);
    
    llvm::Value *isZero;
    llvm::Value *errorValue;
    llvm::Value *modResult;
    
    if (right->getType()->isDoubleTy()) {
        // 浮点数取模
        llvm::Value *zero = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), 0.0);
        isZero = builder->CreateFCmpOEQ(right, zero, "is_zero");
        
        builder->CreateCondBr(isZero, errorBB, computeBB);
        
        // 错误分支
        builder->SetInsertPoint(errorBB);
        llvm::Function *printfFunc = getPrintfFunction();
        llvm::Value *errorMsgVal = builder->CreateGlobalString(errorMsg, "", 0, module.get());
        builder->CreateCall(printfFunc, {errorMsgVal});
        
        errorValue = llvm::ConstantFP::getNaN(llvm::Type::getDoubleTy(*context));
        builder->CreateBr(mergeBB);
        
        // 计算分支
        builder->SetInsertPoint(computeBB);
        modResult = builder->CreateFRem(left, right, "mod_result");
        builder->CreateBr(mergeBB);
        
        // 合并分支
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder->CreatePHI(llvm::Type::getDoubleTy(*context), 2, "mod_phi");
        phi->addIncoming(errorValue, errorBB);
        phi->addIncoming(modResult, computeBB);
        
        return phi;
    } else {
        // 整数取模
        llvm::Value *zero = llvm::ConstantInt::get(right->getType(), 0);
        isZero = builder->CreateICmpEQ(right, zero, "is_zero");
        
        builder->CreateCondBr(isZero, errorBB, computeBB);
        
        // 错误分支
        builder->SetInsertPoint(errorBB);
        llvm::Function *printfFunc = getPrintfFunction();
        llvm::Value *errorMsgVal = builder->CreateGlobalString(errorMsg, "", 0, module.get());
        builder->CreateCall(printfFunc, {errorMsgVal});
        
        errorValue = llvm::ConstantInt::get(left->getType(), 0);
        builder->CreateBr(mergeBB);
        
        // 计算分支
        builder->SetInsertPoint(computeBB);
        modResult = builder->CreateSRem(left, right, "mod_result");
        builder->CreateBr(mergeBB);
        
        // 合并分支
        builder->SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder->CreatePHI(left->getType(), 2, "mod_phi");
        phi->addIncoming(errorValue, errorBB);
        phi->addIncoming(modResult, computeBB);
        
        return phi;
    }
}

// 创建全局构造函数来初始化需要动态初始化的全局变量
void CodeGenerator::createGlobalConstructor() {
    // 如果没有需要动态初始化的全局变量，直接返回
    if (globalInitializers.empty()) {
        return;
    }
    
    if (g_verbose) {
        std::cout << "[IR Gen] Creating global constructor for " 
                  << globalInitializers.size() << " dynamic initializers" << std::endl;
    }
    
    // 创建全局构造函数类型: void ()
    llvm::FunctionType *ctorType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), false);
    
    // 创建全局构造函数
    llvm::Function *ctor = llvm::Function::Create(
        ctorType, llvm::Function::InternalLinkage,
        "__global_init", module.get());
    
    // 创建基本块
    llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*context, "entry", ctor);
    builder->SetInsertPoint(entryBB);
    
    // 保存当前函数上下文
    llvm::Function *savedFunction = currentFunction;
    currentFunction = ctor;
    
    // 为每个需要动态初始化的全局变量生成初始化代码
    for (const auto &init : globalInitializers) {
        // 生成初始化表达式的代码
        llvm::Value *initValue = codegenExpr(init.initializer);
        
        if (initValue) {
            // 存储到全局变量
            builder->CreateStore(initValue, init.variable);
            
            if (g_verbose) {
                std::cout << "[IR Gen] Initialized global variable dynamically" << std::endl;
            }
        } else {
            std::cerr << "Warning: Failed to generate initializer for global variable" 
                      << std::endl;
        }
        
        // 清理初始化过程中产生的临时内存
        clearTempMemory();
    }
    
    // 恢复函数上下文
    currentFunction = savedFunction;
    
    // 返回
    builder->CreateRetVoid();
    
    // 将全局构造函数注册到 llvm.global_ctors
    // llvm.global_ctors 是一个特殊的全局数组，包含程序启动时要调用的函数
    llvm::Type *i32Type = llvm::Type::getInt32Ty(*context);
    llvm::PointerType *voidFuncPtrType = llvm::PointerType::get(*context, 0);
    llvm::PointerType *i8PtrType = llvm::PointerType::get(*context, 0);
    
    // 结构体类型: { i32, void ()*, i8* }
    llvm::StructType *ctorStructType = llvm::StructType::get(
        i32Type, voidFuncPtrType, i8PtrType);
    
    // 创建构造函数数组元素: { 65535, @__global_init, null }
    // 优先级 65535 表示在所有用户代码之前运行
    llvm::Constant *ctorStruct = llvm::ConstantStruct::get(
        ctorStructType,
        llvm::ConstantInt::get(i32Type, 65535),  // 优先级
        ctor,                                     // 构造函数指针
        llvm::Constant::getNullValue(i8PtrType)  // 关联数据（null）
    );
    
    // 创建全局数组: [@__global_init]
    llvm::ArrayType *ctorArrayType = llvm::ArrayType::get(ctorStructType, 1);
    llvm::Constant *ctorArray = llvm::ConstantArray::get(
        ctorArrayType, {ctorStruct});
    
    // 创建 llvm.global_ctors 全局变量
    new llvm::GlobalVariable(
        *module,
        ctorArrayType,
        false,  // 不是常量
        llvm::GlobalValue::AppendingLinkage,  // 使用 appending linkage
        ctorArray,
        "llvm.global_ctors"
    );
    
    if (g_verbose) {
        std::cout << "[IR Gen] Global constructor registered" << std::endl;
    }
}

// 代码生成主入口
bool CodeGenerator::generate(ProgramNode *root) {
    if (!root)
        return false;

    for (auto &stmt : root->statements) {
        codegenStmt(stmt.get());
    }
    
    // 创建全局构造函数（如果有需要动态初始化的全局变量）
    createGlobalConstructor();

    // 检查是否有编译错误
    if (hasErrors()) {
        std::cerr << "\nLLVM IR generation failed with " << errorCount << " error(s)";
        if (warningCount > 0) {
            std::cerr << " and " << warningCount << " warning(s)";
        }
        std::cerr << std::endl;
        return false;
    }

    // 显示警告统计（如果有）
    if (warningCount > 0) {
        std::cerr << "LLVM IR generated with " << warningCount << " warning(s)" << std::endl;
    }

    std::string errorStr;
    llvm::raw_string_ostream errorStream(errorStr);
    if (llvm::verifyModule(*module, &errorStream)) {
        std::cerr << "Module verification failed:" << std::endl
                  << errorStr << std::endl;
        return false;
    }

    return true;
}

// 输出接口

void CodeGenerator::printIR() { module->print(llvm::outs(), nullptr); }

bool CodeGenerator::writeIRToFile(const std::string &filename) {
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return false;
    }
    module->print(dest, nullptr);
    return true;
}

bool CodeGenerator::compileToObjectFile(const std::string &filename) {
    // 初始化所有目标
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    
    // 获取目标三元组
    llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());
    module->setTargetTriple(targetTriple);
    
    if (g_verbose) {
        std::cout << "[CodeGen] Target triple: " << targetTriple.str() << std::endl;
    }
    
    // 查找目标
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple.str(), error);
    
    if (!target) {
        std::cerr << "Error: Failed to lookup target: " << error << std::endl;
        return false;
    }
    
    // 创建 TargetMachine
    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto relocModel = llvm::Reloc::PIC_;  // 位置无关代码
    auto targetMachine = target->createTargetMachine(
        targetTriple, CPU, features, opt, relocModel);
    
    if (!targetMachine) {
        std::cerr << "Error: Failed to create target machine" << std::endl;
        return false;
    }
    
    // 设置模块的数据布局
    module->setDataLayout(targetMachine->createDataLayout());
    
    // 打开输出文件
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    
    if (EC) {
        std::cerr << "Error: Could not open file '" << filename 
                  << "': " << EC.message() << std::endl;
        return false;
    }
    
    // 创建 PassManager 并添加生成目标代码的 Pass
    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::ObjectFile;
    
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        std::cerr << "Error: TargetMachine can't emit a file of this type" 
                  << std::endl;
        return false;
    }
    
    // 运行 Pass 生成目标文件
    pass.run(*module);
    dest.flush();
    
    if (g_verbose) {
        std::cout << "[CodeGen] Object file generated: " << filename << std::endl;
    }
    
    return true;
}

bool CodeGenerator::compileToExecutable(const std::string &filename) {
    // 安全检查：验证文件名不包含危险字符
    if (!isValidFilePath(filename)) {
        std::cerr << "Error: Invalid output filename (contains unsafe characters)" << std::endl;
        return false;
    }
    
    // 1. 先生成 LLVM IR 文件
    std::string llFilename = filename + ".ll";
    if (!writeIRToFile(llFilename)) {
        std::cerr << "Error: Failed to write LLVM IR file" << std::endl;
        return false;
    }

    if (g_verbose) {
        std::cout << "[Compile] LLVM IR written to: " << llFilename
                  << std::endl;
    }

    // 2. 使用 clang 编译 LLVM IR 到可执行文件
    // 使用安全的 fork/exec 方式执行，避免命令注入攻击
    std::vector<std::string> args = {
        "clang",
        "-Wno-override-module",
        llFilename,
        "-o",
        filename
    };

    int result = safeExecuteCommand(args, g_verbose);

    if (result != 0) {
        std::cerr << "Error: Failed to compile to executable (clang exit code: "
                  << result << ")" << std::endl;
        return false;
    }

    // 3. 清理临时的 .ll 文件（可选）
    if (!g_verbose) {
        std::remove(llFilename.c_str());
    }

    if (g_verbose) {
        std::cout << "[Compile] Executable generated: " << filename
                  << std::endl;
    }

    return true;
}

// 异常处理实现 - 使用 setjmp/longjmp 机制
void CodeGenerator::declareExceptionHandlingFunctions() {
    // setjmp 函数
    std::vector<llvm::Type*> setjmpArgs;
    setjmpArgs.push_back(llvm::PointerType::get(*context, 0));
    llvm::FunctionType *setjmpFuncType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), setjmpArgs, false);
    llvm::Function::Create(setjmpFuncType, llvm::Function::ExternalLinkage,
                          "setjmp", module.get());
    
    // longjmp 函数
    std::vector<llvm::Type*> longjmpArgs;
    longjmpArgs.push_back(llvm::PointerType::get(*context, 0));
    longjmpArgs.push_back(llvm::Type::getInt32Ty(*context));
    llvm::FunctionType *longjmpFuncType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context), longjmpArgs, false);
    llvm::Function::Create(longjmpFuncType, llvm::Function::ExternalLinkage,
                          "longjmp", module.get());
}

// 获取 setjmp 函数
llvm::Function* CodeGenerator::getSetjmpFunction() {
    return module->getFunction("setjmp");
}

// 获取 longjmp 函数
llvm::Function* CodeGenerator::getLongjmpFunction() {
    return module->getFunction("longjmp");
}

// 获取或创建异常消息全局变量
llvm::GlobalVariable* CodeGenerator::getOrCreateExceptionMsgGlobal() {
    if (!currentExceptionMsg) {
        // 创建全局字符串缓冲区用于存储异常消息
        llvm::ArrayType *bufType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*context), CodeGenConstants::EXCEPTION_MSG_BUFFER_SIZE);
        currentExceptionMsg = new llvm::GlobalVariable(
            *module, bufType, false,
            llvm::GlobalValue::InternalLinkage,
            llvm::ConstantAggregateZero::get(bufType),
            "__exception_msg");
    }
    return currentExceptionMsg;
}

// 生成 Try-Catch 语句
void CodeGenerator::codegenTryCatch(TryCatchNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Try-Catch statement (complete version)" << std::endl;
    }
    
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::Function *setjmpFunc = getSetjmpFunction();
    llvm::Function *longjmpFunc = getLongjmpFunction();
    
    if (!setjmpFunc || !longjmpFunc) {
        std::cerr << "Error: 异常处理函数不可用" << std::endl;
        return;
    }
    
    // 1. 在栈上分配异常上下文缓冲区
    llvm::ArrayType *jmpBufType = llvm::ArrayType::get(llvm::Type::getInt8Ty(*context), CodeGenConstants::JMP_BUF_SIZE);
    llvm::AllocaInst *jmpBuf = createEntryBlockAlloca(function, "jmp_buf", jmpBufType);
    
    // 压入异常上下文栈
    exceptionContextStack.push_back(jmpBuf);
    
    // 2. 保存当前执行上下文
    llvm::Value *jmpBufPtr = builder->CreatePointerCast(
        jmpBuf, llvm::PointerType::get(*context, 0), "jmpbuf_ptr");
    llvm::Value *setjmpResult = builder->CreateCall(setjmpFunc, {jmpBufPtr}, "setjmp_result");
    
    // 3. 检查返回值：0=正常执行，非0=异常发生
    llvm::Value *isNormalPath = builder->CreateICmpEQ(
        setjmpResult, llvm::ConstantInt::get(*context, llvm::APInt(32, 0)), "is_normal");
    
    // 4. 创建基本块
    llvm::BasicBlock *tryBB = llvm::BasicBlock::Create(*context, "try_block", function);
    llvm::BasicBlock *catchBB = llvm::BasicBlock::Create(*context, "catch_block", function);
    llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(*context, "after_try_catch", function);
    
    builder->CreateCondBr(isNormalPath, tryBB, catchBB);
    
    // 5. 生成try块代码
    builder->SetInsertPoint(tryBB);
    if (node->tryBlock) {
        if (g_verbose) {
            std::cout << "[IR Gen]   Generating try block" << std::endl;
        }
        codegenStmt(node->tryBlock.get());
    }
    
    // try块正常结束，跳转到after块
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(afterBB);
    }
    
    // 6. 生成catch块代码
    builder->SetInsertPoint(catchBB);
    if (node->catchBlock) {
        if (g_verbose) {
            std::cout << "[IR Gen]   Generating catch block" << std::endl;
        }
        
        // 如果catch定义了异常变量，创建局部变量并从全局异常消息复制
        if (!node->exceptionVar.empty()) {
            llvm::GlobalVariable *exceptionMsgGlobal = getOrCreateExceptionMsgGlobal();
            
            // 创建局部字符串变量
            llvm::Type *strType = llvm::PointerType::get(*context, 0);
            llvm::AllocaInst *exceptionVarAlloca = createEntryBlockAlloca(
                function, node->exceptionVar, strType);
            
            // 将全局异常消息的地址存储到局部变量
            llvm::Value *msgPtr = builder->CreatePointerCast(
                exceptionMsgGlobal, strType, "exception_msg_ptr");
            builder->CreateStore(msgPtr, exceptionVarAlloca);
            
            // 添加到符号表
            namedValues[node->exceptionVar] = exceptionVarAlloca;
        }
        
        codegenStmt(node->catchBlock.get());
        
        // 从符号表移除异常变量
        if (!node->exceptionVar.empty()) {
            namedValues.erase(node->exceptionVar);
        }
    }
    
    // catch块结束，跳转到after块
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(afterBB);
    }
    
    // 7. 继续后续代码
    builder->SetInsertPoint(afterBB);
    
    // 从异常上下文栈弹出
    exceptionContextStack.pop_back();
    
    if (g_verbose) {
        std::cout << "[IR Gen]   Try-catch completed" << std::endl;
    }
}

// 生成 Throw 语句
void CodeGenerator::codegenThrow(ThrowStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Throw statement (complete version)" << std::endl;
    }
    
    // 1. 获取异常值并转换为字符串
    llvm::Value *exceptionValue = node->value ? codegenExpr(node->value.get()) : nullptr;
    llvm::Value *errorMsg = nullptr;
    
    if (exceptionValue) {
        if (exceptionValue->getType()->isPointerTy()) {
            errorMsg = exceptionValue;
        } else {
            errorMsg = convertToString(exceptionValue);
        }
    } else {
        errorMsg = builder->CreateGlobalString("Exception thrown", "", 0, module.get());
    }
    
    // 2. 将异常消息复制到全局异常缓冲区
    llvm::GlobalVariable *exceptionMsgGlobal = getOrCreateExceptionMsgGlobal();
    llvm::Function *strcpyFunc = module->getFunction("strcpy");
    if (strcpyFunc) {
        llvm::Value *destPtr = builder->CreatePointerCast(
            exceptionMsgGlobal, llvm::PointerType::get(*context, 0), "dest_ptr");
        builder->CreateCall(strcpyFunc, {destPtr, errorMsg});
    }
    
    // 3. 在抛出异常前清理临时内存
    // 这对异常安全至关重要，防止异常处理时的内存泄漏
    clearTempMemory();
    
    // 4. 检查是否在try块中
    if (exceptionContextStack.empty()) {
        // 没有try块捕获，打印错误并退出
        if (g_verbose) {
            std::cout << "[IR Gen]   No try block to catch exception, will exit" << std::endl;
        }
        
        llvm::Function *printfFunc = getPrintfFunction();
        if (printfFunc) {
            llvm::Value *formatStr = builder->CreateGlobalString(
                "Uncaught exception: %s\\n", "", 0, module.get());
            builder->CreateCall(printfFunc, {formatStr, errorMsg});
        }
        
        llvm::Function *exitFunc = module->getFunction("exit");
        if (exitFunc) {
            llvm::Value *exitCode = llvm::ConstantInt::get(*context, llvm::APInt(32, 1));
            builder->CreateCall(exitFunc, {exitCode});
        }
        
        builder->CreateUnreachable();
        
        // 创建新块用于后续代码（不会执行）
        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock *afterThrowBB = llvm::BasicBlock::Create(
            *context, "after_throw", function);
        builder->SetInsertPoint(afterThrowBB);
    } else {
        // 在try块中，跳转到对应的catch块
        if (g_verbose) {
            std::cout << "[IR Gen]   Exception will be caught by try block" << std::endl;
        }
        
        llvm::Function *longjmpFunc = getLongjmpFunction();
        llvm::AllocaInst *jmpBuf = exceptionContextStack.back();
        
        llvm::Value *jmpBufPtr = builder->CreatePointerCast(
            jmpBuf, llvm::PointerType::get(*context, 0), "jmpbuf_ptr");
        llvm::Value *exceptionCode = llvm::ConstantInt::get(*context, llvm::APInt(32, 1));
        
        builder->CreateCall(longjmpFunc, {jmpBufPtr, exceptionCode});
        builder->CreateUnreachable();  // 不会返回
        
        // 创建新块用于后续代码（不会执行）
        llvm::Function *function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock *afterThrowBB = llvm::BasicBlock::Create(
            *context, "after_throw", function);
        builder->SetInsertPoint(afterThrowBB);
    }
}

// 生成 Break 语句
void CodeGenerator::codegenBreakStmt(BreakStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Break statement" << std::endl;
    }
    
    // 检查是否在循环或switch中
    if (loopContextStack.empty()) {
        std::cerr << "Error: 'break' statement not in loop or switch" << std::endl;
        return;
    }
    
    // 清理临时内存
    clearTempMemory();
    
    // 跳转到循环/switch的结束块
    llvm::BasicBlock *breakBlock = loopContextStack.back().breakBlock;
    builder->CreateBr(breakBlock);
    
    // 创建新的不可达块（break后的代码不会执行）
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *afterBreakBB = llvm::BasicBlock::Create(
        *context, "after_break", function);
    builder->SetInsertPoint(afterBreakBB);
}

// 生成 Continue 语句
void CodeGenerator::codegenContinueStmt(ContinueStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Continue statement" << std::endl;
    }
    
    // 检查是否在循环中
    if (loopContextStack.empty()) {
        std::cerr << "Error: 'continue' statement not in loop" << std::endl;
        return;
    }
    
    // 清理临时内存
    clearTempMemory();
    
    // 跳转到循环的继续块（通常是条件检查或迭代更新）
    llvm::BasicBlock *continueBlock = loopContextStack.back().continueBlock;
    builder->CreateBr(continueBlock);
    
    // 创建新的不可达块（continue后的代码不会执行）
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *afterContinueBB = llvm::BasicBlock::Create(
        *context, "after_continue", function);
    builder->SetInsertPoint(afterContinueBB);
}

// 生成 Switch 语句
void CodeGenerator::codegenSwitchStmt(SwitchStmtNode *node) {
    if (g_verbose) {
        std::cout << "[IR Gen] Switch statement" << std::endl;
    }
    
    // 计算switch条件表达式
    llvm::Value *condValue = codegenExpr(node->condition.get());
    if (!condValue) {
        std::cerr << "Error: Invalid switch condition" << std::endl;
        return;
    }
    
    llvm::Function *function = builder->GetInsertBlock()->getParent();
    
    // 创建基本块
    llvm::BasicBlock *afterSwitchBB = llvm::BasicBlock::Create(
        *context, "after_switch", function);
    
    // 创建switch指令（使用LLVM的switch指令）
    llvm::BasicBlock *defaultBB = nullptr;
    std::vector<std::pair<llvm::BasicBlock*, std::shared_ptr<CaseNode>>> caseBlocks;
    
    // 先创建所有case块
    for (const auto& caseNode : node->cases) {
        llvm::BasicBlock *caseBB = llvm::BasicBlock::Create(
            *context, caseNode->value ? "case" : "default", function);
        caseBlocks.push_back({caseBB, caseNode});
        
        if (!caseNode->value) {
            defaultBB = caseBB;
        }
    }
    
    // 如果没有default，跳转到after_switch
    if (!defaultBB) {
        defaultBB = afterSwitchBB;
    }
    
    // 创建switch指令
    llvm::SwitchInst *switchInst = builder->CreateSwitch(condValue, defaultBB, node->cases.size());
    
    // 添加循环上下文
    LoopContext loopCtx;
    loopCtx.breakBlock = afterSwitchBB;
    loopCtx.continueBlock = nullptr; 
    loopContextStack.push_back(loopCtx);
    
    // 生成每个case的代码
    for (size_t i = 0; i < caseBlocks.size(); i++) {
        llvm::BasicBlock *caseBB = caseBlocks[i].first;
        auto caseNode = caseBlocks[i].second;
        
        builder->SetInsertPoint(caseBB);
        
        // 为非default的case添加到switch指令
        if (caseNode->value) {
            llvm::Value *caseValue = codegenExpr(caseNode->value.get());
            if (caseValue && llvm::isa<llvm::ConstantInt>(caseValue)) {
                switchInst->addCase(llvm::cast<llvm::ConstantInt>(caseValue), caseBB);
            }
        }
        
        // 生成case体的代码
        if (caseNode->body) {
            codegenBlock(caseNode->body.get());
        }
        
        // 如果case体没有显式break，自动跳转到after_switch（不fall-through）
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(afterSwitchBB);
        }
    }
    
    // 弹出循环上下文
    loopContextStack.pop_back();
    
    // 继续在switch后的代码
    builder->SetInsertPoint(afterSwitchBB);
    
    if (g_verbose) {
        std::cout << "[IR Gen]   Switch completed" << std::endl;
    }
}

