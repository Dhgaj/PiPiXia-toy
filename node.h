#ifndef NODE_H
#define NODE_H

// 标准库头文件
#include <string>
#include <vector>
#include <memory>
#include <iostream>

// 前置声明
class ASTNode;
class ExprNode;
class StmtNode;
class TypeNode;

// 基础节点

// AST 节点基类 - 所有节点的基类
class ASTNode {
public:
    int lineNumber = 0;  // 源代码行号，用于错误报告
    
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
};

// 类型节点 - 表示变量和函数的类型
class TypeNode : public ASTNode {
public:
    std::string typeName;
    std::vector<int> arrayDimensions;
    int arraySize;  // 兼容旧接口  
    
    TypeNode(const std::string& name, int arrSize = 0) 
        : typeName(name), arraySize(arrSize) {
        if (arrSize > 0) {
            arrayDimensions.push_back(arrSize);
        }
    }
    
    TypeNode(const std::string& name, const std::vector<int>& dims) 
        : typeName(name), arrayDimensions(dims) {
        arraySize = dims.empty() ? 0 : dims[0];
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Type: " << typeName;
        for (int dim : arrayDimensions) {
            std::cout << "[" << dim << "]";
        }
        std::cout << std::endl;
    }
};

// 表达式节点

// 表达式节点基类
class ExprNode : public ASTNode {
public:
    virtual ~ExprNode() = default;
};

// 字面量表达式

// 整数字面量
class IntLiteralNode : public ExprNode {
public:
    int value;
    
    IntLiteralNode(int val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "IntLiteral: " << value << std::endl;
    }
};

// 浮点数字面量
class DoubleLiteralNode : public ExprNode {
public:
    double value;
    
    DoubleLiteralNode(double val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "DoubleLiteral: " << value << std::endl;
    }
};

// 字符串字面量
class StringLiteralNode : public ExprNode {
public:
    std::string value;
    
    StringLiteralNode(const std::string& val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "StringLiteral: \"" << value << "\"" << std::endl;
    }
};

// 字符串插值 - 支持 "${expr}" 和格式化 "${expr:.2f}"
class InterpolatedStringNode : public ExprNode {
public:
    std::vector<std::string> stringParts;
    std::vector<std::shared_ptr<ExprNode>> expressions;
    std::vector<std::string> formatSpecs;
    
    InterpolatedStringNode() {}
    
    void addStringPart(const std::string& part) {
        stringParts.push_back(part);
    }
    
    void addExpression(std::shared_ptr<ExprNode> expr) {
        expressions.push_back(expr);
        formatSpecs.push_back("");
    }
    
    void addExpression(std::shared_ptr<ExprNode> expr, const std::string& format) {
        expressions.push_back(expr);
        formatSpecs.push_back(format);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "InterpolatedString:" << std::endl;
        std::cout << std::string(indent + 2, ' ') << "Parts: " << stringParts.size() 
                  << ", Exprs: " << expressions.size() << std::endl;
        for (size_t i = 0; i < stringParts.size(); i++) {
            std::cout << std::string(indent + 4, ' ') << "String[" << i << "]: \"" 
                      << stringParts[i] << "\"" << std::endl;
            if (i < expressions.size()) {
                std::cout << std::string(indent + 4, ' ') << "Expr[" << i << "]:" << std::endl;
                expressions[i]->print(indent + 6);
            }
        }
    }
};

// 字符字面量
class CharLiteralNode : public ExprNode {
public:
    char value;
    
    CharLiteralNode(char val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "CharLiteral: '" << value << "'" << std::endl;
    }
};

// 布尔字面量
class BoolLiteralNode : public ExprNode {
public:
    bool value;
    
    BoolLiteralNode(bool val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BoolLiteral: " << (value ? "true" : "false") << std::endl;
    }
};

// 数组字面量节点
class ArrayLiteralNode : public ExprNode {
public:
    std::vector<std::shared_ptr<ExprNode>> elements;
    
    ArrayLiteralNode(std::vector<std::shared_ptr<ExprNode>> elems) 
        : elements(std::move(elems)) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ArrayLiteral: [" << elements.size() << " elements]" << std::endl;
        for (const auto& elem : elements) {
            if (elem) elem->print(indent + 2);
        }
    }
};

// 标识符和访问表达式

// 标识符 - 变量名、函数名等
class IdentifierNode : public ExprNode {
public:
    std::string name;
    
    IdentifierNode(const std::string& n) : name(n) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Identifier: " << name << std::endl;
    }
};

// 运算符表达式

// 二元运算符 - +, -, *, / 等
class BinaryOpNode : public ExprNode {
public:
    std::string op;
    std::shared_ptr<ExprNode> left;
    std::shared_ptr<ExprNode> right;
    
    BinaryOpNode(const std::string& operation, 
                 std::shared_ptr<ExprNode> l, 
                 std::shared_ptr<ExprNode> r)
        : op(operation), left(l), right(r) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BinaryOp: " << op << std::endl;
        if (left) left->print(indent + 2);
        if (right) right->print(indent + 2);
    }
};

// 一元运算符 - !, -, + 等
class UnaryOpNode : public ExprNode {
public:
    std::string op;
    std::shared_ptr<ExprNode> operand;
    
    UnaryOpNode(const std::string& operation, std::shared_ptr<ExprNode> expr)
        : op(operation), operand(expr) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "UnaryOp: " << op << std::endl;
        if (operand) operand->print(indent + 2);
    }
};

// 函数调用表达式

// 函数调用 - func(arg1, arg2, ...)
class FunctionCallNode : public ExprNode {
public:
    std::string functionName;
    std::vector<std::shared_ptr<ExprNode>> arguments;
    std::shared_ptr<ExprNode> object;
    
    FunctionCallNode(const std::string& name) : functionName(name), object(nullptr) {}
    
    void addArgument(std::shared_ptr<ExprNode> arg) {
        arguments.push_back(arg);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "FunctionCall: " << functionName << std::endl;
        if (object) {
            std::cout << std::string(indent + 2, ' ') << "Object:" << std::endl;
            object->print(indent + 4);
        }
        for (const auto& arg : arguments) {
            if (arg) arg->print(indent + 2);
        }
    }
};

// 数组访问
class ArrayAccessNode : public ExprNode {
public:
    std::shared_ptr<ExprNode> array;
    std::shared_ptr<ExprNode> index;
    
    ArrayAccessNode(std::shared_ptr<ExprNode> arr, std::shared_ptr<ExprNode> idx)
        : array(arr), index(idx) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ArrayAccess:" << std::endl;
        if (array) array->print(indent + 2);
        if (index) index->print(indent + 2);
    }
};

// 成员访问 - object.member 语法
class MemberAccessNode : public ExprNode {
public:
    std::shared_ptr<ExprNode> object;
    std::string memberName;
    
    MemberAccessNode(std::shared_ptr<ExprNode> obj, const std::string& member)
        : object(obj), memberName(member) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "MemberAccess: ." << memberName << std::endl;
        if (object) {
            std::cout << std::string(indent + 2, ' ') << "Object:" << std::endl;
            object->print(indent + 4);
        }
    }
};

// 语句节点

// 语句节点基类
class StmtNode : public ASTNode {
public:
    virtual ~StmtNode() = default;
};

// 声明语句

// 变量声明 - let/const 语句
class VarDeclNode : public StmtNode {
public:
    bool isConst;
    std::string name;
    std::shared_ptr<TypeNode> type;
    std::shared_ptr<ExprNode> initializer;
    
    VarDeclNode(bool constant, const std::string& varName, 
                std::shared_ptr<TypeNode> varType, 
                std::shared_ptr<ExprNode> init)
        : isConst(constant), name(varName), type(varType), initializer(init) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << (isConst ? "ConstDecl: " : "VarDecl: ") 
                  << name << std::endl;
        if (type) type->print(indent + 2);
        if (initializer) initializer->print(indent + 2);
    }
};

// 赋值语句

// 赋值语句 - =, +=, -= 等
class AssignmentNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> target;
    std::string op;
    std::shared_ptr<ExprNode> value;
    
    AssignmentNode(std::shared_ptr<ExprNode> tgt, const std::string& operation, 
                   std::shared_ptr<ExprNode> val)
        : target(tgt), op(operation), value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Assignment: " << op << std::endl;
        if (target) target->print(indent + 2);
        if (value) value->print(indent + 2);
    }
};

// 控制流语句

// 代码块 - { statements }
class BlockNode : public StmtNode {
public:
    std::vector<std::shared_ptr<StmtNode>> statements;
    
    void addStatement(std::shared_ptr<StmtNode> stmt) {
        statements.push_back(stmt);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Block:" << std::endl;
        for (const auto& stmt : statements) {
            if (stmt) stmt->print(indent + 2);
        }
    }
};

// If 条件语句
class IfStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> condition;
    std::shared_ptr<StmtNode> thenBranch;
    std::shared_ptr<StmtNode> elseBranch;
    
    IfStmtNode(std::shared_ptr<ExprNode> cond, 
               std::shared_ptr<StmtNode> thenStmt, 
               std::shared_ptr<StmtNode> elseStmt = nullptr)
        : condition(cond), thenBranch(thenStmt), elseBranch(elseStmt) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "IfStmt:" << std::endl;
        if (condition) condition->print(indent + 2);
        if (thenBranch) thenBranch->print(indent + 2);
        if (elseBranch) {
            std::cout << std::string(indent, ' ') << "Else:" << std::endl;
            elseBranch->print(indent + 2);
        }
    }
};

// While 循环语句
class WhileStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> condition;
    std::shared_ptr<StmtNode> body;
    
    WhileStmtNode(std::shared_ptr<ExprNode> cond, std::shared_ptr<StmtNode> bodyStmt)
        : condition(cond), body(bodyStmt) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "WhileStmt:" << std::endl;
        if (condition) condition->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

// For 循环语句
class ForStmtNode : public StmtNode {
public:
    std::string variable;
    std::shared_ptr<ExprNode> start;
    std::shared_ptr<ExprNode> end;
    std::shared_ptr<StmtNode> body;
    
    ForStmtNode(const std::string& var, std::shared_ptr<ExprNode> startExpr,
                std::shared_ptr<ExprNode> endExpr, std::shared_ptr<StmtNode> bodyStmt)
        : variable(var), start(startExpr), end(endExpr), body(bodyStmt) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ForStmt: " << variable << std::endl;
        if (start) start->print(indent + 2);
        if (end) end->print(indent + 2);
        if (body) body->print(indent + 2);
    }
};

// Switch/Case 语句

// Case 分支节点
class CaseNode : public ASTNode {
public:
    std::shared_ptr<ExprNode> value;  
    std::shared_ptr<BlockNode> body;  
    
    CaseNode(std::shared_ptr<ExprNode> val, std::shared_ptr<BlockNode> caseBody)
        : value(val), body(caseBody) {}
    
    void print(int indent = 0) const override {
        if (value) {
            std::cout << std::string(indent, ' ') << "Case:" << std::endl;
            value->print(indent + 2);
        } else {
            std::cout << std::string(indent, ' ') << "Default:" << std::endl;
        }
        if (body) body->print(indent + 2);
    }
};

// Switch 语句
class SwitchStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> condition;
    std::vector<std::shared_ptr<CaseNode>> cases;
    
    SwitchStmtNode(std::shared_ptr<ExprNode> cond) : condition(cond) {}
    
    void addCase(std::shared_ptr<CaseNode> caseNode) {
        cases.push_back(caseNode);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "SwitchStmt:" << std::endl;
        if (condition) {
            std::cout << std::string(indent + 2, ' ') << "Condition:" << std::endl;
            condition->print(indent + 4);
        }
        for (const auto& caseNode : cases) {
            if (caseNode) caseNode->print(indent + 2);
        }
    }
};

// 跳转语句

// Return 语句
class ReturnStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> value;
    
    ReturnStmtNode(std::shared_ptr<ExprNode> val = nullptr) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ReturnStmt:" << std::endl;
        if (value) value->print(indent + 2);
    }
};

// Break 语句 - 退出循环
class BreakStmtNode : public StmtNode {
public:
    BreakStmtNode() {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BreakStmt" << std::endl;
    }
};

// Continue 语句 - 跳过当前迭代
class ContinueStmtNode : public StmtNode {
public:
    ContinueStmtNode() {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ContinueStmt" << std::endl;
    }
};

// 异常处理语句

// Try-Catch 语句（使用 setjmp/longjmp 实现）
class TryCatchNode : public StmtNode {
public:
    std::shared_ptr<StmtNode> tryBlock;
    std::string exceptionVar;
    std::shared_ptr<TypeNode> exceptionType;
    std::shared_ptr<StmtNode> catchBlock;
    
    TryCatchNode(std::shared_ptr<StmtNode> tryStmt, const std::string& excVar,
                 std::shared_ptr<TypeNode> excType, std::shared_ptr<StmtNode> catchStmt)
        : tryBlock(tryStmt), exceptionVar(excVar), exceptionType(excType), catchBlock(catchStmt) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "TryCatch:" << std::endl;
        if (tryBlock) tryBlock->print(indent + 2);
        std::cout << std::string(indent + 2, ' ') << "Catch: " << exceptionVar << std::endl;
        if (exceptionType) exceptionType->print(indent + 4);
        if (catchBlock) catchBlock->print(indent + 2);
    }
};

// Throw 语句 - 抛出异常
class ThrowStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> value;
    
    ThrowStmtNode(std::shared_ptr<ExprNode> val) : value(val) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ThrowStmt:" << std::endl;
        if (value) value->print(indent + 2);
    }
};

// 其他语句

// 表达式语句 - 单独一行的表达式
class ExprStmtNode : public StmtNode {
public:
    std::shared_ptr<ExprNode> expression;
    
    ExprStmtNode(std::shared_ptr<ExprNode> expr) : expression(expr) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ExprStmt:" << std::endl;
        if (expression) expression->print(indent + 2);
    }
};

// 函数相关节点

// 函数参数 - 函数定义的参数
class ParameterNode : public ASTNode {
public:
    std::string name;
    std::shared_ptr<TypeNode> type;
    
    ParameterNode(const std::string& paramName, std::shared_ptr<TypeNode> paramType)
        : name(paramName), type(paramType) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Parameter: " << name << std::endl;
        if (type) type->print(indent + 2);
    }
};

// 函数声明 - func name(params): returnType { body }
class FunctionDeclNode : public StmtNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ParameterNode>> parameters;
    std::shared_ptr<TypeNode> returnType;
    std::shared_ptr<BlockNode> body;
    
    FunctionDeclNode(const std::string& funcName) : name(funcName) {}
    
    void addParameter(std::shared_ptr<ParameterNode> param) {
        parameters.push_back(param);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "FunctionDecl: " << name << std::endl;
        for (const auto& param : parameters) {
            if (param) param->print(indent + 2);
        }
        if (returnType) {
            std::cout << std::string(indent + 2, ' ') << "ReturnType:" << std::endl;
            returnType->print(indent + 4);
        }
        if (body) body->print(indent + 2);
    }
};

// 模块导入节点

// Import 语句 - import module [as alias]
class ImportNode : public StmtNode {
public:
    std::string moduleName;
    std::string alias;
    
    ImportNode(const std::string& module, const std::string& moduleAlias = "")
        : moduleName(module), alias(moduleAlias) {}
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Import: " << moduleName;
        if (!alias.empty()) {
            std::cout << " as " << alias;
        }
        std::cout << std::endl;
    }
};

// 程序根节点 - 整个 AST 的根
class ProgramNode : public ASTNode {
public:
    std::vector<std::shared_ptr<StmtNode>> statements;
    
    void addStatement(std::shared_ptr<StmtNode> stmt) {
        statements.push_back(stmt);
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Program:" << std::endl;
        for (const auto& stmt : statements) {
            if (stmt) stmt->print(indent + 2);
        }
    }
};

#endif // NODE_H
