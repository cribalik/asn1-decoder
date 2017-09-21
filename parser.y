%{
#include "parser.c"
%}

%token TOK_NULL ENUMERATED SIZE UTF8_STRING PRINTABLE_STRING IA5_STRING BIT_STRING BOOLEAN OCTET_STRING INTEGER DOUBLEDOT TRIPLEDOT TAGS BEGIN_ END_ DEFINITIONS IMPLICIT CHOICE SEQUENCE OF OPTIONAL NAME ASSIGNMENT NUMBER

%union
{
  int number;
  char *string;
  ASN1_Type* type;
  Array(Tag) tags;
  Tag tag;
  unsigned int flag;
}

%token <number> NUMBER
%token <string> NAME
%type <tags> tags
%type <tag> tag
%type <type> type
%type <flag> tagflags
%type <flag> tagflag

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
    array_push(types, asn1_typedef_create($3, $1));
  } |

  NAME INTEGER ASSIGNMENT NUMBER ;

type:
  NAME
  { $$ = asn1_typeref_create($1); array_push(type_references, $$); } |

  NAME sizeinfo
  { $$ = asn1_typeref_create($1); array_push(type_references, $$); } |

  CHOICE '{' tags '}'
  { $$ = asn1_choice_create((Array(Tag))$3); } |

  SEQUENCE '{' tags '}'
  { $$ = asn1_sequence_create((Array(Tag))$3); } |

  SEQUENCE sizeinfo OF type
  { $$ = asn1_list_create($4); } |
  SEQUENCE OF type
  { $$ = asn1_list_create($3); } |

  TOK_NULL
  { $$ = &asn1_null_type; } |

  BOOLEAN
  { $$ = &asn1_boolean_type; } |

  OCTET_STRING
  { $$ = &asn1_octet_string_type; } |
  OCTET_STRING sizeinfo
  { $$ = &asn1_octet_string_type; } |

  BIT_STRING
  { $$ = &asn1_bit_string_type; } |
  BIT_STRING enumdecl
  { $$ = &asn1_bit_string_type; } |
  BIT_STRING enumdecl sizeinfo
  { $$ = &asn1_bit_string_type; } |
  BIT_STRING sizeinfo
  { $$ = &asn1_bit_string_type; } |

  INTEGER
  { $$ = &asn1_integer_type; } |
  INTEGER enumdecl
  { $$ = &asn1_integer_type; } |
  INTEGER range
  { $$ = &asn1_integer_type; } |

  UTF8_STRING
  { $$ = &asn1_utf8_string_type; } |
  UTF8_STRING sizeinfo
  { $$ = &asn1_utf8_string_type; } |

  IA5_STRING
  { $$ = &asn1_ia5_string_type; } |
  IA5_STRING sizeinfo
  { $$ = &asn1_ia5_string_type; } |

  PRINTABLE_STRING
  { $$ = &asn1_printable_string_type; } |
  PRINTABLE_STRING sizeinfo
  { $$ = &asn1_printable_string_type; } |

  ENUMERATED enumdecl
  { $$ = &asn1_integer_type; } ;

tags:
  tags ',' tag
  { array_push($$, $3); } |

  tag
  { Tag _tag = $1; $$ = 0; array_push($$, _tag); } ;

tag:
   NAME '[' NUMBER ']' type tagflags
   { $$ = asn1_tag_create($1, $3, $5, $6); } |

   NAME type tagflags
   { $$ = asn1_tag_create($1, TAG_NO_ID, $2, $3); } ;

tagflags:
    { $$ = 0; } |

    tagflags tagflag
    { $$ = $1 | $2; } ;

tagflag:
    OPTIONAL
    { $$ = TAG_FLAG_OPTIONAL; }

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
