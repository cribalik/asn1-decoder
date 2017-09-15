#include "defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum {
	BERTAG_EOC               = 0,
	BERTAG_BOOLEAN           = 1,
	BERTAG_INTEGER           = 2,
	BERTAG_BIT               = 3,
	BERTAG_OCTET             = 4,
	BERTAG_NULL              = 5,
	BERTAG_OBJECT_IDENTIFIER = 6,
	BERTAG_OBJECT_DESCRIPTOR = 7,
	BERTAG_EXTERNAL          = 8,
	BERTAG_REAL              = 9,
	BERTAG_ENUMERATED        = 10,
	BERTAG_EMBEDDED          = 11,
	BERTAG_UTF8STRING        = 12,
	BERTAG_RELATIVE_OID      = 13,
	BERTAG_Reserved_1        = 14,
	BERTAG_Reserved_2        = 15,
	BERTAG_SEQUENCE          = 16,
	BERTAG_SET               = 17,
	BERTAG_NUMERICsTRING     = 18,
	BERTAG_PRINTABLEsTRING   = 19,
	BERTAG_T61STRING         = 20,
	BERTAG_VIDEOTEXSTRING    = 21,
	BERTAG_IA5STRING         = 22,
	BERTAG_UTCTIME           = 23,
	BERTAG_GENERALIZEDTIME   = 24,
	BERTAG_GRAPHICsTRING     = 25,
	BERTAG_VISIBLESTRING     = 26,
	BERTAG_GENERALSTRING     = 27,
	BERTAG_UNIVERSALSTRING   = 28,
	BERTAG_CHARACTER         = 29,
	BERTAG_BMPSTRING         = 30
} BerTag;

static int tag_number_read(FILE *f, unsigned char c) {
  int tag_number;

  if (c != 0x1f)
    return c;

	tag_number = 0;
  for (;;) {
    c = fgetc(f);
    if (c == EOF) {
      fprintf(stderr, "Unexpected end of input stream\n");
      return -1;
    }

    tag_number <<= 7;
    tag_number |= (c & 0x7f);

    if (!(c & 0x80))
      return tag_number;
  }
  return tag_number;
}

#define LENGTH_INDEFINITE -2
static int length_read(FILE *f) {
  unsigned char c;
  c = fgetc(f);
  if (c == EOF)
    return -1;

  /* definite short ? */
  if (!(c & 0x80))
    return c;

  /* reserved ? */
  if ((c & 0x7F) == 0x7F) {
    fprintf(stderr, "Reserved length field\n");
    return -1;
  }

  /* definite long ? */
  if (c & 0x7F) {
    int i, result;

    i = c & 0x7F;
    result = 0;
    while (i--) {
      c = fgetc(f);
      if (c == EOF) {
        fprintf(stderr, "Unexpected end of stream\n");
        return -1;
      }

      result <<= 8;
      result |= c;
    }
    return result;
  }

  /* indefinite */
  return LENGTH_INDEFINITE;
}

static void decode_file(const char *filename, Typedef *start_type) {
  FILE *f;

  f = fopen(filename, "rb");
  if (!f)
    goto done;

  for (;;) {
    unsigned char c, identifier_class, primitive, tag_number;
    int length;
    
    c = fgetc(f);
    if (c == EOF)
      goto done;

    identifier_class = c & 0xC0;
    primitive = c & 0x20;
    tag_number = tag_number_read(f, c);
    if (tag_number == -1)
      goto done;

    length = length_read(f);
    if (length == -1)
      goto done;
    if (length == LENGTH_INDEFINITE) {
      fprintf(stderr, "Indefinite length not supported\n");
      goto done;
    }

    printf("length: %i, identifier_class: %i, primitive: %i, tag_number: %i\n", length, identifier_class, primitive, tag_number);
  }
  done:
  if (f)
    fclose(f);
}

int main(int argc, const char **argv) {
  Typedef **types;
  int num_types;
  Typedef *start_type;
  int i,err;

  if (argc < 4) {
    fprintf(stderr, "Usage: decoder ASN1FILE... FILE TYPENAME \n");
    exit(1);
  }

  err = parse(argv+1, argc-3, &types, &num_types);
  if (err)
    return 1;

  start_type = 0;
  for (i = 0; i < num_types; ++i) {
    if (strcmp(types[i]->header.name, argv[argc-1]) == 0) {
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

