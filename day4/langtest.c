/*
    DAY 4
    https://www.youtube.com/watch?v=AvF5xtuNb7A
    
    Fixed some critical parsing bugs (more to be found later guaranteed)
    Added declaration statements
    Added additional instructions to the register based virtual machine (separate repo)
    Generated very simple/brittle text VM instructions

*/



#include <stdio.h>
#include <stdlib.h>



// Text styles
#define BOLD        "\033[1m"
#define DIM         "\033[2m"
#define ITALIC      "\033[3m"    // Not widely supported
#define UNDERLINE   "\033[4m"
#define BLINK       "\033[5m"    // Not widely supported
#define REVERSE     "\033[7m"    // Inverts fg/bg colors
#define HIDDEN      "\033[8m"
#define STRIKE      "\033[9m"    // Not widely supported

// Regular colors
#define BLACK       "\033[30m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"
#define DEFAULT     "\033[39m"

// Bold/bright colors
#define BBLACK      "\033[1;30m"
#define BRED        "\033[1;31m"
#define BGREEN      "\033[1;32m"
#define BYELLOW     "\033[1;33m"
#define BBLUE       "\033[1;34m"
#define BMAGENTA    "\033[1;35m"
#define BCYAN       "\033[1;36m"
#define BWHITE      "\033[1;37m"

// Background colors
#define BG_BLACK    "\033[40m"
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"
#define BG_MAGENTA  "\033[45m"
#define BG_CYAN     "\033[46m"
#define BG_WHITE    "\033[47m"
#define BG_DEFAULT  "\033[49m"

#define RESET       "\033[0m"

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
    int line;
    union{
        int integer;
        int varOffset;
    }data;
}token;

typedef struct expr expr;

typedef enum{
    expr_none,
    expr_variable,
    expr_primary,
    expr_unary,
    expr_binary,
    expr_factor,
    expr_term,
    expr_comparison,
    expr_equality,
    expr_grouping,
    expr_assignment,
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

typedef struct variable_expr{
    int varOffset;
}variable_expr;

typedef struct assignment_expr{
    expr* left; //variable expr
    expr* right; 
}assignment_expr;


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
        variable_expr variable;
        primary_expr primary;
        unary_expr unary;
        binary_expr binary;
        grouping_expr grouping;
        assignment_expr assignment;
    }data;
}expr;


typedef struct hashEntry{
    u32 hash;
    char* str;
}hashEntry;


//for determining the value and position of a local (x y or z) on the VM stack
typedef struct localsHashEntry{
    u32 hash;
    char* str;
    int value;//what to push to the stack
    int localPos;//position on the stack
}localsHashEntry;


typedef enum{
    stmt_none,
    stmt_decl,
    stmt_expr,

}stmt_type;


typedef struct decl_stmt{
    token_type type;
    expr* e;//assignment
}decl_stmt;

typedef struct expr_stmt{
    expr* e;
}expr_stmt;


typedef struct stmt{
    stmt_type type;
    union{
        decl_stmt decl;
        expr_stmt expr;
    }data;
}stmt;

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
stmt stmts[2048]; //arena for statement allocation
stmt* blockStmtChildren[2048]; //sequential list of statements within blocks
stmt* blockStmtBuilder[2048];
int blockStmtBuilderCount = 0;


char varStorage[2048];
hashEntry hashTable[HASH_TABLE_SIZE];
localsHashEntry localsHashTable[HASH_TABLE_SIZE];

object objects[2048];
int objectCount = 0;

char astStr[2048];
int astStrCount = 0;

int programCount;

int varStorageOffset = 0;
int tokenCount = 0;
int exprCount = 0;
int stmtCount = 0;
int blockStmtChildrenCount = 0;


int cursor = 0;

int parsingError = 0;
int processingError = 0;
int errors = 0;
int runtimeError = 0;

int nextToken = 0;

int parserLine = 1;
char* inputLines[2048];

char tempErrorLine[2048];
int tempErrorLineCount = 0;


char byteStr[2048];
int byteStrCount = 0;

//experimental TAC emission
//variables can only be integers ATM
int locals[128];

//30 is reserved for the frame pointer
//31 is reserved for the stack pointer
//broken way to handle register allocation
#define MAX_REGISTER (29) // 0 to 29 

int currentRegister = 0;
int localCount = 0;
int startByteCodeCount = 0;
int byteCodeCount = 3;


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


void printErrorLine(int line){
    tempErrorLineCount = 0;
    char* str = inputLines[line];
    int i = 0;
    while((*str) != '\0' && (*str) != '\n'){
        tempErrorLine[i++] = *str;
        str++;
    }
    printf("ERROR LINE %3d : %s%s%s\n", line, BRED, tempErrorLine, RESET);
}


void tokenError(token t, const char* str){
    printErrorLine(t.line);
    if(!str){
        printf("TOKEN ERROR: %s\n", tokStr(t.type));
    }else{
        printf("TOKEN ERROR: %s | %s\n", tokStr(t.type), str);
    }
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

localsHashEntry* getLocalsHashTable(char* str){
    localsHashEntry* entry = NULL;

    u32 hash = hashStr(str);
    hash = hash & (HASH_TABLE_SIZE - 1);

    if((localsHashTable + hash)->str != NULL){
        return localsHashTable + hash;
    }else{
        return NULL;
    }
}


localsHashEntry* pushToLocalsHashTable(char* str, int value, int localPos){
    localsHashEntry entry = {};
    u32 hash = hashStr(str);
    hash = hash & (HASH_TABLE_SIZE - 1);

    entry.hash = hash;
    entry.str = str;
    entry.value = value;
    entry.localPos = localPos;
    if((localsHashTable + hash)->str != NULL){
        printf("HASH ENTRY ERROR! TABLE SLOT IS ALREADY OCCUPIED!\n");
        return NULL;
    }else{
        localsHashTable[hash] = entry;
        PDEBUG("PUSHED %s TO HASH TABLE AT ENTRY: %u\n", str, hash);
        return localsHashTable + hash;
    }
}



token_type peekToken(int curToken){
    return tokens[curToken + 1].type;
}

/*
expression    -> assignment
assignment    -> comparison (=) comparison
equality       → assignment ( ( "!=" | "==" ) comparison )* ;
comparison     → term       ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor     ( ( "-" | "+" ) factor )* ;
factor         → unary      ( ( "/" | "*" ) unary )* ;
unary          → ( "!" | "-" ) unary | primary ;
call          -> (functions)
primary        → NUMBER | STRING | "true" | "false" | "nil" | VARIABLE
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
    }else if(tokens[nextToken].type == tok_var){
        e = exprs + exprCount++;
        e->type = expr_variable;
        e->data.variable.varOffset = tokens[nextToken].data.varOffset;
        nextToken++;
    }else{
        processingError++;
        tokenError(tokens[nextToken], "UNHANDLED TOKEN TYPE!");
        // printf("ERROR: UNHANDLED TOKEN TYPE: %s\n", tokStr(tokens[nextToken].type));
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

expr* assignment(){
    expr* e = equality();

    if(e->type == expr_variable){
        expr* left = e;//variable expression
        if(tokens[nextToken].type != tok_equal){
            tokenError(tokens[nextToken], "expected equals sign after variable");
            processingError++;
            return e;
        }
        nextToken++;
        e = exprs + exprCount++;
        e->type = expr_assignment;
        e->data.assignment.left = left;
        e->data.assignment.right = expression();
    }

    return e;
}

expr* expression(){
    expr* e = assignment();
    
    return e;
}

stmt* declstmt(){
    stmt* s = NULL;
    //reads the int token
    //int x = 1;
    token typet = tokens[nextToken++];

    token vart = tokens[nextToken];
    expr* e = NULL;
    if(vart.type == tok_var){
        e = expression();
    }else{
        tokenError(vart, "EXPECTED VARIABLE/DECLARATOR AFTER TYPE!");
        processingError++;
        return s;
    }

    if(tokens[nextToken].type != tok_semicolon){
        tokenError(tokens[nextToken-1], "EXPECTED SEMICOLON AFTER EXPRESSION!");
        processingError++;
        return s;
    }
    nextToken++;

    s = stmts + stmtCount++;
    s->type = stmt_decl;
    s->data.decl.type = typet.type;
    s->data.decl.e = e;

    return s;
}
stmt* exprstmt(){
    expr* e = NULL;
    stmt* s = NULL;

    e = expression();
    if(processingError){
        return s;
    }
    
    if(tokens[nextToken].type != tok_semicolon){
        tokenError(tokens[nextToken-1], "EXPECTED SEMICOLON AFTER EXPRESSION!");
        processingError++;
        return s;
    }
    nextToken++;


    s = stmts + stmtCount++;
    s->type = stmt_expr;
    s->data.expr.e = e;

    return s;
}

stmt* statement(){
    stmt* s = NULL;
    token t = tokens[nextToken];
    if(t.type == tok_int){
        s = declstmt();
    }else{
        s = exprstmt();
    }
    return s;
}

void printExpression(expr* e);

void printVariable(expr* e){
    astStrCount += sprintf(astStr + astStrCount, "%s ", varStorage + e->data.variable.varOffset);
}

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

void printAssignment(expr* e){
    //print left hand expression variable
    printExpression(e->data.assignment.left);

    //print equals sign
    astStrCount += sprintf(astStr + astStrCount, "= ");

    //print right expression
    printExpression(e->data.assignment.right);

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
        case expr_assignment:{
            printAssignment(e);
        }break;
        case expr_variable:{
            printVariable(e);
        }break;

        default:{}break;

    }
}

void printDeclStmt(stmt* s){
    //print token type
    switch(s->data.decl.type){
        case tok_int   :{astStrCount += sprintf(astStr + astStrCount, "int ");}break;
        default:{}break;
    }
    printExpression(s->data.decl.e);
}

void printStmt(stmt* s){
    switch(s->type){
        case stmt_decl:{
            printDeclStmt(s);
        }break;
        case stmt_expr:{
            printExpression(s->data.expr.e);
        }break;
        default:{}break;
    }
}


void emitBytesExpression(expr* e);

void emitBytesExprVariable(expr* e){
    printf("emitBytesExprVariable()\n");
    char* varStr = varStorage + e->data.variable.varOffset;
    localsHashEntry* entry = getLocalsHashTable(varStr);
    if(!entry){
        printf("BYTECODE EMISSION ERROR! %s HAS NO VALID HASHTABLE ENTRY!\n", varStr);
        runtimeError++;
        return;
    }

    int value = entry->value;
    printf("LOAD  $%d [$30 + %d]\n", currentRegister, entry->localPos*4);
    byteStrCount += sprintf(byteStr + byteStrCount, "LOAD  $%d [$30 + %d]   ;%d\\n\\\n", currentRegister, entry->localPos*4, (byteCodeCount++)*4);
    currentRegister++;
    if(currentRegister > MAX_REGISTER){
        currentRegister = 0;
    }
}

void emitBytesExprPrimary(expr* e){
    //assume its a number
    Assert(0);
}

void emitBytesExprBinary(expr* e){
    //assume its a number
    
    //we can currently only handle addition
    Assert(e->data.binary.operator.type == tok_plus);
    //CAN ONLY HANDLE + OPERATORS

    int firstRegisterPos = currentRegister;
    emitBytesExpression(e->data.binary.left);
    //LOAD ONTO REGISTER LOCALX

    int secondRegisterPos = currentRegister;
    emitBytesExpression(e->data.binary.right);
    //LOAD ONTO REGISTER LOCALY
    
    printf("ADD $%d $%d\n", firstRegisterPos, secondRegisterPos);
    byteStrCount += sprintf(byteStr + byteStrCount, "ADD $%d $%d  ;%d\\n\\\n", firstRegisterPos, secondRegisterPos, (byteCodeCount++)*4);


}


void emitBytesExprAssignment(expr* e){
    //assume the result is a number

    //the end result of assignment expression
    //get the stack position of the variable assigned to
    //compute the result of the right hand expression
    //
    //LOAD [$30 - (left expr stackPos)] rightHandExpressionResult
    int firstRegister = currentRegister;
    emitBytesExpression(e->data.assignment.right);
    
    //CAN ONLY HANDLE EXPRESSION VARIABLES
    Assert(e->data.assignment.left->type == expr_variable);

    char* varStr = varStorage + e->data.assignment.left->data.variable.varOffset;
    localsHashEntry* entry = getLocalsHashTable(varStr);
    if(!entry){
        printf("BYTECODE EMISSION ERROR! %s HAS NO VALID HASHTABLE ENTRY!\n", varStr);
        runtimeError++;
        return;
    }

    int value = entry->value;
    printf("LOAD  [$30 + %d] $%d\n", entry->localPos*4, firstRegister);
    byteStrCount += sprintf(byteStr + byteStrCount, "LOAD  [$30 + %d] $%d ;%d\\n\\\n", entry->localPos*4, firstRegister, (byteCodeCount++)*4);

}


void emitBytesExpression(expr* e){
    switch(e->type){
        case expr_primary:{
            emitBytesExprPrimary(e);
        }break;
        case expr_variable:{
            emitBytesExprVariable(e);
        }break;
        case expr_assignment:{
            emitBytesExprAssignment(e);
        }break;
        case expr_binary:{
            emitBytesExprBinary(e);
        }break;
        default:{}break;
    }
}

int getValueOfExpression(expr* e){
    Assert(e->type == expr_primary){
        return e->data.primary.t.data.integer;
    }
    return 0;
}



void emitBytesDeclStmt(stmt* s){
    //every variable will have a location on the VM stack
    //based on the localCount
    printf("emitBytesDeclStmt()\n");

    int value = getValueOfExpression(s->data.decl.e->data.assignment.right);
    char* varStr = varStorage + s->data.decl.e->data.assignment.left->data.variable.varOffset;
    printf("identifier: %s\n", varStr);
    localsHashEntry* entry = getLocalsHashTable(varStr);
    
    if(entry){
        

    }else{//entry is null, push to table
        entry = pushToLocalsHashTable(varStr, value, ++localCount);
        //this is the first instance of the variable, we have the value, we have the local
        
        //we have a new local, it has a value, we have the local position (stackPos)


        //allocate some amount of local space on the stack, need to know ahead of time
        //LOAD [$30 - (localCount)*4] (value)
        //LOAD [$30 - 4] 1
        byteStrCount += sprintf(byteStr + byteStrCount, "LOAD  $%d #%d  ;%d\\n\\\n", currentRegister, entry->value, (byteCodeCount++)*4);
        printf("LOAD  $%d #%d\n", currentRegister, entry->value);

        byteStrCount += sprintf(byteStr + byteStrCount, "LOAD  [$30 + %d] $%d   ;%d\\n\\\n", localCount*4, currentRegister, (byteCodeCount++)*4);
        printf("LOAD  [$30 + %d] $%d\n", localCount*4, currentRegister);

    }

    //need to map the variable name to a local position
    //hash variable name
    //check if its already in the hash
    //if not, assign it a local
    //if it is, get back the local

    // emitBytesExpression()
    // printf("%d", *(int*)s);
    // printf("%p", s);
    // locals[localCount++] = s->data.decl.e->data.assignment.right->
    // byteStrCount += sprintf(byteStr + byteStrCount, "int ");
}





void emitBytesStmt(stmt* s){
        switch(s->type){
        case stmt_decl:{
            emitBytesDeclStmt(s);
        }break;
        case stmt_expr:{
            emitBytesExpression(s->data.expr.e);
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
    if(c >= 'A' && c <= 'Z')return 1;
    if(c >= 'a' && c <= 'z')return 1;
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

void addToken(token t){
    t.line = parserLine;
    tokens[tokenCount++] = t;
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
            addToken(t);
        }
        else if(isAlpha(c)){
            token_type type = isKeyword(input + i);
            if(type){
                printf("KEYWORD FOUND | %s\n", tokStr(type));
                token t = {};
                switch(type){
                    case tok_int        :{i += 3; t.type = tok_int        ;}break;
                    case tok_char       :{i += 4; t.type = tok_char       ;}break;
                    case tok_bool       :{i += 4; t.type = tok_bool       ;}break;
                    case tok_true       :{i += 4; t.type = tok_true       ;}break;
                    case tok_false      :{i += 5; t.type = tok_false      ;}break;
                    default:{}break;
                }
                addToken(t);
            }else{

                int start = varStorageOffset;
                while(isAlpha(c) || isNum(c)){
                    //TODO:
                        //either need to switch to a hash table
                        //or store start and length to lookup in the original code text
                    varStorage[varStorageOffset++] = c;
                    c = input[++i];
                }
                varStorageOffset++;
                token t = {};
                t.type = tok_var;
                t.data.varOffset = start;
                printf("token var = %s\n", varStorage + t.data.varOffset);
                addToken(t);
                
            }
        }
        else if(!isWhitespace(c)){
            switch(c){
                case '+':{token t = {}; t.type = tok_plus;          addToken(t); i++;}break;
                case '-':{token t = {}; t.type = tok_minus;         addToken(t); i++;}break;
                case '/':{token t = {}; t.type = tok_slash;         addToken(t); i++;}break;
                case '*':{token t = {}; t.type = tok_star;          addToken(t); i++;}break;
                case ';':{token t = {}; t.type = tok_semicolon;     addToken(t); i++;}break;
                case '(':{token t = {}; t.type = tok_lparen;        addToken(t); i++;}break;
                case ')':{token t = {}; t.type = tok_rparen;        addToken(t); i++;}break;
                case '{':{token t = {}; t.type = tok_lcurly;        addToken(t); i++;}break;
                case '}':{token t = {}; t.type = tok_rcurly;        addToken(t); i++;}break;
                case '=':{token t = {}; t.type = tok_equal;         addToken(t); i++;}break;
                default:{
                    i++;
                    printf("ERROR: UNHANDLED CHAR: %c\n", c);
                    parsingError++;
                    return;
    
                }break;
            }

        }else if(isWhitespace(c)){
            i++;
            if(c == '\n'){
                parserLine++;
                inputLines[parserLine] = input + i;
            }
        }
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
    addToken(t);
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


int hstrcpy(char* dst, char* src){
    if(!dst)return 0;
    if(!src)return 0;
    int count = 0;
    while(*src != '\0'){
        *dst = *src;
        src++;
        dst++;
        count++;
    }
    return count;
}


int main(){

    #define LOOP 0
    #if LOOP
    while(1){
    #endif
        objectCount = 0;
        exprCount = 0;
        stmtCount = 0;
        blockStmtChildrenCount = 0;
        blockStmtBuilderCount = 0;
        tokenCount = 0;
        processingError = 0;
        runtimeError = 0;
        parsingError = 0;
        astStrCount = 0;
        memset(varStorage, 0, 2048);
        #if LOOP
            fputs("langtest>", stdout);
            fgets(input, 2048, stdin);
        #else
            char testStr[2048] =
                "int x = 1;\n"
                "int y = 2;\n"
                "int z = 0;\n"
                "z = x + y;\n"
            ;
            int inputSize = hstrcpy(input, testStr);
        #endif
        inputLines[1] = input;

        parser(input);
        
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
        while(tokens[nextToken].type != tok_eof){
            blockStmtChildren[blockStmtChildrenCount++] = statement();

            //panic mode, synchronization skip until we hit another semicolon
            if(processingError){
                while(tokens[nextToken].type != tok_semicolon){
                    if(tokens[nextToken].type == tok_eof){
                        break;
                    }
                    nextToken++;
                }
                if(tokens[nextToken].type == tok_semicolon)nextToken++;
                processingError = 0;
                errors++;
            }

        }
        if(errors){
            #if LOOP
                continue;
            #else
                return;
            #endif
        }


        for(int i = 0; i < blockStmtChildrenCount; ++i){
            astStrCount = 0;
            stmt* s = blockStmtChildren[i];
            printStmt(s);
            printf("%s\n", astStr);
        }

        //emit bytecode
        byteStrCount = 0;
        for(int i = 0; i < blockStmtChildrenCount; ++i){
            stmt* s = blockStmtChildren[i];
            emitBytesStmt(s);
        }            
        printf("COMPILED BYTECODE:\n");
        printf("LOAD $0  #%d    ;%u\\n\\\n", localCount*4, (startByteCodeCount++)*4);
        printf("SUB $31 $0      ;%u\\n\\\n", (startByteCodeCount++)*4);
        printf("LOAD $30 $31    ;%u\\n\\\n", (startByteCodeCount++)*4);//FP = SP
        printf("%s\n", byteStr);


        if(processingError){
            printf("ERROR IN EXPRESSION EVALUATION! NOT EVALUATING\n");
            #if LOOP
                continue;
            #else
                return;
            #endif

        }
        #if 0
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
        #endif
    
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
//MOV $1 #1 ;y = 2
//ADD $0 $1 ;x + y
//


/*
    WRONG COMPILED BYTECODE:

    SUB $31 #12
    MOV $30 $31
    LOAD  $1 [$30 - 4]
    LOAD  $3 [$30 - 8]
    ADD $0 $2
    LOAD  [$30 - 12] $0
*/

/*
    CORRECT COMPILED BYTECODE
    LOAD $0  #12    ;0\n\
    SUB $31 $0      ;4\n\
    LOAD $30 $31    ;8\n\
    LOAD  $0 #1  ;12\n\
    LOAD  [$30 + 4] $0   ;16\n\
    LOAD  $0 #2  ;20\n\
    LOAD  [$30 + 8] $0   ;24\n\
    LOAD  $0 #0  ;28\n\
    LOAD  [$30 + 12] $0   ;32\n\
    LOAD  $0 [$30 + 4]   ;36\n\
    LOAD  $1 [$30 + 8]   ;40\n\
    ADD $0 $1  ;44\n\
    LOAD  [$30 + 12] $0 ;48\n\
    
    */

//TODO TO VM:
//print every parsed line for easier debugging
//print every generated opcode for every line
//add direct constant loading to local indices on the stack (for local variables)

