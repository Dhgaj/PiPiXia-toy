%{
#include <iostream>
#include <string>
#include <memory>
#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>
#include <iomanip>
#include "node.h"

extern int yylex();
extern int yylineno;
extern char* yytext;  
extern bool g_verbose;  
void yyerror(const char* s);

// ANSI 颜色码
static const char* ERR_RED = "\033[1;31m";
static const char* ERR_CYAN = "\033[36m";
static const char* ERR_RESET = "\033[0m";
static const char* ERR_BOLD = "\033[1m";

// 源代码行缓存，用于错误报告时显示上下文
std::vector<std::string> g_sourceLines;
std::string g_currentFile;

// 加载源文件内容到缓存
void loadSourceFile(const std::string& filename) {
    g_currentFile = filename;
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

// 获取指定行的源代码
std::string getSourceLine(int lineNum) {
    if (lineNum > 0 && lineNum <= static_cast<int>(g_sourceLines.size())) {
        return g_sourceLines[lineNum - 1];
    }
    return "";
}

std::shared_ptr<ProgramNode> root;

// 辅助函数：创建节点并自动设置行号
template<typename T, typename... Args>
std::shared_ptr<T> makeNode(Args&&... args) {
    auto node = std::make_shared<T>(std::forward<Args>(args)...);
    node->lineNumber = yylineno;
    return node;
}

// 插值字符串表达式解析器 - 简单表达式分词器
class SimpleTokenizer {
private:
    std::string expr;
    size_t pos;
    
public:
    SimpleTokenizer(const std::string& e) : expr(e), pos(0) {}
    
    void skipWhitespace() {
        while (pos < expr.length() && std::isspace(expr[pos])) pos++;
    }
    
    char peek() {
        skipWhitespace();
        return pos < expr.length() ? expr[pos] : '\0';
    }
    
    char consume() {
        skipWhitespace();
        return pos < expr.length() ? expr[pos++] : '\0';
    }
    
    std::string consumeIdentifier() {
        skipWhitespace();
        std::string ident;
        while (pos < expr.length() && (std::isalnum(expr[pos]) || expr[pos] == '_')) {
            ident += expr[pos++];
        }
        return ident;
    }
    
    std::string consumeNumber() {
        skipWhitespace();
        std::string num;
        bool hasDot = false;
        while (pos < expr.length() && (std::isdigit(expr[pos]) || expr[pos] == '.')) {
            if (expr[pos] == '.') {
                if (hasDot) break;
                hasDot = true;
            }
            num += expr[pos++];
        }
        return num;
    }
    
    bool hasMore() {
        skipWhitespace();
        return pos < expr.length();
    }
    
    size_t getPos() const { return pos; }
    void setPos(size_t p) { pos = p; }
    
    std::string peekString(size_t start, size_t length) const {
        if (start >= expr.length()) return "";
        return expr.substr(start, length);
    }
    
    char charAt(size_t index) const {
        return index < expr.length() ? expr[index] : '\0';
    }
    
    size_t length() const { return expr.length(); }
};

// 表达式解析函数前向声明
std::shared_ptr<ExprNode> parseSimpleExpr(SimpleTokenizer& tok);
std::shared_ptr<ExprNode> parseSimpleMulDiv(SimpleTokenizer& tok);
std::shared_ptr<ExprNode> parseSimpleAddSub(SimpleTokenizer& tok);
std::shared_ptr<ExprNode> parseSimpleComparison(SimpleTokenizer& tok);
std::shared_ptr<ExprNode> parseSimpleLogical(SimpleTokenizer& tok);

// 解析基本表达式（字面量、标识符、括号表达式、数组访问）
std::shared_ptr<ExprNode> parseSimpleExpr(SimpleTokenizer& tok) {
    char ch = tok.peek();
    
    if (ch == '(') {
        tok.consume();
        auto expr = parseSimpleLogical(tok);
        if (tok.peek() == ')') tok.consume();
        return expr;
    }
    
    if (std::isdigit(ch)) {
        std::string numStr = tok.consumeNumber();
        if (numStr.find('.') != std::string::npos) {
            return std::make_shared<DoubleLiteralNode>(std::stod(numStr));
        } else {
            return std::make_shared<IntLiteralNode>(std::stoi(numStr));
        }
    }
    
    if (std::isalpha(ch) || ch == '_') {
        std::string ident = tok.consumeIdentifier();
        std::shared_ptr<ExprNode> expr = std::make_shared<IdentifierNode>(ident);
        
        // 处理数组访问，支持多维数组如 arr[i][j]
        while (tok.peek() == '[') {
            tok.consume();
            auto indexExpr = parseSimpleAddSub(tok);
            if (tok.peek() == ']') tok.consume();
            expr = std::make_shared<ArrayAccessNode>(expr, indexExpr);
        }
        
        return expr;
    }
    
    return nullptr;
}

// 解析乘除模运算（*, /, //, %）
std::shared_ptr<ExprNode> parseSimpleMulDiv(SimpleTokenizer& tok) {
    auto left = parseSimpleExpr(tok);
    if (!left) return nullptr;
    
    while (tok.hasMore()) {
        char op = tok.peek();
        if (op == '*' || op == '%') {
            tok.consume();
            auto right = parseSimpleExpr(tok);
            if (!right) return nullptr;
            std::string opStr(1, op);
            left = std::make_shared<BinaryOpNode>(opStr, left, right);
        } else if (op == '/') {
            tok.consume();
            std::string opStr = "/";
            if (tok.peek() == '/') {
                tok.consume();
                opStr = "//";
            }
            auto right = parseSimpleExpr(tok);
            if (!right) return nullptr;
            left = std::make_shared<BinaryOpNode>(opStr, left, right);
        } else {
            break;
        }
    }
    
    return left;
}

// 解析加减运算（+, -）
std::shared_ptr<ExprNode> parseSimpleAddSub(SimpleTokenizer& tok) {
    auto left = parseSimpleMulDiv(tok);
    if (!left) return nullptr;
    
    while (tok.hasMore()) {
        char op = tok.peek();
        if (op == '+' || op == '-') {
            tok.consume();
            auto right = parseSimpleMulDiv(tok);
            if (!right) return nullptr;
            std::string opStr(1, op);
            left = std::make_shared<BinaryOpNode>(opStr, left, right);
        } else {
            break;
        }
    }
    
    return left;
}

// 解析比较运算（==, !=, <, >, <=, >=）
std::shared_ptr<ExprNode> parseSimpleComparison(SimpleTokenizer& tok) {
    auto left = parseSimpleAddSub(tok);
    if (!left) return nullptr;
    
    tok.skipWhitespace();
    if (tok.hasMore()) {
        size_t currentPos = tok.getPos();
        std::string op;
        
        // 尝试匹配双字符比较运算符
        if (currentPos + 1 < tok.length()) {
            std::string twoChar = tok.peekString(currentPos, 2);
            if (twoChar == "==" || twoChar == "!=" || 
                twoChar == "<=" || twoChar == ">=") {
                op = twoChar;
                tok.setPos(currentPos + 2);
            }
        }
        
        // 尝试匹配单字符比较运算符
        if (op.empty() && currentPos < tok.length()) {
            char ch = tok.charAt(currentPos);
            if (ch == '<' || ch == '>') {
                op = std::string(1, ch);
                tok.setPos(currentPos + 1);
            }
        }
        
        // 找到比较运算符，解析右侧表达式
        if (!op.empty()) {
            auto right = parseSimpleAddSub(tok);
            if (!right) return nullptr;
            left = std::make_shared<BinaryOpNode>(op, left, right);
        }
    }
    
    return left;
}

// 解析逻辑运算（&&, ||）
std::shared_ptr<ExprNode> parseSimpleLogical(SimpleTokenizer& tok) {
    auto left = parseSimpleComparison(tok);
    if (!left) return nullptr;
    
    tok.skipWhitespace();
    while (tok.hasMore()) {
        size_t currentPos = tok.getPos();
        std::string op;
        
        // 尝试匹配逻辑运算符
        if (currentPos + 1 < tok.length()) {
            std::string twoChar = tok.peekString(currentPos, 2);
            if (twoChar == "&&" || twoChar == "||") {
                op = twoChar;
                tok.setPos(currentPos + 2);
            }
        }
        
        if (!op.empty()) {
            auto right = parseSimpleComparison(tok);
            if (!right) return nullptr;
            left = std::make_shared<BinaryOpNode>(op, left, right);
        } else {
            break;
        }
    }
    
    return left;
}

// 处理字符串中的转义序列
std::string processEscapeSequences(const std::string& str) {
    std::string processed;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n':  processed += '\n'; i++; break;  // 换行
                case 't':  processed += '\t'; i++; break;  // 制表符
                case 'r':  processed += '\r'; i++; break;  // 回车
                case '\\': processed += '\\'; i++; break;  // 反斜杠
                case '"':  processed += '"';  i++; break;  // 双引号
                case '\'': processed += '\''; i++; break;  // 单引号
                case '0':  i++; break;                     // 空字符，跳过以避免截断
                case 'b':  processed += '\b'; i++; break;  // 退格
                case 'f':  processed += '\f'; i++; break;  // 换页
                case 'v':  processed += '\v'; i++; break;  // 垂直制表符
                default:   processed += str[i]; break;     // 未知转义序列保留原样
            }
        } else {
            processed += str[i];
        }
    }
    return processed;
}

// 解析插值字符串，提取表达式和格式说明符
InterpolatedStringNode* parseInterpolatedString(const std::string& raw) {
    auto node = std::make_unique<InterpolatedStringNode>();
    
    // 移除首尾引号
    std::string str = raw;
    if (str.length() >= 2 && str[0] == '"' && str[str.length()-1] == '"') {
        str = str.substr(1, str.length() - 2);
    }
    
    size_t pos = 0;
    std::string currentLiteral;
    
    while (pos < str.length()) {
        if (pos + 1 < str.length() && str[pos] == '$' && str[pos+1] == '{') {
            // 找到插值开始，先处理之前的字面量部分
            node->addStringPart(processEscapeSequences(currentLiteral));
            currentLiteral.clear();
            
            // 解析 ${...} 中的表达式
            size_t start = pos;
            size_t end = str.find('}', start);
            if (end != std::string::npos) {
                std::string exprStr = str.substr(start + 2, end - start - 2);
                
                // 检查格式说明符 ${expr:format}
                std::string formatSpec;
                size_t colonPos = exprStr.find(':');
                if (colonPos != std::string::npos) {
                    formatSpec = exprStr.substr(colonPos + 1);
                    exprStr = exprStr.substr(0, colonPos);
                }
                
                SimpleTokenizer tok(exprStr);
                auto expr = parseSimpleLogical(tok);
                if (!expr) {
                    std::cerr << "Error: Failed to parse expression '" << exprStr << "'" << std::endl;
                    return nullptr;
                }
                
                // 添加表达式及其格式说明符
                if (formatSpec.empty()) {
                    node->addExpression(expr);
                } else {
                    node->addExpression(expr, formatSpec);
                }
                
                pos = end + 1;
            } else {
                std::cerr << "Error: Unclosed ${} in interpolated string" << std::endl;
                return nullptr;
            }
        } else {
            currentLiteral += str[pos];
            pos++;
        }
    }
    
    // 处理最后一段字面量
    node->addStringPart(processEscapeSequences(currentLiteral));
    return node.release();
}
%}

// 启用位置跟踪以获取正确的行号
%locations

// 启用详细错误信息
%define parse.error verbose

// 语义值类型定义
%union {
    int intVal;
    double doubleVal;
    char charVal;
    bool boolVal;
    std::string* strVal;
    
    ASTNode* node;
    ExprNode* expr;
    StmtNode* stmt;
    TypeNode* type;
    ParameterNode* param;
    BlockNode* block;
    ProgramNode* program;
    FunctionCallNode* funcCall;
    CaseNode* caseNode;
    std::vector<std::shared_ptr<ParameterNode>>* paramList;
    std::vector<std::shared_ptr<ExprNode>>* exprList;
    std::vector<int>* intList;
    std::vector<std::shared_ptr<CaseNode>>* caseList;
}

// Token 定义
%token <intVal> INT_LITERAL
%token <doubleVal> DOUBLE_LITERAL
%token <strVal> STRING_LITERAL
%token <strVal> INTERPOLATED_STRING
%token <charVal> CHAR_LITERAL
%token <boolVal> BOOL_LITERAL
%token <strVal> IDENTIFIER
%token <strVal> TYPE

// 关键字
%token LET CONST FUNC RETURN IF ELSE WHILE FOR IN IMPORT AS TRY CATCH THROW
%token BREAK CONTINUE SWITCH CASE DEFAULT

// 运算符
%token PLUS MINUS MULTIPLY DIVIDE FLOORDIV MODULO
%token EQ NE LT GT LE GE
%token AND OR NOT
%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN MULT_ASSIGN DIV_ASSIGN FLOORDIV_ASSIGN MOD_ASSIGN

// 分隔符
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token COMMA COLON SEMICOLON DOT DOTDOT

// 错误记号
%token ERROR

// 非终结符类型
%type <program> program
%type <stmt> statement declaration function_decl var_decl
%type <stmt> if_stmt while_stmt for_stmt return_stmt try_catch_stmt
%type <stmt> assignment expr_stmt import_stmt throw_stmt
%type <stmt> break_stmt continue_stmt switch_stmt
%type <caseNode> case_clause
%type <caseList> case_list
%type <block> block statement_list
%type <expr> expression primary_expr postfix_expr unary_expr
%type <expr> multiplicative_expr additive_expr relational_expr
%type <expr> equality_expr logical_and_expr logical_or_expr
%type <expr> assignment_expr
%type <funcCall> function_call
%type <exprList> argument_list argument_list_opt array_elements
%type <type> type_spec
%type <intList> array_dimensions
%type <param> parameter
%type <paramList> parameter_list_opt parameter_list

// 错误时自动清理动态分配的内存
%destructor { delete $$; } <strVal>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <type>
%destructor { delete $$; } <param>
%destructor { delete $$; } <block>
%destructor { delete $$; } <funcCall>
%destructor { delete $$; } <caseNode>
%destructor { delete $$; } <paramList>
%destructor { delete $$; } <exprList>
%destructor { delete $$; } <intList>
%destructor { delete $$; } <caseList>

// 运算符优先级和结合性（从低到高）
%right ASSIGN PLUS_ASSIGN MINUS_ASSIGN MULT_ASSIGN DIV_ASSIGN MOD_ASSIGN
%left OR
%left AND
%left EQ NE
%left LT GT LE GE
%left PLUS MINUS
%left MULTIPLY DIVIDE FLOORDIV MODULO
%right NOT UNARY_MINUS
%left DOT LBRACKET

%%

program:
    statement_list {
        root = std::make_shared<ProgramNode>();
        if ($1) {
            for (auto& stmt : $1->statements) {
                root->addStatement(stmt);
            }
        }
        $$ = root.get();
    }
    | /* 空程序 */ {
        root = std::make_shared<ProgramNode>();
        $$ = root.get();
    }
    ;

statement_list:
    statement {
        $$ = new BlockNode();
        if ($1) {
            $$->addStatement(std::shared_ptr<StmtNode>($1));
        }
    }
    | statement_list statement {
        $$ = $1;
        if ($2) {
            $$->addStatement(std::shared_ptr<StmtNode>($2));
        }
    }
    ;

statement:
    declaration
    | function_decl
    | if_stmt
    | while_stmt
    | for_stmt
    | return_stmt
    | break_stmt
    | continue_stmt
    | switch_stmt
    | try_catch_stmt
    | throw_stmt
    | assignment
    | expr_stmt
    | import_stmt
    | block { $$ = $1; }
    ;

block:
    LBRACE statement_list RBRACE { $$ = $2; }
    | LBRACE RBRACE { $$ = new BlockNode(); }
    ;

import_stmt:
    IMPORT IDENTIFIER {
        $$ = new ImportNode(*$2);
        delete $2;
    }
    | IMPORT IDENTIFIER AS IDENTIFIER {
        $$ = new ImportNode(*$2, *$4);
        delete $2;
        delete $4;
    }
    ;

declaration:
    var_decl
    ;

var_decl:
    LET IDENTIFIER COLON type_spec ASSIGN expression {
        if (g_verbose) {
            std::cout << "[AST] Parsing variable: " << *$2 << " : " << $4->typeName << std::endl;
        }
        $$ = new VarDeclNode(false, *$2, std::shared_ptr<TypeNode>($4), 
                             std::shared_ptr<ExprNode>($6));
        delete $2;
    }
    | CONST IDENTIFIER COLON type_spec ASSIGN expression {
        $$ = new VarDeclNode(true, *$2, std::shared_ptr<TypeNode>($4), 
                             std::shared_ptr<ExprNode>($6));
        delete $2;
    }
    ;

type_spec:
    TYPE {
        $$ = new TypeNode(*$1);
        delete $1;
    }
    | TYPE array_dimensions {
        $$ = new TypeNode(*$1, *$2);
        delete $1;
        delete $2;
    }
    ;

array_dimensions:
    LBRACKET INT_LITERAL RBRACKET {
        $$ = new std::vector<int>();
        $$->push_back($2);
    }
    | array_dimensions LBRACKET INT_LITERAL RBRACKET {
        $1->push_back($3);
        $$ = $1;
    }
    ;

function_decl:
    FUNC IDENTIFIER LPAREN parameter_list_opt RPAREN block {
        if (g_verbose) {
            std::cout << "[AST] Parsing function: " << *$2 << std::endl;
        }
        auto func = new FunctionDeclNode(*$2);
        func->body = std::shared_ptr<BlockNode>($6);
        if ($4) {
            func->parameters = *$4;
            delete $4;
        }
        $$ = func;
        delete $2;
    }
    | FUNC IDENTIFIER LPAREN parameter_list_opt RPAREN COLON type_spec block {
        if (g_verbose) {
            std::cout << "[AST] Parsing function: " << *$2 << " -> " << $7->typeName << std::endl;
        }
        auto func = new FunctionDeclNode(*$2);
        func->returnType = std::shared_ptr<TypeNode>($7);
        func->body = std::shared_ptr<BlockNode>($8);
        if ($4) {
            func->parameters = *$4;
            delete $4;
        }
        $$ = func;
        delete $2;
    }
    ;

parameter_list_opt:
    /* 空参数列表 */ { $$ = nullptr; }
    | parameter_list { $$ = $1; }
    ;

parameter_list:
    parameter {
        $$ = new std::vector<std::shared_ptr<ParameterNode>>();
        $$->push_back(std::shared_ptr<ParameterNode>($1));
    }
    | parameter_list COMMA parameter {
        $$ = $1;
        $$->push_back(std::shared_ptr<ParameterNode>($3));
    }
    ;

parameter:
    IDENTIFIER COLON type_spec {
        $$ = new ParameterNode(*$1, std::shared_ptr<TypeNode>($3));
        delete $1;
    }
    ;

if_stmt:
    IF expression block {
        if (g_verbose) {
            std::cout << "[AST] Parsing if statement" << std::endl;
        }
        $$ = new IfStmtNode(std::shared_ptr<ExprNode>($2), 
                            std::shared_ptr<StmtNode>($3));
    }
    | IF expression block ELSE block {
        $$ = new IfStmtNode(std::shared_ptr<ExprNode>($2), 
                            std::shared_ptr<StmtNode>($3),
                            std::shared_ptr<StmtNode>($5));
    }
    | IF expression block ELSE if_stmt {
        $$ = new IfStmtNode(std::shared_ptr<ExprNode>($2), 
                            std::shared_ptr<StmtNode>($3),
                            std::shared_ptr<StmtNode>($5));
    }
    ;

while_stmt:
    WHILE expression block {
        if (g_verbose) {
            std::cout << "[AST] Parsing while loop" << std::endl;
        }
        $$ = new WhileStmtNode(std::shared_ptr<ExprNode>($2), 
                               std::shared_ptr<StmtNode>($3));
    }
    ;

for_stmt:
    FOR IDENTIFIER IN expression DOTDOT expression block {
        if (g_verbose) {
            std::cout << "[AST] Parsing for loop: " << *$2 << " in range" << std::endl;
        }
        $$ = new ForStmtNode(*$2, std::shared_ptr<ExprNode>($4),
                             std::shared_ptr<ExprNode>($6),
                             std::shared_ptr<StmtNode>($7));
        delete $2;
    }
    ;

return_stmt:
    RETURN expression {
        $$ = new ReturnStmtNode(std::shared_ptr<ExprNode>($2));
    }
    | RETURN {
        $$ = new ReturnStmtNode();
    }
    ;

break_stmt:
    BREAK {
        $$ = new BreakStmtNode();
    }
    ;

continue_stmt:
    CONTINUE {
        $$ = new ContinueStmtNode();
    }
    ;

try_catch_stmt:
    TRY block CATCH LPAREN IDENTIFIER COLON type_spec RPAREN block {
        $$ = new TryCatchNode(std::shared_ptr<StmtNode>($2), *$5,
                              std::shared_ptr<TypeNode>($7),
                              std::shared_ptr<StmtNode>($9));
        delete $5;
    }
    ;

throw_stmt:
    THROW expression {
        $$ = new ThrowStmtNode(std::shared_ptr<ExprNode>($2));
    }
    ;

switch_stmt:
    SWITCH expression LBRACE case_list RBRACE {
        if (g_verbose) {
            std::cout << "[AST] Parsing switch statement" << std::endl;
        }
        auto switchNode = new SwitchStmtNode(std::shared_ptr<ExprNode>($2));
        if ($4) {
            for (auto& caseNode : *$4) {
                switchNode->addCase(caseNode);
            }
            delete $4;
        }
        $$ = switchNode;
    }
    ;

case_list:
    case_clause {
        $$ = new std::vector<std::shared_ptr<CaseNode>>();
        $$->push_back(std::shared_ptr<CaseNode>($1));
    }
    | case_list case_clause {
        $$ = $1;
        $$->push_back(std::shared_ptr<CaseNode>($2));
    }
    ;

case_clause:
    CASE expression COLON block {
        $$ = new CaseNode(std::shared_ptr<ExprNode>($2), std::shared_ptr<BlockNode>($4));
    }
    | DEFAULT COLON block {
        $$ = new CaseNode(nullptr, std::shared_ptr<BlockNode>($3));
    }
    ;

assignment:
    postfix_expr ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr PLUS_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "+=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr MINUS_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "-=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr MULT_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "*=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr DIV_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "/=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr FLOORDIV_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "//=", 
                                std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr MOD_ASSIGN expression {
        $$ = new AssignmentNode(std::shared_ptr<ExprNode>($1), "%=", 
                                std::shared_ptr<ExprNode>($3));
    }
    ;

expr_stmt:
    expression {
        $$ = new ExprStmtNode(std::shared_ptr<ExprNode>($1));
    }
    ;

expression:
    assignment_expr
    ;

assignment_expr:
    logical_or_expr
    ;

logical_or_expr:
    logical_and_expr
    | logical_or_expr OR logical_and_expr {
        $$ = new BinaryOpNode("||", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

logical_and_expr:
    equality_expr
    | logical_and_expr AND equality_expr {
        $$ = new BinaryOpNode("&&", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

equality_expr:
    relational_expr
    | equality_expr EQ relational_expr {
        $$ = new BinaryOpNode("==", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | equality_expr NE relational_expr {
        $$ = new BinaryOpNode("!=", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

relational_expr:
    additive_expr
    | relational_expr LT additive_expr {
        $$ = new BinaryOpNode("<", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | relational_expr GT additive_expr {
        $$ = new BinaryOpNode(">", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | relational_expr LE additive_expr {
        $$ = new BinaryOpNode("<=", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | relational_expr GE additive_expr {
        $$ = new BinaryOpNode(">=", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

additive_expr:
    multiplicative_expr
    | additive_expr PLUS multiplicative_expr {
        $$ = new BinaryOpNode("+", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | additive_expr MINUS multiplicative_expr {
        $$ = new BinaryOpNode("-", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

multiplicative_expr:
    unary_expr
    | multiplicative_expr MULTIPLY unary_expr {
        $$ = new BinaryOpNode("*", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | multiplicative_expr DIVIDE unary_expr {
        $$ = new BinaryOpNode("/", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | multiplicative_expr FLOORDIV unary_expr {
        $$ = new BinaryOpNode("//", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    | multiplicative_expr MODULO unary_expr {
        $$ = new BinaryOpNode("%", std::shared_ptr<ExprNode>($1), 
                              std::shared_ptr<ExprNode>($3));
    }
    ;

unary_expr:
    postfix_expr
    | NOT unary_expr {
        $$ = new UnaryOpNode("!", std::shared_ptr<ExprNode>($2));
    }
    | MINUS unary_expr %prec UNARY_MINUS {
        $$ = new UnaryOpNode("-", std::shared_ptr<ExprNode>($2));
    }
    ;

postfix_expr:
    primary_expr
    | postfix_expr LBRACKET expression RBRACKET {
        $$ = new ArrayAccessNode(std::shared_ptr<ExprNode>($1), 
                                 std::shared_ptr<ExprNode>($3));
    }
    | postfix_expr DOT IDENTIFIER {
        $$ = new MemberAccessNode(std::shared_ptr<ExprNode>($1), *$3);
        delete $3;
    }
    | function_call {
        $$ = $1;
    }
    ;

function_call:
    IDENTIFIER LPAREN argument_list_opt RPAREN {
        auto call = new FunctionCallNode(*$1);
        call->lineNumber = @1.first_line;  // 使用位置跟踪获取正确行号
        if ($3) {
            call->arguments = *$3;
            delete $3;
        }
        delete $1;
        $$ = call;
    }
    | postfix_expr DOT IDENTIFIER LPAREN argument_list_opt RPAREN {
        auto call = new FunctionCallNode(*$3);
        call->object = std::shared_ptr<ExprNode>($1);
        if ($5) {
            call->arguments = *$5;
            delete $5;
        }
        delete $3;
        $$ = call;
    }
    ;

argument_list_opt:
    /* 空参数 */ { $$ = nullptr; }
    | argument_list { $$ = $1; }
    ;

argument_list:
    expression {
        $$ = new std::vector<std::shared_ptr<ExprNode>>();
        $$->push_back(std::shared_ptr<ExprNode>($1));
    }
    | argument_list COMMA expression {
        $$->push_back(std::shared_ptr<ExprNode>($3));
        $$ = $1;
    }
    ;

primary_expr:
    INT_LITERAL             { $$ = new IntLiteralNode($1); }
    | DOUBLE_LITERAL        { $$ = new DoubleLiteralNode($1); }
    | STRING_LITERAL        { $$ = new StringLiteralNode(*$1); delete $1; }
    | INTERPOLATED_STRING   { $$ = parseInterpolatedString(*$1); delete $1; }
    | CHAR_LITERAL          { $$ = new CharLiteralNode($1); }
    | BOOL_LITERAL          { $$ = new BoolLiteralNode($1); }
    | IDENTIFIER            { 
        $$ = new IdentifierNode(*$1);
        $$->lineNumber = @1.first_line;  // 使用位置跟踪获取正确行号
        delete $1; 
    }
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    | LBRACKET array_elements RBRACKET {
        $$ = new ArrayLiteralNode(*$2);
        delete $2;
    }
    | LBRACKET RBRACKET {
        $$ = new ArrayLiteralNode(std::vector<std::shared_ptr<ExprNode>>());
    }
    ;

array_elements:
    expression {
        $$ = new std::vector<std::shared_ptr<ExprNode>>();
        $$->push_back(std::shared_ptr<ExprNode>($1));
    }
    | array_elements COMMA expression {
        $1->push_back(std::shared_ptr<ExprNode>($3));
        $$ = $1;
    }
    ;

%%



// 将 Bison 生成的错误信息转换为更友好的中文描述
std::string translateErrorMessage(const std::string& msg) {
    std::string result = msg;
    
    // Token 名称翻译映射
    struct TokenTranslation {
        const char* from;
        const char* to;
    };
    
    static const TokenTranslation translations[] = {
        // 关键字
        {"RETURN", "'return'"},
        {"LET", "'let'"},
        {"CONST", "'const'"},
        {"FUNC", "'func'"},
        {"IF", "'if'"},
        {"ELSE", "'else'"},
        {"WHILE", "'while'"},
        {"FOR", "'for'"},
        {"IN", "'in'"},
        {"BREAK", "'break'"},
        {"CONTINUE", "'continue'"},
        {"SWITCH", "'switch'"},
        {"CASE", "'case'"},
        {"DEFAULT", "'default'"},
        {"IMPORT", "'import'"},
        {"TRY", "'try'"},
        {"CATCH", "'catch'"},
        {"THROW", "'throw'"},
        
        // 分隔符
        {"RPAREN", "')' (右括号)"},
        {"LPAREN", "'(' (左括号)"},
        {"RBRACE", "'}' (右花括号)"},
        {"LBRACE", "'{' (左花括号)"},
        {"RBRACKET", "']' (右方括号)"},
        {"LBRACKET", "'[' (左方括号)"},
        {"COMMA", "','"},
        {"COLON", "':'"},
        {"SEMICOLON", "';'"},
        {"DOT", "'.'"},
        {"DOTDOT", "'..'"},
        
        // 运算符
        {"ASSIGN", "'='"},
        {"PLUS_ASSIGN", "'+='"},
        {"MINUS_ASSIGN", "'-='"},
        {"MULT_ASSIGN", "'*='"},
        {"DIV_ASSIGN", "'/='"},
        {"MOD_ASSIGN", "'%='"},
        {"PLUS", "'+'"},
        {"MINUS", "'-'"},
        {"MULTIPLY", "'*'"},
        {"DIVIDE", "'/'"},
        {"MODULO", "'%'"},
        {"EQ", "'=='"},
        {"NE", "'!='"},
        {"LT", "'<'"},
        {"GT", "'>'"},
        {"LE", "'<='"},
        {"GE", "'>='"},
        {"AND", "'&&'"},
        {"OR", "'||'"},
        {"NOT", "'!'"},
        
        // 字面量
        {"INT_LITERAL", "整数"},
        {"DOUBLE_LITERAL", "浮点数"},
        {"STRING_LITERAL", "字符串"},
        {"CHAR_LITERAL", "字符"},
        {"BOOL_LITERAL", "布尔值"},
        {"IDENTIFIER", "标识符"},
        {"TYPE", "类型"},
        
        // 错误描述
        {"syntax error", "语法错误"},
        {"unexpected", "遇到意外的"},
        {"expecting", "期望"},
        {"$end", "文件结尾"},
        {nullptr, nullptr}
    };
    
    for (const auto& t : translations) {
        if (t.from == nullptr) break;
        size_t pos = 0;
        while ((pos = result.find(t.from, pos)) != std::string::npos) {
            result.replace(pos, strlen(t.from), t.to);
            pos += strlen(t.to);
        }
    }
    
    return result;
}

// 辅助函数：去除字符串首尾空白
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

// 辅助函数：检查字符串是否以指定前缀开头
bool startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && str.substr(0, prefix.length()) == prefix;
}

// 辅助函数：检查字符串是否以指定后缀结尾
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.substr(str.length() - suffix.length()) == suffix;
}

// 分析上下文并生成修复建议
std::string generateHint(const std::string& errorMsg, int lineNum) {
    std::string currentLine = trim(getSourceLine(lineNum));
    std::string prevLine = (lineNum > 1) ? trim(getSourceLine(lineNum - 1)) : "";
    std::string lineNumStr = std::to_string(lineNum);
    std::string prevLineNumStr = std::to_string(lineNum - 1);
    
    // 检查当前行的错误模式
    if (!currentLine.empty()) {
        // if 语句缺少条件: if { 或 if 后直接跟 {
        if (currentLine == "if" || currentLine == "if {") {
            return "提示: 第 " + lineNumStr + " 行的 'if' 语句缺少条件表达式，格式: if 条件 { ... }";
        }
        
        // while 语句缺少条件
        if (currentLine == "while" || currentLine == "while {") {
            return "提示: 第 " + lineNumStr + " 行的 'while' 语句缺少条件表达式，格式: while 条件 { ... }";
        }
        
        // for 语句不完整
        if (startsWith(currentLine, "for ") || currentLine == "for") {
            if (currentLine.find(" in ") == std::string::npos) {
                return "提示: 第 " + lineNumStr + " 行的 'for' 语句格式不完整，格式: for 变量 in 起始..结束 { ... }";
            }
            if (currentLine.find("..") == std::string::npos) {
                return "提示: 第 " + lineNumStr + " 行的 'for' 语句缺少范围运算符 '..'，格式: for 变量 in 起始..结束 { ... }";
            }
        }
        
        // func 函数定义不完整
        if (startsWith(currentLine, "func ")) {
            if (currentLine.find("(") == std::string::npos) {
                return "提示: 第 " + lineNumStr + " 行的函数定义缺少参数列表 '()'";
            }
            if (currentLine.find("{") == std::string::npos && currentLine.find(")") != std::string::npos) {
                size_t parenPos = currentLine.find(")");
                std::string afterParen = trim(currentLine.substr(parenPos + 1));
                if (afterParen.empty()) {
                    return "提示: 第 " + lineNumStr + " 行的函数定义缺少函数体 '{}'";
                }
            }
        }
        
        // switch 缺少表达式
        if (currentLine == "switch" || currentLine == "switch {") {
            return "提示: 第 " + lineNumStr + " 行的 'switch' 语句缺少匹配表达式，格式: switch 表达式 { case ... }";
        }
        
        // case 缺少值
        if (currentLine == "case" || currentLine == "case:") {
            return "提示: 第 " + lineNumStr + " 行的 'case' 子句缺少匹配值，格式: case 值: { ... }";
        }
    }
    
    // 检查前一行的错误模式
    if (!prevLine.empty()) {
        // 变量声明缺少初始值: let x: type = 
        if (startsWith(prevLine, "let ") || startsWith(prevLine, "const ")) {
            if (endsWith(prevLine, "=") || endsWith(prevLine, "= ")) {
                return "提示: 第 " + prevLineNumStr + " 行的变量声明缺少初始值";
            }
            // 变量声明缺少类型: let x = 
            if (prevLine.find(":") == std::string::npos && prevLine.find("=") != std::string::npos) {
                return "提示: 第 " + prevLineNumStr + " 行的变量声明可能缺少类型注解，格式: let 变量: 类型 = 值";
            }
        }
        
        // 函数调用缺少参数或括号: func(
        if (endsWith(prevLine, "(") || endsWith(prevLine, "( ")) {
            return "提示: 第 " + prevLineNumStr + " 行可能缺少参数或 ')'";
        }
        
        // 不完整的表达式: + - * / 结尾
        char lastChar = prevLine.back();
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/' || lastChar == '%') {
            return "提示: 第 " + prevLineNumStr + " 行的表达式不完整";
        }
        
        // 比较运算符不完整: == != < > <= >= 结尾
        if (endsWith(prevLine, "==") || endsWith(prevLine, "!=") || 
            endsWith(prevLine, "<=") || endsWith(prevLine, ">=") ||
            endsWith(prevLine, "&&") || endsWith(prevLine, "||")) {
            return "提示: 第 " + prevLineNumStr + " 行的表达式不完整，缺少右侧操作数";
        }
        
        // 赋值运算符不完整
        if (endsWith(prevLine, "+=") || endsWith(prevLine, "-=") || 
            endsWith(prevLine, "*=") || endsWith(prevLine, "/=") || endsWith(prevLine, "%=")) {
            return "提示: 第 " + prevLineNumStr + " 行的复合赋值表达式不完整";
        }
        
        // if/while/for 语句缺少花括号
        if (startsWith(prevLine, "if ") && prevLine.find("{") == std::string::npos) {
            return "提示: 第 " + prevLineNumStr + " 行的 'if' 语句缺少 '{'";
        }
        if (startsWith(prevLine, "while ") && prevLine.find("{") == std::string::npos) {
            return "提示: 第 " + prevLineNumStr + " 行的 'while' 语句缺少 '{'";
        }
        if (startsWith(prevLine, "for ") && prevLine.find("{") == std::string::npos) {
            return "提示: 第 " + prevLineNumStr + " 行的 'for' 语句缺少 '{'";
        }
        
        // 数组访问不完整: arr[
        if (endsWith(prevLine, "[")) {
            return "提示: 第 " + prevLineNumStr + " 行的数组访问不完整，缺少索引和 ']'";
        }
        
        // 冒号后缺少类型
        if (endsWith(prevLine, ":") || endsWith(prevLine, ": ")) {
            return "提示: 第 " + prevLineNumStr + " 行可能缺少类型声明";
        }
    }
    
    // 根据错误信息提供通用建议
    if (errorMsg.find("期望") != std::string::npos) {
        if (errorMsg.find("')'") != std::string::npos) {
            return "提示: 检查是否有未闭合的括号 '('";
        }
        if (errorMsg.find("'}'") != std::string::npos) {
            return "提示: 检查是否有未闭合的花括号 '{'";
        }
        if (errorMsg.find("']'") != std::string::npos) {
            return "提示: 检查是否有未闭合的方括号 '['";
        }
    }
    
    // 文件结尾错误
    if (errorMsg.find("文件结尾") != std::string::npos) {
        return "提示: 文件可能缺少闭合的括号或花括号";
    }
    
    return "";
}

void yyerror(const char* s) {
    // 翻译错误信息为更友好的格式
    std::string friendlyMsg = translateErrorMessage(s);
    
    // 输出文件位置和错误类型
    std::cerr << ERR_BOLD;
    if (!g_currentFile.empty()) {
        std::cerr << g_currentFile << ":";
    }
    std::cerr << yylineno << ": " << ERR_RED << "error: " << ERR_RESET;
    std::cerr << ERR_BOLD << friendlyMsg << ERR_RESET << std::endl;
    
    std::cerr << std::endl;
    
    // 显示上下文：前2行 + 当前行
    int startLine = std::max(1, yylineno - 2);
    for (int i = startLine; i <= yylineno; i++) {
        std::string line = getSourceLine(i);
        if (i == yylineno) {
            // 高亮当前错误行
            std::cerr << ERR_RED << " >> " << ERR_RESET;
            std::cerr << ERR_CYAN << std::setw(4) << i << " | " << ERR_RESET;
            std::cerr << ERR_BOLD << line << ERR_RESET << std::endl;
            
            // 显示错误位置指示器
            std::cerr << "         | " << ERR_RED << "^~~~" << ERR_RESET << std::endl;
        } else {
            // 上下文行
            std::cerr << "    ";
            std::cerr << ERR_CYAN << std::setw(4) << i << " | " << ERR_RESET;
            std::cerr << line << std::endl;
        }
    }
    
    // 生成并显示修复建议
    std::string hint = generateHint(friendlyMsg, yylineno);
    if (!hint.empty()) {
        std::cerr << ERR_CYAN << hint << ERR_RESET << std::endl;
    }
    
    std::cerr << std::endl;
}
