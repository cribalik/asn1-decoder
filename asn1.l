%{
#include <stdio.h>
#include "array.h"
#include "defs.h"

Typedef *type_alloc(Typedef type) {
  Typedef *p = malloc(sizeof(*p));
  *p = type;
  return p;
}

Typedef *choice_create(Array(Tag) choices) {
  Typedef r = {0};
  r.header.type = TYPE_CHOICE;
  r.choice.choices = choices;
  return type_alloc(r);
}

Typedef *sequence_create(Array(Tag) items) {
  Typedef r = {0};
  r.header.type = TYPE_SEQUENCE;
  r.sequence.items = items;
  return type_alloc(r);
}

Tag tag_create(char *name, int id, Typedef *type) {
  Tag t = {0};
  t.name = name;
  t.id = id;
  t.type = type;
  return t;
}

#include "y.tab.h"

%}

%option yylineno

%%
ENUMERATED				return ENUMERATED;
SIZE				return SIZE;
BOOLEAN				return BOOLEAN;
BIT\ STRING				return BIT_STRING;
OCTET\ STRING				return OCTET_STRING;
\.\.				return DOUBLEDOT;
\.\.\.				return TRIPLEDOT;
OPTIONAL				return OPTIONAL;
INTEGER					return INTEGER;
DEFINITIONS				return DEFINITIONS;
TAGS					return TAGS;
BEGIN					return BEGIN_;
END					return END_;
IMPLICIT				return IMPLICIT;
CHOICE					return CHOICE;
SEQUENCE				return SEQUENCE;
OF					return OF;
[A-Za-z_][A-Za-z_0-9-]*[A-Za-z]*	yylval.string = strdup(yytext); return NAME;
::=					return ASSIGNMENT;
\{					return '{';
\}					return '}';
\[					return '[';
\]					return ']';
\(					return '(';
\)					return ')';
[0-9]+					yylval.number = atoi(yytext); return NUMBER;
,					return ',';
--.*$					/* ignore comments */
[ \t\n\r]+				/* ignore whitespace */
%%
