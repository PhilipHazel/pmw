/*************************************************
*             MusicXML input for PMW             *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last modified: January 2022 */


/* This module contains functions for generating stave data */


#include "pmw.h"




#ifdef NEVER


enum { REHEARSAL_TYPE_ROMAN,
       REHEARSAL_TYPE_ITALIC,
       REHEARSAL_TYPE_BOLD,
       REHEARSAL_TYPE_BOLDITALIC };

enum { REHEARSAL_ENCLOSURE_BOX,
       REHEARSAL_ENCLOSURE_RING };

enum { TIED_NONE, TIED_DEFAULT, TIED_ABOVE, TIED_BELOW };

// enum { PBRK_UNSET, PBRK_UNKNOWN, PBRK_YES, PBRK_NO };

#define PBRK_YES     0
#define PBRK_NO      (-1)
#define PBRK_UNSET   (-2)
#define PBRK_UNKNOWN (-3)

#define TURNS_ABOVE  0x00000001u
#define TURNS_BELOW  0x00000002u
#define TURNS_BOTH   0x00000004u
#define TURNS_PLAIN  0x00000008u

#define MAX_CUSTOM_KEYS 10


/*************************************************
*             Local structures                   *
*************************************************/

/* Structure for handling in-memory buffered staff output */

typedef struct {
  uschar *buffer;
  size_t size;
  size_t hwm;
  BOOL   lastwasnl;
} stave_buffer;

/* Structure for remembering the start of a slur or line */

typedef struct sl_start {
  struct sl_start *next;
  size_t offset;
  int pstaff;
  int id;
  int pmw_id;
  int default_y;
} sl_start;

/* Structure for remembering default-x settings for notes so that after
<backup> other notes can be moved. */

typedef struct note_position {
  int moff;    /* Musical offset */
  int x;       /* default-x value */
} note_position;

/* Structure for remember which makekey settings have already been output.
These are used for non-traditional key signatures. */

typedef struct nt_save {
  struct nt_save *next;
  int key;
  char defstring[100];
} nt_save;



/*************************************************
*             Static variables                   *
*************************************************/

static BOOL    all_pending[64];
static int     clef_octave_change[PARTSTAFFMAX+1];
static int     current_clef[64];
static int     current_octave[PARTSTAFFMAX+1];
static int     divisions = 8;
static int     duration[PARTSTAFFMAX+1];
static int     measure_length;
static int     measure_number;
static int     measure_number_absolute;
static int     next_measure_fraction;
static uschar *part_id;
static int     pending_backup[PARTSTAFFMAX+1];
static int     next_custom_key = 1;
static int     rehearsal_enclosure = -1;  /* Unset */
static int     rehearsal_size = -1;
static int     rehearsal_type = -1;
static char    save_clef[PARTSTAFFMAX+1][24];
static char    save_key[PARTSTAFFMAX+1][24];
static char    save_ssabove[PARTSTAFFMAX+1][24];
static char    save_time[PARTSTAFFMAX+1][24];
static int     time_num;
static int     time_den;
static BOOL    tuplet_size_set = FALSE;
static int     turns_used = 0;

static int     open_wedge_type[64];
static size_t  open_wedge_start[64];
static stave_buffer pmw_buffers[64];
static int     set_noteheads[64];
static BOOL    set_stems[64];
static size_t  staff_bar_start[64];

static sl_start *slur_free_starts = NULL;
static sl_start *slur_starts = NULL;

static sl_start *line_free_starts = NULL;
static sl_start *line_starts = NULL;

static nt_save  *nt_save_list = NULL;



/*************************************************
*              Local tables                      *
*************************************************/

typedef struct {
  uschar *name;
  uschar *after;
  BOOL    upper;
  int     abbrev;
} note_type;

static note_type note_types[] = {
  { US"whole",   US"+",   TRUE, 's' },
  { US"half",    US"",    TRUE, 'm' },
  { US"quarter", US"",   FALSE, 'c' },
  { US"eighth",  US"-",  FALSE, 'Q' },
  { US"16th",    US"=",  FALSE, 'q' },
  { US"32nd",    US"=-", FALSE, -1  },
  { US"64th",    US"==", FALSE, -1  }
};

static int note_types_count = (sizeof(note_types)/sizeof(note_type));


typedef struct {
  uschar *name;
  uschar *string;
  BOOL used;
} dynamic_type;

static dynamic_type dynamics[] = {
  { US"f",   US"\\bi\\f",         FALSE },
  { US"p",   US"\\bi\\p",         FALSE },
  { US"ff",  US"\\bi\\ff",        FALSE },
  { US"fp",  US"\\bi\\fp",        FALSE },
  { US"pp",  US"\\bi\\pp",        FALSE },
  { US"mf",  US"\\it\\m\\bi\\f",  FALSE },
  { US"sf",  US"\\it\\s\\bi\\f",  FALSE },
  { US"mp",  US"\\it\\m\\bi\\p",  FALSE },
};

static int dynamics_count = (sizeof(dynamics)/sizeof(dynamic_type));


static const char *barstyles[] = {
  "regular",     " |",
  "dotted",      " :|?",
  "dashed",      " |1",
  "heavy",       " \"\\mf\\B\"/b0 |?",
  "light-light", " ||",
  "light-heavy", " |||",
  "heavy-light", " \"\\mf\\B{y@\"/b0 |?",
  "heavy-heavy", " \"\\mf\\B{yB\"/b0 |?",
  "tick",        " \"\\mf\\xxx~\\132\\\"/b0 |?",
  "short",       " |4",
  "none",        " |?",
};

static int barstyles_count = (sizeof(barstyles)/sizeof(char *));


static const char *irests[] = {
  "Q+", "Q", "q", "q-", "q=", "q=-", "q==" };

/* PMW key signatures, indexed by number of accidentals, offset by 7 for major
keys and offset by 22 for minor ones. */

static const char *keys[] = {
  "C$",  "G$",  "D$",  "A$",  "E$",  "B$",  "F",
  "C",   "G",   "D",   "A",   "E",   "B"    "F#",  "C#",
  "A$m", "E$m", "B$m", "Fm",  "Cm",  "Gm",  "Dm",
  "Am",  "Em",  "Bm",  "F#m", "C#m", "G#m", "D#m", "A#m" };

enum { Clef_alto, Clef_baritone, Clef_bass, Clef_cbaritone, Clef_contrabass,
       Clef_deepbass, Clef_hclef, Clef_mezzo, Clef_noclef, Clef_soprabass,
       Clef_soprano, Clef_tenor, Clef_treble, Clef_trebledescant,
       Clef_trebletenor, Clef_trebletenorb };

/* Not currently needed -- keep in case a need arises
static const char *clef_names[] = {
  "alto", "baritone", "bass", "cbaritone", "contrabass", "deepbass", "hclef",
  "mezzo", "noclef", "soprabass", "soprano", "tenor", "treble",
  "trebledescant", "trebletenor", "trebletenorb" };
-- */

/* Position of middle C in each clef, where the bottom line of the staff is
numbered 0. For clefs that imply octave transposition for performance, this is
ignored. */

static int clef_cposition[] = {
   4, /* alto */         8, /* baritone */       10, /* bass */
   8, /* cbaritone */   10, /* contrabass */     12, /* deepbass */
  -2, /* hclef */        2, /* mezzo */          -2, /* noclef */
  12, /* soprabass */    0, /* soprano */         6, /* tenor */
  -2, /* treble */      -2, /* trebledescant */  -2, /* trebletenor */
  -2, /* trebletenorb */ };

static int cclefs[] = {
  -1, Clef_soprano, Clef_mezzo, Clef_alto, Clef_tenor, Clef_cbaritone };

static const char *cclefnames[] = {
  "", "soprano", "mezzo", "alto", "tenor", "cbaritone" };

static const int cclef_octaves[] = { 0, 1, 1, 1, 0, 0 };

/* In the accidentals list, the second column contains the PMW code for use in
notes and key signatures, and the third contains the character to be used in
text string (these are used for accidentals above/below turns). Character
numbers are used to avoid & and ( because these strings are used in macro
calls. */

static const char *accidentals[] = {
  "sharp",         "#",  "%",
  "flat",          "$",  "'",
  "double-sharp",  "##", "\\38\\",
  "flat-flat",     "$$", "''",
  "natural",       "%",  "\\40\\",
  "slash-flat",    "$-", "\\191\\",
  "quarter-sharp", "#-", "\\189\\",
  "UNKNOWN",       " ",  "" };

static int accidentals_count = (sizeof(accidentals)/sizeof(char *));

static const char *fonttypes[] = { "\\rm\\", "\\it\\", "\\bf\\", "\\bi\\" };

static const char *pmw_articulations[] = {
  "accent",           ">",
  "breath-mark",      " [comma]",  /* Leading space is important to */
  "caesura",          " //",       /* differentiate from accents */
  "detached-legato",  ".-",
  "spiccato",         "..",
  "staccatissimo",    "v",
  "staccato",         ".",
  "strong-accent",    "V",
  "tenuto",           "-",
  NULL };

static const char *ordabbrev[] =
  { "", "st", "nd", "rd", "th", "th", "th", "th", "th", "th" };




/*************************************************
*      Put text into a stave-specific buffer     *
*************************************************/

/* This function adds the text onto the end of whatever is in the buffer.
Spaces at the start of a line are ignored. */

static void
stave_text(int pstaff, const char *format, ...)
{
size_t len;
va_list ap;
stave_buffer *b = pmw_buffers + pstaff;
char temp[256];
char *p = temp;

va_start(ap, format);
len = vsnprintf(temp, sizeof(temp), format, ap);
va_end(ap);

if (b->lastwasnl)
  {
  while (*p == ' ')
    {
    p++;
    len--;
    }
  }
if (len == 0) return;

b->lastwasnl = p[len-1] == '\n';

while (len >= b->size - b->hwm)
  {
  size_t oldsize = b->size;
  b->size += STAVEBUFFERMIN;
  b->buffer = misc_realloc(b->buffer, b->size, oldsize);
  }

(void)Ustrcpy(b->buffer + b->hwm, p);
b->hwm += len;
}


/* This function inserts the text at a given offset in the buffer, moving up
later text to make space. It's simpler to have two functions than to amalgamate
them. Because there may be several remembered insertion places, we have to scan
them and adjust if necessary. */

static void
stave_text_insert(int pstaff, size_t offset, const char *format, ...)
{
sl_start *sl;
size_t len;
va_list ap;
stave_buffer *b = pmw_buffers + pstaff;
uschar *p = b->buffer + offset;
char temp[256];

va_start(ap, format);
len = vsnprintf(temp, sizeof(temp), format, ap);
va_end(ap);

while (len >= b->size - b->hwm)
  {
  size_t oldsize = b->size;
  b->size += STAVEBUFFERMIN;
  b->buffer = misc_realloc(b->buffer, b->size, oldsize);
  }

if (offset > b->hwm) error(ERR40, offset, b->hwm);
(void)memmove(p + len, p, b->hwm - offset + 1);
(void)memcpy(p, temp, len);
b->hwm += len;

/* Scan remembered insertion points. */

for (sl = slur_starts; sl != NULL; sl = sl->next)
  if (sl->offset >= offset) sl->offset += len;

for (sl = line_starts; sl != NULL; sl = sl->next)
  if (sl->offset >= offset) sl->offset += len;

if (open_wedge_start[pstaff] >= offset) open_wedge_start[pstaff] += len;
if (staff_bar_start[pstaff] >= offset) staff_bar_start[pstaff] += len;
}



/*************************************************
*         Write parts from timewise XML          *
*************************************************/

static BOOL
write_timewise(void)
{
return TRUE;  /* Not yet supported */
}



/*************************************************
*             Write invisible rests              *
*************************************************/

/* Used for both <forward> and <backup>

Arguments:
  pstaff          PMW staff to write on
  rest_duration   length of rests

Returns:     nothing
*/

static void
write_invisible_rests(int pstaff, int rest_duration)
{
int i, j;

for (i = 4, j = 0; i > 0; i /= 2)
  {
  while (rest_duration > 0 && rest_duration >= divisions * i)
    {
    rest_duration -= divisions * i;
    stave_text(pstaff, "%s", irests[j]);
    }
  j++;
  }

for (i = 2; i <= 16; i *= 2)
  {
  while (rest_duration > 0 && rest_duration >= divisions/i)
    {
    rest_duration -= divisions/i;
    stave_text(pstaff, "%s", irests[j]);
    }
  j++;
  }
}



/*************************************************
*         Write one note or rest letter          *
*************************************************/

/*
Arguments:
  pstave     PMW stave number
  note       note item (used for error line number)
  type       note type
  letter     note letter
  octave     PMW octave change from current
  dots       number of dots

Returns:     nothing
*/

static void
write_noteletter(int pstave, item *note, uschar *type, int letter, int octave,
  int dots)
{
int i;

for (i = 0; i < note_types_count; i++)
  if (Ustrcmp(type, note_types[i].name) == 0) break;

if (i >= note_types_count) Eerror(note, ERR18, type);  /* Hard */
stave_text(pstave, "%c", (note_types[i].upper)? toupper(letter):tolower(letter));

if (octave != 0)
  {
  int ochar;
  if (octave < 0)
    {
    ochar = '`';
    octave = - octave;
    }
  else ochar = '\'';
  while (octave-- > 0) stave_text(pstave, "%c", ochar);
  }

stave_text(pstave, "%s", note_types[i].after);
while (dots-- > 0) stave_text(pstave, ".");
}



/*************************************************
*       Output quoted string in staff            *
*************************************************/

/* Front and back wrapping text can be provided.

Arguments:
  n             PMW stave number
  s             the basic string
  p             pre-text
  f             front insert text
  b             back insert text

Returns:        nothing
*/

static void
stave_text_quoted_string(int n, uschar *s, const char *p, uschar *f, uschar *b)
{
stave_text(n, "%s\"", p);
if (f != NULL) stave_text(n, "%s", f);
for (; *s != 0; s++)
  if (*s == '\"') stave_text(n, "\\x22\\");
    else stave_text(n, "%c", *s);
if (b != NULL) stave_text(n, "%s", b);
stave_text(n, "\"");
}



/*************************************************
*           Conditionally insert space           *
*************************************************/

/* This is used to insert a space after alphanumeric items. The input must
already be a string. */

static void
conditional_space(char *s)
{
size_t len = Ustrlen(s);
if (len > 0 && isalnum(s[len-1])) Ustrcat(s, " ");
}


/*************************************************
*      Handle absolute vertical positioning      *
*************************************************/


static void
handle_dy(int dy, int pstaff, BOOL above)
{
if (dy == 0) stave_text(pstaff, above? "/a" : "/b"); else
  {
  dy *= 400;  /* Convert tenths to millipoints */

  /* This is messy. It seems that default-y is relative to the top of
  the stave. If it is positive, generate a PMW "above" string at a
  fixed place. If negative, we can use a "below" string only if it is
  below the stave, because /a and /b can only be followed by unsigned
  numbers. So for strings positioned inside the stave, use /a0 or /b0 with a
  following movement. Taking note of "above" is important because whether /a0
  or /b0 is used may affect things, for example, the jogs on line. */

  if (dy > 0)
    stave_text(pstaff, "/a%s", format_fixed(dy));
  else
    {
    dy = -dy;
    if (dy > 16000)
      stave_text(pstaff, "/b%s", format_fixed(dy - 16000));
    else
      {
      if (above)
        stave_text(pstaff, "/a0/d%s", format_fixed(dy));
      else
        stave_text(pstaff, "/b0/u%s", format_fixed(16000-dy));
      }
    }
  }
}



/*************************************************
*          Handle relative positioning           *
*************************************************/

static void
leftright(int pstaff, int x, const char *pre)
{
if (x != 0) stave_text(pstaff, "/%s%c%s", pre, (x > 0)? 'r':'l',
  format_fixed(abs(x)*400));
}

static void
updown(int pstaff, int y, const char *pre)
{
if (y != 0) stave_text(pstaff, "/%s%c%s", pre, (y > 0)? 'u':'d',
  format_fixed(abs(y)*400));
}

static void
leftright_insert(int pstaff, size_t offset, int x, const char *pre)
{
if (x != 0) stave_text_insert(pstaff, offset, "/%s%c%s", pre, (x > 0)? 'r':'l',
  format_fixed(abs(x)*400));
}

static void
updown_insert(int pstaff, size_t offset, int y, const char *pre)
{
if (y != 0) stave_text_insert(pstaff, offset, "/%s%c%s", pre, (y > 0)? 'u':'d',
  format_fixed(abs(y)*400));
}


/* This is used for offsets on things other than text, which use the special
/lc and /rc positioning that is available there. For wedges (hairpins), the rx
value is relative to the next note position. We fudge this by adding /lc0.001
(can't use zero, because that means none). */

static void
rxandoffset(int pstaff, int rx, int offset, BOOL rxnext)
{
if (rx != 0)  /* relative-x is specified */
  {
  if (rxnext) stave_text(pstaff, "/lc0.001");
  stave_text(pstaff, "/%c%s", (rx > 0)? 'r':'l',
    format_fixed(abs(rx*400)));
  }

else if (offset != 0)
  stave_text(pstaff, "/%cc%s", (offset > 0)? 'r':'l',
    format_fixed((abs(offset)*1000)/divisions));
}



/*************************************************
*         Add a font size to a string            *
*************************************************/

/* Note that PMW /s sizes start at 1, not 0. */

static void
write_fontsize(int pstaff, int fsize)
{
int n;
if (fsize <= 0) return;
for (n = 0; n < fontsize_next; n++) if (fontsizes[n] == fsize) break;
if (n >= fontsize_next) fontsizes[fontsize_next++] = fsize;
if (n != 0) stave_text(pstaff, "/s%d", n+1);
}



/*************************************************
*             Handle pending backup              *
*************************************************/

/* Handle a pending backup; ignore if at the start of a measure. The pending
backup value is where we want to be in the measure - computed from the staff
before <backup>. Check for following a tie - PMW cannot support this, so give
warning and remove the tie. It is always safe to look behind several characters
because the stave start must have already been output. */

static void
do_pending_backup(int staff, int pstaff, item *where)
{
if (pending_backup[staff] >= 0)
  {
  if (duration[staff] > 0)
    {
    stave_buffer *b = pmw_buffers + pstaff;
    uschar *s = b->buffer;
    size_t x = b->hwm;

    while (s[x-1] == ' ') x--;
    if (s[x-1] == ';') x--;
    if (s[x-1] == '_')
      {
      Eerror(where, ERR53);
      x--;
      b->hwm = x;
      }
    else if (Ustrncmp(s + x - 3, "_/a", 3) == 0 ||
             Ustrncmp(s + x - 3, "_/b", 3) == 0)
      {
      Eerror(where, ERR53);
      x -= 3;
      b->hwm = x;
      }

    stave_text(pstaff, "\n  [reset]");
    write_invisible_rests(pstaff, pending_backup[staff]);
    duration[staff] = pending_backup[staff];
    }
  pending_backup[staff] = -1;
  }
}



/*************************************************
*           Do non-standard key signature        *
*************************************************/

/* Create a makekey directive for PMW, keeping track of which ones have
already been used.

Arguments:
  pstaff       PMW staff number
  nskptr       pointer to nsk data

Returns:       update to nskptr
*/

static int *
do_nsk(int pstaff, int *nskptr)
{
int pkey;
nt_save *s;
char buffer[100];
char *p = buffer;

while (*nskptr > 0)
  {
  int step = *nskptr++;
  int acc = *nskptr++;
  p += sprintf(p, "%s", accidentals[acc + 1]);
  p += sprintf(p, "%d", step + 3);
  }

/* Search to see if this key has already been seen. */

for (s = nt_save_list; s != NULL; s = s->next)
  {
  if (strcmp(buffer, s->defstring) == 0) break;
  }

/* Found; get the custom key */

if (s != NULL)
  {
  pkey = s->key;
  }

/* Not found; create a custom key */

else
  {
  if (next_custom_key > MAX_CUSTOM_KEYS) error(ERR57);  /* Hard error */
  pkey = next_custom_key++;
  fprintf(outfile, "Makekey X%d %s\n", pkey, buffer);

  s = misc_malloc(sizeof(nt_save));
  s->next = nt_save_list;
  nt_save_list = s;
  s->key = pkey;
  Ustrcpy(s->defstring, buffer);
  }

stave_text(pstaff, "[key X%d]\n", pkey);
return nskptr + 1;
}



/*************************************************
*                Write one measure               *
*************************************************/

/*
Arguments:
  pstave    previous PMW stave
  p         the part data block
  measure   points to <measure>

Returns:    1 for a normal measure; 0 for an implicit (uncounted) measure
*/

static int
write_measure(int pstave, part_data *p, item *measure)
{
item *mi;
int n;
int yield;
int pstaff, staff;
int scount = p->stave_count;
int note_positions_next = 0;
int pending_beambreak[PARTSTAFFMAX+1];
int pending_tie[PARTSTAFFMAX+1];

note_position note_positions[100];
char pending_post_chord[PARTSTAFFMAX+1][32];
char pending_post_note[PARTSTAFFMAX+1][32];
uschar *measure_number_text[16];

BOOL hadrepeat;
BOOL starting_noprint = FALSE;
BOOL lastwasbarline;
BOOL inchord[PARTSTAFFMAX+1];
BOOL incue[PARTSTAFFMAX+1];
BOOL end_tuplet[PARTSTAFFMAX+1];

for (n = 0; n <= PARTSTAFFMAX; n++)
  {
  inchord[n] = incue[n] = end_tuplet[n] = FALSE;
  pending_beambreak[n] = PBRK_UNKNOWN;
  pending_tie[n] = TIED_NONE;
  pending_post_chord[n][0] = 0;
  pending_post_note[n][0] = 0;
  duration[n] = 0;
  pending_backup[n] = -1;
  staff_bar_start[pstave+n] = pmw_buffers[pstave+n].hwm;
  }

staff = 0;
hadrepeat = lastwasbarline = FALSE;

/* Handle end of repeated bars */

for (n = 1; n <= scount; n++)
  {
  if (all_pending[pstave + n])
    {
    stave_text(pstave + n, "[all]");
    all_pending[pstave + n] = FALSE;
    }
  }

/* The "implicit" attribute means this measure it not counted. We assume it to
mean no length check either. Arrange for the measure number that is shown as a
comment to be in PMW style. */

if (ISATTR(measure, "implicit", "no", FALSE, "yes"))
  {
  for (n = 1; n <= scount; n++)
    stave_text(pstave + n, "[nocount nocheck]\n");
  if (measure_number == 1 && next_measure_fraction == 0)
    {
    sprintf(CS measure_number_text, "0");
    next_measure_fraction = 1;
    }
  else
    {
    sprintf(CS measure_number_text, "%d.%d", measure_number - 1,
      next_measure_fraction++);
    }
  yield = 0;
  }

/* Normal measure */

else
  {
  sprintf(CS measure_number_text, "%d", measure_number);
  yield = 1;
  next_measure_fraction = 1;
  }

/* Handle special cases when the printing start of this part is beyond the
first measure. */

if (p->noprint_before > 1)
  {
  /* Measure within initial non-printing measures. Set a flag to cause certain
  items (e.g. clef) to be remembered rather than output. */

  if (measure_number_absolute < p->noprint_before)
    {
    starting_noprint = TRUE;
    }

  /* First printing measure. Output remembered items if this measure doesn't
  have them. */

  else if (measure_number_absolute == p->noprint_before)
    {
    item *attributes, *print;
    BOOL hasclef = FALSE;
    BOOL haskey = FALSE;
    BOOL hastime = FALSE;
    BOOL hasssabove = FALSE;

    for (attributes = xml_find_item(measure, US"attributes");
         attributes != NULL;
         attributes = xml_find_next(measure, attributes))
      {
      hasclef |= xml_find_item(attributes, US"clef") != NULL;
      haskey |= xml_find_item(attributes, US"key") != NULL;
      hastime |= xml_find_item(attributes, US"time") != NULL;
      }

    for (print = xml_find_item(measure, US"print");
         print != NULL;
         print = xml_find_next(measure, print))
      {
      item *layout = xml_find_item(print, US"staff-layout");
      if (layout != NULL)
        hasssabove = xml_find_item(layout, US"staff-distance") != NULL;
      }

    for (n = 1; n <= scount; n++)
      {
      if (!hasclef) stave_text(pstave + n, "%s", save_clef[n]);
      if (!haskey) stave_text(pstave + n, "%s", save_key[n]);
      if (!hastime) stave_text(pstave + n, "%s", save_time[n]);
      if (!hasssabove) stave_text(pstave + n, "%s", save_ssabove[n]);
      }
    }
  }

/* Measure before the first time signature */

if (measure_number_absolute < time_signature_seen)
  for (n = 1; n <= scount; n++)
    stave_text(pstave + n, "[nocheck]\n");

/* Scan all the top-level elements in the measure, in order. */

for (mi = measure->next; mi != measure->partner; mi = mi->partner->next)
  {
  BOOL invisible = FALSE;
  lastwasbarline = FALSE;

  DEBUG(D_write) debug_printf("<%s>\n", mi->name);

  /* Many elements can have an optional <staff> element. If not present,
  assume staff 1, with pstaff being the PMW staff number. */

  staff = xml_get_number(mi, US"staff", 1, 500, 1, FALSE);
  pstaff = pstave + staff;

  /* If the element has print-object="no", skip it, unless it also has
  print-spacing="yes", in which case set a flag. */

  if (ISATTR(mi, US"print-object", "yes", FALSE, "no"))
    {
    if (ISATTR(mi, US"print-spacing", "yes", FALSE, "yes"))
      invisible = TRUE;
    else
      {
      if (Ustrcmp(mi->name, "note") == 0)
        {
        int note_duration = xml_get_number(mi, US"duration", 0, 10000, 0, FALSE);
        if (note_duration > 0)
          {
          if (pending_backup[staff] < 0)
            pending_backup[staff] = duration[staff];
          pending_backup[staff] += note_duration;
          }
        }
      continue;
      }
    }


  /* ======================== Attributes ======================== */

  if (Ustrcmp(mi->name, "attributes") == 0)
    {
    item *ai;

    /* Scan through the elements contained in <attributes>. */

    for (ai = mi->next; ai != mi->partner; ai = ai->partner->next)
      {
      BOOL noprint = ISATTR(ai, US"print-object", "yes", FALSE, "no");
      const char *assume = noprint? "assume " : "";


      /* ==== Divisions attribute ==== */

      if (Ustrcmp(ai->name, "divisions") == 0)
        {
        divisions = xml_get_this_number(ai, 1, 1024*1024, 8, TRUE);
        }


      /* ==== Clef attribute ==== */

      else if (Ustrcmp(ai->name, "clef") == 0)
        {
        int o;
        uschar *s;
        const char *clefname = NULL;

        /* Each clef has its own staff specified as "number". */

        staff = xml_get_attr_number(ai, US"number", 1, 10, 1, FALSE);
        pstaff = pstave + staff;

        s = xml_get_string(ai, US"sign", US"", TRUE);
        o = xml_get_number(ai, US"clef-octave-change", -1, +1, 0, FALSE);
        n = xml_get_number(ai, US"line", 1, 5, -1, FALSE);

        if (Ustrcmp(s, "G") == 0)
          {
          current_octave[staff] = 1;
          if (n >= 0 && n != 2) Eerror(mi, ERR41, 'G', "2", n);
          switch (o)
            {
            case -1:
            clefname = "trebletenor";
            current_clef[pstaff] = Clef_trebletenor;
            break;

            case +1:
            clefname = "trebledescant";
            current_clef[pstaff] = Clef_trebledescant;
            break;

            default:
            clefname = "treble";
            current_clef[pstaff] = Clef_treble;
            break;
            }
          }
        else if (Ustrcmp(s, "F") == 0)
          {
          current_octave[staff] = 0;
          if (n == 5)
            {
            clefname = "deepbass";
            current_clef[pstaff] = Clef_deepbass;
            }
          else
            {
            if (n >= 0 && n != 4) Eerror(mi, ERR41, 'F', "4 or 5", n);
            switch(o)
              {
              case -1:
              clefname = "contrabass";
              current_clef[pstaff] = Clef_contrabass;
              break;

              case +1:
              clefname = "soprabass";
              current_clef[pstaff] = Clef_soprabass;
              break;

              default:
              clefname = "bass";
              current_clef[pstaff] = Clef_bass;
              break;
              }
            }
          }
        else if (Ustrcmp(s, "C") == 0)
          {
          current_octave[staff] = cclef_octaves[n];
          clefname = cclefnames[n];
          current_clef[pstaff] = cclefs[n];
          }
        else if (Ustrcmp(s, "percussion") == 0)
          {
          if (n >= 0) Eerror(mi, ERR42, n);
          current_octave[staff] = 1;
          clefname = "hclef";
          current_clef[pstaff] = Clef_hclef;
          }
        else
          {
          Eerror(ai, ERR25, s);
          }

        if (starting_noprint)
          sprintf(save_clef[staff], "[%s %d]\n", clefname, current_octave[staff]);
        else
          stave_text(pstaff, "[%s %d]\n", clefname, current_octave[staff]);

        clef_octave_change[staff] = -o;
        }


      /* ==== Key signature attribute ==== */

      else if (Ustrcmp(ai->name, "key") == 0)
        {
        uschar *s;
        int sn;
        int offset = 7;
        BOOL follows_repeat[PARTSTAFFMAX+1];

        for (sn = 1; sn <= scount; sn++) follows_repeat[sn] = FALSE;

        /* Each key signature has its staff specifed as "number" */

        staff = xml_get_attr_number(ai, US"number", 1, 10, -1, FALSE);

        /* If a a key signature follows a repeat at the start of a line, PMW
        prints the old key signature followed by the repeat, followed by the
        new signature, which usually looks weird. We can check whether this
        immediately follows a repeat, and if so, put the repeat after the key
        signature. */

        if (staff < 0)
          {
          for (sn = 1; sn <= scount; sn++)
            {
            stave_buffer *b = pmw_buffers + pstave + sn;
            s = b->buffer;
            if (b->hwm >= 2 && s[b->hwm-2] == '(' && s[b->hwm-1] == ':')
              {
              follows_repeat[sn] = TRUE;
              b->hwm -= 2;
              }
            }
          }
        else
          {
          stave_buffer *b = pmw_buffers + pstave + staff;
          s = b->buffer;
          if (b->hwm >= 2 && s[b->hwm-2] == '(' && s[b->hwm-1] == ':')
            {
            follows_repeat[staff] = TRUE;
            b->hwm -= 2;
            }
          }

        /* Look for a conventional key signature */

        s = xml_get_string(ai, US"fifths", NULL, FALSE);

        /* If "fifths" is present, output a traditional time signature. */

        if (s != NULL)
          {
          BOOL wasbad;
          n = misc_string_check_number(s, -7, 7, 0, &wasbad);
          if (wasbad) Eerror(ai, ERR23, s);
          s = xml_get_string(ai, US"mode", US"major", FALSE);
          if (Ustrcmp(s, "minor") == 0) offset = 22;
            else if (Ustrcmp(s, "major") != 0) Eerror(ai, ERR22, s);

          if (staff < 0)
            {
            for (sn = 1; sn <= scount; sn++)
              {
              if (starting_noprint)
                sprintf(save_key[sn], "[%skey %s]\n", assume, keys[n + offset]);
              else
                stave_text(pstave + sn, "[%skey %s]\n", assume,
                  keys[n + offset]);
              }
            }
          else
            {
            if (starting_noprint)
              sprintf(save_key[staff], "[%skey %s]\n", assume, keys[n + offset]);
            else
              stave_text(pstave + staff, "[%skey %s]\n", assume,
                keys[n + offset]);
            }
          }

        /* In the absence of "fifths", look for explicit definition of a
        non-traditional key signature. Save up the data and then call a
        function that writes the output, possibly on more than one staff. */

        else
          {
          item *ki;
          uschar *key_step = NULL;
          uschar *key_accidental = NULL;
          int nsklist[100];
          int *nskptr;
          int nsktop = 0;
          BOOL first = TRUE;

          for (ki = ai->next; ki != ai->partner; ki = ki->partner->next)
            {
            if (Ustrcmp(ki->name, "key-step") == 0)
              key_step = xml_get_this_string(ki);
            else if (Ustrcmp(ki->name, "key-accidental") == 0)
              key_accidental = xml_get_this_string(ki);

            if (key_step != NULL && key_accidental != NULL)
              {
              if (first) nsklist[nsktop++] = staff;
              nsklist[nsktop++] = toupper(*key_step) - 'A';

              for (n = 0; n < accidentals_count; n += 3)
                if (Ustrcmp(key_accidental, accidentals[n]) == 0) break;

              if (n < accidentals_count) nsklist[nsktop++] = n; else
                {
                Eerror(ki, ERR56, key_accidental);
                nsklist[nsktop++] = accidentals_count - 3; /* UNKNOWN */
                }

              key_step = key_accidental = NULL;
              first = FALSE;
              }
            }
          nsklist[nsktop++] = -1;  /* End list */

          /* Now output the key for relevant staves. */

          nskptr = nsklist;
          while (nskptr < nsklist + nsktop)
            {
            if (*nskptr < 0)   /* No staff value */
              {
              int *nextptr = NULL;  /* Avoid compiler warning */
              for (n = 1; n <= scount; n++)
                nextptr = do_nsk(pstave + n, nskptr + 1);
              nskptr = nextptr;
              }
            else               /* Staff value given */
              {
              nskptr = do_nsk(pstave + *nskptr, nskptr + 1);
              }
            }
          }      /* End non-standard key signature */

        /* If this key signature followed the start of a repeat, we took out
        the repeat above. Now re-insert it after the key signature. */

        if (staff < 0)
          {
          for (sn = 1; sn <= scount; sn++)
            {
            if (follows_repeat[sn]) stave_text(pstave + sn, "(:");
            }
          }
        else
          {
          if (follows_repeat[staff]) stave_text(pstave + staff, "(:");
          }
        }        /* End key signature */


      /* ==== Time signature attribute ==== */

      else if (Ustrcmp(ai->name, "time") == 0)
        {
        char tstring[30];
        uschar *symbol = xml_get_attr_string(ai, US"symbol", US"", FALSE);
        staff = xml_get_attr_number(ai, US"number", 1, 10, -1, FALSE);
        time_num = xml_get_number(ai, US"beats", 1, 32, 4, TRUE);
        time_den = xml_get_number(ai, US"beat-type", 1, 32, 4, TRUE);
        measure_length = (time_num * divisions * 4)/time_den;

        if (Ustrcmp(symbol, "cut") == 0)
          {
          sprintf(tstring, "A");
          }
        else if (Ustrcmp(symbol, "common") == 0)
          {
          sprintf(tstring, "C");
          }
        else
          {
          if (symbol[0] != 0) Eerror(ai, ERR43, "time symbol", symbol);
          sprintf(tstring, "%d/%d", time_num, time_den);
          }

        /* When saving for later, always "assume" for time signatures. */

        if (staff < 0)
          {
          for (n = 1; n <= scount; n++)
            {
            if (starting_noprint)
              //sprintf(save_time[n], "[%stime %s]\n", assume, tstring);
              sprintf(save_time[n], "[assume time %s]\n", tstring);
            else
              stave_text(pstave + n, "[%stime %s]\n", assume, tstring);
            }
          }
        else
          {
          if (starting_noprint)
            // sprintf(save_time[staff], "[%stime %s]\n", assume, tstring);
            sprintf(save_time[staff], "[assume time %s]\n", tstring);
          else
            stave_text(pstave + staff, "[%stime %s]\n", assume, tstring);
          }
        }

      /* Unknown attribute */

      // Ignore protem

      else
        {
        }

      }    /* End scan attributes loop */
    }      /* End of <attributes> */


  /* ======================== <backup> ======================== */

  /* This is not totally straightforward because there is not a <staff> element
  within <backup>. If there is more than one stave for this part, all must
  backup, but we want to backup only if there are more notes for a stave. Do
  this by setting a pending backup that is not actioned until we find another
  note. We look at where we are on the most recent staff, and compute the
  backup value - where we want to be - from that. Ignore backups at the start
  of the measure. */

  else if (Ustrcmp(mi->name, "backup") == 0)
    {
    int backupby = xml_get_number(mi, US"duration", 0, 10000, 0, TRUE);
    for (n = 1; n <= scount; n++)
      {
      int d = duration[n];
      if (pending_backup[n] >= 0) d = pending_backup[n];
      if (d > 0)
        {
        int backupto = d - backupby;
        if (backupto < 0) backupto = 0;
        pending_backup[n] = backupto;
        }
      }
    }


  /* ======================== <barline> ======================== */

  /* For the moment, we assume that this element is in the correct place in the
  bar. There doesn't seem to be a staff element within barline. */

  else if (Ustrcmp(mi->name, "barline") == 0)
    {
    uschar *s = NULL;
    uschar *location = xml_get_attr_string(mi, US"location", US"right", TRUE);
    item *repeat = xml_find_item(mi, US"repeat");
    item *ending = xml_find_item(mi, US"ending");

    if (ending != NULL)
      {
      int number = xml_get_attr_number(ending, US"number", 1, 10, 1, TRUE);
      uschar *type = xml_get_attr_string(ending, US"type", US"", TRUE);

      /* Ending stuff only on the topmost of multistave part. When we hit a
      "stop", we must arrange for [all] at the start of the next measure. This
      will be overridden if there is another ending number. */

      if (Ustrcmp(type, "start") == 0)
        {
        if (number < 10)
          {
          stave_buffer *b = pmw_buffers + pstaff;
          uschar *bb = b->buffer + staff_bar_start[pstave+1];
          if (Ustrncmp(bb, "[all]", 5) == 0)
            {
            bb[1] = '1' + number - 1;
            bb[2] = ordabbrev[number][0];
            bb[3] = ordabbrev[number][1];
            }
          else stave_text(pstave + 1, "[%d%s]", number, ordabbrev[number]);
          }
        else Eerror(ending, ERR55, 9);
        }

      else if (Ustrcmp(type, "stop") == 0)
        {
        all_pending[pstave + 1] = TRUE;
        }

      else if (Ustrcmp(type, "discontinue") == 0)
        {
        stave_text(pstave + 1, "[all]");
        }

      else Eerror(ending, ERR52, "ending type", type);
      }

    if (repeat != NULL)
      {
      uschar *direction = xml_get_attr_string(repeat, US"direction", NULL,
        TRUE);

      hadrepeat = TRUE;
      if (direction != NULL)
        {
        if (Ustrcmp(location, "left") != 0 &&
            Ustrcmp(direction, "backward") == 0)
          s = US" :)";
        else if (Ustrcmp(location, "right") != 0 &&
                 Ustrcmp(direction, "forward") == 0)
          s = US"(:";
        else Eerror(mi, ERR33, direction, location);
        }
      }

    else if (repeat == NULL)  /* Not a repeat barline */
      {
      uschar *style = xml_get_string(mi, US"bar-style", NULL, FALSE);
      if (style != NULL)
        {
        for (n = 0; n < barstyles_count; n += 2)
          {
          if (Ustrcmp(style, barstyles[n]) == 0)
            {
            s = (uschar *)barstyles[n+1];
            lastwasbarline = TRUE;
            break;
            }
          }
        }
      }

    if (s != NULL)
      {
      int sn;
      for (sn = 1; sn <= scount; sn++)
        stave_text(pstave + sn, "%s", s);
      }
    }


  /* ======================== <direction> ======================== */

  else if (Ustrcmp(mi->name, "direction") == 0)
    {
    BOOL firsttext = TRUE;
    BOOL above = ISATTR(mi, "placement", "above", FALSE, "above");
    BOOL directive = ISATTR(mi, "directive", "no", FALSE, "yes");
    int offset = xml_get_number(mi, US"offset", -1000, 1000, 0, FALSE);
    item *dtg;

    for (dtg = xml_find_item(mi, US"direction-type"); dtg != NULL;
         dtg = xml_find_next(mi, dtg))
      {
      item *dt;

      /* Scan the elements within <direction-type> */

      for (dt = dtg->next; dt != dtg->partner; dt = dt->partner->next)
        {
        uschar *halign = xml_get_attr_string(dt, US"halign", US"left", FALSE);
        int dy = xml_get_attr_number(dt, US"default-y", -1000, 1000, 0, FALSE);
        int rx = xml_get_attr_number(dt, US"relative-x", -1000, 1000, 0, FALSE);
        int ry = xml_get_attr_number(dt, US"relative-y", -1000, 1000, 0, FALSE);
        int fsize = xml_get_attr_mils(dt, US"font-size", 1000, 100000, -1,
          FALSE);
        int fweight = ((ISATTR(dt, "font-style", "", FALSE, "italic"))? 1:0) +
           ((ISATTR(dt, "font-weight", "", FALSE, "bold"))? 2:0);


        /* Handle a textual direction */

        if (Ustrcmp(dt->name, "words") == 0)
          {
          uschar *s = xml_get_this_string(dt);
          uschar *enclosure = xml_get_attr_string(dt, US"enclosure", NULL,
            FALSE);

          stave_text_quoted_string(pstaff, s, " ", US fonttypes[fweight], NULL);
          write_fontsize(pstaff, fsize);

          if (firsttext)
            {
            handle_dy(dy, pstaff, above);
            if (Ustrcmp(halign, "center") == 0) stave_text(pstaff, "/c");
              else if (Ustrcmp(halign, "right") == 0) stave_text(pstaff, "/e");
                else if (Ustrcmp(halign, "left") != 0)
                  misc_add_attrval_to_tree(&unrecognized_element_tree, dt,
                    xml_find_attr(dt, US"halign"));

            if (ry != 0)  /* relative-y is specified */
              stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
                format_fixed(abs(ry*400)));
            }

          if (enclosure != NULL)
            {
            if (Ustrcmp(enclosure, "oval") == 0 ||
                Ustrcmp(enclosure, "circle") == 0)
              stave_text(pstaff, "/ring");
            else if (Ustrcmp(enclosure, "rectangle") == 0 ||
                     Ustrcmp(enclosure, "square") == 0)
              stave_text(pstaff, "/box");
            else if (Ustrcmp(enclosure, "none") != 0)
              Eerror(dt, ERR43, US"<words> enclosure", enclosure);
            }

          /* The "offset" value is in divisions, positioning the item in terms
          of musical position. This is overridden by explicit horizontal
          positioning. The "directive" attribute, if "yes" specifies that the
          item is positioned according to "tempo rules", which in essence means
          aligned with any time signature. However, any other horizontal
          positioning setting overrides. If not the first text, only relative
          movement is relevant. */

          if (rx != 0)  /* relative-x is specified */
            stave_text(pstaff, "/%c%s", (rx > 0)? 'r':'l',
              format_fixed(abs(rx*400)));

          else if (firsttext)
            {
            if (offset != 0)
              stave_text(pstaff, "/%cc%s", (offset > 0)? 'r':'l',
                format_fixed((abs(offset)*1000)/divisions));

            else if (directive) stave_text(pstaff, "/ts");
            }

          /* Use the follow-on feature if not the first text */

          if (!firsttext) stave_text(pstaff, "/F");

          stave_text(pstaff, " ");
          firsttext = FALSE;
          }


        /* Handle a dynamic */

        else if (Ustrcmp(dt->name, "dynamics") == 0)
          {
          item *which;

          for (which = dt->next; which != dt->partner;
               which = which->partner->next)
            {
            for (n = 0; n < dynamics_count; n++)
              if (Ustrcmp(which->name, dynamics[n].name) == 0) break;

            if (n >= dynamics_count) Eerror(dt, 35, which->name); else
              {
              dynamics[n].used = TRUE;
              stave_text(pstaff, " &%s/s1", which->name);

              if (offset != 0)
                stave_text(pstaff, "/%cc%s", (offset > 0)? 'r':'l',
                  format_fixed(abs((offset*1000)/divisions)));

              if (rx != 0)  /* relative-x is specified */
                stave_text(pstaff, "/%c%s", (rx > 0)? 'r':'l',
                  format_fixed(abs(rx*400)));

              handle_dy(dy, pstaff, above);

// Untested, need an example
#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif

              if (Ustrcmp(halign, "center") == 0)
                stave_text(pstaff, "/c");
              else if (Ustrcmp(halign, "right") == 0)
                stave_text(pstaff, "/e");
              else if (Ustrcmp(halign, "left") != 0)
                misc_add_attrval_to_tree(&unrecognized_element_tree, dt,
                  xml_find_attr(dt, US"halign"));
              }
            }
          }


        /* Handle a metronome mark */

        else if (Ustrcmp(dt->name, "metronome") == 0)
          {
          uschar *beat_unit = xml_get_string(dt, US"beat-unit", US"quarter",
            TRUE);
          uschar *per_minute = xml_get_string(dt, US"per-minute", US"?", TRUE);
          BOOL parens = ISATTR(dt, "parentheses", "no", FALSE, "yes");

          for (n = 0; n < note_types_count; n++)
            if (Ustrcmp(beat_unit, note_types[n].name) == 0) break;
          if (n >= note_types_count) Eerror(dt, ERR18, beat_unit);  /* Hard */

          if (note_types[n].abbrev < 0) Eerror(dt, ERR11, beat_unit); else
            {
            stave_text(pstaff, "\"\\rm\\%s\\*%c\\ = %s%s\"%s",
              parens? "(":"", note_types[n].abbrev, per_minute,
              parens? ")":"", firsttext? (above? "/a":"") : "/F");
            write_fontsize(pstaff, fsize);

// Untested, need an example
#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif
            }

          firsttext = FALSE;
          }


        /* Handle a rehearsal mark. PMW does not support different font sizes,
        styles, or different enclosures for rehearsal marks. We check that they
        are all the same. */

        else if (Ustrcmp(dt->name, "rehearsal") == 0)
          {
          uschar *mark = xml_get_this_string(dt);
          uschar *weight_string = xml_get_attr_string(dt, US"font-weight", NULL,
            FALSE);
          uschar *style_string = xml_get_attr_string(dt, US"font-style", NULL,
            FALSE);
          uschar *enclosure_string = xml_get_attr_string(dt, US"enclosure",
            NULL, FALSE);

          BOOL bold = weight_string == NULL ||    /* default is bold*/
            Ustrcmp(weight_string, "bold") == 0;
          BOOL italic = style_string != NULL &&   /* default is roman */
            Ustrcmp(style_string, "italic") == 0;

          int font_type = bold?
            (italic? REHEARSAL_TYPE_BOLDITALIC : REHEARSAL_TYPE_BOLD) :
            (italic? REHEARSAL_TYPE_ITALIC : REHEARSAL_TYPE_ROMAN);
          int font_size = xml_get_attr_mils(dt, US"font-size", 1000, 100000,
            -1, FALSE);
          int default_x = xml_get_attr_number(dt, US"default-x", -100,
            100, 0, FALSE);

          int enclosure = REHEARSAL_ENCLOSURE_BOX;

          if (enclosure_string != NULL)
            {
            if (Ustrcmp(enclosure_string, "oval") == 0 ||
                Ustrcmp(enclosure_string, "circle") == 0)
              enclosure = REHEARSAL_ENCLOSURE_RING;

            else if (Ustrcmp(enclosure_string, "rectangle") != 0 &&
                Ustrcmp(enclosure_string, "square") != 0)
              Eerror(dt, ERR43, US"rehearsal enclosure", enclosure_string);
            }

          if (rehearsal_enclosure >= 0 && enclosure != rehearsal_enclosure)
            Eerror(dt, ERR46, US"enclosure marks");
          rehearsal_enclosure = enclosure;

          if (rehearsal_type >= 0 && font_type != rehearsal_type)
            Eerror(dt, ERR46, "font styles or weights");
          rehearsal_type = font_type;

          if (font_size > 0)
            {
            if (rehearsal_size >= 0 && font_size != rehearsal_size)
              Eerror(dt, ERR46, US"font sizes");
            rehearsal_size = font_size;
            }

          stave_text_quoted_string(pstaff, mark, " [", NULL, NULL);
          leftright(pstaff, default_x, "");
// Untested, need an example

#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif
          stave_text(pstaff, "]");
          }


        /* Handle a wedge (aka hairpin). MusicXML puts the "spread" setting on
        the open end, whereas PMW puts its /w option on the opening character.
        So in the case of a crescendo we have to do an insert at the starting
        item. */

        else if (Ustrcmp(dt->name, "wedge") == 0)
          {
          uschar *wtype = xml_get_attr_string(dt, US"type", US"unset", FALSE);
          int spread = xml_get_attr_number(dt, US"spread", 1, 100, -1, FALSE);

          do_pending_backup(staff, pstaff, dt);

// Untested, need an example. Useful for wedge?
#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif

          if (Ustrcmp(wtype, "stop") == 0)
            {
            if (open_wedge_type[pstaff] != 0)
              {
              stave_text(pstaff, " %c", open_wedge_type[pstaff]);
              rxandoffset(pstaff, rx, offset, TRUE);
              if (spread > 0)   /* Should only happen for crescendo */
                stave_text_insert(pstaff, open_wedge_start[pstaff],
                  "/w%s", format_fixed(spread*400));
              open_wedge_type[pstaff] = 0;
              }
            else Eerror(dt, ERR48);
            }

          else if (open_wedge_type[pstaff] == 0)
            {
            int c = 0;
            if (Ustrcmp(wtype, "crescendo") == 0) c = '<';
              else if (Ustrcmp(wtype, "diminuendo") == 0) c = '>';
                else Eerror(dt, ERR43, US"wedge type", wtype);

            if (c != 0)
              {
              stave_text(pstaff, " %c", c);
              handle_dy(dy, pstaff, above);
              rxandoffset(pstaff, rx, offset, TRUE);
              if (spread > 0)   /* Should only happen for diminuendo */
                stave_text(pstaff, "/w%s", format_fixed(spread*400));
              open_wedge_type[pstaff] = c;
              open_wedge_start[pstaff] = pmw_buffers[pstaff].hwm;
              }
            }

          else Eerror(dt, ERR49);
          }


        /* Handle pedal marks */

        else if (Ustrcmp(dt->name, "pedal") == 0)
          {
          uschar *line = xml_get_attr_string(dt, US"line", US"no", FALSE);
          uschar *type = xml_get_attr_string(dt, US"type", NULL, FALSE);

          if (type == NULL) Eerror(dt, ERR32, US"type"); else
            {
            uschar *s = NULL;

            if (Ustrcmp(line, "yes") == 0) Eerror(dt, ERR34, "Pedal lines");
            else if (Ustrcmp(type, "start") == 0)
              {
              s = US"\\mf\\\\163\\";
              }
            else if (Ustrcmp(type, "stop") == 0)
              {
              s = US"\\mf\\\\36\\";
              }
            else Eerror(dt, ERR43, US"<pedal> type", type);

            /* Compare outputting of text above */

            if (s != NULL)
              {
              stave_text(pstaff, "\"%s\"", s);
              write_fontsize(pstaff, fsize);

              handle_dy(dy, pstaff, above);
              if (Ustrcmp(halign, "center") == 0) stave_text(pstaff, "/c");
                else if (Ustrcmp(halign, "right") == 0) stave_text(pstaff, "/e");
                  else if (Ustrcmp(halign, "left") != 0)
                    misc_add_attrval_to_tree(&unrecognized_element_tree, dt,
                      xml_find_attr(dt, US"halign"));

// Untested, need an example
#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif

              if (rx != 0)  /* relative-x is specified */
                stave_text(pstaff, "/%c%s", (rx > 0)? 'r':'l',
                  format_fixed(abs(rx*400)));

              else if (offset != 0)
                stave_text(pstaff, "/%cc%s", (offset > 0)? 'r':'l',
                  format_fixed((abs(offset)*1000)/divisions));

              else if (directive) stave_text(pstaff, "/ts");
              }
            }
          }


        /* Handle <bracket>, which in PMW is [line] and is very much like
        [slur]. */

        if (Ustrcmp(dt->name, "bracket") == 0)
          {
          uschar *sn = xml_get_attr_string(dt, US"number", NULL, FALSE);
          uschar *line_end = xml_get_attr_string(dt, US"line-end", US"", FALSE);
          uschar *line_type = xml_get_attr_string(dt, US"line-type", US"solid",
            FALSE);
          BOOL nojog = Ustrcmp(line_end, "none") == 0;
          int jogsize = nojog? 0:7;

          /* MusicXML seems to give the dy point as the end of the jog, not
          the position of the line, in contrast to PMW. */

          if (above) dy += jogsize; else dy -= jogsize;

          if (ISATTR(dt, "type", "", TRUE, "start"))
            {
            sl_start *ss;

            stave_text(pstaff, "[line");
            handle_dy(dy, pstaff, above);

            if (Ustrcmp(line_type, "dashed") == 0)
              stave_text(pstaff, "/i");
            else if (Ustrcmp(line_type, "dotted") == 0)
              stave_text(pstaff, "/ip");
            else if (Ustrcmp(line_type, "solid") != 0)
              Eerror(dt, ERR43, "bracket type", line_type);

            if (Ustrcmp(line_end, "up") == 0)
              {
              if (above) Eerror(dt, ERR44, line_end, "above");
              }
            else if (Ustrcmp(line_end, "down") == 0)
              {
              if (!above) Eerror(dt, ERR44, line_end, "below");
              }
            else if (nojog) stave_text(pstaff, "/ol");
            else if (line_end[0] != 0) Eerror(dt, ERR43, "line end", line_end);

            /* Set base position from offset if supplied, then adjust using
            relative-x. */

            if (offset != 0)
              stave_text(pstaff, "/l%cc%s", (offset > 0)? 'r':'l',
                format_fixed((abs(offset)*1000)/divisions));

            rx = rx*400;  /* Convert to millipoints */
            if (abs(rx) >= 100)  /* Ignore very small */
              stave_text(pstaff, "/l%c%s", (rx > 0)? 'r':'l',
                format_fixed(abs(rx)));

// Untested, need an example
#ifdef NEVER
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif

            /* Handle line number */

            if (sn != NULL) stave_text(pstaff, "/=%c", sn[0]);

            /* We have to remember where in the output this [line] ends in case
            there are attributes on the end element that have to be inserted. A
            chain of data blocks is maintained; when finished with, each is put
            on an empty chain, to reduce malloc/free activity. */

            if (line_free_starts != NULL)
              {
              ss = line_free_starts;
              line_free_starts = ss->next;
              }
            else
              {
              ss = misc_malloc(sizeof(sl_start));
              }

            ss->next = line_starts;
            line_starts = ss;
            ss->offset = pmw_buffers[pstaff].hwm;
            ss->pstaff = pstaff;
            ss->id = sn[0];
            ss->default_y = dy;

            /* End off the [line] directive */

            stave_text(pstaff, "] ");
            }

          /* If it's an end line, find the block that remembers the start, and
          insert extra characters if required. */

          else
            {
            sl_start *ss, **sp;

            stave_text(pstaff, " [el");
               if (sn != NULL) stave_text(pstaff, "/=%c", sn[0]);
               stave_text(pstaff, "]");

            for (sp = &line_starts, ss = line_starts;
                 ss != NULL;
                 sp = &(ss->next), ss = ss->next)
              {
              if (ss->pstaff == pstaff && ss->id == sn[0])
                {
                *sp = ss->next;
                break;
                }
              }

            if (ss == NULL) Eerror(mi, ERR39); else
              {
              if (Ustrcmp(line_end, "up") == 0)
                {
                if (above) Eerror(dt, ERR44, line_end, "above");
                }
              else if (Ustrcmp(line_end, "down") == 0)
                {
                if (!above) Eerror(dt, ERR44, line_end, "below");
                }
              else if (Ustrcmp(line_end, "none") == 0)
                {
                stave_text_insert(pstaff, ss->offset, "/or");
                }
              else if (line_end[0] != 0)
                Eerror(dt, ERR43, "line end", line_end);

              /* Insert a right-end up/down movement on the [line] directive. */

              if (dy != ss->default_y)
                {
                int adjust = (dy - ss->default_y)*400;
                stave_text_insert(pstaff, ss->offset, "/r%c%s",
                  (adjust > 0)? 'u':'d', format_fixed(abs(adjust)));
                }

              /* Handle left/right offset. MusicXML offsets are from the next
              note; PMW defaults /rrc and /rlc to the previous note, but has a
              /cx option to make it behave line MusicXML. */

              if (offset != 0)
                stave_text_insert(pstaff, ss->offset, "/cx/r%cc%s",
                  (offset > 0)? 'r':'l',
                  format_fixed((abs(offset)*1000)/divisions));

              rx = rx*400;  /* Convert to millipoints */
              if (abs(rx) >= 100)  /* Ignore very small */
                stave_text(pstaff, "/r%c%s", (rx > 0)? 'r':'l',
                  format_fixed(abs(rx)));

              ss->next = line_free_starts;
              line_free_starts = ss;
              }
            }
          }


        /* Ignoring unrecognized; should automatically be listed. */


        }  /* End of loop scanning <direction-type> elements */
      }    /* End of loop scanning <direction> for <direction-type> */
    }


  /* ======================== <forward> ======================== */

  /* Unlike <backup>, <forward> may have a <staff> element. It is also
  necessary to handle <forward> when a <backup> is still pending. Note that the
  pending_backup values are where to back up to, not the amount to backup. A
  negative value means "no backup pending". */

  else if (Ustrcmp(mi->name, "forward") == 0)
    {
    int forward_duration = xml_get_number(mi, US"duration", 0, 10000, -1, TRUE);
    if (forward_duration > 0)
      {
      if (pending_backup[staff] >= 0)
        {
        pending_backup[staff] += forward_duration;
        if (pending_backup[staff] >= duration[staff])
          {
          forward_duration = pending_backup[staff] - duration[staff];
          pending_backup[staff] = -1;
          }
        else forward_duration = 0;
        }
      if (forward_duration > 0)
        {
        stave_text(pstaff, " ");
        write_invisible_rests(pstaff, forward_duration);
        duration[staff] += forward_duration;
        }
      }
    }


  /* ======================== Notes and rests ======================== */

  else if (Ustrcmp(mi->name, "note") == 0)
    {
    uschar *note_type_name, *note_size;
    const char *rest_wholebar;
    int note_default_x, note_duration, dots, rest_letter;
    int thisnotehead;
    int tremlines;
    int tied;
    int slurstop[10];  /* Should be overkill */
    item *dot, *grace, *lyric, *notations, *pitch, *rest, *slur,
      *tremolo, *type, *unpitched;
    BOOL whole_bar_rest = FALSE;
    char options[32];

    options[0] = '\\';
    options[1] = 0;
    tied = TIED_NONE;
    tremolo = NULL;
    tremlines = 2;

    if (!inchord[staff]) stave_text(pstaff, " ");

    do_pending_backup(staff, pstaff, mi);

    /* Handle lyrics. Not all <lyric> elements have <text> because some just
    have <extend>. */

    for (lyric = xml_find_item(mi, US"lyric");
         lyric != NULL;
         lyric = xml_find_next(mi, lyric))
      {
      uschar *t = xml_get_string(lyric, US"text", NULL, FALSE);
      uschar *j = xml_get_attr_string(lyric, US"justify", US"", FALSE);
      item *extend = xml_find_item(lyric, US"extend");

      if (t != NULL)
        {
        uschar s_end[8];
        uschar *s_start;
        uschar *s = xml_get_string(lyric, US"syllabic", US"single", FALSE);
        int rx = xml_get_attr_number(lyric, US"relative-x", -100, 100, 0,
          FALSE);

        s_start = (Ustrcmp(j, "left") == 0)? US"^" : NULL;
        s_end[0] = 0;
        if (Ustrcmp(j, "right") == 0) (void)Ustrcat(s_end, "^^");

        if (Ustrcmp(s, "begin") == 0 || Ustrcmp(s, "middle") == 0)
          (void)Ustrcat(s_end, "-");

        else if (extend != NULL)
          {
          attrstr *atype = xml_find_attr(extend, US"type");
          if (atype == NULL || Ustrcmp(atype->value, "start") == 0)
            (void)Ustrcat(s_end, "=");
          }

        stave_text_quoted_string(pstaff, t, " ", s_start, s_end);
        stave_text(pstaff, "/ul");

        if (rx != 0)
          stave_text(pstaff, "/%c%s", (rx > 0)? 'r':'l',
            format_fixed(abs(rx*400)));
        }
      else
        {
        if (extend != NULL && ISATTR(extend, "type", "", FALSE, "stop"))
          stave_text(pstaff, "\"=\"/ul");
        }

      stave_text(pstaff, " ");
      }


    /* Now deal with the note. */

    dots = 0;
    dot = xml_find_item(mi, US"dot");
    pitch = xml_find_item(mi, US"pitch");
    grace = xml_find_item(mi, US"grace");
    unpitched = rest = NULL;
    notations = xml_find_item(mi, US"notations");
    type = xml_find_item(mi, US"type");

    /* The duration value is documented as intended for a performance vs
    notated value, but hopefully <backup> values, which are also durations,
    will make sense with it. If not, we might have to get a "notated duration"
    value. The same applies to whole bar rests. No duration is supplied for
    grace notes. */

    note_duration = xml_get_number(mi, US"duration", 0, 10000, 0,
      grace == NULL);

    if (grace != NULL)
      {
      conditional_space(options);
      (void)Ustrcat(options, "g");
      if (ISATTR(grace, "slash", "", FALSE, "yes")) (void)Ustrcat(options, "/");
      }

    /* Sometimes a whole bar rest is notated as a whole note with a duration of
    a complete measure. */

    if (pitch == NULL)
      {
      unpitched = xml_find_item(mi, US"unpitched");
      if (unpitched == NULL)
        {
        rest = xml_find_item(mi, US"rest");
        if (rest == NULL) Eerror(mi, ERR17, "<pitch>, <unpitched>, or <rest>");  /* Hard */
        whole_bar_rest = ISATTR(rest, "measure", "", FALSE, "yes") ||
          note_duration == measure_length;
        }
      }

    /* If the note is invisible (print-object="no" but print-spacing="yes"),
    arrange for it to be handled as a rest using Q instead of R. */

    if (invisible)
      {
      rest_letter = 'Q';
      rest_wholebar = "Q!";
      if (rest == NULL) rest = (pitch == NULL)? unpitched : pitch;
      }
    else if (starting_noprint)
      {
      rest_letter = 'Q';    /* FIXME: This may need changing */
      rest_wholebar = "";
      }
    else
      {
      rest_letter = 'R';
      rest_wholebar = "R!";
      }

    /* We keep a table of the current musical position with any default-x
    value, so that after <backup> we can try to separate notes that might
    clash. This is not useful in chords, nor for whole bar rests or grace
    notes. */

    if (!inchord[staff] && !whole_bar_rest && grace == NULL)
      {
      note_default_x = xml_get_attr_number(mi, US"default-x", -10000, 10000,
        INT_MAX, FALSE);

      for (n = 0; n < note_positions_next; n++)
        {
        if (duration[staff] < note_positions[n].moff)
          {
          memmove((char *)(note_positions + n + 1), (char *)(note_positions + n),
            (note_positions_next - n)*sizeof(note_position));
          break;
          }

        else if (duration[staff] == note_positions[n].moff)
          {
          if (note_default_x != INT_MAX)
            {
            if (note_positions[n].x == INT_MAX)
              {
              note_positions[n].x = note_default_x;
              }
            else
              {
              int adjustx = note_default_x - note_positions[n].x;

              /* If a negative move is at musical offset 0, insert some space
              at the start of the bar. */

              if (adjustx != 0)
                {
                if (adjustx < 0 && duration[staff] == 0)
                  stave_text_insert(pstaff, staff_bar_start[pstaff],
                    "[space %s]", format_fixed(abs(adjustx*400)/2));
                stave_text(pstaff, "[move %s] ", format_fixed(adjustx*400));
                }
              }
            }
          n = INT_MAX;   /* No new insert needed */
          break;
          }
        }

      /* Need to insert a new item in the table. */

      if (n <= note_positions_next)
        {
        note_positions[n].moff = duration[staff];
        note_positions[n].x = note_default_x;
        note_positions_next++;
        }
      }

    /* Can now update to the next duration position. */

    if (note_duration > 0 && !inchord[staff]) duration[staff] += note_duration;

    /* Type may be NULL for whole-measure rests */

    if (type != NULL)
      {
      note_type_name = xml_get_this_string(type);
      note_size = xml_get_attr_string(type, US"size", US"", FALSE);
      }
    else  /* Need to keep compiler happy by setting those variables */
      {
      note_type_name = NULL;
      note_size = US"";
      }

    while (dot != NULL)
      {
      dots++;
      dot = xml_find_next(mi, dot);
      }

    /* Handle cue notes; for chords <cue> is only on the first note. */

    if (!inchord[staff])
      {
      if (xml_find_item(mi, US"cue") != NULL || Ustrcmp(note_size, "cue") == 0)
        {
        if (!incue[staff])
          {
          stave_text(pstaff, "[cue] ");
          incue[staff] = TRUE;
          }
        }
      else
        {
        if (incue[staff])
          {
          stave_text(pstaff, "[endcue] ");
          incue[staff] = FALSE;
          }
        }
      }

    /* Handle notations. The slurstop[] vector contains a list of slur numbers
    that must be terminated after this note. */

    slurstop[0] = -1;
    if (notations != NULL)
      {
      item *fermata, *ni, *ornaments, *tuplet;

      /* Slurs and lines are very similar in PMW, but rather than trying to
      combine the code, we do them separately. */

      for (slur = xml_find_item(notations, US"slur");
           slur != NULL;
           slur = xml_find_next(notations, slur))
        {
        uschar *sn = xml_get_attr_string(slur, US"number", NULL, FALSE);
        uschar *slurtype = xml_get_attr_string(slur, US"type", US"", TRUE);
        uschar *line_type = xml_get_attr_string(slur, US"line-type", NULL,
          FALSE);

        int default_x = xml_get_attr_number(slur, US"default-x", -1000, 1000,
          0, FALSE);
        int default_y = xml_get_attr_number(slur, US"default-y", -1000, 1000,
          0, FALSE);
        int bezier_x = xml_get_attr_number(slur, US"bezier-x", -1000, 1000,
          0, FALSE);
        int bezier_y = xml_get_attr_number(slur, US"bezier-y", -1000, 1000,
          0, FALSE);
        int slurchar = (sn == NULL)? 'Z' : sn[0];

        if (Ustrcmp(slurtype, "start") == 0)
          {
          int pmw_id = slurchar;
          sl_start *ss;

          /* If we have had an endslur on this note with the same id, we have
          to generate a different PMW id because the endslur comes after the
          note. */

          for (n = 0; n < (int)(sizeof(slurstop)/sizeof(int)); n++)
            {
            if (slurstop[n] < 0) break;
            if (slurstop[n] == slurchar)
              {
              pmw_id = 'A' + slurchar - '1';
              break;
              }
            }

          stave_text(pstaff, "[slur/%c",
            (Ustrcmp(xml_get_attr_string(slur, US"placement", US"", FALSE),
              "above") == 0)? 'a' : 'b');
          stave_text(pstaff, "/=%c", pmw_id);

          if (line_type != NULL)
            {
            if (Ustrcmp(line_type, "dashed") == 0)
              stave_text(pstaff, "/i");
            else if (Ustrcmp(line_type, "dotted") == 0)
              stave_text(pstaff, "/ip");
            else if (Ustrcmp(line_type, "solid") != 0)
              Eerror(slur, ERR43, "slur line type", line_type);
            }

          /* FIXME: need much more investigation of what to do here. Perhaps
          implement an overall option for modifying slurs: default off. The
          default-y thing is tricky. */

if (FALSE)
{
          leftright(pstaff, default_x, "l");
          updown(pstaff, default_y, "l");   /* This is especially bad */

          leftright(pstaff, bezier_x, "cl");
          updown(pstaff, bezier_y, "cl");
}


          /* We have to remember where in the output this [slur] ends in case
          there are attributes on the end element that have to be inserted.
          A chain of data blocks is maintained; when finished with, each is put
          on an empty chain, to reduce malloc/free activity. */

          if (slur_free_starts != NULL)
            {
            ss = slur_free_starts;
            slur_free_starts = ss->next;
            }
          else
            {
            ss = misc_malloc(sizeof(sl_start));
            }

          ss->next = slur_starts;
          slur_starts = ss;
          ss->offset = pmw_buffers[pstaff].hwm;
          ss->pstaff = pstaff;
          ss->id = slurchar;
          ss->pmw_id = pmw_id;
          ss->default_y = default_y;

          /* End off the [slur] directive */

          stave_text(pstaff, "] ");
          }

        /* If it's an end slur, find the block that remembers the start, and
        insert extra characters if required. */

        else if (Ustrcmp(slurtype, "stop") == 0)
          {
          sl_start *ss, **sp;

          for (sp = &slur_starts, ss = slur_starts;
               ss != NULL;
               sp = &(ss->next), ss = ss->next)
            {
            if (ss->pstaff == pstaff && ss->id == slurchar)
              {
              *sp = ss->next;
              break;
              }
            }

          if (ss == NULL) Eerror(mi, ERR39); else
            {
            size_t offset = ss->offset;

            for (n = 0; n < (int)(sizeof(slurstop)/sizeof(int)); n++)
              if (slurstop[n] < 0) break;
            slurstop[n] = ss->pmw_id;
            slurstop[n+1] = -1;

if (FALSE)
{
            leftright_insert(pstaff, offset, default_x, "r");
            updown_insert(pstaff, offset, default_y, "r");

            leftright_insert(pstaff, offset, bezier_x, "cr");
            updown_insert(pstaff, offset, bezier_y, "cr");
}

            ss->next = slur_free_starts;
            slur_free_starts = ss;
            }
          }
        }

      /* Tuplets. There doesn't seem to be a separate font description for
      them, so default to word font size and italic, but enforce a minimum. */

      tuplet = xml_find_item(notations, US"tuplet");
      if (tuplet != NULL)
        {
        if (!tuplet_size_set)
          {
          if (fontsize_word_default > 0)
            {
            int fsize = fontsizes[fontsize_word_default];
            if (fsize < 8000) fsize = 8000;
            fprintf(outfile, "Tripletfont %s italic\n", format_fixed(fsize));
            }
          tuplet_size_set = TRUE;
          }

        if (ISATTR(tuplet, "type", "", FALSE, "start"))
          {
          item *time_modification;
          uschar *show_number = xml_get_attr_string(tuplet, US"show-number",
            US"actual", FALSE);
          uschar *placement = xml_get_attr_string(tuplet, US"placement",
            US"", FALSE);
          int actual = 3;
          int normal = 2;

          time_modification = xml_find_item(mi, US"time-modification");
          if (time_modification != NULL)
            {
            actual = xml_get_number(time_modification, US"actual-notes", 1,
              100, 3, TRUE);
            normal = xml_get_number(time_modification, US"normal-notes", 1,
              100, 2, TRUE);
            }

          if (actual == 3 && normal == 2)
            stave_text(pstaff, "{");
          else
            stave_text(pstaff, "{%d/%d", normal, actual);

          if (Ustrcmp(show_number, "none") == 0)
            stave_text(pstaff, "/x");
          else if (Ustrcmp(show_number, "actual") != 0)
            Eerror(tuplet, ERR43, US"tuplet show number", show_number);

          if (Ustrcmp(placement, "below") == 0) stave_text(pstaff, "/b");
            else if (Ustrcmp(placement, "above") == 0) stave_text(pstaff, "/a");

          if (ISATTR(tuplet, "bracket", "no", FALSE, "no"))
            stave_text(pstaff, "/n");

          stave_text(pstaff, " ");
          }

        /* If not start, assume stop */

        else end_tuplet[staff] = TRUE;
        }

      /* Other notations build up options for the note. There can be multiple
      <tied> items on a note, typically stop,start. Treat "continue" as "start"
      for the moment. */

      for (ni = xml_find_item(notations, US"tied");
           ni != NULL;
           ni = xml_find_next(notations, ni))
        {
        attrstr *t = xml_find_attr(ni, US"type");
        if (t != NULL &&
            (Ustrcmp(t->value, "start") == 0 ||
             Ustrcmp(t->value, "continue") == 0))
          {
          uschar *orientation = xml_get_attr_string(ni, US"orientation", US"",
            FALSE);
          tied = (Ustrcmp(orientation, "over") == 0)? TIED_ABOVE :
                 (Ustrcmp(orientation, "under") == 0)? TIED_BELOW :
                 TIED_DEFAULT;
          }
        }

      /* Fermata */

      fermata = xml_find_item(notations, US"fermata");
      if (fermata != NULL)
        {
        conditional_space(options);
        (void)Ustrcat(options, "f");
        if (ISATTR(fermata, "type", "upright", FALSE, "inverted"))
          (void)Ustrcat(options, "!");
        }

      /* Arpeggio: take note only if first note of a chord. The inchord[]
      setting happens later. */

      if (xml_find_item(notations, US"arpeggiate") != NULL &&
          !inchord[staff])
        {
        conditional_space(options);
        (void)Ustrcat(options, "ar");
        }

      /* Tremolo. If this is left non-NULL, [tremolo] is output following the
      note. */

      tremolo = xml_find_item(notations, US"tremolo");
      if (tremolo != NULL)
        {
        tremlines = xml_get_this_number(tremolo, 1, 3, 2, FALSE);
        uschar *tremtype = xml_get_attr_string(tremolo, US"type", US"single",
          FALSE);

        if (Ustrcmp(tremtype, "start") != 0)
          {
          if (Ustrcmp(tremtype, "single") == 0)
            {
            while (tremlines-- > 0) (void)Ustrcat(options, "/");
            }
          else if (Ustrcmp(tremtype, "stop") == 0)
            {
            /* Perhaps should check for previous note start? */
            }
          else Eerror(tremolo, ERR43, "tremolo type", tremtype);
          tremolo = NULL;
          }
        }

      /* Ornaments */

      ornaments = xml_find_item(notations, US"ornaments");
      if (ornaments != NULL)
        {
        item *orn;
        const char **accabove = NULL;
        const char **accbelow = NULL;
        BOOL delayed_turn = FALSE;

        for (orn = xml_find_item(ornaments, US"accidental-mark");
             orn != NULL;
             orn = xml_find_next(ornaments, orn))
          {
          uschar *accname = xml_get_this_string(orn);
          const char ***which =
            (ISATTR(orn, "placement", "below", FALSE, "below"))?
              &accbelow : &accabove;

          for (n = 0; n < accidentals_count; n += 3)
            if (Ustrcmp(accname, accidentals[n]) == 0) break;

          if (n >= accidentals_count)
            Eerror(orn, ERR52, "accidental", accname);
          else *which = accidentals + n;
          }

        orn = xml_find_item(ornaments, US"trill-mark");
        if (orn != NULL)
          {
          conditional_space(options);
          (void)Ustrcat(options, "tr");
          if (accbelow != NULL) Eerror(orn, ERR43,
            "trill with accidental placement", "below");
          if (accabove != NULL)
            {
            if (accabove[1][1] == 0)  /* # or $ or % */
              (void)Ustrcat(options, accabove[1]);
            else Eerror(orn, ERR43, "double accidental on", "trill");
            }
          }

        /* Use text (via macros) for turns */

        orn = xml_find_item(ornaments, US"turn");
        if (orn == NULL)
          {
          delayed_turn = TRUE;
          orn = xml_find_item(ornaments, US"delayed-turn");
          }
        if (orn != NULL)
          {
          BOOL below = ISATTR(orn, "placement", "above", FALSE, "below");
          int typechar = (ISATTR(orn, "slash", "no", FALSE, "yes"))? 'i':'S';

          if (accabove != NULL && accbelow != NULL)
            {
            stave_text(pstaff, "&turnAB(%c,%s,%s)", typechar, accabove[2],
              accbelow[2]);
            turns_used |= TURNS_BOTH;
            }
          else if (accabove != NULL)
            {
            stave_text(pstaff, "&turnA(%c,%s)", typechar, accabove[2]);
            turns_used |= TURNS_ABOVE;
            }
          else if (accbelow != NULL)
            {
            stave_text(pstaff, "&turnB(%c,%s)", typechar, accbelow[2]);
            turns_used |= TURNS_BELOW;
            }
          else
            {
            stave_text(pstaff, "&turn(%c)", typechar);
            turns_used |= TURNS_PLAIN;
            }
          if (below) stave_text(pstaff, "/b");
          if (delayed_turn) stave_text(pstaff, "/h");
          stave_text(pstaff, " ");
          }
        }
      }

    /* Handle non-rests; either pitch or unpitched is not NULL. Note that an
    invisible note has been turned into a rest. */

    if (rest == NULL)
      {
      int octave;
      uschar *step;
      uschar *stem = xml_get_string(mi, US"stem", NULL, FALSE);
      uschar *notehead = xml_get_string(mi, US"notehead", NULL, FALSE);
      item *accidental = xml_find_item(mi, US"accidental");

      if (stem != NULL && Ustrcmp(stem, "none") == 0)
        {
        if (set_stems[pstaff])
          {
          stave_text(pstaff, "[noteheads only]");
          set_stems[pstaff] = FALSE;
          }
        }
      else if (!set_stems[pstaff])
        {
        stave_text(pstaff, "[noteheads normal]");
        set_stems[pstaff] = TRUE;
        }

      /* Handle articulations. */

      if (notations != NULL)
        {
        BOOL stem_known = stem != NULL;
        BOOL stem_up = stem_known && Ustrcmp(stem, "up") == 0;
        item *ni;
        item *articulations = xml_find_item(notations, US"articulations");
        item *technical = xml_find_item(notations, US"technical");

        /* Articulations */

        if (articulations != NULL)
          {
          for (ni = articulations->next;
               ni != articulations->partner;
               ni = ni->partner->next)
            {
            const char **pa;
            attrstr *placement = xml_find_attr(ni, US"placement");
            uschar *atype = xml_get_attr_string(ni, US"type", NULL, FALSE);
            BOOL placement_known = placement != NULL;
            BOOL placement_above = placement_known &&
              Ustrcmp(placement->value, "above") == 0;

            /* MusicXML has two kinds of thing in <articulations>. Accents etc
            are marks on a note, but also included are breath mark and
            caesuara, which in PMW come after a note. These are indicated by
            starting with a space. */

            for (pa = pmw_articulations; *pa != NULL; pa += 2)
              {
              if (Ustrcmp(ni->name, *pa) == 0)
                {
                const char *ss = pa[1];

                if (atype != NULL && (Ustrcmp(ni->name, "strong-accent") != 0 ||
                    Ustrcmp(atype, "up") != 0))
                  Eerror(ni, ERR43, "Type", atype);

                if (*ss == ' ')
                  {
                  Ustrcat(pending_post_note[staff], ss);
                  }
                else
                  {
                  if (isalnum(ss[0])) conditional_space(options);
                  (void)Ustrcat(options, ss);
                  }
                break;
                }
              }
            if (*pa == NULL) Eerror(articulations, ERR43, US"articulation",
              ni->name);
            if (stem_known && placement_known && stem_up == placement_above)
              (void)Ustrcat(options, "!");
            }
          }


        /* Technical marks */

        if (technical != NULL)
          {
          const char *c = "d";
          item *bow = xml_find_item(technical, US"down-bow");

          if (bow == NULL)
            {
            bow = xml_find_item(technical, US"up-bow");
            c = "u";
            }

          if (bow != NULL)
            {
            //uschar *placement = xml_get_attr_string(bow, US"placement", "",
              //FALSE);
            conditional_space(options);
            (void)Ustrcat(options, c);

            // FIXME: think about placement
            }
          }
        }

      /* Handle pitch or unpitched */

      octave = clef_octave_change[staff];
      if (pitch != NULL)
        {
        step = xml_get_string(pitch, US"step", NULL, FALSE);
        if (step == NULL) Eerror(pitch, ERR17, "<step>");  /* Hard */
        octave += xml_get_number(pitch, US"octave", 0, 12, 4, TRUE);
        }

      else
        {
        step = xml_get_string(unpitched, US"display-step", NULL, FALSE);
        if (step == NULL) Eerror(unpitched, ERR17, "<display-step>");  /* Hard */
        octave += xml_get_number(unpitched, US"display-octave", 0, 12, 4, TRUE);
        }

      /* Failure of any of these tests is a hard error. */

      if (type == NULL) Eerror(mi, ERR17, "<type>");
      if (note_type_name == NULL) Eerror(type, ERR17, "value for <type>");

      /* Handle noteheads. */

      thisnotehead = 'o';    /* Default */
      if (notehead != NULL)
        {
        if (Ustrcmp(notehead, "diamond") == 0) thisnotehead = 'h';
        else if (Ustrcmp(notehead, "normal") == 0) thisnotehead = 'o';
        else if (Ustrcmp(notehead, "none") == 0) thisnotehead = 'z';
        else if (Ustrcmp(notehead, "x") == 0) thisnotehead = 'x';
        else Eerror(mi, ERR43, "notehead type", notehead);
        }

      if (thisnotehead != set_noteheads[pstaff])
        {
        if (inchord[staff])
          {
          if (thisnotehead == 'h' || thisnotehead == 'x')
            {
            conditional_space(options);
            (void)Ustrcat(options, (thisnotehead == 'h')? "nh" : "nx");
            }
          else Eerror(mi, ERR45, notehead);
          }
        else
          {
          stave_text(pstaff, "[%c] ", thisnotehead);
          set_noteheads[pstaff] = thisnotehead;
          }
        }

      /* The analysis phase inserts start/end chord items into the chain,
      because all MusicXML does is to flag the 2nd and subsequent notes of a
      chord. */

      if (xml_find_item(mi, US"pmw-chord-first") != NULL)
        {
        if (inchord[staff]) Eerror(mi, ERR28, "chord start within chord");
        inchord[staff] = TRUE;
        pending_beambreak[staff] = PBRK_UNKNOWN;
        stave_text(pstaff, "(");
        }

      /* Now the actual note */

      if (accidental != NULL)
        {
        uschar *astring = xml_get_this_string(accidental);
        for (n = 0; n < accidentals_count; n += 3)
          {
          if (Ustrcmp(astring, accidentals[n]) == 0)
            {
            stave_text(pstaff, "%s", accidentals[n+1]);
            break;
            }
          }
        if (n >= accidentals_count) Eerror(mi, ERR30, astring);
        if (ISATTR(accidental, "parentheses", "", FALSE, "yes"))
          stave_text(pstaff, ")");
        }

      /* PMW octave is 3 less than MXL octave */

      write_noteletter(pstaff, mi, note_type_name, *step,
        octave - 3 - current_octave[staff], dots);

      /* Handle stems */

      if (stem != NULL)
        {
        conditional_space(options);
        if (Ustrcmp(stem, "up") == 0) (void)Ustrcat(options, "su");
          else if (Ustrcmp(stem, "down") == 0) (void)Ustrcat(options, "sd");
        }

      /* Output any accents, ornaments, etc. */

      if (options[1] != 0) stave_text(pstaff, "%s\\", options);

      /* Ties must come after the end of a chord. */

      if (tied != TIED_NONE)
        {
        if (inchord[staff]) pending_tie[staff] = tied;
          else stave_text(pstaff, "_%s", (tied == TIED_ABOVE)? "/a" :
            (tied == TIED_BELOW)? "/b" : "");
        }

      /* Deal with beaming for notes shorter than a crotchet. If there is no
      beam item we break - for a chord we have to do this only if no note in
      the chord has a beam item. */

      if (note_duration < divisions)
        {
        int maxbn = -1;
        int isbreak[4];
        int breakleave;
        item *beam;

        isbreak[0] = isbreak[1] = isbreak[2] = isbreak[3] = PBRK_UNSET;

        for (beam = xml_find_item(mi, US"beam");
             beam != NULL;
             beam = xml_find_next(mi, beam))
          {
          uschar *s = xml_get_this_string(beam);
          BOOL begin = Ustrcmp(s, US"begin") == 0;
          BOOL cont = !begin && Ustrcmp(s, US"continue") == 0;
          BOOL end = !begin && !cont && Ustrcmp(s, US"end") == 0;
          int bn = xml_get_attr_number(beam, US"number", 1, 10, 1, FALSE) - 1;

          if (bn > maxbn) maxbn = bn;
          isbreak[bn] = end? PBRK_YES : PBRK_NO;
          }

        if (maxbn < 0)   /* No item found */
          {
          if (!inchord[staff])
            {
            isbreak[0] = PBRK_YES;
            maxbn = 0;
            }
          }
        else             /* Beam item found */
          {
          if (inchord[staff] && pending_beambreak[staff] == PBRK_UNKNOWN)
            pending_beambreak[staff] = PBRK_UNSET;
          }

        for (breakleave = 0; breakleave <= maxbn; breakleave++)
          {
          if (isbreak[breakleave] == PBRK_YES)
            {
            if (inchord[staff]) pending_beambreak[staff] = breakleave;
            else if (breakleave == 0) stave_text(pstaff, ";");
            else if (breakleave == 1) stave_text(pstaff, ",");
            else if (breakleave >= 2) stave_text(pstaff, ",%d", breakleave);
            break;
            }
          }
        }

      /* Close off after the last note of a chord. */

      if (xml_find_item(mi, US"pmw-chord-last") != NULL)
        {
        if (!inchord[staff]) Eerror(mi, ERR28, "chord end not within chord");
        inchord[staff] = FALSE;
        stave_text(pstaff, ")");

        if (pending_tie[staff] != TIED_NONE)
          {
          stave_text(pstaff, "_%s", (pending_tie[staff] == TIED_ABOVE)? "/a" :
            (pending_tie[staff] == TIED_BELOW)? "/b" : "");
          pending_tie[staff] = TIED_NONE;
          }

        /* Chord was shorter than a crotchet - check beam break. */

        if (note_duration < divisions)
          {
          if (pending_beambreak[staff] == 0 ||
              pending_beambreak[staff] == PBRK_UNKNOWN)
            stave_text(pstaff, ";");
          else if (pending_beambreak[staff] == 1)
            stave_text(pstaff, ",");
          else if (pending_beambreak[staff] >= 2)
            stave_text(pstaff, ",%d",pending_beambreak[staff]);
          }

        if (end_tuplet[staff])
          {
          stave_text(pstaff, "}");
          end_tuplet[staff] = FALSE;
          }
        stave_text(pstaff, "%s", pending_post_chord[staff]);
        pending_post_chord[staff][0] = 0;
        }

      else if (!inchord[staff] && end_tuplet[staff])
        {
        stave_text(pstaff, "}");
        end_tuplet[staff] = FALSE;
        }
      }

    /* Handle rests */

    else
      {
      uschar *dstep;
      int yadjust = 0;

      dstep = xml_get_string(rest, US"display-step", NULL, FALSE);

      if (dstep != NULL)
        {
        int doctave = xml_get_number(rest, US"display-octave", 0, 9, -1,
          TRUE);

        if (doctave >= 0)
          {
          int nstep = *dstep - 'A' - 2;
          if (nstep < 0) nstep += 7;

          /* Position on staff as for current clef is computed as how far above
          or below middle C plus the middle C position on the staff. */

          yadjust = 2*(nstep + 7*(doctave - 4) - 4 +
            clef_cposition[current_clef[pstaff]]);
          }
        }

      if (whole_bar_rest)
        {
        stave_text(pstaff, "%s", rest_wholebar);
        }
      else
        {
        if (note_type_name == NULL) Eerror(mi, ERR17, "<type> or its value");
        write_noteletter(pstaff, mi, note_type_name, rest_letter, 0, dots);
        }
      if (yadjust != 0) stave_text(pstaff, "\\l%d\\", yadjust);
      if (end_tuplet[staff])
        {
        stave_text(pstaff, "}");
        end_tuplet[staff] = FALSE;
        }
      }

    /* Handle tremolo between notes - if tremolo != NULL, this note has a
    tremolo start item. */

    if (tremolo != NULL)
      stave_text(pstaff, " [tremolo/x%d]", tremlines);

    /* Handle slur ends. If we are in the middle of a chord, save up the text,
    to be output after the end. */

    for (n = 0; n < (int)(sizeof(slurstop)/sizeof(int)); n++)
      {
      if (slurstop[n] < 0) break;

      if (!inchord[staff])
        {
        stave_text(pstaff, " [es");
        stave_text(pstaff, "/=%c", slurstop[n]);
        stave_text(pstaff, "]");
        }
      else
        {
        char *ss = pending_post_chord[staff];
        Ustrcat(ss, " [es");
        ss += Ustrlen(ss);
        sprintf(ss, "/=%c", slurstop[n]);
        Ustrcat(ss, "]");
        Ustrcat(ss, pending_post_note[staff]);
        }
      }

    /* Things like comma and caesura are notated in MusicXML as articulations,
    but in PMW are separate items that come after a note. */

    if (!inchord[staff])
      stave_text(pstaff, "%s", pending_post_note[staff]);
    else
      Ustrcat(pending_post_chord[staff], pending_post_note[staff]);
    pending_post_note[staff][0] = 0;
    }


  /* ======================== <print> ======================== */

  else if (Ustrcmp(mi->name, "print") == 0)
    {
    item *staff_layout, *system_layout;

    /* PMW measure numbering is only ever at the top of systems, not
    controllable for individual parts/staves. The best we can do is write a
    heading directive. There doesn't seem to be a separate measure numbering
    font size in MXL, so use the word default if there is one, but enforce a
    minimum. */

    uschar *mn = xml_get_string(mi, US"measure-numbering", NULL, FALSE);

    if (mn != NULL)
      {
      uschar *fss = US"";
      if (fontsize_word_default > 0)
        {
        int fsize = fontsizes[fontsize_word_default];
        if (fsize < 8000) fsize = 8000;
        fss = format_fixed(fsize);
        }

      if (Ustrcmp(mn, "system") == 0)
        fprintf(outfile, "Barnumbers line %s\n", fss);
      else if (Ustrcmp(mn, "measure") == 0)
        fprintf(outfile, "Barnumbers 1 %s\n", fss);
      else if (Ustrcmp(mn, "none") != 0)
        Eerror(mi, ERR31, mn);
      }

    /* Newlines are currently turned into layout items, so for the moment,
    ignore here. There may be cases where they are relevant, or maybe there
    will be an option to do some other handling. */

#ifdef NEVER
    if (ISATTR(mi, "new-system", "", FALSE, "yes"))
      stave_text(pstave+1, "@ [newline]\n");   /* Put it on 1st stave of part */
#endif

    /* MusicXML's staff distance is above the relevant staff, and does not
    include the staff itself, unlike PMW, whose normal spacing is below the
    staff and is inclusive. If the space is specified for the second or later
    staff in the part, we put [sshere] on the previous staff, assuming that
    they are printed or suspended together. If it's for the first staff of a
    part, we use PMW's special [ssabove] directive, which ensures a spacing
    above the current staff, because this will work even if the previous staff
    is suspended. However, it cannot reduce the spacing. */

    for (staff_layout = xml_find_item(mi, US"staff-layout");
         staff_layout != NULL;
         staff_layout = xml_find_next(mi, staff_layout))
      {
      int d = xml_get_number(staff_layout, US"staff-distance", 0, 1000, -1,
        FALSE);
      if (d > 0)
        {
        int dfixed = d*400 + 16000;
        n = xml_get_attr_number(staff_layout, US"number", 1, 100, 1, FALSE);

        if (starting_noprint)
          {
          sprintf(save_ssabove[n], " [ssabove %s]\n", format_fixed(dfixed));
          }
        else
          {
          if (n == 1)
            stave_text(pstave + n, " [ssabove %s]\n", format_fixed(dfixed));
          else
            {
            n += pstave - 1;                           /* This stave */
            if (couple_settings[n] == COUPLE_DOWN ||   /* When coupled, must */
                couple_settings[n+1] == COUPLE_UP)     /* round to 4 pts */
              {
              dfixed += 3999;
              dfixed -= dfixed % 4000;
              }
            stave_text(n, " [sshere %s]\n", format_fixed(dfixed));
            } 
          }
        }
      }

    /* Similarly for the system gap, though there is no "number" to deal with;
    just put the directive on the first staff of this part. */

    for (system_layout = xml_find_item(mi, US"system-layout");
         system_layout != NULL;
         system_layout = xml_find_next(mi, system_layout))
      {
      int d = xml_get_number(system_layout, US"system-distance", 0, 1000, -1,
        FALSE);
      if (d > 0)
        {
        uschar *s = format_fixed(d*400 + 16000);
        stave_text(pstave+1, " [sgabove %s sgnext %s]\n", s, s);
        }
      }
    }


  /* ======================== pmw-suspend ======================== */

  else if (Ustrcmp(mi->name, "pmw-suspend") == 0)
    {
    int number = xml_get_attr_number(mi, US"number", 1, scount, -1, FALSE);
    if (number > 0)
      stave_text(pstave + number, " [suspend]");
    else
      for (staff = 1; staff <= scount; staff++)
        stave_text(pstave + staff, " [suspend]");
    }


  /* ======================== Known, but ignored ======================== */

  else if (Ustrcmp(mi->name, "sound") == 0 ||
           Ustrcmp(mi->name, "xxxxxx") == 0)
    {
    }


  /* ======================== Unknown ========================  */

  else
    {
    Eerror(mi, ERR26, measure_number, part_id);  /* Warning */
    }
  }  /* Repeat for items in the measure */


/* End of measure; write a PMW barline for each stave and put in an extra
newline every 5 measures, except at the end of the part. */

for (staff = 1; staff <= scount; staff++)
  {
  pstaff = pstave + staff;
  if (hadrepeat && duration[staff] != measure_length)
    stave_text(pstaff, " [nocheck]");
  if (!lastwasbarline) stave_text(pstaff, " |");
  stave_text(pstaff, " @%s\n", measure_number_text);
  if (measure_number % 5 == 0) stave_text(pstaff, "\n");
  }

return yield;
}



/*************************************************
*            Write part name(s)                  *
*************************************************/

/* Use the display name if set, else name. */

static BOOL
do_stave_name(item *name, item *name_display, int pstaff, const char *pretext,
  BOOL middle)
{
BOOL yield = FALSE;
uschar buff[256];
buff[0] = 0;

if (name_display != NULL)
  {
  item *pp;

  for (pp = name_display->next;
       pp != name_display->partner;
       pp = pp->partner->next)
    {
    uschar *ss = xml_get_this_string(pp);
    if (Ustrcmp(pp->name, "display-text") == 0) Ustrcat(buff, ss);
    else if (Ustrcmp(pp->name, "accidental-text") == 0)
      {
      if (Ustrcmp(ss, "flat") == 0)
        Ustrcat(buff, "\\*$\\");
      else if (Ustrcmp(ss, "sharp") == 0)
        Ustrcat(buff, "\\*#\\");
      else Eerror(pp, ERR43, US"accidental-text", ss);
      }
    }
  }

else if (name != NULL)
  {
  if (ISATTR(name, US"print-object", "yes", FALSE, "yes"))
    Ustrcat(buff, xml_get_this_string(name));
  }

if (buff[0] != 0)
  {
  uschar *p;
  for (p = buff; *p != 0; p++) if (*p == '\n') *p = '|';
  stave_text_quoted_string(pstaff, buff, pretext, NULL, NULL);
  if (middle) stave_text(pstaff, "/m");
  if (right_justify_stave_names) stave_text(pstaff, "/e");
  yield = TRUE;
  }

return yield;
}
#endif


/*************************************************
*        Generate parts from partwise XML        *
*************************************************/

void
xml_do_parts(void)
{

#ifdef NEVER

int pstaff;
part_data *p;
group_data *g;
int prev_pmw_stave = 0;
group_data *group_name_staves[64];
group_data *group_abbrev_staves[64];

for (pstaff = 0; pstaff <= PARTSTAFFMAX; pstaff++)
  {
  current_octave[pstaff] = 1;
  clef_octave_change[pstaff] = 0;
  }

for (pstaff = 0; pstaff <= pmw_stave_count; pstaff++)
  group_name_staves[pstaff] = group_abbrev_staves[pstaff] = NULL;

/* Scan the groups to check for group names. When found, find the PMW stave
with which to associate the name. */

for (g = groups_list; g != NULL; g = g->next)
  {
  if (g->name != NULL || g->name_display != NULL ||
      g->abbreviation != NULL || g->abbreviation_display != NULL)
    {
    int n = g->last_pstave - g->first_pstave + 1;  /* Number of staves */
    pstaff = g->first_pstave + n/2 - (((n & 1) == 0)? 1:0);
    if (g->name != NULL || g->name_display != NULL)
      group_name_staves[pstaff] = g;
    if (g->abbreviation != NULL || g->abbreviation_display != NULL)
      group_abbrev_staves[pstaff] = g;
    }
  }

/* Now scan the parts */

for (p = parts_list; p != NULL; p = p->next)
  {
  int n;
  item *measure;
  BOOL first = TRUE;

  for (n = 0; n <= PARTSTAFFMAX; n++)
    {
    save_clef[n][0] = 0;
    save_key[n][0] = 0;
    save_ssabove[n][0] = 0;
    save_time[n][0] = 0;
    }

  DEBUG(D_write) debug_printf("Part %s \"%s\"\n", p->id, p->name->name);

  /* Stave start for each stave used by this part */

  for (pstaff = prev_pmw_stave + 1; pstaff <= prev_pmw_stave + p->stave_count;
       pstaff++)
    {
    stave_text(pstaff, "[stave %d", pstaff);

    if (first)
      {
      BOOL hasname = do_stave_name(p->name, p->name_display, pstaff, " ",
          p->stave_count == 2);

      if ((g = group_name_staves[pstaff]) != NULL)
        {
        if (hasname) stave_text(pstaff, "/");
        (void)do_stave_name(g->name, g->name_display, pstaff, "",
          ((g->last_pstave - g->first_pstave) & 1) != 0);
        }

      hasname = do_stave_name(p->abbreviation, p->abbreviation_display, pstaff,
        " ", p->stave_count == 2);

      if ((g = group_abbrev_staves[pstaff]) != NULL)
        {
        if (hasname) stave_text(pstaff, "/");
        (void)do_stave_name(g->abbreviation, g->abbreviation_display, pstaff,
          "", ((g->last_pstave - g->first_pstave) & 1) != 0);
        }

      first = FALSE;
      }

    if (fontsize_word_default > 0)
      stave_text(pstaff, " textsize %d", fontsize_word_default + 1);
    if (p->noprint_before > 1) stave_text(pstaff, " omitempty");

    switch (couple_settings[pstaff])
      {
      case COUPLE_UP: stave_text(pstaff, " couple up"); break;
      case COUPLE_DOWN: stave_text(pstaff, " couple down"); break;
      default: break;
      }

    stave_text(pstaff, "]\n", pstaff);
    }

  /* Scan the measures for this part */

  measure_number = 1;
  measure_number_absolute = 1;
  next_measure_fraction = 0;

  for (measure = xml_find_item(p->part, US"measure");
       measure != NULL;
       measure = xml_find_next(p->part, measure))
    {
    DEBUG(D_write) debug_printf("Measure %d\n", measure_number);
    measure_number += write_measure(prev_pmw_stave, p, measure);
    measure_number_absolute++;
    }

  prev_pmw_stave += p->stave_count;
  }

/* Avoid a double newline before [endstave], which can happen when the number
of bars is a multiple of 5. */

for (pstaff = 1; pstaff < 64; pstaff++)
  {
  stave_buffer *b = pmw_buffers + pstaff;
  if (b->buffer == NULL) break;
  if (b->buffer[b->hwm-2] == '\n') b->hwm--;
  stave_text(pstaff, "[endstave]\n\n");
  }
  
#endif 

}




#ifdef NEVER
/*************************************************
*             Main write function                *
*************************************************/

BOOL
write_output(uschar *filename)
{
int i;
BOOL yield;
BOOL used_dynamics = FALSE;

for (i = 0; i < 64; i++)
  {
  pmw_buffers[i].buffer = NULL;
  pmw_buffers[i].size = 0;
  pmw_buffers[i].hwm = 0;
  pmw_buffers[i].lastwasnl = TRUE;
  all_pending[i] = FALSE;
  current_clef[i] = Clef_treble;
  set_noteheads[i] = 'o';    /* Normal */
  set_stems[i] = TRUE;       /* Stems are on */
  open_wedge_type[i] = 0;    /* Unset */
  open_wedge_start[i] = -1;  /* Unset */
  }

fontsizes[0] = 10000;
fontsize_next = 1;

/* Set up the output file. */

if (filename == NULL || Ustrcmp(filename, "-") == 0)
  {
  DEBUG(D_any) debug_printf("==> Writing to stdout\n");
  outfile = stdout;
  }
else
  {
  DEBUG(D_any) debug_printf("==> Writing to %s\n", filename);
  outfile = Ufopen(filename, "wb");
  if (outfile == NULL)
    error(ERR0, filename, "output file", strerror(errno));  /* Hard error */
  }

if (!suppress_version)
  {
  char datebuf[20];
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  strftime(datebuf, sizeof(datebuf), "%d %b %Y", local);
  fprintf(outfile, "@ PMW input created from MusicXML by mxl2pmw %s on %s\n",
    MXL2PMW_VERSION, datebuf);
  }

/* Process the music partwise or time wise. */

if (partwise_item_list != NULL)
  {
  heading_write(partwise_item_list);
  yield = write_partwise();
  }
else
  {
//  process_heading(timewise_item_list);
  yield = write_timewise();
  }

/* Output the list of font sizes if there are any other than the default
10-point size. For all except the default 10pt font, we unscale by the
magnification so that, when re-scaled by PMW they are the sizes specified in
the XML. This sometimes means that the first two end up at the same size. */

if (fontsize_next > 1)
  {
  fprintf(outfile, "Textsizes 10");
  for (i = 1; i < fontsize_next; i++)
    fprintf(outfile, " %s", format_fixed(MULDIV(fontsizes[i], 1000,
      magnification)));
  fprintf(outfile, "\n");
  }

/* Output stave size setting if required */

if (set_stave_size)
  {
  fprintf(outfile, "Stavesizes");
  for (i = 1; i <= pmw_stave_count; i++)
    if (stave_sizes[i] > 0) fprintf(outfile, " %d/%s", i,
      format_fixed(stave_sizes[i]));
  fprintf(outfile, "\n");
  }

/* Output rehearal mark setting if required */

if ((rehearsal_enclosure >= 0 &&
     rehearsal_enclosure != REHEARSAL_ENCLOSURE_BOX)  ||
    (rehearsal_size >= 0 && rehearsal_size != 12000) ||
    (rehearsal_type >= 0 && rehearsal_type != REHEARSAL_TYPE_ROMAN))
  {
  fprintf(outfile, "Rehearsalmarks %s %s %s\n",
    (rehearsal_enclosure == REHEARSAL_ENCLOSURE_RING)? "ringed" : "boxed",
    (rehearsal_size >= 0)? CS format_fixed(rehearsal_size) : "12",
    (rehearsal_type == REHEARSAL_TYPE_ITALIC)? "italic" :
    (rehearsal_type == REHEARSAL_TYPE_BOLD)? "bold" :
    (rehearsal_type == REHEARSAL_TYPE_BOLDITALIC)? "bolditalic" : "roman");
  }

/* Output nocheck if forced by command line option and not otherwise set. */

if (nocheck) fprintf(outfile, "Nocheck  @ Do not check bar lengths\n");

/* Output macros for the dynamics that have been used. */

for (i = 0; i < dynamics_count; i++)
  {
  if (!dynamics[i].used) continue;
  if (!used_dynamics)
    {
    fprintf(outfile, "\n@ Macros for dynamics\n");
    used_dynamics = TRUE;
    }
  fprintf(outfile, "*define %s \"%s\"/b\n", dynamics[i].name,
    dynamics[i].string);
  }

/* Output other macros */

if (turns_used != 0)
  {
  fprintf(outfile, "\n@ Macros for turns\n");
  if ((turns_used & TURNS_PLAIN) != 0)
    fprintf(outfile, "*define turn() \"\\mf\\&&1\"/a\n");

  if ((turns_used & TURNS_ABOVE) != 0)
    fprintf(outfile, "*define turnA() \"\\mf\\&&1xxzz\\mu\\&&2\"/a\n");

  if ((turns_used & TURNS_BELOW) != 0)
    fprintf(outfile, "*define turnB() \"\\mf\\xx&&1wwzz\\mu\\&&2\"/a\n");

  if ((turns_used & TURNS_BOTH) != 0)
    fprintf(outfile, "*define turnAB() \"\\mf\\xx&&1xxzz\\mu\\&&2\\mf\\wwww{{&&3\"/a\n");
  }

fprintf(outfile, "\n");

/* Output the parts */

for (i = 1; i < 64; i++)
  {
  if (pmw_buffers[i].buffer == NULL) break;
  fprintf(outfile, "%s", pmw_buffers[i].buffer);
  }

fprintf(outfile, "@ End\n");

if (outfile != stdout) (void)fclose(outfile);
DEBUG(D_any) debug_printf("Finished writing\n");
return yield;
}

#endif

/* End of xml_staves.c */
