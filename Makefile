all: parser linux

parser:
	lex lexer.l
	yacc -d parser.y

linux:
	gcc -Wall -Wno-unused-function -g lex.yy.c y.tab.c decoder.c -o decoder -lncurses

clean:
	rm -f asn1 lex.yy.c y.output y.tab.c y.tab.h decoder decoder.exe

windows: parser
	i686-w64-mingw32-gcc -Wall -g -Wno-unused-function lex.yy.c y.tab.c decoder.c -o decoder.exe -lncurses

