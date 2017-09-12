#include "array.h"
#include "defs.h"

Typedef boolean_type;
Typedef integer_type;
Typedef octet_string_type;
Typedef bit_string_type;

Typedef *type_alloc(Typedef type) {
  Typedef *p = malloc(sizeof(*p));
  *p = type;
  return p;
}

Typedef *typeref_create(char *name) {
  Typedef r = {0};
  r.header.type = TYPE_REFERENCE;
  r.reference.name = name;
  return type_alloc(r);
}

Typedef *choice_create(Array(Tag) choices) {
  Typedef r = {0};
  r.header.type = TYPE_CHOICE;
  r.choice.choices = choices;
  return type_alloc(r);
}

Typedef *list_create(char *name) {
  Typedef r = {0};
  r.header.type = TYPE_LIST;
  r.list.name = name;
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

void builtin_types_init(void) {
  boolean_type.header.type = TYPE_BOOLEAN;
  boolean_type.header.type = TYPE_INTEGER;
  boolean_type.header.type = TYPE_OCTET_STRING;
  boolean_type.header.type = TYPE_BIT_STRING;
}

int main(void) {
  int i;

  builtin_types_init();

	yyparse();
  for (i = 0; i < array_len(types); ++i) {
    char *name = types[i]->header.name;
    Typedef *t = types[i];
    switch (t->header.type) {
      case TYPE_CHOICE:
        printf("'%s'- CHOICE - #tags: %i\n", name, array_len(t->choice.choices)); 
        break;
      case TYPE_SEQUENCE:
        printf("'%s' - SEQUENCE - #tags: %i\n", name, array_len(t->sequence.items)); 
        break;
      case TYPE_REFERENCE:
        printf("'%s' - typedef of '%s'\n", name, t->reference.name);
        break;
      case TYPE_BOOLEAN:
        printf("'%s' - BOOLEAN\n", name);
        break;
      case TYPE_ENUM:
        printf("'%s' - ENUM\n", name);
        break;
      case TYPE_OCTET_STRING:
        printf("'%s' - OCTET STRING\n", name);
        break;
      case TYPE_BIT_STRING:
        printf("'%s' - BIT STRING\n", name);
        break;
      case TYPE_INTEGER:
        printf("'%s' - INTEGER\n", name);
        break;
      case TYPE_LIST:
        printf("'%s' - list of '%s'\n", name, t->list.name);
        break;
    }
  }
}

