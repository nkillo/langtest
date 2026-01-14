/*
    DAY 6 - 1/14/26

    Added a minimal virtual machine
    Only handles brittle loading of immediate values into registers, and adding registers

    All the tokens across both languages are in a single enum
    Separate assembly-like language parser function
    brittle/broken opcode emission
    Simple vm runtime loop

*/



#include <stdio.h>
#include <stdlib.h>

#define BRED        "\033[1;31m"
#define RESET       "\033[0m"

#define u32 unsigned long int
#define s32 long int
#define i32 long int
#define u16 unsigned short
#define i16 short
#define u8  unsigned char
#define i8  char
#define s8  char
#define MAX_REGISTERS 32
#define MAX_VM_MEM (4 * 1024)
#define MAX_BYTECODE (4 * 1024)

#define Assert(Expression) if(!(Expression)){*(int*)0 = 0;}

#define DEBUG 1

#if DEBUG
    #define PDEBUG(...) printf(__VA_ARGS__);
#else
    #define PDEBUG(...)
#endif


//MOV = LOAD/STORE
//MOV32
//MOV16
//MOV8
//ADD
//SUB
//DIV
//MUL
//EQ    = x == y | set a flag
//NE    = x != y | set a flag
//LT    = x <  y | set a flag
//GT    = x >  y | set a flag
//LTE   = x <= y | set a flag
//GTE   = x >= y | set a flag
//JMP
//JEQ   = checks if the flag is set, and jumps
//JNE   = checks if flag is NOT equal and jumps
//INC
//DEC

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
    tok_if,
    tok_else,
    tok_elseif,
    tok_for,
    tok_while,
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
    tok_plusequal,
    tok_minequal,
    tok_lbrack,
    tok_rbrack,
    tok_eof,
    tok_REG,
    tok_octothorpe,
    
    tok_MOV,
    tok_MOV32,
    tok_MOV16,
    tok_MOV8,
    tok_ADD,
    tok_SUB,
    tok_DIV,
    tok_MUL,
    tok_EQ  ,
    tok_NE  ,
    tok_LT  ,
    tok_GT  ,
    tok_LTE ,
    tok_GTE ,
    tok_JMP,
    tok_JEQ ,
    tok_JNE ,
    tok_INC,
    tok_DEC,
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
    token operator;//prefix
    token postfix;
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
    int localPos;//position on the stack
}localsHashEntry;

typedef struct stmt stmt;

typedef struct block_link block_link;

typedef struct block_link{
    block_link* next;
    stmt* s;
}block_link;


typedef enum{
    stmt_none,
    stmt_decl,
    stmt_expr,
    stmt_block,
    stmt_forloop,
    stmt_whileloop,
    stmt_cond,

}stmt_type;

typedef struct loop_stmt{
    stmt* decl;//int i = 0;
    expr* cond;// i < x;
    expr* inc; // i++
    stmt* block;//for(;;){int x = i; print x}
}loop_stmt;


typedef struct cond_stmt{
    expr* e;//conditional expression
    stmt* block;
}cond_stmt;



typedef struct block_stmt{
    int count;
    block_link* start;
}block_stmt;



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
        block_stmt block;
        loop_stmt loop;
        cond_stmt cond;
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

typedef struct VM{
    u8 bytecode[MAX_BYTECODE]; //actual commands
    u32 bytecodeCount; //how many instructions there are 

    u32 ip;//instruction pointer | current instruction
    u32 registers[MAX_REGISTERS];
    
    u32 framePointer; //points to the latest frame in the stack
    u32 stackPointer; //points to the top of the stack
    
    u8 mem[MAX_VM_MEM];

    //unused, if you want floating point operations you would need more instructions
    float fregisters[MAX_REGISTERS]; 


}VM;


#define HASH_BUCKETS 4
#define HASH_TABLE_SIZE 2048

VM vm = {};
char vmInput[2048];
token vmTokens[2048];
char vmVarStorage[2048];
int vmVarStorageOffset = 0;
int vmParserLine = 1;
char* vmInputLines[2048];
token vmTokens[2048];
int vmTokenCount = 0;
char tempVMLine[2048];
int tempVMLineCount = 0;


char input[2048];
token tokens[2048];
expr exprs[2048];
stmt stmts[2048]; //arena for statement allocation
block_link blockLinks[2048];
int blockLinkCount = 0;

stmt* globalStmts[2048]; //sequential list of statements within blocks
int globalStmtCount = 0;

char varStorage[2048];
hashEntry hashTable[HASH_TABLE_SIZE];
localsHashEntry localsHashTable[16][HASH_TABLE_SIZE];
int maxScope = 16;
int currentScope = 0;

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
int backps[64];
int backpcount = 0;

const char* tokStr(token_type t){
    switch(t){
        case tok_none       :{return "tok_none";}break;
        case tok_int        :{return "tok_int";}break;
        case tok_num        :{return "tok_num";}break;
        case tok_char       :{return "tok_char";}break;
        case tok_bool       :{return "tok_bool";}break;
        case tok_if         :{return "tok_if";}break;
        case tok_else       :{return "tok_else";}break;
        case tok_elseif     :{return "tok_elseif";}break;
        case tok_for        :{return "tok_for";}break;
        case tok_while      :{return "tok_while";}break;
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
        case tok_plusequal  :{return "tok_plusequal";}break;
        case tok_minequal   :{return "tok_minequal";}break;
        case tok_lbrack     :{return "tok_lbrack";}break;
        case tok_rbrack     :{return "tok_rbrack";}break;
        case tok_eof        :{return "tok_eof";}break;

        case tok_MOV :{return "tok_MOV";}break;
        case tok_MOV32 :{return "tok_MOV32";}break;
        case tok_MOV16 :{return "tok_MOV16";}break;
        case tok_MOV8 :{return "tok_MOV8";}break;
        case tok_ADD :{return "tok_ADD";}break;
        case tok_SUB :{return "tok_SUB";}break;
        case tok_DIV :{return "tok_DIV";}break;
        case tok_MUL :{return "tok_MUL";}break;
        case tok_EQ :{return "tok_EQ";}break;
        case tok_NE :{return "tok_NE";}break;
        case tok_LT :{return "tok_LT";}break;
        case tok_GT :{return "tok_GT";}break;
        case tok_LTE :{return "tok_LTE";}break;
        case tok_GTE :{return "tok_GTE";}break;
        case tok_JMP :{return "tok_JMP";}break;
        case tok_JEQ :{return "tok_JEQ";}break;
        case tok_JNE :{return "tok_JNE";}break;
        case tok_INC :{return "tok_INC";}break;
        case tok_DEC :{return "tok_DEC";}break;
        case tok_REG :{return "tok_REG";}break;
        case tok_octothorpe:{return "tok_octothorpe";}break;
        default:{}return "";
    }
    return "TEST";
}


void printVMLine(int line){
    tempVMLineCount = 0;
    char* str = vmInputLines[line];
    int i = 0;
    while((*str) != '\0' && (*str) != '\n'){
        tempVMLine[i++] = *str;
        str++;
    }
    tempVMLine[i] = 0;
    printf("VM LINE %3d : %s%s%s\n", line, BRED, tempVMLine, RESET);

}


void printErrorLine(int line){
    tempErrorLineCount = 0;
    char* str = inputLines[line];
    int i = 0;
    while((*str) != '\0' && (*str) != '\n'){
        tempErrorLine[i++] = *str;
        str++;
    }
    tempVMLine[i] = 0;
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

localsHashEntry* getLocalsHashTable(char* str, int scope){
    localsHashEntry* entry = NULL;

    u32 hash = hashStr(str);
    hash = hash & (HASH_TABLE_SIZE - 1);

    if((localsHashTable[scope] + hash)->str != NULL){
        return localsHashTable[scope] + hash;
    }else{
        printf("VAR %s NOT FOUND AT DEPTH %d\n", str, scope);
        if(scope > 0){
            entry = getLocalsHashTable(str, scope - 1);
        }
        return entry;
    }
}


localsHashEntry* pushToLocalsHashTable(char* str, int localPos, int scope){
    localsHashEntry entry = {};
    u32 hash = hashStr(str);
    hash = hash & (HASH_TABLE_SIZE - 1);

    entry.hash = hash;
    entry.str = str;
    entry.localPos = localPos;
    if((localsHashTable[scope] + hash)->str != NULL){
        printf("HASH ENTRY ERROR! TABLE SLOT IS ALREADY OCCUPIED!\n");
        return NULL;
    }else{
        localsHashTable[scope][hash] = entry;
        PDEBUG("PUSHED %s TO HASH TABLE AT ENTRY: %u\n", str, hash);
        return localsHashTable[scope] + hash;
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
    if(tokens[nextToken].type == tok_minus || tokens[nextToken].type == tok_bang || tokens[nextToken].type == tok_pplus || tokens[nextToken].type == tok_mminus){
        e = exprs + exprCount++;
        e->type = expr_unary;
        e->data.unary.operator = tokens[nextToken++];
        e->data.unary.right = primary();
    }else{
        e = primary();
        if(tokens[nextToken].type == tok_mminus || tokens[nextToken].type == tok_pplus){
            printf("CANNOT YET HANDLE POST FIX OPERATORS\n");
            Assert(0);
        }
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


    while(tokens[nextToken].type == tok_gr || tokens[nextToken].type == tok_grequal || tokens[nextToken].type == tok_less || tokens[nextToken].type == tok_lequal){
        expr* laste = e;
        e = exprs + exprCount++;
        e->type = expr_binary;
        e->data.binary.left = laste;
        e->data.binary.operator = tokens[nextToken++];
        e->data.binary.right = factors();
    }

    return e;
}

expr* equality(){
    expr* e = comparison();

    while(tokens[nextToken].type == tok_eequal || tokens[nextToken].type == tok_nequal){
        expr* laste = e;
        e = exprs + exprCount++;
        e->type = expr_binary;
        e->data.binary.left = laste;
        e->data.binary.operator = tokens[nextToken++];
        e->data.binary.right = factors();
    }

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
        e->data.assignment.right = equality();
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

stmt* statement();

stmt* blockstmt(){
    stmt* s = stmts + stmtCount++;
    nextToken++;//advances past the lcurly
    s->type = stmt_block;
    s->data.block.start = blockLinks + blockLinkCount;
    block_link* link = s->data.block.start;

    while(tokens[nextToken].type != tok_rcurly){
        blockLinkCount++;
        link->s = statement();

        //defer error handling to later
        Assert(processingError == 0);

        if(tokens[nextToken].type != tok_rcurly){
            link->next = blockLinks + blockLinkCount;
            link = link->next;
        }
    }
    if(tokens[nextToken].type == tok_rcurly)nextToken++;

    return s;
}

stmt* forstmt(){
    nextToken++;
    stmt* s = stmts + stmtCount++;
    s->type = stmt_forloop;
    if(tokens[nextToken].type == tok_lparen){
        nextToken++;
        s->data.loop.decl = statement();
        
        s->data.loop.cond = expression();
        if(tokens[nextToken].type == tok_semicolon)nextToken++;
        
        s->data.loop.inc  = expression();
        if(tokens[nextToken].type != tok_rparen){
            printf("EXPECTED CLOSING PARENTHESES AFTER FOR LOOP STATEMENTS!\n");
            Assert(0);
        }
        nextToken++;

        s->data.loop.block  = statement();

    }else{
        Assert(0);
        processingError++;
        printf("EXPECTED OPENING PARANETHESES AFTER FOR\n");
        return NULL;
    }

    if(tokens[nextToken].type == tok_semicolon)nextToken++;

    return s;
}
stmt* whilestmt(){
    nextToken++;
    stmt* s = stmts + stmtCount++;
    s->type = stmt_whileloop;

    if(tokens[nextToken].type == tok_lparen){
    }else{
        Assert(0);
        processingError++;
        printf("EXPECTED OPENING PARANETHESES AFTER WHILE\n");
        return NULL;
    }

    return s;

}

//AST generation with recursive descent
stmt* statement(){
    stmt* s = NULL;
    token t = tokens[nextToken];
    if(t.type == tok_int){
        s = declstmt();
    }else if(t.type == tok_lcurly){
        s = blockstmt();
    }else if(t.type == tok_for){
        s = forstmt();
    }else if(t.type == tok_while){
        s = whilestmt();
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
        case tok_plus       :{astStrCount += sprintf(astStr + astStrCount, "+ ");   }break;
        case tok_minus      :{astStrCount += sprintf(astStr + astStrCount, "- ");   }break;
        case tok_slash      :{astStrCount += sprintf(astStr + astStrCount, "/ ");   }break;
        case tok_star       :{astStrCount += sprintf(astStr + astStrCount, "* ");   }break;
        case tok_bang       :{astStrCount += sprintf(astStr + astStrCount, "! ");   }break;
        case tok_pplus      :{astStrCount += sprintf(astStr + astStrCount, "++ ");  }break;
        case tok_mminus     :{astStrCount += sprintf(astStr + astStrCount, "-- ");  }break;
        case tok_eequal     :{astStrCount += sprintf(astStr + astStrCount, "== ");  }break;
        case tok_nequal     :{astStrCount += sprintf(astStr + astStrCount, "!= ");  }break;
        case tok_gr         :{astStrCount += sprintf(astStr + astStrCount, "> ");   }break;
        case tok_less       :{astStrCount += sprintf(astStr + astStrCount, "< ");   }break;
        case tok_grequal    :{astStrCount += sprintf(astStr + astStrCount, ">= ");  }break;
        case tok_lequal     :{astStrCount += sprintf(astStr + astStrCount, "<= ");  }break;
        case tok_colon      :{astStrCount += sprintf(astStr + astStrCount, ": ");   }break;
        case tok_semicolon  :{astStrCount += sprintf(astStr + astStrCount, "; ");   }break;
        case tok_plusequal  :{astStrCount += sprintf(astStr + astStrCount, "+= ");  }break;
        case tok_minequal   :{astStrCount += sprintf(astStr + astStrCount, "-= ");  }break;
        case tok_lbrack     :{astStrCount += sprintf(astStr + astStrCount, "[ ");   }break;
        case tok_rbrack     :{astStrCount += sprintf(astStr + astStrCount, "] ");   }break;

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

void printStmt(stmt* s, int depth);

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

void printDeclStmt(stmt* s, int depth){
    char indent[32] = {};
    for(int i = 0; i < depth; ++i){
        indent[i] = '\t';
    }

    //print token type
    switch(s->data.decl.type){
        case tok_int   :{astStrCount += sprintf(astStr + astStrCount, "%sint ", indent);}break;
        default:{}break;
    }
    printExpression(s->data.decl.e);
}

void printBlockStmt(stmt* s, int depth){
    char indent[32] = {};
    for(int i = 0; i < depth; ++i){
        indent[i] = '\t';
    }

    astStrCount += sprintf(astStr + astStrCount, "%s{\n", indent);
    block_link* link = s->data.block.start;
    while(link){
        printStmt(link->s, depth+1);
        astStrCount += sprintf(astStr + astStrCount, "\n");
        link = link->next;
    }
    astStrCount += sprintf(astStr + astStrCount, "%s}\n", indent);

}

void indentStr(char* str, int strlen, int depth){
    if(strlen < depth)return;
    for(int i = 0; i < depth; ++i){
        str[i] = '\t';
    }
}

void printForLoopStmt(stmt* s, int depth){
    char indent[32] = {};
    indentStr(indent, 32, depth);

    astStrCount += sprintf(astStr + astStrCount, "for ( ", indent);
    printStmt(s->data.loop.decl, depth);
    astStrCount += sprintf(astStr + astStrCount, "; ", indent);
    printExpression(s->data.loop.cond);
    astStrCount += sprintf(astStr + astStrCount, "; ", indent);
    printExpression(s->data.loop.inc);
    astStrCount += sprintf(astStr + astStrCount, ")", indent);
    printBlockStmt(s->data.loop.block, depth);
}


void printStmt(stmt* s, int depth){
    char indent[32] = {};
    for(int i = 0; i < depth; ++i){
        indent[i] = '\t';
    }

    
    switch(s->type){
        case stmt_decl:{
            printDeclStmt(s, depth);
        }break;
        case stmt_expr:{
            astStrCount += sprintf(astStr + astStrCount, "%s", indent);
            printExpression(s->data.expr.e);
        }break;
        case stmt_block:{
            printBlockStmt(s, depth);
        }break;
        case stmt_forloop:{
            printForLoopStmt(s, depth);
        }break;
        default:{}break;
    }
}


void emitBytesExpression(expr* e, int scope);

void emitBytesExprVariable(expr* e, int scope){
    printf("emitBytesExprVariable()\n");
    char* varStr = varStorage + e->data.variable.varOffset;
    localsHashEntry* entry = getLocalsHashTable(varStr, scope);
    if(!entry){
        printf("BYTECODE EMISSION ERROR! %s HAS NO VALID HASHTABLE ENTRY!\n", varStr);
        runtimeError++;
        return;
    }

    printf("LOAD  $%d [$30 + %d]\n", currentRegister, entry->localPos*4);
    byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-10d[$30 + %-2d]  ;%-3d\\n\\\n", "LOAD",currentRegister, entry->localPos*4, (byteCodeCount++)*4);
    currentRegister++;
    if(currentRegister > MAX_REGISTER){
        currentRegister = 0;
    }
}

void emitBytesExprPrimary(expr* e){
    //assume its a number
    Assert(e->data.primary.t.type == tok_num);

    printf("LOAD  $%d #%d           ;%d\n", currentRegister, e->data.primary.t.data.integer, (byteCodeCount)*4);
    byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-11d#%-10d;%-3d\\n\\\n", "LOAD",currentRegister, e->data.primary.t.data.integer, (byteCodeCount++)*4);
    currentRegister++;
}

void emitBytesExprBinary(expr* e, int scope){
    //assume its a number
    
    //we can currently only handle addition
    
    //CAN ONLY HANDLE + OPERATORS
    int firstRegisterPos = currentRegister;
    emitBytesExpression(e->data.binary.left, scope);
    //LOAD ONTO REGISTER LOCALX
    int secondRegisterPos = currentRegister;
    emitBytesExpression(e->data.binary.right, scope);
    switch(e->data.binary.operator.type){
        case tok_plus:{
            //LOAD ONTO REGISTER LOCALY
            printf("ADD $%d $%d\n", firstRegisterPos, secondRegisterPos);
            byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-10d $%-8d  ;%-3d\\n\\\n","ADD", firstRegisterPos, secondRegisterPos, (byteCodeCount++)*4);
            //TODO: constant folding if all expressions are literals/primaries
        }break;
        case tok_less:{

        }break;
        default:{
            Assert("UNHANDLED TOKEN TYPE IN EmitBytesBinary" && 0);
        }break;
    }
}


void emitBytesExprAssignment(expr* e, int scope){
    //assume the result is a number

    //the end result of assignment expression
    //get the stack position of the variable assigned to
    //compute the result of the right hand expression
    //
    //LOAD [$30 - (left expr stackPos)] rightHandExpressionResult
    int firstRegister = currentRegister;
    emitBytesExpression(e->data.assignment.right, scope);
    
    //CAN ONLY HANDLE EXPRESSION VARIABLES
    Assert(e->data.assignment.left->type == expr_variable);

    char* varStr = varStorage + e->data.assignment.left->data.variable.varOffset;
    localsHashEntry* entry = getLocalsHashTable(varStr, scope);
    if(!entry){
        printf("BYTECODE EMISSION ERROR! %s HAS NO VALID HASHTABLE ENTRY!\n", varStr);
        runtimeError++;
        return;
    }

    printf("LOAD  [$30 + %d] $%d\n", entry->localPos*4, firstRegister);
    byteStrCount += sprintf(byteStr + byteStrCount, "%-8s[$30 + %-2d]   $%-10d;%-3d\\n\\\n","LOAD", entry->localPos*4, firstRegister, (byteCodeCount++)*4);

}


void emitBytesExpression(expr* e, int scope){
    switch(e->type){
        case expr_primary:{
            emitBytesExprPrimary(e);
        }break;
        case expr_variable:{
            emitBytesExprVariable(e, scope);
        }break;
        case expr_assignment:{
            emitBytesExprAssignment(e, scope);
        }break;
        case expr_binary:{
            emitBytesExprBinary(e, scope);
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



void emitBytesDeclStmt(stmt* s, int scope){
    //every variable will have a location on the VM stack
    //based on the localCount
    printf("emitBytesDeclStmt()\n");

    char* varStr = varStorage + s->data.decl.e->data.assignment.left->data.variable.varOffset;
    printf("identifier: %s, localCount: %d\n", varStr, localCount+1);
    localsHashEntry* entry = pushToLocalsHashTable(varStr, localCount+1, scope);
    
    if(entry){
        localCount++;        



        //allocate some amount of local space on the stack, need to know ahead of time
        //LOAD [$30 - (localCount)*4] (value)
        //LOAD [$30 - 4] 1
        
        if(s->data.decl.e->data.assignment.right->type == expr_primary){
            int value = getValueOfExpression(s->data.decl.e->data.assignment.right);
            byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-11d#%-10d;%-3d\\n\\\n","LOAD", currentRegister, value, (byteCodeCount++)*4);
            printf("LOAD  $%d #%d\n", currentRegister, value);

            byteStrCount += sprintf(byteStr + byteStrCount, "%-8s[$30 + %-2d]   $%-10d;%-3d\\n\\\n","LOAD", localCount*4, currentRegister, (byteCodeCount++)*4);
            printf("LOAD  [$30 + %d] $%d\n", localCount*4, currentRegister);

        }else if(s->data.decl.e->data.assignment.right->type == expr_variable){
            char* entry2str = varStorage + s->data.decl.e->data.assignment.right->data.variable.varOffset;
            localsHashEntry* entry2 = getLocalsHashTable(entry2str, scope);
            byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-10d[$30 + %-2d]  ;%-3d\\n\\\n","LOAD", currentRegister, entry2->localPos*4, (byteCodeCount++)*4);
        
            byteStrCount += sprintf(byteStr + byteStrCount, "%-8s[$30 + %-2d]   $%-10d;%-3d\\n\\\n","LOAD", localCount*4, currentRegister, (byteCodeCount++)*4);
            printf("LOAD  [$30 + %d] $%d\n", localCount*4, currentRegister);

        }


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


void emitBytesStmt(stmt* s, int scope);

void emitBytesBlock(stmt* s, int scope){
    block_link* link = s->data.block.start;
    while(link){
        emitBytesStmt(link->s, scope);
        link = link->next;
    }
    int debug = 0;
}


void emitBytesForLoop(stmt* s, int scope){
    emitBytesDeclStmt(s->data.loop.decl, scope);
    int loopStartInstruction = byteCodeCount * 4;
    // byteStrCount += sprintf(byteStr + byteStrCount, "START OF FOR LOOP\n");
    
    // emitBytesExpression(s->data.loop.cond, scope);
    {//special case logic for jumping in the for loop

        Assert(s->data.loop.cond->type == expr_binary);
        Assert(s->data.loop.cond->data.binary.operator.type == tok_less);
        expr* bin = s->data.loop.cond;
        //CAN ONLY HANDLE + OPERATORS
        int firstRegisterPos = currentRegister;
        emitBytesExpression(bin->data.binary.left, scope);
        // LOAD ONTO REGISTER LOCALX
        int secondRegisterPos = currentRegister;
        emitBytesExpression(bin->data.binary.right, scope);
        printf("JEQ  $%d $%d           ;%d\n", firstRegisterPos, secondRegisterPos, (byteCodeCount)*4);
        byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-11d$%-6d#@@@;%-3d\\n\\\n", "JEQ",firstRegisterPos, secondRegisterPos, (byteCodeCount++)*4);

        block_link* link = s->data.loop.block->data.block.start;
        while(link){
            emitBytesStmt(link->s, scope);
            link = link->next;
        }
        Assert(s->data.loop.inc->type == expr_unary);
        Assert(s->data.loop.inc->data.unary.operator.type == tok_pplus);
        // __debugbreak();

        int reg = currentRegister++;
        
        byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-21d ;%-3d\\n\\\n","INC", firstRegisterPos, (byteCodeCount++)*4);
        
        int incStackPos = 0;
        char* varStr = varStorage + bin->data.binary.left->data.variable.varOffset;
        localsHashEntry* entry = getLocalsHashTable(varStr, scope);
        if(!entry){
            printf("BYTECODE EMISSION ERROR! %s HAS NO VALID HASHTABLE ENTRY!\n", varStr);
            runtimeError++;
            return;
        }
        
        byteStrCount += sprintf(byteStr + byteStrCount, "%-8s[$30 + %-2d]   $%-10d;%-3d\\n\\\n","LOAD", localCount*4, firstRegisterPos, (byteCodeCount++)*4);

        byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-11d#%-10d;%-3d\\n\\\n","LOAD", reg, (byteCodeCount*4) - loopStartInstruction, (byteCodeCount)*4);
        byteCodeCount++;
        byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-21d ;%-3d\\n\\\n","JMPB", reg, (byteCodeCount++)*4);
        Assert(backpcount < 64);
        printf("BACK PATCH %d AT SLOT %d\n", (byteCodeCount)*4, backpcount);
        backps[backpcount++] = (byteCodeCount)*4;

    
        // switch(e->data.binary.operator.type){
            // case tok_plus:{
                // LOAD ONTO REGISTER LOCALY
                // printf("ADD $%d $%d\n", firstRegisterPos, secondRegisterPos);
                // byteStrCount += sprintf(byteStr + byteStrCount, "%-9s$%-10d $%-8d  ;%-3d\\n\\\n","ADD", firstRegisterPos, secondRegisterPos, (byteCodeCount++)*4);
                // TODO: constant folding if all expressions are literals/primaries
            // }break;
            // case tok_less:{
            // }break;
            // default:{
                // Assert("UNHANDLED TOKEN TYPE IN EmitBytesBinary" && 0);
            // }break;
        // }

    }


    emitBytesExpression(s->data.loop.inc, scope);
 
    // __debugbreak();
    
}



void emitBytesStmt(stmt* s, int scope){
        switch(s->type){
        case stmt_decl:{
            emitBytesDeclStmt(s, scope);
        }break;
        case stmt_expr:{
            emitBytesExpression(s->data.expr.e, scope);
        }break;
        case stmt_block:{
            emitBytesBlock(s, scope+1);
        }
        case stmt_forloop:{
            emitBytesForLoop(s, scope+1);
        }
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
    int loweroffset = 'a' - 'A';
    for(int i = 0; i < size; ++i){
        char c = (*(str + i));
        if(isAlpha(c)){
            if(c >= 'a'){//lowercase
            }else{//uppercase
                c += loweroffset;
            }
        }

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
            if(str[1] == 'f'){
                    type = tok_if;
            }else{
                if(keywordMatch((str + 1), "nt", 2)){
                    type = tok_int;
                }
            }
        }break;
        case 'b':{//bool
            if(keywordMatch((str + 1), "ool", 3)){
                type = tok_bool;
            }
        }break;
        case 'c':{//char
            if(keywordMatch((str + 1), "har", 3)){
                type = tok_char;
            }
        }break;
        case 't':{//true
            if(keywordMatch((str + 1), "rue", 3)){
                type = tok_true;
            }
        }break;
        case 'f':{
            if(str[1] == 'o'){
                if(str[2] == 'r'){
                    type = tok_for;
                }
            }else if(str[1] == 'a'){
                if(keywordMatch((str + 1), "alse", 4)){
                    type = tok_false;
                }
            }

        }break;
        case 'e':{//else / elseif
            if(keywordMatch((str + 1), "lse if", 6)){
                type = tok_elseif;
            }else{
                if(keywordMatch((str + 1), "lse", 3)){
                    type = tok_else;
                }
            }
        }break;
        case 'w':{//while
            if(keywordMatch((str + 1), "hile", 4)){
                type = tok_while;
            }
        }break;

    }
        

    return type;
}




token_type isVMKeyword(char* str){
    char c = (*str);
    token_type type = tok_none;
    int loweroffset = 'a' - 'A';
    if(c >= 'a'){//lowercase
    }else{//uppercase
        c += loweroffset;
    }
    switch(c){//trie
        case 'm':{//ov
            if     (keywordMatch((str + 1), "ov32", 4))type = tok_MOV32;
            else if(keywordMatch((str + 1), "ov16", 4))type = tok_MOV16;
            else if(keywordMatch((str + 1), "ov8", 3))type = tok_MOV8;
            else if(keywordMatch((str + 1), "ov", 2))type = tok_MOV;
            else if(keywordMatch((str + 1), "ul", 2))type = tok_MUL;
        }break;
        case 's':{if(keywordMatch((str + 1), "ub", 2))type = tok_SUB;}break;
        case 'a':{if(keywordMatch((str + 1), "dd", 2))type = tok_ADD;}break;
        case 'd':{
                 if(keywordMatch((str + 1), "iv", 2))type = tok_DIV;
            else if(keywordMatch((str + 1), "ec", 2))type = tok_DEC;
        }break;
        case 'e':{if(keywordMatch((str + 1), "q", 1))type = tok_EQ;}break;
        case 'n':{if(keywordMatch((str + 1), "e", 1))type = tok_NE;}break;
        case 'l':{
            if      (keywordMatch((str + 1), "te", 2))type = tok_LTE;
            else if (keywordMatch((str + 1), "t", 1))type = tok_LT;
        }break;
        case 'g':{
            if      (keywordMatch((str + 1), "te", 2))type = tok_GTE;
            else if (keywordMatch((str + 1), "t", 1))type = tok_GT;
        }break;
        case 'j':{
            if      (keywordMatch((str + 1), "mp", 2))type = tok_JMP;
            else if (keywordMatch((str + 1), "eq", 2))type = tok_JEQ;
            else if (keywordMatch((str + 1), "ne", 2))type = tok_JNE;
        }break;
        case 'i':{if(keywordMatch((str + 1), "nc", 2))type = tok_INC;}break;

        default:{}break;
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

void vmAddToken(token t){
    t.line = vmParserLine;
    vmTokens[vmTokenCount++] = t;
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

                    case tok_if         :{i += 2; t.type = tok_if         ;}break;
                    case tok_else       :{i += 4; t.type = tok_else       ;}break;
                    case tok_elseif     :{i += 6; t.type = tok_elseif     ;}break;
                    case tok_for        :{i += 3; t.type = tok_for        ;}break;
                    case tok_while      :{i += 5; t.type = tok_while      ;}break;
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
                case '+':{
                    token t = {}; 
                    if(input[i+1] == '+'){
                        t.type = tok_pplus;
                        i++;
                    }else if(input[i+1] == '='){
                        t.type = tok_plusequal;
                        i++;
                    }else{
                        t.type = tok_plus;          
                    }
                    addToken(t);
                    i++;
                }break;
                case '-':{
                    token t = {}; 
                    if(input[i+1] == '-'){
                        t.type = tok_mminus;
                        i++;
                    }else if(input[i+1] == '='){
                        t.type = tok_minequal;
                        i++;
                    }else{
                        t.type = tok_minus;         
                    }
                    addToken(t);
                    i++;
                }break;
                case '/':{token t = {}; t.type = tok_slash;         addToken(t); i++;}break;
                case '*':{token t = {}; t.type = tok_star;          addToken(t); i++;}break;
                case ';':{token t = {}; t.type = tok_semicolon;     addToken(t); i++;}break;
                case '(':{token t = {}; t.type = tok_lparen;        addToken(t); i++;}break;
                case ')':{token t = {}; t.type = tok_rparen;        addToken(t); i++;}break;
                case '{':{token t = {}; t.type = tok_lcurly;        addToken(t); i++;}break;
                case '}':{token t = {}; t.type = tok_rcurly;        addToken(t); i++;}break;
                case '[':{token t = {}; t.type = tok_lbrack;        addToken(t); i++;}break;
                case ']':{token t = {}; t.type = tok_rbrack;        addToken(t); i++;}break;
                case '!':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_nequal;
                        i++;
                    }else{
                        t.type = tok_bang;
                    }
                    addToken(t); 
                    i++;
                }break;
                case '<':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_lequal;
                        i++;
                    }else{
                        t.type = tok_less;
                    }
                    addToken(t); 
                    i++;
                }break;
                case '>':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_grequal;
                        i++;
                    }else{
                        t.type = tok_gr;
                    }
                    addToken(t); 
                    i++;
                }break;
                case '=':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_eequal;
                        i++;
                    }else{
                        t.type = tok_equal;
                    }
                    addToken(t); 
                    i++;
                }break;
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


void printTokens(token* tokens, int tokenCount){
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
            case tok_if         :{printf("tok_if         %3d\n", i);}break;
            case tok_else       :{printf("tok_else       %3d\n", i);}break;
            case tok_elseif     :{printf("tok_elseif     %3d\n", i);}break;
            case tok_for        :{printf("tok_for        %3d\n", i);}break;
            case tok_while      :{printf("tok_while      %3d\n", i);}break;
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
            case tok_plusequal  :{printf("tok_plusequal  %3d\n", i);}break;
            case tok_minequal   :{printf("tok_minequal   %3d\n", i);}break;
            case tok_lbrack     :{printf("tok_lbrack     %3d\n", i);}break;
            case tok_rbrack     :{printf("tok_rbrack     %3d\n", i);}break;
            case tok_MOV        :{printf("tok_MOV        %3d\n", i);}break;
            case tok_MOV32      :{printf("tok_MOV32      %3d\n", i);}break;
            case tok_MOV16      :{printf("tok_MOV16      %3d\n", i);}break;
            case tok_MOV8       :{printf("tok_MOV8       %3d\n", i);}break;
            case tok_ADD        :{printf("tok_ADD        %3d\n", i);}break;
            case tok_SUB        :{printf("tok_SUB        %3d\n", i);}break;
            case tok_DIV        :{printf("tok_DIV        %3d\n", i);}break;
            case tok_MUL        :{printf("tok_MUL        %3d\n", i);}break;
            case tok_EQ         :{printf("tok_EQ         %3d\n", i);}break;
            case tok_NE         :{printf("tok_NE         %3d\n", i);}break;
            case tok_LT         :{printf("tok_LT         %3d\n", i);}break;
            case tok_GT         :{printf("tok_GT         %3d\n", i);}break;
            case tok_LTE        :{printf("tok_LTE        %3d\n", i);}break;
            case tok_GTE        :{printf("tok_GTE        %3d\n", i);}break;
            case tok_JMP        :{printf("tok_JMP        %3d\n", i);}break;
            case tok_JEQ        :{printf("tok_JEQ        %3d\n", i);}break;
            case tok_JNE        :{printf("tok_JNE        %3d\n", i);}break;
            case tok_INC        :{printf("tok_INC        %3d\n", i);}break;
            case tok_DEC        :{printf("tok_DEC        %3d\n", i);}break;
            case tok_REG        :{printf("tok_REG        %3d\n", i);}break;
            case tok_octothorpe:{printf("tok_octothorpe %3d\n", i);}break;

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








void vmParser(char* input){
    int i = 0;
    char c = input[i];
    while(input[i] != '\0'){
        // printf("%c | isAlpha : %d, isNum : %d, isWhitespace %d\n", c, isAlpha(c), isNum(c), isWhitespace(c));
        
        if(isNum(c)){//TODO add floats, only handles integers
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
            vmAddToken(t);
        }
        else if(isAlpha(c)){
            token_type type = isVMKeyword(input + i);
            if(type){
                printf("ASM KEYWORD FOUND | %s\n", tokStr(type));
                token t = {};
                switch(type){
                    case tok_MOV        :{i += 3; t.type = tok_MOV        ;}break;
                    case tok_MOV32      :{i += 5; t.type = tok_MOV32      ;}break;
                    case tok_MOV16      :{i += 5; t.type = tok_MOV16      ;}break;
                    case tok_MOV8       :{i += 4; t.type = tok_MOV8       ;}break;
                    case tok_ADD        :{i += 3; t.type = tok_ADD        ;}break;
                    case tok_SUB        :{i += 3; t.type = tok_SUB        ;}break;
                    case tok_DIV        :{i += 3; t.type = tok_DIV        ;}break;
                    case tok_MUL        :{i += 3; t.type = tok_MUL        ;}break;
                    case tok_EQ         :{i += 2; t.type = tok_EQ         ;}break;
                    case tok_NE         :{i += 2; t.type = tok_NE         ;}break;
                    case tok_LT         :{i += 2; t.type = tok_LT         ;}break;
                    case tok_GT         :{i += 2; t.type = tok_GT         ;}break;
                    case tok_LTE        :{i += 3; t.type = tok_LTE        ;}break;
                    case tok_GTE        :{i += 3; t.type = tok_GTE        ;}break;
                    case tok_JMP        :{i += 3; t.type = tok_JMP        ;}break;
                    case tok_JEQ        :{i += 3; t.type = tok_JEQ        ;}break;
                    case tok_JNE        :{i += 3; t.type = tok_JNE        ;}break;
                    case tok_INC        :{i += 3; t.type = tok_INC        ;}break;
                    case tok_DEC        :{i += 3; t.type = tok_DEC        ;}break;
                    default:{
                        printf("ERROR UNHANDLED ASM TOKEN!\n"); 
                        parsingError++;
                    }break;
                }
                vmAddToken(t);
            }else{
                //catches all alphanumeric non keywords, treating them as variables
                if(c == 'r' && isNum(input[i+1])){//PROBABLY a register
                    //adds the register token
                    token t = {};
                    t.type = tok_REG;
                    vmAddToken(t);

                    //now extract the number
                    //extract number from register
                    c = input[++i];
                    t.type = tok_num;
                    int val = 0;
                    while(isNum(c)){
                        val *= 10;
                        val += c - '0';
                        c = input[++i];
                    }
                    t.data.integer = val;
                    vmAddToken(t);
                }else{//anything that isnt r(num)
                    int start = vmVarStorageOffset;
                    while(isAlpha(c) || isNum(c)){
                        //TODO:
                        //either need to switch to a hash table
                        //or store start and length to lookup in the original code text
                        vmVarStorage[vmVarStorageOffset++] = c;
                        c = input[++i];
                    }
                    vmVarStorageOffset++;
                    token t = {};
                    t.type = tok_var;
                    t.data.varOffset = start;
                    printf("token var = %s\n", vmVarStorage + t.data.varOffset);
                    vmAddToken(t);
                }
                
            }
        }
        else if(!isWhitespace(c)){
            switch(c){
                case '$':{
                    token t = {};
                    t.type = tok_REG;
                    vmAddToken(t);
                    i++;
                }break;
                case '#':{
                    token t = {};
                    t.type = tok_octothorpe;
                    vmAddToken(t);
                    i++;
                }break;
                case '+':{
                    token t = {}; 
                    if(input[i+1] == '+'){
                        t.type = tok_pplus;
                        i++;
                    }else if(input[i+1] == '='){
                        t.type = tok_plusequal;
                        i++;
                    }else{
                        t.type = tok_plus;          
                    }
                    vmAddToken(t);
                    i++;
                }break;
                case '-':{
                    token t = {}; 
                    if(input[i+1] == '-'){
                        t.type = tok_mminus;
                        i++;
                    }else if(input[i+1] == '='){
                        t.type = tok_minequal;
                        i++;
                    }else{
                        t.type = tok_minus;         
                    }
                    vmAddToken(t);
                    i++;
                }break;
                case '/':{token t = {}; t.type = tok_slash;         vmAddToken(t); i++;}break;
                case '*':{token t = {}; t.type = tok_star;          vmAddToken(t); i++;}break;
                case ';':{token t = {}; t.type = tok_semicolon;     vmAddToken(t); i++;}break;
                case '(':{token t = {}; t.type = tok_lparen;        vmAddToken(t); i++;}break;
                case ')':{token t = {}; t.type = tok_rparen;        vmAddToken(t); i++;}break;
                case '{':{token t = {}; t.type = tok_lcurly;        vmAddToken(t); i++;}break;
                case '}':{token t = {}; t.type = tok_rcurly;        vmAddToken(t); i++;}break;
                case '[':{token t = {}; t.type = tok_lbrack;        vmAddToken(t); i++;}break;
                case ']':{token t = {}; t.type = tok_rbrack;        vmAddToken(t); i++;}break;
                case '!':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_nequal;
                        i++;
                    }else{
                        t.type = tok_bang;
                    }
                    vmAddToken(t); 
                    i++;
                }break;
                case '<':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_lequal;
                        i++;
                    }else{
                        t.type = tok_less;
                    }
                    vmAddToken(t); 
                    i++;
                }break;
                case '>':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_grequal;
                        i++;
                    }else{
                        t.type = tok_gr;
                    }
                    vmAddToken(t); 
                    i++;
                }break;
                case '=':{
                    token t = {}; 
                    if(input[i+1] == '='){
                        t.type = tok_eequal;
                        i++;
                    }else{
                        t.type = tok_equal;
                    }
                    vmAddToken(t); 
                    i++;
                }break;
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
                vmParserLine++;
                vmInputLines[vmParserLine] = input + i;
            }
        }
        else{
            i++;
            printf("ERROR: UNHANDLED CHAR: %c\n", c);
            vmParserLine++;
            return;
        }

        c = input[i];

    }
    token t = {};
    t.type = tok_eof;
    vmAddToken(t);
}

typedef enum {
    op_none,
    op_mov_reg_reg,
    op_mov_reg_imm, //MOV r0 #1
    op_add_reg_reg,
    op_add_reg_imm,
    op_sub_reg_reg,
    op_mul_reg_reg,
    op_div_reg_reg,
}op_type;

void runVM(){
    vm.ip = 0;
    while(vm.ip < vm.bytecodeCount){
        switch(vm.bytecode[vm.ip]){
            case op_mov_reg_reg:{}break;
            case op_mov_reg_imm:{
                u8 reg1 = vm.bytecode[vm.ip + 1];
                
                u16 imm = 0;
                //TODO: BUG, 0xFF ERASES THE SHIFTED VALUE! REMOVE & 0xFF! 
                imm += (vm.bytecode[vm.ip + 2] << 8) & 0xFF;
                imm += (vm.bytecode[vm.ip + 3] << 0) & 0xFF;
                
                vm.registers[reg1] = imm;

            }break;
            case op_add_reg_reg:{
                u8 reg1 = vm.bytecode[vm.ip + 1];
                u8 reg2 = vm.bytecode[vm.ip + 2];
                vm.registers[reg1] = vm.registers[reg1] + vm.registers[reg2]; 
            }break;
            case op_add_reg_imm:{}break;
            case op_sub_reg_reg:{}break;
            case op_mul_reg_reg:{}break;
            case op_div_reg_reg:{}break;
            default:{}break;
        }
        vm.ip += 4;
    }
}

void vmEmitBytecode(token* tokens, int tokenCount){
    vm.bytecodeCount = 0;
    //TODO: bake integer number (for registers/immediates) INTO the tok_reg/octothorpe token itself
    int i = 0;
    token top = {};
    token targ1 = {};
    token tnum1 = {};
    token targ2 = {};
    token tnum2 = {};

    while(i < tokenCount){
        //ASSUME THIS IS AN OPERATION TOKEN
        if(top.type != tok_none){
            op_type optype = op_none;

            // 0 - 255 | 0 - 255 | 0 - 255 | 0 - 255
            // ^^^
            //operation| arg1    | arg2    | arg3
            int startBytecode = vm.bytecodeCount;
            vm.bytecodeCount += 4;
            if(top.type == tok_MOV){
                if(targ1.type == tok_REG){
                    if(targ2.type == tok_REG){
                        optype = op_mov_reg_reg;
                    }else if(targ2.type == tok_octothorpe){
                        optype = op_mov_reg_imm;
                    }
                }
            }
            if(top.type == tok_ADD){
                if(targ1.type == tok_REG){
                    if(targ2.type == tok_REG){
                        optype = op_add_reg_reg;
                    }else if(targ2.type == tok_octothorpe){
                        optype = op_add_reg_imm;
                    }
                }
            }
            //TODO: check if the register number is valid (0 <= reg < MAX_REGISTERS)
            vm.bytecode[startBytecode+0] = optype;
            vm.bytecode[startBytecode+1] = tnum1.data.integer;//register number
            if(targ2.type == tok_REG){
                vm.bytecode[startBytecode+2] = tnum2.data.integer; //register number
            }else{
                //0 - 255
                //u16 = 65536
                //TODO: check if its larger than a u16
                vm.bytecode[startBytecode+2] = (tnum2.data.integer >> 8) & 255;
                vm.bytecode[startBytecode+3] = (tnum2.data.integer) & 255;
            }

        }
        if(tokens[i].type == tok_eof)break;
        top = tokens[i++];

        targ1 = tokens[i];
        if(targ1.type >= tok_MOV){//error, expected arg after initial operator
            processingError++;
            printVMLine(top.line);
            printf("ASM ERROR: EXPECTED ARGS AFTER OPERATOR\n");
            break;
        }else{
            i++;
            switch(targ1.type){
                case tok_num        :{

                }break;
                case tok_REG        :{
                    tnum1 = tokens[i++];
                }break;
                case tok_octothorpe :{
                    tnum1 = tokens[i++];
                }break;
            }
        }

        targ2 = tokens[i];
        if(targ2.type >= tok_MOV){//error, expected arg after initial operator
            // processingError++;
            // printVMLine(top.line);
            // printf("ASM ERROR: EXPECTED ARGS AFTER OPERATOR\n");
            // break;
            continue;
        }else{
            i++;
            switch(targ2.type){
                case tok_num        :{
                
                }break;
                case tok_REG        :{
                    tnum2 = tokens[i++];
                    if(tnum2.type != tok_num){
                    }
                }break;
                case tok_octothorpe :{
                    tnum2 = tokens[i++];
                    if(tnum2.type != tok_num){
                    }
                }break;
            }
        }


    }
}


int main(){

    //fibonacci
    //0  1  2  3  4  5  6  7   8 , 9
    //0, 1, 1, 2, 3, 5, 8, 13, 21, 34

    // int y = 0;
    // int x = 1;
    // int fib = 8;
    // for(int i = 0; i < fib-1; ++i){
    //     int prev = x;
    //     x = x + y;
    //     y = prev;
    // }
    // printf("%d\n", x);
    // __debugbreak();
    char asminput[2048] = 
    "MOV r0 #1\n"
    "MOV r1 #2\n"
    "ADD r0 r1\n"
    "ADD r1 r0\n"
    // "MOV $0 $1\n" //same thing
    // "ADD $0 $1\n"
    // "SUB $2 $3\n"
    // "DIV $4 $5\n"
    // "MUL $6 $7\n" // ;^)))
    // "MOV32 $6 $7\n"
    // "MOV16 $8 $9\n"
    // "MOV8 $10 $11\n"
    // "INC $12\n"
    // "DEC $13\n"
    // "EQ  r14 r15\n"
    // "NE  r16 r17\n"
    // "LT  r18 r19\n"
    // "GT  r20 r21\n"
    // "LTE r22 r23\n"
    // "GTE $24 r25\n"
    // "JMP #42\n"
    // "JEQ #69\n"
    // "JNE #67\n"
    ;
    vmInputLines[1] = asminput;
    vmParser(asminput);
    printTokens(vmTokens, vmTokenCount);
    vmEmitBytecode(vmTokens, vmTokenCount);
    runVM();

    Assert(vm.registers[0] == 3);
    Assert(vm.registers[1] == 5);
    return 0;

    #define LOOP 0
    #if LOOP
    while(1){
    #endif
        objectCount = 0;
        exprCount = 0;
        stmtCount = 0;
        blockLinkCount = 0;
        globalStmtCount = 0;
        tokenCount = 0;
        processingError = 0;
        runtimeError = 0;
        parsingError = 0;
        astStrCount = 0;
        errors = 0;
        memset(varStorage, 0, 2048);
        #if LOOP
            fputs("langtest>", stdout);
            fgets(input, 2048, stdin);
        #else
            #if 0
            char testStr[2048] =
                "int x = 1;\n"
                "int y = 2;\n"
                "int z = 0;\n"
                "z = x + y;\n"
            ;
            #else
            char testStr[2048] =
                "int y = 0;\n"
                "int x = 1;\n"
                "for(int i = 0; i < 7; ++i){\n"
                "int temp = x;\n"
                "x = x + y;\n"
                "y = temp;\n"
                "};\n"
                // "{\n"
                // "int x = 2;\n"
                // "}\n"
            ;
        int test = 0;
            #endif
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
        printTokens(tokens, tokenCount);

        nextToken = 0;
        processingError = 0;
        while(tokens[nextToken].type != tok_eof){
            globalStmts[globalStmtCount++] = statement();

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


        for(int i = 0; i < globalStmtCount; ++i){
            astStrCount = 0;
            stmt* s = globalStmts[i];
            printStmt(s, 0);
            printf("%s\n", astStr);
        }

        #if 1
            //emit bytecode
            byteStrCount = 0;
            for(int i = 0; i < globalStmtCount; ++i){
                stmt* s = globalStmts[i];
                emitBytesStmt(s, 0);
            }            
            printf("COMPILED BYTECODE:\n");
            printf("LOAD     $0          #%-10d;%-3u\\n\\\n", localCount*4, (startByteCodeCount++)*4);
            printf("SUB      $31         $0         ;%-3u\\n\\\n", (startByteCodeCount++)*4);
            printf("LOAD     $30         $31        ;%-3u\\n\\\n", (startByteCodeCount++)*4);//FP = SP

            //emberassing brittle string backpatching
            int charc = 0;
            int backpconsumed = 0;
            char curbackp[32] = {};
            char flipped[32] = {};
            while(byteStr[charc] != 0){
                if(byteStr[charc] == '@'){//assumption is there are 3 @s in a row, @@@ is the backpatch location
                    for(int ii = 0; ii < 32; ii++)curbackp[ii] = 0;
                    int backpval = backps[backpconsumed++];
                    int copy = backpval;
                    int count = 0;
                    while(copy > 0){//generate backwards number string
                        int integer = copy % 10;
                        copy /= 10;
                        curbackp[count] = integer + '0';
                        count++;
                    }
                    int flippedi = 0;   
                    for(int i = count-1; i >= 0; --i){//flip the backwards string
                        flipped[flippedi] = curbackp[i];
                        flippedi++;
                    }

                    for(int i = 0; i < count; ++i){
                        byteStr[charc + i] = flipped[i];
                    }
                    //eliminate backpatching symbols
                    if(byteStr[charc + 1] == '@')byteStr[charc + 1] = ' ';
                    if(byteStr[charc + 2] == '@')byteStr[charc + 2] = ' ';

                }
                charc++;
            }

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



//LONGTERM PLAN:
//reduce hardcoded assumptions in looping
//add if/else statements
//add functions



/*
    CORRECT COMPILED BYTECODE (computes 8th fibonacci number)
        LOAD     $0          #16        ;0  \n\
        SUB      $31         $0         ;4  \n\
        LOAD     $30         $31        ;8  \n\
        LOAD     $0          #0         ;12 \n\
        LOAD    [$30 + 4 ]   $0         ;16 \n\
        LOAD     $0          #1         ;20 \n\
        LOAD    [$30 + 8 ]   $0         ;24 \n\
        LOAD     $0          #0         ;28 \n\
        LOAD    [$30 + 12]   $0         ;32 \n\
        LOAD     $0         [$30 + 12]  ;36 \n\
        LOAD     $1          #7         ;40 \n\
        JEQ      $0          $1     #96 ;44 \n\
        LOAD     $2         [$30 + 8 ]  ;48 \n\
        LOAD    [$30 + 16]   $2         ;52 \n\
        LOAD     $2         [$30 + 8 ]  ;56 \n\
        LOAD     $3         [$30 + 4 ]  ;60 \n\
        ADD      $2          $3         ;64 \n\
        LOAD    [$30 + 8 ]   $2         ;68 \n\
        LOAD     $4         [$30 + 16]  ;72 \n\
        LOAD    [$30 + 4 ]   $4         ;76 \n\
        INC      $0                     ;80 \n\
        LOAD    [$30 + 16]   $0         ;84 \n\
        LOAD     $5          #52        ;88 \n\
        JMPB     $5                     ;92 \n\
    */

//TODO TO VM:
//print every parsed line for easier debugging
//print every generated opcode for every line
//add direct constant loading to local indices on the stack (for local variables)




/*
brainstorming for while/for loops (day 5)

*/

/*
//braintstorming for how to handle if/else statements eventually, not day 5
elseiflink{
    next
    stmt* conditional
    stmt* block
} 
elseiflink* link ->next elseif statement
elseifCount 
ifstmt{
    stmt* if
    stmt* else
    int count;
    elseiflink start
};

if(){

}
else if(){ 
else if(){
else if(){
    stmt1 blocklink1
    stmt2 blocklink2
}
else{
}
*/