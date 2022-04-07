/*************************************************
*              MusicXML input reading            *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: January 2022 */
/* This file last modified: April 2022 */

/* This file contains the top-level function for MusicXML input files. */

#include "pmw.h"


/*************************************************
*             Static variables                   *
*************************************************/


static FILE *infile;
static uschar *linebuffer;



/*************************************************
*      Print unknown element/attribute tree      *
*************************************************/

/* Prints the tree of unknown or ignored elements and attributes, calling
itself recursively to get them in order.

Argument:    a tree node
Returns:     nothing
*/

static void
print_unknown_tree(tree_node *tn)
{
if (tn->left != NULL) print_unknown_tree(tn->left);
if (tn->name[0] == '+')
  {
  uschar *eptr = Ustrrchr(tn->name, ':');
  uschar *vptr = Ustrchr(tn->name, '=');
  if (vptr == NULL)
    fprintf(stderr, "'%.*s' in <%s>\n", (int)(eptr - tn->name - 1), tn->name + 1,
      eptr + 1);
  else
    fprintf(stderr, "'%.*s' value \"%.*s\" in <%s>\n", (int)(vptr - tn->name - 1),
      tn->name + 1, (int)(eptr - vptr - 1), vptr + 1, eptr + 1);
  }
else
  {
  fprintf(stderr, "<%s>\n", tn->name);
  }
if (tn->right != NULL) print_unknown_tree(tn->right);
}



/*************************************************
*            Process XML element list            *
*************************************************/

/* Called after reading and analyzing a MusicXML input file. */

static void
xml_process(void)
{

/* If movement_count == 0 we are processing a freestanding MusicXML file and
need to set up and initialize a movement. */

if (movement_count == 0)
  {
  movements_size += MOVTVECTOR_CHUNKSIZE;
  movements = realloc(movements, movements_size * sizeof(movtstr *));
  if (movements == NULL)
    error(ERR0, "re-", "movements vector", movements_size); /* Hard */
  premovt = &default_movtstr;
  movements[movement_count++] = curmovt = mem_get(sizeof(movtstr));
  read_init_movement(curmovt, xml_movt_unsetflags, xml_movt_setflags);
  }

/* Process the items. It is during this processing that various parameters such
as the magnification and font sizes are discovered. */

xml_do_heading();
xml_do_parts();

/* Deal with any explicit stave sizes. */

if (xml_set_stave_size)
  {
  int32_t *stavesizes = mem_get((MAX_STAVE+1)*sizeof(int32_t));
  memcpy(stavesizes, curmovt->stavesizes, (MAX_STAVE+1)*sizeof(int32_t));
  curmovt->stavesizes = stavesizes;
  for (int i = 1; i <= xml_pmw_stave_count; i++)
    if (xml_stave_sizes[i] > 0) stavesizes[i] = xml_stave_sizes[i];
  }

/* Deal with font sizes if there are any other than the default 10-point size.
For all except the default 10pt font, we unscale by the magnification so that,
when re-scaled by PMW they are the sizes specified in the XML. This sometimes
means that the first two end up at the same size. */

if (xml_fontsize_next > 1)
  {
  fontinststr *fdata = curmovt->fontsizes->fontsize_text;
  for (int i = 1; i < xml_fontsize_next; i++)
    {
    fdata[i].size = mac_muldiv(xml_fontsizes[i], 1000, main_magnification);
    fdata[i].matrix = NULL;
    }
  }
}



/*************************************************
*           Check for supported element          *
*************************************************/

/* This function is called when a new element is encountered. We check to see
whether it and its attributes are recognized. If not, remember what is not
recognized and what it ignored for possible outputting at the end of
processing.

Argument:   pointer to the item
Returns:    nothing
*/

static void
check_supported(xml_item *new)
{
int c = -1;
int bot = 0;
int top = xml_supported_elements_count;
int mid;
tree_node *tn;
xml_attrstr *attr;

while (top > bot)
  {
  mid = (top + bot)/2;
  c = Ustrcmp(new->name, xml_supported_elements[mid].name);
  if (c == 0) break;
  if (c < 0) top = mid; else bot = mid + 1;
  }

/* Element is not recognized; insert in tree if not already there. */

if (c != 0)
  {
  tn = tree_search(xml_unrecognized_element_tree, new->name);
  if (tn == NULL)
    {
    tn = mem_get(sizeof(tree_node));
    tn->name = mem_copystring(new->name);
    (void)tree_insert(&xml_unrecognized_element_tree, tn);
    }
  return;
  }

/* Element is recognized, check its attributes. Those that start with "#" are
invented internal ones. If the first attribute is "*" it means the entire
element is ignored. If a subsequent attribute is "**" it means all the
following attributes are ignored. */

if (xml_supported_elements[mid].attrs != NULL &&
    Ustrcmp((xml_supported_elements[mid].attrs)[0], "*") == 0)
  {
  tn = tree_search(xml_ignored_element_tree, new->name);
  if (tn == NULL)
    {
    tn = mem_get(sizeof(tree_node));
    tn->name = mem_copystring(new->name);
    (void)tree_insert(&xml_ignored_element_tree, tn);
    }
  }

/* Scan attribute list */

else for (attr = new->p.attr; attr != NULL; attr = attr->next)
  {
  uschar buffer[256];
  uschar **aptr;
  if (attr->name[0] == '#') continue;

  aptr = xml_supported_elements[mid].attrs;
  if (aptr != NULL)
    {
    BOOL ignored = FALSE;
    for (; *aptr != NULL; aptr++)
      {
      if (Ustrcmp(*aptr, "**") == 0) ignored = TRUE;
        else if (Ustrcmp(*aptr, attr->name) == 0) break;
      }
    if (*aptr != NULL)
      {
      if (ignored)  /* Add the attribute to the ignored tree */
        {
        (void)sprintf(CS buffer, "+%s:%s", attr->name, new->name);
        tn = tree_search(xml_ignored_element_tree, buffer);
        if (tn == NULL)
          {
          tn = mem_get(sizeof(tree_node));
          tn->name = mem_copystring(buffer);
          (void)tree_insert(&xml_ignored_element_tree, tn);
          }
        }
      continue;  /* Recognized, next attribute */
      }
    }

  /* Add the attribute to the unrecognized tree */

  (void)sprintf(CS buffer, "+%s:%s", attr->name, new->name);
  tn = tree_search(xml_unrecognized_element_tree, buffer);
  if (tn == NULL)
    {
    tn = mem_get(sizeof(tree_node));
    tn->name = mem_copystring(buffer);
    (void)tree_insert(&xml_unrecognized_element_tree, tn);
    }
  }
}



/*************************************************
*            Handle some actual text             *
*************************************************/

/* This function is called when a character that is not inside an element is
encountered. Copy the text until we hit '<' or end of line.

Argument:
  p          current data pointer

Returns:     updated data pointer
*/

static uschar *
read_text(uschar *p)
{
int len;
int extra = 0;
uschar *pp = p;

while (*p != 0 && *p != '<') p++;
len = p - pp;

/* If the previous item is a data item, tack this text onto it. */

if (Ustrcmp(xml_read_addto->name, "#TEXT") == 0)
  {
  xml_textblock *tb = xml_read_addto->p.txtblk;
  xml_textblock *tbnew = mem_get(sizeof(xml_textblock) + tb->length + len + 1);
  tbnew->next = NULL;

  (void)memcpy(tbnew->string, tb->string, tb->length);
  (void)memcpy(tbnew->string + tb->length, pp, len);
  tbnew->length = tb->length + len;
  tbnew->string[tbnew->length] = 0;

  xml_read_addto->p.txtblk = tbnew;
  }

/* Otherwise we have to make a new data item. The item's name is #TEXT;
it points to a textblock item. */

else
  {
  xml_item *new;
  xml_textblock *tbnew;

  tbnew = mem_get(sizeof(xml_textblock) + len + 1);
  tbnew->next = NULL;

  (void)memcpy(tbnew->string, pp, len);
  tbnew->length = len;
  tbnew->string[tbnew->length] = 0;

  new = mem_get(sizeof(xml_item));
  new->next = xml_read_addto->next;
  new->prev = xml_read_addto;
  new->partner = new;
  new->linenumber = xml_read_linenumber;
  new->flags = 0;
  Ustrcpy(new->name, US"#TEXT");
  new->p.txtblk = tbnew;

  xml_read_addto->next = new;
  xml_read_addto = new;
  }

return p + extra;
}



/*************************************************
*              Handle markup item                *
*************************************************/

/* This function is called when a '<' character is encountered.
It creates an element item with a chain of attribute settings. If the element
ends with /> the partner pointer is set to point to itself. Otherwise, we push
the element onto the stack so that it can get matched up when the terminator is
encountered later.

Arguments:
  p            pointer in input line, at initial '<'
  nest_stack   the nesting stack
  nest_ptrptr  pointer to the stack pointer

Returns:       updated pointer, past the terminating '>'
               updates the stack pointer
*/

static uschar *
read_element(uschar *p, xml_item **nest_stack, int *nest_ptrptr)
{
BOOL ender = FALSE;
BOOL procinst = FALSE;
int i = 0;
int nest_stackptr = *nest_ptrptr;
int elementstartline = read_linenumber;
uschar name[NAMESIZE];
uschar *pp = name;
xml_item *new;

/* Handle special kinds of markup:

  <?.....?> is a processing instruction - ignore unless for pmw
  <!......> is a heading (?) - currently ignored
  <!--..--> is a comment that may span multiple lines, and be nested (?)
*/

if (*(++p) == '?')
  {
  if (Ustrncmp(p, "?pmw", 4) != 0 || !isspace(p[4]))
    {
    for (;;)
      {
      while (*(++p) != 0 && *p != '>');
      if (*p == '>')
        {
        p++;
        return p;
        }
      p = Ufgets(linebuffer, LINEBUFSIZE, infile);
      xml_read_linenumber++;
      if (p == NULL) xml_error(ERR12, elementstartline);  /* Hard */
      }
    }

  /* We have a processing instruction for pmw. Arrange to set this up as an
  item whose name is "?pmw", with appropriate attributes. We can do this by
  setting the leading '?' and then falling through. */

  procinst = TRUE;
  *pp++ = '?';
  i++;
  p++;
  }

/* Handle headings, and comments */

else if (*p == '!')
  {
  int nestcount;

  /* If not a comment, just skip to the closing '>' */

  if (Ustrncmp(p, "!--", 3) != 0)
    {
    while (*(++p) != 0 && *p != '>');
    if (*p == '>') p++;
    return p;
    }

  /* Handle comments */

  nestcount = 1;
  p += 3;

  while (nestcount > 0)
    {
    while (*p != 0)
      {
      if (Ustrncmp(p, "-->", 3) == 0)
        {
        p += 3;
        if (--nestcount <= 0) break;
        }
      else if (Ustrncmp(p, "<!--", 4) == 0)
        {
        nestcount++;
        p += 4;
        }
      else p++;
      }

    /* Comment continues onto the next line */

    if (*p == 0 && nestcount > 0)
      {
      uschar *ppp = Ufgets(linebuffer, LINEBUFSIZE, infile);
      xml_read_linenumber++;
      if (ppp == NULL) xml_error(ERR13, elementstartline);  /* Hard */
        else p = ppp;
      }
    }

  return p;
  }

/* Handle "normal" markup: test for an ending tag. */

if (*p == '/') { ender = TRUE; p++; }

/* Scan for the element name */

while (isalnum(*p) || *p == '_' || *p == '-' || *p == '.')
  {
  if (i++ < NAMESIZE - 1) *pp++ = *p;
  p++;
  }
*pp = 0;

/* Deal with an ending tag */

if (ender)
  {
  xml_item *partner;

  if (*p != '>')
    {
    xml_error(ERR2, name);
    while (*p != 0 && *p != '>') p++;
    }

  else
    {
    p++;
    if (nest_stackptr <= 0)
      {
      xml_error(ERR3, name);
      }
    else if (Ustrcmp(nest_stack[nest_stackptr-1]->name, name) != 0)
      {
      xml_error(ERR5, name, nest_stack[nest_stackptr-1]->name);
      }
    else
      {
      partner = nest_stack[--nest_stackptr];
      new = mem_get(sizeof(xml_item));
      new->prev = xml_read_addto;
      new->next = xml_read_addto->next;
      if (new->next != NULL) new->next->prev = new;
      new->linenumber = read_linenumber;
      new->flags = 0;
      Ustrcpy(new->name, "/");
      new->p.attr = NULL;
      xml_read_addto->next = new;
      xml_read_addto = new;

      new->partner = partner;
      partner->partner = new;
      }
    }
  }

/* Deal with a starting tag */

else
  {
  BOOL ended = FALSE;
  xml_attrstr *attr = NULL;
  xml_attrstr *lastattr = NULL;
  xml_attrstr *newattr;

  /* Now read any attributes that are set in the element. This may continue
  onto more than one line. */

  while (isspace(*p)) p++;
  for (;;)
    {
    int quote, dlen;
    uschar attname[NAMESIZE];

    /* Handle line continuations */

    while (*p == 0)
      {
      uschar *pnew = Ufgets(linebuffer, LINEBUFSIZE, infile);
      xml_read_linenumber++;
      if (pnew == NULL)
        {
        xml_error(ERR14, elementstartline);
        break;
        }
      else p = pnew;
      while (isspace(*p)) p++;
      }

    /* Test for end of the element */

    if (*p == '>' || *p == '/' || *p == '?')  break;

    /* Now read the name of the attribute */

    pp = attname;
    i = 0;
    while (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == ':')
      {
      if (i++ < NAMESIZE - 1) *pp++ = *p;
      p++;
      }
    *pp = 0;

    while (isspace(*p)) p++;
    if (*p != '=') { xml_error(ERR6, attname); break; }
    while (isspace(*(++p)));
    if (*p != '"' && *p != '\'') { error(ERR7, attname); break; }
    quote = *p++;
    pp = p;

    while (*p != 0 && *p != quote) p++;
    if (*p != quote) { xml_error(ERR8, quote, attname, quote); break; }

    dlen = p - pp;
    newattr = mem_get(sizeof(xml_attrstr) + dlen);
    newattr->next = NULL;
    Ustrcpy(newattr->name, attname);
    Ustrncpy(newattr->value, pp, dlen);
    newattr->value[dlen] = 0;

    if (attr == NULL) attr = newattr;
      else lastattr->next = newattr;
    lastattr = newattr;
    while (isspace(*(++p)));
    }

  if (*p == '/' || (procinst && *p == '?'))
    {
    ended = TRUE;
    if (*(++p) != '>') xml_error(ERR9, name);
    }

  /* Skip to end of element (in case jumped out after an error) */

  while (*p != 0 && *p != '>') p++;
  if (*p == 0) xml_error(ERR10, name); else p++;

  /* Create the new element, crossreference its id, note if it or any of its
  attributes are not supported, and push it onto the stack for checking its
  partner. */

  new = mem_get(sizeof(xml_item));
  new->linenumber = xml_read_linenumber;
  new->flags = 0;
  new->partner = ended? new : NULL;

  /* Valgrind can give false positives on name comparisons with strcmp(); the
  exact circumstances are unknown, but writing to the whole field here gets rid
  of them. */

  memset(new->name, 0, NAMESIZE);
  Ustrcpy(new->name, name);

  new->p.attr = attr;
  new->prev = xml_read_addto;
  new->next = xml_read_addto->next;
  if (new->next != NULL) new->next->prev = new;
  xml_read_addto->next = new;
  xml_read_addto = new;

  if (new->name[0] != '?') check_supported(new);

  if (!ended)
    {
    if (nest_stackptr >= NESTSTACKSIZE) xml_error(ERR4);  /* Hard error */
    nest_stack[nest_stackptr++] = new;
    }
  }

/* Update stack pointer and return char pointer */

*nest_ptrptr = nest_stackptr;
return p;
}




/*************************************************
*       Read a string of elements and text       *
*************************************************/

/* This function processes a single string that may contain both elements and
text. Skip white space that follows an element or processing instruction.

Arguments:
  p            pointer to input line
  nest_stack   the nesting stack
  nest_ptrptr  pointer to the stack pointer

Returns:       nothing
*/

static void
read_string(uschar *p, xml_item **nest_stack, int *nest_stackptr)
{
while (*p != 0)
  {
  if (*p == '<')
    {
    p = read_element(p, nest_stack, nest_stackptr);
    while (isspace(*p)) p++;
    }
  else
    {
    p = read_text(p);
    }
  }
}



/*************************************************
*         Read the input file into memory        *
*************************************************/

/* This function opens an input file and reads it, creating a chain of element
and text items. The basic function passes back a list of unclosed items. There
is a second function that generates an error for any unclosed items.

Arguments:
  filename       file name, or NULL for stdin
  filehandle     open file or NULL for file to be opened
  nest_stack     stack for nesting, size must be NESTSTACKSIZE
  nest_stackptr  points to stack offset pointer

  The read_addto global variable must be set to point the existing last item of
  the list that is being created. The variable is updated.

Returns:         TRUE if OK (currently always; errors are hard)
*/

static BOOL
read_file2(uschar *filename, FILE *filehandle, xml_item **nest_stack,
  int *nest_stackptr)
{
xml_item *fn;
uschar *p;
uschar buffer[LINEBUFSIZE];

linebuffer = buffer;      /* For use when comments overflow lines */

if (filename == NULL)
  {
  DEBUG(D_any) eprintf("===> Reading MusicXML from stdin\n");
  infile = filehandle;
  }
else
  {
  DEBUG(D_any) eprintf("===> Reading MusicXML from %s\n", filename);
  if (filehandle == NULL)
    {
    infile = Ufopen(filename, "rb");
    if (infile == NULL)
      error(ERR0, filename, "input file", strerror(errno));  /* Hard */
    }
  else infile = filehandle;
  }

xml_read_filename = (filename == NULL)? US"(stdin)" : filename;
xml_read_linenumber = 0;

/* Stick in a dummy element to hold the file name so we can distinguish
included files in error messages. */

fn = mem_get(sizeof(xml_item));
fn->prev = xml_read_addto;
fn->next = xml_read_addto->next;
fn->linenumber = 0;
fn->flags = 0;
fn->partner = fn;
Ustrcpy(fn->name, "#FILENAME");
fn->p.string = mem_get(Ustrlen(xml_read_filename) + 1);
Ustrcpy(fn->p.string, xml_read_filename);

xml_read_addto->next = fn;
xml_read_addto = fn;

/* Process the lines of the file, retaining the newline on the end of each
line, but removing any white space that precedes it. Also remove any leading
white space when not in the middle of text data. */

while ((p = Ufgets(buffer, sizeof(buffer), infile)) != NULL)
  {
  uschar *pp = p + Ustrlen(p);

  while (pp > p + 1)
    {
    if (!isspace(pp[-2])) break;
    (pp--)[-2] = '\n';
    }
  *pp = 0;

  if (Ustrcmp(xml_read_addto->name, "#TEXT") != 0)
    {
    while(isspace(*p)) p++;
    }
  xml_read_linenumber++;
  read_string(p, nest_stack, nest_stackptr);
  }

xml_read_linenumber = 0;
return TRUE;
}



/*************************************************
*     Read input file and check nest closed      *
*************************************************/

/* This is a wrapper function for the one above; if there are any unclosed
elements at the end, it generates an error. This function is called both for
the main input file and to read inclusions.

Arguments:
  filename   file name, or NULL for stdin
  filehandle file handle or NULL for file to be opened
  item_list  the dummy item at the start of the list that's being read

Returns:     TRUE if all went well
*/

BOOL
xml_read_file(uschar *filename, FILE *filehandle, xml_item *item_list)
{
int nest_stackptr = 0;
xml_item *nest_stack[NESTSTACKSIZE];

xml_read_addto = item_list;
(void)read_file2(filename, filehandle, nest_stack, &nest_stackptr);

if (nest_stackptr > 0)
  {
  uschar *s = (nest_stackptr > 1)? US"s" : US"";
  xml_error(ERR15, s);                  /* General message */
  eprintf("** Start of unclosed element%s:\n", s);
  while (nest_stackptr > 0)
    {
    xml_item *i = nest_stack[--nest_stackptr];
    eprintf("  Line %5d: <%s>\n", i->linenumber, i->name);
    }
  return FALSE;
  }

return TRUE;
}




/*************************************************
*              Read main input file              *
*************************************************/

/* This function is called when the top-level input file is determined to be
MusicXML input.

Argument:    file name, or NULL for stdin
Returns:     TRUE if all went well
*/

void
xml_read(void)
{
BOOL rc;

xml_main_item_list = xml_new_item(US"#");  /* Anchor item */

for (int i = 0; i < 64; i++)
  {
  xml_stave_sizes[i] = -1;
  xml_couple_settings[i] = COUPLE_NOT;
  }

xml_fontsizes[xml_fontsize_next++] = 10000;

xml_read_linenumber = 1;
rc = xml_read_file(read_filename, read_filehandle, xml_main_item_list);

DEBUG(D_xmlread) xml_debug_print_item_list(xml_main_item_list,
  "at end of reading main MusicXML input");
xml_read_done = TRUE;

rc = rc && xml_analyze();

/* If there has been an error of at least severity ec_major, do not proceed */

if (xml_error_max >= ec_major)
  {
  eprintf("** No output has been generated.\n");
  main_suppress_output = TRUE;
  }

/* If all is well, generate the PMW data structures */

if (rc) xml_process();

if (main_verify && xml_ignored_element_tree != NULL)
  {
  eprintf("-------- Ignored XML elements and attributes --------\n");
  print_unknown_tree(xml_ignored_element_tree);
  eprintf("-----------------------------------------------------\n\n");
  }

if (xml_warn_unrecognized && xml_unrecognized_element_tree != NULL)
  {
  eprintf("-------- Unrecognized XML elements and attributes --------\n");
  print_unknown_tree(xml_unrecognized_element_tree);
  eprintf("----------------------------------------------------------\n\n");
  }

/* If there were errors that did not prevent the output being generated, give a
warning. */

if (xml_error_max > 0 && xml_error_max < ec_major)
  eprintf("** Warning: generated output may be flawed.\n");
}

/* End of xml_read.c */
