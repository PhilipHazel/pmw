/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* File last edited: March 2022 */

/* This module contains definitions of structures that are used throughout the
XML modules. */

struct xml_item;                   /* For forward reference */

/* Structure for the built-in list of named XML entities */

typedef struct xml_entity_block {
  uschar  *name;
  uint32_t value;
} xml_entity_block;

/* Structure for table of supported elements */

typedef struct xml_elliststr {
  uschar *name;
  uschar **attrs;
} xml_elliststr;

/* This is the structure for attributes that hang off items that are elements */

typedef struct xml_attrstr {
  struct xml_attrstr *next;
  uschar name[NAMESIZE];
  uschar value[1];
} xml_attrstr;

/* This structure contains a string of characters. It is used for raw input
data (hanging off a #TEXT item). */

typedef struct xml_textblock {
  struct xml_textblock *next;    /* Chain for paragraphs and formatted lines */
  struct xml_textblock *lastin;  /* Last input textblock in an output one */
  int length;                    /* Length of string */
  uschar string[1];              /* The string data */
} xml_textblock;


/* The input file is read into a chain of items. */

typedef struct xml_item {
  struct xml_item *next;
  struct xml_item *prev;
  struct xml_item *partner;
  int linenumber;
  int flags;
  uschar name[NAMESIZE];
  union {
    xml_attrstr *attr;
    uschar *string;
    xml_textblock *txtblk;
  } p;
} xml_item;


/* Structure for bit tables for debugging */

typedef struct bit_table {
  uschar *name;
  unsigned int bit;
} bit_table;


/* Structure for a list of parts */

typedef struct xml_part_data {
  struct xml_part_data *next;
  xml_item *part;
  int measure_count;
  int stave_count;
  int stave_first;
  int divisions;
  int noprint_before;
  BOOL has_lyrics;
  uschar *id;
  xml_item *name;
  xml_item *name_display;
  xml_item *abbreviation;
  xml_item *abbreviation_display;
} xml_part_data;


/* Structure for a list of part groups */

typedef struct xml_group_data {
  struct xml_group_data *next;
  struct xml_group_data *prev;
  xml_item *name;
  xml_item *name_display;
  xml_item *abbreviation;
  xml_item *abbreviation_display;
  uschar *group_symbol;
  uschar *group_barline;
  int number;
  int first_pstave;
  int last_pstave;
} xml_group_data;


/* End of xml_structs.h */
