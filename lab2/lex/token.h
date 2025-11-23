#ifndef TOKEN_H
#define TOKEN_H

#include "../rdlab2.h"

typedef union {
    int int_value;
    float float_value;
    char* id_name;
} _YYLVAL;

extern _YYLVAL yylval;

#endif