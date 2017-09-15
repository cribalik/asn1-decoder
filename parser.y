%{
#include "parser.c"
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
  { $$ = typeref_create($1); } |

	NAME sizeinfo
  { $$ = typeref_create($1); } |

	CHOICE '{' tags '}'
  { $$ = choice_create((Array(Tag))$3); } |

	SEQUENCE '{' tags '}'
  { $$ = sequence_create((Array(Tag))$3); } |

	SEQUENCE sizeinfo OF NAME
  { $$ = list_create($4); } |
	SEQUENCE OF NAME
  { $$ = list_create($3); } |

	BOOLEAN
  { $$ = &boolean_type; } |

	OCTET_STRING
  { $$ = &octet_string_type; } |
	OCTET_STRING sizeinfo
  { $$ = &octet_string_type; } |

	BIT_STRING
  { $$ = &bit_string_type; } |
  BIT_STRING enumdecl
  { $$ = &bit_string_type; } |
	BIT_STRING enumdecl sizeinfo
  { $$ = &bit_string_type; } |
	BIT_STRING sizeinfo
  { $$ = &bit_string_type; } |

	INTEGER
  { $$ = &integer_type; } |
	INTEGER enumdecl
  { $$ = &integer_type; } |
	INTEGER range
  { $$ = &integer_type; } |

	ENUMERATED enumdecl
  { $$ = &integer_type; } ;

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
