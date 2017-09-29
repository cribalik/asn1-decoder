/* TODO:
 * Improve tree rendering, don't
 * Support object editing
 * Support Bit string (with enum specs) and Enum
 * Support explicit tags
 */

#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdint.h>
#include <ncurses.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
/* windows */
  #include <io.h>
#else
/* linux */
  #include <unistd.h>
#endif

#define RED_STR "\x1B[31m"
#define GREEN_STR "\x1B[32m"
#define YELLOW_STR "\x1B[33m"
#define BLUE_STR "\x1B[34m"
#define MAGENTA_STR "\x1B[35m"
#define CYAN_STR "\x1B[36m"
#define NORMAL_STR "\x1B[39m"

const char *RED = "";
const char *GREEN = "";
const char *YELLOW = "";
const char *BLUE = "";
const char *MAGENTA = "";
const char *CYAN = "";
const char *NORMAL = "";

#define STATIC_ASSERT(expr, name) typedef char static_assert_##name[expr?1:-1]

typedef uint64_t u64;
STATIC_ASSERT(sizeof(u64) == 8, u64_is_64bit);

typedef struct Object Object;
struct Object {

  /* asn1 data */
  Object *parent;
  char *name;
  ASN1_Type *type;
  union {
    struct {
      Object *value;
    } choice;

    struct {
      Array(Object*) values;
    } sequence;

    /* IA5String, UTF8String, OCTET STRING */
    struct {
      int len;
      unsigned char *value;
    } string;

    struct {
      u64 value;
    } integer;
  } data;

  /* rendering data */
  char collapsed;
};

#define IS_UTF8_TRAIL(c) (((c)&0xC0) == 0x80)
#define isset(x, flag) ((x) & (flag))

#if 0
#define DEBUG(stmt) do {stmt;} while (0)
#else
#define DEBUG(stmt)
#endif

static struct {
  Array(unsigned char) data_begin;
  unsigned char *data;
  Array(ASN1_Typedef) types;
  Object *current_object;
} Global;

int strstri(const char *needle, const char *haystack) {
  int l1, l2, i;
  if (!haystack || !*haystack || !needle || !*needle)
    return 0;

  l1 = strlen(needle);
  l2 = strlen(haystack);
  if (l1 > l2)
    return 0;

  for (i = 0; i <= l2-l1; ++i) {
    const char *a, *b;

    for (a = needle, b = haystack+i; *a; ++a, ++b)
      if (tolower(*a) != tolower(*b))
        goto next;

    return 1;

    next:;
  }
  return 0;
}

enum {
  BER_TAG_EOC               = 0,
  BER_TAG_BOOLEAN           = 1,
  BER_TAG_INTEGER           = 2,
  BER_TAG_BIT               = 3,
  BER_TAG_OCTET             = 4,
  BER_TAG_NULL              = 5,
  BER_TAG_OBJECT_IDENTIFIER = 6,
  BER_TAG_OBJECT_DESCRIPTOR = 7,
  BER_TAG_EXTERNAL          = 8,
  BER_TAG_REAL              = 9,
  BER_TAG_ENUMERATED        = 10,
  BER_TAG_EMBEDDED          = 11,
  BER_TAG_UTF8STRING        = 12,
  BER_TAG_RELATIVE_OID      = 13,
  BER_TAG_Reserved_1        = 14,
  BER_TAG_Reserved_2        = 15,
  BER_TAG_SEQUENCE          = 16,
  BER_TAG_SET               = 17,
  BER_TAG_NUMERICsTRING     = 18,
  BER_TAG_PRINTABLESTRING   = 19,
  BER_TAG_T61STRING         = 20,
  BER_TAG_VIDEOTEXSTRING    = 21,
  BER_TAG_IA5STRING         = 22,
  BER_TAG_UTCTIME           = 23,
  BER_TAG_GENERALIZEDTIME   = 24,
  BER_TAG_GRAPHICsTRING     = 25,
  BER_TAG_VISIBLESTRING     = 26,
  BER_TAG_GENERALSTRING     = 27,
  BER_TAG_UNIVERSALSTRING   = 28,
  BER_TAG_CHARACTER         = 29,
  BER_TAG_BMPSTRING         = 30
};

typedef enum {
  BER_IDENTIFIER_CLASS_UNIVERSAL = 0,
  BER_IDENTIFIER_CLASS_APPLICATION = 1,
  BER_IDENTIFIER_CLASS_CONTEXT_SPECIFIC = 2,
  BER_IDENTIFIER_CLASS_PRIVATE = 3
} BerIdentifierClass;

typedef enum {
  BER_CONSTRUCTED = 1,
  BER_PRIMITIVE = 0,
} BerPC;

typedef struct {
  BerPC pc;
  BerIdentifierClass class;
  int tag_number;
} BerIdentifier;

static void vprint_debug(const char *fmt, va_list args) {
  printf("%sDebug: ", YELLOW);
  vprintf(fmt, args);
  printf("%s", NORMAL);
}

static void print_debug(const char *fmt, ...) {
  DEBUG(
    va_list args;
    va_start(args, fmt);
    vprint_debug(fmt, args);
    va_end(args);
  );
}

static void vprint_error(const char *fmt, va_list args) {
  printf("\n\n%sError at byte %i: ", RED, (int)(Global.data - Global.data_begin));
  vprintf(fmt, args);
  printf("%s", NORMAL);
}

static void print_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_error(fmt, args);
  va_end(args);
}

static void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprint_error(fmt, args);
  va_end(args);
  exit(1);
}

static void check_if_past_end() {
  if (Global.data > array_last(Global.data_begin))
    die("Unexpected end of input stream\n");
}

static void check_end(unsigned char *end) {
  if (end > array_end(Global.data_begin))
    die("Went past end of data, exiting..\n");
}

static unsigned char next() {
  check_if_past_end();
  return *Global.data++;
}

static int ber_tag_number_read(unsigned char c) {
  int tag_number;

  if ((c & 0x1f) != 0x1f)
    return c & 0x1f;

  tag_number = 0;
  for (;;) {
    c = next();
    tag_number <<= 7;
    tag_number |= (c & 0x7f);

    if (!(c & 0x80))
      return tag_number;
  }
  return tag_number;
}

enum {
  LENGTH_INDEFINITE = -2
};

static int _ber_length_read() {
  unsigned char c;
  c = next();

  /* definite short ? */
  if (!(c & 0x80))
    return c;

  /* reserved ? */
  if ((c & 0x7F) == 0x7F)
    die("Reserved length field\n");

  /* definite long ? */
  if (c & 0x7F) {
    int i, result;

    i = c & 0x7F;
    result = 0;
    while (i--) {
      c = next();

      result <<= 8;
      result |= c;
    }
    return result;
  }

  /* indefinite not supported */
  die("Indefinite length encoding not supported\n");
  return -1;
}

static int ber_length_read() {
  int l = _ber_length_read();
  print_debug("Length: %i\n", l);
  return l;
}

static BerIdentifier ber_identifier_read() {
  BerIdentifier i = {0};
  unsigned char c;

  c = next();

  i.class = (c & 0xC0) >> 6;
  i.pc = !!(c & 0x20);
  i.tag_number = ber_tag_number_read(c);

  if (i.tag_number == -1)
    exit(1);

  print_debug("BerIdentifier = (class: %i, pc: %i, tag number: %i)\n", i.class, i.pc, i.tag_number);
  return i;
}

static long file_get_size(FILE *file) {
  long result, old_pos;

  old_pos = ftell(file);
  fseek(file, 0, SEEK_END);
  result = ftell(file);
  fseek(file, old_pos, SEEK_SET);
  return result;
}

static Array(unsigned char) file_get_contents(const char *filename) {
  FILE *f;
  long num_read;
  Array(unsigned char) data;

  data = 0;

  f = fopen(filename, "rb");
  if (!f)
    return 0;

  array_resize(data, file_get_size(f));
  num_read = fread(data, 1, array_len(data), f);
  if (num_read != array_len(data))
    goto err;

  fclose(f);
  return data;

  err:
  fclose(f);
  array_free(data);
  return 0;
}

#define TABS "%*c"
#define TAB(n) ((n)*4), ' '

static BerIdentifier ber_identifier_create(BerPC pc, BerIdentifierClass class, int tag_number) {
  BerIdentifier r = {0};
  r.pc = pc;
  r.class = class;
  r.tag_number = tag_number;
  return r;
}

static int ber_identifier_eq(BerIdentifier a, BerIdentifier b) {
  return a.pc == b.pc && a.class == b.class && a.tag_number == b.tag_number;
}

#if 1
static void print_definition(ASN1_Type *t, int indent) {
  if (indent > 10)
    return;
  switch (t->type) {
    case TYPE_NULL:
      printf("NULL");
      break;
    case TYPE_IA5_STRING:
      printf("IA5String");
      break;
    case TYPE_PRINTABLE_STRING:
      printf("PrintableString");
      break;
    case TYPE_UNKNOWN:
      printf("UNKNOWN");
      break;
    case TYPE_CHOICE: {
      Tag *tag;

      printf("CHOICE\n"); 
      array_foreach(t->choice.choices, tag) {
        printf(TABS "%s [%i] ", TAB(indent+1), tag->name, tag->id);
        print_definition(tag->type, indent+1);
      }
    } break;
    case TYPE_SEQUENCE: {
      Tag *tag;

      printf("SEQUENCE\n"); 
      array_foreach(t->sequence.items, tag) {
        printf(TABS "%s [%i] ", TAB(indent+1), tag->name, tag->id);
        print_definition(tag->type, indent+1);
      }
    } break;
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
      printf("list of ");
      print_definition(t->list.item_type, indent);
      break;
    case TYPE_UTF8_STRING:
      printf("UTF8String");
      break;
  }
  putchar('\n');
}
#else
#define print_definition(t, i)
#endif


static BerIdentifier get_identifier_of_type(ASN1_Type *t) {
  switch (t->type) {
    case TYPE_CHOICE:
      goto fail;
    case TYPE_SEQUENCE:
      return ber_identifier_create(BER_CONSTRUCTED, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_SEQUENCE);
    case TYPE_BOOLEAN:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_BOOLEAN);
    case TYPE_ENUM:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_ENUMERATED);
    case TYPE_OCTET_STRING:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_OCTET);
    case TYPE_BIT_STRING:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_BIT);
    case TYPE_INTEGER:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_INTEGER);
    case TYPE_LIST:
      return ber_identifier_create(BER_CONSTRUCTED, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_SEQUENCE);
    case TYPE_UTF8_STRING:
      return ber_identifier_create(BER_PRIMITIVE, BER_IDENTIFIER_CLASS_UNIVERSAL, BER_TAG_UTF8STRING);
    default:
      goto fail;
  }

  fail:
  print_error("Could not get identifier of type");
  print_definition(t, 0);
  exit(1);
}

static Tag *ber_find_matching_tag(Tag *tags, int n, BerIdentifier ber_identifier) {
  Tag *tag;
  int i;

  for (i = 0; i < n; ++i) {
    tag = tags+i;
    if (/* if it has no id, match on type */
        (tag->id == TAG_NO_ID && ber_identifier_eq(ber_identifier, get_identifier_of_type(tag->type))) ||
        /* otherwise match on id */
        ber_identifier.tag_number == tag->id)
      return tag;
  }
  return 0;
}

static int ber_tag_is_implicit(Tag *tag) {
  return tag->id != TAG_NO_ID;
}

static ASN1_Typedef *get_type_by_name(const char *name);

static Object* decode(ASN1_Type *type, char *name, BerIdentifier *bi, unsigned char *end, int indent) {
  Object *object;
  BerIdentifier ber_identifier;
  unsigned char *start;

  object = malloc(sizeof(Object));
  memset(object, 0, sizeof(*object));
  object->name = strdup(name);
  object->type = type;
  object->parent = 0;

  start = Global.data;

  switch (type->type) {
    case TYPE_CHOICE: {
      Tag *tag;
      int len;

      ber_identifier = bi ? *bi : ber_identifier_read();
      len = ber_length_read();
      end = Global.data + len;

      /* find a matching tag */
      tag = ber_find_matching_tag(type->choice.choices, array_len(type->choice.choices), ber_identifier);
      if (!tag) {
        print_error("For CHOICE %s, BER tag number was %i, but no such choice exists.\n", name, ber_identifier.tag_number);
        printf("Available tags:\n");
        print_definition(type, indent);
        exit(1);
      }

      if (ber_identifier.pc == BER_CONSTRUCTED)
        ber_identifier = ber_identifier_read();

      object->data.choice.value = decode(tag->type, tag->name,
                                       &ber_identifier,
                                       end,
                                       indent+1);
      object->data.choice.value->parent = object;
    } break;

    case TYPE_SEQUENCE: {
      Tag *tag, *next;
      unsigned char *item_end;
      int item_length, first = 1;
      Object *d;

      object->data.sequence.values = 0;

      if (Global.data == end)
        break;

      ber_identifier = bi ? *bi : ber_identifier_read();

      next = type->sequence.items;

      for (; Global.data < end;) {
        if (!first)
          ber_identifier = ber_identifier_read();
        first = 0;

        item_length = ber_length_read();
        item_end = Global.data + item_length;

        if (Global.data >= end)
          break;

        tag = ber_find_matching_tag(next, array_end(type->sequence.items)-next, ber_identifier);
        if (!tag) {
          print_error("Unable to matching tag for ber identifier (%i, %i, %i)\nAlternatives are:\n", ber_identifier.class, ber_identifier.pc, ber_identifier.tag_number);
          print_definition(type, 0);
          exit(1);
        }

        /* check that we didn't skip any non-optionals */
        for (; next < tag; ++next) {
          if (!isset(next->flags, TAG_FLAG_OPTIONAL)) {
            print_error("Tag %s skips over non-optional tag %s.\n", tag->name, next->name);
            print_definition(type, 0);
            exit(1);
          }
        }
        next = tag+1;

        d = decode(tag->type, tag->name, ber_identifier.pc == BER_PRIMITIVE ? &ber_identifier : 0, item_end, indent+1);
        d->parent = object;
        array_push(object->data.sequence.values, d);
      }

      if (Global.data != end)
        die("Expected to read %i bytes from sequence, but it was of size %i\n", end-start, Global.data-start);
    } break;

    case TYPE_LIST: {
      int i, item_length, first = 1;
      unsigned char *item_end;
      char item_name[32];
      Object *d;

      object->data.sequence.values = 0;

      if (Global.data == end)
        break;

      ber_identifier = bi ? *bi : ber_identifier_read();

      /* account for the identifier already read */
      for (i = 1; Global.data < end; ++i) {

        if (!first)
          ber_identifier = ber_identifier_read();
        first = 0;
        item_length = ber_length_read();
        item_end = Global.data + item_length;

        if (Global.data == end)
          break;

        sprintf(item_name, "item #%i", i);
        d = decode(type->list.item_type, item_name, 0, item_end, indent+1);
        d->parent = object;
        array_push(object->data.sequence.values, d);
      }

      if (Global.data != end)
        die("List should be %i long, but was at least %i\n", end-start, Global.data-start);
    } break;

    case TYPE_BOOLEAN: {
      if (end - Global.data != 1)
        die("Length of boolean was not 1, but %i\n", end - Global.data);

      object->data.integer.value = next();
    } break;

    case TYPE_INTEGER: {
      u64 i;
      int len;

      /* TODO: handle enumdecls */

      len = end - Global.data;

      for (i = 0; len; --len)
        i <<= 8, i |= next();
      object->data.integer.value = i;
    } break;

    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING: {
      int len;

      len = end - Global.data;

      /* polystar special sauce */
      if (strcmp(name, "cdrData") == 0) {
        ASN1_Typedef *xdr_type;
        xdr_type = get_type_by_name("XDR-TYPE");
        if (xdr_type) {
          free(object);
          object = decode(xdr_type->type, "cdrData", 0, end, indent+1);
          break;
        }
      }

      /* copy the string just in case the raw data stops existing */
      object->data.string.value = malloc(len);
      object->data.string.len = len;
      memcpy(object->data.string.value, Global.data, len);

      Global.data = end;
    } break;

    case TYPE_PRINTABLE_STRING:
    case TYPE_IA5_STRING:
    case TYPE_UTF8_STRING: {
      int len;

      len = end - Global.data;
      object->data.string.value = malloc(len);
      object->data.string.len = len;
      memcpy(object->data.string.value, Global.data, len);

      Global.data = end;
    } break;

    default:
      print_error("Type not supported:\n");
      print_definition(type, 0);
      exit(1);
  }

  return object;
}

static void init_colors() {
  int is_a_terminal;

#if defined(_WIN32) || defined(_WIN64)
  is_a_terminal = _isatty(fileno(stdout));
#else
  is_a_terminal = isatty(fileno(stdout));
#endif

  if (is_a_terminal) {
    RED = RED_STR;
    GREEN = GREEN_STR;
    YELLOW = YELLOW_STR;
    BLUE = BLUE_STR;
    MAGENTA = MAGENTA_STR;
    CYAN = CYAN_STR;
    NORMAL = NORMAL_STR;
  }
}

static void print_usage() {
  printf(
    "Usage: decoder ASN1FILE... BINARY TYPENAME\n"
    "\n"
    "    --interactive  interactive mode\n"
  );
}

static int is_option(const char *str) {
  return str[0] == '-' && str[1] == '-';
}

static ASN1_Typedef *get_type_by_name(const char *name) {
  ASN1_Typedef *t;
  array_find(Global.types, t, strcmp(t->name, name) == 0);
  return t;
}

#define MIN(a,b) ((b) < (a) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

static int type_is_primitive(ASN1_Type *type) {
  switch (type->type) {
    case TYPE_UNKNOWN:
    case TYPE_NULL:
    case TYPE_SEQUENCE:
    case TYPE_CHOICE:
    case TYPE_LIST:
    case _TYPE_REFERENCE:
      return 0;
    case TYPE_BOOLEAN:
    case TYPE_ENUM:
    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING:
    case TYPE_UTF8_STRING:
    case TYPE_IA5_STRING:
    case TYPE_PRINTABLE_STRING:
    case TYPE_INTEGER:
      return 1;
  }
  return 0;
}

static int type_is_compound(ASN1_Type *type) {
  switch (type->type) {
    case TYPE_UNKNOWN:
    case TYPE_NULL:
    case _TYPE_REFERENCE:
    case TYPE_BOOLEAN:
    case TYPE_ENUM:
    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING:
    case TYPE_UTF8_STRING:
    case TYPE_IA5_STRING:
    case TYPE_PRINTABLE_STRING:
    case TYPE_INTEGER:
      return 0;
    case TYPE_SEQUENCE:
    case TYPE_CHOICE:
    case TYPE_LIST:
      return 1;
  }
  return 0;
}

static void object_get_children(Object *parent, Object ***children, int *num_children) {
  *children = 0;
  *num_children = 0;
  if (type_is_primitive(parent->type))
    return;

  switch (parent->type->type) {
    case TYPE_CHOICE:
      *children = &parent->data.choice.value;
      *num_children = 1;
      break;
    case TYPE_SEQUENCE:
    case TYPE_LIST:
      *children = parent->data.sequence.values;
      *num_children = array_len(parent->data.sequence.values);
      break;

    default:
      die("Unexpected error");
      break;
  }
}

static void object_get_siblings(Object *obj, Object ***siblings, int *num_siblings) {
  *siblings = 0;
  *num_siblings = 0;
  if (!obj->parent)
    return;
  object_get_children(obj->parent, siblings, num_siblings);
}

enum {
  CURSES_GREEN = 1,
  CURSES_BLUE = 2,
  CURSES_YELLOW = 3,
  CURSES_INV = 4
};

enum {
  COLOR_FOR_IP = CURSES_BLUE,
  COLOR_FOR_INT = CURSES_GREEN,
  COLOR_FOR_TIME = CURSES_BLUE,
  COLOR_FOR_STRING = CURSES_BLUE,
  COLOR_FOR_BOOL = CURSES_BLUE,
  COLOR_FOR_HEX = CURSES_GREEN,
  COLOR_FOR_SELECTED = CURSES_INV,
  COLOR_FOR_STATUSBAR = CURSES_INV
};

static void render_object(WINDOW *window, Object *object, int x, int x_max, int y);
static int render_tree(WINDOW *window, Object *object, int x, int x_max, int *y, int y_max, int *current_object_y);

static void print_help(WINDOW* window) {
  int y = 2;
  int w = getmaxx(window);

  erase();
  mvwprintw(window, y++, w/2 - 9, "##################");
  mvwprintw(window, y++, w/2 - 9, "#                #");
  mvwprintw(window, y++, w/2 - 9, "#      HELP      #");
  mvwprintw(window, y++, w/2 - 9, "#                #");
  mvwprintw(window, y++, w/2 - 9, "##################");
  y += 2;
  mvwprintw(window, y++, w/6, "q  quit");
  mvwprintw(window, y++, w/6, "?  help");
  mvwprintw(window, y++, w/6, "=  collapse all siblings");
}

static void render_status_bar(WINDOW *statusw) {
  mvwprintw(statusw, 0, 0, "  ?: help   q: quit");
}

typedef enum {
  MODE_NORMAL,
  MODE_HELP
} Mode;
static void run_interactive(ASN1_Typedef *start_type) {
  Object *root;
  Mode mode = MODE_NORMAL;
  WINDOW *objw, *statusw;
  int width, height;

  initscr();
  curs_set(0);
  start_color();
  init_pair(CURSES_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(CURSES_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(CURSES_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(CURSES_INV, COLOR_BLACK, COLOR_WHITE);
  getmaxyx(stdscr, height, width);

  objw = newwin(height-1, width, 0, 0);
  keypad(objw, 1);
  scrollok(objw, 1);

  statusw = newwin(1, width, height-1, 0);
  wcolor_set(statusw, COLOR_FOR_STATUSBAR, 0);
  wbkgdset(statusw, COLOR_PAIR(COLOR_FOR_STATUSBAR));

  root = Global.current_object = decode(start_type->type, start_type->name, 0, 0, 0);
  for (;;) {
    int c, y, y_max, x_max, current_object_y, w,h;

    getmaxyx(stdscr, h, w);
    if (w != width || h != height) {
      wresize(objw, h-1, w);
      mvwin(objw, 0, 0);
      wresize(statusw, 1, w);
      mvwin(statusw, h-1, 0);
      width = w, height = h;
    }

    getmaxyx(objw, y_max, x_max);
    y = 0;
    current_object_y = -1;
    werase(objw);
    werase(statusw);
    switch (mode) {
    case MODE_NORMAL:
      render_tree(objw, root, 0, x_max, &y, y_max, &current_object_y);
      render_status_bar(statusw);
      break;
    case MODE_HELP:
      print_help(objw);
      break;
    }
    wrefresh(objw);
    wrefresh(statusw);

    c = wgetch(objw);
    switch (mode) {
    case MODE_HELP:
      mode = MODE_NORMAL;
      break;
    case MODE_NORMAL:
      switch (c) {
      case 'q':
        goto done;

      case '?':
        mode = MODE_HELP;
        break;

      case '=': {
        Object **siblings;
        int num_siblings;
        int i;

        object_get_siblings(Global.current_object, &siblings, &num_siblings);
        if (!siblings)
          break;
        for (i = 0; i < num_siblings; ++i)
          if (type_is_compound(siblings[i]->type))
            siblings[i]->collapsed = 1;
      } break;

      case 'h':
      case KEY_LEFT: {
        /* if already collapsed, collaps parent */
        if ((type_is_primitive(Global.current_object->type) || Global.current_object->collapsed) && Global.current_object->parent)
          Global.current_object = Global.current_object->parent;
        else
          Global.current_object->collapsed = 1;
      } break;

      case 'l':
      case KEY_RIGHT:
        Global.current_object->collapsed = 0;
        break;

      case 'k':
      case KEY_UP: {
        int i;
        Object **siblings, **children;
        int num_siblings, num_children;

        object_get_siblings(Global.current_object, &siblings, &num_siblings);
        if (!siblings)
          goto jump_to_parent;

        for (i = 0; i < num_siblings; ++i)
          if (siblings[i] == Global.current_object)
            break;
        assert(i != num_siblings);

        if (i == 0)
          goto jump_to_parent;

        /* we go to sibling, and then continue down as far as possible */
        Global.current_object = siblings[i-1];
        while (!Global.current_object->collapsed && (object_get_children(Global.current_object, &children, &num_children), children))
          Global.current_object = children[num_children-1];
        break;

        jump_to_parent:
        if (Global.current_object->parent)
          Global.current_object = Global.current_object->parent;
      } break;

      case 'j':
      case KEY_DOWN: {
        Object **children;
        int num_children;

        object_get_children(Global.current_object, &children, &num_children);

        if (!Global.current_object->collapsed && children) {
          Global.current_object = children[0];
          break;
        }
        else {
          Object **siblings;
          int num_siblings;
          Object *next;

          next = Global.current_object;

          for (;;) {
            /* find the next sibling */
            int i;

            object_get_siblings(next, &siblings, &num_siblings);
            if (!siblings)
              goto dont_move;

            for (i = 0; i < num_siblings; ++i)
              if (siblings[i] == next)
                break;
            assert(i != num_siblings);

            if (i+1 >= num_siblings) {
              /* if last sibling, go to parents sibling */
              if (!next->parent)
                break;
              next = next->parent;
              continue;
            }

            next = siblings[i+1];
            break;
          }

          Global.current_object = next;
          dont_move:;
        }
      } break;

      default:
        break;
      }
      break;
    }
    
  }
  done:
  endwin();
}

static int render_tree(WINDOW* window, Object *object, int x, int x_max, int *y, int y_max, int *current_object_y) {
  Object **children;
  int num_children, i;
  int wy;

  wy = MIN(*y, y_max-1);

  /* have we gone at least half a screen past the selected object? */
  if (*y >= y_max && *current_object_y != -1 && *y - *current_object_y > y_max/2)
    return 1;

  /* if we're not done, scroll down one and keep going */
  if (*y >= y_max)
    wscrl(window, 1);

  wmove(window, wy, 0);
  wclrtoeol(window);

  if (type_is_compound(object->type))
    mvwprintw(window, wy, x, object->collapsed ? "+" : "-");

  if (object == Global.current_object)
    *current_object_y = *y;
  render_object(window, object, x+2, x_max, wy);

  ++(*y);

  if (object->collapsed)
    return 0;

  object_get_children(object, &children, &num_children);
  if (!num_children)
    return 0;

  for (i = 0; i < num_children; ++i)
    if (render_tree(window, children[i], x + 2, x_max, y, y_max, current_object_y))
      return 1;
  return 0;
}


static u64 octet_to_int(Object *object) {
  u64 val = 0;
  int i;
  for (i = 0; i < object->data.string.len; ++i)
    val <<= 8, val |= object->data.string.value[i];
  return val;
}

static int octet_is_ip_address(Object *object) {
  /* TODO: ipv6 */
  return object->data.string.len == 4 && (strstri("ipaddr", object->name) || strstri("ip", object->name));
}

static int octet_is_printable(Object *object) {
  int i;
  for (i = 0; i < object->data.string.len; ++i)
    if (!isprint(object->data.string.value[i]))
      return 0;
  return 1;
}

static char* int_to_time(u64 val) {
  static char date[32];
  time_t t;
  struct tm *tm_time;

  t = val/1000;

  /* 3 years old or 1 day in future is ok */
  if (t >= time(0) + 60*60*24 || t <= time(0) - 60*60*24*365*3)
    return 0;

  tm_time = localtime(&t);
  strftime(date, sizeof(date)-1, "%Y-%m-%d %H:%M:%S", tm_time);
  sprintf(date+19, ".%"PRIu64, val % 1000);
  return date;
}

static char* octet_to_numberstring(Object *object) {
  static char number[64];
  int i;
  char *s = number;

  if (object->data.string.len > 8)
    return 0;

  for (i = 0; i < object->data.string.len; ++i) {
    unsigned char lo,hi,c;

    c = object->data.string.value[i];
    lo = c & 0xf;
    hi = (c & 0xf0) >> 4;

    if (lo != 0xf && lo > 9)
      return 0;
    if (lo != 0xf)
      *s++ = '0'+lo;

    if (hi != 0xf && hi > 9)
      return 0;
    if (hi != 0xf)
      *s++ = '0'+hi;
  }
  *s = 0;

  return number;
}

static void render_object(WINDOW *window, Object *object, int x, int x_max, int y) {
  /* WARNING: If you make changes here, remember to mirror the changes in dump_object_tree */
  wmove(window, y, x);

  if (object == Global.current_object)
    wattron(window, COLOR_PAIR(COLOR_FOR_SELECTED));
  wprintw(window, "%s", object->name);
  if (object == Global.current_object)
    wattroff(window, COLOR_PAIR(COLOR_FOR_SELECTED));

  switch (object->type->type) {
    case TYPE_CHOICE:
    case TYPE_SEQUENCE:
    case TYPE_LIST:
      break;

    case TYPE_BOOLEAN:
      wattron(window, COLOR_PAIR(COLOR_FOR_BOOL));
      wprintw(window, object->data.integer.value ? " TRUE" : " FALSE");
      wattroff(window, COLOR_PAIR(COLOR_FOR_BOOL));
      break;

    case TYPE_INTEGER:
      wattron(window, COLOR_PAIR(COLOR_FOR_INT));
      wprintw(window, " %"PRIu64, object->data.integer.value);
      wattroff(window, COLOR_PAIR(COLOR_FOR_INT));
      break;

    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING: {
      /* This code is weird because almost everything we have at CICS is encoded as OCTET STRINGs;
       * ip addresses, numberstrings, numbers, milliseconds etc,
       * so we employ some heuristics on common cases to try to figure out what the value really is
       */
      int i;
      const char *str;

      /* if it's small, it might be something special */
      if (object->data.string.len <= 8) {
        u64 val = octet_to_int(object);

        /* is it an ip address ? */
        if (octet_is_ip_address(object)) {
          wattron(window, COLOR_PAIR(COLOR_FOR_IP));
          wprintw(window, " %"PRIu64 ".%"PRIu64 ".%"PRIu64 ".%"PRIu64, (val & 0xFF000000) >> 24, (val & 0xFF0000) >> 16, (val & 0xFF00) >> 8, val & 0xFF);
          wattroff(window, COLOR_PAIR(COLOR_FOR_IP));
          break;
        }

        /* could it be a timestamp ? */
        str = int_to_time(val);
        if (str) {
          wattron(window, COLOR_PAIR(COLOR_FOR_TIME));
          wprintw(window, " %s", str);
          wattroff(window, COLOR_PAIR(COLOR_FOR_TIME));

          wprintw(window, " (");
          wattron(window, COLOR_PAIR(COLOR_FOR_INT));
          wprintw(window, "%"PRIu64, val);
          wattroff(window, COLOR_PAIR(COLOR_FOR_INT));
          wprintw(window, ")");
          break;
        }

        /* otherwise just print it as a number */
        wattron(window, COLOR_PAIR(COLOR_FOR_INT));
        wprintw(window, " %"PRIu64, val);
        wattroff(window, COLOR_PAIR(COLOR_FOR_INT));
        break;
      }

      /* could it be a numberstring? */
      str = octet_to_numberstring(object);
      if (str) {
        wattron(window, COLOR_PAIR(COLOR_FOR_STRING));
        wprintw(window, " %s", str);
        wattroff(window, COLOR_PAIR(COLOR_FOR_STRING));
        break;
      }

      /* is it printable as a string? */
      if (octet_is_printable(object)) {
        wattron(window, COLOR_PAIR(COLOR_FOR_STRING));
        wprintw(window, " \"%.*s\"", object->data.string.len, object->data.string.value);
        wattroff(window, COLOR_PAIR(COLOR_FOR_STRING));
        break;
      }

      /* otherwise print as hex */
      wattron(window, COLOR_PAIR(COLOR_FOR_HEX));
      wprintw(window, " 0x");
      for (i = 0; i < object->data.string.len; ++i) {
        if (i >= 20) {
          wattroff(window, COLOR_PAIR(COLOR_FOR_HEX));
          wprintw(window, "...");
          break;
        }
        wprintw(window, "%.2x", object->data.string.value[i]);
      }
      wattroff(window, COLOR_PAIR(COLOR_FOR_HEX));

    } break;

    case TYPE_PRINTABLE_STRING:
    case TYPE_IA5_STRING:
    case TYPE_UTF8_STRING:
      wattron(window, COLOR_PAIR(COLOR_FOR_STRING));
      wprintw(window, " \"%.*s\"", object->data.string.len, object->data.string.value);
      wattroff(window, COLOR_PAIR(COLOR_FOR_STRING));
      break;

    default:
      print_error("Type not supported:\n");
      print_definition(object->type, 0);
      exit(1);
  }
}

static void dump_object_tree(Object *object, int indent, int max_indent) {
  Object **d;

  switch (object->type->type) {
    case TYPE_CHOICE:
      if (object->name)
        printf(TABS "%s%s%s\n", TAB(indent), NORMAL, object->name, NORMAL);
      if (!max_indent || indent+1 < max_indent)
        dump_object_tree(object->data.choice.value, indent+1, max_indent);
      break;
    case TYPE_SEQUENCE:
      if (object->name)
        printf(TABS "%s%s%s\n", TAB(indent), NORMAL, object->name, NORMAL);
      if (!max_indent || indent+1 < max_indent)
        array_foreach(object->data.sequence.values, d)
          dump_object_tree(*d, indent+1, max_indent);
      break;

    case TYPE_LIST:
      if (object->name)
        printf(TABS "%s%s%s\n", TAB(indent), NORMAL, object->name, NORMAL);
      if (!max_indent || indent+1 < max_indent)
        array_foreach(object->data.sequence.values, d)
          dump_object_tree(*d, indent+1, max_indent);
      break;

    case TYPE_BOOLEAN:
      if (object->name)
        printf(TABS "%s%s%s ", TAB(indent), NORMAL, object->name, NORMAL);
      printf("%s%s%s\n", MAGENTA, object->data.integer.value ? "TRUE" : "FALSE", NORMAL);
      break;

    case TYPE_INTEGER:
      if (object->name)
        printf(TABS "%s%s%s ", TAB(indent), NORMAL, object->name, NORMAL);
      printf("%s%"PRIu64 "%s\n", GREEN, object->data.integer.value, NORMAL);
      break;

    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING: {
      /* This code is weird because almost everything we have at CICS is encoded as OCTET STRINGs;
       * ip addresses, numberstrings, numbers, milliseconds etc,
       * so we employ some heuristics on common cases to try to figure out what the value really is
       */
      int i;
      const char *str;

      if (object->name)
        printf(TABS "%s%s%s ", TAB(indent), NORMAL, object->name, NORMAL);

      /* if it's small, it might be something special */
      if (object->data.string.len <= 8) {
        u64 val = octet_to_int(object);

        if (octet_is_ip_address(object)) {
          printf("%s%"PRIu64 ".%"PRIu64 ".%"PRIu64 ".%"PRIu64 "%s", CYAN, (val & 0xFF000000) >> 24, (val & 0xFF0000) >> 16, (val & 0xFF00) >> 8, val & 0xFF, NORMAL);
          break;
        }

        /* could it be a timestamp ? */
        str = int_to_time(val);
        if (str) {
          printf("%s%s%s", GREEN, str, NORMAL);
          printf(" (%s%"PRIu64 "%s)", MAGENTA, val, NORMAL);
          break;
        }

        /* otherwise just print it as a number */
        printf("%s%"PRIu64"%s", GREEN, val, NORMAL);
        break;
      }

      /* could it be a numberstring? */
      str = octet_to_numberstring(object);
      if (str) {
        printf("%s%s%s", MAGENTA, str, NORMAL);
        break;
      }

      /* is it printable as a string? */
      if (octet_is_printable(object)) {
        printf(" (%s" "\"%.*s\"" "%s)", CYAN, object->data.string.len, object->data.string.value, NORMAL);
        break;
      }

      /* otherwise print as hex */
      printf("%s0x", BLUE);
      for (i = 0; i < object->data.string.len; ++i) {
        if (i >= 20) {
          printf("%s...", NORMAL);
          break;
        }
        printf("%.2x", object->data.string.value[i]);
      }
      printf("%s", NORMAL);

    } break;

    case TYPE_PRINTABLE_STRING:
    case TYPE_IA5_STRING:
    case TYPE_UTF8_STRING:
      if (object->name)
        printf(TABS "%s%s%s ", TAB(indent), NORMAL, object->name, NORMAL);
      printf("%s\"%.*s\"%s\n", CYAN, object->data.string.len, object->data.string.value, NORMAL);
      break;

    default:
      print_error("Type not supported:\n");
      print_definition(object->type, 0);
      exit(1);
  }
}

static void dump_all(ASN1_Typedef *start_type) {
  while (Global.data < array_end(Global.data_begin)) {
    Object *o = decode(start_type->type, start_type->name, 0, 0, 0);
    dump_object_tree(o, 0, 0);
  }
}

int main(int argc, const char **argv) {
  ASN1_Typedef *start_type;
  const char **input_files;
  const char *binary_file;
  const char *type_name;
  int num_input_files;
  int interactive = 0;
  int num_opts = 0;
  int i;

  init_colors();

  /* skip the first arg.. */
  --argc, ++argv;

  if (argc == 0)
    print_usage(), exit(1);

  for (i = 0; i < argc; ++i) {
    if (is_option(argv[i])) {
      if (strcmp(argv[i], "--interactive") == 0)
        interactive = 1;
      else {
        printf("Unknown option \"%s\"\n", argv[i]+2);
        print_usage(), exit(1);
      }
      ++num_opts;
    }
  }

  if (argc - num_opts < 3)
    print_usage(), exit(1);

  /* get our args */
  i = 0;
  /* input files */
  input_files = malloc(sizeof(*input_files) * (argc - num_opts - 2));
  for (num_input_files = 0; num_input_files < (argc - num_opts - 2); ++i)
    if (!is_option(argv[i]))
      input_files[num_input_files++] = argv[i];

  /* binary */
  for (; i < argc; ++i) {
    if (!is_option(argv[i])) {
      binary_file = argv[i++];
      break;
    }
  }

  /* type name */
  for (; i < argc; ++i) {
    if (!is_option(argv[i])) {
      type_name = argv[i++];
      break;
    }
  }

  Global.types = asn1_parse(input_files, num_input_files);
  if (!Global.types)
    die("Failed parsing\n");

  start_type = get_type_by_name(type_name);
  if (!start_type)
    die("Found no type '%s' in definition\n", type_name);

  /* read file */

  Global.data_begin = Global.data = file_get_contents(binary_file);
  if (!Global.data)
    die("Failed to read contents of %s: %s\n", binary_file, strerror(errno));


  /* interactive mode ? */
  if (interactive)
    run_interactive(start_type);
  else
    dump_all(start_type);
}
