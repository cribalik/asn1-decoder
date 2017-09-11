all:
	lex lexer.l
	yacc -d parser.y
	cc lex.yy.c y.tab.c -o asn1 -ll

clean:
	rm -f asn1 lex.yy.c y.output y.tab.c y.tab.h
