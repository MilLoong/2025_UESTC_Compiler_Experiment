lexer: lex.yy.c lexer_main.c
	gcc lex.yy.c lexer_main.c -o lexer -lfl

lex.yy.c: lexer.l
	flex lexer.l

clean:
	rm -f lexer lex.yy.c *.o