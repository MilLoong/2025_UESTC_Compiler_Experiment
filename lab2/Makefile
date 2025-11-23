CC = gcc
CFLAGS = -g -Wall -I./lex

TARGET = parser

LEX_DIR = lex
LEX_SRC = $(LEX_DIR)/lexer.l
LEX_C   = $(LEX_DIR)/lex.yy.c
LEX_OBJ = $(LEX_DIR)/lex.yy.o
TOKEN_H = $(LEX_DIR)/token.h

SRCS = $(LEX_C) rd.c main.c
OBJS = $(LEX_OBJ) rd.o main.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(LEX_C): $(LEX_SRC) $(TOKEN_H)
	flex -o $(LEX_C) $(LEX_SRC)

$(LEX_OBJ): $(LEX_C) $(TOKEN_H) rdlab2.h
	$(CC) $(CFLAGS) -c $(LEX_C) -o $(LEX_OBJ)

rd.o: rd.c rdlab2.h node_type.h
	$(CC) $(CFLAGS) -c rd.c

main.o: main.c rdlab2.h $(TOKEN_H)
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f $(TARGET) *.o $(LEX_DIR)/*.o $(LEX_C)

.PHONY: all clean