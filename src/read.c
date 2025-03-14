/*************************************************
*         PMW general reading functions          *
*************************************************/

/* Copyright Philip Hazel 2024 */
/* This file created: December 2020 */
/* This file last modified: December 2024 */

#include "pmw.h"



/*************************************************
*      Compute barlength from time signature     *
*************************************************/

/*
Argument:  the time signature
Returns:   the bar length
*/

uint32_t
read_compute_barlength(uint32_t ts)
{
uint32_t m = (ts >> 16) & 255;
uint32_t n = (ts >> 8) & 255;
uint32_t d = ts & 255;

if (d == time_common) n = d = 4;
  else if (d == time_cut) n = d = 2;

return n * m * (len_semibreve/d);
}



/*************************************************
*            Initialize a new stave              *
*************************************************/

stavestr *
read_init_stave(int32_t stave, BOOL ispmw)
{
if (stave > MAX_STAVE) error(ERR84, stave);  /* Hard */
if (curmovt->stavetable[stave] != NULL) error(ERR85, stave);  /* Hard */

curmovt->stavetable[stave] = mem_get(sizeof(stavestr));
*(curmovt->stavetable[stave]) = init_stavestr;
if (stave > curmovt->laststave) curmovt->laststave = stave;

/* Additional stuff when in PMW mode */

if (ispmw)
  {
  /* The transpose parameters are in globals because they are also used during
  header reading for transposing note and key names. */

  active_transpose = curmovt->transpose;
  active_transposedaccforce = main_transposedaccforce;

  /* Copy a default set of values for reading a stave, then adjust a few from the
  current movement's parameters. */

  srs = init_sreadstr;
  srs.stavenumber = stave;
  srs.hairpinwidth = curmovt->hairpinwidth;
  srs.barlinestyle = curmovt->barlinestyle;
  srs.key = curmovt->key;
  srs.key_tp = transpose_key(srs.key);
  if (stave == 0) srs.noteflags |= nf_hidden;
  srs.notenum = curmovt->notenum;
  srs.noteden = curmovt->noteden;
  srs.required_barlength = read_compute_barlength(curmovt->time);
  srs.suspended = (curmovt->suspend_staves & (1Lu << stave)) != 0;
  }

return curmovt->stavetable[stave];
}



/*************************************************
*       Tidy omitted staves and bars             *
*************************************************/

/* Called after reading to fill in any gaps.

Argument: TRUE if reading a PMW file, FALSE for MusicXML
Returns:  nothing
*/

void
read_tidy_staves(BOOL ispmw)
{
/* Create a stave structure for omitted staves. */

for (int i = 0; i <= curmovt->laststave; i++)
  if (curmovt->stavetable[i] == NULL)
    curmovt->stavetable[i] = read_init_stave(i, ispmw);

/* Remember the last read stave, because when we get to pagination, laststave
is cut back to the highest selected stave. The last read stave is needed when
freeing memory at the end. */

curmovt->lastreadstave = curmovt->laststave;

/* Scan all staves and ensure that their bar indexes are as long as the
longest stave, setting non-existent bar pointers to point to an empty bar
structure with an appropriate barline style. */

for (int i = 0; i <= curmovt->laststave; i++)
  {
  st = curmovt->stavetable[i];
  while ((size_t)(curmovt->barcount) > st->barindex_size)
    {
    size_t size;
    st->barindex_size += BARINDEX_CHUNKSIZE;
    size = st->barindex_size * sizeof(barstr *);
    st->barindex = realloc(st->barindex, size);
    if (st->barindex == NULL) error(ERR0, "re-", "bar index", size);  /* Hard */
    }

  /* Create empty bars if necessary. */

  if (st->barcount < curmovt->barcount)
    {
    b_barlinestr *bl;
    barstr *empty_bar = mem_get(sizeof(barstr));
    read_lastitem = (bstr *)empty_bar;

    empty_bar->next = empty_bar->prev = NULL;
    empty_bar->type = b_start;
    empty_bar->repeatnumber = 0;

    bl = mem_get_item(sizeof(b_barlinestr), b_barline);
    bl->bartype = barline_normal;
    bl->barstyle = barlinestyles[i];

    for (int j = st->barcount; j < curmovt->barcount; j++)
      st->barindex[j] = empty_bar;
    }
  }

}



/*************************************************
*            Initialize a new movement           *
*************************************************/

/* If this is the first movement, premovt points to the default movement data.
Copy from the previous movement and then change values that are not carried
over.

Arguments:
  new         pointer to new movtstr, uninitialized
  unsetflags  flags to unset
  setflags    flags to set (movement type)

Returns:    nothing
*/

void
read_init_movement(movtstr *new, uint32_t unsetflags, uint32_t setflags)
{
*new = *premovt;  /* Copy from previous movement */

/* Reset certain values within the movement structure. */

new->barcount = 0;
new->baroffset = 0;
new->barvector = NULL;
new->barvector_size = 0;
new->flags = (new->flags & ~(mf_unsetflags|unsetflags)) | mf_resetflags | setflags;
new->heading = new->footing = NULL;
new->key = key_C;
new->laststave = -1;
new->layout = NULL;
new->midichanset = 0;
new->miditempochanges = NULL;
new->notenum = new->noteden = 1;
memcpy(new->note_spacing, read_absnotespacing, NOTETYPE_COUNT*sizeof(uint32_t));
new->number = movement_count;
new->select_staves = main_selectedstaves;
new->suspend_staves = 0;
new->startbracketbar = 0;
new->time = new->time_unscaled = 0x00010404;   /* 1*4/4 */
new->transpose = main_transpose;

for (int i = 0; i <= MAX_STAVE; i++) new->stavetable[i] = NULL;

/* Values that don't need to be kept with the movement data. */

read_headmap = 0;   /* Map of which headings/footing have been seen. */
read_nextheadsize = (movement_count == 1)? 0:1;  /* First size for 1st movt only */

active_transpose = new->transpose;
active_transposedaccforce = FALSE;
(void)transpose_key(new->key);
}



/*************************************************
*      Convert string to stave map or chain      *
*************************************************/

/* The turns strings like "1,3,4-6,10" into a bitmap or a chain of stavelist
blocks. If ss is used directly with strtol(), even with a cast to char *, gcc
grumbles about the lack of a "restrict" qualifier. It is less hassle to use
another variable of type char *.

Arguments:
  ss         the string
  endptr     where to return a pointer to the char after the last used
  map        pointer to bitmap or NULL
  slp        pointer to stavelist anchor or NULL

Returns:     zero or an error number
*/

int
read_stavelist(uschar *ss, uschar **endptr, uint64_t *map, stavelist **slp)
{
stavelist *prev = NULL;
char *sss = (char *)ss;

*endptr = ss;
if (map != NULL) *map = 0;
if (slp != NULL) *slp = NULL;

while (isdigit((unsigned char)*sss))
  {
  long int s = strtol(sss, &sss, 0);
  long int t = s;

  if (*sss == '-')
    {
    sss++;
    t = strtol(sss, &sss, 0);
    }

  if (t < s) return ERR34;
  if (t > MAX_STAVE) return ERR10;

  if (map != NULL) for (long int i = s; i <= t; i++) *map |= 1ul << i;
  if (slp != NULL)
    {
    stavelist *sl = mem_get(sizeof(stavelist));
    sl->first = s;
    sl->last = t;
    sl->next = NULL;
    sl->prev = prev;
    if (prev == NULL) *slp = sl; else prev->next = sl;
    prev = sl;
    }

  while (*sss == ',' || *sss == ' ') sss++;
  }
*endptr = US sss;
return 0;
}



/*************************************************
*           Extend input buffers                 *
*************************************************/

/* When the main readbuffer is too small it means the input is crazy. Rather
than showing thousands of characters in the error message, truncate it. Setting
read_i and macro_i to zero (to cover both the cases of main input or expanded
input) stops the output of a current pointer. */

void
read_extend_buffers(void)
{
if (main_readbuffer_size >= MAIN_READBUFFER_SIZELIMIT)
  {
  Ustrcpy(main_readbuffer+50, "..... etc ......\n");
  read_i = macro_in = 0;
  error(ERR5, "input", MAIN_READBUFFER_SIZELIMIT);  /* Hard */
  }
main_readbuffer_size += MAIN_READBUFFER_CHUNKSIZE;
main_readbuffer = realloc(main_readbuffer, main_readbuffer_size);
main_readbuffer_previous = realloc(main_readbuffer_previous,
  main_readbuffer_size);
main_readbuffer_raw = realloc(main_readbuffer_raw, main_readbuffer_size);
if (main_readbuffer == NULL || main_readbuffer_previous == NULL ||
    main_readbuffer_raw == NULL)
  error(ERR0, "re-", "line buffers", main_readbuffer_size);     /* Hard */
main_readbuffer_threshold = main_readbuffer_size - 2;
}



/*************************************************
*                 Manage bar indexes             *
*************************************************/

/* The bar indexes for movements and staves are expandable as necessary. This
function handles that. The current movement is in curmovt and the current stave
in st.

Argument:  size needed
Returns:   nothing
*/

void
read_ensure_bar_indexes(size_t needed)
{
while (needed >= curmovt->barvector_size)
  {
  size_t size;
  curmovt->barvector_size += BARINDEX_CHUNKSIZE;
  size = curmovt->barvector_size * sizeof(uint32_t);
  curmovt->barvector = realloc(curmovt->barvector, size);
  if (curmovt->barvector == NULL) error(ERR0, "re-",
    "movement bar index", size);  /* Hard */
  memset(curmovt->barvector + curmovt->barvector_size -
    BARINDEX_CHUNKSIZE, 0, BARINDEX_CHUNKSIZE * sizeof(uint32_t));
  }

while (needed >= st->barindex_size)
  {
  size_t size;
  st->barindex_size += BARINDEX_CHUNKSIZE;
  size = st->barindex_size * sizeof(barstr *);
  st->barindex = realloc(st->barindex, size);
  if (st->barindex == NULL) error(ERR0, "re-", "bar index", size);  /* Hard */
  }
}



/*************************************************
*        Read the next physical input line       *
*************************************************/

/* When reading a non-continuation line (i == 0), unless the current line is
empty, swap the buffers to make it the previous line, for use by the error
function.

Argument:  starting offset in line buffer
Returns:   TRUE if line read, FALSE at EOF
*/

BOOL
read_physical_line(size_t i)
{
BOOL binfound = FALSE;
size_t binoffset;

if (i == 0 && main_readbuffer[0] != 0 && main_readbuffer[0] != '\n')
  {
  uschar *temp = main_readbuffer_previous;
  main_readbuffer_previous = main_readbuffer;
  main_readbuffer = temp;
  }

read_linenumber++;

for (;;)
  {
  int ch = fgetc(read_filehandle);

  /* Binary zeros are not supported. Remember where the first one was. */

  if (ch == 0)
    {
    if (!binfound)
      {
      binoffset = i;
      if (binoffset == 0) binoffset++;
      binfound = TRUE;
      }
    continue;
    }

  /* At end of file, invent a missing newline. */

  if (ch < 0)
    {
    if (i == 0) return FALSE;
    ch = '\n';
    }

  /* Ensure enough space for at least two more bytes */

  if (i > main_readbuffer_threshold) read_extend_buffers();

  /* Remove any white space before the terminating newline. */

  if (ch == '\n')
    {
    while (i > 0 && isspace(main_readbuffer[i-1])) i--;
    main_readbuffer[i++] = '\n';
    main_readbuffer[i] = 0;
    break;
    }

  main_readbuffer[i++] = ch;
  }

/* Give an error if any binary zeros were found - can't do earlier as we need
the complete line for reflection. */

if (binfound)
  {
  read_i = binoffset;
  error(ERR2);
  }

main_readlength = i;
return TRUE;
}


/*************************************************
*              Read an input file                *
*************************************************/

void
read_file(enum filetype ft)
{
uschar *p;

if (!read_physical_line(0)) return;  /* Empty file */

/* Ignore a Unicode BOM at the start of the file. */

p = main_readbuffer;
if (Ustrncmp(main_readbuffer, "\xef\xbb\xbf", 3) == 0) p += 3;

if (Ustrncmp(p, "%abc-", 5) == 0)
  {
  TRACE("ABC file detected\n");
  read_i = Ustrlen(main_readbuffer);
  error(ERR3, "ABC");    /* Hard - in future will be tested */
  if (ft != FT_AUTO || ft != FT_ABC)
    error(ERR4, "ABC");  /* Hard */
  }

else if (Ustrncmp(p, "<?xml version=", 14) == 0)
  {
  TRACE("MusicXML file detected\n");
  read_i = Ustrlen(main_readbuffer);
#if !SUPPORT_XML
  error(ERR3, "MusicXML");    /* Hard */
#else
  if (ft != FT_AUTO && ft != FT_MXML)
    error(ERR4, "MusicXML");  /* Hard */
  xml_read();
#endif
  }

else
  {
  TRACE("PMW file assumed\n");
  pmw_read();
  }

read_linenumber = 0;  /* No longer in reading phase */

/* Warn for unsupported Unicode code points. */

if (read_uunext > 0)
  {
  fprintf(stderr, "** Warning: the following unsupported Unicode code point%s "
    "been changed\nto U+%04X:", (read_uunext == 1)? " has":"s have",
    UNKNOWN_CHAR_S);
  while (read_uunext > 0) fprintf(stderr, " U+%04X",
    read_unsupported_unicode[--read_uunext]);
  if (read_uuoverflow) fprintf(stderr, " ...");
  fprintf(stderr, "\n\n");
  }

/* Warn for invalid Unicode code points in non-standardly encode fonts. */

if (read_uinvnext > 0)
  {
  fprintf(stderr, "** Warning: in one or more non-standardly encoded fonts, "
    "the following\nunsupported Unicode code point%s been changed to a "
    "font-specific code point\nor to 0x%02X in "
    "the Music font:", (read_uinvnext == 1)? " has":"s have", UNKNOWN_CHAR_N);
  while (read_uinvnext > 0) fprintf(stderr, " U+%04X",
    read_invalid_unicode[--read_uinvnext]);
  if (read_uinvoverflow) fprintf(stderr, " ...");
  fprintf(stderr, "\n\n");
  }

DEBUG(D_movtflags)
  {
  usint i;
  for (i = 0; i < movement_count; i++)
    {
    eprintf("\nMOVEMENT %d\n", i+1);
    eprintf("  flags = 0x%08x\n", (movements[i])->flags);
    }
  }

DEBUG(D_header_all|D_header_glob) debug_header();
DEBUG(D_bar) debug_bar();
}

/* End of read.c */
