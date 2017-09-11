all:
	lex asn1.l
	yacc -v -d asn1.y
	cc lex.yy.c y.tab.c -o asn1 -ll
