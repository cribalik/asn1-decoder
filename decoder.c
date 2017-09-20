#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define NORMAL "\x1B[39m"

#define IS_UTF8_TRAIL(c) (((c)&0xC0) == 0x80)

#if 0
#define DEBUG(stmt) do {stmt;} while (0)
#else
#define DEBUG(stmt)
#endif

static struct {
  Array(unsigned char) data_begin;
  unsigned char *data;
} Global;

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

typedef struct {
  char is_primitive;
  BerIdentifierClass class;
  int tag_number;
} BerIdentifier;


static void check_end() {
  if (Global.data > array_last(Global.data_begin)) {
    fprintf(stderr, "Unexpected end of input stream\n");
    exit(1);
  }
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
  if ((c & 0x7F) == 0x7F) {
    fprintf(stderr, "Reserved length field\n");
    exit(1);
  }

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
  fprintf(stderr, RED "Indefinite length encoding not supported\n" NORMAL);
  exit(1);
  return -1;
}

static int ber_length_read() {
  int l = _ber_length_read();
  DEBUG(fprintf(stderr, YELLOW "debug: Length: %i\n" NORMAL, l));
  return l;
}

static BerIdentifier ber_identifier_read() {
  BerIdentifier i = {0};
  unsigned char c;

  c = next();

  i.class = (c & 0xC0) >> 6;
  i.is_primitive = !(c & 0x20);
  i.tag_number = ber_tag_number_read(c);

  if (i.tag_number == -1)
    exit(1);

  DEBUG(fprintf(stderr, YELLOW "debug: BerIdentifier = (class: %i, primitive: %i, tag number: %i)\n" NORMAL, i.class, i.is_primitive, i.tag_number));
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

static void print_definition(ASN1_Type *t, int indent) {
  switch (t->type) {
    case TYPE_UNKNOWN:
      printf("UNKNOWN");
      break;
    case TYPE_CHOICE: {
      Tag *tag;

      printf(TABS "CHOICE\n", TAB(indent)); 
      array_foreach(t->choice.choices, tag) {
        printf(TABS "%s [%i] ", TAB(indent+1), tag->name, tag->id);
        print_definition(tag->type, indent+1);
        printf("\n");
      }
    } break;
    case TYPE_SEQUENCE: {
      Tag *tag;

      printf(TABS "SEQUENCE\n", TAB(indent)); 
      array_foreach(t->sequence.items, tag) {
        printf(TABS "%s [%i] ", TAB(indent+1), tag->name, tag->id);
        print_definition(tag->type, indent+1);
        printf("\n");
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
      printf("list of: \t");
      t = t->list.item_type;
  }
  putchar('\n');
}


static void decode_type(BerIdentifier *ber_identifier_in, ASN1_Type *type, char *name, int indent) {
#define ber_identifier_get() (ber_identifier_in ? *ber_identifier_in : ber_identifier_read())

  BerIdentifier ber_identifier;

  printf(TABS "%-20s ", TAB(indent), name);
  
  switch (type->type) {
    case TYPE_CHOICE: {
      Tag *tag;

      ber_identifier = ber_identifier_get();

      array_find(type->choice.choices, tag, ber_identifier.tag_number == tag->id)

      if (!tag) {
        fprintf(stderr, "For CHOICE, BER tag number was %i, but no such choice exists.\n", ber_identifier.tag_number);
        fprintf(stderr, "Available tags:\n");
        print_definition(type, indent);

        exit(1);
      }

      printf(RED "CHOICE\n" NORMAL);
      decode_type(0, tag->type, tag->name, indent+1);
    } break;

    case TYPE_SEQUENCE: {
      int i;

      printf(RED "SEQUENCE\n" NORMAL);

      ber_length_read();

      for (i = 0; i < array_len(type->sequence.items); ++i) {
        Tag *tag;

        tag = type->sequence.items+i;

        ber_identifier = i == 0 ? ber_identifier_get() : ber_identifier_read();
        if (ber_identifier.tag_number != i) {
          fprintf(stderr, RED "\n\nTag number %i does not match the sequence index %i.\n" NORMAL, ber_identifier.tag_number, i);
          print_definition(type, 0);
          exit(1);
        }

        decode_type(&ber_identifier, tag->type, tag->name, indent+1);
      }
    } break;

    case TYPE_INTEGER: {
      int l;
      long i;

      ber_identifier = ber_identifier_get();
      l = ber_length_read();
      for (i = 0; l; --l)
        i <<= 8, i |= next();
      printf(GREEN "%li\n" NORMAL, i);
    } break;

    case TYPE_OCTET_STRING: {
      int len, i, printable;
      time_t t;
      struct tm *time;
      unsigned char *data;
      char date[32];

      ber_identifier = ber_identifier_get();
      len = ber_length_read();

      data = Global.data;

      /* at least print as hex */
      printf(BLUE "0x");
      for (i = 0; i < len; ++i) {
        if (i >= 20) {
          printf(NORMAL "...");
          break;
        }
        printf("%.2x", data[i]);
      }
      printf(NORMAL);

      /* is it printable as a string? */
      printable = 1;
      for (i = 0; i < len; ++i)
          printable &= isprint(data[i]) || IS_UTF8_TRAIL(data[i]);
      if (printable) {
        printf(" (" GREEN "\"%*.s\"" NORMAL ")", len, data);
        goto print_done;
      }

      /* convertable to int? */
      if (len <= 8) {
        long val = 0;
        for (i = 0; i < len; ++i)
          val <<= 8, val |= data[i];

        /* could it be a timestamp ? */
        t = val/1000;
        time = localtime(&t);
        strftime(date, sizeof(date)-1, "%Y-%m-%d %H:%M:%S", time);
        if (time->tm_year > 90 && time->tm_year < 150) {
          printf(" (" GREEN "%s" NORMAL ")", date);
          goto print_done;
        }
      }

      print_done:
      Global.data += len;
      printf(NORMAL "\n");
    } break;

    default:
      printf("\n\n" RED "Error: Type not supported:\n" NORMAL);
      print_definition(type, 0);
      exit(1);
  }
}

int main(int argc, const char **argv) {
  ASN1_Typedef **types;
  int num_types;
  ASN1_Typedef *start_type;
  int i,err;

  if (argc < 4) {
    fprintf(stderr, "Usage: decoder ASN1FILE... FILE TYPENAME \n");
    exit(1);
  }

  err = asn1_parse(argv+1, argc-3, &types, &num_types);
  if (err)
    return 1;

  start_type = 0;
  for (i = 0; i < num_types; ++i) {
    if (strcmp(types[i]->name, argv[argc-1]) == 0) {
      start_type = types[i];
      break;
    }
  }
  if (!start_type) {
    fprintf(stderr, "Found no type '%s' in definition\n", argv[argc-1]);
    return 1;
  }

  /* read file */
  Global.data_begin = Global.data = file_get_contents(argv[argc-2]);
  if (!Global.data) {
    fprintf(stderr, "Failed to read contents of %s: %s\n", argv[argc-2], strerror(errno));
    exit(1);
  }
  decode_type(0, start_type->type, start_type->name, 0);
}

