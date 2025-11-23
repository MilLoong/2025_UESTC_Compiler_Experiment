#include <stddef.h>
#include "node_type.h"

enum yytokentype {
	num_INT = 258,
	num_FLOAT = 259,

	Y_ID = 260,

	Y_INT = 261,
	Y_VOID = 262,
	Y_CONST = 263,
	Y_IF = 264,
	Y_ELSE = 265,
	Y_WHILE = 266,
	Y_BREAK = 267,
	Y_CONTINUE = 268,
	Y_RETURN = 269,

	Y_ADD = 270,
	Y_SUB = 271,
	Y_MUL = 272,
	Y_DIV = 273,
	Y_MODULO = 274,
	Y_LESS = 275,
	Y_LESSEQ = 276,
	Y_GREAT = 277,
	Y_GREATEQ = 278,
	Y_NOTEQ = 279,
	Y_EQ = 280,
	Y_NOT = 281,
	Y_AND = 282,
	Y_OR = 283,
	Y_ASSIGN = 284,

	Y_LPAR = 285,
	Y_RPAR = 286,
	Y_LBRACKET = 287,
	Y_RBRACKET = 288,
	Y_LSQUARE = 289,
	Y_RSQUARE = 290,
	Y_COMMA = 291,
	Y_SEMICOLON = 292,

	Y_FLOAT = 293
};

typedef struct _TokenType{
	enum yytokentype token;
	union {
		int		ivalue;
		float   fvalue;
		char*	svalue;
	}attr;
}TokenType;


void set_cur_tok_index(int ind);
int get_cur_tok_index();
TokenType advance();
extern TokenType cur_token;


///Non-terminator
enum Non_terminator
{
	_UNKNOWN
    //TODO
};

typedef struct _ast ast;
typedef struct _ast *past;

struct _ast{
	int ivalue;
	float fvalue;
	char* svalue;
	node_type nodeType;
	past left;
	past right;
	past if_cond;
	past next;
};

past rd_block();
past rd_array_subscripts();

past newAstNode();
past newID(char* value);
past newInt(int value);

past rd_call_paras();
past rd_relexp();
past rd_stmt();
