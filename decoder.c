/* TODO:
 * AST for integer enum specs
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

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define NORMAL "\x1B[39m"

typedef unsigned long long u64;

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
} Global;

int strstri(const char *needle, const char *haystack) {
  int l1, l2, i,j;
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
  printf(YELLOW "Debug: ");
  vprintf(fmt, args);
  printf(NORMAL);
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
  printf(RED "\n\nError at byte %li: ", Global.data - Global.data_begin);
  vprintf(fmt, args);
  printf(NORMAL);
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

static void check_end() {
  if (Global.data > array_last(Global.data_begin))
    die("Unexpected end of input stream\n");
}

static unsigned char next() {
  check_end();
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

static int decode_type(ASN1_Type *type, char *name, BerIdentifier *bi, unsigned char *end, int indent) {
  BerIdentifier ber_identifier;
  unsigned char *start;

  start = Global.data;

  switch (type->type) {
    case TYPE_CHOICE: {
      Tag *tag;
      int len;

      printf(TABS NORMAL "%s\n" NORMAL, TAB(indent), name);

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

      decode_type(tag->type, tag->name,
                  &ber_identifier,
                  end,
                  indent+1);
    } break;

    case TYPE_SEQUENCE: {
      Tag *tag, *next;
      int len;
      unsigned char *item_end;
      int item_length, first = 1;

      printf(TABS NORMAL "%s\n" NORMAL, TAB(indent), name);

      if (Global.data == end)
        break;

      ber_identifier = bi ? *bi : ber_identifier_read();

      next = type->sequence.items;

      for (; Global.data < end;) {
        print_debug("%lli\n", end - Global.data);

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

        decode_type(tag->type, tag->name, ber_identifier.pc == BER_PRIMITIVE ? &ber_identifier : 0, item_end, indent+1);
      }

      if (Global.data != end)
        die("Expected to read %i bytes from sequence, but it was of size %i\n", end-start, Global.data-start);
      printf(NORMAL);
    } break;

    case TYPE_LIST: {
      char item_name[16];
      int i, item_length, first = 1;
      unsigned char *item_end;

      printf(TABS NORMAL "%s\n" NORMAL, TAB(indent), name);

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

        printf(TABS YELLOW "item #%i" NORMAL, TAB(indent+1), i);
        decode_type(type->list.item_type, "", 0, item_end, indent+1);
      }
      if (Global.data != end)
        die("List should be %i long, but was at least %i\n", end-start, Global.data-start);
    } break;

    case TYPE_BOOLEAN: {
      int b;

      if (end - Global.data != 1)
        die("Length of boolean was not 1, but %i\n", end - Global.data);

      b = next();

      printf(TABS NORMAL "%s " MAGENTA "%s\n" NORMAL, TAB(indent), name, b ? "TRUE" : "FALSE");
    } break;

    case TYPE_INTEGER: {
      u64 i;
      int len;

      /* TODO: handle enumdecls */
      printf(TABS NORMAL "%s " NORMAL, TAB(indent), name);

      len = end - Global.data;

      for (i = 0; len; --len)
        i <<= 8, i |= next();
      printf(GREEN "%llu\n" NORMAL, i);
    } break;

    case TYPE_OCTET_STRING:
    case TYPE_BIT_STRING: {
      /* This code is weird because almost everything we have at CICS is encoded as OCTET STRINGs;
       * ip addresses, numberstrings, numbers, milliseconds etc,
       * so we employ some heuristics on common cases to try to figure out what the value really is
       */
      int i, printable, len;
      time_t t;
      struct tm *time;
      unsigned char *data;
      char date[32];


      printf(TABS NORMAL "%s " NORMAL, TAB(indent), name);

      len = end - Global.data;
      data = Global.data;

      /* polystar special sauce */
      if (strcmp(name, "cdrData") == 0) {
        ASN1_Typedef *xdr_type;
        putchar('\n');
        array_find(Global.types, xdr_type, strcmp(xdr_type->name, "XDR-TYPE") == 0);
        decode_type(xdr_type->type, xdr_type->name, 0, end, indent+1);
        break;
      }

      /* if it's small, it might be something special */
      if (len <= 8) {
        u64 val = 0;
        for (i = 0; i < len; ++i)
          val <<= 8, val |= data[i];

        /* is it an ip address ? */
        /* TODO: ipv6 */
        if (len == 4 && (
            strstri("ipaddr", name) ||
            strstri("ip", name))) {
          printf(CYAN "%llu.%llu.%llu.%llu" NORMAL, (val & 0xFF000000) >> 24, (val & 0xFF0000) >> 16, (val & 0xFF00) >> 8, val & 0xFF);
          goto print_done;
        }

        /* could it be a timestamp ? */
        t = val/1000;
        time = localtime(&t);
        if (time->tm_year > 90 && time->tm_year < 150) {
          strftime(date, sizeof(date)-1, "%Y-%m-%d %H:%M:%S", time);
          sprintf(date+19, ".%llu", val % 1000);
          printf(GREEN "%s" NORMAL, date);

          printf(" (" MAGENTA "%lli" NORMAL ")", val);
          goto print_done;
        }

        /* is it just a small readable value ? */
        if (val <= 1000000) {
          printf(GREEN "%lli" NORMAL, val);
          goto print_done;
        }
      }

      /* could it be a numberstring? */
      if (len <= 8) {
        char number[64];
        char *s = number;
        for (i = 0; i < len; ++i) {
          unsigned char lo,hi,c;

          c = data[i];
          lo = c & 0xf;
          hi = (c & 0xf0) >> 4;

          if (lo != 0xf && lo > 9)
            goto skip_numberstring;
          if (lo != 0xf)
            *s++ = '0'+lo;

          if (hi != 0xf && hi > 9)
            goto skip_numberstring;
          if (hi != 0xf)
            *s++ = '0'+hi;
        }
        *s = 0;
        printf(MAGENTA "%s" NORMAL, number);
        goto print_done;

        skip_numberstring:;
      }

      /* is it printable as a string? */
      printable = 1;
      for (i = 0; i < len; ++i)
          printable &= isprint(data[i]) || IS_UTF8_TRAIL(data[i]);
      if (printable) {
        printf(" (" CYAN "\"%.*s\"" NORMAL ")", len, data);
        goto print_done;
      }

      /* otherwise print as hex */
      printf(BLUE "0x");
      for (i = 0; i < len; ++i) {
        if (i >= 20) {
          printf(NORMAL "...");
          break;
        }
        printf("%.2x", data[i]);
      }
      printf(NORMAL);

      print_done:
      Global.data = end;
      printf(NORMAL "\n");
    } break;

    case TYPE_PRINTABLE_STRING:
    case TYPE_IA5_STRING:
    case TYPE_UTF8_STRING: {
      int len;

      printf(TABS NORMAL "%s " NORMAL, TAB(indent), name);

      len = end - Global.data;

      printf(CYAN "\"%.*s\"\n" NORMAL, len, Global.data);
      Global.data = end;
    } break;

    default:
      print_error("Type not supported:\n");
      print_definition(type, 0);
      exit(1);
  }

  return Global.data - start;
}

int main(int argc, const char **argv) {
  ASN1_Typedef *start_type;
  int i;

  if (argc < 4) {
    printf("Usage: decoder ASN1FILE... BINARY TYPENAME \n");
    exit(1);
  }

  Global.types = asn1_parse(argv+1, argc-3);
  if (!Global.types) {
    die("Failed parsing\n");
    exit(1);
  }

  start_type = 0;
  array_find(Global.types, start_type, strcmp(start_type->name, argv[argc-1]) == 0);
  if (!start_type) {
    die("Found no type '%s' in definition\n", argv[argc-1]);
    exit(1);
  }

  /* read file */
  Global.data_begin = Global.data = file_get_contents(argv[argc-2]);
  if (!Global.data) {
    die("Failed to read contents of %s: %s\n", argv[argc-2], strerror(errno));
    exit(1);
  }

  while (Global.data < array_end(Global.data_begin)) {
    decode_type(start_type->type, start_type->name, 0, 0, 0);
  }
}

