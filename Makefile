all:
	lex lexer.l
	yacc -d parser.y
	gcc -g lex.yy.c y.tab.c decoder.c -o decoder -ll

clean:
	rm -f asn1 lex.yy.c y.output y.tab.c y.tab.h decoder
