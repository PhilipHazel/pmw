/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: January 2022 */
/* This file last modified: April*/

/* This module contains XML debugging functions. */

#include "pmw.h"



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
  if (*s >= 32 && *s < 127) eprintf("%c", *s);
  else if (*s == '\n') eprintf("\\n");
  else if (*s == '\\') eprintf("\\");
  else eprintf("\\x%02x", *s);
  }
eprintf("%s", post);
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
eprintf("----- Item list %s -----\n", when);
for (i = item_list; i != NULL; i = i->next)
  {
  if (i->next != NULL && i->next->prev != i)
    {
    eprintf("*** Chain error: i=%p next=%p next->prev=%p\n",
      (void *)(i), (void *)(i->next), (void *)(i->next->prev));
    eprintf("*** name=%s nextname=%s\n", i->name, i->next->name);
    }

  /* The anchor item for the whole shebang. */

  if (Ustrcmp(i->name, "#") == 0) eprintf("#Anchor\n");

  /* An input text item; there can be dummies, inserted when very large
  paragraphs are split. */

  else if (Ustrcmp(i->name, "#TEXT") == 0)
    {
    xml_textblock *tb = i->p.txtblk; 
    if (tb == NULL) eprintf("Dummy text block\n");
      else debug_print_string(tb->string, tb->length, "\n");
    }

  /* A source file name item */

  else if (Ustrcmp(i->name, "#FILENAME") == 0)
    {
    eprintf("<#FILENAME> %s\n", i->p.string);
    }

  /* An unknown #-item is an internal error. */

  else if (i->name[0] == '#')
    {
    eprintf("** Unknown special item %s\n", i->name);
    }

  /* An ending item */

  else if (i->name[0] == '/')
    {
    xml_item *p = i->partner;
    if (p == NULL)
      eprintf("***bad end: no partner\n");
    else
      eprintf("</%s>\n", p->name);
    }

  /* A starting item */

  else
    {
    xml_attrstr *p;
    eprintf("<%s", i->name);
    for (p = i->p.attr; p != NULL; p = p->next)
      {
      eprintf(" %s='%s'", p->name, p->value);
      }
    if (i->partner == i) eprintf((i->name[0] == '?')? "?" : "/");
    eprintf(">\n");
    }
  }
eprintf("----- End of item list -----\n");   
}


/* End of xml_debug.c */
