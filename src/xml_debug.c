/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: January 2022 */
/* This file last modified: January 2022 */

/* This module contains XML debugging functions. */

#include "pmw.h"


/*************************************************
*             Debug printing function            *
*************************************************/

/* Pretty obvious really. Look just like printf(), but prints to stderr.

Arguments:
  format        a format string
  ...           data for same

Returns:        nothing
*/

void
xml_debug_printf(const char *format, ...)
{
va_list ap;
if (xml_debug_need_nl) fprintf(stderr, "\n");
xml_debug_need_nl = FALSE;
va_start(ap, format);
(void)vfprintf(stderr, format, ap);
va_end(ap);
}



/*************************************************
*              Print a string                    *
*************************************************/

/* Non-printing characters are converted to escapes.

Arguments:
  s           the string
  len         the length of the string
  post        a string to print afterwards

Returns:      nothing
*/

static void
debug_print_string(uschar *s, int len, const char *post)
{
for (; len-- > 0; s++)
  {
  if (*s >= 32 && *s < 127) xml_debug_printf("%c", *s);
  else if (*s == '\n') xml_debug_printf("\\n");
  else if (*s == '\\') xml_debug_printf("\\");
  else xml_debug_printf("\\x%02x", *s);
  }
xml_debug_printf("%s", post);
}


/*************************************************
*               Print a textblock                *
*************************************************/

/* This function prints the contents of a textblock, showing its font
characteristics, if present.

Argument:    the textblock
Returns:     nothing
*/

static void
debug_print_textblock(xml_textblock *tb)
{
// vfontstr *vf = tb->vfont;

//if (vf != NULL)
//  {
//  xml_debug_printf("{%s,%s,", family_names[vf->family], type_names[vf->type]);
//  xml_debug_printfixed(vf->size, "}");
//  }
//else

debug_print_string(tb->string, tb->length, "\n");
}



/*************************************************
*             Output an item list                *
*************************************************/

/* This function scans an item list and writes it out. It's called from
various places when the appropriate debugging switches are set.

Arguments:
  item_list  the start of the list
  when       text string for heading, indicating where called from

Returns:     nothing
*/

void
xml_debug_print_item_list(xml_item *item_list, const char *when)
{
xml_item *i;
xml_debug_printf("----- Item list %s -----\n", when);
for (i = item_list; i != NULL; i = i->next)
  {
  if (i->next != NULL && i->next->prev != i)
    {
    xml_debug_printf("*** Chain error: i=%p next=%p next->prev=%p\n",
      (void *)(i), (void *)(i->next), (void *)(i->next->prev));
    xml_debug_printf("*** name=%s nextname=%s\n", i->name, i->next->name);
    }

  /* The anchor item for the whole shebang. */

  if (Ustrcmp(i->name, "#") == 0) xml_debug_printf("#Anchor\n");

  /* An input text item; there can be dummies, inserted when very large
  paragraphs are split. */

  else if (Ustrcmp(i->name, "#TEXT") == 0)
    {
    if (i->p.txtblk == NULL)
      xml_debug_printf("Dummy text block\n");
    else debug_print_textblock(i->p.txtblk);
    }

  /* A source file name item */

  else if (Ustrcmp(i->name, "#FILENAME") == 0)
    {
    xml_debug_printf("<#FILENAME> %s\n", i->p.string);
    }

  /* An unknown #-item is an internal error. */

  else if (i->name[0] == '#')
    {
    xml_debug_printf("** Unknown special item %s\n", i->name);
    }

  /* An ending item */

  else if (i->name[0] == '/')
    {
    xml_item *p = i->partner;
    if (p == NULL)
      xml_debug_printf("***bad end: no partner\n");
    else
      xml_debug_printf("</%s>\n", p->name);
    }

  /* A starting item */

  else
    {
    xml_attrstr *p;
    xml_debug_printf("<%s", i->name);
    for (p = i->p.attr; p != NULL; p = p->next)
      {
      xml_debug_printf(" %s='%s'", p->name, p->value);
      }
    if (i->partner == i) xml_debug_printf((i->name[0] == '?')? "?" : "/");
    xml_debug_printf(">\n");
    }
  }
xml_debug_printf("----- End of item list -----\n");   
}


/* End of xml_debug.c */
