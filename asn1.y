%{
#include <stdio.h>
#include <string.h>
#include "array.h"
#include "defs.h"

int yyparse(void);
int yylex(void);
extern int yylineno;

void yyerror(const char *str) {
	fprintf(stderr, "error %i: %s\n", yylineno, str);
}

int yywrap(void) {
	return 1;
}

static Array(Typedef*) types;

int main(void) {
  int i;

	yyparse();
  for (i = 0; i < array_len(types); ++i) {
    switch (types[i]->header.type) {
      case TYPE_CHOICE:
        printf("CHOICE '%s' - #tags: %i\n", types[i]->header.name, array_len(types[i]->choice.choices)); 
        break;
      case TYPE_SEQUENCE:
        printf("SEQUENCE '%s' - #tags: %i\n", types[i]->header.name, array_len(types[i]->sequence.items)); 
        break;
    }
  }
}
%}

%token ENUMERATED SIZE BIT_STRING BOOLEAN OCTET_STRING INTEGER DOUBLEDOT TRIPLEDOT TAGS BEGIN_ END_ DEFINITIONS IMPLICIT CHOICE SEQUENCE OF OPTIONAL NAME ASSIGNMENT NUMBER

%union
{
	int number;
	char *string;
  Typedef* type;
  Array(Tag) tags;
  Tag tag;
}

%token <number> NUMBER
%token <string> NAME
%type <tags> tags
%type <tag> tag
%type <type> type

%%
commands: | commands command ;

command:
	NAME DEFINITIONS cmdflags ASSIGNMENT BEGIN_ definitions END_ ;

cmdflags: | cmdflags cmdflag ;

cmdflag: IMPLICIT | TAGS ;

definitions: | definitions definition ;

definition:
	NAME ASSIGNMENT type
  { 
    if ($3) {
      $3->header.name = $1;
      array_push(types, $3);
    }
  } |

	NAME INTEGER ASSIGNMENT NUMBER ;

type:
	NAME
  { $$ = 0; } |

	NAME sizeinfo
  { $$ = 0; } |

	CHOICE '{' tags '}'
  { $$ = choice_create((Array(Tag))$3); } |

	SEQUENCE '{' tags '}'
  { $$ = sequence_create((Array(Tag))$3); } |

	SEQUENCE sizeinfo OF NAME
  { $$ = 0; } |

	SEQUENCE OF NAME
  { $$ = 0; } |

	BOOLEAN
  { $$ = 0; } |

	OCTET_STRING { $$ = 0; } |
	OCTET_STRING sizeinfo { $$ = 0; } |
	BIT_STRING { $$ = 0; } |
  BIT_STRING enumdecl { $$ = 0; } |
	BIT_STRING enumdecl sizeinfo { $$ = 0; } |
	BIT_STRING sizeinfo { $$ = 0; } |
	INTEGER { $$ = 0; } |
	INTEGER enumdecl { $$ = 0; } |
	INTEGER range { $$ = 0; } |
	ENUMERATED enumdecl { $$ = 0; } ;

tags:
	tags ',' tag
  { array_push($$, $3); } |

	tag
  { $$ = 0; array_push($$, $1); } ;

tag:
   NAME '[' NUMBER ']' type tagflags
   { $$ = tag_create($1, $3, $5); } |

   NAME type tagflags
   { $$ = tag_create($1, 0, $2); } ;

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
