/*
 * 词法分析器
 * 文件: lexical.l
 * 功能: 将源代码文本转换为 Token 流，供语法分析器使用
 * 工具: Flex 
 * 
 * 主要功能:
 * 1. 识别关键字 (let, func, if, while, etc.)
 * 2. 识别运算符 (+, -, *, /, ==, !=, etc.)
 * 3. 识别字面量 (整数, 浮点数, 字符串, 字符, 布尔值)
 * 4. 识别标识符 (变量名, 函数名)
 * 5. 处理注释 (单行 # 和多行 /#/ ... /#/)
 * 6. 处理转义字符 (\n, \t, \\, etc.)
 * 7. 跟踪行号用于错误报告
 */

%{
#include <string>
#include <iostream>
#include "node.h"
#include "syntax.hh"

extern int yylineno;  /* 当前行号，由 Flex 自动维护 */

// 列号跟踪
static int yycolumn = 1;

// 定义 YY_USER_ACTION 宏，在每个 token 动作前自动更新位置信息（包括列号）
#define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

/* Flex 选项配置 */
%option noyywrap
%option yylineno

/* 
 * 正则表达式定义
 * 用于简化后续的词法规则
 */
DIGIT       [0-9]
LETTER      [a-zA-Z_]
IDENTIFIER  {LETTER}({LETTER}|{DIGIT})*
INTEGER     {DIGIT}+
DOUBLE      {DIGIT}+\.{DIGIT}+
STRING      \"([^\"\\]|\\.)*\"
INTERPOL_STRING  \"([^\"\\]|\\.)*\$\{([^\"\\]|\\.)*\"
CHAR        '([^'\\]|\\.)'
WHITESPACE  [ \t\r]+
NEWLINE     \n

%%

  /* 词法规则 */
  /* 注释处理 */
"#".*                   { /* 单行注释 - 忽略到行尾 */ }
"/#/"(.|\n)*?"/#/"      { /* 多行注释 - 忽略所有内容（包括换行） */ }

  /* 关键字 (Keywords) - 必须在标识符规则之前 */
  /* 变量和常量声明 */
"let"                   { return LET; }
"const"                 { return CONST; }

  /* 函数相关 */
"func"                  { return FUNC; }
"return"                { return RETURN; }

  /* 控制流 */
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"for"                   { return FOR; }
"in"                    { return IN; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"switch"                { return SWITCH; }
"case"                  { return CASE; }
"default"               { return DEFAULT; }

  /* 模块系统 */
"import"                { return IMPORT; }
"as"                    { return AS; }

  /* 异常处理 */
"try"                   { return TRY; }
"catch"                 { return CATCH; }
"throw"                 { return THROW; }

  /* 布尔字面量 */
"true"                  { yylval.boolVal = true; return BOOL_LITERAL; }
"false"                 { yylval.boolVal = false; return BOOL_LITERAL; }

  /* 类型关键字 */
"int"                   { yylval.strVal = new std::string(yytext); return TYPE; }
"double"                { yylval.strVal = new std::string(yytext); return TYPE; }
"string"                { yylval.strVal = new std::string(yytext); return TYPE; }
"bool"                  { yylval.strVal = new std::string(yytext); return TYPE; }
"char"                  { yylval.strVal = new std::string(yytext); return TYPE; }

  /* 运算符 */
  /* 算术运算符 */
"+"                     { return PLUS; }              /* 加法 */
"-"                     { return MINUS; }             /* 减法 */
"*"                     { return MULTIPLY; }          /* 乘法 */
"//="                   { return FLOORDIV_ASSIGN; }   /* 整除赋值 (必须在 // 之前) */
"//"                    { return FLOORDIV; }          /* 整除 */
"/="                    { return DIV_ASSIGN; }        /* 除法赋值 (必须在 / 之前) */
"/"                     { return DIVIDE; }            /* 除法 */
"%"                     { return MODULO; }            /* 取模 */

  /* 比较运算符 */
"=="                    { return EQ; }                /* 等于 */
"!="                    { return NE; }                /* 不等于 */
"<>"                    { return NE; }                /* 不等于 (替代语法) */
"<="                    { return LE; }                /* 小于等于 (必须在 < 之前) */
">="                    { return GE; }                /* 大于等于 (必须在 > 之前) */
"<"                     { return LT; }                /* 小于 */
">"                     { return GT; }                /* 大于 */

  /* 逻辑运算符 */
"&&"                    { return AND; }               /* 逻辑与 */
"||"                    { return OR; }                /* 逻辑或 */
"!"                     { return NOT; }               /* 逻辑非 */

  /* 赋值运算符 */
"+="                    { return PLUS_ASSIGN; }       /* 加法赋值 */
"-="                    { return MINUS_ASSIGN; }      /* 减法赋值 */
"*="                    { return MULT_ASSIGN; }       /* 乘法赋值 */
"%="                    { return MOD_ASSIGN; }        /* 取模赋值 */
"="                     { return ASSIGN; }            /* 赋值 (必须在复合赋值之后) */

  /* 分隔符 */
"("                     { return LPAREN; }            /* 左圆括号 */
")"                     { return RPAREN; }            /* 右圆括号 */
"{"                     { return LBRACE; }            /* 左花括号 */
"}"                     { return RBRACE; }            /* 右花括号 */
"["                     { return LBRACKET; }          /* 左方括号 */
"]"                     { return RBRACKET; }          /* 右方括号 */
","                     { return COMMA; }             /* 逗号 */
":"                     { return COLON; }             /* 冒号 */
";"                     { return SEMICOLON; }         /* 分号 */
".."                    { return DOTDOT; }            /* 范围运算符 (必须在 . 之前) */
"."                     { return DOT; }               /* 点号 (成员访问) */

  /* 字面量 (Literals) */
  /* 浮点数字面量 */
{DOUBLE}                { 
                          yylval.doubleVal = atof(yytext); 
                          return DOUBLE_LITERAL; 
                        }

  /* 整数字面量 */
{INTEGER}               { 
                          yylval.intVal = atoi(yytext); 
                          return INT_LITERAL; 
                        }

  /* 插值字符串字面量 */
{INTERPOL_STRING}       {
                          std::string str(yytext);
                          /* 保留原始字符串（包含引号），稍后在语法分析器中解析 */
                          yylval.strVal = new std::string(str);
                          return INTERPOLATED_STRING;
                        }

  /* 普通字符串字面量 */
{STRING}                { 
                          std::string str(yytext);
                          /* 移除首尾引号 */
                          str = str.substr(1, str.length() - 2);
                          
                          /* 处理转义字符 */
                          std::string processed;
                          for (size_t i = 0; i < str.length(); i++) {
                              if (str[i] == '\\' && i + 1 < str.length()) {
                                  switch (str[i + 1]) {
                                      case 'n':  processed += '\n'; i++; break;  /* 换行 */
                                      case 't':  processed += '\t'; i++; break;  /* 制表符 */
                                      case 'r':  processed += '\r'; i++; break;  /* 回车 */
                                      case '\\': processed += '\\'; i++; break;  /* 反斜杠 */
                                      case '"':  processed += '"';  i++; break;  /* 双引号 */
                                      case '\'': processed += '\''; i++; break;  /* 单引号 */
                                      case '0':  i++; break;  /* 空字符 - 跳过以避免字符串截断 */
                                      case 'b':  processed += '\b'; i++; break;  /* 退格 */
                                      case 'f':  processed += '\f'; i++; break;  /* 换页 */
                                      case 'v':  processed += '\v'; i++; break;  /* 垂直制表符 */
                                      default:   processed += str[i]; break;     /* 未知转义，保留原样 */
                                  }
                              } else {
                                  processed += str[i];
                              }
                          }
                          
                          yylval.strVal = new std::string(processed);
                          return STRING_LITERAL; 
                        }

  /* 字符字面量 */
{CHAR}                  { 
                          /* 处理字符转义 */
                          if (yytext[1] == '\\' && yyleng >= 3) {
                              /* 转义字符: '\\x' */
                              switch (yytext[2]) {
                                  case 'n':  yylval.charVal = '\n'; break;  /* 换行 */
                                  case 't':  yylval.charVal = '\t'; break;  /* 制表符 */
                                  case 'r':  yylval.charVal = '\r'; break;  /* 回车 */
                                  case '\\': yylval.charVal = '\\'; break;  /* 反斜杠 */
                                  case '\'': yylval.charVal = '\''; break;  /* 单引号 */
                                  case '"':  yylval.charVal = '"';  break;  /* 双引号 */
                                  case '0':  yylval.charVal = '\0'; break;  /* 空字符 */
                                  case 'b':  yylval.charVal = '\b'; break;  /* 退格 */
                                  case 'f':  yylval.charVal = '\f'; break;  /* 换页 */
                                  case 'v':  yylval.charVal = '\v'; break;  /* 垂直制表符 */
                                  default:   
                                      /* 未知转义序列，保留字面字符 */
                                      yylval.charVal = yytext[2]; 
                                      break;
                              }
                          } else {
                              /* 普通字符: 'x' */
                              yylval.charVal = yytext[1];
                          }
                          return CHAR_LITERAL; 
                        }

  /* 
  * 标识符 (Identifiers)
  * 变量名、函数名等
  * 必须在关键字之后，避免关键字被识别为标识符
  */
{IDENTIFIER}            { 
                          yylval.strVal = new std::string(yytext); 
                          return IDENTIFIER; 
                        }

  /* 
  * 空白字符处理
  */
{WHITESPACE}            { /* 忽略空格、制表符、回车 */ }
{NEWLINE}               { yycolumn = 1; /* 换行时重置列号 */ }

  /* 
  * 错误处理
  * 匹配任何未被识别的字符
  */
.                       { 
                          std::cerr << "Lexical error: Unknown character '" 
                                    << yytext << "' at line " << yylineno << std::endl; 
                          return ERROR;
                        }

%%
