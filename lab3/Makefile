CC = gcc
CFLAGS = -g -Wall -I.
BISON = bison
FLEX = flex

YACC_SRC = lrparser.y
LEX_SRC = lrlex.l
C_SRCS = ast.c

YACC_OUT_C = lrparser.tab.c
YACC_OUT_H = lrparser.tab.h
LEX_OUT_C = lex.yy.c

TARGET = ast

all: $(TARGET)

$(TARGET): $(C_SRCS:.c=.o) $(YACC_OUT_C:.c=.o) $(LEX_OUT_C:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

$(YACC_OUT_C) $(YACC_OUT_H): $(YACC_SRC)
	$(BISON) -d -o $(YACC_OUT_C) $(YACC_SRC)

$(LEX_OUT_C): $(LEX_SRC) $(YACC_OUT_H)
	$(FLEX) -o $(LEX_OUT_C) $(LEX_SRC)

ast.o: ast.c ast.h $(YACC_OUT_H)
	$(CC) $(CFLAGS) -c ast.c

clean:
	rm -f $(TARGET) *.o $(YACC_OUT_C) $(YACC_OUT_H) $(LEX_OUT_C)

.PHONY: all clean