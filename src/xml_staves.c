/*************************************************
*             MusicXML input for PMW             *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last modified: March 2022 */


/* This module contains functions for generating stave data */


#include "pmw.h"


/* Shorthand for font values for constructing static PMW strings. */

#define MF (font_mf<<24)
#define RM (font_rm<<24)
#define IT (font_it<<24)
#define BF (font_bf<<24)
#define BI (font_bi<<24)

#define BREAK_UNSET 100


#ifdef NEVER


enum { REHEARSAL_TYPE_ROMAN,
       REHEARSAL_TYPE_ITALIC,
       REHEARSAL_TYPE_BOLD,
       REHEARSAL_TYPE_BOLDITALIC };

enum { REHEARSAL_ENCLOSURE_BOX,
       REHEARSAL_ENCLOSURE_RING };

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
} stave._buffer;

/* Structure for remembering the start of a slur or line */

typedef struct sl_start {
  struct sl_start *next;
  size_t offset;
  int pstaff;
  int id;
  int pmw_id;
  int default_y;
} sl_start;
#endif

/* Structure for remembering default-x settings for notes so that after
<backup> other notes can be moved. */

typedef struct note_position {
  int32_t moff;    /* Musical offset */
  int32_t x;       /* default-x value */
} note_position;

#ifdef NEVER
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
#endif

static int beam_leastbreak[PARTSTAFFMAX+1];
static int beam_breakpending[PARTSTAFFMAX+1];
static BOOL beam_seen[PARTSTAFFMAX+1];

static int       clef_octave_change[PARTSTAFFMAX+1];
static int       current_clef[PARTSTAFFMAX+1];
//static int       current_octave[PARTSTAFFMAX+1];

static int       custom_key_count = 0;

static int32_t   divisions = 8;
static int32_t   duration[PARTSTAFFMAX+1];

static xml_group_data *group_name_staves[64];
static xml_group_data *group_abbrev_staves[64];

static b_hairpinstr *open_wedge[PARTSTAFFMAX+1];

static void     *last_item_cache[PARTSTAFFMAX];

static uint32_t  measure_length;
static int       measure_number;
static int       measure_number_absolute;
static int       next_measure_fraction;
static int       pending_backup[PARTSTAFFMAX+1];
static bstr     *pending_post_chord[PARTSTAFFMAX+1];

#ifdef NEVER
static uschar   *part_id;
static int       next_custom_key = 1;
static int       rehearsal_enclosure = -1;  /* Unset */
static int       rehearsal_size = -1;
static int       rehearsal_type = -1;
#endif

//static char      save_clef[PARTSTAFFMAX+1][24];
//static char      save_key[PARTSTAFFMAX+1][24];
//static char      save_ssabove[PARTSTAFFMAX+1][24];
//static char      save_time[PARTSTAFFMAX+1][24];

static BOOL      suspended[PARTSTAFFMAX+1];

static BOOL      tuplet_size_set = FALSE;

#ifdef NEVER
static int       turns_used = 0;

static stave_buffer pmw_buffers[64];
#endif

static int       set_noteheads[PARTSTAFFMAX+1];

static uschar    slurchars[] = "ABCDEFGHIJKLMNOPQ";

#ifdef NEVER
static BOOL      set_stems[64];
static size_t    staff_bar_start[64];

static sl_start *slur_free_starts = NULL;
static sl_start *slur_starts = NULL;

static sl_start *line_free_starts = NULL;
static sl_start *line_starts = NULL;

static nt_save  *nt_save_list = NULL;
#endif


/*************************************************
*              Local tables                      *
*************************************************/


typedef struct {
  uschar  *name;
  uint32_t length;
  uint8_t  pmwtype;
  uint8_t  pmwchar;
} note_type;

static note_type note_types[] = {
  { US"breve",   len_breve,     breve,     '1' },
  { US"whole",   len_semibreve, semibreve, '2' },
  { US"half",    len_minim,     minim,     '3' },
  { US"quarter", len_crotchet,  crotchet,  '5' },
  { US"eighth",  len_quaver,    quaver,    '7' },
  { US"16th",    len_squaver,   squaver,   '9' },
  { US"32nd",    len_dsquaver,  dsquaver,   0  },
  { US"64th",    len_hdsquaver, hdsquaver,  0  }
};

static int note_types_count = (sizeof(note_types)/sizeof(note_type));

typedef struct {
  uschar   *name;
  uint32_t  string[3];
} dynamic_type;

static dynamic_type dynamics[] = {
  { US"f",   { BI|'f', 0, 0 } },
  { US"p",   { BI|'p', 0, 0 } },
  { US"ff",  { BI|'f', BI|'f', 0 } },
  { US"fp",  { BI|'f', BI|'p', 0 } },
  { US"pp",  { BI|'p', BI|'p', 0 } },
  { US"mf",  { IT|'m', BI|'f', 0 } },
  { US"sf",  { IT|'s', BI|'f', 0 } },
  { US"mp",  { IT|'m', BI|'p', 0 } }
};

static int dynamics_count = (sizeof(dynamics)/sizeof(dynamic_type));

/* Barline styles are implemented either as a PMW barline type and style, or by
a PMW font string followed by an invisible barline (at least pro tem, until
more PMW styles are available). */

typedef struct {
  uschar  *name;
  uint16_t type;
  uint16_t style;
  uint32_t mfstring[10];
} barlinestyle;

static barlinestyle barstyles[] = {
  { US"regular",     barline_normal,    0, {0} },
  { US"dotted",      barline_invisible, 0, {MF|'{', MF|'w', MF|'>', MF|'x', MF|'>', MF|'x', MF|'>', MF|'x', MF|'>', 0} },
  { US"dashed",      barline_normal,    1, {0} },
  { US"heavy",       barline_invisible, 0, {MF|'B', 0} },
  { US"light-light", barline_double,    0, {0} },
  { US"light-heavy", barline_ending,    0, {0} },
  { US"heavy-light", barline_invisible, 0, {MF|'B', MF|'{', MF|'y', MF|'@', 0} },
  { US"heavy-heavy", barline_invisible, 0, {MF|'B', MF|'{', MF|'y', MF|'B', 0} },
  { US"tick",        barline_invisible, 0, {MF|'x', MF|'x', MF|'x', MF|'~', MF|0x84u, 0} },
  { US"short",       barline_normal,    4, {0} },
  { US"none",        barline_invisible, 0, {0} }
};

static int barstyles_count = (sizeof(barstyles)/sizeof(barlinestyle));

static uint32_t plainturn[]  = { MF|'S', 0 };
static uint32_t plainiturn[] = { MF|'i', 0 };


/* PMW conventional key signatures, indexed by number of accidentals, positive
for sharps, negative for flats, offset by 7 for major keys and offset by 22 for
minor ones. */

static const int keys[] = {
/* C$   G$   D$   A$   E$   B$   F  C  G  D  A  E  B  F# C#     major keys */
   16,  20,  17,  14,  18,  15,  5, 2, 6, 3, 0, 4, 1, 12, 9,

/* A$  E$  B$   F   C   G   D   A   E   B  F#  C#  G#  D#  A#   minor keys */
   36, 40, 37, 26, 23, 27, 24, 21, 25, 22, 33, 30, 34, 31, 28 };



#ifdef NEVER
static const char *irests[] = {
  "Q+", "Q", "q", "q-", "q=", "q=-", "q==" };

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

static const char *ordabbrev[] =
  { "", "st", "nd", "rd", "th", "th", "th", "th", "th", "th" };
#endif


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

static int fonttypes[] = { font_rm, font_it, font_bf, font_bi };

static int cclefs[] = {
  -1, clef_soprano, clef_mezzo, clef_alto, clef_tenor, clef_cbaritone };

//static const int cclef_octaves[] = { 0, 1, 1, 1, 0, 0 };

/* In the accidentals list, the third column contains the PMW ornament type
for trills with that accidental above, or unset if that one is not supported.
The fourth column codes for the accidental in the music font. Most are just a
single character, but double-flat is two packed up characters. */

typedef struct accdef {
  uschar  *name;
  int32_t  value;
  int32_t  trillvalue;
  int32_t  accchars;
} accdef;

static const accdef accidentals[] = {
  { US"sharp",         ac_sh, or_trsh,  '%' },
  { US"flat",          ac_fl, or_trfl,  '\'' },
  { US"natural",       ac_nt, or_trnat, '(' },
  { US"double-sharp",  ac_ds, or_unset, '&' },
  { US"flat-flat",     ac_df, or_unset, ('\'' << 8) | '\'' },
  { US"slash-flat",    ac_hf, or_unset, 191 },
  { US"quarter-sharp", ac_hs, or_unset, 189 }
};

static int accidentals_count = (sizeof(accidentals)/sizeof(accdef));

/* These strings code for fancy turns with accidentals above and/or below. They
are turned into text strings in the music font by replacing T with the turn
character, A with an above accidental, and B with a below accidental. The first
is used when there's just an accidental above, the second for below, and the
third for both. */

static const char *turnA = "TxxzzA";
static const char *turnB = "xxTwwzzB";
static const char *turnC = "xxTxxzzAwwww{{B";

typedef struct articdef {
  uschar  *name;
  uint32_t flags;
} articdef;

static const articdef pmw_articulations[] = {
  { US"accent",           af_gt },
  { US"breath-mark",      b_comma },
  { US"caesura",          b_caesura },
  { US"detached-legato",  af_staccato|af_bar },
  { US"spiccato",         af_staccatiss },
  { US"staccatissimo",    af_wedge },
  { US"staccato",         af_staccato },
  { US"strong-accent",    af_tp },
  { US"tenuto",           af_bar }
};

static int pmw_articulations_count = sizeof(pmw_articulations)/sizeof(articdef);



#ifdef NEVER

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

#endif



/*************************************************
*       Handle vertical positioning of text      *
*************************************************/

/* It seems that default-y is relative to the top of the stave. If it is
non-negative, we can force an "above" value. Otherwise, adjust for a PMW
"below" string.

Arguments:
  tx         the text item
  dy         the vertical adjustment
  above      TRUE for "above"

Returns:     nothing
*/

static void
handle_text_dy(b_textstr *tx, int dy, BOOL above)
{
tx->y = dy * 400;   /* Convert tenths to millipoints */
if (dy >= 0) above = TRUE;
if (above) tx->flags |= text_above; else tx->y += 16000;
tx->flags |= text_absolute;
}



#ifdef NEVER
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
#endif



/*************************************************
*       Get a PMW item on appropriate chain      *
*************************************************/

/* Because MusicXML parts may contain multiple staves, we have to keep track of
the last item for multiple staves, so all PMW items are obtained via this
indirection function.

Arguments:
  n        number of stave within part (starting at 1)
  size     size of item
  type     item type

Returns:   the new item
*/

static void *
xml_get_item(int n, size_t size, usint type)
{
void *yield;
read_lastitem = last_item_cache[n];
yield = mem_get_item(size, type);
last_item_cache[n] = yield;
return yield;
}



/*************************************************
*             Duplicate a PMW item               *
*************************************************/

/* Put a new item on the end of the chain that is a duplicate of an existing
item.

Arguments:
  n        number of stave within part (starting at 1)
  p        item to copy
  size     size of item

Returns:   the new item
*/

static void *
xml_duplicate_item(int n, void *p, size_t size)
{
size_t offset = offsetof(bstr, type);
void *new = xml_get_item(n, size, 0);
memcpy((char *)new + offset, (char *)p + offset, size - offset);
return new;
}



/*************************************************
*             Handle pending backup              *
*************************************************/

/* Handle a pending backup, which in practive can be a move in either
direction. The pending backup value is where we want to be in the measure.
Check for following a tie - PMW cannot support this, so give warning and remove
the tie.

Arguments:
  staff     the local staff number
  where     the item we are about to process (for error messages)

Returns:    nothing
*/

static void
do_pending_backup(int staff, xml_item *where)
{
bstr *last;
b_notestr *rest;
int bac = pending_backup[staff];  /* Where we need to be in the bar */
int dur = duration[staff];        /* Where we are in the bar */

pending_backup[staff] = -1;
if (bac < 0 || bac == dur) return;
duration[staff] = bac;

last = last_item_cache[staff];
if (last->type == b_tie)
  {
  xml_Eerror(where, ERR53);
  last_item_cache[staff] = last->prev;
  }

if (bac >= dur) bac -= dur; else
  {
  (void)xml_get_item(staff, sizeof(bstr), b_reset);
  beam_breakpending[staff] = BREAK_UNSET;
  }

/* bac is now the amount we need to move forward. Create an invisible rest. */

if (bac != 0)
  {
  rest = xml_get_item(staff, sizeof(b_notestr), b_note);
  rest->notetype = crotchet;  /* This is irrelevant for an invisible rest */
  rest->masq = MASQ_UNSET;    /* So is this */
  rest->acc = 0;
  rest->accleft = 0;
  rest->acflags = 0;
  rest->flags = nf_hidden;
  rest->length = bac * (len_crotchet/divisions);
  rest->yextra = 0;
  rest->abspitch = 0;
  rest->spitch = 0;
  rest->acc_orig = 0;
  rest->char_orig = 'Q';
  }
}



/*************************************************
*              Insert an ornament                *
*************************************************/

static bstr *
insert_ornament(bstr *where, int orval)
{
b_ornamentstr *os = mem_get_insert_item(sizeof(b_ornamentstr), b_ornament,
  where);
os->ornament = orval;
os->bflags = 0;
os->x = 0;
os->y = 0;
return (bstr *)os;
}



/*************************************************
*           Insert a PMW text string             *
*************************************************/

/* Used when we have either a static or constructed PMW string.

Arguments:
  n          number of staff - used if p is NULL
  p          item to insert before, if NULL, add to end of staff items
  s          the string
  flags      text flags
  x          an x offset
  halfway    the halfway value
  offset     the crotchet offset value

Returns:     pointer to the text item
*/

static b_textstr *
stave_text_pmw(int n, bstr *p, uint32_t *s, uint32_t flags, int32_t x,
  int32_t halfway, int32_t offset)
{
b_textstr *tx = (p != NULL)?
  mem_get_insert_item(sizeof(b_textstr), b_text, p)
  :
  xml_get_item(n, sizeof(b_textstr), b_text);

tx->flags = flags;
tx->string = s;
tx->x = x;
tx->y = 0;
tx->rotate = 0;
tx->halfway = halfway;
tx->offset = offset;
tx->size = 0;
tx->htype = 0;
tx->laylevel = 0;
tx->laylen = 0;
return tx;
}



/*************************************************
*           Insert a UTF text string             *
*************************************************/

/* Used for straightforward strings such as lyrics. Overestimate the size of
the 32-bit PMW character string by using the byte-lengths of the constituents.
This could be improved, at a computational cost, by inventing a string
character count function. It hardly seems worth it.

Arguments:
  n         number of stave within part (starting from 1)
  pre       preliminary text
  t         main text
  post      following text
  font      font type
  flags     flags

Returns:    the new text item
*/

static b_textstr *
stave_text_utf(int n, uschar *pre, uschar *t, uschar *post, uint32_t font,
  uint32_t flags)
{
b_textstr *tx = xml_get_item(n, sizeof(b_textstr), b_text);
size_t size = Ustrlen(pre) + Ustrlen(t) + Ustrlen(post);
uint32_t *pmw_string = mem_get(sizeof(uint32_t) * (size + 1));
uint32_t *p = pmw_string;

p += xml_convert_utf8(p, pre, font, FALSE);
p += xml_convert_utf8(p, t, font, FALSE);
p += xml_convert_utf8(p, post, font, FALSE);

tx->flags = flags;
tx->string = pmw_string;
tx->x = 0;
tx->y = 0;
tx->rotate = 0;
tx->halfway = 0;
tx->offset = 0;
tx->htype = 0;
tx->laylevel = 0;

/* If this is an underlay string we must set the length, which excludes
syllable and extender flag characters, and also select the underlay size. */

if ((flags & text_ul) != 0)
  {
  tx->laylen = p - pmw_string;
  if (tx->laylen > 1)
    {
    uint32_t last = PCHAR(p[-1]);
    if (last == '-' || last == '=') tx->laylen -= 1;
    }
  tx->size = ff_offset_ulay;
  }

/* Not an underlay string */

else
  {
  tx->laylen = 0;
  tx->size = 0;
  }

return tx;
}




/*************************************************
*            Search for a custom key             *
*************************************************/

/* This is used to see if a custom key actually matches a standard key or if it
matches an existing custom key.

Arguments:
  klist     point to the custom key
  n         where to start searching
  count     how many to search

Returns:    key number of -1 if not found
*/

static int
find_key(uint8_t *klist, int n, int count)
{
for (; count > 0; count--, n++)
  {
  uint8_t *cp = klist;
  uint8_t *kp = keysigtable[n];
  for (;;)
    {
    if (*cp != *kp++) break;
    if (*cp++ == ks_end) return n;
    }
  }
return -1;
}



/*************************************************
*      Set note (or rest) type and length        *
*************************************************/

/* This is used for notes and non-whole-bar rests.

Arguments:
  note        the current note
  type_name   the note type name, e.g. "quarter"
  dots        number of augmentation dots
  tuplet_num  numerator for tuplet or 1
  tuplet_den  denominator for tuplet or 1
  mi          current measure item ) for use in error messages
  type        <type> item          )

Returns:      nothing
*/

static void
note_type_and_length(b_notestr *note, uschar *type_name, int dots,
  int tuplet_num, int tuplet_den, xml_item *mi, xml_item *type)
{
int i;
uint32_t blen;

if (type == NULL) xml_Eerror(mi, ERR17, "<type>");
if (type_name == NULL) xml_Eerror(type, ERR17, "value for <type>");

for (i = 0; i < note_types_count; i++)
  if (Ustrcmp(type_name, note_types[i].name) == 0) break;
if (i >= note_types_count) xml_Eerror(mi, ERR18, type_name);  /* Hard */

note->notetype = note_types[i].pmwtype;
blen = note->length = note_types[i].length;

if (dots > 0) note->flags |= nf_dot;
if (dots > 1) note->flags |= nf_dot2;
while (dots-- > 0)
  {
  blen /= 2;
  note->length += blen;
  }

note->length = (note->length * tuplet_num) / tuplet_den;
}



/*************************************************
*       Add item to pending post-chord list      *
*************************************************/

/* This adds the item to the end of the list. */

static void
add_pending_post_chord(int staff, bstr *bs)
{
bstr *ppc = pending_post_chord[staff];
bs->next = NULL;
if (ppc == NULL)
  {
  pending_post_chord[staff] = (bstr *)bs;
  bs->prev = NULL;
  }
else
  {
  while (ppc->next != NULL) ppc = ppc->next;
  ppc->next = bs;
  bs->prev = ppc;
  }
}



/*************************************************
*        Process one measure for one part        *
*************************************************/

/*
Arguments:
  prevstave  previous PMW stave
  p          the part data block
  measure    points to <measure>

Returns:    1 for a normal measure; 0 for an implicit (uncounted) measure
*/

static int
do_measure(int prevstave, xml_part_data *p, xml_item *measure)
{
int note_positions_next = 0;
int scount = p->stave_count;
int pmwstaff, staff;
int yield;

int tuplet_num[PARTSTAFFMAX+1];
int tuplet_den[PARTSTAFFMAX+1];

BOOL end_tuplet[PARTSTAFFMAX+1];
BOOL contains_backup= xml_find_item(measure, US"backup") != NULL;
BOOL lastwasbarline;

b_notestr *inchord[PARTSTAFFMAX+1];

note_position note_positions[100];


#ifdef NEVER
int pending_tie[PARTSTAFFMAX+1];

char pending_post_chord[PARTSTAFFMAX+1][32];
char pending_post_note[PARTSTAFFMAX+1][32];
uschar *measure_number_text[16];

BOOL starting_noprint = FALSE;
#endif



for (int n = 0; n <= PARTSTAFFMAX; n++)
  {
  beam_leastbreak[n] = BREAK_UNSET;
  beam_breakpending[n] = BREAK_UNSET;
  beam_seen[n] = FALSE;

  duration[n] = 0;
  end_tuplet[n] = FALSE;
  inchord[n] = NULL;
  pending_backup[n] = -1;
  pending_post_chord[n] = NULL;
  tuplet_num[n] = tuplet_den[n] = 1;

//  pending_tie[n] = TIED_NONE;
//  pending_post_note[n][0] = 0;
//  staff_bar_start[pstave+n] = pmw_buffers[pstave+n].hwm;
  }

#ifdef NEVER
staff = 0;
lastwasbarline = FALSE;

/* Handle end of repeated bars */

for (n = 1; n <= scount; n++)
  {
  if (all_pending[pstave + n])
    {
    stave_text(pstave + n, "[all]");
    all_pending[pstave + n] = FALSE;
    }
  }
#endif

/* The "implicit" attribute means this measure it not counted. */

if (ISATTR(measure, "implicit", "no", FALSE, "yes"))
  {
  if (measure_number == 1 && next_measure_fraction == 0)
    {
    curmovt->barvector[measure_number_absolute] = 0;
    next_measure_fraction = 1;
    }
  else
    {
    curmovt->barvector[measure_number_absolute] =
      (measure_number << 16) | next_measure_fraction++;
    }
  yield = 0;
  }

/* Normal measure */

else
  {
  curmovt->barvector[measure_number_absolute] = measure_number << 16;
  next_measure_fraction = 1;
  yield = 1;
  }

/* Create the first item in the bar for each stave in the part, and increment
bar counts. MXL stave numbers start at 1. */

for (int n = 1; n <= scount; n++)
  {
  barstr *b = mem_get(sizeof(barstr));
  b->next = b->prev = NULL;
  b->type = b_start;
  b->repeatnumber = 0;

  st = curmovt->stavetable[prevstave + n];
  st->barindex[measure_number_absolute] = b;
  st->barcount++;

  last_item_cache[n] = b;
  }

if (st->barcount > curmovt->barcount) curmovt->barcount = st->barcount;



#ifdef NEVER
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
#endif





/* Scan all the top-level elements in the measure, in order. */

for (xml_item *mi = measure->next; mi != measure->partner; mi = mi->partner->next)
  {
  BOOL invisible = FALSE;
  lastwasbarline = FALSE;

  DEBUG(D_xmlstaves) eprintf("<%s>\n", mi->name);

  /* Many elements can have an optional <staff> element. If not present,
  assume staff 1, with pmwstaff being the PMW staff number. */

  staff = xml_get_number(mi, US"staff", 1, 500, 1, FALSE);
  pmwstaff = prevstave + staff;

  /* If the element has print-object="no", skip it, unless it also has
  print-spacing="yes", in which case set a flag instead. When skipping a note,
  rather than inserting an invisible rest immediately, adjust the
  pending_backup value so that [reset] and/or invisible rests won't be inserted
  unless actually needed. */

  if (ISATTR(mi, US"print-object", "yes", FALSE, "no"))
    {
    if (ISATTR(mi, US"print-spacing", "yes", FALSE, "yes")) invisible = TRUE;

    /* Skip this item */

    else
      {
      if (Ustrcmp(mi->name, "note") == 0)
        {
        int note_duration = xml_get_number(mi, US"duration", 0, 10000, 0, FALSE);
        if (note_duration > 0)
          {
          if (pending_backup[staff] < 0) pending_backup[staff] = duration[staff];
          pending_backup[staff] += note_duration;
          }
        }
      continue;
      }
    }


  /* ======================== Attributes ======================== */

  if (Ustrcmp(mi->name, "attributes") == 0)
    {
    for (xml_item *ai = mi->next; ai != mi->partner; ai = ai->partner->next)
      {
      BOOL noprint = ISATTR(ai, US"print-object", "yes", FALSE, "no");

      //const char *assume = noprint? "assume " : "";


      /* ==== Divisions attribute ==== */

      if (Ustrcmp(ai->name, "divisions") == 0)
        {
        divisions = xml_get_this_number(ai, 1, 1024*1024, 8, TRUE);
        }


      /* ==== Clef attribute ==== */

      else if (Ustrcmp(ai->name, "clef") == 0)
        {
        uschar *s;
        int n, o;
        b_clefstr *cl;

        /* Each clef has its own staff specified as "number". */

        staff = xml_get_attr_number(ai, US"number", 1, 10, 1, FALSE);
        pmwstaff = prevstave + staff;

        s = xml_get_string(ai, US"sign", US"", TRUE);
        o = xml_get_number(ai, US"clef-octave-change", -1, +1, 0, FALSE);
        n = xml_get_number(ai, US"line", 1, 5, -1, FALSE);

        if (Ustrcmp(s, "G") == 0)
          {
//          current_octave[staff] = 1;
          if (n >= 0 && n != 2) xml_Eerror(mi, ERR41, 'G', "2", n);
          switch (o)
            {
            case -1:
            current_clef[staff] = clef_trebletenor;
            break;

            case +1:
            current_clef[staff] = clef_trebledescant;
            break;

            default:
            current_clef[staff] = clef_treble;
            break;
            }
          }
        else if (Ustrcmp(s, "F") == 0)
          {
//          current_octave[staff] = 0;
          if (n == 5)
            {
            current_clef[staff] = clef_deepbass;
            }
          else
            {
            if (n >= 0 && n != 4) xml_Eerror(mi, ERR41, 'F', "4 or 5", n);
            switch(o)
              {
              case -1:
              current_clef[staff] = clef_contrabass;
              break;

              case +1:
              current_clef[staff] = clef_soprabass;
              break;

              default:
              current_clef[staff] = clef_bass;
              break;
              }
            }
          }
        else if (Ustrcmp(s, "C") == 0)
          {
//          current_octave[staff] = cclef_octaves[n];
          current_clef[staff] = cclefs[n];
          }
        else if (Ustrcmp(s, "percussion") == 0)
          {
          if (n >= 0) xml_Eerror(mi, ERR42, n);
//          current_octave[staff] = 1;
          current_clef[staff] = clef_hclef;
          }
        else
          {
          xml_Eerror(ai, ERR25, s);
          }

        clef_octave_change[staff] = -o;

        cl = xml_get_item(staff, sizeof(b_clefstr), b_clef);
        cl->clef = current_clef[staff];
        cl->assume = noprint;
        cl->suppress = FALSE;

#ifdef NEVER2
        if (starting_noprint)
          sprintf(save_clef[staff], "[%s %d]\n", clefname, current_octave[staff]);
        else
          stave_text(staff, "[%s %d]\n", clefname, current_octave[staff]);
#endif

        }

      /* ==== Key signature attribute ==== */

      else if (Ustrcmp(ai->name, "key") == 0)
        {
        uschar *s;
        int key;
        int sn;
        b_keystr *ks;
//        BOOL follows_repeat[PARTSTAFFMAX+1];

//        for (sn = 1; sn <= scount; sn++) follows_repeat[sn] = FALSE;

        /* A key signature may have its staff specifed as "number". If this is
        not present, the signature applies to all staves in the part. */

        staff = xml_get_attr_number(ai, US"number", 1, 10, -1, FALSE);


#ifdef NEVER
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
#endif



        /* Look for a conventional key signature */

        s = xml_get_string(ai, US"fifths", NULL, FALSE);

        /* If "fifths" is present, handle a traditional key signature. */

        if (s != NULL)
          {
          int offset = 7;
          BOOL wasbad;
          int n = xml_string_check_number(s, -7, 7, 0, &wasbad);

          if (wasbad) xml_Eerror(ai, ERR23, s);
          offset += n;
          s = xml_get_string(ai, US"mode", US"major", FALSE);
          if (Ustrcmp(s, "minor") == 0) offset += 15;
            else if (Ustrcmp(s, "major") != 0) xml_Eerror(ai, ERR22, s);
          key = keys[offset];
          }

        /* In the absence of "fifths", look for explicit definition of a
        non-traditional key signature. Save up the data and then call a
        function that writes the output, possibly on more than one staff. */

        else
          {
          xml_item *ki;
          uschar *key_step = NULL;
          uschar *key_accidental = NULL;

          uint8_t kslist[100];
          int ksn = 0;

          for (ki = ai->next; ki != ai->partner; ki = ki->partner->next)
            {
            if (Ustrcmp(ki->name, "key-step") == 0)
              key_step = xml_get_this_string(ki);
            else if (Ustrcmp(ki->name, "key-accidental") == 0)
              key_accidental = xml_get_this_string(ki);

            if (key_step != NULL && key_accidental != NULL)
              {
              int n;
              uint8_t ksval = key_step[0] - 'A' + 4;
              if (ksval > 9) ksval -= 7;

              for (n = 0; n < accidentals_count; n++)
                if (Ustrcmp(key_accidental, accidentals[n].name) == 0) break;
              if (n >= accidentals_count)
                xml_Eerror(ki, ERR52, "accidental", key_accidental);
              else ksval |= (accidentals[n].value) << 4;

              kslist[ksn++] = ksval;

              key_step = key_accidental = NULL;
              }
            }

          kslist[ksn] = ks_end;

          /* Search for matching standard key. If not found, search for a
          custom key and create one if not found. */

          key = find_key(kslist, 0, key_N);
          if (key < 0)
            {
            key = find_key(kslist, key_X, custom_key_count);
            if (key < 0)
              {
              key = key_X + custom_key_count++;
              memcpy(keysigtable[key], kslist, ksn + 1);
              }
            }
          }      /* End non-standard key signature */

        /* We now have a key signature number. Create a key signature item for
        the relevant staves. */

        for (sn = 1; sn <= scount; sn++)
          {
          if (staff >= 0 && staff != sn) continue;
          ks = xml_get_item(sn, sizeof(b_keystr), b_key);
          ks->assume = FALSE;
          ks->suppress = FALSE;
          ks->warn = TRUE;
          ks->key = key;
          }


        /* If this key signature followed the start of a repeat, we took out
        the repeat above. Now re-insert it after the key signature. */

#ifdef NEVER
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
#endif


        }        /* End key signature */


      /* ==== Time signature attribute ==== */

      else if (Ustrcmp(ai->name, "time") == 0)
        {
        uint32_t time;
        int time_num = xml_get_number(ai, US"beats", 1, 32, 4, TRUE);
        int time_den = xml_get_number(ai, US"beat-type", 1, 32, 4, TRUE);
        uschar *symbol = xml_get_attr_string(ai, US"symbol", US"", FALSE);

        staff = xml_get_attr_number(ai, US"number", 1, 10, -1, FALSE);
        measure_length = (time_num * divisions * 4)/time_den;

        if (Ustrcmp(symbol, "cut") == 0) time = time_cut;
        else if (Ustrcmp(symbol, "common") == 0) time = time_common;
        else
          {
          if (symbol[0] != 0) xml_Eerror(ai, ERR43, "time symbol", symbol);
          time = 0x00010000u | (time_num << 8) | time_den;
          }

        for (int n = 1; n <= scount; n++)
          {
          b_timestr *ts;
          if (staff > 0 && staff != n) continue;
          ts = xml_get_item(n, sizeof(b_timestr), b_time);
          ts->assume = FALSE;
          ts->suppress = !MFLAG(mf_showtime);
          ts->warn = MFLAG(mf_timewarn);
          ts->time = time;
          }



#ifdef NEVER
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
#endif


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
  note or something else that needs it. We look at where we are on each staff,
  and compute the backup value - where we want to be - from that. Ignore
  backups at the start of the measure. */

  else if (Ustrcmp(mi->name, "backup") == 0)
    {
    int backupby = xml_get_number(mi, US"duration", 0, 10000, 0, TRUE);
    for (int n = 1; n <= scount; n++)
      {
      int d = (pending_backup[n] >= 0)? pending_backup[n] : duration[n];
      if (d > 0)
        {
        int backupto = d - backupby;
        if (backupto < 0) backupto = 0;
        pending_backup[n] = backupto;
        }
      }
    }


  /* ======================== <barline> ======================== */

  /* Assume that this element is in the correct place in the bar. There doesn't
  seem to be a staff element within barline. */

  else if (Ustrcmp(mi->name, "barline") == 0)
    {
    uschar *location = xml_get_attr_string(mi, US"location", US"right", TRUE);
    xml_item *repeat = xml_find_item(mi, US"repeat");
    b_textstr *tx = NULL;

#ifdef NEVER
    uschar *s = NULL;
    xml_item *ending = xml_find_item(mi, US"ending");

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
#endif

    if (repeat != NULL)
      {
      uschar *direction = xml_get_attr_string(repeat, US"direction", NULL,
        TRUE);

      if (direction != NULL)
        {
        int type = 0;  /* Avoid compiler warning */
        if (Ustrcmp(location, "left") != 0 &&
            Ustrcmp(direction, "backward") == 0)
          type = b_rrepeat;
        else if (Ustrcmp(location, "right") != 0 &&
                 Ustrcmp(direction, "forward") == 0)
          type = b_lrepeat;
        else xml_Eerror(mi, ERR33, direction, location);
        for (int sn = 1; sn <= scount; sn++)
          (void)xml_get_item(sn, sizeof(bstr), type);
        }

      else xml_Eerror(mi, ERR32, "direction");
      }

    else   /* Not a repeat barline */
      {
      uschar *style = xml_get_string(mi, US"bar-style", NULL, FALSE);
      b_barlinestr *bl = xml_get_item(1, sizeof(b_barlinestr), b_barline);

      if (style != NULL)
        {
        for (int n = 0; n < barstyles_count; n++)
          {
          if (Ustrcmp(style, barstyles[n].name) == 0)
            {
            bl->bartype = barstyles[n].type;
            bl->barstyle = barstyles[n].style;
            if (barstyles[n].mfstring[0] != 0)
              (void)stave_text_pmw(0, (bstr *)bl, barstyles[n].mfstring,
                text_absolute, 0, 0, 0);
            lastwasbarline = TRUE;
            break;
            }
          }
        }

      else
        {
        bl->bartype = barline_normal;
        bl->barstyle = 0;
        }

      /* We've set up the barline on the first stave of the part. If there is
      more than one stave, copy the barline (with preceding text if present)
      onto the others. */

      for (int n = 2; n <= scount; n++)
        {
        b_barlinestr *bl2;
        if (tx != NULL) (void)xml_duplicate_item(n, tx, sizeof(b_textstr));
        bl2 = xml_get_item(n, sizeof(b_barlinestr), b_barline);
        bl2->bartype = bl->bartype;
        bl2->barstyle = bl->barstyle;
        }
      }
    }


  /* ======================== <direction> ======================== */

  else if (Ustrcmp(mi->name, "direction") == 0)
    {
    BOOL firsttext = TRUE;
    BOOL placement_above = ISATTR(mi, "placement", "above", FALSE, "above");
    BOOL directive = ISATTR(mi, "directive", "no", FALSE, "yes");
    int offset = xml_get_number(mi, US"offset", -1000, 1000, 0, FALSE);

    /* Each direction has a direction-type, which in turn contains various
    elements. */

    for (xml_item *dtg = xml_find_item(mi, US"direction-type");
         dtg != NULL;
         dtg = xml_find_next(mi, dtg))
      {
      for (xml_item *dt = dtg->next; dt != dtg->partner; dt = dt->partner->next)
        {
        uschar *halign = xml_get_attr_string(dt, US"halign", US"left", FALSE);
        xml_attrstr *dyattr = xml_find_attr(dt, US"default-y");
        BOOL dyset;
        int dy;
        int rx = xml_get_attr_number(dt, US"relative-x", -1000, 1000, 0, FALSE);
        int ry = xml_get_attr_number(dt, US"relative-y", -1000, 1000, 0, FALSE);
        int fsize = xml_get_attr_mils(dt, US"font-size", 1000, 100000, -1, FALSE);
        int fweight = ((ISATTR(dt, "font-style", "", FALSE, "italic"))? 1:0) +
           ((ISATTR(dt, "font-weight", "", FALSE, "bold"))? 2:0);

        if (dyattr != NULL)
          {
          BOOL wasbad = TRUE;
          dyset = TRUE;
          dy = xml_string_check_number(dyattr->value, -1000, 1000, 0, &wasbad);
          if (wasbad) xml_Eerror(dt, ERR23, dyattr->value);
          }
        else
          {
          dyset = FALSE;
          dy = 0;
          }

        /* Handle a textual direction */

        if (Ustrcmp(dt->name, "words") == 0)
          {
          uschar *s = xml_get_this_string(dt);
          uschar *enclosure = xml_get_attr_string(dt, US"enclosure", NULL,
            FALSE);
          b_textstr *tx = stave_text_utf(staff, US"", s, US"",
            fonttypes[fweight], 0);

          tx->size = (fsize >= 0)? xml_pmw_fontsize(fsize) :
            (xml_fontsize_word_default >= 0)? xml_fontsize_word_default : 0;

          if (firsttext)
            {
            handle_text_dy(tx, dy, placement_above);
            tx->y += ry*400;
            if (Ustrcmp(halign, "center") == 0) tx->flags |= text_centre;
            else if (Ustrcmp(halign, "right") == 0) tx->flags |= text_endalign;
            else if (Ustrcmp(halign, "left") != 0)
              xml_add_attrval_to_tree(&xml_unrecognized_element_tree, dt,
                xml_find_attr(dt, US"halign"));
            }

          if (enclosure != NULL)
            {
            if (Ustrcmp(enclosure, "oval") == 0 ||
                Ustrcmp(enclosure, "circle") == 0)
              tx->flags |= text_ringed;
            else if (Ustrcmp(enclosure, "rectangle") == 0 ||
                     Ustrcmp(enclosure, "square") == 0)
              tx->flags |= text_boxed;
            else if (Ustrcmp(enclosure, "none") != 0)
              xml_Eerror(dt, ERR43, US"<words> enclosure", enclosure);
            }

          /* The "offset" value is in divisions, positioning the item in terms
          of musical position. This is overridden by explicit horizontal
          positioning. The "directive" attribute, if "yes" specifies that the
          item is positioned according to "tempo rules", which in essence means
          aligned with any time signature. However, any other horizontal
          positioning setting overrides. If not the first text, only relative
          movement is relevant. */

          if (rx != 0) tx->x += rx * 400; /* Explicit relative-x is specified */
          else if (firsttext)
            {
            if (offset != 0) tx->offset += (offset*1000)/divisions;
            else if (directive) tx->flags |= text_timealign;
            }

          /* Use the follow-on feature if not the first text */

          if (!firsttext) tx->flags |= text_followon;
          firsttext = FALSE;
          }


        /* Handle a metronome mark */

        else if (Ustrcmp(dt->name, "metronome") == 0)
          {
          int n;
          uschar *beat_unit = xml_get_string(dt, US"beat-unit", US"quarter",
            TRUE);
          uschar *per_minute = xml_get_string(dt, US"per-minute", US"?", TRUE);
          BOOL parens = ISATTR(dt, "parentheses", "no", FALSE, "yes");

          for (n = 0; n < note_types_count; n++)
            if (Ustrcmp(beat_unit, note_types[n].name) == 0) break;
          if (n >= note_types_count) xml_Eerror(dt, ERR18, beat_unit);  /* Hard */

          if (note_types[n].pmwchar == 0)
            xml_Eerror(dt, ERR11, beat_unit);
          else
            {
            b_textstr *tx;
            uint32_t *mm = mem_get(12 * sizeof(uint32_t));
            uint32_t *pt = mm;
            if (parens) *pt++ = RM|'(';
            *pt++ = MF |(font_small << 24)|note_types[n].pmwchar;
            *pt++ = RM|' ';
            *pt++ = RM|'=';
            *pt++ = RM|' ';
            while (*per_minute != 0) *pt++ = RM | *per_minute++;
            if (parens) *pt++ = RM|')';
            *pt = 0;
            tx = stave_text_pmw(staff, NULL, mm, text_above, 0, 0, 0);
            tx->size = (fsize >= 0)? xml_pmw_fontsize(fsize) :
              (xml_fontsize_word_default >= 0)? xml_fontsize_word_default : 0;

            /* Use the follow-on feature if not the first text */

            if (!firsttext) tx->flags |= text_followon;
            firsttext = FALSE;
            }


// Untested, need an example
#ifdef NEVER2
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif


          }


        /* Handle a dynamic */

        else if (Ustrcmp(dt->name, "dynamics") == 0)
          {
          xml_item *which;

          for (which = dt->next; which != dt->partner;
               which = which->partner->next)
            {
            int n;
            for (n = 0; n < dynamics_count; n++)
              if (Ustrcmp(which->name, dynamics[n].name) == 0) break;

            if (n >= dynamics_count) xml_Eerror(dt, 35, which->name); else
              {
              b_textstr *tx = stave_text_pmw(staff, NULL, dynamics[n].string,
                0, rx * 400, 0, (offset*1000)/divisions);
              handle_text_dy(tx, dy, placement_above);


// Untested, need an example
#ifdef NEVER2
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif

              if (Ustrcmp(halign, "center") == 0) tx->flags |= text_centre;
              else if (Ustrcmp(halign, "right") == 0) tx->flags |= text_endalign;
              else if (Ustrcmp(halign, "left") != 0)
                xml_add_attrval_to_tree(&xml_unrecognized_element_tree, dt,
                  xml_find_attr(dt, US"halign"));
              }
            }
          }


        /* Handle a rehearsal mark. PMW does not support different font sizes,
        styles, or different enclosures for rehearsal marks. We check that they
        are all the same. */

        else if (Ustrcmp(dt->name, "rehearsal") == 0)
          {

#ifdef NEVER
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

#ifdef NEVER2
          if (ry != 0)  /* relative-y is specified */
            stave_text(pstaff, "/%c%s", (ry > 0)? 'u':'d',
              format_fixed(abs(ry*400)));
#endif


          stave_text(pstaff, "]");

#endif
          }


        /* Handle a wedge (aka hairpin). MusicXML puts the "spread" setting on
        the open end, whereas PMW puts its /w option on the opening character.
        So in the case of a crescendo we have to do an insert at the starting
        item. */

        else if (Ustrcmp(dt->name, "wedge") == 0)
          {
          b_hairpinstr *hp;
          uschar *wtype = xml_get_attr_string(dt, US"type", US"unset", FALSE);
          int32_t spread = xml_get_attr_number(dt, US"spread", 1, 100, -1, FALSE);

          do_pending_backup(staff, dt);
          hp = xml_get_item(staff, sizeof(b_hairpinstr), b_hairpin);
          hp->flags = 0;
          hp->x = rx * 400;
          hp->y = ry * 400;
          hp->halfway = 0;
          hp->offset = (offset*1000)/divisions;
          hp->su = 0;
          hp->width = 0;

          /* Terminate a hairpin */

          if (Ustrcmp(wtype, "stop") == 0)
            {
            b_hairpinstr *ohp = open_wedge[staff];
            if (ohp != NULL)
              {
              hp->flags |= hp_end | (ohp->flags & hp_cresc);
              if (spread > 0) ohp->width = spread * 400;

              if (dyset)
                {
                hp->y += dy * 400 - ohp->y;
                }

              open_wedge[staff] = NULL;

              }
            else xml_Eerror(dt, ERR48);
            }

          /* Start a hairpin */

          else if (open_wedge[staff] == NULL)
            {
            if (Ustrcmp(wtype, "crescendo") == 0) hp->flags |= hp_cresc;
              else if (Ustrcmp(wtype, "diminuendo") != 0)
                xml_Eerror(dt, ERR43, US"wedge type", wtype);
            if (spread > 0) hp->width = spread * 400;   /* Diminuendo */
            open_wedge[staff] = hp;

            if (dyset)
              {
              hp->flags |= hp_abs;
              hp->y += dy * 400;
              }
            else
              {
              if (!placement_above) hp->flags |= hp_below;
              }
            }

          /* Nested hairpins not supported */

          else xml_Eerror(dt, ERR49);
          }


        /* Handle pedal marks */

        else if (Ustrcmp(dt->name, "pedal") == 0)
          {
#ifdef NEVER

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
                    sml_add_attrval_to_tree(&unrecognized_element_tree, dt,
                      xml_find_attr(dt, US"halign"));

// Untested, need an example
#ifdef NEVER2
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
#endif

          }


        /* Handle <bracket>, which in PMW is [line] and is very much like
        [slur]. */

        if (Ustrcmp(dt->name, "bracket") == 0)
          {
#ifdef NEVER

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
#ifdef NEVER2
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
#endif

          }


        /* Ignoring unrecognized; should automatically be listed. */


        }  /* End of loop scanning <direction-type> elements */
      }    /* End of loop scanning <direction> for <direction-type> */
    }


  /* ======================== <forward> ======================== */

  /* Unlike <backup>, <forward> may have a <staff> element, but if it doesn't,
  it must apply to all staves. It is also necessary to handle <forward> when a
  <backup> is still pending. Note that the pending_backup values are where to
  back up to, not the amount to backup. A negative value means "no backup
  pending". */

  else if (Ustrcmp(mi->name, "forward") == 0)
    {
    int forward_duration = xml_get_number(mi, US"duration", 0, 10000, -1, TRUE);
    if (forward_duration > 0)
      {
      staff = xml_get_attr_number(mi, US"staff", 1, 10, -1, FALSE);
      for (int n = 1; n <= scount; n++)
        {
        if (staff > 0 && n != staff) continue;
        pending_backup[n] = forward_duration +
          ((pending_backup[n] >= 0)? pending_backup[n] : duration[n]);
        }
      }
    }


  /* ======================== Notes and rests ======================== */

  else if (Ustrcmp(mi->name, "note") == 0)
    {
    xml_item *dot, *grace, *lyric, *notations, *pitch, *slur, *type;
    xml_item *rest = NULL;
    xml_item *unpitched = NULL;

    int dots = 0;
    int octave;
    uint32_t note_duration;

    uschar *note_type_name, *note_size, *step;
    b_notestr *newnote;
    bstr *insertpoint;

    BOOL whole_bar_rest = FALSE;


#ifdef NEVER
    const char *rest_wholebar;
    int thisnotehead;
    int tremlines;
    int slurstop[10];  /* Should be overkill */
    char options[32];

    options[0] = '\\';
    options[1] = 0;
    tied = TIED_NONE;
    tremolo = NULL;
    tremlines = 2;
#endif

    /* Deal with any pending <backup> to adjust the musical position on this
    staff. This can be a reverse or forward move. */

    do_pending_backup(staff, mi);

    /* Handle lyrics. Not all <lyric> elements have <text> because some just
    have <extend>. */

    for (lyric = xml_find_item(mi, US"lyric");
         lyric != NULL;
         lyric = xml_find_next(mi, lyric))
      {
      uschar *t = xml_get_string(lyric, US"text", NULL, FALSE);
      uschar *j = xml_get_attr_string(lyric, US"justify", US"", FALSE);
      xml_item *extend = xml_find_item(lyric, US"extend");

      if (t != NULL)
        {
        b_textstr *tx;
        uschar s_end[8];
        uschar *s_start;
        uschar *s = xml_get_string(lyric, US"syllabic", US"single", FALSE);
        int rx = xml_get_attr_number(lyric, US"relative-x", -100, 100, 0,
          FALSE);

        s_start = (Ustrcmp(j, "left") == 0)? US"^" : US"";
        s_end[0] = 0;
        if (Ustrcmp(j, "right") == 0) (void)Ustrcat(s_end, "^^");
        if (Ustrcmp(s, "begin") == 0 || Ustrcmp(s, "middle") == 0)
          (void)Ustrcat(s_end, "-");

        else if (extend != NULL)
          {
          xml_attrstr *atype = xml_find_attr(extend, US"type");
          if (atype == NULL || Ustrcmp(atype->value, "start") == 0)
            (void)Ustrcat(s_end, "=");
          }

        tx = stave_text_utf(staff, s_start, t, s_end, font_rm, text_ul);
        if (rx != 0) tx->x = rx*400;
        }

      else if (extend != NULL && ISATTR(extend, "type", "", FALSE, "stop"))
        {
        (void)stave_text_utf(staff, US"", US"=", US"", font_rm, text_ul);
        }
      }

    /* Now deal with the note. */

    dot = xml_find_item(mi, US"dot");
    pitch = xml_find_item(mi, US"pitch");
    grace = xml_find_item(mi, US"grace");
    unpitched = rest = NULL;
    notations = xml_find_item(mi, US"notations");
    type = xml_find_item(mi, US"type");

    newnote = xml_get_item(staff, sizeof(b_notestr), b_note);
    insertpoint = (bstr *)newnote;

    /* Defaults pro tem - may be able to tidy later. */

    newnote->notetype = crotchet;
    newnote->masq = MASQ_UNSET;
    newnote->acc = 0;
    newnote->accleft = 0;
    newnote->acflags = 0;
    newnote->flags = 0;
    newnote->length = len_crotchet;
    newnote->yextra = 0;
    newnote->abspitch = 0;
    newnote->spitch = 0;
    newnote->acc_orig = 0;
    newnote->char_orig = 'Z';

    /* The duration value is documented as intended for a performance vs
    notated value, but hopefully <backup> values, which are also durations,
    will make sense with it. If not, we might have to get a "notated duration"
    value. The same applies to whole bar rests. No duration is supplied for
    grace notes. */

    note_duration = xml_get_number(mi, US"duration", 0, 10000, 0,
      grace == NULL);

    /* A note must have exactly one of <pitch>, <unpitched>, or <rest>, but we
    assume we are dealing with valid XML, so uniqueness is not enforced.
    Whichever comes first is used. */

    octave = clef_octave_change[staff];
    if (pitch != NULL)
      {
      int stepletter;
      step = xml_get_string(pitch, US"step", NULL, FALSE);
      if (step == NULL) xml_Eerror(pitch, ERR17, "<step>");  /* Hard */
      newnote->char_orig = stepletter = step[0];
      if (Ustrchr("ABCDEFG", stepletter) == NULL || stepletter == 0)
        xml_Eerror(pitch, ERR18, step);  /* Hard */

      /* Figure out the absolute pitch in PMW terms, where middle C is 96. MXL
      octaves have octave 4 starting at middle C, conveniently. Then compute
      the stave- relative pitch, where 256 is the bottom line, using the same
      tables that are used for PMW input files. */

      octave += xml_get_number(pitch, US"octave", 0, 12, 4, TRUE);
      newnote->abspitch = octave * 24 + read_basicpitch[stepletter - 'A'];
      newnote->spitch = pitch_stave[newnote->abspitch] +
        pitch_clef[current_clef[staff]];
      }

    /* Sometimes a whole bar rest is notated as a whole note with a duration of
    a complete measure. */

    else
      {
      unpitched = xml_find_item(mi, US"unpitched");
      if (unpitched == NULL)
        {
        rest = xml_find_item(mi, US"rest");
        if (rest == NULL)
          xml_Eerror(mi, ERR17, "<pitch>, <unpitched>, or <rest>");  /* Hard */
        whole_bar_rest = ISATTR(rest, "measure", "", FALSE, "yes") ||
          note_duration == measure_length;
        }
      else
        {
        step = xml_get_string(unpitched, US"display-step", NULL, FALSE);
        if (step == NULL) xml_Eerror(unpitched, ERR17, "<display-step>");  /* Hard */
        octave += xml_get_number(unpitched, US"display-octave", 0, 12, 4, TRUE);
        }
      }




#ifdef NEVER
    if (grace != NULL)
      {
      conditional_space(options);
      (void)Ustrcat(options, "g");
      if (ISATTR(grace, "slash", "", FALSE, "yes")) (void)Ustrcat(options, "/");
      }
#endif


#ifdef NEVER
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
#endif


    /* We keep a table of the current musical position with any default-x
    value, so that after <backup> we can try to separate notes that might
    clash. This is not useful in chords, nor for whole bar rests or grace
    notes. */

    if (!inchord[staff] && !whole_bar_rest && grace == NULL)
      {
      int n;
      int note_default_x = xml_get_attr_number(mi, US"default-x", -10000, 10000,
        INT_MAX, FALSE);

      for (n = 0; n < note_positions_next; n++)
        {
        if (duration[staff] < note_positions[n].moff)  /* Need to insert */
          {
          memmove((char *)(note_positions + n + 1), (char *)(note_positions + n),
            (note_positions_next - n)*sizeof(note_position));
          break;
          }

        /* We have already been at this position. If default-x exists for this
        note but the table contains "unset" (i.e. INT_MAX), update the table
        with this note's value. Otherwise create an adjustment if necessary. */

        if (duration[staff] == note_positions[n].moff)
          {
          if (note_default_x != INT_MAX)
            {
            if (note_positions[n].x == INT_MAX)
              {
              note_positions[n].x = note_default_x;
              }
            else
              {
              int32_t adjustx = note_default_x - note_positions[n].x;

              if (adjustx != 0)
                {
                b_movestr *mv;

                /* If a negative move is at the start of a bar (musical offset
                zero), insert some space. */

                if (adjustx < 0 && duration[staff] == 0)
                  {
                  b_spacestr *sp = mem_get_insert_item(sizeof(b_spacestr),
                    b_space, (bstr *)newnote);
                  sp->relative = FALSE;
                  sp->x = -(adjustx*400)/2;
                  }

                /* Insert the [move] adjustment. */

                mv = mem_get_insert_item(sizeof(b_movestr), b_move,
                  (bstr *)newnote);
                mv->relative = FALSE;
                mv->x = adjustx * 400;
                mv->y = 0;
                }
              }
            }

          n = INT_MAX;   /* No new insert needed */
          break;
          } /* End handling matching position */
        }   /* End of table scan */

      /* Need to insert a new item in the table. If this is in the middle of
      the table, space was made above by moving everthing else up. */

      if (n <= note_positions_next)
        {
        note_positions[n].moff = duration[staff];
        note_positions[n].x = note_default_x;
        note_positions_next++;
        }
      }   /* End of table position handling */

    /* Can now update to the next duration position. */

    if (note_duration > 0 && inchord[staff] == NULL)
      duration[staff] += note_duration;

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

    dots = 0;
    while (dot != NULL)
      {
      dots++;
      dot = xml_find_next(mi, dot);
      }

    /* Handle cue notes; for chords <cue> is only on the first note. */

    if (inchord[staff] == NULL &&
        (xml_find_item(mi, US"cue") != NULL || Ustrcmp(note_size, "cue") == 0))
      newnote->flags |= nf_cuesize;

    /* Handle notations. */

//    slurstop[0] = -1;

    if (notations != NULL)
      {
      xml_item *fermata, *ni, *ornaments, *tremolo, *tuplet;

      /* Slurs and lines are very similar in PMW, but rather than trying to
      combine the code, we do them separately. */

      for (slur = xml_find_item(notations, US"slur");
           slur != NULL;
           slur = xml_find_next(notations, slur))
        {
        uschar *slurtype = xml_get_attr_string(slur, US"type", US"", TRUE);
        uschar *line_type = xml_get_attr_string(slur, US"line-type", NULL,
          FALSE);

//        int default_x = xml_get_attr_number(slur, US"default-x", -1000, 1000,
//          0, FALSE);
//        int default_y = xml_get_attr_number(slur, US"default-y", -1000, 1000,
//          0, FALSE);
//        int bezier_x = xml_get_attr_number(slur, US"bezier-x", -1000, 1000,
//          0, FALSE);
//        int bezier_y = xml_get_attr_number(slur, US"bezier-y", -1000, 1000,
//          0, FALSE);

        int sn = xml_get_attr_number(slur, US"number", 1, 16, 0, FALSE);

        if (Ustrcmp(slurtype, "start") == 0)
          {
          b_slurstr *sl = mem_get_insert_item(sizeof(b_slurstr), b_slur,
            (bstr *)newnote);

          sl->id = slurchars[sn];
          sl->mods = NULL;
          sl->ally = 0;
          sl->flags = (Ustrcmp(xml_get_attr_string(slur, US"placement", US"",
            FALSE), "above") == 0)? 0 : sflag_b;

          if (line_type != NULL)
            {
            if (Ustrcmp(line_type, "dashed") == 0)
              sl->flags |= sflag_i;
            else if (Ustrcmp(line_type, "dotted") == 0)
              sl->flags |= sflag_idot;
            else if (Ustrcmp(line_type, "solid") != 0)
              xml_Eerror(slur, ERR43, "slur line type", line_type);
            }




#ifdef NEVER
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
#endif


          /* FIXME: need much more investigation of what to do here. Perhaps
          implement an overall option for modifying slurs: default off. The
          default-y thing is tricky. */

#ifdef NEVER
          leftright(pstaff, default_x, "l");
          updown(pstaff, default_y, "l");   /* This is especially bad */

          leftright(pstaff, bezier_x, "cl");
          updown(pstaff, bezier_y, "cl");
#endif


#ifdef NEVER
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
#endif

          }


        /* End of slur */

        else if (Ustrcmp(slurtype, "stop") == 0)
          {
          b_endslurstr *be = mem_get(sizeof(b_endslurstr));
          be->type = b_endslur;
          be->value = slurchars[sn];
          add_pending_post_chord(staff, (bstr *)be);
          slurchars[sn] ^= 0x20u;

#ifdef NEVER
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
#endif


#ifdef NEVER
            leftright_insert(pstaff, offset, default_x, "r");
            updown_insert(pstaff, offset, default_y, "r");

            leftright_insert(pstaff, offset, bezier_x, "cr");
            updown_insert(pstaff, offset, bezier_y, "cr");


            ss->next = slur_free_starts;
            slur_free_starts = ss;
            }
#endif

          }
        }




      /* Tuplets. There doesn't seem to be a separate font description for
      them, so default to word font size and italic, but enforce a minimum. */

      tuplet = xml_find_item(notations, US"tuplet");
      if (tuplet != NULL)
        {
        if (!tuplet_size_set)
          {
          if (xml_fontsize_word_default > 0)
            {
            int32_t fsize = xml_fontsizes[xml_fontsize_word_default];
            if (fsize < 8000) fsize = 8000;
            curmovt->fontsizes->fontsize_triplet.size = fsize;
            curmovt->fonttype_triplet = font_it;
            }
          tuplet_size_set = TRUE;
          }

        /* Start of tuplet */

        if (ISATTR(tuplet, "type", "", FALSE, "start"))
          {
          int actual, normal;
          b_pletstr *plet = mem_get_insert_item(sizeof(b_pletstr), b_plet,
            (bstr *)newnote);
          uschar *show_number = xml_get_attr_string(tuplet, US"show-number",
            US"actual", FALSE);
          uschar *placement = xml_get_attr_string(tuplet, US"placement",
            US"", FALSE);
          xml_item *time_modification = xml_find_item(mi, US"time-modification");

          if (time_modification == NULL)
            {
            actual = 3;
            normal = 2;
            }
          else
            {
            actual = xml_get_number(time_modification, US"actual-notes", 1,
              100, 3, TRUE);
            normal = xml_get_number(time_modification, US"normal-notes", 1,
              100, 2, TRUE);
            }

          plet->pletlen = actual;
          plet->flags = 0;
          plet->x = 0;
          plet->yleft = 0;
          plet->yright = 0;

          tuplet_num[staff] = normal;
          tuplet_den[staff] = actual;

          if (Ustrcmp(show_number, "none") == 0) plet->flags |= plet_x;
            else if (Ustrcmp(show_number, "actual") != 0)
              xml_Eerror(tuplet, ERR43, US"tuplet show number", show_number);

          if (Ustrcmp(placement, "below") == 0) plet->flags |= plet_b;
            else if (Ustrcmp(placement, "above") == 0) plet->flags |= plet_a;

          if (ISATTR(tuplet, "bracket", "no", FALSE, "no"))
            plet->flags |= plet_bn;
          }

        /* If not start, assume stop. We have to set up the end item to be
        added after the end of the note or chord. */

        else
          {
          bstr *be = mem_get(sizeof(bstr));
          be->type = b_endplet;
          add_pending_post_chord(staff, (bstr *)be);
          end_tuplet[staff] = TRUE;
          }
        }


      /* There can be multiple <tied> items on a note, typically stop,start.
      Treat "continue" as "start" for the moment. We only need one tie item for
      a chord, so do nothing if the pending post-chord list already starts with
      a tie - ties are always added at the start, others at the end. */

      if (pending_post_chord[staff] == NULL ||
          pending_post_chord[staff]->type != b_tie)
        {
        for (ni = xml_find_item(notations, US"tied");
             ni != NULL;
             ni = xml_find_next(notations, ni))
          {
          xml_attrstr *t = xml_find_attr(ni, US"type");
          if (t != NULL &&
              (Ustrcmp(t->value, "start") == 0 ||
               Ustrcmp(t->value, "continue") == 0))
            {
            b_tiestr *ts = mem_get(sizeof(b_tiestr));
            uschar *orientation = xml_get_attr_string(ni, US"orientation", US"",
              FALSE);

            ts->type = b_tie;
            ts->flags = tief_default;
            ts->abovecount = ts->belowcount = 0;
            if (Ustrcmp(orientation, "over") == 0) ts->abovecount = 255;
            if (Ustrcmp(orientation, "under") == 0) ts->belowcount = 255;

            ts->noteprev = newnote;

            /* Add to the FRONT of the post-chord list */

            ts->prev = NULL;
            ts->next = pending_post_chord[staff];
            if (ts->next != NULL) ts->next->prev = (bstr *)ts;
            pending_post_chord[staff] = (bstr *)ts;
            }
          }
        }


      /* Fermata */

#ifdef NEVER
      fermata = xml_find_item(notations, US"fermata");
      if (fermata != NULL)
        {
        (void)Ustrcat(options, "f");



        if (ISATTR(fermata, "type", "upright", FALSE, "inverted"))
          (void)Ustrcat(options, "!");
        }

      /* Arpeggio: take note only if first note of a chord. The inchord[]
      setting happens later. */

      if (xml_find_item(notations, US"arpeggiate") != NULL &&
          inchord[staff] == NULL)
        {
        conditional_space(options);
        (void)Ustrcat(options, "ar");
        }
#endif


      /* Tremolo. */

      tremolo = xml_find_item(notations, US"tremolo");
      if (tremolo != NULL)
        {
        int tremlines = xml_get_this_number(tremolo, 1, 3, 2, FALSE);
        uschar *tremtype = xml_get_attr_string(tremolo, US"type", US"single",
          FALSE);

        if (Ustrcmp(tremtype, "single") == 0)
          insertpoint = insert_ornament(insertpoint,
            (tremlines == 1)? or_trem1 : (tremlines == 2)? or_trem2 : or_trem3);

        /* This is a tremolo between notes. Output a tremolo item after this
        note. */

        else if (Ustrcmp(tremtype, "start") == 0)
          {
          b_tremolostr *ts = xml_get_item(staff, sizeof(b_tremolostr),
            b_tremolo);
          ts->count = tremlines;
          ts->join = 0;
          }

        else if (Ustrcmp(tremtype, "stop") == 0)
          {
          /* Perhaps should check for previous note start? */
          }

        else xml_Eerror(tremolo, ERR43, "tremolo type", tremtype);
        }

      /* Ornaments */

      ornaments = xml_find_item(notations, US"ornaments");
      if (ornaments != NULL)
        {
        xml_item *orn;
        const accdef *accabove = NULL;
        const accdef *accbelow = NULL;
        BOOL delayed_turn = FALSE;

        for (orn = xml_find_item(ornaments, US"accidental-mark");
             orn != NULL;
             orn = xml_find_next(ornaments, orn))
          {
          int n;
          uschar *accname = xml_get_this_string(orn);
          const accdef **which =
            (ISATTR(orn, "placement", "below", FALSE, "below"))?
              &accbelow : &accabove;

          for (n = 0; n < accidentals_count; n++)
            if (Ustrcmp(accname, accidentals[n].name) == 0) break;

          if (n >= accidentals_count)
            xml_Eerror(orn, ERR52, "accidental", accname);
          else *which = accidentals + n;
          }

        orn = xml_find_item(ornaments, US"trill-mark");
        if (orn != NULL)
          {
          int orval = or_tr;
          if (accbelow != NULL) xml_Eerror(orn, ERR43,
            "trill with accidental placement", "below");  /* Not supported */
          if (accabove != NULL)
            {
            if (accabove->trillvalue != or_unset)
              orval = accabove->trillvalue;
            else xml_Eerror(orn, ERR43, "double or quarter accidental on", "trill");
            }
          insertpoint = insert_ornament(insertpoint, orval);
          }

        /* Use text for turns */

        orn = xml_find_item(ornaments, US"turn");
        if (orn == NULL)
          {
          delayed_turn = TRUE;
          orn = xml_find_item(ornaments, US"delayed-turn");
          }

        if (orn != NULL)
          {
          uint32_t *ps;
          int typechar = (ISATTR(orn, "slash", "no", FALSE, "yes"))? 'i':'S';
          BOOL below = ISATTR(orn, "placement", "above", FALSE, "below");

          /* No accidentals: use a fixed string. */

          if (accabove == NULL && accbelow == NULL)
            {
            ps = (typechar == 'S')? plainturn : plainiturn;
            }

          /* Otherwise build a custom text string. */

          else
            {
            uint32_t *pp = ps = mem_get(20 * sizeof(uint32_t));
            const char *t = (accabove == NULL)? turnB :
                            (accbelow == NULL)? turnA : turnC;

            for (; *t != 0; t++)
              {
              uint32_t x = 0;
              if (*t == 'T')
                {
                *pp++ = MF|typechar;
                continue;
                }

              if (*t == 'A') x = accabove->accchars;
              else if (*t == 'B') x = accbelow->accchars;

              if (x == 0)
                {
                *pp++ = MF|*t;
                continue;
                }

              else while (x != 0)
                {
                *pp++ = MF|(font_small << 24)|(x & 0xff);
                x >>= 8;
                }
              }
            *pp = 0;
            }

          (void)stave_text_pmw(0, (bstr *)newnote, ps, below? 0 : text_above,
            0, delayed_turn? 500 : 0, 0);
          }
        }  /* End ornaments */
      }

    /* Handle non-rests; either pitch or unpitched is not NULL. Note that an
    invisible note has been turned into a rest. */

    if (rest == NULL)
      {
      int i;
      int stem_up = 1;
      int thisnotehead = nh_normal;  /* Default */
      uschar *notehead = xml_get_string(mi, US"notehead", NULL, FALSE);
      uschar *stemtype = NULL;
      xml_item *accidental = xml_find_item(mi, US"accidental");
      xml_item *stem = xml_find_item(mi, US"stem");

      if (suspended[staff])
        {
        (void)mem_get_insert_item(sizeof(bstr), b_resume, (bstr *)newnote);
        suspended[staff] = FALSE;
        }

      if (stem != NULL)
        {
        xml_attrstr *default_y = xml_find_attr(stem, US"default-y");
        int ry = xml_get_attr_number(stem, US"relative-y", -100, 100, 0, FALSE);

        stemtype = xml_get_this_string(stem);
        if (Ustrcmp(stemtype, "none") != 0)
          {
          stem_up = (Ustrcmp(stemtype, "up") == 0)? +1 : -1;
          newnote->flags |= nf_stem;

          /* If a stem has a default-y setting, it specifies where the end of
          the stem should be, in tenths, relative to the top of the stave. We
          calculate where the default stem would end (one octave, or the middle
          line for low/high notes) and then compute a stem lengthening or
          shortening.

          However, this leads to bad results for chords, and it also can
          produce very long stems in plain beams. The real use is for getting
          overprinted notes aligned, so we skip it for chords and for beams
          unless the bar contains <backup>. */

          if (default_y != NULL && inchord[staff] == NULL &&
              (contains_backup || xml_find_item(mi, US"beam") == NULL))
            {
            BOOL bad;
            int sizefactor = ((newnote->flags & nf_cuesize) != 0)?
              curmovt->fontsizes->fontsize_cue.size/10 :
              (newnote->length == 0)?
              curmovt->fontsizes->fontsize_grace.size/10 : 1000;
            int stemend = ((stem_up > 0 && newnote->spitch < 244) ||
                           (stem_up < 0 && newnote->spitch > 300))?
              (P_3L - P_1L) :
              (newnote->spitch + (28*stem_up*sizefactor)/1000 - P_1L);
            int32_t dy = xml_string_check_number(default_y->value, -1000,
              1000, 0, &bad);
            if (bad) xml_Eerror(stem, ERR23, default_y->value);
            dy = dy * 400 + 16000;  /* Distance wanted above base of stave */
            newnote->yextra = stem_up * (dy - stemend * 500);
            }

          /* A relative value adds to the default. For the moment, always
          accept this. */

          newnote->yextra += stem_up * ry*400;
          }
        }

      /* Stem not specified */

      else
        {
        newnote->flags |= nf_stem;
        stem_up = (newnote->spitch < P_3L)? +1 : -1;
        }

      if (stem_up > 0) newnote->flags |= nf_stemup;

      /* Handle an accidental */

      if (accidental != NULL)
        {
        int n;
        uschar *astring = xml_get_this_string(accidental);
        for (n = 0; n < accidentals_count; n++)
          {
          if (Ustrcmp(astring, accidentals[n].name) == 0)
            {
            newnote->acc = accidentals[n].value;
            newnote->abspitch += read_accpitch[newnote->acc]; 
            newnote->accleft += curmovt->accspacing[newnote->acc] -
              curmovt->accadjusts[newnote->notetype];
            break;
            }
          }
        if (n >= accidentals_count) xml_Eerror(mi, ERR30, astring);
        if (ISATTR(accidental, "parentheses", "", FALSE, "yes"))
          newnote->flags |= nf_accrbra;
        }

      /* Handle noteheads. */

      if (notehead != NULL)
        {
        if (Ustrcmp(notehead, "diamond") == 0) thisnotehead = nh_harmonic;
        else if (Ustrcmp(notehead, "normal") == 0) thisnotehead = nh_normal;
        else if (Ustrcmp(notehead, "none") == 0) thisnotehead = nh_none;
        else if (Ustrcmp(notehead, "x") == 0) thisnotehead = nh_cross;
        else xml_Eerror(mi, ERR43, "notehead type", notehead);
        }

      if (thisnotehead != set_noteheads[staff])
        {
        if (inchord[staff] != NULL)
          {
          if (thisnotehead == nh_harmonic || thisnotehead == nh_cross)
            {
            newnote->flags |= (thisnotehead == nh_harmonic)?
              nf_nhharmonic : nf_nhcross;
            }
          else xml_Eerror(mi, ERR45, notehead);
          }
        else
          {
          b_noteheadsstr *bn = mem_get_insert_item(sizeof(b_noteheadsstr),
            b_noteheads, (bstr *)newnote);
          bn->value = thisnotehead;
          set_noteheads[staff] = thisnotehead;
          }
        }

      /* Sort out type and length of note. */

      note_type_and_length(newnote, note_type_name, dots, tuplet_num[staff],
        tuplet_den[staff], mi, type);

      /* Handle chords. The analysis phase inserts start/end chord items into
      the chain, because all MusicXML does is to flag the 2nd and subsequent
      notes of a chord. */

      if (xml_find_item(mi, US"pmw-chord-first") != NULL)
        {
        if (inchord[staff] != NULL)
          xml_Eerror(mi, ERR28, "chord start within chord");

        newnote->flags |= nf_chord;
        inchord[staff] = newnote;
        }

      /* Handle subsequent notes in a chord. */

      else if (inchord[staff] != NULL)
        {
        newnote->type = b_chord;
        newnote->flags |= nf_chord;
        }

      /* When we reach the final note in a chord, we must run the sortting
      function, which also handles positioning of accidentals and notes on the
      wrong side of the stem. This function may adjust the "lastitem" value. */

      if (xml_find_item(mi, US"pmw-chord-last") != NULL)
        {
        if (inchord[staff] == NULL)
          xml_Eerror(mi, ERR28, "chord end not within chord");
        read_sortchord(inchord[staff], newnote->flags & nf_stemup);
        last_item_cache[staff] = read_lastitem;
        inchord[staff] = NULL;
        }


      /* Handle notations. */

      if (notations != NULL)
        {
        xml_item *ni;
        xml_item *articulations = xml_find_item(notations, US"articulations");
        xml_item *technical = xml_find_item(notations, US"technical");

        /* Articulations */

        if (articulations != NULL)
          {
          for (ni = articulations->next;
               ni != articulations->partner;
               ni = ni->partner->next)
            {
            xml_attrstr *placement = xml_find_attr(ni, US"placement");
            uschar *atype = xml_get_attr_string(ni, US"type", NULL, FALSE);
            BOOL placement_known = placement != NULL;
            BOOL placement_above = placement_known &&
              Ustrcmp(placement->value, "above") == 0;

            /* MusicXML has two kinds of thing in <articulations>. Accents etc
            are marks on a note, but also included are breath mark and
            caesura, which in PMW are separate items. Luckily, accent flags are
            all greater than 256 and item IDs are all less, so these are easily
            distinguished. */

            for (i = 0; i < pmw_articulations_count; i++)
              {
              const articdef *pa = pmw_articulations + i;

              if (Ustrcmp(ni->name, pa->name) == 0)
                {
                if (atype != NULL && (Ustrcmp(ni->name, "strong-accent") != 0 ||
                    Ustrcmp(atype, "up") != 0))
                  xml_Eerror(ni, ERR43, "Type", atype);

                if (pa->flags < 256)
                  {
                  (void)xml_get_item(staff, sizeof(bstr), pa->flags);
                  }
                else
                  {
                  newnote->acflags |= pa->flags;
                  }

                break;
                }
              }

            if (i >= pmw_articulations_count)
              xml_Eerror(articulations, ERR43, US"articulation", ni->name);

            if (stem != NULL && placement_known &&
                (stem_up > 0) == placement_above)
              newnote->acflags |= af_opposite;
            }
          }  /* End articulations */


        /* Technical marks */

        if (technical != NULL)
          {
          xml_item *bow = xml_find_item(technical, US"down-bow");
          if (bow != NULL) newnote->acflags |= af_down; else
            {
            bow = xml_find_item(technical, US"up-bow");
            if (bow != NULL) newnote->acflags |= af_up;
            }
          }
        }   /* End notations */


      /* Deal with beaming for notes shorter than a crotchet. For a chord, the
      beam setting might be on any of the notes. MusicXML has begin, continue,
      and ending beam setting, and also hooks, in contrast to PMW, which has
      only beambreak items. In order not to generate redundant beambreaks, e.g.
      before a longer note, we save them for output before the next shorter-
      than-a-crotchet note, if any, rather than outputting immediately.

      If the note/chord has no beam references whatsoever, set up a complete
      beam break. Otherwise, find the lowest-numbered "end" item for this
      note/chord. If there isn't one, do nothing; otherwise set up an
      appropriate beam break. */

      if (note_duration < (uint)divisions)
        {
        int bbs = beam_breakpending[staff];

        if (bbs < BREAK_UNSET)
          {
          b_beambreakstr *bb = mem_get_insert_item(sizeof(b_beambreakstr),
            b_beambreak, (bstr *)newnote);
          bb->value = (bbs == 0)? 255 : bbs;
          beam_breakpending[staff] = BREAK_UNSET;
          }

        /* Scan for the lowest-numbered beam ending, but note if *any* beam
        item is seen. */

        for (xml_item *beam = xml_find_item(mi, US"beam");
             beam != NULL;
             beam = xml_find_next(mi, beam))
          {
          uschar *s = xml_get_this_string(beam);
          beam_seen[staff] = TRUE;
          if (Ustrcmp(s, US"end") == 0)
            {
            int bn = xml_get_attr_number(beam, US"number", 1, 10, 1, FALSE);
            if (bn < beam_leastbreak[staff]) beam_leastbreak[staff] = bn;
            }
          }

        /* After a single note or the last note in a chord, set up any required
        break. */

        if (inchord[staff] == NULL)
          {
          int blb = -1;
          if (beam_seen[staff])
            {
            if (beam_leastbreak[staff] < 100) blb = beam_leastbreak[staff];
            }
          else blb = 1;

          if (blb > 0)
            {
            beam_breakpending[staff] = blb - 1;
            beam_leastbreak[staff] = 100;
            beam_seen[staff] = FALSE;
            }
          }
        }  /* End handling notes shorter than a crotchet. */

      /* If this note is not shorter than a crotchet, discard any pending beam
      break. */

      else
        {
        beam_breakpending[staff] = BREAK_UNSET;
        beam_leastbreak[staff] = BREAK_UNSET;
        beam_seen[staff] = FALSE;
        }
      }  /* End of handling note */

    /* Handle a rest */

    else
      {
      int yadjust = 0;
      uschar *dstep = xml_get_string(rest, US"display-step", NULL, FALSE);

      if (dstep != NULL)
        {
        int doctave = xml_get_number(rest, US"display-octave", 0, 9, -1, TRUE);

        if (doctave >= 0)
          {
          int nstep = *dstep - 'A' - 2;
          if (nstep < 0) nstep += 7;

          /* Position on staff as for current clef is computed as how far above
          or below middle C plus the middle C position on the staff. */

          yadjust = 2000*(nstep + 7*(doctave - 4) - 4 +
            clef_cposition[current_clef[staff]]);
          }
        }

      if (whole_bar_rest)
        {
        newnote->length = (note_duration/divisions) * len_crotchet;
        newnote->notetype = semibreve;
        newnote->flags = nf_centre;
        }
      else
        {
        note_type_and_length(newnote, note_type_name, dots, tuplet_num[staff],
          tuplet_den[staff], mi, type);

// set nf_hidden for invisible
//        write_noteletter(pstaff, mi, note_type_name, rest_letter, 0, dots);
        }

      newnote->yextra += yadjust;
      }



#ifdef NEVER  /* This is all obsolete */
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
#endif


    /* At the end of a chord, or after a single note, add on items that have
    to wait for a chord's end, such as ends of slurs and tuplets. */

    if (inchord[staff] == NULL && pending_post_chord[staff] != NULL)
      {
      bstr *b = last_item_cache[staff];
      b->next = pending_post_chord[staff];
      pending_post_chord[staff]->prev = b;
      while (b->next != NULL) b = b->next;
      last_item_cache[staff] = b;
      pending_post_chord[staff] = NULL;

      if (end_tuplet[staff])
        {
        tuplet_num[staff] = 1;
        tuplet_den[staff] = 1;
        }
      }
    }


  /* ======================== <print> ======================== */

  else if (Ustrcmp(mi->name, "print") == 0)
    {

#ifdef NEVER
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
#endif


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

    for (xml_item *staff_layout = xml_find_item(mi, US"staff-layout");
         staff_layout != NULL;
         staff_layout = xml_find_next(mi, staff_layout))
      {
      int d = xml_get_number(staff_layout, US"staff-distance", 0, 1000, -1,
        FALSE);
      if (d > 0)
        {
        int n = xml_get_attr_number(staff_layout, US"number", 1, 100, 1, FALSE);

        d = d*400 + 16000;


#ifdef NEVER
        if (starting_noprint)
          {
          sprintf(save_ssabove[n], " [ssabove %s]\n", format_fixed(dfixed));
          }

        else
#endif

          {
          b_ssstr *ss;
          int which;

          if (n == 1)
            {
            which = b_ssabove;
            }
          else
            {
            which = b_sshere;
            n--;

#ifdef NEVER
            if (couple_settings[n] == COUPLE_DOWN ||   /* When coupled, must */
                couple_settings[n+1] == COUPLE_UP)     /* round to 4 pts */
              {
              d += 3999;
              d -= d % 4000;
              }
#endif

            }

          ss = xml_get_item(n, sizeof(b_ssstr), which);
          ss->relative = FALSE;
          ss->stave = prevstave + n;
          ss->value = d;
          }
        }
      }

    /* Similarly for the system gap, though there is no "number" to deal with;
    just put "above" and "next" directives on the first staff of this part. */

    for (xml_item *system_layout = xml_find_item(mi, US"system-layout");
         system_layout != NULL;
         system_layout = xml_find_next(mi, system_layout))
      {
      int d = xml_get_number(system_layout, US"system-distance", 0, 1000, -1,
        FALSE);
      if (d > 0)
        {
        b_sgstr *sg1 = xml_get_item(1, sizeof(b_sgstr), b_sgabove);
        b_sgstr *sg2 = xml_get_item(1, sizeof(b_sgstr), b_sgnext);
        d = d*400 + 16000;
        sg1->relative = sg2->relative = FALSE;
        sg1->value = sg2->value = d;
        }
      }
    }


  /* ======================== pmw-suspend ======================== */

  else if (Ustrcmp(mi->name, "pmw-suspend") == 0)
    {
    int number = xml_get_attr_number(mi, US"number", 1, scount, -1, FALSE);
    for (int n = 1; n <= scount; n++)
      {
      if (number > 0 && number != n) continue;
      (void)xml_get_item(n, sizeof(bstr), b_suspend);
      suspended[n] = TRUE;
      }
    }



#ifdef NEVER
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

#endif


  }  /* Repeat for items in the measure */

/* End of measure; add a barline for staves that don't already have one. */

for (int n = 1; n <= scount; n++)
  {
  if (((bstr *)(last_item_cache[n]))->type != b_barline)
    {
    b_barlinestr *bl = xml_get_item(n, sizeof(b_barlinestr), b_barline);
    bl->bartype = barline_normal;
    bl->barstyle = 0;
    }
  }

return yield;
}



/*************************************************
*         Find widest line in stave name         *
*************************************************/

static int32_t
getsnwidth(snamestr *sn)
{
fontinststr fdata = { NULL, sn->size, 0 };
uint32_t *s = sn->text;
int32_t max = 0;

for (;;)
  {
  int32_t w, save;
  uint32_t *t;

  for (t = s; *t != 0 && *t != ss_verticalbar; t++) {}
  save = *t;
  *t = 0;
  w = string_width(s, &fdata, NULL);
  if (w > max) max = w;
  if (save == 0) break;
  *t = save;
  s = t + 1;
  }

return max;
}



/*************************************************
*              Process stave name(s)             *
*************************************************/

/* Use the display name if set, else name..

Arguments:
  name          pointer to name item
  name_display  pointer to name-display item
  middle        TRUE if name is between two staves

Returns:        pointer to a namestr block or NULL if no names

*/

static snamestr *
do_stave_name(xml_item *name, xml_item *name_display, BOOL middle)
{
snamestr *sn = NULL;
uint32_t buff[256];
uint32_t *bp = buff;
size_t len;

if (name_display != NULL)
  {
  for (xml_item *pp = name_display->next;
       pp != name_display->partner;
       pp = pp->partner->next)
    {
    uschar *ss = xml_get_this_string(pp);
    if (Ustrcmp(pp->name, "display-text") == 0)
      {
      bp += xml_convert_utf8(bp, ss, font_rm, TRUE);
      }
    else if (Ustrcmp(pp->name, "accidental-text") == 0)
      {
      if (Ustrcmp(ss, "flat") == 0)
        *bp++ = 0x27 | ((font_mf|font_small) << 24);   /* Char in music font */
      else if (Ustrcmp(ss, "sharp") == 0)
        *bp++ = 0x25 | ((font_mf|font_small) << 24);
      else xml_Eerror(pp, ERR43, US"accidental-text", ss);
      }
    }
  }

else if (name != NULL)
  {
  if (ISATTR(name, US"print-object", "yes", FALSE, "yes"))
    {
    uschar *ss = xml_get_this_string(name);
    bp += xml_convert_utf8(bp, ss, font_rm, TRUE);
    }
  }

*bp = 0;
len = bp - buff;

if (len != 0)
  {
  sn = mem_get(sizeof(snamestr));
  *sn = init_snamestr;
  sn->linecount = 1;

  if (middle) sn->flags |= snf_vcentre;
  if (xml_right_justify_stave_names) sn->flags |= snf_rightjust;

  for (bp = buff; *bp != 0; bp++) if (PCHAR(*bp) == '\n')
    {
    *bp = ss_verticalbar | PFTOP(*bp);
    sn->linecount++;
    }

  sn->text = mem_get((len + 1) * sizeof(uint32_t));
  memcpy(sn->text, buff, (len + 1) * sizeof(uint32_t));
  }

return sn;
}



/*************************************************
*          Process stave and group name          *
*************************************************/

/* Set up a name for a stave, and then add a group name as an additional
string. This works OK if the group contains more than one stave and the group
name is vertically separate from the stave. However, at least one example has
been seen where there is only one stave in a part that is also a group, without
any indication as to the separation of the strings. There is therefore some
fudgery in an attempt not to overprint.

Arguments:
  name          points to name item, or NULL
  name_display  points to name-display item, or NULL
  gname         points to group name item, or NULL
  gname-display points to group name-display item, or NULL
  middle        TRUE if the part name goes between two staves
  gmiddle       ditto for the group name
  snptr         where to store the pointer to the generated snamestr item

Returns:        nothing
*/

static void
do_stave_group_name(xml_item *name, xml_item *name_display, xml_item *gname,
  xml_item *gname_display, BOOL middle, BOOL gmiddle, snamestr **snptr)
{
snamestr *gn = do_stave_name(gname, gname_display, gmiddle);  /* Group name */
snamestr *sn = do_stave_name(name, name_display, middle);  /* Stave name to return */

*snptr = sn;
if (gn == NULL) return;   /* No group name */

/* When there is a group name, return it as *the* name if there is no stave
name. Otherwise, attach the group to the stave name's "extra" field. Without
further processing, it is likely that the stave name and the group name will
overprint. Therefore, we measure the lengths of the longest line in both of
them and add to the longer spaces equivalent to the length of the shorter. This
moves the longer one to the left. This isn't strictly necessary when one has
the "middle" flag and the other doesn't and neither this or the next stave is
suspended, but at this time we don't know the suspension state when these texts
will are used, */

if (sn == NULL) *snptr = gn; else
  {
  int add, len;
  uint32_t *s, *new, **which;
  int32_t spwidth = font_charwidth(' ', 0, font_rm, sn->size, NULL);
  int32_t swidth = getsnwidth(sn);
  int32_t gwidth = getsnwidth(gn);

  sn->extra = gn;

  if (swidth > gwidth)
    {
    add = gwidth/spwidth + 1;
    which = &(sn->text);
    }
  else
    {
    add = swidth/spwidth + 1;
    which = &(gn->text);
    }

  s = *which;

  /* Copy the stave name text, adding the required number of spaces. This
  abandons the original memory, but that doesn't seem a huge issue. At present,
  this code works only for right-justified texts (the default). */

  for (len = 0; s[len] != 0; len++) {}
  new = mem_get((len+add+1)*sizeof(uint32_t));
  memcpy(new, s, len * sizeof(uint32_t));
  for (int n = 0; n < add; n++) new[len+n] = RM|' ';
  new[len+add] = 0;
  *which = new;
  }
}



/*************************************************
*              Process part name(s)              *
*************************************************/

/* The stavestr for the first stave is in the global "st".

Argument:
  p         pointer to part element
  pstaff    first PMW staff number

Returns:    nothing
*/

static void
do_part_name(xml_part_data *p, int pstaff)
{
xml_item *gname, *gname_display, *gabbrev, *gabbrev_display;
xml_group_data *g = group_name_staves[pstaff];
snamestr **snptr = &(st->stave_name);

BOOL middle = p->stave_count == 2;
BOOL gmiddle;

if (g != NULL)
  {
  gname = g->name;
  gname_display = g->name_display;
  gabbrev = g->abbreviation;
  gabbrev_display = g->abbreviation_display;
  gmiddle = ((g->last_pstave - g->first_pstave) & 1) != 0;
  }
else
  {
  gname = gname_display = NULL;
  gabbrev = gabbrev_display = NULL;
  gmiddle = FALSE;
  }

/* Main name */

do_stave_group_name(p->name, p->name_display, gname, gname_display, middle,
  gmiddle, snptr);

/* Abbreviations are tacked on as additional snamestr blocks. */

if (*snptr != NULL) snptr = &((*snptr)->next);
do_stave_group_name(p->abbreviation, p->abbreviation_display, gabbrev,
  gabbrev_display, middle, gmiddle, snptr);




#ifdef NEVER
st->stave_name = do_stave_name(p->name, p->name_display, p->stave_count == 2);

/* Add group name as an additional string */

if ((g = group_name_staves[pstaff]) != NULL)
  {
  snamestr *sn = do_stave_name(g->name, g->name_display,
    ((g->last_pstave - g->first_pstave) & 1) != 0);
  if (st->stave_name == NULL) st->stave_name = sn;
    else st->stave_name->extra = sn;
  }

/* Abbreviations are tacked on as additional snamestr blocks. */

snptr = &(st->stave_name);
if (*snptr != NULL) snptr = &((*snptr)->next);

*snptr =
  do_stave_name(p->abbreviation, p->abbreviation_display, p->stave_count == 2);

if ((g = group_abbrev_staves[pstaff]) != NULL)
  {
  snamestr *sn = do_stave_name(g->abbreviation, g->abbreviation_display,
    ((g->last_pstave - g->first_pstave) & 1) != 0);
  if (*snptr == NULL) *snptr = sn;
    else (*snptr)->extra = sn;
  }
#endif


}



/*************************************************
*           Generate from partwise XML           *
*************************************************/

void
xml_do_parts(void)
{
int pstaff;
xml_group_data *g;
int prev_pmw_stave = 0;

/* SEE BELOW - IS THAT THE RIGHT PLACE?? */
#ifdef NEVER
for (pstaff = 0; pstaff <= PARTSTAFFMAX; pstaff++)
  {
  current_octave[pstaff] = 1;
  clef_octave_change[pstaff] = 0;
  }
#endif

for (pstaff = 0; pstaff <= xml_pmw_stave_count; pstaff++)
  group_name_staves[pstaff] = group_abbrev_staves[pstaff] = NULL;

/* Scan the groups to check for group names. When found, find the PMW stave
with which to associate the name. */

for (g = xml_groups_list; g != NULL; g = g->next)
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

DEBUG(D_any) eprintf("===> Processing XML parts\n");

for (xml_part_data *p = xml_parts_list; p != NULL; p = p->next)
  {
  BOOL first = TRUE;

  for (int n = 0; n <= PARTSTAFFMAX; n++)
    {
    current_clef[n] = clef_treble;
//    current_octave[n] = 1;
    clef_octave_change[n] = 0;
    open_wedge[n] = NULL;

//    save_clef[n][0] = 0;
//    save_key[n][0] = 0;
//    save_ssabove[n][0] = 0;
//    save_time[n][0] = 0;

    set_noteheads[n] = nh_normal;
    suspended[n] = FALSE;
    }

  DEBUG(D_xmlstaves)
    {
    eprintf("Part %s ", p->id);
    if (p->name == NULL) eprintf("<No name>\n");
      else eprintf("\"%s\"\n", p->name->name);
    }

  /* Stave start for each stave used by this part */

  for (pstaff = prev_pmw_stave + 1; pstaff <= prev_pmw_stave + p->stave_count;
       pstaff++)
    {
    barlinestyles[pstaff] = 0;   /* Default */
    st = curmovt->stavetable[pstaff] = read_init_stave(pstaff, FALSE);
    if (pstaff > main_maxstave) main_maxstave = pstaff;

    /* Ensure bar indexes are big enough */

    read_ensure_bar_indexes(p->measure_count);

    /* Names apply only to the first staff of a part. */

    if (first)
      {
      do_part_name(p, pstaff);
      first = FALSE;
      }



#ifdef NEVER
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
#endif

    }

  /* Scan the measures for this part */

  measure_number = 1;
  measure_number_absolute = 0;
  next_measure_fraction = 0;

  for (xml_item *measure = xml_find_item(p->part, US"measure");
       measure != NULL;
       measure = xml_find_next(p->part, measure))
    {
    DEBUG(D_xmlstaves) eprintf("Measure %d\n", measure_number);
    measure_number += do_measure(prev_pmw_stave, p, measure);
    measure_number_absolute++;
    }

  prev_pmw_stave += p->stave_count;
  }

/* Fill in gaps in staves and bars */

read_tidy_staves(FALSE);


#ifdef NEVER
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
