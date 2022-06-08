/*************************************************
*        Header for MusicXML input for PMW       *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* File last edited: April 2022 */

/* This is a header for the optional MusicXML processing code. */

/* These are some parameters that specify sizes of things in the code. They
must appear before including the local headers. */

#define LINEBUFSIZE         1024
#define NAMESIZE              32
#define NESTSTACKSIZE        100

/* These values do not necessarily have to appear before including the local
headers, but they might as well be together with those above. */

#define LAYOUTLISTMIN        256
#define STAVEBUFFERMIN     10240
#define PARTSTAFFMAX          10

/* Macro to simplify checking the value of an attribute.
  a = item whose attribute is to be checked
  b = name of attribute
  c = default value for missing attribute
  d = TRUE for error if missing
  e = value to be compared
*/

#define ISATTR(a,b,c,d,e) \
  (Ustrcmp(xml_get_attr_string(a,US b,US c,d),e) == 0)

/* Stave coupling settings */

enum { COUPLE_NOT, COUPLE_UP, COUPLE_DOWN };

/* Include the other XML header files */

#include "xml_structs.h"
#include "xml_globals.h"
#include "xml_functions.h"

/* End of xml.h */
