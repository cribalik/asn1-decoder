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

