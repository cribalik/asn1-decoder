#ifndef DEFS_H
#define DEFS_H

#include "array.h"

typedef enum Type Type;
typedef struct TypedefHeader TypedefHeader;
typedef union Typedef Typedef;
typedef struct Tag Tag;

enum Type {
  TYPE_UNKNOWN,
  TYPE_REFERENCE,
  TYPE_SEQUENCE,
  TYPE_BOOLEAN,
  TYPE_ENUM,
  TYPE_OCTET_STRING,
  TYPE_BIT_STRING,
  TYPE_INTEGER,
  TYPE_CHOICE,
  TYPE_LIST
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

  struct {
    TypedefHeader header;
    char *name;
    Typedef *type; /* will be null until after type resolve phase */
  } reference;

  struct {
    TypedefHeader header;
    char *name;
    Typedef *type; /* will be null until after type resolve phase */
  } list;
};

Tag tag_create(char *name, int id, Typedef *type);
Typedef *choice_create(Array(Tag) choices);
Typedef *sequence_create(Array(Tag) items);
Typedef *type_create(char *name, Typedef *base);
Typedef *list_create(char *name);

extern Typedef boolean_type;

#endif /* DEFS_H */
