#ifndef DEFS_H
#define DEFS_H

#include "array.h"

typedef enum Type Type;
typedef struct TypedefHeader TypedefHeader;
typedef union Typedef Typedef;
typedef struct Tag Tag;

enum Type {
  TYPE_SEQUENCE,
  TYPE_BOOLEAN,
  TYPE_ENUM,
  TYPE_OCTET_STRING,
  TYPE_INTEGER,
  TYPE_CHOICE
};

struct TypedefHeader {
  Type type;
  char *name;
};

struct Tag {
  char *name;
  Typedef *type;
  char *type_name; /* might be null for inline types */
  int id;
};


union Typedef {
  TypedefHeader header;

  struct {
    TypedefHeader header;
    Array(Tag) choices;
  } choice;

  struct {
    TypedefHeader header;
    Array(Tag) items;
  } sequence;
};

Typedef *choice_create(Array(Tag) choices);
Tag tag_create(char *name, int id, Typedef *type);
Typedef *sequence_create(Array(Tag) items);

#endif /* DEFS_H */
