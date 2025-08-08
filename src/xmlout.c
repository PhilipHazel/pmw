/*************************************************
*          PMW MusicXML output generation        *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: August 2025 */
/* This file last modified: August 2025 */

#include "pmw.h"


/* This file contains code for writing a MusicXML file. */

#define PART_SEPARATOR \
  "<!--===============================================================-->"

#define MEASURE_SEPARATOR \
  "<!--============================-->"

/* Convert millipoints to tenths. */

#define T(x) (x/400)


/*************************************************
*             Local variables                    *
*************************************************/

static int       indent = 0;
static int       comment_bar = 0;
static int       comment_stave = 0;
static FILE     *xml_file;
static movtstr  *xml_movt;
static uint64_t  xml_staves = ~0uL;



/*************************************************
*            Write commentary item               *
*************************************************/

static void
comment(const char *format, ...)
{
va_list ap;
va_start(ap, format);

uint32_t pno = xml_movt->barvector[comment_bar];
uint32_t pnofr = pno & 0xffff;
pno >>= 16;

fprintf(stderr, "XML (%d/", comment_stave);
if (pnofr == 0) fprintf(stderr, "%d) ", pno);
  else fprintf(stderr, "%d.%d) ", pno, pnofr);

vfprintf(stderr, format, ap);
fprintf(stderr, "\n");
va_end(ap);
}



/*************************************************
*            Write text with indent              *
*************************************************/

/* Write text to the XML output file, followed by a newline, optionally
adjusting the indent before or after. The basic function is called from three
wrappers for common cases.

Arguments:
  pre       indent adjustment before
  post      indent adjustment after
  format    format string
  ...       arguments

Returns:    nothing
*/

static void
xprintf(int pre, int post, const char *format, va_list ap)
{
indent += pre;
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
indent += post;
}

static void
PA(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(0, 2, format, ap);
}

static void
PB(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(-2, 0, format, ap);
}

static void
PN(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(0, 0, format, ap);
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
//  {"none", 0, 0},       /* none, using deprecated value */
  {"F", 4, 1},          /* soprabass */
  {"C", 1, 0},          /* soprano */
  {"C", 4, 0},          /* tenor */
  {"G", 2, 0},          /* treble */
  {"G", 2, 0},          /* trebledescant */
  {"G", 2, 0},          /* trebletenor*/
  {"G", 2, 0}           /* trebletenorB*/
};

static void
write_clef(unsigned int clef, BOOL *openattr)
{
const char *nonestring = (clef != clef_none)? "" : " print-object=\"no\"";

if (openattr != NULL && !*openattr)
  {
  PA("<attributes>");
  *openattr = TRUE;
  }

PA("<clef%s>", nonestring);
PN("<sign>%s</sign>", clef_data[clef].sign);
if (clef_data[clef].line != 0)
  PN("<line>%d</line>", clef_data[clef].line);
if (clef_data[clef].octave != 0)
  PN("<clef-octave-change>%d</clef-octave-change>", clef_data[clef].octave);

switch(clef)
  {
  case clef_contrabass:
  comment("contrbass clef lacks '8'");
  break;

  case clef_soprabass:
  comment("soprabass clef lacks '8'");
  break;

  case clef_trebledescant:
  comment("trebledescant clef lacks '8'");
  break;

  case clef_trebletenor:
  comment("trebletenor clef lacks '8'");
  break;

  case clef_trebletenorB:
  comment("trebletenorB clef lacks '8'");
  break;

  default:
  break;
  }

PB("</clef>");
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
write_key(uint32_t key, BOOL *openattr)
{
if (key == key_N)
  {
  comment("key N treated as C");
  key = key_C;
  }

else if (key >= key_X)
  {
  comment("custom key not supported: treated as C");
  key = key_C;
  }

if (openattr != NULL && !*openattr)
  {
  PA("<attributes>");
  *openattr = TRUE;
  }

PA("<key>");
PN("<fifths>%d</fifths>", key_fifths[key]);
PB("</key>");
}
#undef BAD



/*************************************************
*                   Write time                   *
*************************************************/

// TODO: use <time-symbol> for common and cut and other specials

static void
write_time(uint32_t time, BOOL *openattr)
{
if (time == time_common)
  {
  comment("common time rendered as 4/4");
  time = time_default;
  }
else if (time == time_cut)
  {
  comment("cut time rendered as 2/2");
  time = 0x00010202u;
  }

else if ((time & 0x00ff0000u) != 0x00010000u)
  {
  comment("time signature multiple ignored");
  }

time &= 0x0000ffffu;

if (openattr != NULL && !*openattr)
  {
  PA("<attributes>");
  *openattr = TRUE;
  }

PA("<time>");
PN("<beats>%d</beats>", time >> 8);
PN("<beat-type>%d</beat-type>", time & 0xffu);
PB("</time>");
}



/*************************************************
*             Handle start of a measure          *
*************************************************/

static void
start_measure(int bar)
{
barposstr *bp = xml_movt->posvector + bar;
int32_t width = bp->vector[bp->count - 1].xoff;

/* A PMW uncounted bar will have a non-zero fractional part or a zero
integer part. */

uint32_t pno = xml_movt->barvector[bar];
uint32_t pnofr = pno & 0xffff;
pno >>= 16;
const char *implicit = (pnofr != 0 || pno == 0)? " implicit=\"yes\"" : "";

PA("<measure number=\"%d\" width=\"%d\"%s>", bar, T(width), implicit);

/* Sort out text for bar numbering. Not sure if this is needed (the element
is optional).*/

#ifdef NEVER
if (pnofr == 0)
  PN("<measure-text>%d</measure-text>", pno);
else
  PN("<measure-text>%d.%d</measure-text>", pno, pnofr);
#endif
}



/*************************************************
*   Special action for the first bar of a stave  *
*************************************************/

/* Take special action for the first bar of a stave. Search this bar to see if
any of clef, key, or time are specified before the first note. If not, default
the clef to treble, and use the movement's default key and time. If any of them
are found, remove that item from the chain. Process these three items, Then
handle the rest of the stave. By doing it this way, all three settings will be
within a single <attributes> element. */

static void
first_measure(barstr *b)
{
uint32_t clef = clef_treble;
uint32_t key = xml_movt->key;
uint32_t time = xml_movt->time;

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

/* Output any of clef, key, and time that are needed, in a single
<attributes> element. */

BOOL openattr = FALSE;
write_clef(clef, &openattr);
write_key(key, &openattr);
if ((xml_movt->flags & (mf_showtime | mf_startnotime)) != 0)
  write_time(time, &openattr);
if (openattr) PB("</attributes>");
}



/*************************************************
*        Handle the items in a bar (measure)     *
*************************************************/

static void
complete_measure(barstr *b)
{
for (; b != NULL; b = (barstr *)b->next)
  {
  switch(b->type)
    {
    case b_start:
    break;

    case b_all:
    comment("ignored [all]");
    break;

    case b_barline:
    comment("ignored barline");
    break;

    case b_barnum:
    comment("ignored [barnumber]");
    break;

    case b_beamacc:
    comment("ignored [beamacc]");
    break;

    case b_beambreak:
    comment("ignored beam break");
    break;

    case b_beammove:
    comment("ignored [beammove]");
    break;

    case b_beamrit:
    comment("ignored [beamrit]");
    break;

    case b_beamslope:
    comment("ignored [beamslope]");
    break;

    case b_breakbarline:
    comment("ignored [breakbarline]");
    break;

    case b_bowing:
    comment("ignored [bowing]");
    break;

    case b_caesura:
    comment("ignored // (caesura)");
    break;

    case b_clef:
    PA("<attributes>");
    write_clef(((b_clefstr *)b)->clef, NULL);
    PB("</attributes>");
    break;

    case b_comma:
    comment("ignored [comma]");
    break;

    case b_dotbar:
    comment("ignored : (dotted barline)");
    break;

    case b_dotright:
    comment("ignored dotright movement");
    break;

    case b_draw:
    comment("ignored [draw]");
    break;

    case b_accentmove:
    comment("ignored accent movement");
    break;

    case b_endline:
    comment("ignored [endline]");
    break;

    case b_endslur:
    comment("ignored [endslur]");
    break;

    case b_endplet:
    comment("ignored } (end plet)");
    break;

    case b_ens:
    comment("ignored [ns]");
    break;

    case b_ensure:
    comment("ignored [ensure]");
    break;

    case b_footnote:
    comment("ignored [footnote]");
    break;

    case b_hairpin:
    comment("ignored hairpin");
    break;

    case b_justify:
    comment("ignored [justify]");
    break;

    case b_key:
    PA("<attributes>");
    write_key(((b_keystr *)b)->key, NULL);
    PB("</attributes>");
    break;

    case b_lrepeat:
    comment("ignored (: (left repeat)");
    break;

    case b_midichange:
    comment("ignored MIDI parameter change");
    break;

    case b_move:
    comment("ignored [move]");
    break;

    case b_name:
    comment("ignored [name]");
    break;

    case b_nbar:
    comment("ignored nth time marking");
    break;

    case b_newline:
    comment("ignored [newline]");
    break;

    case b_newpage:
    comment("ignored [newpage]");
    break;

    case b_note:
    comment("ignored note/rest");
    break;

    case b_notes:
    comment("ignored [notes]");
    break;

    case b_ns:
    comment("ignored absolute note spacing");
    break;

    case b_nsm:
    comment("ignored multiply note spacing");
    break;

    case b_olevel:
    comment("ignored [olevel]");
    break;

    case b_olhere:
    comment("ignored [olhere]");
    break;

    case b_ornament:
    comment("ignored ornament on next note");
    break;

    case b_overbeam:
    comment("ignored beam over barline");
    break;

    case b_page:
    comment("ignored [page]");
    break;

    case b_pagebotmargin:
    comment("ignored [bottommargin]");
    break;

    case b_pagetopmargin:
    comment("ignored [topmargin]");
    break;

    case b_plet:
    comment("ignored plet (triplet, etc)");
    break;

    case b_reset:
    comment("ignored [reset]");
    break;

    case b_resume:
    comment("ignored [resume]");
    break;

    case b_rrepeat:
    comment("ignored :) (right repeat)");
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

    case b_slur:
    comment("ignored [slur]");
    break;

    case b_slurgap:
    comment("ignored [slurgap]");
    break;

    case b_linegap:
    comment("ignored [linegap]");
    break;

    case b_space:
    comment("ignored [space]");
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

    case b_suspend:
    comment("ignored [suspend]");
    break;

    case b_text:
    comment("ignored text string");
    break;

    case b_tick:
    comment("ignored [tick]");
    break;

    case b_tie:
    comment("ignored tie");
    break;

    case b_time:
    if ((xml_movt->flags & mf_showtime) != 0)
      {
      PA("<attributes>");
      write_time(((b_timestr *)b)->time, NULL);
      PB("</attributes>");
      }
    break;

    case b_tremolo:
    comment("ignored [tremolo]");
    break;

    case b_tripsw:
    comment("ignored [triplets]");
    break;

    case b_ulevel:
    comment("ignored [ulevel]");
    break;

    case b_ulhere:
    comment("ignored [ulhere]");
    break;

    case b_unbreakbarline:
    comment("ignored [unbreakbarline]");
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
}



/*************************************************
*                Write MusicXML file             *
*************************************************/

/* This is the only external entry to this set of functions. The data is all in
memory and global variables. Writing a MusicXML file is triggered by the use of
the -musicxml or -xml command line option, which sets outxml_filename non-NULL.

Arguments:  none
Returns:    nothing
*/

void
outxml_write(void)
{
char datebuff[100];
time_t now;

TRACE("outxml_write() movement %d\n", outxml_movement);

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

/* Stave selection is the movement's stave selection. Currently there's no way
of changing this. */

xml_staves = xml_movt->select_staves;

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

PA("<identification>");
PA("<encoding>");
PN("<software>PMW %s</software>", PMW_VERSION);
PN("<encoding-date>%s</encoding-date>", datebuff);
PB("</encoding>");
PB("</identification>");

/* Next comes various defaults */

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

// TODO Any need for system-layout?
// TODO Any need for appearance?
// TODO Any need for music-font?

// TODO May need to set word-font and lyric-font

PB("</defaults>");

// TODO Headings
comment("headings and footings not yet supported");

/* Output a list of parts, mapping each PMW stave to a part. */

PA("<part-list>");

for (int stave = 1; stave <= xml_movt->laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  PA("<score-part id=\"P%d\">", stave);

// TODO score-instrument - get stave name
// TODO midi-instrument - if anything set

  PB("</score-part>");
  }

PB("</part-list>");
PN(PART_SEPARATOR);

/* Now we can output each stave as a "part". */

for (int stave = 1; stave <= xml_movt->laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  stavestr *stavebase = xml_movt->stavetable[stave];
  barstr **barvector = stavebase->barindex;
  comment_stave = stave;

  PA("<part id=\"P%d\">", stave);

  /* The first bar has special handling because of the need to deal with
  default clef, key, and time. */

  start_measure(0);
  comment_bar = 0;
  first_measure(barvector[0]);
  complete_measure(barvector[0]);

  /* Now process the remaining bars of the stave. */

  for (int bar = 1; bar < stavebase->barcount; bar++)
    {
    start_measure(bar);
    comment_bar = bar;
    complete_measure(barvector[bar]);
    }

  PB("</part>");
  PN(PART_SEPARATOR);
  }    /* End of loop through the staves */

/* Write ending boilerplate and close the file. */

PB("</score-partwise>");
fclose(xml_file);
}

/* End of xmlout.c */
