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
*                Data tables                     *
*************************************************/

static const char *MXL_type_names[] = {
  "breve", "whole", "half", "quarter", "eighth", "16th", "32nd", "64th" };

static const char *MXL_accidental_names[] = {
  "unused", "natural", "quarter-sharp", "sharp", "double-sharp",
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



/*************************************************
*             Local variables                    *
*************************************************/

static int        indent = 0;
static int        comment_bar = 0;
static int        comment_stave = 0;

static BOOL       beam_active = FALSE;

/* NOTE: PMW doesn't yet support nested tuplets - nobody has ever remarked on 
this - but if it ever does, the code in this module should cope. */

static int        plet_level = 0;
static b_pletstr *plet_pending = NULL;
static uint8_t    plet_actual[8] = {0};  /* Initialize level 0 => no plet */
static uint8_t    plet_normal[8] = {0};

static FILE      *xml_file;
static movtstr   *xml_movt;
static uint64_t   xml_staves = ~0uL;



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
write_clef(unsigned int clef)
{
const char *nonestring = (clef != clef_none)? "" : " print-object=\"no\"";

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
write_key(uint32_t key)
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
write_time(uint32_t time)
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

PA("<time>");
PN("<beats>%d</beats>", time >> 8);
PN("<beat-type>%d</beat-type>", time & 0xffu);
PB("</time>");
}



/*************************************************
*               Write a note or rest             *
*************************************************/

/* PMW and MusicXML have different conventions for chords. In PMW, the first
note of a chord has type b_note, with the nf_chord flag set. Subsequent notes
are of type b_chord, also with the flag set. The end of the chord happens when
a b_chord item is followed either by NULL (end of bar) or an item with type
other than b_chord. In MusicXML, the first note is unaffected, but subsequent
notes have a <cord/> element.

We handle an entire chord here, returning the pointer to the final note.
*/

static barstr *
write_note(barstr *b, int divisions)
{
BOOL inchord = FALSE;

for (;;)
  {
  b_notestr *note = (b_notestr *)b;

  PA("<note>");
  if (inchord) PN("<chord/>");

  // TODO Handle rest level

  if (note->spitch == 0)
    {
    PN("<rest/>");
    }

  // TODO Think about <unpitched>?
  else
    {
    int letter = toupper(note->char_orig);
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
  likely. (The length of a breve is 0x015fea00.) Therefore, resort to using
  64-bit arithmetic. */

  PN("<duration>%d</duration>",
    (uint32_t)(((uint64_t)note->length * divisions) / (uint64_t)len_crotchet));

//tie  type = start/stop

  PN("<type>%s</type>", MXL_type_names[note->notetype]);
  if ((note->flags & (nf_dot|nf_dot2)) != 0) PN("<dot/>");
  if ((note->flags & nf_dot2) != 0) PN("<dot/>");
  if (note->acc != ac_no) PN("<accidental>%s</accidental>",
    MXL_accidental_names[note->acc]);

//TODO allow for different half-accidental styles

  /* If this is the start of a tuplet, we have to set up <time-modification> 
  here, and save the data for the remaining notes in the tuplet. A stack is 
  used so as to deal with nested tuplets. Leave plet_pending set, because it is
  used later on for other tuplet options. */

  if (plet_pending != NULL)
    {
    plet_actual[++plet_level] = plet_pending->pletlen;
    plet_normal[plet_level] = plet_pending->pletnum; 
    }  
  
  /* This note is in a tuplet. */
  
  if (plet_actual[plet_level] != 0)
    {
    PA("<time-modification>");
    PN("<actual-notes>%d</actual-notes>", plet_actual[plet_level]); 
    PN("<normal-notes>%d</normal-notes>", plet_normal[plet_level]); 
    PB("</time-modification>"); 
    }  

  /* Stem */

  if ((note->flags & nf_stem) == 0) PN("<stem>none</stem>");
    else PN("<stem>%s</stem>", ((note->flags & nf_stemup) != 0)? "up":"down");

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
  
// beam

  /* Handle tuplets. The start is indicated by a pending b_pletstr; for the end 
  we have to peek ahead. Note that plet_level has already been incremented 
  above. */ 
  
  if (plet_pending != NULL)
    {
    const char *bracket = beam_active? "no" : "yes";
    const char *placement, *show;
      
    if ((plet_pending->flags & plet_bn) != 0) bracket = "no"; 
    if ((plet_pending->flags & plet_by) != 0) bracket = "yes"; 
    
    if ((plet_pending->flags & plet_a) != 0) placement = "above";
    else if ((plet_pending->flags & plet_b) != 0) placement = "below";
    else placement = ((note->flags & nf_stemup) == 0)? "above" : "below"; 
    
    if ((plet_pending->flags & plet_x) != 0)
      {
      bracket = "no";
      show = "none";
      }
    else show = "actual";        
 
    PA("<notations>"); 
    PA("<tuplet number=\"%d\" type=\"start\" bracket=\"%s\" "
      "placement=\"%s\" show-number=\"%s\">",  
      plet_level, bracket, placement, show);
    
    // tuplet-actual? tuplet-normal?

    PB("</tuplet>"); 
    PB("</notations>");
      
    plet_pending = NULL; 
    }  
    
  else if (b->next->type == b_endplet)
    {
    PA("<notations>"); 
    PN("<tuplet number=\"%d\" type=\"stop\"/>", plet_level--); 
    PB("</notations>"); 
    }  

  PB("</note>");

  if (b->next->type != b_chord) break;
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
one-to-one match. We don't need a <barline> element for a normal barline. */

static void
write_barline(barstr *b, BOOL finalbar)
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
    comment("bar line style %d not supported", bl->barstyle);
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

if (style != NULL)
  {
  PA("<barline>");
  PN("<bar-style>%s</bar-style>", style);
  PB("</barline>");
  }
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
are found, remove that item from the chain. Process these three items, along
with the divisions setting. */

static void
first_measure(barstr *b, int divisions)
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

/* Output divisions and any of clef, key, and time that are needed. */

PA("<attributes>");
PN("<divisions>%d</divisions>", divisions);
write_clef(clef);
write_key(key);
if ((xml_movt->flags & (mf_showtime | mf_startnotime)) != 0)
  write_time(time);
PB("</attributes>");
}



/*************************************************
*        Handle the items in a bar (measure)     *
*************************************************/

static void
complete_measure(barstr *b, int divisions, BOOL finalbar)
{
for (; b != NULL; b = (barstr *)b->next)
  {
  switch(b->type)
    {
    case b_start:
    break;

    case b_accentmove:
    comment("ignored accentmove");
    break;

    case b_all:
    comment("ignored [all]");
    break;

    case b_barline:
    write_barline(b, finalbar);
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

    case b_bowing:
    comment("ignored [bowing]");
    break;

    case b_breakbarline:
    comment("ignored [breakbarline]");
    break;

    case b_caesura:
    comment("ignored // (caesura)");
    break;

    case b_chord:
    comment("b_chord encountered at top level: ERROR!");
    break;

    case b_clef:
    PA("<attributes>");
    write_clef(((b_clefstr *)b)->clef);
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

    case b_endline:
    comment("ignored [endline]");
    break;

    case b_endslur:
    comment("ignored [endslur]");
    break;

    case b_endplet:  /* Handled from within write_note */
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
    write_key(((b_keystr *)b)->key);
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
    b = write_note(b, divisions);
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
    plet_pending = (b_pletstr *)b;
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
      write_time(((b_timestr *)b)->time);
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

if (main_testing == 0)
  {
  PN("<software>PMW %s</software>", PMW_VERSION);
  PN("<encoding-date>%s</encoding-date>", datebuff);
  }
else
  {
  PN("<software>PMW</software>");
  PN("<encoding-date>0000-00-00</encoding-date>");
  }

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
if (main_testing == 0)
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

  /* Choose a value for "divisions", which is the length of a crotchet. We must
  allow for tuplets. The tuplet_bits field has a bit set for every tuplet
  counting from 1 for the least significant. Thus 0x02 for triplets, e.g. */

  uint32_t tuplets = stavebase->tuplet_bits;
  int divisions = 1;

  if (stavebase->shortest_note < len_crotchet)
    divisions = len_crotchet/stavebase->shortest_note;

  for (int i = 2; i < 32 && tuplets != 0; i++, tuplets >>= 1)
    {
    if ((tuplets & 1) != 0 && divisions % i != 0) divisions *= i;
    }
  if (divisions < 4) divisions = 4;  /* Impose a minimum */

  /* Start the part */

  PA("<part id=\"P%d\">", stave);

  /* The first bar has special handling because of the need to deal with
  default clef, key, and time, and to set <divisions>. */

  start_measure(0);
  comment_bar = 0;
  first_measure(barvector[0], divisions);
  complete_measure(barvector[0], divisions, stavebase->barcount < 2);

  /* Now process the remaining bars of the stave. */

  for (int bar = 1; bar < stavebase->barcount; bar++)
    {
    start_measure(bar);
    comment_bar = bar;
    complete_measure(barvector[bar], divisions, bar == stavebase->barcount - 1);
    }

  PB("</part>");
  PN(PART_SEPARATOR);
  }    /* End of loop through the staves */

/* Write ending boilerplate and close the file. */

PB("</score-partwise>");
fclose(xml_file);
}

/* End of xmlout.c */
