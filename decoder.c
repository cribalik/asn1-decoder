#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static struct {
  Array(unsigned char) data_begin;
  unsigned char *data_end;
  unsigned char *data;
} Global;

typedef enum {
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
  BER_TAG_PRINTABLEsTRING   = 19,
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
} BerTag;

typedef enum {
  BER_IDENTIFIER_CLASS_UNIVERSAL = 0,
  BER_IDENTIFIER_CLASS_APPLICATION = 1,
  BER_IDENTIFIER_CLASS_CONTEXT_SPECIFIC = 2,
  BER_IDENTIFIER_CLASS_PRIVATE = 3
} BerIdentifierClass;

static void check_end() {
  if (Global.data >= Global.data_end) {
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

static int ber_length_read() {
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

  /* indefinite */
  return LENGTH_INDEFINITE;
}

static int ber_identifier_read(BerIdentifierClass *identifier_class, int *is_primitive, int *tag_number) {
  unsigned char c;

  c = next();

  *identifier_class = (c & 0xC0) >> 6;
  *is_primitive = !(c & 0x20);
  *tag_number = ber_tag_number_read(c);

  if (*tag_number == -1)
    exit(1);
  return 0;
}

static void print_type(ASN1_Typedef *type) {
  if (!type)
    return;
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

  f = fopen(filename, "rb");
  if (!f)
    return 0;

  data = 0;
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

static void decode_file(const char *filename, ASN1_Typedef *start_type) {
  Global.data_begin = Global.data = file_get_contents(filename);
  Global.data_end = Global.data_begin + array_len(Global.data_begin);
  if (!Global.data) {
    fprintf(stderr, "Failed to read contents of %s: %s\n", filename, strerror(errno));
    exit(1);
  }

  for (;;) {
    int is_primitive, tag_number, length;
    BerIdentifierClass identifier_class;
    
    ber_identifier_read(&identifier_class, &is_primitive, &tag_number);
    length = ber_length_read();
    if (length == LENGTH_INDEFINITE) {
      fprintf(stderr, "Indefinite length not supported\n");
      exit(1);
    }

    printf("length: %i, identifier_class: %i, is_primitive: %i, tag_number: %i\n", length, identifier_class, is_primitive, tag_number);
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

  decode_file(argv[argc-2], start_type);
}

