#ifndef DEFS_H
#define DEFS_H

#include "array.h"

typedef enum Type Type;
typedef struct TypedefHeader TypedefHeader;
typedef struct ASN1_Typedef ASN1_Typedef;
typedef union ASN1_Type ASN1_Type;
typedef struct Tag Tag;

enum Type {
  TYPE_UNKNOWN,
  TYPE_SEQUENCE,
  TYPE_BOOLEAN,
  TYPE_ENUM,
  TYPE_OCTET_STRING,
  TYPE_BIT_STRING,
  TYPE_INTEGER,
  TYPE_CHOICE,
  TYPE_LIST,
  _TYPE_REFERENCE, /* only used during parsing phase. All type references should have been resolved and removed when parse() returns */
};

struct ASN1_Typedef {
  char *name;
  ASN1_Type *type;
  /* TODO sizespecs and stuff? */
};

struct Tag {
  char *name;
  ASN1_Type *type;
  int id;
};

union ASN1_Type {
  Type type;

  struct {
    Type type;
    Array(Tag) choices;
  } choice;

  struct {
    Type type;
    Array(Tag) items;
  } sequence;

  struct {
    Type type;
    ASN1_Type *item_type;
  } list;

  struct {
    Type type;
    char *reference_name;
  } reference;
};

static Tag asn1_tag_create(char *name, int id, ASN1_Type *type);
static ASN1_Type *asn1_choice_create(Array(Tag) choices);
static ASN1_Type *asn1_sequence_create(Array(Tag) items);
static ASN1_Type *asn1_type_create(char *name, ASN1_Type *base);
static ASN1_Type *asn1_list_create(ASN1_Type *type);
static ASN1_Typedef *asn1_typedef_create(ASN1_Type *type, char *name);
int asn1_parse(const char **filenames, int num_files, ASN1_Typedef ***types_out, int *num_types_out);

#endif /* DEFS_H */
