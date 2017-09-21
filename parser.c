#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "array.h"
#include "defs.h"

static ASN1_Type asn1_null_type;
static ASN1_Type asn1_boolean_type;
static ASN1_Type asn1_integer_type;
static ASN1_Type asn1_octet_string_type;
static ASN1_Type asn1_bit_string_type;
static ASN1_Type asn1_utf8_string_type;
static ASN1_Type asn1_ia5_string_type;
static ASN1_Type asn1_printable_string_type;

static Array(ASN1_Typedef*) types;
static Array(ASN1_Type*) type_references;

extern FILE *yyin;

int yyparse(void);
int yylex(void);
extern int yylineno;
static const char *current_file;

static ASN1_Type *type_alloc(ASN1_Type in) {
  ASN1_Type *t;
  t = malloc(sizeof(*t));
  *t = in;
  return t;
}

ASN1_Typedef *asn1_typedef_create(ASN1_Type *type, char *name) {
  ASN1_Typedef *t;
  t = malloc(sizeof(*t));
  t->type = type;
  t->name = name;
  return t;
}

ASN1_Type *asn1_typeref_create(char *name) {
  ASN1_Type t = {0};
  t.type = _TYPE_REFERENCE;
  t.reference.reference_name = name;
  return type_alloc(t);
}

ASN1_Type *asn1_choice_create(Array(Tag) choices) {
  ASN1_Type t = {0};
  t.type = TYPE_CHOICE;
  t.choice.choices = choices;
  return type_alloc(t);
}

ASN1_Type *asn1_list_create(ASN1_Type *type) {
  ASN1_Type r = {0};
  r.type = TYPE_LIST;
  r.list.item_type = type;
  return type_alloc(r);
}

ASN1_Type *asn1_sequence_create(Array(Tag) items) {
  ASN1_Type r = {0};
  r.type = TYPE_SEQUENCE;
  r.sequence.items = items;
  return type_alloc(r);
}

Tag asn1_tag_create(char *name, int id, ASN1_Type *type, unsigned int flags) {
  Tag t = {0};
  t.name = name;
  t.id = id;
  t.type = type;
  t.flags = flags;
  return t;
}

static void yyerror(const char *str) {
  fprintf(stderr, "error %s:%i: %s\n", current_file, yylineno, str);
  exit(1);
}

static void builtin_types_init(void) {
  asn1_null_type.type = TYPE_NULL;
  asn1_boolean_type.type = TYPE_BOOLEAN;
  asn1_integer_type.type = TYPE_INTEGER;
  asn1_octet_string_type.type = TYPE_OCTET_STRING;
  asn1_bit_string_type.type = TYPE_BIT_STRING;
  asn1_utf8_string_type.type = TYPE_UTF8_STRING;
  asn1_ia5_string_type.type = TYPE_IA5_STRING;
  asn1_printable_string_type.type = TYPE_PRINTABLE_STRING;
}

static int asn1_resolve_reference_type(ASN1_Type *type) {
  int i,err;
  ASN1_Typedef **match;

  if (type->type != _TYPE_REFERENCE)
    return 0;

  array_find(types, match, !strcmp((*match)->name, type->reference.reference_name))
  if (!match) {
    fprintf(stderr, "Type '%s' does not exist\n", type->reference.reference_name);
    return 1;
  }

  err = asn1_resolve_reference_type((*match)->type);
  if (err)
    return 1;
  *type = *(*match)->type;
  return 0;
}

Array(ASN1_Typedef) asn1_parse(const char **filenames, int num_files) {
  int i = 0;
  FILE *f;
  Array(ASN1_Typedef) result = 0;

  /* parse each file */
  for (; num_files; --num_files, ++filenames) {
    f = fopen(*filenames, "rb");
    if (!f) {
      fprintf(stderr, "Could not find file '%s': %s\n", *filenames, strerror(errno));
      exit(1);
    }

    builtin_types_init();

    yyin = f;
    current_file = *filenames;

    yyparse();

    fclose(f);
  }

  /* resolve reference types */
  for (i = 0; i < array_len(type_references); ++i)
    if (asn1_resolve_reference_type(type_references[i])) {
      fprintf(stderr, "Failed to resolve reftypes, exiting..\n");
      exit(1);
    }


  for (i = 0; i < array_len(types); ++i) {
    if (types[i]->type->type == TYPE_UNKNOWN) {
      fprintf(stderr, "Unable to parse type of %s, exiting..\n", types[i]->name);
      exit(1);
    }
  }

  /* print the types */
  #if 0
  for (i = 0; i < array_len(types); ++i) {
    ASN1_Type *t = types[i]->type;
    printf("%s => ", types[i]->name);
    for (;;) {
      switch (t->type) {
        case TYPE_UNKNOWN:
          printf("UNKNOWN");
          break;
        case TYPE_CHOICE:
          printf("CHOICE - #tags: %i", array_len(t->choice.choices)); 
          break;
        case TYPE_SEQUENCE:
          printf("SEQUENCE - #tags: %i", array_len(t->sequence.items)); 
          break;
        case _TYPE_REFERENCE:
          printf("typedef of '%s'", t->reference.reference_name);
          break;
        case TYPE_BOOLEAN:
          printf("BOOLEAN");
          break;
        case TYPE_ENUM:
          printf("ENUM");
          break;
        case TYPE_OCTET_STRING:
          printf("OCTET STRING");
          break;
        case TYPE_BIT_STRING:
          printf("BIT STRING");
          break;
        case TYPE_INTEGER:
          printf("INTEGER");
          break;
        case TYPE_LIST:
          printf("list of: \t");
          t = t->list.item_type;
          continue;
      }
      break;
    }
    putchar('\n');
  }
  #endif

  array_resize(result, array_len(types));
  for (i = 0; i < array_len(types); ++i)
    result[i] = *types[i];
  array_free(types);

  return result;
}

