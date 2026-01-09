/*
    DAY 3
    https://www.youtube.com/watch?v=BKYOa1xdYD0

    Implementing very basic pratt parsing. An alternative to recursive descent.
    You need to enable the block of code that runs prattParsing in main()
    You ultimately still end up with an abstract syntax tree.
    This method is potentially faster if you skip the tree generation -
    and instead emit bytecode directly (if you're using a VM architecture)
    For now we want to work with the AST since it will be easier to debug -
    when emitting our bytecode for our register based VM

    This roughly touches on chapter 17 from the amazing book 
        'Crafting Interpreters' by Robert Nystrom: 
        https://craftinginterpreters.com/compiling-expressions.html
*/



#include <stdio.h>
#include <stdlib.h>

#define u32 unsigned long int
#define s32 long int

#define Assert(Expression) if(!Expression){*(int*)0 = 0;}

#define DEBUG 1

#if DEBUG
    #define PDEBUG(...) printf(__VA_ARGS__);
#else
    #define PDEBUG(...)
#endif


typedef enum{
    tok_none = 0,
    tok_int,
    tok_num,
    tok_char,
    tok_bool,
    tok_var,
    tok_plus,
    tok_minus,
    tok_slash,
    tok_star,
    tok_equal,
    tok_semicolon,
    tok_lparen,
    tok_rparen,
    tok_lcurly,
    tok_rcurly,
    tok_true,
    tok_false,
    tok_bang,
    tok_pplus,
    tok_mminus,
    tok_eequal,
    tok_nequal,
    tok_gr,
    tok_less,
    tok_grequal,
    tok_lequal,
    tok_colon,
    tok_eof,
}token_type;

typedef struct token{
    token_type type;
    union{
        int integer;
        int varOffset;
    }data;
}token;

typedef struct expr expr;

typedef enum{
    expr_none,
    expr_primary,
    expr_unary,
    expr_binary,
    expr_factor,
    expr_term,
    expr_comparison,
    expr_equality,
    expr_grouping,
}expr_type;

typedef struct primary_expr{
    token t;
    int varOffset;
    union{
        int integer;
    }data;
}primary_expr;

typedef struct unary_expr{
    expr* right;
    token operator;
}unary_expr;


typedef struct binary_expr{
    expr* left;
    expr* right;
    token operator;
}binary_expr;

typedef struct grouping_expr{
    expr* e;
}grouping_expr;


typedef struct expr{
    expr_type type;
    union{
        primary_expr primary;
        unary_expr unary;
        binary_expr binary;
        grouping_expr grouping;
    }data;
}expr;


typedef struct hashEntry{
    u32 hash;
    char* str;
}hashEntry;



typedef enum{
    obj_none,
    obj_num,
    obj_string,
    obj_bool,
    obj_char,
    obj_function,
}
object_type;

typedef struct object{
    object_type type;
    
    union{
        int integer;
    }data;

}object;




#define HASH_BUCKETS 4
#define HASH_TABLE_SIZE 2048

char input[2048];
token tokens[2048];
expr exprs[2048];
char varStorage[2048];
hashEntry hashTable[HASH_TABLE_SIZE];
expr* program[2048];

object objects[2048];
int objectCount = 0;

char astStr[2048];
int astStrCount;

int programCount;

int varStorageOffset = 0;
int tokenCount = 0;
int exprCount = 0;

int cursor = 0;

int parsingError = 0;
int processingError = 0;
int runtimeError = 0;

int nextToken = 0;





const char* tokStr(token_type t){
    switch(t){
        case tok_none       :{return "tok_none";}break;
        case tok_int        :{return "tok_int";}break;
        case tok_num        :{return "tok_num";}break;
        case tok_char       :{return "tok_char";}break;
        case tok_bool       :{return "tok_bool";}break;
        case tok_var        :{return "tok_var";}break;
        case tok_plus       :{return "tok_plus";}break;
        case tok_minus      :{return "tok_minus";}break;
        case tok_slash      :{return "tok_slash";}break;
        case tok_star       :{return "tok_star";}break;
        case tok_equal      :{return "tok_equal";}break;
        case tok_semicolon  :{return "tok_semicolon";}break;
        case tok_lparen     :{return "tok_lparen";}break;
        case tok_rparen     :{return "tok_rparen";}break;
        case tok_lcurly     :{return "tok_lcurly";}break;
        case tok_rcurly     :{return "tok_rcurly";}break;
        case tok_true       :{return "tok_true";}break;
        case tok_false      :{return "tok_false";}break;
        case tok_bang       :{return "tok_bang";}break;
        case tok_pplus      :{return "tok_pplus";}break;
        case tok_mminus     :{return "tok_mminus";}break;
        case tok_eequal     :{return "tok_eequal";}break;
        case tok_nequal     :{return "tok_nequal";}break;
        case tok_gr         :{return "tok_gr";}break;
        case tok_less       :{return "tok_less";}break;
        case tok_grequal    :{return "tok_grequal";}break;
        case tok_lequal     :{return "tok_lequal";}break;
        case tok_colon      :{return "tok_colon";}break;
        case tok_eof        :{return "tok_eof";}break;
        default:{}return "";
    }
    return "TEST";
}



u32 hashStr(char* str){
    //broken FNVo hasher
    u32 hash = 0b01010101010101010101000001110010;
    int strPos = 0;
    while((str[strPos]) != '\0'){
        hash ^= (*str);
        hash *= 16777619;
        ++strPos;
    }
    return hash;
}

void pushToHashTable(char* str){
    hashEntry entry = {};
    u32 hash = hashStr(str);
    //TODO: EXPLAIN AND OPERATION FOR MODULO
    //8 = 1000 | 8 - 1 = 7 = 0111
    // 21 = 10101
    //     & 0111
    //    = 00101 = 5       ==      21 % 8 = 5     ==      21 - 8 - 8 = 5
    //      16+0+4+0+1
    hash = hash & (HASH_TABLE_SIZE - 1);
    entry.hash = hash;
    entry.str = str;
    if((hashTable + hash)->str != NULL){
        printf("HASH ENTRY ERROR! TABLE SLOT IS ALREADY OCCUPIED!\n");
    }else{
        hashTable[hash] = entry;
        PDEBUG("PUSHED %s TO HASH TABLE AT ENTRY: %u\n", str, hash);
    }
}



token_type peekToken(int curToken){
    return tokens[curToken + 1].type;
}

/*
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term       ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor     ( ( "-" | "+" ) factor )* ;
factor         → unary      ( ( "/" | "*" ) unary )* ;
unary          → ( "!" | "-" ) unary | primary ;
primary        → NUMBER | STRING | "true" | "false" | "nil"
               | "(" expression ")" ;
*/

//expression
//equality
//comparison
//terms
//factors
//unary
//primary

expr* expression();

expr* primary(){
    expr* e = NULL;
    if(tokens[nextToken].type == tok_num){
        e = exprs + exprCount++;
        e->type = expr_primary;
        e->data.primary.t = tokens[nextToken++];
    }else if(tokens[nextToken].type == tok_lparen){
        e = exprs + exprCount++;
        e->type = expr_grouping;
        nextToken++;
        e->data.grouping.e = expression();
        if(tokens[nextToken].type == tok_rparen){
            nextToken++;
        }else{
            processingError++;
            printf("EXPECTED A CLOSING PARENTHESES FOLLOWING THE GROUPING\n");
        }
    }else{
        processingError++;
        printf("ERROR: UNHANDLED TOKEN TYPE: %s\n", tokStr(tokens[nextToken].type));
    }
    
    return e;
}

expr* unary(){
    expr* e = NULL;
    if(tokens[nextToken].type == tok_minus || tokens[nextToken].type == tok_bang){
        e = exprs + exprCount++;
        e->type = expr_unary;
        e->data.unary.operator = tokens[nextToken++];
        e->data.unary.right = primary();
    }else{
        e = primary();
    }
    
    return e;

}
expr* factors(){
    expr* e = unary();
    

    
    while(tokens[nextToken].type == tok_star || tokens[nextToken].type == tok_slash){
        expr* laste = e;
        e = exprs + exprCount++;
        e->type = expr_binary;
        e->data.binary.left = laste;
        e->data.binary.operator = tokens[nextToken++];
        e->data.binary.right = factors();
    }



    return e;

}
expr* terms(){//+ | -
    expr* e = factors();

    while(tokens[nextToken].type == tok_plus || tokens[nextToken].type == tok_minus){
        expr* laste = e;
        e = exprs + exprCount++;
        e->type = expr_binary;
        e->data.binary.left = laste;
        e->data.binary.operator = tokens[nextToken++];
        e->data.binary.right = factors();
    }

    return e;
}


expr* comparison(){
    expr* e = terms();

    return e;
}

expr* equality(){
    expr* e = comparison();

    return e;
}

expr* expression(){
    expr* e = equality();
    
    return e;
}


void printExpression(expr* e);

void printPrimary(expr* e){
    switch(e->type){
        case expr_primary:{
            switch(e->data.primary.t.type){
                case tok_num:{astStrCount += sprintf(astStr + astStrCount, "%d ", e->data.primary.t.data.integer);}break;
            }
            
        }break;
        default:{}break;

    }
}

void printToken(token t){
    switch(t.type){
        case tok_plus   :{astStrCount += sprintf(astStr + astStrCount, "+ ");}break;
        case tok_minus  :{astStrCount += sprintf(astStr + astStrCount, "- ");}break;
        case tok_slash  :{astStrCount += sprintf(astStr + astStrCount, "/ ");}break;
        case tok_star   :{astStrCount += sprintf(astStr + astStrCount, "* ");}break;
        default:{}break;
    }
}

void printBinary(expr* e){
    printExpression(e->data.binary.left);
    printToken(e->data.binary.operator);
    printExpression(e->data.binary.right);
}


void printUnary(expr* e){
    printToken(e->data.unary.operator);
    printExpression(e->data.unary.right);
}

void printGrouping(expr* e){
    astStrCount += sprintf(astStr + astStrCount, "( ");
    printExpression(e->data.grouping.e);
    astStrCount += sprintf(astStr + astStrCount, ") ");
}


void printExpression(expr* e){
    switch(e->type){
        case expr_primary:{
            printPrimary(e);
        }break;
        case expr_binary:{
            printBinary(e);
        }break;
        case expr_grouping:{
            printGrouping(e);
        }break;
        case expr_unary:{
            printUnary(e);
        }break;
        default:{}break;

    }
}

object* addObjects(object* o1, object* o2){
    object* o = objects + objectCount++;
    o->type = obj_num;
    o->data.integer = o1->data.integer + o2->data.integer;

    return o;
}

object* subtractObjects(object* o1, object* o2){
    object* o = objects + objectCount++;
    o->type = obj_num;
    o->data.integer = o1->data.integer - o2->data.integer;

    return o;
}


object* multiplyObjects(object* o1, object* o2){
    object* o = objects + objectCount++;
    o->type = obj_num;
    o->data.integer = o1->data.integer * o2->data.integer;

    return o;
}

object* divideObjects(object* o1, object* o2){
    object* o = objects + objectCount++;
    o->type = obj_num;
    if(o2->data.integer == 0){
        printf("ERROR DIVIDING BY 0!\n");
        runtimeError++;
        o->data.integer = 0;
        return o;
    }
    o->data.integer = o1->data.integer / o2->data.integer;

    return o;
}



object* evaluateMath(token t, object* o1, object* o2){
    object* o = NULL;
    switch(t.type){
        case tok_plus:{
            if(o1->type == obj_num && o2->type == obj_num){
                o = addObjects(o1, o2);
                PDEBUG("ADDING %d + %d = %d\n", o1->data.integer, o2->data.integer, o->data.integer);
            }
            else{
                runtimeError++;
                printf("EXPECTED 2 NUMBERS IN + TERM!\n");
            }
        }break;
        case tok_minus:{
            if(o1->type == obj_num && o2->type == obj_num){
                o = subtractObjects(o1, o2);
                PDEBUG("SUBTRACTING %d - %d = %d\n", o1->data.integer, o2->data.integer, o->data.integer);
            }
            else{
                runtimeError++;
                printf("EXPECTED 2 NUMBERS IN - TERM!\n");
            }

        }break;
        case tok_slash:{
            if(o1->type == obj_num && o2->type == obj_num){
                o = divideObjects(o1, o2);
                PDEBUG("DIVIDING %d / %d = %d\n", o1->data.integer, o2->data.integer, o->data.integer);
            }
            else{
                runtimeError++;
                printf("EXPECTED 2 NUMBERS IN / TERM!\n");
            }
        }break;
        case tok_star:{
            if(o1->type == obj_num && o2->type == obj_num){
                o = multiplyObjects(o1, o2);
                PDEBUG("MULTIPLYING %d * %d = %d\n", o1->data.integer, o2->data.integer, o->data.integer);
            }
            else{
                runtimeError++;
                printf("EXPECTED 2 NUMBERS IN * TERM!\n");
            }

        }break;
        default:{}break;
    }
    return o;
}

object* evaluateExpression(expr* e);

object* evaluateGrouping(expr* e){
    return evaluateExpression(e->data.grouping.e);
}

object* evaluateBinary(expr* e){
    object* o1 = evaluateExpression(e->data.binary.left);
    object* o2 = evaluateExpression(e->data.binary.right);
    object* o = NULL;

    switch(e->data.binary.operator.type){
        case tok_plus:
        case tok_minus:
        case tok_slash:
        case tok_star:{
            o = evaluateMath(e->data.binary.operator, o1, o2);
        }break;
        default:{}break;

    }
    return o;
}

object* evaluateUnary(expr* e){
    object* o = NULL;
    o = evaluateExpression(e->data.unary.right);
    switch(e->data.unary.operator.type){
        case tok_minus:{
            if(o->type == obj_num){
                PDEBUG("NEGATING NUMBER OBJECT: %d\n", o->data.integer);
                o->data.integer = -o->data.integer;
            }else{
                printf("ERROR IN evaluateUnary() | UNHANDLED UNARY CASE!\n");
                runtimeError++;
            }
        }break;
        case tok_bang:{
            if(o->type == obj_num){
                printf("ERROR CANNOT USE BANG (!) TOKEN ON NUMBER VALUE\n");
                runtimeError++;
            }
        }break;
    }

    return o;
}

object* evaluatePrimary(expr* e){
    object* o = NULL;
    switch(e->data.primary.t.type){
        case tok_num:{
            o = objects + objectCount++;
            o->type = obj_num;
            o->data.integer = e->data.primary.t.data.integer;
        }break;
        default:{}break;

    }

    return o;

}

object* evaluateExpression(expr* e){
    object* o = NULL;

    switch(e->type){
        case expr_grouping  :{
            PDEBUG("EVALUATING GROUPING\n");
            o = evaluateGrouping(e);
        }break;
        case expr_binary    :{
            PDEBUG("EVALUATING BINARY\n");
            o = evaluateBinary(e);
        }break;
        case expr_unary  :{
            PDEBUG("EVALUATING UNARY\n");
            o = evaluateUnary(e);
        }break;
        case expr_primary   :{
            PDEBUG("EVALUATING PRIMARY\n");
            o = evaluatePrimary(e);
        }break;
        default:{}break;

    }

    return o;
}


void printObject(object* o){
    switch(o->type){
        case obj_num:{
            printf("NUMBER OBJECT: %d\n", o->data.integer);
        }break;
        default:{}break;
    }
}



int keywordMatch(char* str, const char* compare, int size){
    for(int i = 0; i < size; ++i){
        char c = (*(str + i));
        if(c != compare[i]){
            return 0;
        }
    }
    return 1;
}

token_type isKeyword(char* str){
    char c = (*str);
    token_type type = tok_none;
    switch(c){//trie
        case 'i':{//int
            if(keywordMatch((str + 1), "nt", 2)){
                type = tok_int;
            }
        }
        case 'b':{//bool
            if(keywordMatch((str + 1), "ool", 3)){
                type = tok_bool;
            }
        }
        case 'c':{//char
            if(keywordMatch((str + 1), "har", 3)){
                type = tok_char;
            }
        }
        case 't':{//true
            if(keywordMatch((str + 1), "rue", 3)){
                type = tok_true;
            }
        }
        case 'f':{//fale
            if(keywordMatch((str + 1), "alse", 4)){
                type = tok_false;
            }
        }
    }
        

    return type;
}

int isAlpha(char c){
    if(c >= 'A' && c < 'Z')return 1;
    if(c >= 'a' && c < 'z')return 1;
    return 0;
};

int isNum(char c){
    if(c >= '0' && c <= '9')return 1;
    return 0;
}

int isWhitespace(char c){
    switch(c){
        case '\n'   :return 1;
        case '\t'   :return 1;
        case '\r'   :return 1;
        case ' '    :return 1;
        default     : return 0;
    }
    return 0;
}


void parser(char* input){
    int i = 0;
    char c = input[i];
    while(input[i] != '\0'){
        // printf("%c | isAlpha : %d, isNum : %d, isWhitespace %d\n", c, isAlpha(c), isNum(c), isWhitespace(c));
        
        if(isNum(c)){
            int val = 0;
            while(isNum(c)){
                val *= 10;
                val += c - '0';
                c = input[++i];
            }
            if(isAlpha(c)){
                parsingError++;
                printf("INVALID VARIABLE NAME CANNOT CONTAIN NUMBERS FIRST!\n");
                return;
            }

            printf("val : %d\n", val);
            token t = {};
            t.type = tok_num;
            t.data.integer = val;
            tokens[tokenCount++] = t;
        }
        else if(isAlpha(c)){
            token_type type = isKeyword(input + i);
            if(type){
                printf("KEYWORD FOUND | %s\n", tokStr(type));
                token t = {};
                switch(type){
                    case tok_int        :{i += 2; t.type = tok_int        ;}break;
                    case tok_char       :{i += 3; t.type = tok_char       ;}break;
                    case tok_bool       :{i += 3; t.type = tok_bool       ;}break;
                    case tok_true       :{i += 3; t.type = tok_true       ;}break;
                    case tok_false      :{i += 4; t.type = tok_false      ;}break;
                    default:{}break;
                }
                tokens[tokenCount++] = t;
            }else{

                int start = varStorageOffset;
                while(isAlpha(c) || isNum(c)){

                    varStorage[varStorageOffset++] = c;
                    c = input[++i];
                }
                varStorageOffset++;
                token t = {};
                t.type = tok_var;
                t.data.varOffset = start;
                printf("token var = %s\n", varStorage + t.data.varOffset);
                tokens[tokenCount++] = t;
                
            }
        }
        else if(!isWhitespace(c)){
            switch(c){
                case '+':{token t = {}; t.type = tok_plus;          tokens[tokenCount++] = t; i++;}break;
                case '-':{token t = {}; t.type = tok_minus;         tokens[tokenCount++] = t; i++;}break;
                case '/':{token t = {}; t.type = tok_slash;         tokens[tokenCount++] = t; i++;}break;
                case '*':{token t = {}; t.type = tok_star;          tokens[tokenCount++] = t; i++;}break;
                case ';':{token t = {}; t.type = tok_semicolon;     tokens[tokenCount++] = t; i++;}break;
                case '(':{token t = {}; t.type = tok_lparen;        tokens[tokenCount++] = t; i++;}break;
                case ')':{token t = {}; t.type = tok_rparen;        tokens[tokenCount++] = t; i++;}break;
                case '{':{token t = {}; t.type = tok_lcurly;        tokens[tokenCount++] = t; i++;}break;
                case '}':{token t = {}; t.type = tok_rcurly;        tokens[tokenCount++] = t; i++;}break;
                case '=':{token t = {}; t.type = tok_equal;         tokens[tokenCount++] = t; i++;}break;
                default:{
                    i++;
                    printf("ERROR: UNHANDLED CHAR: %c\n", c);
                    parsingError++;
                    return;
    
                }break;
            }

        }else if(isWhitespace(c))i++;
        else{
            i++;
            printf("ERROR: UNHANDLED CHAR: %c\n", c);
            parsingError++;
            return;
        }

        c = input[i];

    }
    token t = {};
    t.type = tok_eof;
    tokens[tokenCount++] = t;
}


void printTokens(){
    for(int i = 0; i < tokenCount; ++i){
        token t = tokens[i];
        switch(t.type){
            case tok_int        :{printf("tok_int          \n", i);
            }break;
            case tok_num        :{printf("tok_num        %3d | %d\n", i, t.data.integer);}break;

            case tok_none       :{printf("tok_none       %3d\n", i);}break;
            case tok_char       :{printf("tok_char       %3d\n", i);}break;
            case tok_bool       :{printf("tok_bool       %3d\n", i);}break;
            case tok_var        :{printf("tok_var        %3d | %s\n", i, varStorage + t.data.varOffset);}break;
            case tok_plus       :{printf("tok_plus       %3d\n", i);}break;
            case tok_minus      :{printf("tok_minus      %3d\n", i);}break;
            case tok_slash      :{printf("tok_slash      %3d\n", i);}break;
            case tok_star       :{printf("tok_star       %3d\n", i);}break;
            case tok_equal      :{printf("tok_equal      %3d\n", i);}break;
            case tok_semicolon  :{printf("tok_semicolon  %3d\n", i);}break;
            case tok_lparen     :{printf("tok_lparen     %3d\n", i);}break;
            case tok_rparen     :{printf("tok_rparen     %3d\n", i);}break;
            case tok_lcurly     :{printf("tok_lcurly     %3d\n", i);}break;
            case tok_rcurly     :{printf("tok_rcurly     %3d\n", i);}break;
            case tok_true       :{printf("tok_true       %3d\n", i);}break;
            case tok_false      :{printf("tok_false      %3d\n", i);}break;
            case tok_bang       :{printf("tok_bang       %3d\n", i);}break;
            case tok_pplus      :{printf("tok_pplus      %3d\n", i);}break;
            case tok_mminus     :{printf("tok_mminus     %3d\n", i);}break;
            case tok_eequal     :{printf("tok_eequal     %3d\n", i);}break;
            case tok_nequal     :{printf("tok_nequal     %3d\n", i);}break;
            case tok_gr         :{printf("tok_gr         %3d\n", i);}break;
            case tok_less       :{printf("tok_less       %3d\n", i);}break;
            case tok_grequal    :{printf("tok_grequal    %3d\n", i);}break;
            case tok_lequal     :{printf("tok_lequal     %3d\n", i);}break;
            case tok_colon      :{printf("tok_colon      %3d\n", i);}break;
            case tok_eof        :{printf("tok_eof        %3d\n", i);}break;
            default:{}break;
        }
    }
}



typedef enum{
    prec_none,
    prec_assign,//assignment
    prec_equality,
    prec_comp,
    prec_term,
    prec_factor,
    prec_unary,
    prec_call,//function calls
    prec_primary,
}prec_type;

typedef struct parse_rule{
    token_type tokType;
    expr* (*prefix)();
    expr* (*infix)(expr* left);
    prec_type prec;
}parse_rule;


expr* prattBinary(expr* left);
expr* prattUnary();
expr* prattPrimary();
expr* prattNumber();
expr* prattGrouping();

parse_rule rules[]={
    //TOK INDEX      PREFIX             INFIX           PRECEDENCE TYPE
    {tok_none,         NULL,            NULL,           prec_none},
    {tok_int,          NULL,            NULL,           prec_none},
    {tok_num,          prattNumber,     NULL,           prec_primary},
    {tok_char,         NULL,            NULL,           prec_none},
    {tok_bool,         NULL,            NULL,           prec_none},
    {tok_var,          NULL,            NULL,           prec_none},
    {tok_plus,         NULL,            prattBinary,    prec_term},
    {tok_minus,        prattUnary,      prattBinary,    prec_term},
    {tok_slash,        NULL,            prattBinary,    prec_factor},
    {tok_star,         NULL,            prattBinary,    prec_factor},
    {tok_equal,        NULL,            NULL,           prec_none},
    {tok_semicolon,    NULL,            NULL,           prec_none},
    {tok_lparen,       prattGrouping,   NULL,           prec_assign},
    {tok_rparen,       NULL,            NULL,           prec_none},
    {tok_lcurly,       NULL,            NULL,           prec_none},
    {tok_rcurly,       NULL,            NULL,           prec_none},
    {tok_true,         NULL,            NULL,           prec_none},
    {tok_false,        NULL,            NULL,           prec_none},
    {tok_bang,         NULL,            NULL,           prec_none},
    {tok_pplus,        NULL,            NULL,           prec_none},
    {tok_mminus,       NULL,            NULL,           prec_none},
    {tok_eequal,       NULL,            NULL,           prec_none},
    {tok_nequal,       NULL,            NULL,           prec_none},
    {tok_gr,           NULL,            NULL,           prec_none},
    {tok_less,         NULL,            NULL,           prec_none},
    {tok_grequal,      NULL,            NULL,           prec_none},
    {tok_lequal,       NULL,            NULL,           prec_none},
    {tok_colon,        NULL,            NULL,           prec_none},
    {tok_eof,          NULL,            NULL,           prec_none},
};

parse_rule* getRule(token_type type){
    return rules + type;
}


expr* prattParsePrecedence(prec_type prec){
    token t = tokens[nextToken];
    parse_rule* rule = getRule(t.type);
    if(!rule->prefix){
        printf("ERROR. EXPECTED EXPRESSION!\n");
        processingError++;
        return NULL;
    }
    expr* e = rule->prefix();

    while(prec <= getRule(tokens[nextToken].type)->prec){
        parse_rule* infix = getRule(tokens[nextToken].type);
        expr* left = e;
        e = infix->infix(left);
     }

    return e;
}
expr* prattExpression(){
    return prattParsePrecedence(prec_assign);
}


expr* prattGrouping(){
    token t = tokens[nextToken++];
    expr* e = exprs + exprCount++;
    e->type = expr_grouping;
    e->data.grouping.e = prattExpression();
    
    if(tokens[nextToken++].type != tok_rparen){
        processingError++;
        printf("EXPECTED CLOSING PARANTHESES!\n");
        return NULL;
    }
    
    return e;

}


expr* prattNumber(){
    token t = tokens[nextToken++];
    expr* e = exprs + exprCount++; 
    e->type = expr_primary;
    e->data.primary.t = t;
    return e;

}

expr* prattUnary(){
    expr* e = exprs + exprCount++;
    e->type = expr_unary;
    e->data.unary.operator = tokens[nextToken++];
    e->data.unary.right = prattParsePrecedence(prec_unary + 1);
    return e;
}

expr* prattBinary(expr* left){
    token t = tokens[nextToken++];
    expr* e = exprs + exprCount++;
    e->type = expr_binary;
    e->data.binary.left = left; 
    e->data.binary.operator = t;
    parse_rule* rule = getRule(t.type);
    e->data.binary.right = prattParsePrecedence(rule->prec + 1);
    return e;
}




int main(){

    #define LOOP 1
    #if LOOP
    while(1){
    #endif
        objectCount = 0;
        exprCount = 0;
        tokenCount = 0;
        processingError = 0;
        runtimeError = 0;
        parsingError = 0;
        astStrCount = 0;
        memset(varStorage, 0, 2048);
        #if LOOP
            fputs("langtest>", stdout);
            fgets(input, 2048, stdin);
            parser(input);
        #else
            char testStr[256] = "1 + 1";
            parser(testStr);
        #endif

        
        if(parsingError){
            printf("ERROR IN THE CODE! NOT EVALUATING\n");
            #if LOOP
            continue;
            #else
            return;
            #endif
        }
        printTokens();

        nextToken = 0;
        processingError = 0;
        expr* e = expression();

        if(processingError){
            printf("ERROR IN EXPRESSION EVALUATION! NOT EVALUATING\n");
            #if LOOP
            continue;
            #else
            return;
            #endif

        }
        printExpression(e);
        printf("RECRS: %s\n", astStr);


        //TEMPORARY TESTING TO COMPARE AGAINST RECURSIVE DESCENT
        #if 0 
            nextToken = 0;
            exprCount = 0;
            e = prattExpression();
            astStrCount = 0;
            printExpression(e);
            printf("PRATT: %s\n", astStr);
        #endif

        objectCount = 0;
        runtimeError = 0;
        object* o = evaluateExpression(e);
        printObject(o);

    #if LOOP
    }
    #endif

}


//statements -> printStatements/expressionStatements/declaration = 2 hours
//expressions   
//assignment    = 2 hours
//equality      x
//comparison    x
//term      x
//factor    x
//unary     x
//call (function) = 4 hours
//primary x

//10 hours
//pratt parsing -> 10 hours
//IR (intermediate represetation) MOV $0 #0 -> 10 hours

//int x = 0; -> declaration
//int y = 1;
//int z = 0;
//z = x + y; -> assign
//


//LONGTERM PLAN:
//get working:
//int x = 1;
//int y = 1;
//int z = x + y;

//requires statements (declaration statements)
//assignment

//break down into VM text commands
//MOV $0 #1 ;x = 1
//MOV $1 #1 ;y = 1
//ADD $0 $1 ;x + y
//