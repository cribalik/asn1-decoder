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

