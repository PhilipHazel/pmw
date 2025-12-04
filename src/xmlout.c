/*************************************************
*          PMW MusicXML output generation        *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: August 2025 */
/* This file last modified: December 2025 */

#include "pmw.h"


/* This module contains code for writing a MusicXML file. The way music is
encoded in MusicXML is very different to the way PMW does it, necessitating
some contortions in the translation. I wanted to minimise any disturbance in
the rest of PMW when adding this code, so there are areas that might have been
different had I had MusicXML in mind from the start - which of course I
couldn't have in 1987.

Note that it is necessary to run this code after PostScript or PDF output has
been generated because some of the analysis on which it depends - for example,
remembering which notes of a chord were tied - happens in the PostScript or PDF
output phase. Other parameters get set during pagination, so there is no
possibility (as things stand) of running this code immediately after reading
the input.

There is an attempt to give feedback on PMW items in the current file that have
not been translated. */

/* Enum values for known ignored items, used in conjuction with the X macro,
and the X_ignored variable. Current implementation allows up to 63. */

enum { X_DRAW, X_SLUROPT, X_SLURSPLITOPT, X_VLINE_ACCENT, X_SQUARE_ACC,
  X_SPREAD, X_HEADING, X_TEXT, X_FONT, X_CIRCUMFLEX, X_STRING_INSERT,
  X_TREBLETENORB, X_FIGBASS, X_TREMJOIN, X_RLEVEL, X_MOVE, X_ENC_BARNO,
  X_BARNO_INTERVAL, X_NEWLINE, X_NEWPAGE, X_SUSPEND, X_HALFSHARP, X_JOIN,
  X_MARGIN, X_FOOTNOTEFONT, X_ACCENTMOVE, X_BARNUMBER, X_BEAMACCRIT,
  X_BEAMMOVESLOPE, X_BREAKBARLINE, X_DOTRIGHT, X_NS, X_ENSURE, X_JUSTIFY,
  X_MIDI, X_NAME, X_NOTES, X_OLEVEL, X_COUNT };

#define X(N) X_ignored |= 1l << N

/* Some comment strings */

#define PART_SEPARATOR \
  "<!--===============================================================-->"

#define MEASURE_SEPARATOR \
  "<!--============================-->"

/* Convert millipoints to tenths. */

#define T(x) (x/400)

/* These are a mask and shifts for the beam data that is packed into a 64-bit
variable to save initializing multiple variables for every note. */

#define BSHIFT_MASK  0xffu
#define BSHIFT_FHOOK     0
#define BSHIFT_BHOOK     8
#define BSHIFT_BEGIN    16
#define BSHIFT_CONTINUE 24
#define BSHIFT_END      32

/* Size of vectors for slur, ornament, and voice handling. */

#define SLURS_MAX        8
#define ORNAMENT_MAX     8
#define UNDERLAY_MAX     8
#define VOICE_MAX        8


/*************************************************
*                Data tables                     *
*************************************************/

static const char *X_ignored_message[] = {
  "PMW drawing features",
  "Some slur options",
  "Slur split options",
  "Vertical line accent (\\'\\)",
  "Round brackets enclosing accidental (square brackets substituted)",
  "Spread chord sign",
  "Support for headings and footings is only partial",
  "Not all text options are supported",
  "Only roman, italic, bold, and bold italic fonts are supported",
  "Circumflex in underlay or overlay",
  "Page or bar number insert into string",
  "(8) with brackets for trebletenorB",
  "Some figured bass notations",
  "Non-zero join in [tremolo]",
  "Rest level adjustment",
  "[move] and/or [space] and/or [rmove], [smove], etc.",
  "Boxing or circling bar numbers",
  "Bar numbering every n bars, can only number all bars",
  "[newline] (use -x+newline to enable)",
  "[newpage] (use -x+newpage to enable)",
  "Suspend/resume (use -x+suspend to enable)",
  "Half sharp style 1",
  "Join or joindotted directive",
  "Margin setting within a stave",
  "Changing fonts within a footnote",
  "Moving the position of accents and/or ornaments",
  "[Barnumber]",
  "[beamacc] and/or [beamrit]",
  "[beammove] and/or [beamslope]",
  "[breakbarline] and/or [unbreakbarline]",
  "Movement of augmentation dots",
  "Change of note spacing within a stave",
  "[ensure]",
  "[justify]",
  "MIDI setting within a stave",
  "[name]",
  "[notes]",
  "[olevel] and/or [olhere]"
};

/* These are header dirctives that have no effect on MusicXML output. */

static const char *nondirs[] = {
  "accadjusts",
  "accspacing",
  "b2pffont",
  "barlinesize",
  "barlinespace",
  "barnumberlevel",
  "beamendrests",
  "beamflaglength",
  "beamthickness",
  "bottommargin",
  "bracestyle",
  "breakbarlinesx",
  "breveledgerextra",
  "breverests",
  "clefsize",
  "clefstyle",
  "clefwidths",
  "copyzero",
  "cuegracesize",
  "cuesize",
  "dotspacefactor",
  "draw",
  "endlinesluradjust",
  "endlineslurstyle",
  "endlinetieadjust",
  "endlinetiestyle",
  "extenderlevel",
  "footnotesep",
  "footnotesize",
  "gracesize",
  "gracespacing",
  "gracestyle",
  "hairpinlinewidth",
  "hyphenstring",
  "hyphenthreshold",
  "justify",
  "keydoublebar",
  "keysinglebar",
  "keywarn",
  "lastfooting",
  "layout",
  "ledgerstyle",
  "leftmargin",
  "longrestfont",
  "makekey",
  "maxbeamslope",
  "maxvertjustify",
  "midichannel",
  "midifornotesoff",
  "midistart",
  "miditempo",
  "miditranspose",
  "miditremolo",
  "midivolume",
  "midkeyspacing",
  "midtimespacing",
  "musicfont",
  "nobeamendrests",
  "nokerning",
  "nokeywarn",
  "nosluroverwarnings",
  "nospreadunderlay",
  "notespacing",
  "notimewarn",
  "nounderlayextenders",
  "nowidechars",
  "overlaydepth",
  "page",
  "pagefooting",
  "pageheading",
  "printkey",
  "printtime",
  "repeatbarfont",
  "repeatstyle",
  "righttoleft",
  "sluroverwarnings",
  "startbracketbar",
  "startlinespacing",
  "textfont",
  "thinbracket",
  "timefont",
  "timewarn",
  "topmargin",
  "trillstring",
  "tripletfont",
  "tripletlinewidth",
  "tupletfont",
  "tupletlinewidth",
  "underlaydepth",
  "underlayextenders",
  "underlaystyle",
  "vertaccsize"
};

static int nondirsize = sizeof(nondirs)/sizeof(char *);
static uint8_t dirs_ignored[100];
static int dirs_ignored_count = 0;

static const char *leftcenterright[] = { "left", "center", "right" };
static int lcr_order[] = { 1, 0, 2 };

static const char *XML_note_names[] = {
  "breve", "whole", "half", "quarter", "eighth", "16th", "32nd", "64th" };

static const int note_relative_lengths[] = {
  512, 256, 128, 64, 32, 16, 8, 4 };

static const char *XML_accidental_names[] = {
  "natural", "quarter-sharp", "sharp", "double-sharp",
  "slash-flat", "flat", "flat-flat" };

/* PMW absolute pitches work in quarter tones. MusicXML needs a letter name,
an octave, and an "alteration" value, which is the difference from the basic
letter pitch. This value is in semitones, with fractions allowed for
microtones. To avoid using floating point here, the values are multiplied by
10. The table gives the non-zero alternation values for each quarter tone in
the octave, depending on which letter name was used to get the absolute pitch.

For example, an absolute pitch of C natural could be generated from B-sharp or
D-double-flat, with alternation values 1 and -2 respectively. */

static int16_t alter_table[][4] = {
  { 'B', 10, 'D', -20 },   /* C natural */
  { 'C',  5, 'D', -15 },   /* C half-sharp */
  { 'C', 10, 'D', -10 },   /* C sharp / D flat */
  { 'C', 15, 'D',  -5 },   /* D half-flat */

  { 'C', 20, 'E', -20 },   /* D natural */
  { 'D',  5, 'E', -15 },   /* D half-sharp */
  { 'D', 10, 'E', -10 },   /* D sharp / E flat */
  { 'D', 15, 'E'   -5 },   /* E half-flat */

  { 'D', 20, 'F', -10 },   /* E natural */
  { 'E',  5, 'F',  -5 },   /* E half-sharp / F half-flat */

  { 'E', 10, 'G', -20 },   /* F natural */
  { 'F',  5, 'G', -15 },   /* F half-sharp */
  { 'F', 10, 'G', -10 },   /* F sharp / G flat */
  { 'F', 15, 'G',  -5 },   /* G half-flat */

  { 'F', 20, 'A', -20 },   /* G natural */
  { 'G',  5, 'A', -15 },   /* G half-sharp */
  { 'G', 10, 'A', -10 },   /* G sharp / A flat */
  { 'G', 15, 'A',  -5 },   /* A half-flat */

  { 'G', 20, 'B', -20 },   /* A natural */
  { 'A',  5, 'B', -15 },   /* A half-sharp */
  { 'A', 10, 'B', -10 },   /* A sharp / B flat */
  { 'A', 15, 'B',  -5 },   /* B half flat */

  { 'A', 20, 'C', -10 },   /* B natural */
  { 'B',  5, 'C'   -5 }    /* B half-sharp / C half flat */
};

/* Strings for beam settings. */

static const char *beam_names[] = {
  "forward hook", "backward hook", "begin", "continue", "end" };



/*************************************************
*             Local variables                    *
*************************************************/

/* Vectors that do not need to be initialized. */

static b_ornamentstr *ornament_pending[ORNAMENT_MAX];

static b_textstr *underlay_pending[UNDERLAY_MAX];
static uint8_t    underlay_state[UNDERLAY_MAX];

static b_notestr *overbeam_start = NULL;

static b_pletstr *plet_pending[MAX_PLETNEST];
static uint8_t    plet_actual[MAX_PLETNEST];
static uint8_t    plet_normal[MAX_PLETNEST];

static b_slurstr *slurs_active[SLURS_MAX];
static b_slurstr *slurs_pending[SLURS_MAX];
static uint16_t   slurs_trans[SLURS_MAX];

static b_slurstr *lines_active[SLURS_MAX];
static uint16_t   lines_trans[SLURS_MAX];

static int32_t    voice_hwm[VOICE_MAX];

static uschar     string_buffer[256];

/* This vector is used for a reverse Unicode translation table, which is
constructed the first time it is needed. It does not need to be reset for a new
movement. */

static uint32_t   unihigh[50] = { 0 };

/* This slur structure is used for PMW "short slurs", which in PMW are coded as
"ties" between different notes. The flags field gets set for each use; the
others are never changed, so static initialization is all that is needed. */

static b_slurstr  short_slur = { NULL, NULL, b_slur, 0, 0, NULL, 0 };

/* The bits in this variable are set for various ignored PMW items. It is
initialized statically, because the collection of bits applies to all movements
if multiple movements are being processed. A single message it output by the
outxml_write_ignored() function, called right at the end of processing. */

static uint64_t   X_ignored = 0;

/* This vector contains pointers to bit maps of suspended bars for each stave.
It is used only when processing of suspend/resume is explicitly enabled. The
pointers are then initialized to dynamic memory when the first movement is
being processed, triggered by the [1] value being NULL. */

static uint8_t   *suspendmap[64] = { NULL, NULL };

/* These variables have to be dynamically initialized for each movement that is
processed. */

static int        beam_state;
static BOOL       bowingabove;
static int        comment_bar;
static int        current_stave;
static int        ending_active;
static BOOL       gliss_active;
static int        indent;
static int        lines_active_count;
static int        lines_trans_count;
static int        ornament_pending_count;
static BOOL       plet_enable;
static int        plet_level;
static int        plet_pending_count;
static int        slurs_active_count;
static int        slurs_pending_count;
static int        slurs_trans_count;
static int        stop_tremolo_pending;
static b_tiestr  *tie_active;
static int        underlay_pending_count;

/* These variables do not need to be initialized. */

static FILE      *xml_file;
static movtstr   *xml_movt;
static uint64_t   xml_staves;
static int        xml_voice;
static BOOL       xml_suspended;

static barposstr *xml_barpos;
static posstr    *xml_pos;
static posstr    *xml_poslast;
static int32_t    xml_moff;
static uint32_t   xml_xoff;



/*************************************************
*        Write commentary item to stderr         *
*************************************************/

/* Used to note unsupported things. */

static void
comment(const char *format, ...)
{
va_list ap;
va_start(ap, format);

if (main_showid)
  {
  (void)fprintf(stderr, "PMW version %s\n", PMW_VERSION);
  main_showid = FALSE;
  }

uint32_t pno = xml_movt->barvector[comment_bar];
uint32_t pnofr = pno & 0xffff;
pno >>= 16;

fprintf(stderr, "XML output (%d/", current_stave);
if (pnofr == 0) fprintf(stderr, "%d) ", pno);
  else fprintf(stderr, "%d.%d) ", pno, pnofr);

vfprintf(stderr, format, ap);
fprintf(stderr, "\n");
va_end(ap);
}



/*************************************************
*         Write text to output with indent       *
*************************************************/

/* Each of these little functions writes to the output file, with variations on
how they use and/or modify the current indent, and whether or not the add a
newline at the end. It was easier just to write them individually rather than
make a common function with lots of arguments. */

/* Output at current indent, add newline, increase indent. */

static void
PA(const char *format, ...)
{
va_list ap;
va_start(ap, format);
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
indent += 2;
}

/* Decrease indent, output, add newline. */

static void
PB(const char *format, ...)
{
va_list ap;
va_start(ap, format);
indent -= 2;
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
}

/* Output at current indent with newline, no change to indent. */

static void
PN(const char *format, ...)
{
va_list ap;
va_start(ap, format);
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
}

/* Output at current indent, no newline (leave open) */

static void
PO(const char *format, ...)
{
va_list ap;
va_start(ap, format);
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
}

/* Output without indent (continue), do not add newline */

static void
PC(const char *format, ...)
{
va_list ap;
va_start(ap, format);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
}

/* Output without indent (continue), add newline, increase indent  */

static void
PCA(const char *format, ...)
{
va_list ap;
va_start(ap, format);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
indent += 2;
}



/*************************************************
*            Find the next stave item            *
*************************************************/

/* This is called to scan a stave when inserting missing "=" underlay
syllables and endslurs for short slurs. Within a bar it just delivers the next
item. At the end of a bar it moves to the next bar. */

static barstr *
nextinstave(barstr *b, int *abarno)
{
if (b->next != NULL) return (barstr *)b->next;
*abarno += 1;
return (*abarno >= st->barcount)? NULL : st->barindex[*abarno];
}



/*************************************************
*           Find the next underlay item          *
*************************************************/

static b_textstr *
nextulinstave(barstr *b, int *abarno)
{
b_textstr *t;
for (;;)
  {
  t = (b_textstr *)(b = nextinstave(b, abarno));
  if (t == NULL) break;
  if (t->type == b_text && (t->flags & text_ul) != 0) break;
  }
return t;
}



/*************************************************
*          Find the next "=" underlay item       *
*************************************************/

static b_textstr *
nexteqinstave(barstr *b, int *abarno)
{
b_textstr *t;
for (;;)
  {
  t = (b_textstr *)(b = (barstr *)nextulinstave(b, abarno));
  if (t == NULL) break;
  if (t->laylen == 1 && PCHAR(t->string[0]) == '=') break;
  }
return t;
}



/*************************************************
*            Find next note in stave             *
*************************************************/

static b_notestr *
find_next_note(barstr *b, int *abarno)
{
for (;;)
  {
  b = nextinstave(b, abarno);
  if (b == NULL || b->type == b_note) break;
  }
return (b_notestr *)b;
}



/*************************************************
*            Convert PMW string                  *
*************************************************/

/* This just converts the characters into a single buffer, ignoring the font
information except to check for standard encoding. */

static uschar *
convert_PMW_string(uint32_t *s)
{
uschar *pp = string_buffer;
for (uint32_t *p = s; *p != 0; p++)
  {
  uint32_t c = PCHAR(*p);

  /* Handle special characters above the Unicode limit. */

  if (c > MAX_UNICODE)
    {
    switch(c)
      {
      case ss_verticalbar:   c = '\n'; break;
      case ss_asciiquote:    c = '\''; break;
      case ss_asciigrave:    c = '`'; break;
      case ss_escapedhyphen: c = '-'; break;
      case ss_escapedequals: c = '='; break;
      case ss_escapedsharp:  c = '#'; break;

      default:
      if (c >= ss_top)
        error(ERR191, "Unknown special character in string");
      else X(X_STRING_INSERT);
      continue;
      }
    }

  /* If the character is above LOWCHARLIMIT and the font is standardly encoded,
  convert the value back to the original Unicode code point. The first time we
  need to do this we construct the relevant lookup table from the table that
  goes the other way. */

  else if (c >= LOWCHARLIMIT)
    {
    int f = PBFONT(c);
    fontstr *fs = &(font_list[font_table[f]]);
    if ((fs->flags & ff_stdencoding) != 0)
      {
      if (unihigh[0] == 0)
        {
        for (usint i = 0; i < an2ucount; i++)
          {
          an2uencod *an = an2ulist + i;
          if (an->poffset >= 0) unihigh[an->poffset] = an->code;
          }
        }
      c = unihigh[c - LOWCHARLIMIT];
      }
    }

  /* Add to the new UTF-8 string, escaping a few characters because this is
  XML. */

  switch(c)
    {
    case '&': pp += sprintf(CS pp, "&amp;"); break;
    case '<': pp += sprintf(CS pp, "&lt;"); break;
    case '>': pp += sprintf(CS pp, "&gt;"); break;
    default: pp += misc_ord2utf8(c, pp); break;
    }
  }

*pp = 0;
return string_buffer;
}



/*************************************************
*        Start an element with font info         *
*************************************************/

/* Start an element that has a font setting. It's left open for possible
further additions. */

static void
element_set_font(const char *name, int font, int size)
{
PO("<%s font-size=\"%s\"", name, sff(size));
switch(font)
  {
  case font_rm:
  break;

  case font_bf:
  PC(" font-weight=\"bold\"");
  break;

  case font_bi:
  PC(" font-weight=\"bold\" font-style=\"italic\"");
  break;

  case font_it:
  PC(" font-style=\"italic\"");
  break;

  default:
  X(X_FONT);
  break;
  }
}



/*************************************************
*              Write PMW string                  *
*************************************************/

/* A PMW string may contain changes of font. Each substring in a particular
font has to be output as a separate element. The second and subsequent ones
should all follow on.

Arguments:
  s        PMW string
  length   maximum length to output (used for underlay)
  size     font size
  elname   element name to use (e.g. "words")
  elattr   element attribute(s) to include
  x        horizontal positioning
  y        vertical positioning - ignore if INT32_MAX
  enc      enclosure value or NULL
  jus      justify value or NULL
  rot      rotation - MusicXML goes the opposite way to PMW (+ve = clockwise)

Returns:   nothing
*/

static void
write_PMW_string(uint32_t *s, uint32_t length, int32_t size, const char *elname,
  const char *elattr, int32_t x, int32_t y, const char *enc, const char *jus,
  int32_t rot)
{
BOOL first = TRUE;
usint count = 0;

(void)x;  /* PRO TEM */

while (*s != 0 && count < length)
  {
  uint32_t save;
  uint32_t *sb = s;
  uint32_t f = PFONT(*s);
  int32_t use_size = size;

  while (count++ < length && *s != 0 && PFONT(*s) == f) s++;
  save = *s;
  *s = 0;        /* Temporary terminator */

  if ((f & 0x80) != 0)  /* Small caps */
    {
    f &= 63;
    use_size = mac_muldiv(size, xml_movt->smallcapsize, 1000);
    }

  element_set_font(elname, f, use_size);
  if (elattr[0] != 0) PC("%s", elattr);

  if (first)
    {
    if (y != INT32_MAX) PC(" default-y=\"%d\"", y);
    first = FALSE;
    }

  if (enc != NULL) PC(" enclosure=\"%s\"", enc);
  if (jus != NULL) PC(" justify=\"%s\"", jus);
  if (rot != 0) PC(" rotation=\"%s\"", sff(-rot));

  PC(">%s</%s>\n", convert_PMW_string(sb), elname);
  *s = save;
  }
}



/*************************************************
*                  Write clef                    *
*************************************************/

typedef struct clef_info {
  const char *sign;
  int         line;
  int         octave;
}
clef_info;

static clef_info clef_data[] = {
  {"C", 3, 0},          /* alto */
  {"F", 3, 0},          /* baritone */
  {"F", 4, 0},          /* bass */
  {"C", 5, 0},          /* cbaritone */
  {"F", 4, -1},         /* contrabass */
  {"F", 5, 0,},         /* deepbass */
  {"percussion", 0, 0}, /* hclef */
  {"C", 2, 0},          /* mezzo */
  {"G", 2, 0},          /* none */
  {"F", 4, 1},          /* soprabass */
  {"C", 1, 0},          /* soprano */
  {"C", 4, 0},          /* tenor */
  {"G", 2, 0},          /* treble */
  {"G", 2, 1},          /* trebledescant */
  {"G", 2, -1},         /* trebletenor*/
  {"G", 2, -1}          /* trebletenorB*/
};

/* The recommendation for no clef is not to use the deprecated "none" sign, but
instead use treble with printing disabled. */

static void
write_clef(usint clef, BOOL assume)
{
const char *nonestring = (clef != clef_none && !assume)? "" :
  " print-object=\"no\"";

PA("<clef%s>", nonestring);
PN("<sign>%s</sign>", clef_data[clef].sign);
if (clef_data[clef].line != 0)
  PN("<line>%d</line>", clef_data[clef].line);
if (clef_data[clef].octave != 0)
  PN("<clef-octave-change>%d</clef-octave-change>", clef_data[clef].octave);
PB("</clef>");

/* Unavailable feature. */

if (clef == clef_trebletenorB) X(X_TREBLETENORB);
}



/*************************************************
*                     Write key                  *
*************************************************/

#define BAD 100

static int16_t key_fifths[] = {
    3,   5,   0,   2,   4,  -1,   1,  /* Major keys */
  BAD, BAD,   7, BAD, BAD,   6, BAD,  /* Major sharp keys */
   -4,  -2,  -7,  -5,  -3, BAD,  -6,  /* Major flat keys */
    0,   2,  -3,  -1,   1,  -4,   2,  /* Minor keys */
    7, BAD,   4,   6, BAD,   3,   5,  /* Minor sharp keys */
   -7,  -5, BAD, BAD,  -6, BAD, BAD   /* Minor flat keys */
};

static void
write_key(uint32_t key, uint32_t cancel_key, BOOL assume)
{
if (key == key_N)
  key = key_C;
else if (key >= key_X)
  {
  comment("custom key not supported: treated as C");
  key = key_C;
  }
PA("<key%s>", assume? " print-object=\"no\"" : "");
if (cancel_key >= 128) PN("<cancel>%d</cancel>", key_fifths[cancel_key & 63]);
PN("<fifths>%d</fifths>", key_fifths[key]);
PB("</key>");
}
#undef BAD



/*************************************************
*                   Write time                   *
*************************************************/

static void
write_time(uint32_t time, BOOL assume)
{
const char *symbol = "";

if ((time & 0x00ff0000u) != 0x00010000u)
  comment("time signature multiple ignored");

time &= 0x0000ffffu;  /* Remove multiplier */

if (time == time_common)
  {
  time = 0x00000404u;
  symbol = " symbol=\"common\"";
  }
else if (time == time_cut)
  {
  time = 0x00000404u;
  symbol = " symbol=\"cut\"";
  }
else if ((xml_movt->flags & mf_showtimebase) == 0)
  symbol = " symbol=\"single-number\"";

PA("<time%s%s>", symbol, assume? " print-object=\"no\"" : "");
PN("<beats>%d</beats>", time >> 8);
PN("<beat-type>%d</beat-type>", time & 0xffu);
PB("</time>");
}



/*************************************************
*         Handle start of a line                 *
*************************************************/

/* A lot of this is the same as for a slur, but it's easier just to keep them
separate because they are handled so differently in MusicXML. */

static void
line_start(b_slurstr *s)
{
BOOL above = (s->flags & sflag_b) == 0;

const char *line_type =
  ((s->flags & sflag_idot) != 0)? " line-type=\"dotted\"" :
  ((s->flags & sflag_i) != 0)? " line-type=\"dashed\"" : "";

if ((s->flags & sflag_e) != 0)
  comment("ignored editoral mark on line");

if (s->id != 0)
  lines_trans[lines_trans_count++] =
    (s->id << 8) | (lines_active_count + 1);

s->id = lines_active_count + 1;

/* If this is [xline] and there is another line active, put this one
before it on the active list. Otherwise, just add to the list. */

if ((s->flags & sflag_x) != 0 && lines_active_count > 0)
  {
  lines_active[lines_active_count] = lines_active[lines_active_count-1];
  lines_active[lines_active_count-1] = s;
  }
else lines_active[lines_active_count] = s;
lines_active_count++;

PA("<direction placement=\"%s\">", above? "above" : "below");
PA("<direction-type>");
PO("<bracket type=\"start\"%s number=\"%d\" line-end=\"%s\"", line_type, s->id,
  ((s->flags & sflag_ol) != 0)? "none" : above? "down" : "up");
if (s->ally != 0) PC(" relative-y=\"%d\"", T(s->ally));
PC("/>\n");

PB("</direction-type>");
PB("</direction>");
}



/*************************************************
*         Handle the end of a line               *
*************************************************/

/* If there's no id, terminate the most recent line. Note that we must use the
remembered MusicXML number that was put into the starting line structure, not
the id in [endline]. */

static void
line_end(b_bytevaluestr *e)
{
b_slurstr *s;

PA("<direction>");
PA("<direction-type>");

if (e->value == 0)   /* No id */
  {
  s = lines_active[--lines_active_count];
  }

/* Translate the PMW id into a line number, then seek that line in the
list. */

else
  {
  int i, n;

  for (i = 0; i < lines_trans_count; i++)
    {
    if ((lines_trans[i] >> 8) == e->value) break;
    }

  if (i >= lines_trans_count)
    error (ERR192, "could not translate PMW line id"); /* Hard */

  n = lines_trans[i] & 0xff;

  for (i = 0; i < lines_active_count; i++)
    {
    if (lines_active[i]->id == n) break;
    }

  if (i >= lines_active_count)
    {
    char buff[64];
    sprintf(buff, "could not find open line with id=%d", n);
    error(ERR192, buff);   /* Hard */
    }

  s = lines_active[i];

  /* Remove the line from the list, if not last. */

  if (i != --lines_active_count)
    memmove(lines_active + i, lines_active + i + 1,
      (lines_active_count - i) * sizeof(b_slurstr *));
  }

PN("<bracket type=\"stop\" number=\"%d\" line-end=\"%s\"/>", s->id,
  ((s->flags & sflag_or) != 0)? "none" :
  ((s->flags & sflag_b) == 0)? "down" : "up");

/* When all lines are closed, we can reset the translations. */

if (lines_active_count == 0) lines_trans_count = 0;

PB("</direction-type>");
PB("</direction>");
}



/*************************************************
*           Write a note, chord, or rest         *
*************************************************/

/* PMW and MusicXML have different conventions for chords. In PMW, the first
note of a chord has type b_note, with the nf_chord flag set. Subsequent notes
are of type b_chord, also with the flag set. The end of the chord happens when
a b_chord item is followed either by NULL (end of bar) or an item with type
other than b_chord. In MusicXML, the first note is unaffected, but subsequent
notes have a <chord/> element. We handle an entire chord here, returning the
pointer to the final note.

Arguments:
  b          bar structure item for the first note
  bar        current bar number
  divisions  divisions setting

Returns:     pointer to the final note processed
*/

static barstr *
write_note(barstr *b, int bar, int divisions)
{
int add_caesura = -1;
int abovecount = 0;
uint64_t beam_data = 0;
BOOL add_comma = FALSE;
BOOL add_tick = FALSE;
BOOL inchord = FALSE;
BOOL stoptie = FALSE;
BOOL stopgliss = FALSE;
b_tremolostr *add_tremolo = NULL;
barstr *bb;

/* Make if the note/chord is at the end of a tie or glissando. */

if (tie_active != NULL)
  {
  stoptie = TRUE;
  tie_active = NULL;
  }

if (gliss_active)
  {
  stopgliss = TRUE;
  gliss_active = FALSE;
  }

/* Find the last note if this is the start of a chord. */

for (bb = b; bb->next != NULL; bb = (barstr *)(bb->next))
  if (bb->next->type != b_chord) break;

/* See if this note/chord is followed by a tie item. If it is, check that a
single note was actually tied in the PS/PDF output. If not, the tie item is
really a short slur. */

if (bb->next->type == b_tie)
  {
  tie_active = (b_tiestr *)(bb->next);
  bb = (barstr *)tie_active;   /* "Last" is now the tie item */

  /* If this is really a short slur we have to set up a fake pending slur and
  insert an endslur after the next note. This may also be a glissando, with or
  without an accompanying slur. */

  if ((((b_notestr *)b)->flags & (nf_chord|nf_wastied)) == 0)
    {
    if ((tie_active->flags & (tief_slur|tief_default)) != 0)
      {
      if (slurs_pending_count >= SLURS_MAX)
        error(ERR191, "too many nested slurs");
      else
        {
        slurs_pending[slurs_pending_count++] = &short_slur;
        short_slur.flags = (tie_active->abovecount <= 0)? sflag_b : 0;

        /* Find next note */

        int barno = bar;
        b_notestr *nb = find_next_note(bb, &barno);
        if (nb == NULL) error (ERR192, "missing note after short slur"); /* Hard */

        b_endslurstr *es =
          mem_get_insert_item(sizeof(b_endslurstr), b_endslur, nb->next);
        es->value = 0;
        }
      }

    if ((tie_active->flags & tief_gliss) != 0) gliss_active = TRUE;
    tie_active = NULL;  /* It's not actually a tie. */
    }

  /* Not a short slur; this is a real tie. */

  else abovecount = tie_active->abovecount;
  }

/* See if [comma], [tick], [tremolo], or a caesura follows, before the next
note or end of bar. [Tremolo] must start in <notations> for the note; the
others have to come in <articulations>. */

for (barstr *bbn = (barstr *)bb->next; bbn != NULL; bbn = (barstr *)bbn->next)
  {
  if (bbn->type == b_note) break;

  switch(bbn->type)
    {
    case b_caesura:
    add_caesura = xml_movt->caesurastyle;
    break;

    case b_comma:
    add_comma = TRUE;
    break;

    case b_tick:
    add_tick = TRUE;
    break;

    case b_tremolo:
    add_tremolo = (b_tremolostr *)bbn;
    break;

    default:
    break;
    }
  }

/* If this is a non-rest that is shorter than a crotchet, do some beam
processing. First of all, take a look at what follows - beambreak and/or a
following note. If there is a beambreak, it will be the immediately next item,
pointed to by bb. */

if (((b_notestr *)b)->notetype >= quaver && ((b_notestr *)b)->spitch != 0)
  {
  int thiscount, thatcount, beambreak;
  b_notestr *nextnote;

  /* Not continuing a beam from the previous bar. */

  if (overbeam_start == NULL)
    {
    thatcount = 0;
    thiscount = ((b_notestr *)b)->notetype - crotchet;
    nextnote = NULL;
    }

  /* We are continuing a beam from the previous bar. */

  else
    {
    thiscount = overbeam_start->notetype - crotchet;
    thatcount = ((b_notestr *)b)->notetype - crotchet;
    }

  beambreak = (bb->next->type == b_beambreak)?
    ((b_beambreakstr *)(bb->next))->value : -1;

  /* All set to go if we are continuing over a bar line. */

  if (overbeam_start != NULL) overbeam_start = NULL;

  /* Otherwise, seek the next note if there isn't an explicit break for all
  beams. No following note in the bar, or a note longer than a quaver is a full
  beambreak. Skip over any rests shorter than a crotchet - if a suitable next
  note is found, the rests will appear under the beam.

  SPECIAL CASE: If an overbeam item is found while looking for the next note
  (overbeam always immediately precedes a barline), look for the next note in
  the next bar. If a continuing beam is required, remember this next note. */

  else if (beambreak != BEAMBREAK_ALL)
    {
    for (barstr *bbn = (barstr *)bb->next; bbn != NULL;
         bbn = (barstr *)bbn->next)
      {
      /* Handle beaming over a bar line. */

      if (bbn->type == b_overbeam && bar + 1 < st->barcount)
        {
        for (barstr *bbo = st->barindex[bar+1]; bbo != NULL;
             bbo = (barstr *)bbo->next)
          {
          b_notestr *nn = (b_notestr *)bbo;
          if (nn->type != b_note) continue;

          if (nn->notetype > crotchet)
            {
            if (nn->spitch == 0) continue;  /* Skip over short rest */

            /* Found a note shorter than a crotchet in the next bar onto which
            we can continue beaming. Set it up as nextnote, and remember this
            note for use in the next bar. */

            nextnote = nn;
            thatcount = nextnote->notetype - crotchet;
            overbeam_start = (b_notestr *)bb;
            }

          /* Either found a continuation or beam ender. No need to look further
          into the next bar. */

          break;
          }

        /* No need to look any further in the current bar. */

        break;
        }

      /* Not an overbeam item. Check out the next note in this bar. */

      else
        {
        nextnote = (b_notestr *)bbn;
        if (nextnote->type == b_note)
          {
          if (nextnote->spitch == 0 && nextnote->notetype > crotchet) continue;
          thatcount = nextnote->notetype - crotchet;
          break;
          }
        }
      }

    /* No next note, or at least a crotchet (which includes a non-skipped rest)
    breaks all beams. */

    if (nextnote == NULL || nextnote->notetype <= crotchet)
      beambreak = BEAMBREAK_ALL;
    }

  /* At this point, nextnote is either NULL (no note follows in the bar) or
  points to the next actual note, and beambreak is either negative (unset),
  BEAMBREAK_ALL (explicitly or implicitly set), or an explicit number NOT to
  break. We can deal with the latter case by adjusting the value of thatcount
  if necessary. */

  if (beambreak >= 0 && beambreak != BEAMBREAK_ALL && beambreak < thatcount)
    thatcount = beambreak;

  /* Now we can set up beaming data that is to be output below in the correct
  place in the <note> element. For each beam setting (begin, end, etc) two
  4-bit values are packed into a byte: the start and end number. For example,
  if we are starting a semiquaver beam, we set (1,2) because two
  <beam>begin</beam> elements are needed. When dealing with changes in the
  number of beams the start can be otner than 1. */

  /* If not currently in a beam, set up to start one unless BEAMBREAK_ALL is
  set. */

  if (beam_state < 0)
    {
    if (beambreak != BEAMBREAK_ALL)
      {
      if (thiscount > thatcount)  /* This note is shorter, need hook */
        {
        beam_data |= (16 | thatcount) << BSHIFT_BEGIN;  /* 16 == 1 << 4 */
        beam_state = thatcount;
        beam_data |= (((thatcount + 1) << 4) | thiscount) << BSHIFT_FHOOK;
        }
      else  /* This note is equal or longer; start all beams from 1 to this */
        {
        beam_data |= (16 | thiscount) << BSHIFT_BEGIN;
        beam_state = thiscount;
        }
      }
    }

  /* There are currently beam_state beams open. Handle termination - end all
  beams, then add backwards hooks if this note needs more than were open. */

  else if (beambreak == BEAMBREAK_ALL)
      {
    beam_data |= (16 | (uint64_t)beam_state) << BSHIFT_END;
    if (thiscount > beam_state)
      beam_data |= (((beam_state + 1) << 4) | thiscount) << BSHIFT_BHOOK;
    beam_state = -1;
    }

  /* Beam continuation. It is easier to enumerate the separate cases than
  try to amalgamate them. We have to consider which beams to continue, which
  to terminate, and which new ones to start. */

  else if (thiscount == beam_state)
    {
    /* This note needs exactly the same number of beams as are in force. If
    the next note needs no fewer, just continue all of them. If not, continue
    those that the next note does need, and terminate the others. */

    if (thatcount >= thiscount)
      beam_data |= (16 | thiscount) << BSHIFT_CONTINUE;
    else
      {
      beam_data |= (16 | thatcount) << BSHIFT_CONTINUE;
      beam_data |=
        (uint64_t)(((thatcount + 1) << 4) | thiscount) << BSHIFT_END;
      beam_state = thatcount;
      }
    }

  /* This note needs more beams than are currently in force. */

  else if (thiscount > beam_state)
    {
    /* This note is shorter than the next note; need hook. */

    if (thiscount > thatcount)
      {
      beam_data |= (16 | thatcount) << BSHIFT_CONTINUE;
      beam_state = thatcount;
      beam_data |= (((thatcount + 1) << 4) | thiscount) << BSHIFT_FHOOK;
      }

    /* This note is equal or longer than the next note; continue all beams up
    to beam_state, then start new beams from beam_state plus 1 to this. */

    else
      {
      beam_data |= (16 | beam_state) << BSHIFT_CONTINUE;
      beam_data |= (((beam_state + 1) << 4) | thiscount) << BSHIFT_BEGIN;
      beam_state = thiscount;
      }
    }

  /* This note needs fewer beams that are currently in force. This state
  should never be possible because the previous note should never send out
  more beams than the next note needs. */

  else
    {
    error(ERR191, "too many beams in force");
    }
  }  /* Done beam setup processing */

/* Now we can process the note/chord */

for (;;)
  {
  b_notestr *note = (b_notestr *)b;
  uint32_t acflags = note->acflags;
  BOOL opposite = (acflags & af_opposite) != 0;
  BOOL wastied = (note->flags & nf_wastied) != 0;
  BOOL grace = (note->length == 0 && note->spitch != 0);
  const char *ac_placement =
    (((note->flags & nf_stemup) != 0) == opposite)? "above" : "below";

  PA("<note%s>", ((note->flags & nf_hidden) != 0)? " print-object=\"no\"" : "");
  if (grace) PN("<grace%s/>", ((note->flags & nf_appogg) != 0)?
    " slash=\"yes\"" : "");
  else if ((note->flags & nf_cuesize) != 0) PN("<cue/>");
  if (inchord) PN("<chord/>");

  // TODO Handle rest level, which can only be done by setting display-step and
  // display-octave, relative to the current clef. PMW just has a relative
  // movement, so we would also need to consider which type of rest it is.

  if (note->spitch == 0)
    {
    PO("<rest");
    if ((note->flags & nf_centre) != 0) PC(" measure=\"yes\"");
    if (note->yextra != 0) X(X_RLEVEL);
    PC("/>\n");
    }

  // TODO Think about <unpitched>?
  else
    {
    int letter = toupper(note->char_orig) + active_transpose_letter;
    while (letter > 'G') letter -= 7;
    while (letter < 'A') letter += 7;

    int16_t *p = alter_table[note->abspitch % OCTAVE];
    int alter = (letter == p[0])? p[1] : (letter == p[2])? p[3] : 0;

    PA("<pitch>");
    PN("<step>%c</step>", letter);

    /* See comments above where alter_table is defined. The alternation value
    is a number of semitones, but we generate it as an integer * 10 to allow
    for quarter tones. */

    if (alter != 0) PN("<alter>%g</alter>", ((float)alter)/10);

    /* The octave value is the octave of the note letter, hence we need to
    adjust the absolute pitch before computing it. */

    PN("<octave>%d</octave>", (note->abspitch - (alter*2)/10)/OCTAVE);
    PB("</pitch>");
    }

  /* We have to convert the note length into "divisions". The value in the
  divisions variable is the number in a crotchet. Therefore, the value we need
  is (notelength/len_crotchet)*divisions, but calculating like that loses
  fractions of a crotchet, and (notelength*divisions)/len_crotchet runs the
  risk of 32-bit overflow if divisions is greater than 8 - which seems quite
  likely as the length of a breve is 0x015fea00. Therefore, resort to using
  64-bit arithmetic. */

  if (!grace) PN("<duration>%d</duration>",
    (uint32_t)(((uint64_t)note->length * divisions) / (uint64_t)len_crotchet));

  /* The <tie> element is concerned with sound, and comes here. Do nothing if
  this note wasn't actually tied. */

  if (wastied)
    {
    if (stoptie) PN("<tie type=\"stop\"/>");
    if (tie_active != NULL) PN("<tie type=\"start\"/>");
    }

  /* Note details. In a movement that uses {backup] or [reset], we include a
  voice number. */

  if (xml_voice != 0) PN("<voice>%d</voice>", xml_voice);

  PN("<type>%s</type>",
    XML_note_names[(note->masq == MASQ_UNSET)? note->notetype : note->masq]);

  if ((note->flags & (nf_dot|nf_dot2)) != 0) PN("<dot/>");
  if ((note->flags & nf_dot2) != 0) PN("<dot/>");
  if (note->acc != ac_no && (note->flags & nf_accinvis) == 0)
    {
    const char *bra = "";
    if ((note->flags & (nf_accrbra|nf_accsbra)) != 0)
      {
      if ((note->flags & nf_accrbra) != 0) X(X_SQUARE_ACC);
      bra = " bracket=\"yes\"";
      }
    PN("<accidental%s>%s</accidental>", bra,
      XML_accidental_names[note->acc - 1]);
    }

  /* MusicXML requires that we supply information about any masquerading,
  giving the ratio of the displayed ("actual") note to the "normal" note that
  it replaces. The note extension flags for dotted notes are those of the
  masquerade, but there is also a record of the original state. */

  if (note->masq != MASQ_UNSET)
    {
    int rla = note_relative_lengths[note->masq];
    int rln = note_relative_lengths[note->notetype];

    switch(note->flags & (nf_dot|nf_dot2|nf_plus))
      {
      case nf_plus: rla += rla/4; break;
      case nf_dot|nf_dot2: rla += 3*rla/4; break;
      case nf_dot: rla += rla/2; break;
      }

    switch(note->dot_orig)
      {
      case od_plus: rln += rln/4; break;
      case od_dot|od_dot2: rln += 3*rln/4; break;
      case od_dot: rln += rln/2; break;
      }

    /* Remove common factors of 2 */

    while (rla % 2 == 0 && rln % 2 == 0) { rla /= 2; rln /= 2; }

    PA("<time-modification>");
      PN("<actual-notes>%d</actual-notes>", rla);
      PN("<normal-notes>%d</normal-notes>", rln);
      PN("<normal-type>%s</normal-type>", XML_note_names[note->notetype]);
    PB("</time-modification>");
    }

//TODO allow for different half-accidental styles

  /* If this is the start of one or more tuplets, put the data onto a stack
  (used to deal with nested tuplets). Leave plet_pending_count unchanged,
  because it is used later on for other tuplet options. */

  if (plet_pending_count > 0)
    {
    for (int i = 0; i < plet_pending_count; i++)
      {
      plet_actual[++plet_level] = plet_pending[i]->pletlen;
      plet_normal[plet_level] = plet_pending[i]->pletnum;
      }
    }

  /* If this note is in a tuplet we have to set up <time-modification> here. */

  if (plet_actual[plet_level] != 0)
    {
    PA("<time-modification>");
    PN("<actual-notes>%d</actual-notes>", plet_actual[plet_level]);
    PN("<normal-notes>%d</normal-notes>", plet_normal[plet_level]);
    PB("</time-modification>");
    }

  /* Stem */

  if ((note->flags & nf_stem) == 0) PN("<stem>none</stem>"); else
    {
    int32_t yextra = note->yextra + xml_movt->stemadjusts[note->notetype];
    PO("<stem");
    if (yextra != 0) PC(" relative-y=\"%d\"", T(yextra));
    if ((note->flags & nf_stemup) != 0) PC(">up</stem>\n");
      else PC(">down</stem>\n");
    }

  /* Notehead style */

  const char *nhstyle = NULL;
  switch(note->noteheadstyle)
    {
    case nh_normal:
    break;

    case nh_cross:
    nhstyle = "x";
    break;

    case nh_harmonic:
    nhstyle = "diamond";
    break;

    case nh_none:
    nhstyle = "none";
    break;

    case nh_direct:
    case nh_circular:
    comment("notehead style 'direct' or 'circular' not supported");
    break;
    }

  if (nhstyle != NULL) PN("<notehead>%s</notehead>", nhstyle);

  /* Handle beam settings. The analsys above packed all the data into a single
  variable, which contains a byte for each type of beam setting. The byte
  contains two nibbles, the starting and ending beam numbers. Experiment has
  shown that some MusicXML interpreters are picky, and insist on the <beam>
  elements being in number order, which makes this processing a bit more
  complicated than it would be in beam_names order (which I originally used).
  */

  if (beam_data != 0)
    {
    for (usint n = 1; n <= 4; n++)   /* Loop for beam number */
      {
      uint64_t bd = beam_data;       /* Rescan beam data for all 4 */
      for (usint i = 0; i < sizeof(beam_names)/sizeof(char *); i++)
        {
        usint x = (usint)(bd & BSHIFT_MASK);
        if (n <= (x & 0x0f) && n >= (x >> 4))
          PN("<beam number=\"%d\">%s</beam>", n, beam_names[i]);
        bd >>= 8;
        if (bd == 0) break;
        }
      }
    }

  /* A number of things have to appear inside a <notations> element. Multiple
  appearances are permitted, but it's tidier to put them all inside just one
  occurrence. Similarly for <ornaments>. */

  BOOL notations_open = FALSE;
  BOOL ornaments_open = FALSE;

  /* Handle PMW accents. First record one that is not supported. */

  if ((acflags & af_vline) != 0)
    {
    X(X_VLINE_ACCENT);
    acflags &= ~af_vline;
    }

  /* These three must be in a <technical> element */

  if ((acflags & (af_down|af_up|af_ring)) != 0)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PA("<technical>");

    /* In PMW "bowing below" is really organ heel/toe. The accidental placement
    value is not relevant. */

    if ((acflags & af_down) != 0)
      PN(bowingabove? "<down-bow/>" : "<heel placement=\"below\"/>");
    else if ((acflags & af_up) != 0)
      PN(bowingabove? "<up-bow/>" : "<toe placement=\"below\"/>");

    else
      {
      if ((acflags & af_ring) != 0) PO("<harmonic");
      PC(" placement=\"%s\"/>\n", ac_placement);
      }

    PB("</technical>");
    acflags &= ~(af_down|af_up|af_ring);
    }

  /* The remaining accents go inside <articulations>, as do [comma] and
  caesura. */

  if ((acflags & af_accents) != 0 || add_caesura >= 0 || add_comma || add_tick)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PA("<articulations>");

    /* The accents have no value */

    if ((acflags & af_accents) != 0)
      {
      if ((acflags & (af_staccato|af_bar)) == (af_staccato|af_bar))
        PO("<detached-legato");
      else if ((acflags & af_staccato) != 0) PO("<staccato");
      else if ((acflags & af_bar) != 0) PO("<tenuto");
      else if ((acflags & af_gt) != 0) PO("<accent");
      else if ((acflags & af_wedge) != 0) PO("<staccatissimo");
      else if ((acflags & af_tp) != 0) PO("<strong-accent");
      else if ((acflags & af_staccatiss) != 0) PO("<spiccato");
      PC(" placement=\"%s\"/>\n", ac_placement);
      }

    /* The various pauses do have values. */

    if (add_caesura >= 0)
      {
      PN("<caesura>%s</caesura>", (add_caesura == 0)? "normal":"single");
      add_caesura = -1;    /* Only on first note in chord */
      }

    if (add_comma)
      {
      PN("<breath-mark>comma</breath-mark>");
      add_comma = FALSE;   /* Only on first note in chord */
      }

    if (add_tick)
      {
      PN("<breath-mark>tick</breath-mark>");
      add_tick = FALSE;   /* Only on first note in chord */
      }

    PB("</articulations>");
    }

  /* Handle ornaments. Things that PMW calls ornaments are represented in
  different ways in MusicXML. Some are directly under <notations> but others
  are inside <ornament> under <notations>. We therefore do a first pass for
  those that don't appear inside <ornament>. The ornament numbers are arranged
  so that these are all greater than or equal to or_ferm. There's typically
  only one ornament, though the code does allow for more than one, so there is
  no point in trying to optimise by doing things like removing those already
  handled. */

  for (int i = 0; i < ornament_pending_count; i++)
    {
    b_ornamentstr *orn = ornament_pending[i];
    if (orn->ornament < or_ferm) continue;

    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }

    /* The requirements for each ornament are subtly different. Rather than
    creating some kind of overall data table, I've gone for the option of
    writing them out in individual groups. */

    switch(orn->ornament)
      {
      case or_ferm:
      PO("<fermata");
      if (opposite)
        {
        PC(" type=\"inverted\" default-y=\"%d\"", T(-20000));
        }

      /* No render that I've tried pays any attention to this, nor to and
      default-{xy} settings. */

      if (orn->x != 0) PC(" relative-x=\"%d\"", T(orn->x));
      if (orn->y != 0) PC(" relative-y=\"%d\"", T(orn->y));

      PC("/>\n");
      break;

      case or_arp:
      PN("<arpeggiate/>");
      break;

      case or_arpu:
      PN("<arpeggiate direction=\"up\"/>");
      break;

      case or_arpd:
      PN("<arpeggiate direction=\"down\"/>");
      break;

      /* These are all the variations of accidentals above or below a note.
      They are defined in triples: without adornment, in round brackets, in
      square brackets. The latter two are treated the same here. */

// TODO: No renderer I've yet tried displays these correctly.

      default:
      int offset = orn->ornament - or_nat;   /* Offset into table */
      if (offset % 3 == 1) X(X_SQUARE_ACC);
      PO("<accidental-mark");
      if (offset % 3 != 0) PC(" bracket=\"yes\"");
      if (orn->ornament >= or_accbelow)
        {
        PC(" placement=\"below\"");
        offset -= (or_accbelow - or_nat);
        }
      else PC(" placement=\"above\"");
      PC(">%s</accidental-mark>\n", XML_accidental_names[offset/3]);
      break;
      }
    }

  /* The PMW directive [tremolo] is for tremolos between two notes. In MusicXML
  it is notated as a start/stop on the relevant notes, as a ornament. */

  if (add_tremolo != NULL || (stop_tremolo_pending != 0 && !inchord))
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }

    if (!ornaments_open)
      {
      PA("<ornaments>");
      ornaments_open = TRUE;
      }

    if (add_tremolo != NULL)
      {
      b_tremolostr *t = (b_tremolostr *)add_tremolo;
      PN("<tremolo type=\"start\">%d</tremolo>", t->count);
      if (t->join != 0) X(X_TREMJOIN);
      add_tremolo = NULL;  /* In case we are in a chord */
      stop_tremolo_pending = t->count;
      }

    else
      {
      PN("<tremolo type=\"stop\">%d</tremolo>", stop_tremolo_pending);
      stop_tremolo_pending = 0;
      }
    }

  /* Now a second pass for any <ornament> based ornaments. Once again, we just
  deal with them individually. */

  for (int i = 0; i < ornament_pending_count; i++)
    {
    b_ornamentstr *orn = ornament_pending[i];
    if (orn->ornament >= or_ferm) continue;

    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }

    if (!ornaments_open)
      {
      PA("<ornaments>");
      ornaments_open = TRUE;
      }

    switch(orn->ornament)
      {
      case or_tr:
      PN("<trill-mark/>");
      break;

      case or_trsh:
      PN("<trill-mark/>");
      PN("<accidental-mark placement=\"above\">sharp</accidental-mark>");
      break;

      case or_trfl:
      PN("<trill-mark/>");
      PN("<accidental-mark placement=\"above\">flat</accidental-mark>");
      break;

      case or_trnat:
      PN("<trill-mark/>");
      PN("<accidental-mark placement=\"above\">natural</accidental-mark>");
      break;

      case or_trem1:
      case or_trem2:
      case or_trem3:
      PN("<tremolo>%d</tremolo>", orn->ornament - or_trem1 + 1);
      break;

      case or_mord:
      PN("<mordent/>");
      break;

      case or_dmord:
      PN("<mordent long=\"yes\"/>");
      break;

      case or_imord:
      PN("<inverted-mordent/>");
      break;

      case or_dimord:
      PN("<inverted-mordent long=\"yes\"/>");
      break;

      case or_turn:
      PN("<turn/>");
      break;

      case or_iturn:
      PN("<turn slash=\"yes\"/>");
      break;

      case or_rturn:
      PN("<inverted-turn/>");
      break;

      case or_irturn:
      PN("<inverted-turn slash=\"yes\"/>");
      break;

      case or_spread:
      X(X_SPREAD);
      break;
      }
    }

  if (ornaments_open) PB("</ornaments>");

  /* Done ornaments; clear in case we are in a chord. */

  ornament_pending_count = 0;

  /* Handle tuplets. The start is indicated by one or more pending b_pletstr;
  for the end we have to peek ahead. Note that plet_level has already been
  incremented above. */

  if (plet_pending_count > 0)
    {
    for (int i = 0; i < plet_pending_count; i++)
      {
      uint32_t flags = plet_pending[i]->flags;
      const char *bracket = (beam_state >= 0)? "no" : "yes";
      const char *placement, *show;

      if ((flags & plet_bn) != 0) bracket = "no";
      if ((flags & plet_by) != 0) bracket = "yes";

      if ((flags & plet_a) != 0) placement = "above";
      else if ((flags & plet_b) != 0) placement = "below";
      else placement = ((note->flags & nf_stemup) == 0)? "above" : "below";

      if ((flags & plet_x) != 0 || !plet_enable)
        {
        bracket = "no";
        show = "none";
        }
      else show = "actual";

      if (!notations_open)
        {
        PA("<notations>");
        notations_open = TRUE;
        }

      /* For a top-level tuplet, the <time-modification> values (output above)
      are sufficient. */

      if (plet_level < 2)
        {
        PN("<tuplet number=\"%d\" type=\"start\" bracket=\"%s\" "
          "placement=\"%s\" show-number=\"%s\"/>",
          plet_level - plet_pending_count + i + 1, bracket, placement, show);
        }

      /* For a nested tuplet we have to specify what is to be printed. For
      example, if a triplet is nested within a triplet, the actual and normal
      values will be 3/2 for the outer triplet, and 9/4 for the inner, but we
      want the inner's displayed number to be 3. */

      else
        {
        PA("<tuplet number=\"%d\" type=\"start\" bracket=\"%s\" "
          "placement=\"%s\" show-number=\"%s\">",
          plet_level - plet_pending_count + i + 1, bracket, placement, show);
        PA("<tuplet-actual>");
          PN("<tuplet-number>%d</tuplet-number>",
            plet_actual[plet_level] / plet_actual[plet_level - 1]);
        PB("</tuplet-actual>");
        PB("</tuplet>");
        }
      }

    plet_pending_count = 0;
    }

  /* Look for plets ending before the next note. */

  else
    {
    for (barstr *bbn = (barstr *)bb->next; bbn != NULL;
         bbn = (barstr *)bbn->next)
      {
      if (bbn->type == b_note) break;
      if (bbn->type == b_endplet)
        {
        if (!notations_open)
          {
          PA("<notations>");
          notations_open = TRUE;
          }
        PN("<tuplet number=\"%d\" type=\"stop\"/>", plet_level--);
        }
      }
    }

  /* Handle the end of a glissando */

  if (stopgliss)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PN("<glissando type=\"stop\"/>");
    }

  /* Handle the start of a glissando */

  if (gliss_active)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PN("<glissando type=\"start\" line-type=\"solid\"/>");
    }

  /* Handle the end of a tie (the notation part - see also <tie> above). */

  if (stoptie && wastied)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PN("<tied type=\"stop\"/>");
    }

  /* Handle the start of a tie (the notation part - see also <tie> above). Skip
  if this note wasn't actually tied - can happen within a chord, but still
  reduce the abovecount. */

  if (tie_active != NULL && wastied)
    {
    const char *placement = (abovecount-- > 0)? "above" : "below";
    const char *line_type =
      ((tie_active->flags & tief_dashed) != 0)? " line-type=\"dashed\"" :
      ((tie_active->flags & tief_dotted) != 0)? " line-type=\"dotted\"" : "";

    if ((tie_active->flags & tief_editorial) != 0)
      comment("ignored editoral mark on tie");

    if ((tie_active->flags & tief_gliss) != 0)
      comment("gliss treated as tie pro tem");

    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }
    PN("<tied type=\"start\" placement=\"%s\"%s/>", placement, line_type);
    }
  else abovecount--;

  /* Deal with slurs. Any [slur] items before this note were put on a pending
  list. We can start them here, moving them to an active list. Then search for
  any [endslur] items before the next note or end of bar, and act on them. */

  if (slurs_pending_count > 0)
    {
    if (!notations_open)
      {
      PA("<notations>");
      notations_open = TRUE;
      }

    /* PMW slurs can have a single-letter ID (default NUL), whereas MusicXML
    slurs can have numbers (1-16). We resolve this by giving each slur the
    number of where it is in the active list (first one is 1), and keeping a
    translation. The number replaces the id in the [slur] item in order to make
    [xlur] handling straightforward. */

    for (int i = 0; i < slurs_pending_count; i++)
      {
      b_slurstr *s = slurs_pending[i];
      b_slurmodstr *mod = NULL;
      const char *line_type =
        ((s->flags & sflag_idot) != 0)? " line-type=\"dotted\"" :
        ((s->flags & sflag_i) != 0)? " line-type=\"dashed\"" : "";
      const char *placement = ((s->flags & sflag_b) != 0)? "below" : "above";

      if (s->id != 0)
        slurs_trans[slurs_trans_count++] =
          (s->id << 8) | (slurs_active_count + 1);
      s->id = slurs_active_count + 1;

      /* Start slur element, but leave open */

      PO("<slur type=\"start\" number=\"%d\"%s placement=\"%s\"",
        s->id, line_type, placement);

      /* See if there are any modifications that apply to the whole slur. There
      is no support (yet?) for modifying split slurs. */

      for (b_slurmodstr *mod2 = s->mods; mod2 != NULL; mod2 = mod2->next)
        {
        if (mod2->sequence != 0) X(X_SLURSPLITOPT);
          else mod = mod2;
        }

// TODO Neither MuseScore nor OSMD demo seem to pay any attention to values
// that modify a slur.

      if (mod != NULL)
        {
        if (mod->lx != 0) PC(" relative-x=\"%d\"", T(mod->lx));
        }

      /* Terminate the <slur> element. */

      PC("/>\n");

      /* If this was [xslur] and there is another slur active, put this one
      before it on the active list. Otherwise, just add to the list. */

      if ((s->flags & sflag_x) != 0 && slurs_active_count > 0)
        {
        slurs_active[slurs_active_count] = slurs_active[slurs_active_count-1];
        slurs_active[slurs_active_count-1] = s;
        }
      else slurs_active[slurs_active_count] = s;
      slurs_active_count++;
      }

    slurs_pending_count = 0;
    }

  /* Only look for [endslur]s when doing the first note of a chord. */

  if (!inchord && slurs_active_count > 0)
    {
    for (bb = (barstr *)bb->next; bb != NULL; bb = (barstr *)bb->next)
      {
      if (bb->type == b_note) break;
      if (bb->type == b_endslur)
        {
        b_endslurstr *e = (b_endslurstr *)bb;
        if (!notations_open)
          {
          PA("<notations>");
          notations_open = TRUE;
          }

        /* If there's no id, terminate the most recent slur. Note that we must
        use the remembered MusicXML number that was put into the starting slur
        structure, not the id in [endslur]. */

        if (e->value == 0)   /* No id */
          {
          b_slurstr *s = slurs_active[--slurs_active_count];
          PN("<slur type=\"stop\" number=\"%d\"/>", s->id);
          }

        /* Translate the PMW id into a slur number, then seek that slur in the
        list. */

        else
          {
          int i, n;

          for (i = 0; i < slurs_trans_count; i++)
            if ((slurs_trans[i] >> 8) == e->value) break;

          if (i >= slurs_trans_count)
            error (ERR192, "could not translate PMW slur id"); /* Hard */

          n = slurs_trans[i] & 0xff;

          for (i = 0; i < slurs_active_count; i++)
            {
            if (slurs_active[i]->id == n) break;
            }

          if (i >= slurs_active_count)
            {
            char buff[64];
            sprintf(buff, "could not find open slur with id=%d", n);
            error(ERR192, buff);   /* Hard */
            }

          PN("<slur type=\"stop\" number=\"%d\"/>", n);

          /* Remove the slur from the list, if not last. */

          if (i != --slurs_active_count)
            memmove(slurs_active + i, slurs_active + i + 1,
              (slurs_active_count - i) * sizeof(b_slurstr *));
          }

        /* When all slurs are closed, we can reset the translations. */

        if (slurs_active_count == 0) slurs_trans_count = 0;
        }
      }   /* End [endslur] loop */
    }     /* End [endslur] processing */

  /* Close <notations> if it's open. */

  if (notations_open) PB("</notations>");

  /* The last thing for a note is underlay or overlay, where each verse goes in
  its own <lyric> element. We have to handle continued syllables, which are
  represented by the string "=". */

  if (underlay_pending_count > 0)
    {
    char numberbuff[16] = {0};
    BOOL number = MX(mx_numberlyrics) || underlay_pending_count > 1;

    for (int i = 0; i < underlay_pending_count; i++)
      {
      b_textstr *t = underlay_pending[i];
      fontinststr *fdata = &xml_movt->fontsizes->fontsize_text[t->size];
      int y = T(t->y) + (((t->flags & text_above) == 0)? -44 : 4);

      if (number) sprintf(numberbuff, " number=\"%d\"", i + 1);

      /* If this string is just the single character '=' it is a continuation
      of the previous syllable. In order to decide whether to use "middle" or
      "stop" on an extender, we have to look forward to the next underlay
      syllable at this level. All very tedious. */

      if (t->laylen == 1 && PCHAR(t->string[0]) == '=')
        {
        if (underlay_state[i] == '-')
          {
          /* It seems there's nothing to do here. */
          }
        else if (underlay_state[i] == '=')
          {
          int barno = bar;
          const char *extend;
          b_textstr *tnext = t;

          /* Hopefully this will work with multiple verses. */

          for (int j = 0; j <= i && tnext != NULL; j++)
            tnext = nextulinstave((barstr *)tnext, &barno);

          if (tnext == NULL || tnext->laylen != 1 ||
              PCHAR(tnext->string[0]) != '=')
            {
            extend = "stop";
            underlay_state[i] = 0;
            }
          else extend = "continue";

          PA("<lyric%s>", numberbuff);
          PN("<extend type=\"%s\"/>", extend);
          PB("</lyric>");
          }
        }

      /* Not an "=" string, this is a real word or syllable. */

      else
        {
        int endchar = PCHAR(t->string[t->laylen]);
        const char *extend = NULL;
        const char *placement = ((t->flags & text_above) == 0)? "" :
          " placement=\"above\"";
        const char *justify = "";

        /* We need to handle the special characters # and ^ in underlsy
        strings. # can just be turned into a space. ^ at the start signifies
        left-justification; other instances are ignored (with a notification).
        Easiest way to do this is to make a copy of the string. */

        uint32_t ss[64];   /* Should be long enough */
        uint32_t len = 0;
        uint32_t j = 0;

        if (PCHAR(t->string[0]) == '^')
          {
          justify = " justify=\"left\"";
          j++;
          }

        for (; j < t->laylen; j++)
          {
          uint32_t c = t->string[j];
          if (PCHAR(c) == '^')
            {
            X(X_CIRCUMFLEX);
            continue;
            }
          if (PCHAR(c) == '#') c = PFTOP(c) | ' ';
          ss[len++] = c;
          }

        // TODO What about x?

        PA("<lyric%s default-y=\"%d\"%s%s>", numberbuff, y, placement, justify);
        PO("<syllabic>");

        /* Underlay state is '-': this is the next syllable of a word. */

        if (underlay_state[i] == '-')
          {
          if (endchar == '-')
            PC("middle");       /* Word continues */
          else
            {
            PC("end");
            if (endchar == '=')
              {
              extend = "start";
              underlay_state[i] = '=';
              }
            else underlay_state[i] = 0;
            }
          }

        /* We are starting a new word. */

        else
          {
          if (underlay_state[i] == '=')  // SHOULD NOT OCCUR
            error(ERR191, "bad underlay state \"=\" at start of word");

          if (endchar == '-')
            {
            PC("begin");
            underlay_state[i] = '-';
            }
          else
            {
            PC("single");
            if (endchar == '=')
              {
              extend = "start";
              underlay_state[i] = '=';
              }
            }
          }

        PC("</syllabic>\n");

        /* If a PMW underlay string starts with a hyphen, it delimits an empty
        syllable. The write_PMW_string() function won't output anything if its
        input is an empty string, but at least one picky MusicXML interpreter
        grumbles if <text> is missing. So set up a single space. */

        if (len == 0)
          {
          ss[0] = ' ' | (font_rm << 24);
          len = 1;
          }

        write_PMW_string(ss, len, fdata->size, "text", "", 0, INT32_MAX, NULL,
          NULL, 0);
        if (extend != NULL) PN("<extend type=\"%s\"/>", extend);
        PB("</lyric>");
        }
      }

    underlay_pending_count = 0;
    }

  /* That's the end of the note. */

  PB("</note>");

  /* If this was a single note or the last note of chord, we update the musical
  position, and also the horizontal position unless there is a [reset] before
  the next note, because in that event there may not be a recorded position at
  the next musical offset. */

  if (b->next->type != b_chord)
    {
    BOOL reset = FALSE;

    /* Check for [reset] */

    for (barstr *bn = (barstr *)b->next; bn != NULL; bn = (barstr *)bn->next)
      {
      if (bn->type == b_note) break;
      if (bn->type == b_reset)
        {
        reset = TRUE;
        break;
        }
      }

    /* Update musical position - needed even if [reset] because the reset
    amount is relative to the next position. */

    xml_moff += note->length;

    /* Unless [reset], update the horizontal position. */

    if (!reset)
      {
      while (xml_moff > xml_pos->moff && xml_pos < xml_poslast) xml_pos++;
      while (xml_moff < xml_pos->moff && xml_pos > xml_barpos->vector) xml_pos--;
      if (xml_pos->moff != xml_moff)
        {
        comment("fatal error");
        error(ERR192, "position data failure");   /* Hard */
        }
      xml_xoff = xml_pos->xoff;
      }

    /* End of single note or chord. */

    break;
    }

  /* More notes of a chord follow; update for next cycle. */

  b = (barstr *)(b->next);
  inchord = TRUE;
  }

return b;
}



/*************************************************
*              Write bar line (or not)           *
*************************************************/

/* We have to convert PMW's two values of barline type and barline style into
the single parameter for MusicXML's <bar-style> element. There isn't a
one-to-one match. We don't need a <barline> element for a normal barline.

Arguments:
  b           barline item
  finalbar    TRUE if this is the last bar
  end_ending  0 or the number of 1st/2nd time ending etc
  eetype      type of end_ending ("stop", "discontinue")

Returns:      nothing
*/

static void
write_barline(barstr *b, BOOL finalbar, int end_ending, const char *eetype)
{
const char *style = NULL;
b_barlinestr *bl = (b_barlinestr *)b;

if (bl->bartype == barline_normal)
  {
  switch(bl->barstyle)
    {
    case 0:   /* Normal */
    break;

    case 1:   /* Dashed */
    style = "dashed";
    break;

    case 4:   /* Short, middle of stave */
    style = "short";
    break;

    case 2:   /* Solid, between staves only */
    case 3:   /* Dashed, between staves only */
    case 5:   /* Stub outside stave */
    comment("bar line style %d is not supported", bl->barstyle);
    break;

    default:
    comment("unrecognized bar line style %d", bl->barstyle);
    break;
    }
  }

/* For non-normal types, only the normal style is supported. */

else
  {
  if (bl->barstyle != 0)
    comment("non-normal style ignored for non-normal barline type");
  switch(bl->bartype)
    {
    case barline_double: style = "light-light"; break;
    case barline_ending: style = "light-heavy"; break;
    case barline_invisible: style = "none"; break;
    default: comment("unrecognized barline type %d\n", bl->bartype);
    }
  }

if (finalbar && style == NULL && (xml_movt->flags & mf_unfinished) == 0)
  style = "light-heavy";

/* We only need to include a <barline> element if there is something
non-default because a normal barline is assumed at the end of a measure by
default. */

if (style != NULL || end_ending != 0)
  {
  PA("<barline>");
  if (end_ending != 0)
    PN("<ending type=\"%s\" number=\"%d\"/>", eetype, end_ending);
  if (style != NULL) PN("<bar-style>%s</bar-style>", style);
  PB("</barline>");
  }
}



/*************************************************
*               Handle text item                 *
*************************************************/

/* Underlay and overlay have to be saved up so they can be output using <lyric>
in the next <note>. Other text can be output here. For figured bass, we handle
several in succession.

Argument:  the barstr that is a textstr
Returns:   the last barstr used (may change for figured bass or follow-on text)
*/

static barstr *
handle_text(barstr *b)
{
b_textstr *t = (b_textstr *)b;
uint32_t flags = t->flags;
BOOL rehearse = ((flags & text_rehearse) != 0);
BOOL figbass = ((flags & text_fb) != 0);
fontinststr *fdata =
  rehearse? &xml_movt->fontsizes->fontsize_rehearse :
   figbass? &xml_movt->fontsizes->fontsize_text[ff_offset_fbass] :
            &xml_movt->fontsizes->fontsize_text[t->size];
int32_t size = fdata->size;

if ((flags & (text_absolute|text_atulevel|text_baralign|text_barcentre|
  text_followon|text_middle|text_timealign)) != 0) X(X_TEXT);

if ((flags & text_ul) != 0)
  {
  if (underlay_pending_count >= UNDERLAY_MAX)
    error(ERR191, "too many underlay strings on one note");
  else underlay_pending[underlay_pending_count++] = t;
  return b;
  }

/* Figured bass has its own element in MusicXML, which requires knowledge of
the figures and other marks (e.g. accidentals). In PMW figured base is just a
special kind of text, so handling this is complicated. */

if (figbass)
  {
  X(X_FIGBASS);

  element_set_font("figured-bass", PFONT(t->string[0]), size);
  PCA(">");

  /* Loop for a sequence of fb strings. In PMW they stack below each other, and
  luckily in MusicXML this is also the case. */

  for (;;)
    {
    BOOL done = FALSE;
    uint32_t *s = t->string;
    uint32_t ss[20];
    uint32_t len = 0;
    uint32_t *p;

    PA("<figure>");

    /* First, remove all the music font moving characters. */

    for (p = s; *p != 0; p++)
      {
      if (PBFONT(*p)  == font_mf)
        {
        uint32_t c = PCHAR(*p);
        if (c == ' ' || (c >= 118 && c <= 126) || (c >= 185 && c <= 188))
          continue;
        }
      if (len >= sizeof(ss) - 1)
        {
        error(ERR196);
        break;
        }
      ss[len++] = *p;
      }
    ss[len] = 0;

    /* Now process the characters in the string. If the first character is an
    accidental in the music font, set up a <prefix> element. */

    p = ss;
    if (PBFONT(*p) == font_mf)
      {
      switch(PCHAR(*p))
        {
        case 37:
        PN("<prefix>sharp</prefix>");
        p++;
        break;

        case 39:
        PN("<prefix>flat</prefix>");
        p++;
        break;

        case 40:
        PN("<prefix>natural</prefix>");
        p++;
        break;
        }
      }

    /* If we now have one of the Music font's special figured bass characters,
    output suitable code, which involves a suffix. Only one suffix is permitted
    in MusicXML, so nothing more is permitted. */

    if (PBFONT(*p) == font_mf)
      {
      switch(PCHAR(*p))
        {
        case 106:
        PN("<figure-number>7</figure-number>");
        PN("<suffix>slash</suffix>");
        done = TRUE;
        break;

        case 107:
        PN("<figure-number>4</figure-number>");
        PN("<suffix>plus</suffix>");
        done = TRUE;
        break;

        case 115:
        PN("<figure-number>6</figure-number>");
        PN("<suffix>back-slash</suffix>");
        done = TRUE;
        break;

        case 179:
        PN("<figure-number>5</figure-number>");
        PN("<suffix>plus</suffix>");
        done = TRUE;
        break;
        }

      if (done) p++;
      }

    /* Not a music font special figured bass character. */

    if (!done && *p != 0)
      {
      /* Output any non-music-font digits. Then check for a following "+"
      character, which is output as a suffix. */

      if (PBFONT(*p) != font_mf && isdigit(PCHAR(*p)))
        {
        PO("<figure-number>");
        do
          PC("%c", PCHAR(*p++));
        while (*p != 0 && PBFONT(*p) != font_mf && isdigit(PCHAR(*p)));
        PC("</figure-number>\n");

        if (*p != 0 && PBFONT(*p) != font_mf && PCHAR(*p) == '+')
          {
          PN("<suffix>plus</suffix>");
          p++;
          done = TRUE;
          }
        }

      /* If we haven't had a suffix, see if there is a following accidental in
      the music font. There is also a plus character in the music font which
      might be used. */

      if (!done && PBFONT(*p) == font_mf)
        {
        switch(PCHAR(*p))
          {
          case 37:
          PN("<suffix>sharp</suffix>");
          p++;
          break;

          case 39:
          PN("<suffix>flat</suffix>");
          p++;
          break;

          case 40:
          PN("<suffix>natural</suffix>");
          p++;
          break;

          case 135:
          PN("<suffix>plus</suffix>");
          p++;
          break;
          }
        }
      }

    /* End the figure and heck that we've used up all the string; warn if not.
    */

    PB("</figure>");
    if (*p != 0) error(ERR197, p);

    /* If the immediately following item is not another figured bass string,
    break the loop. Otherwise, advance and handle it. */

    if (b->next->type != b_text ||
       (((b_textstr *)(b->next))->flags & text_fb) == 0)
      break;

    b = (barstr *)b->next;
    t = (b_textstr *)b;
    }

  PB("</figured-bass>");
  return b;
  }

/* All other types of text come within <direction> */

const char *elname = rehearse? "rehearsal":"words";
int y = T(t->y) + (((flags & text_above) == 0)? -44 : 4);

PA("<direction>");
PA("<direction-type>");

/* If the text string consists of just character 0x63 or character 0x64 in the
music font, output a <segno> element because those are the two "dal segno"
characters. */

if (t->string[1] == 0 && PBFONT(t->string[0]) == font_mf &&
     (PCHAR(t->string[0]) == 0x63 || PCHAR(t->string[0]) == 0x64))
  {
  PN("<segno/>");
  }

/* Loop in case there is follow-on text */

else for (;;)
  {
  write_PMW_string(t->string, UINT_MAX, size, elname, "", 0, y,
    ((flags & (text_boxed|text_boxrounded)) != 0)? "rectangle" :
    ((flags & text_ringed) != 0)? "circle" : NULL,
    ((flags & text_centre) != 0)? "center" :
    ((flags & text_endalign) != 0)? "right" : "left",
    t->rotate);

  if (b->next->type != b_text ||
      (((b_textstr *)(b->next))->flags & text_followon) == 0)
    break;

  b = (barstr *)(b->next);
  t = (b_textstr *)b;
  }

PB("</direction-type>");
PB("</direction>");
return b;
}



/*************************************************
*             Handle start of a measure          *
*************************************************/

/* This is used at the start of all measures.

Argument: the absolute bar number (starts at 0)
Returns:  nothing
*/

static void
start_measure(int bar)
{
comment_bar = bar;
xml_barpos = xml_movt->posvector + bar;
xml_pos = xml_barpos->vector;
xml_poslast = xml_pos + xml_barpos->count - 1;
xml_moff = 0;
xml_xoff = 0;

/* If the movement had no [backup] or [reset] directives, we need not bother
with <voice> elements. Otherwise reset to voice 1 at the start of each bar and
zero the high water marks. */

if ((xml_movt->flags & mf_hadbackup) == 0)
  {
  xml_voice = 0;
  }
else
  {
  xml_voice = 1;
  memset(voice_hwm, 0, sizeof(voice_hwm));
  }

/* A PMW uncounted bar will have a non-zero fractional part or a zero
integer part. */

uint32_t pno = xml_movt->barvector[bar];
uint32_t pnofr = pno & 0xffff;
pno >>= 16;
const char *implicit = (pnofr != 0 || pno == 0)? " implicit=\"yes\"" : "";

/* The measure number given here is what is supposed to be shown when measure
numbering is enabled, though not all renderers seem to honour it. The number
attribute is documented as not necessarily being numeric. Therefore, we set it
to PMW's identification string. */

if (pnofr == 0)
  PA("<measure number=\"%d\" width=\"%d\"%s>", pno,
    T(xml_pos[xml_barpos->count - 1].xoff), implicit);
else
  PA("<measure number=\"%d.%d\" width=\"%d\"%s>", pno, pnofr,
    T(xml_pos[xml_barpos->count - 1].xoff), implicit);
}



/*************************************************
*   Special action for the first bar of a stave  *
*************************************************/

/* Take special action for the first bar of a stave. Search this bar to see if
any of clef, key, or time are specified before the first note. If not, default
the clef to treble, and use the movement's default key and time. If any of them
are found, remove that item from the chain. Process these three items, along
with the divisions setting. NOTE: the key in the movement structure is not
transposed, but any b_key items are already transposed. */

static void
first_measure(barstr *b, int divisions)
{
uint32_t clef = clef_treble;
uint32_t key = transpose_key(xml_movt->key);
uint32_t time = xml_movt->time;
uint32_t lines = xml_movt->stavetable[current_stave]->stavelines;
uint32_t size = xml_movt->stavesizes[current_stave];

for (; b != NULL; b = (barstr *)b->next)
  {
  BOOL stop = FALSE;
  BOOL remove = FALSE;

  switch(b->type)
    {
    case b_clef:
    clef = ((b_clefstr *)b)->clef;
    remove = TRUE;
    break;

    case b_key:
    key = ((b_keystr *)b)->key;
    remove = TRUE;
    break;

    case b_time:
    time = ((b_timestr *)b)->time;
    remove = TRUE;
    break;

    case b_note:
    stop = TRUE;
    break;
    }

  if (remove)
    {
    b->prev->next = b->next;
    b->next->prev = b->prev;
    }

  if (stop) break;
  }

/* Output divisions and any of clef, key, and time that are needed. */

PA("<attributes>");
PN("<divisions>%d</divisions>", divisions);
write_key(key, 0, FALSE);
write_time(time, (xml_movt->flags & mf_showtime) == 0 ||
                 (xml_movt->flags & mf_startnotime) != 0);
write_clef(clef, FALSE);
if (lines != 5 || size != 1000)
  {
  PA("<staff-details>");
  if (lines != 5) PN("<staff-lines>%d</staff-lines>", lines);
  if (size != 1000)
    PN("<staff-size scaling=\"%d\">%d</staff-size>", size/10, size/10);
  PB("</staff-details>");
  }
PB("</attributes>");
}



/*************************************************
*        Set up bar numbering as required        *
*************************************************/

/* This function is called for the first bar of the first stave. */

static void
set_barnumbering(void)
{
if ((xml_movt->barnumber_textflags & (text_boxed|text_ringed)) != 0)
  X(X_ENC_BARNO);

/* The numbering info is put into a <print> element. */

PA("<print>");

/* No bar numbering */

if (xml_movt->barnumber_interval == 0)
  PN("<measure-numbering>none</measure-numbering>");

/* Yes bar numbering */

else
  {
  element_set_font("measure-numbering", xml_movt->fonttype_barnumber,
    xml_movt->fontsizes->fontsize_barnumber.size);

  /* Bar numbers at start of systems */

  if (xml_movt->barnumber_interval < 0)
    PC(">system</measure-numbering>\n");

  /* Bar numbers every n bars: MusicXML does not support this, except for
  numbering every bar. */

  else
    {
    if (xml_movt->barnumber_interval != 1) X(X_BARNO_INTERVAL);
    PC(">measure</measure-numbering>\n");
    }
  }

PB("</print>");
}



/*************************************************
*        Handle the items in a bar (measure)     *
*************************************************/

/* This is called for all bars except the non-first bars in a multirest, after
first_measure() for bar 0, otherwise directly after start_measure(). It
normally returns 1, but a bigger number after a multirest.

Arguments:
  bar         the absolute bar number (starting at 0)
  divisions   divisions value

Returns:      number of bars processed
*/

static int
complete_measure(int bar, int divisions)
{
/* Handle suspension. This is not done by default. */

if (MX(mx_suspend) &&
    ((suspendmap[current_stave][bar/8] & (1<<(bar%8))) != 0) != xml_suspended)
  {
  PA("<attributes>");
  PN("<staff-details print-object=\"%s\"/>", xml_suspended? "yes" : "no");
  PB("</attributes>");
  xml_suspended = !xml_suspended;
  }

/* Handle a multirest bar */

if (xml_barpos->multi > 1)
  {
  barstr *b = (barstr *)(st->barindex[bar]->next);
  if (b->type != b_note)
    error(ERR193, "missing note in multirest bar"); /* Hard */

  uint32_t duration =
    (uint32_t)(((uint64_t)(((b_notestr *)b)->length) * divisions) /
      (uint64_t)len_crotchet);

  const char *cmr = ((xml_movt->flags & mf_codemultirests) == 0)? "" :
    " use-symbols=\"yes\"";
  PA("<attributes>");
  PA("<measure-style>");
  PN("<multiple-rest%s>%d</multiple-rest>", cmr, xml_barpos->multi);
  PB("</measure-style>");
  PB("</attributes>");

  /* Note that the value of "bar" starts at zero, but the number given to
  MusicXML bars starts at one. */

  for (int i = 1; i <= xml_barpos->multi; i++)
    {
    if (i != 1) PA("<measure number=\"%d\">", bar + i);
    PA("<note>");
    PN("<rest/>");
    PN("<duration>%d</duration>", duration);
    if (xml_voice != 0) PN("<voice>%d</voice>", xml_voice);
    PB("</note>");
    PB("</measure>");
    PN("%s", MEASURE_SEPARATOR);
    }

  return xml_barpos->multi;
  }

/* Handle non multirest bar */

int moff_hwm = xml_moff;   /* Will actually be zero */
barstr *bnext = (st->barcount > bar + 1)? st->barindex[bar + 1] : NULL;

for (barstr *b = st->barindex[bar]; b != NULL; b = (barstr *)b->next)
  {
  switch(b->type)
    {
    case b_start:
    break;


    /* --------------------------------------------------------*/
    /* These larger items are farmed out to separate functions */

    case b_note:   /* Changes b if it's a chord */
    b = write_note(b, bar, divisions);
    break;

    case b_text:
    b = handle_text(b);   /* Changes b for multiple figured bass */
    break;


    /* --------------------------------------------------------*/
    /* These are entirely handled from within write_note(). */

    case b_beambreak:
    case b_caesura:
    case b_comma:
    case b_endslur:
    case b_endplet:
    case b_overbeam:
    case b_tick:
    case b_tie:
    case b_tremolo:
    break;


    /* --------------------------------------------------------*/
    /* These can be ignored because when support for suspension is enabled,
    lists of suspended bars are created for each stave from the systems on
    output pages, and they are consulted when each bar is output. */

    case b_resume:
    if (!MX(mx_suspend)) X(X_SUSPEND);
    break;

    case b_suspend:
    if (!MX(mx_suspend)) X(X_SUSPEND);
    break;

    /* --------------------------------------------------------*/
    /* These are partially or wholly handled here. */

    case b_all:  /* Actually handled in lookahead in b_barline below. */
    break;

    case b_barline:
    int end_ending = 0;
    const char *end_ending_type = NULL;

    /* Deal with the end of a 1st or 2nd (etc) time bar. */

    if (ending_active != 0)
      {
      if (bnext == NULL) end_ending = ending_active; else
        {
        for (barstr *bx = bnext; bx != NULL; bx = (barstr *)bx->next)
          {
          if (bx->type == b_note) break;
          if (bx->type == b_all)
            {
            end_ending_type = "discontinue";
            end_ending = ending_active;
            break;
            }
          if (bx->type == b_nbar)
            {
            end_ending_type = "stop";
            end_ending = ending_active;
            break;
            }
          }
        }
      if (end_ending != 0) ending_active = 0;
      }

    /* If we've had a b_reset, the current musical position may be less than
    the high water mark recorded, for example if there's a full bar, reset,
    then only half a bar. Picky MusicXML interpreters grumble if a barline's
    position doesn't agree with the musical position, so we generate a
    <forward> element in this event, with a <voice> setting it is non zero. If
    there's never been a b_reset, moff_hwm will be zero.

    As in the case of b_reset, there's an overflow trap here because
    "divisions" may be quite large, so just multiplying the amount by it is a
    bad plan (found by experience). We do not want to lose any precision, so do
    things the hard way. */

    if (xml_moff < moff_hwm)
      {
      uint32_t forwardby = moff_hwm - xml_moff;
      forwardby = (forwardby / len_crotchet) * divisions +
        ((forwardby % len_crotchet) * divisions)/len_crotchet;
      PA("<forward>");
      PN("<duration>%d</duration>", forwardby);
      if (xml_voice != 0) PN("<voice>%d</voice>", xml_voice);
      PB("</forward>");
      }

    write_barline(b, bar == st->barcount-1, end_ending, end_ending_type);
    break;

    case b_bowing:
    bowingabove = ((b_bowingstr *)b)->value;
    break;

    case b_chord:
    comment("b_chord encountered at top level: ERROR!");
    break;

    case b_clef:
    PA("<attributes>");
    write_clef(((b_clefstr *)b)->clef, ((b_clefstr *)b)->assume);
    PB("</attributes>");
    break;

    case b_dotbar:
    PA("<barline location=\"middle\">");
    PN("<bar-style>dashed</bar-style>");
    PB("</barline>");
    break;

    case b_draw:
    X(X_DRAW);
    break;

    case b_endline:
    line_end((b_bytevaluestr *)b);
    break;

    /* It appears that the use of multiple fonts within a footnote is not
    possible in MusicXML. Force the whole string to be in the initial font. It
    also seems necessary to include an empty "words" string within
    "direction-type" in order to satisfy the schema. */

    case b_footnote:
    headstr *hd = &(((b_footnotestr *)b)->h);
    uint32_t fnfont = PFTOP(hd->string[0][0]);

    for (uint32_t *s = hd->string[0]; *s != 0; s++)
      {
      if (PFTOP(*s) != fnfont)
        {
        *s = (*s & 0x00ffffffu) | fnfont;
        X(X_FOOTNOTEFONT);
        }
      }

    PA("<direction>");
    PA("<direction-type>");
    PN("<words/>");
    PB("</direction-type>");
    write_PMW_string(hd->string[0], UINT_MAX, hd->fdata.size,
      "footnote", "", 0, 0, NULL, NULL, 0);
    PB("</direction>");
    break;

    case b_hairpin:
    b_hairpinstr *h = (b_hairpinstr *)b;
    PA("<direction placement=\"%s\">", ((h->flags & hp_below) == 0)?
      "above":"below");
    PA("<direction-type>");

    if ((h->flags & hp_end) == 0)
      {
      PO("<wedge type=");
      if ((h->flags & hp_cresc) != 0)
        PC("\"crescendo\"");
      else
        PC("\"diminuendo\" spread=\"%d\"", T(h->width));
      }
    else
      {
      PO("<wedge type=\"stop\"");
      if ((h->flags & hp_cresc) != 0)
      PC(" spread=\"%d\"", T(h->width));
      }

    PC("/>\n");
    PB("</direction-type>");
    PB("</direction>");
    break;

    /* A key value > 128 is a cancelling key, always followed immediately by
    the new key. */

    case b_key:
    PA("<attributes>");
    if (((b_keystr *)b)->key >= 128)
      {
      int oldkey = ((b_keystr *)b)->key;
      b = (barstr *)b->next;
      write_key(((b_keystr *)b)->key, oldkey, ((b_keystr *)b)->assume);
      }
    else
      {
      write_key(((b_keystr *)b)->key, 0, ((b_keystr *)b)->assume);
      }
    PB("</attributes>");
    break;

    case b_lrepeat:
    PA("<barline location=\"%s\">", (xml_moff == 0)? "left" : "middle");
    PN("<repeat direction=\"forward\"/>");
    PB("</barline>");
    break;

    case b_nbar:
    b_nbarstr *nb = (b_nbarstr *)b;
    PA("<barline location=\"left\">");
    element_set_font("ending", xml_movt->fonttype_repeatbar,
      xml_movt->fontsizes->fontsize_repno.size);
    PC(" type=\"start\" number=\"%d\">", nb->n);
    if (nb->s != NULL)
      PC("%s", convert_PMW_string(nb->s));
    else
      PC("%d", nb->n);
    PC("</ending>\n");
    PB("</barline>");
    ending_active = nb->n;
    break;

    case b_newline:
    if (MX(mx_newline)) PN("<print new-system=\"yes\"/>"); else X(X_NEWLINE);
    break;

    case b_newpage:
    if (MX(mx_newpage)) PN("<print new-page=\"yes\"/>"); else X(X_NEWPAGE);
    break;

    case b_ornament:
    if (ornament_pending_count >= ORNAMENT_MAX)
      error(ERR191, "too many ornaments on one note");
    else
      ornament_pending[ornament_pending_count++] = (b_ornamentstr *)b;
    break;

    case b_plet:
    plet_pending[plet_pending_count++] = (b_pletstr *)b;
    break;

    /* There's an overflow trap here because "divisions" may be quite large, so
    just multiplying the amount by it is a bad plan (found by experience). We
    do not want to lose any precision, so do things the hard way. */

    case b_reset:
    uint32_t backby = xml_moff - ((b_resetstr *)b)->moff;
    backby = (backby / len_crotchet) * divisions +
      ((backby % len_crotchet) * divisions)/len_crotchet;
    PA("<backup>");
    PN("<duration>%d</duration>", backby);
    PB("</backup>");

    /* Keep overall high water mark for all voices. */

    if (xml_moff > moff_hwm) moff_hwm = xml_moff;  /* Keep high water mark */

    /* The value of xml_voice should not be zero in a movement that contains
    b_reset. First save the high water mark for this voice, then move back and
    look for a voice whose high water mark is less than or equal to the new
    position. */

    if (xml_voice == 0)
      error(ERR193, "voice is zero in a [backup] or [reset] movement");  /* Hard */

    voice_hwm[xml_voice] = xml_moff;
    xml_moff = ((b_resetstr *)b)->moff;

    for (xml_voice = 1; xml_voice < VOICE_MAX; xml_voice++)
      {
      if (voice_hwm[xml_voice] <= xml_moff) break;
      }

    if (xml_voice >= VOICE_MAX)
      error(ERR193, "can't find valid voice after [backup] or [reset]"); /* Hard */

// TODO? xml_xoff is not being reset, but neither is it actually used at the
// moment.

    break;

    case b_rrepeat:
    PA("<barline location=\"%s\">", (b->next->type == b_barline)?
      "right" : "middle" );
    PN("<repeat direction=\"backward\"/>");
    PB("</barline>");
    break;

    /* In MusicXML, slur control is within <note>, but lines are outside notes,
    at top level. This means they each have to be handled separately. */

    case b_slur:
    if ((((b_slurstr *)b)->flags & sflag_l) != 0)
      {
      if (lines_active_count >= SLURS_MAX)
        error(ERR191, "too many nested lines");
      else line_start((b_slurstr *)b);
      }
    else if (slurs_pending_count >= SLURS_MAX)
      error(ERR191, "too many nested slurs");
    else
      {
      b_slurstr *s = (b_slurstr *)b;
      slurs_pending[slurs_pending_count++] = s;
      if ((s->flags & (sflag_w|sflag_h|sflag_e|sflag_lay)) != 0)
        X(X_SLUROPT);
      }
    break;

    case b_time:
    PA("<attributes>");
    write_time(((b_timestr *)b)->time, ((b_timestr *)b)->assume ||
     (xml_movt->flags & mf_showtime) == 0);
    PB("</attributes>");
    break;

    case b_transpose:
    active_transpose = ((b_transposestr *)b)->transpose;
    active_transpose_letter = ((b_transposestr *)b)->transpose_letter;
    break;

    case b_tripsw:
    plet_enable = ((b_tripswstr *)b)->value;
    break;


    /* --------------------------------------------------------*/
    /* These are currently not supported */

    case b_accentmove:
    X(X_ACCENTMOVE);
    break;

    /* With some difficulty it *might* be possible to handle [barnumber]. In
    MusicXML, a sequence such as

      <print>
        <measure-numbering>measure</measure-numbering>
      </print>

    may appear within a measure, but only one interpreter I've tried takes any
    notice when it's not in the first measure. Also, this interpreter applies
    the setting to the *following* bar(s). So, to implement [barnumber] there
    would have to be some kind of pre-pass to discover which bars need special
    handling for turning this on and off. */

    case b_barnum:
    X(X_BARNUMBER);
    break;

    case b_beamacc:
    X(X_BEAMACCRIT);
    break;

    case b_beammove:
    X(X_BEAMMOVESLOPE);
    break;

    case b_beamrit:
    X(X_BEAMACCRIT);
    break;

    case b_beamslope:
    X(X_BEAMMOVESLOPE);
    break;

    case b_breakbarline:
    X(X_BREAKBARLINE);
    break;

    case b_dotright:
    X(X_DOTRIGHT);
    break;

    case b_ens:
    X(X_NS);
    break;

    case b_ensure:
    X(X_ENSURE);
    break;

    case b_justify:
    X(X_JUSTIFY);
    break;

    case b_midichange:
    X(X_MIDI);
    break;

    case b_move:
    X(X_MOVE);
    break;

    case b_name:
    X(X_NAME);
    break;

    case b_notes:
    X(X_NOTES);
    break;

    case b_ns:
    X(X_NS);
    break;

    case b_nsm:
    X(X_NS);
    break;

    case b_olevel:
    X(X_OLEVEL);
    break;

    case b_olhere:
    X(X_OLEVEL);
    break;

    case b_page:
    comment("ignored [page]");
    break;

    case b_pagebotmargin:
    X(X_MARGIN);
    break;

    case b_pagetopmargin:
    X(X_MARGIN);
    break;

    case b_sgabove:
    comment("ignored [sgabove]");
    break;

    case b_sghere:
    comment("ignored [sghere]");
    break;

    case b_sgnext:
    comment("ignored [sgnext]");
    break;

    case b_slurgap:
    comment("ignored [slurgap]");
    break;

    case b_linegap:
    comment("ignored [linegap]");
    break;

    case b_space:
    X(X_MOVE);
    break;

    case b_ssabove:
    comment("ignored [ssabove]");
    break;

    case b_sshere:
    comment("ignored [sshere]");
    break;

    case b_ssnext:
    comment("ignored [ssnext]");
    break;

    case b_ulevel:
    comment("ignored [ulevel]");
    break;

    case b_ulhere:
    comment("ignored [ulhere]");
    break;

    case b_unbreakbarline:
    X(X_BREAKBARLINE);
    break;

    case b_zerocopy:
    comment("ignored [copyzero]");
    break;

    default:
    comment("ignored unknown b_type %d", b->type);
    break;
    }
  }

PB("</measure>");
PN("%s", MEASURE_SEPARATOR);
return 1;
}



/*************************************************
*         Output comments about ignorances       *
*************************************************/

/* There is one set of "ignored" bits that may be set by multiple calls to
outxml_write() or outxml_check_directive() below, for multiple movements. There
is also a list of ignored header directives. When all are done, this function
is called to output a relevant comment.

Argument: the bits
Returns:  nothing
*/

void
outxml_write_ignored(void)
{
if (X_ignored == 0 && dirs_ignored_count == 0) return;

fprintf(stderr,
  "\nSome PMW items cannot be translated to MusicXML. A few items that are ignored\n"
  "by default can be requested by a -x option. Items in this file that were wholly\n"
  "or partially ignored while generating XML output are listed below. This is\n"
  "probably not a complete list:\n\n");

for (int i = 0; i < X_COUNT; i++)
  {
  if ((X_ignored & 1l << i) != 0)
    fprintf(stderr, "  %s\n", X_ignored_message[i]);
  }

for (int i = 0; i < dirs_ignored_count; i++)
  fprintf(stderr, "  Header directive \"%s\"\n", nondirs[dirs_ignored[i]]);

fprintf(stderr, "\n");
}



/*************************************************
*          Check for unsupported directive       *
*************************************************/

/* There are plenty of PMW directives that are not supported in MusicXML
output. This function is called for every header directive so that warning bits
can be set. Stave directives affect the contents of staves, which are
independently checked.

Argument: the header directive
Returns:  nothing
*/

void
outxml_check_directive(const char *s)
{
int first = 0;
int last = nondirsize - 1;
while (last > first)
  {
  int mid = (first + last)/2;
  int c = strcmp(s, nondirs[mid]);
  if (c == 0)
    {
    dirs_ignored[dirs_ignored_count++] = mid;
    break;
    }
  if (c > 0) first = mid + 1; else last = mid;
  }
}



/*************************************************
*             Set brace/bracket join bits        *
*************************************************/

/* This function sets start/stop bits for braces and brackets in a per-stave
vector. As the default for bracket is 1-63, we need to check the actual highest
stave, and in any case a user may create a value that is greater than the
actual. We must also take note of stave selection so as not to start or end a
group on an unselected stave. */

static void
set_bracebrack(uint8_t *joinbits, stavelist *s, usint startbit, usint stopbit)
{
for (; s != NULL; s = s->next)
  {
  uint32_t first = s->first;
  uint32_t last = (s->last <= (uint32_t)xml_movt->laststave)?
    s->last : (uint32_t)xml_movt->laststave;

  while (first < last && mac_notbit(xml_staves, first)) first++;
  while (last > first && mac_notbit(xml_staves, last)) last--;

  if (first < last)  /* Omit single-stave groups */
    {
    joinbits[first] |= startbit;
    joinbits[last] |= stopbit;
    }
  }
}



/*************************************************
*          Initialize static variables           *
*************************************************/

/* Needed to ensure they have the right values at the start of a new movement,
just in case the previoue movement left them in the wrong state. Also
initialize the suspend maps when support for suspension is enabled. */

static void
initialize(void)
{
pagestr *p;
sysblock *s = NULL;

beam_state = -1;
bowingabove = TRUE;
comment_bar = 0;
current_stave = 0;
ending_active = 0;
gliss_active = FALSE;
indent = 0;
lines_active_count = 0;
lines_trans_count = 0;
ornament_pending_count = 0;
plet_enable = TRUE;
plet_level = 0;
plet_pending_count = 0;
slurs_active_count = 0;
slurs_pending_count = 0;
slurs_trans_count = 0;
stop_tremolo_pending = 0;
tie_active = NULL;
underlay_pending_count = 0;

/* That's all we need to do unless support for suspension is enabled. */

if (!MX(mx_suspend)) return;

/* If suspendmap[1] is NULL we are processing the first (or only) movement.
Find the largest number of bars and the maximum number of staves in any
movement. Then allocate suitable memory for the suspend maps. */

if (suspendmap[1] == NULL)
  {
  int maxbars = 0;
  int maxstave = 0;

  for (usint i = 0; i < movement_count; i++)
    {
    movtstr *m = movements[i];
    if (m->barcount > maxbars) maxbars = m->barcount;
    if (m->laststave > maxstave) maxstave = m->laststave;
    }

  size_t size = (maxbars + 7) / 8;

  for (int i = 1; i <= maxstave; i++) suspendmap[i] = mem_get(size);
  }

/* Initialize the suspend maps for the current movement. First clear to zero,
then scan all the output pages and set bits for all bars in systems where
staves are suspended. */

for (int i = 1; i <= xml_movt->laststave; i++)
  memset(suspendmap[i], 0, (xml_movt->barcount + 7) / 8);

/* Scan all the pages, looking for the first one that starts this movement. */

for (p = main_pageanchor; p != NULL; p = p->next)
  {
  for (s = p->sysblocks; s != NULL; s = s->next)
    if (s->movt == xml_movt) break;
  if (s != NULL) break;
  }

if (s == NULL) error(ERR192, "did not find movement in page data"); /* Hard */

/* We should now have p pointing to the first page that contains some of this
movement, and s pointing to the first sysblock or headblock. Scan for sysblocks
and for each extract any suspend data. Then move on to the next page, until we
hit a sysblock/headblock that does not belong to this movement. */

for (;;)
  {
  for (; s != NULL; s = s->next)
    {
    if (s->movt != xml_movt) break;
    if (!s->is_sysblock) continue;
    uint64_t suspend = (~s->notsuspend) >> 1;  /* Bottom bit is stave 1 */

    if (suspend == 0) continue;

    for (int i = 1;
         i <= xml_movt->laststave && suspend != 0;
         i++, suspend = suspend >> 1)
      {
      if ((suspend & 1) != 0)
        {
        uint8_t *map = suspendmap[i];
        for (int j = s->barstart; j <= s->barend; j++)
          map[j/8] |= 1 << (j%8);
        }
      }
    }

  /* Move on to the next page; break if it's another movement. */

  p = p->next;
  if (p == NULL) break;
  s = p-> sysblocks;
  if (s->movt != xml_movt) break;
  }

/********* This was debugging code, left here just in case it is ever needed
again.

for (int i = 1; i <= xml_movt->laststave; i++)
  {
  uint8_t *map = suspendmap[i];
  eprintf("Stave %d:", i);

  for (int j = 0; j < (xml_movt->barcount + 7) / 8; j++)
    eprintf(" 0x%02x", map[j]);
  eprintf("\n");
  }
***************************/
}


/*************************************************
*                Write MusicXML file             *
*************************************************/

/* This is the only external entry to this set of functions. The data is all in
memory and global variables. Writing a MusicXML file is triggered by the use of
the -musicxml or -xml command line option, which sets outxml_filename non-NULL.

Argument:   TRUE if a <movement-number> element is wanted
Returns:    bits for ignored things
*/

void
outxml_write(BOOL include_movement_number)
{
char datebuff[100];
uint32_t laststave;
uint32_t lcr_positions[3];
time_t now;

TRACE("outxml_write() movement %d\n", outxml_movement);

/* Safety check in case the X_ numbers get too big. */

if (X_COUNT > 63) error(192, "X_COUNT is greater than 63"); /* Hard */

/* Set up a pointer to the required movement. */

if (outxml_movement < 1 || (usint)outxml_movement > movement_count)
  {
  error(ERR189, outxml_movement, "MusicXML");
  return;
  }

xml_movt = movements[outxml_movement - 1];
if (xml_movt->barcount < 1)
  {
  error(ERR160, xml_movt->number, "MusicXML");
  return;
  }

/* Set warning flags for various unsupported things. Note that flags for
unsupported header directives are set via xmlout_check_directive() above. */

if (xml_movt->joinlist->first != 1 ||
    xml_movt->joinlist->last != MAX_STAVE ||
    xml_movt->joindottedlist != NULL)
  X(X_JOIN);

/* Set the warning flag if any staves are suspended from the start when
suspension support is not enabled (there may not be a [resume]). */

if (xml_movt->suspend_staves != 0 && !MX(mx_suspend)) X(X_SUSPEND);

/* PMW's alternative half sharp doesn't seem to be available in MusicXML, but
there is a variant half flat. */

if (xml_movt->halfflatstyle != 0) XML_accidental_names[4] = "quarter-flat";
if (xml_movt->halfsharpstyle != 0) X(X_HALFSHARP);

/* Initialize static variables because this function may be called several
times for multiple movements. */

initialize();

/* Stave selection is the movement's stave selection. Currently there's no way
of changing this. */

xml_staves = xml_movt->select_staves;
laststave = xml_movt->laststave;

/* Open the output file */

xml_file = Ufopen(outxml_filename, "w");
if (xml_file == NULL) error(ERR23, outxml_filename, strerror(errno));  /* Hard */

/* Write header boilerplate, followed by the identification element. */

now = time(NULL);
strftime(datebuff, sizeof(datebuff), "%Y-%m-%d", localtime(&now));

PA("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\""
  " \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
  "<score-partwise version=\"3.1\">");

if (include_movement_number)
  PN("<movement-number>%d</movement-number>", outxml_movement);

/* If the momvement has a first heading that has a centered part, use that as
the movement title. All font information in the string is ignored. */

if (xml_movt->heading != NULL)
  {
  headstr *h = xml_movt->heading;
  if (h->string[1] != NULL && h->string[1][0] != 0)
    PN("<movement-title>%s</movement-title>", convert_PMW_string(h->string[1]));
  }

PA("<identification>");
PA("<encoding>");

/* For testing we put in a fixed date so that comparisons work. It has to be
a real date because otherwise some software complains. */

if (main_testing == 0)
  {
  PN("<software>PMW %s</software>", PMW_VERSION);
  PN("<encoding-date>%s</encoding-date>", datebuff);
  }
else
  {
  PN("<software>PMW</software>");
  PN("<encoding-date>2025-01-01</encoding-date>");
  }

PN("<supports element=\"accidental\" type=\"yes\"/>");
PN("<supports element=\"beam\" type=\"yes\"/>");
PN("<supports element=\"stem\" type=\"yes\"/>");

PN("<supports attribute=\"new-system\" element=\"print\" type=\"yes\""
   " value=\"yes\"/>");
PN("<supports attribute=\"new-page\" element=\"print\" type=\"yes\""
  " value=\"yes\"/>");

PB("</encoding>");
PB("</identification>");

/* Next come various defaults */

PA("<defaults>");

/* The MusicXML staff is set at 40 tenths and the millimeters item specifies
how many millimeters this should scale to. We set this value from the PMW
magnification, knowing that the unmagnified PMW staff is 16 points (16*0.3528
millimeters). */

PA("<scaling>");
PN("<millimeters>%.4f</millimeters>",
  16.0*0.3528*(double)main_magnification/1000.0);
PN("<tenths>40</tenths>");
PB("</scaling>");

/* Set page size and margins. Note that PMW's pagelength and linelength are not
scaled by the magnification, but MusicXML tenths are scaled. If P is the
PMW pagelength in millipoints, P/1000 is the length in points. The
magnification is in milliunits, so to get the unmagnified pagelength we need
(P/1000) divided by (magnification/1000) which simplifies to P/magnification.
To convert points to tenths we divide by 16 and multiply by 40 (the respective
staff heights), which simplfies to 5/2. */

int converted_pagelength = (main_pagelength * 5)/(main_magnification * 2);
int converted_sheetdepth = (main_sheetdepth * 5)/(main_magnification * 2);
int vmargin = (converted_sheetdepth - converted_pagelength)/2;

int converted_linelength = (xml_movt->linelength * 5)/(main_magnification * 2);
int converted_sheetwidth = (main_sheetwidth * 5)/(main_magnification * 2);
int hmargin = (converted_sheetwidth - converted_linelength)/2;

PA("<page-layout>");
PN("<page-height>%d</page-height>", converted_pagelength);
PN("<page-width>%d</page-width>", converted_linelength);

PA("<page-margins type=\"both\">");
PN("<left-margin>%d</left-margin>", hmargin);
PN("<right-margin>%d</right-margin>", hmargin);
PN("<top-margin>%d</top-margin>", vmargin);
PN("<bottom-margin>%d</bottom-margin>", vmargin);
PB("</page-margins>");
PB("</page-layout>");

/* Set positions for left/centre/right for headings and footings. */

lcr_positions[0] = hmargin;
lcr_positions[1] = converted_linelength/2;
lcr_positions[2] = converted_linelength;

/* PMW measures system gap from bottom of last stave to bottom of top stave;
MusicXML measures from bottom to top. PMW supports only lefthand system
separators; MusicXML doesn't seem to allow for defining separator
characteristics. */

PA("<system-layout>");
PN("<system-distance>%d</system-distance>", T(xml_movt->systemgap) - 40);

if (xml_movt->systemseplength != 0)
  {
  PA("<system-dividers>");
    PN("<left-divider print-object=\"yes\"/>");
    PN("<right-divider print-object=\"no\"/>");
  PB("</system-dividers>");
  }
PB("</system-layout>");

/* PMW stave spacing gives the distance *below* a stave, with stave ensure a
minimum for above. MusicXML staff-distance is a measure from above, bottom to
top. The value for the top staff is ignored. Hard to know exactly what to do
here; for the moment, just set values for stave 2 onwards. */

for (usint stave = 2; stave <= laststave; stave++)
  {
  PA("<staff-layout number=\"%d\">", stave);
  PN("<staff-distance>%d</staff-distance>",
    T(xml_movt->stave_spacing[stave - 1]) - 40);
  PB("</staff-layout>");
  }

// TODO Any need for appearance?
// TODO Any need for music-font?

// TODO May need to set word-font and lyric-font

PB("</defaults>");

/* Try to do something with headings and footings. Different apps do different
things with these strings. For example, MuseScore needs default-y for it to
show them in a sensible place; Lilypond does not.*/

BOOL had_heading = FALSE;

if (xml_movt->heading != NULL)
  {
  uint32_t y = converted_pagelength - vmargin;
  int heading_count = 0;

  for (headstr *h = xml_movt->heading; h != NULL; h = h->next)
    {
    if (h->drawing != NULL) X(X_DRAW);

    for (int j = 0; j < 3; j++)
      {
      int i = lcr_order[j];   /* Do it in centre, left, right order */
      if (h->string[i] != NULL && h->string[i][0] != 0)
        {
        char attrbuff[80];
        sprintf(attrbuff, " default-x=\"%d\" default-y=\"%d\"",
          lcr_positions[i], y);
        PA("<credit page=\"1\">");
        if (heading_count == 0) PN("<credit-type>title</credit-type>");
          else if (heading_count == 1) PN("<credit-type>subtitle</credit-type>");
        heading_count++;
        write_PMW_string(h->string[i], UINT_MAX, h->fdata.size,
          "credit-words", attrbuff, 0, INT32_MAX, NULL, leftcenterright[i], 0);
        PB("</credit>");
        had_heading = TRUE;
        X(X_HEADING);
        }
      }

    y -= (h->space * 5) / (main_magnification * 2);
    }
  }

/* Something similar for footings. A credit type is required, at least for the
first one. Use "rights" because that's interpreted as wanted at least by some
renderers. */

if (xml_movt->footing != NULL)
  {
  int footing_count = 0;
  uint32_t y = vmargin;

  for (headstr *h = xml_movt->footing; h!= NULL; h = h->next)
    {
    char attrbuff[80];

    if (h->drawing != NULL) X(X_DRAW);

    sprintf(attrbuff, " valign=\"bottom\" default-y=\"%d\"", y);

    for (int j = 0; j < 3; j++)
      {
      int i = lcr_order[j];   /* Do it in centre, left, right order */

      if (h->string[i] != NULL && h->string[i][0] != 0)
        {
        PA("<credit page=\"1\">");
        if (footing_count++ == 0) PN("<credit-type>rights</credit-type>");
        write_PMW_string(h->string[i], UINT_MAX, h->fdata.size,
          "credit-words", attrbuff, 0, INT32_MAX, NULL, leftcenterright[i], 0);
        PB("</credit>");
        }
      }

    y -= (h->space * 5) / (main_magnification * 2);
    }

  X(X_HEADING);
  }

  /* If there were no footings, but we did output at least one heading,
  generate an empty footing, because this stops at least one renderer showing
  one heading as a footing. */

else if (had_heading)
  {
  PA("<credit page=\"1\">");
  PN("<credit-type>rights</credit-type>");
  PN("<credit-words valign=\"bottom\"/>");
  PB("</credit>");
  }

/* Before we can output a list of parts, we have to analyse PMW's bracket,
brace, and breakbarlines data because MusicXML has this data in the part list.
There doesn't seem to be any equivalent of PMW's join and joindotted, and we
amalgamate bracket with thinbracket.

Build a vector of bits for each stave indicating start/stop of these various
characteristics. */

#define jb_brace_start   0x01u
#define jb_brace_stop    0x02u
#define jb_bracket_start 0x04u
#define jb_bracket_stop  0x08u
#define jb_barline_start 0x10u
#define jb_barline_stop  0x20u
#define jb_break_start   0x40u
#define jb_break_stop    0x80u

uint8_t joinbits[MAX_STAVE + 1];
uint64_t breaks = xml_movt->breakbarlines;

for (usint stave = 0; stave <= laststave; stave++) joinbits[stave] = 0;

/* Find runs of staves that either have or have not barlines. */

for (usint stave = 1; stave <= laststave - 1;)
  {
  usint ss;
  usint next;
  uint64_t first = (breaks >> stave) & 1;

  for (ss = stave + 1; ss <= laststave; ss++)
    if (((breaks >> ss) & 1) != first) break;
  if (ss > laststave) ss = laststave;

  next = ss;  /* Where to continue */

  /* We must now take note of any stave selection and adjust the start and
  end of the run if necessary so as not to be on omitted staves. If we end up
  with just one stave, do not set any joining bits. */

  while (stave < ss && mac_notbit(xml_staves, stave)) stave++;
  while (ss > stave && mac_notbit(xml_staves, ss)) ss--;

  if (stave < ss)
    {
    if (first != 0)
      {
      joinbits[stave] |= jb_break_start;
      joinbits[ss] |= jb_break_stop;
      }
    else
      {
      joinbits[stave] |= jb_barline_start;
      joinbits[ss] |= jb_barline_stop;
      }
    }

  /* Advance for another group */

  stave = next;
  }

/* Set up braces and brackets. */

set_bracebrack(joinbits, xml_movt->bracelist, jb_brace_start, jb_brace_stop);
set_bracebrack(joinbits, xml_movt->bracketlist, jb_bracket_start, jb_bracket_stop);
set_bracebrack(joinbits, xml_movt->thinbracketlist, jb_bracket_start, jb_bracket_stop);

/* Now we can output a list of parts, mapping each PMW stave to a part and
creating groups for joining information. Brackets are always group 1, braces
group 2, barline groups are 3 and no-barline groups are 4. */

PA("<part-list>");

for (usint stave = 1; stave <= laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  uint32_t *name = NULL;
  uint32_t *abbr = NULL;

  st = xml_movt->stavetable[stave];
  snamestr *sn = st->stave_name;

  if ((joinbits[stave] & jb_bracket_start) != 0)
    {
    PA("<part-group number=\"1\" type=\"start\">");
    PN("<group-symbol>bracket</group-symbol>");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_brace_start) != 0)
    {
    PA("<part-group number=\"2\" type=\"start\">");
    PN("<group-symbol>brace</group-symbol>");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_barline_start) != 0)
    {
    PA("<part-group number=\"3\" type=\"start\">");
    PN("<group-barline>yes</group-barline>");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_break_start) != 0)
    {
    PA("<part-group number=\"4\" type=\"start\">");
    PN("<group-barline>no</group-barline>");
    PB("</part-group>");
    }

  /* Support only a basic stave name and abbreviation. At least one XML
  processor insists on the presence of <part-name>, though it can be empty. We
  have to convert strings to UTF-8. MusicXML requires <part-name> be present,
  even if there is also <part-name-display>. */

  if (sn!= NULL)
    {
    if (sn->text != NULL) name = sn->text;
    if (sn->next != NULL && sn->next->text != NULL) abbr = sn->next->text;
    }

  PA("<score-part id=\"P%d\">", stave);
  PN("<part-name>%s</part-name>", (name == NULL)?
    US"" : convert_PMW_string(name));

  if (name != NULL)
    {
    PA("<part-name-display>");
    write_PMW_string(name, INT32_MAX,
      (&xml_movt->fontsizes->fontsize_text[sn->size])->size,
      "display-text", "", 0,  INT32_MAX, NULL, NULL, 0);
    PB("</part-name-display>");
    }

  if (abbr != NULL)
    {
    PN("<part-abbreviation>%s</part-abbreviation>", convert_PMW_string(abbr));
    PA("<part-abbreviation-display>");
    write_PMW_string(abbr, INT32_MAX,
      (&xml_movt->fontsizes->fontsize_text[sn->next->size])->size,
      "display-text", "", 0,  INT32_MAX, NULL, NULL, 0);
    PB("</part-abbreviation-display>");
    }

// TODO midi-instrument - if anything set

  PB("</score-part>");

  if ((joinbits[stave] & jb_bracket_stop) != 0)
    {
    PA("<part-group number=\"1\" type=\"stop\">");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_brace_stop) != 0)
    {
    PA("<part-group number=\"2\" type=\"stop\">");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_barline_stop) != 0)
    {
    PA("<part-group number=\"3\" type=\"stop\">");
    PB("</part-group>");
    }

  if ((joinbits[stave] & jb_break_stop) != 0)
    {
    PA("<part-group number=\"4\" type=\"stop\">");
    PB("</part-group>");
    }
  }

PB("</part-list>");
PN(PART_SEPARATOR);

/* Now we can output each stave as a "part". */

for (usint stave = 1; stave <= laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  st = xml_movt->stavetable[stave];
  barstr **barvector = st->barindex;
  current_stave = stave;
  xml_suspended = FALSE;

  /* If this stave has underlay and used the "=" continuation feature, we need
  to scan it and insert any missing "=" syllables because MusicXML needs
  extender continuations on the intermediate notes. (PS and PDF output don't
  require this.) The logic is:

    (1) Scan stave for an underlay syllable that is just "=".
    (2) See if the next underlay syllable is also "=".
    (3) If there are any notes in between, give them an "=" syllable.
    (4) Advance to second "=" then go to (2).

  The nextulinstave() function yields the next underlay item in the stave,
  jumping to the next bar where necessary. The nexteqinstave() function yields
  the next "=" item in the stave. */

  if (st->hadlayequals)  /* There will be at least one "=" */
    {
    int barno = 0;
    b_textstr *t1 = nexteqinstave(barvector[0], &barno);
    b_textstr *t2;
    int barno_t1 = barno;

    while ((t2 = nextulinstave((barstr *)t1, &barno)) != NULL)
      {
      /* Look at the next underlay string. If it's not "=", move on to restart
      from there, seeking the next "=". */

      if (t2->laylen != 1 || PCHAR(t2->string[0]) != '=')
        {
        t1 = nexteqinstave((barstr *)t2, &barno);
        if (t1 == NULL) break;
        barno_t1 = barno;
        continue;
        }

      /* We now have two "=" strings. The first note after the first one is
      associated with it. Any other notes before the second "=" now need to
      have "=" inserted. */

      BOOL first = TRUE;
      int barno_t2 = barno;
      barno = barno_t1;

      for (barstr *b = nextinstave((barstr *)t1, &barno);
           b != (barstr *)t2;
           b = nextinstave(b, &barno))
        {
        if (b->type != b_note) continue;
        if (((b_notestr *)b)->spitch == 0) break;  /* A rest kills it */
        if (first)
          {
          first = FALSE;
          continue;
          }

        /* Insert a copy of the first "=" item. */

        b_textstr *t = mem_get_insert_item(sizeof(b_textstr), b_text, (bstr *)b);
        size_t offset = offsetof(b_textstr, flags);
        memcpy((char *)t + offset, (char *)t1 + offset,
          sizeof(b_textstr) - offset);
        }

      /* Restart from the second "=". */

      t1 = t2;
      barno_t1 = barno_t2;
      }

    DEBUG(D_barX) debug_bar("After inserting \"=\" underlay items for XML");
    }

  else DEBUG(D_barX) eprintf("\n---- No changes made for XML output ----\n");

  /* Reset underlay states */

  memset(underlay_state, 0, UNDERLAY_MAX);

  /* Choose a value for "divisions", which is the length of a crotchet. We must
  allow for tuplets. The st->tuplet_bits field has a bit set for every tuplet
  counting from 1 for the least significant. Thus 0x04 for triplets, for
  example. The least significant bit is never set (there is no such thing as a
  uniplet :-). */

  uint32_t tuplets = st->tuplet_bits >> 1;
  int divisions = 1;

  if (st->shortest_note < len_crotchet)
    divisions = len_crotchet/st->shortest_note;

  for (int i = 2; i < 32 && tuplets != 0; i++, tuplets >>= 1)
    {
    if ((tuplets & 1) != 0 && divisions % i != 0) divisions *= i;
    }

  /* Impose a minimum */

  if (divisions < 4) divisions = (divisions == 3)? 6:4;

  /* Start the part */

  PA("<part id=\"P%d\">", stave);

  /* The first bar has special handling because of the need to deal with
  default clef, key, and time, and to set <divisions>. The first measure of the
  first stave is where bar numbering can be specified. */

  int bar = 0;
  start_measure(bar);
  first_measure(barvector[bar], divisions);
  if (stave == 1) set_barnumbering();
  bar += complete_measure(bar, divisions);

  /* Now process the remaining bars of the stave. */

  while (bar < st->barcount)
    {
    start_measure(bar);
    bar += complete_measure(bar, divisions);
    }

  PB("</part>");
  PN(PART_SEPARATOR);
  }    /* End of loop through the staves */

/* Write ending boilerplate and close the file. */

PB("</score-partwise>");
fclose(xml_file);
}

/* End of xmlout.c */
