/*************************************************
*        PMW native input reading functions      *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: December 2020 */
/* This file last modified: April 2022 */

/* This file contains the top-level function and character handling functions
that are called from the modules that read headings and staves. */

#include "pmw.h"


/* Default "macro" for the &* replication feature */

static macrostr replicate_macro = { 1, US"&1", { US"" } };



/*************************************************
*        Get MIDI number from file               *
*************************************************/

/* This function scans a list of MIDI voice or percussion names to find the
number for a given name. If we are not generating MIDI (midi_filename is NULL),
there is no search, but we do not give an error. Just return zero.

Arguments:
  list        the list of MIDI names
  string      the string to search for
  text        text for use in the error message

Returns:      the MIDI number
*/

int
read_getmidinumber(uschar *list, uschar *string, uschar *text)
{
int yield = -1;

if (list != NULL) while (*list != 0)
  {
  int len = Ustrlen(list);
  if (Ustrcmp(list, string) == 0)
    {
    yield = list[len+1];
    break;
    }
  list += len + 2;
  }

if (yield < 0)
  {
  error(ERR49, text, string);
  yield = 1;
  }

return yield;
}



/*************************************************
*            Double/halve time signature         *
*************************************************/

/* The variables curmovt->notenum and curmovt->noteden contain the numerator
and denominator of any note scaling (doublenotes, halvenotes) that was set up
in the movement's header. This affects a previously-defined time signature. The
stave directives [doublenotes] and [halvenotes] do not affect time signatures.

Argument:   the unscaled time signature
Returns:    the scaled time signature
*/

uint32_t
read_scaletime(uint32_t ts)
{
int m, n, d;
if (curmovt->notenum == 1 && curmovt->noteden == 1) return ts;

m = (ts & 0xffff0000u) >> 16;      /* multiplier */
n = (ts & 0x0000ff00u) >> 8;       /* numerator */
d = (ts & 0x000000ffu);            /* denominator */

if (d == time_common || d == time_cut)
  {
  m *= curmovt->notenum;
  if (curmovt->noteden > 1)
    {
    if (m % (curmovt->noteden) == 0) m /= curmovt->noteden;
      else error(ERR41);  /* Hard */
    }
  return (m << 16) | d;
  }

d *= curmovt->noteden;
if (d % (curmovt->notenum) == 0) d /= curmovt->notenum;
  else n *= curmovt->notenum;

return (m << 16) + (n << 8) + d;
}



/*************************************************
*              Read time signature               *
*************************************************/

/* Return zero after an error. A time signature can be of the form m*n/d.

Arguments:  none
Returns:    the packed up time signature
*/

uint32_t
read_time(void)
{
BOOL gotnum = FALSE;
int32_t m, n, d;

if (isdigit(read_c))
  {
  (void)read_expect_integer(&m, FALSE, FALSE);
  read_sigc();
  if (read_c == '*') read_nextc(); else
    {
    n = m;          /* m becomes numerator */
    m = 1;          /* multiplier becomes 1 */
    gotnum = TRUE;  /* numerator has been read */
    }
  }
else m = 1;         /* non-numeric time sig */

/* Look for A or C or a numerator */

if (!gotnum)
  {
  read_c = tolower(read_c);
  if (read_c == 'a' || read_c == 'c')
    {
    uint32_t type = (read_c == 'c')? time_common : time_cut;
    read_nextc();
    return (m << 16) | type;
    }
  if (!read_expect_integer(&n, FALSE, FALSE)) return 0;
  }

/* Look for denominator */

read_sigc();
if (read_c != '/')
  {
  error(ERR8, "\"/\"");
  return 0;
  }

read_nextc();
if (!read_expect_integer(&d, FALSE, FALSE)) return 0;

if (d < 1 || d > 64 || d != (d & (-d))) error(ERR46); /* hard error */
return (m << 16) | (n << 8) | d;
}



/*************************************************
*             Read key signature                 *
*************************************************/

/*
Arguments:  none
Returns:    the key signature (C major after an error)
*/

uint32_t
read_key(void)
{
uint32_t key = key_C;

read_c = tolower(read_c);
if ('a' <= read_c && read_c <= 'g')
  {
  key = read_c - 'a';
  read_nextc();
  if (read_c == '#')      { key += 7; read_nextc(); }
    else if (read_c == '$') { key += 14; read_nextc(); }
  if (tolower(read_c) == 'm') { key += 21; read_nextc(); }
  if (keysigtable[key][0] == ks_bad) { key = key_C; error(ERR43); }
  }
else if (read_c == 'n') { key = key_N; read_nextc(); }

else if (read_c == 'x')
  {
  int32_t n;
  read_nextc();
  if (!read_expect_integer(&n, FALSE, FALSE) || n == 0 || n > MAX_XKEYS)
    error(ERR44, MAX_XKEYS);
  else
    key = key_X + n - 1;
  }

else error(ERR8, "key signature");
return key;
}



/*************************************************
*          Read and lowercase next word          *
*************************************************/

void
read_nextword(void)
{
int i = 0;
read_sigc();
if (isalpha(read_c))
  {
  do
    {
    if (i >= WORDBUFFER_SIZE - 1)
      error(ERR7, "word", WORDBUFFER_SIZE - 1);  /* Hard */
    read_wordbuffer[i++] = tolower(read_c);
    read_nextc();
    }
  while (isalnum(read_c) || read_c == '_');
  }
read_wordbuffer[i] = 0;
}



/*************************************************
*           Read unsigned integer                *
*************************************************/

/* Called when we know the current character is a digit. */

uint32_t
read_usint(void)
{
uint32_t yield = 0;
do
  {
  usint x = read_c - '0';
  if (yield > (UINT32_MAX - x)/10) error(ERR36, "integer");
  yield = yield*10 + x;
  read_nextc();
  }
while (isdigit(read_c));
return yield;
}



/*************************************************
*            Read logical bar number             *
*************************************************/

/* Called when we know the current character is a digit. */

uint32_t
read_barnumber(void)
{
uint32_t bar = read_usint() << 16;

if (read_c == '.')
  {
  read_nextc();
  if (!isdigit(read_c)) error(ERR8, "fractional part for bar number");
    else bar |= read_usint();
  }

return bar;
}



/*************************************************
*            Read fixed-point number             *
*************************************************/

/* Called when we know the current character is a digit. */

uint32_t
read_fixed(void)
{
uint32_t yield = read_usint();
if (yield > UINT32_MAX/1000) error(ERR36, "fixed point number");
yield *= 1000;
if (read_c == '.')
  {
  usint d = 100;
  while (read_nextc(), isdigit(read_c))
    {
    yield += (read_c - '0')*d;
    d /= 10;
    }
  }
return yield;
}



/*************************************************
*       Read an expected movement dimension      *
*************************************************/

/* This function is called after /u, /d, /l, or /r has been read.

Arguments:   none
Returns:     the value, or zero after an error
*/

int32_t
read_movevalue(void)
{
int32_t x;
read_nextc();
return (read_expect_integer(&x, TRUE, FALSE))? x : 0;
}




/*************************************************
*         Read an expected int or fixed          *
*************************************************/

/* This is called when the first character hasn't been checked, and an error
indication is required if the value is missing.

Arguments:
  yield       where to put the value
  fixed       TRUE for a fixed point value, FALSE for an integer
  allowsign   TRUE if a sign is permitted

Returns:      TRUE if OK, FALSE on error
*/

BOOL
read_expect_integer(int32_t *yield, BOOL fixed, BOOL allowsign)
{
int sign = 1;
uint32_t n;
read_sigc();

if (allowsign)
  {
  if (read_c == '+') read_nextc();
    else if (read_c == '-') { sign = -1; read_nextc(); }
  }

if (!isdigit(read_c))
  {
  error(ERR8, allowsign? "number" : "unsigned number");
  return FALSE;
  }

n = fixed? read_fixed() : read_usint();
if (n > INT32_MAX) error(ERR36, "number");
*yield = n * sign;
return TRUE;
}



/*************************************************
*            Read accent or ornament             *
*************************************************/

/* Accents and ornaments are listed in the same table. They are distinguished
by the value. */

uint32_t
read_accororn(uint32_t eskip)
{
accent *ap;
uschar *s = main_readbuffer + read_i - 1;
for (ap = accent_chars; ap->string != NULL; ap++)
  {
  if (Ustrncmp(s, ap->string, ap->len) == 0)
    {
    read_i += ap->len - 1;
    read_nextc();
    return ap->flag;
    }
  }
error_skip(ERR92, eskip);
return or_unset;
}



/*************************************************
*      Read a font size and stretch/shear        *
*************************************************/

/* This is always called with the first digit already read.

Arguments:
  fdata      pointer to fontinststr to receive the data
  stretchOK  TRUE if stretching is allowed

Returns: nothing
*/

void
read_fontsize(fontinststr *fdata, BOOL stretchOK)
{
int32_t stretch;
int32_t shear;

fdata->size = read_fixed();
fdata->matrix = NULL;
if (read_c != '/') return;

/* Handle optional stretch and shear */

shear = 0;
read_nextc();
if (!read_expect_integer(&stretch, TRUE, FALSE)) return; /* Fixed, unsigned */

if (read_c == '/')
  {
  read_nextc();
  if (!read_expect_integer(&shear, TRUE, TRUE)) return;  /* Fixed, signed */
  }

if (!stretchOK) error(ERR35, "allowed", "with this directive"); else
  {
  int32_t *matrix = mem_get(6*sizeof(int32_t));
  fdata->matrix = matrix;
  matrix[0] = mac_muldiv(stretch, 65536, 1000);
  matrix[1] = 0;
  matrix[2] = (int32_t)(tan(((double)shear)*atan(1.0)/45000.0)*65536.0);
  matrix[3] = 65536;
  matrix[4] = 0;               /* Sine of rotation */
  matrix[5] = 1000;            /* Cosine of rotation */
  }
}



/*************************************************
*        Read a drawing call with arguments      *
*************************************************/

/* This function is called in a number of contexts where the word "draw" has
just been read. It reads arguments and a drawing name. */

void
read_draw(tree_node **drawptr, drawitem **argsptr, uint32_t eskip)
{
tree_node *node;
int argcount = 0;
drawitem *drawargs = NULL;
drawitem args[20];

/* Read optional arguments */

read_sigc();
while (isdigitorsign(read_c) || read_c == '\"')
  {
  if (read_c == '\"')
    {
    args[++argcount].d.ptr = read_draw_text();
    args[argcount].dtype = dd_text;
    }
  else
    {
    if (!read_expect_integer(&(args[++argcount].d.val), TRUE, TRUE)) break;
    args[argcount].dtype = dd_number;
    }
  read_sigc();
  }

if (argcount > 0)
  {
  drawargs = mem_get((argcount+1)*sizeof(drawitem));
  drawargs[0].dtype = dd_number;
  drawargs[0].d.val = argcount;
  memcpy(drawargs+1, args+1, argcount * sizeof(drawitem));
  }

read_nextword();
node = tree_search(draw_tree, read_wordbuffer);
if (node == NULL)
  {
  error_skip(ERR70, eskip, read_wordbuffer);
  return;
  }

*drawptr = node;
*argsptr = drawargs;
}



/*************************************************
*        Read arguments for heading/footing      *
*************************************************/

/* This code is called when reading headers. It is also called to read the text
of footnotes, which occur within staves.

Arguments:
  new        the new headstr
  type       type of heading/footing
  eskip      character to skip to on error

Returns:     nothing
*/

void
read_headfootingtext(headstr *new, uint32_t type, uint32_t eskip)
{
int32_t followspace = 0;

new->next = NULL;
new->drawing = NULL;

/* If the next character is a letter, we expect to read "draw [<arguments>]
<name>". */

if (isalpha(read_c))
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "draw") != 0)
    {
    error_skip(ERR8, eskip, "\"draw\"");
    return;
    }
  read_draw(&(new->drawing), &(new->drawargs), eskip);
  }

/* Otherwise we expect a PMW string. Set the type size, either by reading, or
by algorithm. If explicit, it can be followed by stretch and possibly shear. */

else
  {
  int i;
  uint32_t *p;

  new->spaceabove = 0;
  new->fdata.matrix = NULL;
  new->fdata.spacestretch = 0;

  /* Explicit size given */

  if (isdigit(read_c)) read_fontsize(&(new->fdata), TRUE);

  /* No size was given */

  else switch(type)
    {
    case rh_heading:
    new->fdata.size = read_headingsizes[read_nextheadsize];
    break;

    case rh_footing:
    case rh_pagefooting:
    case rh_lastfooting:
    new->fdata.size = READ_FOOTINGSIZE;
    break;

    case rh_pageheading:
    new->fdata.size = READ_PAGEHEADINGSIZE;
    break;

    case rh_footnote:
    new->fdata.matrix = curmovt->fontsizes->fontsize_footnote.matrix;
    new->fdata.size = curmovt->fontsizes->fontsize_footnote.size;
    break;
    }

  /* Default following space is type size */

  followspace = new->fdata.size;

  /* Must increment next heading size whether explicit size supplied or not, up
  to the end of the list. */

  if (type == rh_heading && read_headingsizes[read_nextheadsize + 1] != 0)
    read_nextheadsize++;

  /* We now read a PMW string and split it into left/middle/right. After the
  split, run the string check on each segment separately. This is necessary in
  case there is B2PF processing that reverses the order of characters - we
  don't want to reverse the order of the segments. */

  p = new->string[0] = string_read(font_rm, FALSE);
  for (i = 1; i < 3; i++)
    {
    while (*p != 0)
      {
      if (PCHAR(*p++) != ss_verticalbar) continue;
      p[-1] = 0;
      new->string[i] = p;
      break;
      }
    if (*p == 0) break;
    }
  for (int j = 0; j < i; j++)
    new->string[j] = string_check(new->string[j], NULL, FALSE);
  for (; i < 3; i++) new->string[i] = NULL;  /* Missing parts */
  }

/* In both cases (either drawing or text), if another number follows, it is an
explicit following space. */

read_sigc();
if (isdigitorsign(read_c)) read_expect_integer(&followspace, TRUE, TRUE);
new->space = followspace;
}



/*************************************************
*           Handle continued input line          *
*************************************************/

/* White space before a line's terminating \n is removed when the line is read,
so we can just test for &&& before the newline. If found, remove it, and add
the next line, repeating as necessary. */

static void
handle_continuation(void)
{
for (;;)
  {
  uschar *p;
  if (main_readlength < 4) return;
  p = main_readbuffer + main_readlength - 4;
  if (Ustrncmp(p, "&&&", 3) != 0) return;
  *p = '\n';
  main_readlength -= 3;   /* In case no further lines */
  if (!read_physical_line(main_readlength - 1)) return;
  }
}



/*************************************************
*        Extend line or argument buffers         *
*************************************************/

/* This is called from expand_string() below, either to extend the line buffers
(if an expanded line is too long) or to extend the buffer into which macro
arguments and their expansions are placed. Expanding an argument uses a
separate buffer at each level. Both input (the raw argument) and output use the
same buffer for this. We use the same extension amount and maximum size in both
cases.

Arguments:
  next        macro expansion nest level: negative => expanding an input line
  inptr       points to existing input buffer     ) for an argument, these
  outptr      points to existing output buffer    )   are the same buffer
  outsizeptr  points to current output buffer size

Returns:      nothing
*/

static void
extend_expand_buffers(int nest, uschar **inptr, uschar **outptr,
  size_t *outsizeptr)
{
if (nest >= 0)  /* Expanding an argument */
  {
  if (main_argbuffer_size[nest] >= MAIN_READBUFFER_SIZELIMIT)
    error(ERR5, "argument expansion", MAIN_READBUFFER_SIZELIMIT);  /* Hard */
  main_argbuffer_size[nest] += MAIN_READBUFFER_CHUNKSIZE;
  main_argbuffer[nest] =
    realloc(main_argbuffer[nest], main_argbuffer_size[nest]);
  if (main_argbuffer[nest] == NULL) error(ERR0, "re-",
    "macro expansion buffer", main_argbuffer_size[nest]);  /* Hard */
  *inptr = *outptr = main_argbuffer[nest];
  *outsizeptr = main_argbuffer_size[nest];
  }

/* Extend the line buffers at top level. */

else
  {
  read_extend_buffers();
  *inptr = main_readbuffer_raw;
  *outptr = main_readbuffer;
  *outsizeptr = main_readbuffer_size;
  }
}



/*************************************************
*     Expand string with macros substitutions    *
*************************************************/

/* This is called for input lines, and also for the arguments of nested macro
calls. It copies the input string, expanding any macros encountered. Macro
arguments are expanded recursively.

Arguments:
  inbuffer      buffer containing the input
  in            offset to start of input string
  inlen         offset to end of input string (points to zero)
  outbuffer     buffer containing the output
  out           offset to start of where to put the output
  outlen        length of output buffer
  nest          nest level; negative = top level, i.e. the line buffers

Returns:        offset one past the end of the active part of the expanded
                  string in outbuffer
*/

static size_t
expand_string(uschar *inbuffer, size_t in, size_t inlen,
  uschar *outbuffer, size_t out, size_t outlen, int nest)
{
DEBUG(D_macro) fprintf(stderr, "Macro expand: %s", inbuffer);

if (nest >= MAX_MACRODEPTH) error(ERR22, MAX_MACRODEPTH);  /* Hard */

while (in <= inlen)  /* Include terminating zero to get buffer extension */
  {
  macrostr *mm;
  BOOL had_semicolon;
  usint count;
  usint ch = inbuffer[in++];

  if (nest < 0) macro_in = in;  /* For error messages */

  /* Handle a literal character. */

  if (ch != '&' || inbuffer[in] == '&')
    {
    if (out >= outlen)
      extend_expand_buffers(nest, &inbuffer, &outbuffer, &outlen);
    outbuffer[out++] = ch;
    if (ch == '&') in++;
    continue;
    }

  /* Deal with macro insertions, possibly with arguments. Also deal with &*()
  repetitions, which use very similar code. */

  mm = NULL;
  had_semicolon = FALSE;
  count = 1;

  /* Set up for a macro call. Note that macro names may start with a digit
  and are case-sensitive, so we can't use read_nextword(). */

  if (isalnum(inbuffer[in]))
    {
    usint i = 0;
    tree_node *s;

    read_wordbuffer[i++] = inbuffer[in++];
    while (isalnum(inbuffer[in]))
      {
      if (i >= WORDBUFFER_SIZE - 1)
        error(ERR7, "macro name", WORDBUFFER_SIZE - 1);  /* Hard */
      read_wordbuffer[i++] = inbuffer[in++];
      }
    read_wordbuffer[i] = 0;

    if (inbuffer[in] == ';')
      {
      in++;                   /* Optional semicolon after name is */
      had_semicolon = TRUE;   /*   skipped, and no args allowed */
      }

    if ((s = tree_search(macro_tree, read_wordbuffer)) != NULL)
      {
      mm = (macrostr *)s->data;
      if (mm == NULL) continue;  /* NULL means no replacement */
      }
    else
      {
      error(ERR17, read_wordbuffer);  /* Couldn't find name */
      continue;
      }
    }

  /* Set up for a replication call */

  else if (inbuffer[in] == '*')
    {
    int len;
    if (sscanf((char *)(inbuffer + in + 1), "%u(%n", &count, &len) <= 0)
      {
      error(ERR8, "after &* unsigned number followed by \"(\"");
      continue;
      }
    in += len;
    mm = &replicate_macro;  /* Pseudo macro for code sharing below */
    }

  /* Invalid character follows '&'. */

  else
    {
    error(ERR18);
    continue;
    }

  /* Found a macro or &*<digits>. For a macro, mm points to its data, and
  count is 1. For a replication, mm points to a dummy with 1 empty default
  argument, count contains the replication count, and we know there is an
  argument. */

  /* Optimize the case when macro is defined with no arguments */

  if (mm->argcount == 0)
    {
    size_t len = Ustrlen(mm->text);
    while (outlen - out < len + inlen - in)
      extend_expand_buffers(nest, &inbuffer, &outbuffer, &outlen);
    Ustrcpy(outbuffer + out, mm->text);
    out += len;
    }

  /* Otherwise we have to process the replacement text character by character,
  having read any arguments that are present. There need not be; they can all
  be defaulted. Arguments are read, serially, into main_argbuffer[nest+1], and
  then expanded for nested macros into what remains of that buffer, which can
  be extended if necessary. */

  else
    {
    int argcount = mm->argcount;
    int nestarg = nest + 1; 
    uschar *argbuff = main_argbuffer[nestarg];
    size_t args[MAX_MACROARGS];
    size_t ap = 0;   /* Offset in argbuff */

    /* Set up with no arguments */

    for (int i = 0; i < argcount; i++) args[i] = SIZE_UNSET;

    /* Read given arguments, if any, increasing the count if more than the
    default number, but only if the name was not followed by a semicolon. */

    if (!had_semicolon && inbuffer[in] == '(')
      {
      for (int i = 0;; i++)
        {
        int bracount = 0;
        BOOL inquotes = FALSE;
        size_t ss = ap;

        if (argcount >= MAX_MACROARGS) error(ERR19, MAX_MACROARGS);  /* Hard */

        while ((ch = inbuffer[++in]) != '\n' && ch != 0 &&
              ((ch != ',' && ch != ')') || bracount > 0 || inquotes))
          {
          if (ap >= main_argbuffer_size[nestarg])
            {
            extend_expand_buffers(nestarg, &(main_argbuffer[nestarg]),
              &(main_argbuffer[nestarg]), &(main_argbuffer_size[nestarg]));
            argbuff = main_argbuffer[nestarg];
            }

          if (ch == '&' && !isalnum(inbuffer[in+1]) && inbuffer[in+1] != '*')
            argbuff[ap++] = inbuffer[++in];  /* Escaped literal */

          else
            {
            if (ch == '\"') inquotes = !inquotes;
            if (!inquotes)
              {
              if (ch == '(') bracount++;
                else if (ch == ')') bracount--;
              }
            argbuff[ap++] = ch;
            }
          }

        /* There are more arguments in the call than were defaulted. */

        if (i >= argcount)
          {
          args[i] = SIZE_UNSET;
          argcount++;
          }

        /* Save a non-empty argument */

        if (ap - ss > 0)
          {
          argbuff[ap++] = 0;
          args[i] = ss;
          }

        /* Missing closing parenthesis */

        if (ch == '\n' || ch == 0)
          {
          error(ERR15);
          break;
          }

        /* Reached the end of the arguments */

        if (ch == ')')
          {
          in++;
          break;
          }
        }

      /* Only one argument is currently allowed for a replication. Any
      others are ignored. */

      if (mm == &replicate_macro && argcount > 1) error(ERR20);
      }

    /* Check the arguments for nested macro calls. */

    for (int i = 0; i < argcount; i++)
      {
      size_t new_ap;
      if (args[i] == SIZE_UNSET || Ustrchr(argbuff + args[i], '&') == NULL)
        continue;
      new_ap = expand_string(argbuff, args[i], Ustrlen(argbuff + args[i]),
        argbuff, ap, main_argbuffer_size[nestarg], nestarg);
      args[i] = ap;
      ap = new_ap + 1;  /* Final zero must remain */
      }

    /* Now copy the replacement, inserting the args. For a replication we
    repeat many times. For a macro, count is always 1. */

    while (count-- > 0)
      {
      uschar *pp = mm->text;
      while (*pp != 0)
        {
        if (*pp == '&' && isdigit(pp[1]))
          {
          int arg = 0;
          while (isdigit(*(++pp))) arg = arg*10 + *pp - '0';
          if (*pp == ';') pp++;
          if (--arg < argcount)
            {
            uschar *ss = NULL;
            if (args[arg] != SIZE_UNSET) ss = argbuff + args[arg];
              else ss = mm->args[arg];
            if (ss != NULL)
              {
              size_t ssl = Ustrlen(ss);
              if (ssl > outlen - out)
                extend_expand_buffers(nest, &inbuffer, &outbuffer, &outlen);
              Ustrcpy(outbuffer + out, ss);
              out += ssl;
              }
            }
          }
        else
          {
          if (out >= outlen)
            extend_expand_buffers(nest, &inbuffer, &outbuffer, &outlen);
          outbuffer[out++] = *pp++;
          }
        }
      }
    }
  }

DEBUG(D_macro) fprintf(stderr, "Expanded: %s", outbuffer);

return out - 1;  /* Exclude terminating zero */
}



/*************************************************
*       Expand macros in an input line           *
*************************************************/

/* First remove any trailing comment. Then, if the line contains at least one
ampersand, swap the input buffer so it becomes the raw input buffer, then
process it into the new buffer, expanding macros. */

static void
expand_macros(void)
{
BOOL inquotes = FALSE;

for (size_t i = 0; i < main_readlength; i++)
  {
  usint c = main_readbuffer[i];
  if (c == '\"') inquotes = !inquotes;
  else if (!inquotes && c == '@')
    {
    while (i > 0 && isspace(main_readbuffer[i-1])) i--;
    main_readbuffer[i++] = '\n';
    main_readbuffer[i] = 0;
    main_readlength = i;
    break;
    }
  }

if (Ustrchr(main_readbuffer, '&') != NULL)
  {
  uschar *temp = main_readbuffer;
  main_readbuffer = main_readbuffer_raw;
  main_readbuffer_raw = temp;
  macro_expanding = TRUE;
  main_readlength = expand_string(main_readbuffer_raw, 0, main_readlength,
    main_readbuffer, 0, main_readbuffer_size, -1);
  macro_expanding = FALSE;
  }
}



/*************************************************
*      Check an input line for pre-processing    *
*************************************************/

/* This function is called when a new line has been read. If it begins with
'*' (ignoring leading spaces and tabs), it is a pre-processing directive. We
process it, and treat as an empty line. If not, we also treat it as an empty
line if we are skipping lines. */

static void
handle_preprocessing(void)
{
BOOL skip;

read_i = 0;
while ((read_c = main_readbuffer[read_i++]) == ' ' || read_c == '\t') {}

if (read_c == '*')
  {
  if (isalpha(read_c = main_readbuffer[read_i++])) preprocess_line();
    else error(ERR6);
  skip = TRUE;
  }
else skip = read_skipdepth > 0;

/* Either skip to the end of the line or reset to the start. Either way, the
current character must be a newline. */

read_c = '\n';
read_i = skip? main_readlength : 0;
}



/*************************************************
*            Get next input character            *
*************************************************/

/* This function updates the global variable read_c with the next character,
including a newline at the end of each line, leaving the global pointer read_i
pointing at the next character. It deals with macro expansions and
preprocessing directives, and it skips comments. */

void
read_nextc(void)
{
for (;;)   /* Loop until a character is obtained or very end is reached. */
  {
  if (read_i < main_readlength)
    {
    read_c = main_readbuffer[read_i++];
    return;
    }

  /* There are no more characters in the current line, so we have to read the
  next one. This loop is for handling included files. */

  for (;;)
    {
    /* Get next logical line, joining together physical lines that end with
    &&&. At end of file, check for missing "*fi" and deal with popping included
    files. */

    if (read_physical_line(0))
      {
      handle_continuation();
      if (read_skipdepth <= 0) expand_macros();
      break;
      }

    /* Handle reaching the end of an input file. Set up a suitable text for
    error reflection, but also set up as an empty line. */

    fclose(read_filehandle);
    read_filehandle = NULL;

    Ustrcpy(main_readbuffer, "---- End of file ----\n");
    read_i = main_readlength = Ustrlen(main_readbuffer);
    read_c = ENDFILE;

    /* Check for missing *fi */

    if (read_skipdepth > 0 || read_okdepth > 0) error(ERR21);

    /* End of outermost file */

    if (read_filestackptr == 0) return;

    /* Pop stack at end of an included file */

    TRACE("end of %s: popping include stack\n", read_filename);

    read_filename = read_filestack[--read_filestackptr].filename;
    read_filehandle = read_filestack[read_filestackptr].file;
    read_linenumber = read_filestack[read_filestackptr].linenumber;
    read_okdepth = read_filestack[read_filestackptr].okdepth;
    read_skipdepth = 0;
    }  /* Loop after reaching the end of an included file */

  /* Another line has been read. Check for a pre-processing directive. If
  found and processed, it leaves this as an empty line. */

  handle_preprocessing();
  }   /* Loop to get the next character */

/* Control never reaches here. */
}



/*************************************************
*            Read a PMW input file               *
*************************************************/

/* We enter with the first physical line of the line already read, because it
has been checked to see what kind of input it is. So the first thing to do is
to complete its processing as a PMW input line. */

void
pmw_read(void)
{
uint32_t movtopts = 0;

handle_continuation();
expand_macros();
handle_preprocessing();

/* Loop for reading movements. */

for (;;)
  {
  int i;
  movtstr *newmovt;
  uint16_t nextbarnumber;
  uint16_t nextbarfraction = 1;

  /* Set up or expand the vector that points to the movements. */

  if (movement_count >= movements_size)
    {
    movements_size += MOVTVECTOR_CHUNKSIZE;
    movements = realloc(movements, movements_size * sizeof(movtstr *));
    if (movements == NULL)
      error(ERR0, "re-", "movements vector", movements_size); /* Hard */
    }

  /* Set up a new movement. For the first movement, the "previous movement"
  pointer points to the default movement data. */

  premovt = (curmovt == NULL)? &default_movtstr : curmovt;
  movements[movement_count++] = newmovt = mem_get(sizeof(movtstr));
  read_init_movement(newmovt, 0, movtopts);
  curmovt = newmovt;

  /* The header is terminated either by EOF or '[' */

  read_sigc();
  pmw_read_header();
  curmovt->select_staves |= 1;     /* Stave 0 is always selected */
  if (read_c == ENDFILE) break;

  /* Read data for all the staves. After each stave, remember the final barline
  style. This is used if empty bars have to be created for this stave. */

  if (Ustrncmpic(main_readbuffer+read_i, "stave", 5) == 0 ||
      Ustrncmpic(main_readbuffer+read_i, "staff", 5) == 0)
    {
    for (;;)
      {
      read_i += 5;
      pmw_reading_stave = TRUE; 
      pmw_read_stave();
      pmw_reading_stave = FALSE; 
      barlinestyles[srs.stavenumber] = srs.barlinestyle;
      if (read_c == ENDFILE) break;
      read_nextsigc();
      if (Ustrncmpic(main_readbuffer + read_i - 1, "[stave ", 7) != 0 &&
          Ustrncmpic(main_readbuffer + read_i - 1, "[staff ", 7) != 0) break;
      }
    }

  /* Remember the highest stave used in any movement. */

  if (curmovt->laststave > main_maxstave) main_maxstave = curmovt->laststave;

  /* Create dummies for empty staves and omitted bars. */
  
  read_tidy_staves(TRUE); 

  /* Scan movement's bar index and handle printing bar numbers. Nocount bars
  are identified as x.1, x.2, etc, except that the very first bar is just 0. */

  nextbarnumber = curmovt->baroffset + 1;
  for (i = 0; i < (int)curmovt->barcount; i++)
    {
    if (curmovt->barvector[i] != 0)   /* Indicates [nocount] */
      {
      curmovt->barnocount++;
      curmovt->barvector[i] = (i == 0) ? 0 :
        ((nextbarnumber - 1) << 16 | nextbarfraction++);
      }
    else
      {
      curmovt->barvector[i] = (nextbarnumber++ << 16);
      nextbarfraction = 1;
      }
    }

  /* The only thing that can follow staves is a new movement */

  if (read_c == ENDFILE) break;
  if (Ustrncmpic(main_readbuffer + read_i - 1, "[newmovement", 12) != 0)
    error(ERR9, "[stave] or [newmovement]");  /* Hard */

  /* Read options for the new movement, checking that only one of newpage,
  thispage, or thisline is set. */

  read_i += 11;
  read_nextsigc();
  movtopts = 0;

  while (read_c != ']')
    {
    uint32_t x;
    read_nextword();
    read_sigc();
    if (Ustrcmp(read_wordbuffer, "thispage") == 0) movtopts |= mf_thispage;
    else if (Ustrcmp(read_wordbuffer, "thisline") == 0) movtopts |= mf_thisline;
    else if (Ustrcmp(read_wordbuffer, "newpage") == 0) movtopts |= mf_newpage;
    else if (Ustrcmp(read_wordbuffer, "nopageheading") == 0)
      movtopts |= mf_nopageheading;
    else if (Ustrcmp(read_wordbuffer, "uselastfooting") == 0)
      movtopts |= mf_uselastfooting;
    else error(ERR8, "\"thispage\", \"thisline\", \"newpage\", "
      "\"nopageheading\", or \"uselastfooting\"");
    x = movtopts & (mf_newpage|mf_thispage|mf_thisline);
    if ((x & (~x + 1)) != x) error(ERR135);
    }
  read_nextc();
  }   /* End movements loop */

TRACE("End PMW read\n");
}

/* End of pmw_read.c */
