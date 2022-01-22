/*************************************************
*           Music|XML input for PMW              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */

/* Error handling routines for XML */

#include "pmw.h"



/*************************************************
*             Static variables                   *
*************************************************/

static int  error_count = 0;
static int  warning_count = 0;
static BOOL suppress_warnings = FALSE;
static tree_node *done_warnings = NULL;



/*************************************************
*            Texts and return codes              *
*************************************************/

typedef struct {
  char ec;
  const char *text;
} error_struct;

typedef struct {
  int number;
  BOOL seen;
} once_struct;

/* These warnings are output only once as long as the text is identical (some
of them have variable text). */

static once_struct once_only[] = {
  { ERR43, FALSE },
  { ERR44, FALSE },
  { ERR45, FALSE },
  { ERR46, FALSE},
  { ERR47, FALSE},
  { ERR50, FALSE},
  { ERR51, FALSE},
  { -1, FALSE }
};


static error_struct error_data[] = {

/* 0-4 */
{ ec_failed,  "failed to open %s (%s): %s" },
{ ec_failed,  "malloc failed: requested %d bytes" },
{ ec_major,   "'>' expected after '</%s'" },
{ ec_major,   "nesting error: found '</%s>' before any other elements" },
{ ec_failed,  "elements too deeply nested" },
/* 5-9 */     
{ ec_major,   "nesting error: found '</%s>', expected '</%s'>" },
{ ec_major,   "expected '=' after '%s'" },
{ ec_major,   "expected \" or ' after '%s='" },
{ ec_major,   "missing terminating %c after %s=%c..." },
{ ec_major,   "expected '>' after '/' or '?' in element '%s'" },
/* 10 - 14 */ 
{ ec_major,   "missing '>' for element '%s'" },
{ ec_major,   "unsupported beat-unit \"%s\"" },
{ ec_failed,  "unexpected EOF while skipping processing instruction "
                "starting on line %d" },
{ ec_failed,  "unexpected EOF inside comment (started on line %d)" },
{ ec_failed,  "unexpected EOF inside element (started on input line %d)" },
/* 15-19 */   
{ ec_major,   "unclosed element%s at end of file" },
{ ec_failed,  "<score-partwise> not found and <score-timewise> not supported" },
{ ec_failed,  "missing %s" },
{ ec_failed,  "unrecognized note type '%s'" },
{ ec_major,   "part has no %s" },
/* 20 - 24 */ 
{ ec_failed,  "part \"%s\", listed in <part-list>, not found" },
{ ec_failed,  "too many staves (%d) - PMW maximum is 64" },
{ ec_major,   "unsupported mode \"%s\"" },
{ ec_major,   "missing, malformed, or invalid number \"%s\"" },
{ ec_major,   "missing <%s> element" },
/* 25 - 29 */ 
{ ec_major,   "unsupported clef sign \"%s\"" },
{ ec_warning, "unrecognized element in measure %d in part \"%s\"" },
{ ec_major,   "note contains <chord> but matching previous note not found" },
{ ec_major,   "internal error: %s" },
{ ec_failed,  "too many staves (%d) for a part" },
/* 30 - 34 */ 
{ ec_major,   "unrecognized accidental \"%s\"" },
{ ec_major,   "unrecognized measure number type \"%s\"" },
{ ec_major,   "missing attribute \"%s\"" },
{ ec_major,   "unsupported %s repeat at location %s" },
{ ec_major,   "%s are not currently supported" },
/* 35 - 39 */
{ ec_major,   "unrecognized dynamic \"%s\"" },
{ ec_major,   "non-start <part-group> %d has no starting predecessor" },
{ ec_major,   "<page-layout> ignored owing to missing data" },
{ ec_warning, "only equal margins for both odd and even are supported in <page-margins>" },
{ ec_major,   "internal error: missing start of slur or line" },
/* 40 - 44 */ 
{ ec_failed,  "internal error: insert past end of text (%ld, %ld)" },
{ ec_warning, "for a %c clef, only line %s is supported (%d ignored)" },
{ ec_warning, "<line>%d</line> ignored for a percussion clef" },
{ ec_warning, "%s \"%s\" is not supported" },
{ ec_major,   "\"%s\" line end on <bracket> %s staff is not supported" },
/* 45 - 49 */ 
{ ec_warning, "PMW does not support \"%s\" notehead within a chord" },
{ ec_warning, "PMW does not support different %s for rehearsal marks" },
{ ec_warning, "PMW does not support different sizes for the same staff" },
{ ec_major,   "wedge type \"stop\" without matching start" },
{ ec_warning, "PMW does not support nested wedges (hairpins)" },
/* 50 - 54 */ 
{ ec_warning, "PMW does not support <credit> for pages other than 1 or 2" },
{ ec_warning, "Different parts set different number of measures in a system, "
              "or systems on a page" },
{ ec_major,   "Unrecognized %s \"%s\"" },
{ ec_warning, "PMW does not support <backup> between tied notes: tie "
                   "discarded" },
{ ec_warning, "XML entity \"%.*s;\" %s" },
/* 55 - 59 */ 
{ ec_warning, "Ending number greater than %d is not supported" },
{ ec_warning, "Unknown accidental \"%s\" in key signature" },
{ ec_failed,  "Too many non-standard custom keys required" },
{ ec_major,   "Multi-staff chords are supported only for 2-staff parts" },
};

#define ERROR_MAXERROR (int)(sizeof(error_data)/sizeof(error_struct))



/*************************************************
*              Error message generator           *
*************************************************/

/* This function outputs an error or warning message, and may abandon the
process if the error is sufficiently serious, or if there have been too many
less serious errors. If there are too many warnings, subsequent ones are
suppressed.

Arguments:
  n           error number
  ap          argument list to fill into message
  linenumber  line number if positive

Returns:      nothing; disastrous errors do not return
*/

static void
base_error(int n, va_list ap, uschar *elname, int linenumber)
{
int ec;
once_struct *onceptr = NULL;

if (n > ERROR_MAXERROR)
  {
  eprintf("** Unknown error number %d\n", n);
  ec = ec_failed;
  }
else
  {
  uschar buffer[256];
  (void)vsprintf(CS buffer, error_data[n].text, ap);

  for (onceptr = once_only; onceptr->number >= 0; onceptr++)
    {
    if (onceptr->number == n)
      {
      tree_node *tn;
      if (onceptr->seen)
        {
        tn = tree_search(done_warnings, buffer);
        if (tn != NULL) return;
        }
      else onceptr->seen = TRUE;
      tn = mem_get(sizeof(tree_node) + Ustrlen(buffer));
      tn->name = mem_copystring(buffer);
      (void)tree_insert(&done_warnings, tn);
      break;
      }
    }

  ec = error_data[n].ec;
  if (ec == ec_warning)
    {
    if (suppress_warnings) return;
    eprintf("** MusicXML warning: ");
    }
  else
    {
    if (ec > ec_warning) eprintf("** MusicXML error: ");
      else eprintf("** ");
    }
  eprintf("%s\n", buffer);
  }

va_end(ap);

if (linenumber > 0)
  {
  if (xml_read_done)
    eprintf("   Detected in element <%s> starting in line %d of %s\n",
      elname, linenumber, read_filename);
  else
    eprintf("   Detected near line %d of %s\n",
      linenumber, read_filename);
  }

if (ec == ec_warning)
  {
  warning_count++;
  if (warning_count > 40)
    {
    eprintf("** Too many warnings - subsequent ones suppressed\n");
    suppress_warnings = TRUE;
    }
  }

else if (ec > ec_warning)
  {
  error_count++;
  if (error_count > 40)
    {
    eprintf("** Too many errors\n** pmw processing abandoned\n");
    exit(EXIT_FAILURE);
    }
  }

if (ec >= ec_failed)
  {
  eprintf("** pmw XML processing abandoned\n");
  exit(EXIT_FAILURE);
  }

if (onceptr->seen) eprintf("** This %s is given only once;"
  " there may be other occurrences.\n",
  (ec == ec_warning)? "warning" : "error");

if (ec > xml_error_max) xml_error_max = ec;   /* Highest error code */
eprintf("\n");                  /* Blank before next output */
return;
}



/*************************************************
*     Error not related to a specific element    *
*************************************************/

/* This function is used during reading and for any general error messages.

Arguments:
  n           error number
  ...         arguments to fill into message

Returns:      nothing, but some errors do not return
*/

void
xml_error(int n, ...)
{
va_list ap;
va_start(ap, n);
base_error(n, ap, NULL, (xml_read_done? -1 : xml_read_linenumber));
}



/*************************************************
*     Error related to a specific element        *
*************************************************/

/* This function is used when the line number from a specifi element is to be
reported.

Arguments:
  i           the element item
  n           error number
  ...         arguments to fill into message

Returns:      nothing, but some errors do not return
*/

void
xml_Eerror(xml_item *i, int n, ...)
{
va_list ap;
va_start(ap, n);
base_error(n, ap, i->name, i->linenumber);
}

/* End of xml_error.c */
