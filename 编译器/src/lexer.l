%option noyywrap
%{
    #define YY_NO_UNPUT
    #define YY_NO_INPUT

    #include "parser.h"
    #include <ostream>
    #include <fstream>
    using namespace std;

    extern FILE *yyin; 
    extern FILE *yyout;
    extern bool dump_tokens;

    void DEBUG_FOR_LAB4(std::string s){
        std::string DEBUG_INFO = "[DEBUG LAB4]: \t" + s + "\n";
        fputs(DEBUG_INFO.c_str(), yyout);
    }
%}

DECIMIAL ([1-9][0-9]*|0)
OCTONARY (0[1-7][0-7]*)
HEXADECIMIAL (0[xX][A-Fa-f1-9][A-Fa-f0-9]*)
ID [[:alpha:]_][[:alpha:][:digit:]_]*
EOL (\r\n|\n|\r)
WHITE [\t ]
blockcommentbegin "/*"
blockcommentelement .|\n
blockcommentend "*/"
commentbegin "//"
commentelement .
%x BLOCKCOMMENT
%x COMMENT

%%

"int" {
    /*
    * Questions: 
    *   Q1: Why we need to return INT in further labs?
    *   Q2: What is "INT" actually?
    */
    if(dump_tokens)
        DEBUG_FOR_LAB4("INT\tint");
    return INT;
}
"float" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("FLOAT\tfloat");
    #else
        return FLOAT;
    #endif
}
"double" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("DOUBLE\tdouble");
    #else
        return DOUBLE;
    #endif
}
"const" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("CONST\tconst");
    return CONST;

}
"void" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("VOID\tvoid");
    return VOID;
}
"if" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("IF\tif");
    return IF;
};
"else" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("ELSE\telse");
    return ELSE;
};
"return" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RETURN\treturn");
    return RETURN;
}
"while" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("WHILE\twhile");
    #else
        return WHILE;
    #endif
}

"for" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("FOR\tfor");
    #else
        return FOR;
    #endif
}

"break" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("BREAK\tbreak");
    #else
        return BREAK;
    #endif
}

"continue" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("CONTINUE\tcontinue");
    #else
        return CONTINUE;
    #endif
}

"==" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("EQUAL\t==");
    #else
        return EQUAL;
    #endif
}

"=" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ASSIGN\t=");
    #else
        return ASSIGN;
    #endif
}
"!=" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("EXCLAIMEQUAL\t!=");
    #else
        return EXCLAIMEQUAL;
    #endif
}
"!" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("EXCLAIM\t!");
    #else
        return EXCLAIM;
    #endif
}
"<=" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("LESSEQUAL\t<=");
    #else
        return LESSEQUAL;
    #endif
}
"<" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("LESS\t<");
    #else
        return LESS;
    #endif
}
">=" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("GREATEREQUAL\t>=");
    #else
        return GREATEREQUAL;
    #endif
}
">" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("GREATER\t>");
    #else
        return GREATER;
    #endif
}
"+" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ADD\t+");
    #else
        return ADD;
    #endif
}
"-" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("SUB\t-");
    #else
        return SUB;
    #endif
}
"*" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("MUL\t*");
    #else
        return MUL;
    #endif
}
"/" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("DIV\t/");
    #else
        return DIV;
    #endif
}
"%" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("MOD\t%");
    #else
        return MOD;
    #endif
}
"&&" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("AND\t&&");
    #else
        return AND;
    #endif
}
"||" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("OR\t||");
    #else
        return OR;
    #endif
}
";" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("SEMICOLON\t;");
    #else
        return SEMICOLON;
    #endif
}
"," {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("COMMA\t,");
    #else
        return COMMA;
    #endif
}

"(" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LPAREN\t(");
    return LPAREN;
}
")" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RPAREN\t)");
    return RPAREN;
}
"{" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("LBRACE\t{");
    return LBRACE;
}
"}" {
    if(dump_tokens)
        DEBUG_FOR_LAB4("RBRACE\t}");
    return RBRACE;
}
"[" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("LSQUARE\t[");
    #else
        return LSQUARE;
    #endif
}
"]" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("RSQUARE\t]");
    #else
        return RSQUARE;
    #endif
}
"getint" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ID", "getint");
        offset+=strlen(yytext);
    #else
        char *lexeme = new char[strlen(yytext) + 1];
        strcpy(lexeme, yytext);
        yylval.strtype = lexeme;
        if(identifiers->lookup(yytext)==nullptr){//符号表内未找到，插入
            Type* funcType = new FunctionType(TypeSystem::intType, {});//返回类型int，无参数
            SymbolTable* globalTable;    //全域符号表
            for(globalTable = identifiers;globalTable->getPrev();globalTable = globalTable->getPrev()); //全域符号表
            SymbolEntry* entry = new IdentifierSymbolEntry(funcType, yytext, 0);//作用域GLOBAL(0)
            globalTable->install(yytext, entry);
        }
        return ID;
    #endif
}
"getch" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ID", "getch");
        offset+=strlen(yytext);
    #else
        char *lexeme = new char[strlen(yytext) + 1];
        strcpy(lexeme, yytext);
        yylval.strtype = lexeme;
        if(identifiers->lookup(yytext)==nullptr){//符号表内未找到，插入
            Type* funcType = new FunctionType(TypeSystem::intType, {});//返回类型int，无参数
            SymbolTable* globalTable;    //全域符号表
            for(globalTable = identifiers;globalTable->getPrev();globalTable = globalTable->getPrev()); //全域符号表
            SymbolEntry* entry = new IdentifierSymbolEntry(funcType, yytext, 0);//作用域GLOBAL(0)
            globalTable->install(yytext, entry);
        }
        return ID;
    #endif
}
"putint" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ID", "putint");
        offset+=strlen(yytext);
    #else
        char *lexeme = new char[strlen(yytext) + 1];
        strcpy(lexeme, yytext);
        yylval.strtype = lexeme;
        if(identifiers->lookup(yytext)==nullptr){//符号表内未找到，插入
            std::vector<Type*> vec; //形参类型表
            vec.push_back(TypeSystem::intType);
            Type* funcType = new FunctionType(TypeSystem::voidType, vec);//返回类型void，参数表[int]
            SymbolTable* globalTable;    //全域符号表
            for(globalTable = identifiers;globalTable->getPrev();globalTable = globalTable->getPrev()); //全域符号表
            SymbolEntry* entry = new IdentifierSymbolEntry(funcType, yytext, 0);//作用域GLOBAL(0)
            globalTable->install(yytext, entry);
        }
        return ID;
    #endif
}
"putch" {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4("ID", "putch");
        offset+=strlen(yytext);
    #else
        char *lexeme = new char[strlen(yytext) + 1];
        strcpy(lexeme, yytext);
        yylval.strtype = lexeme;
        if(identifiers->lookup(yytext)==nullptr){//符号表内未找到，插入
            std::vector<Type*> vec; //形参类型表
            vec.push_back(TypeSystem::intType);
            Type* funcType = new FunctionType(TypeSystem::voidType, vec);//返回类型void，参数表[int]
            SymbolTable* globalTable;    //全域符号表
            for(globalTable = identifiers;globalTable->getPrev();globalTable = globalTable->getPrev()); //全域符号表
            SymbolEntry* entry = new IdentifierSymbolEntry(funcType, yytext, 0);//作用域GLOBAL(0)
            globalTable->install(yytext, entry);
        }
        return ID;
    #endif
}

{DECIMIAL} {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    yylval.itype = atoi(yytext);
    return INTEGER;
}

{OCTONARY} {

    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4(yytext);
    #else
    	char temp[50];
        int a;
        
	sscanf(yytext, "%o", &a);
        sprintf(temp,"%d",a);

	yylval.itype = atoi(temp);
        return INTEGER;
    #endif
}

{HEXADECIMIAL} {
    #ifdef ONLY_FOR_LEX
        DEBUG_FOR_LAB4(yytext);
    #else
    	char temp[50];
        int a;
        
	sscanf(yytext, "%x", &a);
        sprintf(temp,"%d",a);

        yylval.itype = atoi(temp);
        return INTEGER;
    #endif
}


{ID} {
    if(dump_tokens)
        DEBUG_FOR_LAB4(yytext);
    char *lexeme;
    lexeme = new char[strlen(yytext) + 1];
    strcpy(lexeme, yytext);
    yylval.strtype = lexeme;
    return ID;
}

{blockcommentbegin} {BEGIN BLOCKCOMMENT;}
<BLOCKCOMMENT>{blockcommentelement} {}
<BLOCKCOMMENT>{blockcommentend} {BEGIN INITIAL;}

{commentbegin} {BEGIN COMMENT;}
<COMMENT>{commentelement} {}
<COMMENT>{EOL} {BEGIN INITIAL;}

{EOL} yylineno++;
{WHITE}
%%