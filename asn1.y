%{
#include <stdio.h>
#include <string.h>

int yyparse(void);
int yylex(void);
extern int yylineno;

void yyerror(const char *str) {
	fprintf(stderr, "error %i: %s\n", yylineno, str);
}

int yywrap(void) {
	return 1;
}


int main(void) {
	yyparse();
}
%}

%token ENUMERATED SIZE BIT_STRING BOOLEAN OCTET_STRING INTEGER DOUBLEDOT TRIPLEDOT TAGS BEGIN_ END_ DEFINITIONS IMPLICIT CHOICE SEQUENCE OF OPTIONAL NAME ASSIGNMENT NUMBER

%union
{
	int number;
	char *string;
}

%token <number> NUMBER
%token <string> NAME
%type <string> definition

%%
commands: | commands command {printf("Found a command\n");} ;

command:
	NAME DEFINITIONS cmdflags ASSIGNMENT BEGIN_ definitions END_ ;

cmdflags: | cmdflags cmdflag ;

cmdflag: IMPLICIT | TAGS ;

definitions: | definitions definition
	{printf("Found definition '%s'\n", $2);} ;

definition:
	NAME ASSIGNMENT type |
	NAME INTEGER ASSIGNMENT NUMBER ;
type:
	NAME |
	NAME sizeinfo |
	CHOICE '{' tags '}' |
	SEQUENCE '{' tags '}' |
	SEQUENCE sizeinfo OF NAME |
	SEQUENCE OF NAME |
	BOOLEAN |
	OCTET_STRING |
	OCTET_STRING sizeinfo |
	BIT_STRING |
  BIT_STRING enumdecl |
	BIT_STRING enumdecl sizeinfo |
	BIT_STRING sizeinfo |
	INTEGER |
	INTEGER enumdecl |
	INTEGER range |
	ENUMERATED enumdecl ;

tags:
	tags ',' tag |
	tag ;
tag:
   NAME '[' NUMBER ']' type tagflags |
   NAME type tagflags ;

tagflags: | tagflags tagflag ;
tagflag: OPTIONAL

enumdecl: '{' enums '}'
enums:
     enums ',' enum |
     enum ;
enum:
	NAME '(' NUMBER ')' ;

range:
     '(' irange ')' |
     '(' irange ',' TRIPLEDOT ')' ;
irange:
     NUMBER DOUBLEDOT NUMBER |
     NAME DOUBLEDOT NUMBER |
     NUMBER DOUBLEDOT NAME |
     NAME DOUBLEDOT NAME ;

sizeinfo:
          '(' SIZE '(' NUMBER ')' ')' |
          '(' SIZE range ')' ;


%%
