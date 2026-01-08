/*
    DAY 1
    https://www.youtube.com/watch?v=6uhwyq2Ndt4
    
    Started from scratch and wrote a simple text parser (with bugs)
    Wrote a very broken evaluation loop
    in Day 2 we demonstrate how to take it further
*/

#include <stdio.h>
#include <stdlib.h>

#define u32 unsigned long int
#define s32 long int

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
    expr_value,
}expr_type;

typedef struct value_expr{
    token t;
    int varOffset;
    union{
        int integer;
    }data;
}value_expr;

typedef struct expr{
    expr_type type;
    union{
        value_expr value;
    }data;
}expr;


typedef struct hashEntry{
    u32 hash;
    char* str;
}hashEntry;

#define HASH_BUCKETS 4
#define HASH_TABLE_SIZE 2048

char input[2048];
token tokens[2048];
expr exprs[2048];
char varStorage[2048];
hashEntry hashTable[HASH_TABLE_SIZE];
expr* program[2048];
int programCount;

int varStorageOffset = 0;
int parseError = 0;
int tokenCount = 0;
int exprCount = 0;

int cursor = 0;
int processingError = 0;

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



expr* makeValueExpr(){
    expr* e = exprs + exprCount++;
    e->type = expr_value;
    return e;
}

void unaryOp(){
}

int binaryOp(int value, int curToken){
    token t = tokens[curToken];
    if(t.type == tok_num){
        value += t.data.integer;
        ++curToken;
        if(tokens[curToken].type == tok_plus){
            ++curToken;
            value = binaryOp(value, curToken);
        }
    }else{
        processingError++;
        printf("ERROR EXPECTED A NUMBER TOKEN AFTER +\n");
        return 0;
    }
    return value;
}

void constructAST(){
    int curToken = 0;
    token t = tokens[curToken];
    while(t.type != tok_eof && t.type != tok_none){
        if(t.type == tok_int){
            PDEBUG("TOKEN INTEGER FOUND AT %d, SCAN FORWARD FOR VARIABLE NAME!\n", curToken);
            if(peekToken(curToken) == tok_var){
                expr* e = makeValueExpr();
                e->data.value.varOffset = tokens[++curToken].data.varOffset;
                PDEBUG("ASSIGNED %s VARIABLE NAME TO INT\n", varStorage + e->data.value.varOffset);
                if(peekToken(curToken) == tok_equal){
                    ++curToken;
                        if(peekToken(curToken) == tok_num){
                            e->data.value.t = tokens[++curToken];
                            int value = e->data.value.t.data.integer;
                            if(peekToken(curToken) == tok_plus){
                                ++curToken;
                                ++curToken;
                                value = binaryOp(value, curToken);
                            }
                            e->data.value.data.integer = value;
                            PDEBUG("ASSIGNED %d TO INT %s = %d\n",value, varStorage + e->data.value.varOffset, value);
                            pushToHashTable(varStorage + e->data.value.varOffset);
                        }else{
                            printf("ERROR EXPECTED NUMBER TOKEN AFTER EQUALS TOKEN FOR int %s!\n", varStorage + e->data.value.varOffset);
                            processingError++;
                            return;
                        }
                }else{
                    printf("EXPECTED ASSIGNMENT OPERATOR FOLLOWING: int %s!\n", varStorage + e->data.value.varOffset);
                    processingError++;
                    return;
                }
            }else{
                printf("ERROR: EXPECTED A VARIABLE NAME FOLLOWING INT TYPE!\n");
                processingError++;
                return;
            }
        }
        t = tokens[++curToken];
    }
}


const char* tokStr(token_type t){
    switch(t){
        case tok_none       :{return "tok_none";}break;
        case tok_int        :{return "tok_int";}break;
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
        default:{}return "";
    }
    return "";
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
                parseError++;
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
                case '+':{token t = {}; t.type = tok_plus;          tokens[tokenCount++] = t;}break;
                case '-':{token t = {}; t.type = tok_minus;         tokens[tokenCount++] = t;}break;
                case '/':{token t = {}; t.type = tok_slash;         tokens[tokenCount++] = t;}break;
                case '*':{token t = {}; t.type = tok_star;          tokens[tokenCount++] = t;}break;
                case ';':{token t = {}; t.type = tok_semicolon;     tokens[tokenCount++] = t;}break;
                case '(':{token t = {}; t.type = tok_lparen;        tokens[tokenCount++] = t;}break;
                case ')':{token t = {}; t.type = tok_rparen;        tokens[tokenCount++] = t;}break;
                case '{':{token t = {}; t.type = tok_lcurly;        tokens[tokenCount++] = t;}break;
                case '}':{token t = {}; t.type = tok_rcurly;        tokens[tokenCount++] = t;}break;
                case '=':{token t = {}; t.type = tok_equal;         tokens[tokenCount++] = t;}break;
                
            }

        }

        c = input[++i];

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
            case tok_eof        :{printf("tok_eof        %3d\n", i);}break;
            default:{}break;
        }
    }
}

int main(){
    memset(varStorage, 0, 2048);
    fputs("langtest>", stdout);
    fgets(input, 2048, stdin);
    
    
    parser(input);
    if(parseError){
        printf("ERROR IN THE CODE! NOT EVALUATING\n");
    }
    printTokens();

    constructAST();
}