#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

TokenType cur_token;
extern int yylex();
extern _YYLVAL yylval;
extern char* yytext;
extern FILE* yyin;

void set_cur_tok_index(int ind) { }
int get_cur_tok_index() { return 0; }

TokenType advance() {
    int token_type = yylex();
    cur_token.token = token_type;
    
    if (token_type == num_INT) {
        cur_token.attr.ivalue = yylval.int_value;
    } else if (token_type == num_FLOAT) {
        cur_token.attr.fvalue = yylval.float_value;
    } else if (token_type == Y_ID) {
        cur_token.attr.svalue = yylval.id_name;
    }
    
    if(token_type != 0) {
        printf("Token: %d, Text: %s\n", token_type, yytext ? yytext : "");
    }
    
    return cur_token;
}

past newAstNode() {
    past node = (past)malloc(sizeof(ast));
    if (node == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    memset(node, 0, sizeof(ast));
    return node;
}

past newInt(int value) {
    past node = newAstNode();
    node->nodeType = INTEGER_LITERAL;
    node->ivalue = value;
    return node;
}

past rd_block() {
    return rd_stmt(); 
}

past rd_array_subscripts() {
    return NULL;
}

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror(argv[1]);
            return 1;
        }
    }

    advance();

    printf("Start parsing...\n");
    past result = rd_stmt();
    
    if (result) {
        printf("Parsing successful! Root type: %d\n", result->nodeType);
    } else {
        printf("Parsing failed or empty.\n");
    }

    return 0;
}

int yywrap() {
    return 1;
}