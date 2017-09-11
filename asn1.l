%{
#include <stdio.h>
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
