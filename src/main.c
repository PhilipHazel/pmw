/*************************************************
*        PMW entry point and initialization      *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: December 2025 */

#include "pmw.h"
#include "rdargs.h"

#if !defined NO_PMWRC || NO_PMWRC == 0
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#endif


/*************************************************
*             Command line data                  *
*************************************************/

/* Keywords for the command line are defined as a single string that is
searched in order. See the rdargs.c source for a specification. If you increase
the number of keys, check that the size of the keyoffset field in rdargs.c is
big enough. Otherwise an error will occur.  */

static const char *arg_pattern =
  ","
  "a4ona3/s,"
  "a4sideways/s,"
  "a5ona4/s,"
  "C/k,"
  "c/k/n,"
  "dbd/k,"
  "dbl=drawbarlines/s,"
  "dsl=drawstavelines=drawstafflines/n=3,"
  "dtp/k/n,"
  "duplex/s,"
  "?d,"                 /* Matches any other key starting with 'd' */
  "em=errormaximum/n,"
  "eps/s,"
  "F/k,"
  "f/k,"
  "H/k,"
  "-help=help/s,"
  "incPMWfont=includePMWfont=incpmwfont=includepmwfont=ipf/s,"
  "MacOSapp/k,"
  "MF/k,"
  "MP/k,"
  "MV/k,"
  "manualfeed/s,"
  "midi/k,"
  "mb=midibars/k,"
  "mm=midimovement/k/n,"
  "musicxml=xml/k,"
  "musicxmlmovement=xmlmovement=xm/k/n,"
  "norc=nopmwrc/s,"
  "nr=norepeats=norepeat/s,"
  "nw=nowidechars/s,"
  "o/k,"
  "p/k,"
  "pamphlet/s,"
  "pdf/s,"
  "printadjust/k/2/m,"
  "printgutter/k,"
  "printscale/k,"
  "printside/k/n,"
  "ps/s,"
  "reverse/s,"
  "SM/k,"
  "s/k,"
  "t/k/n,"
  "testing/n=2,"
  "tumble/s,"
  "-version=V/s,"
  "v/s,"
  "?x";                  /* Matches any other key starting with 'd' */

/* Offsets in results vector for command line keys */

enum {
  arg_aa_input,    /* The only unkeyed possibility */
  arg_a4ona3,
  arg_a4sideways,
  arg_a5ona4,
  arg_C,
  arg_c,
  arg_dbd,
  arg_drawbarlines,
  arg_drawstavelines,
  arg_dtp,
  arg_duplex,
  arg_d,
  arg_em,
  arg_eps,
  arg_F,
  arg_f,
  arg_H,
  arg_help,
  arg_incPMWfont,
  arg_MacOSapp,
  arg_MF,
  arg_MP,
  arg_MV,
  arg_manualfeed,
  arg_midi,
  arg_midibars,
  arg_midimovement,
  arg_musicxml,
  arg_musicxmlmovement,
  arg_norc,
  arg_norepeats,
  arg_nowidechars,
  arg_o,
  arg_p,
  arg_pamphlet,
  arg_pdf,
  arg_printadjustx,
  arg_printadjusty,
  arg_printgutter,
  arg_printscale,
  arg_printside,
  arg_ps,
  arg_reverse,
  arg_SM,
  arg_s,
  arg_t,
  arg_testing,
  arg_tumble,
  arg_V,
  arg_v,
  arg_x
};

/* Vector for modified command line options (after adding .pmwrc) */

static char **newargv = NULL;

/* Flag to record when -xmlmovement is set */

#if SUPPORT_XML
static BOOL xml_movement_set = FALSE;
#endif

/* This table must be in alphabetical order because it is searched by binary
chop. */

static bit_table debug_options[] = {
  { US"all",             D_all },
  { US"any",             D_any },
  { US"bar",             D_bar },
  { US"barO",            D_barO },
  { US"barP",            D_barP },
  { US"barR",            D_barR },
  { US"barX",            D_barX },
  { US"font",            D_font },
  { US"header",          D_header_all },
  { US"headerglob",      D_header_glob },
  { US"headers",         D_header_all },
  { US"macro",           D_macro },
  { US"macros",          D_macro },
  { US"memory",          D_memory },
  { US"memorydetail",    D_memorydetail },
  { US"movtflags",       D_movtflags },
  { US"preprocess",      D_preprocess },
  { US"sortchord",       D_sortchord },
  { US"stringwidth",     D_stringwidth },
  { US"trace",           D_trace },
  { US"xmlanalyze",      D_xmlanalyze },
  { US"xmlgroups",       D_xmlgroups },
  { US"xmlread",         D_xmlread },
  { US"xmlstaves",       D_xmlstaves }
};

#define DEBUG_OPTIONS_COUNT (sizeof(debug_options)/sizeof(bit_table))

/* Likewise, this table of XML output options must be in alphabetical order. */

#if SUPPORT_XML
static bit_table xout_options[] = {
  { US"newline",         mx_newline },
  { US"newpage",         mx_newpage },
  { US"numberlyrics",    mx_numberlyrics },
  { US"suspend",         mx_suspend }
};

#define XOUT_OPTIONS_COUNT (sizeof(xout_options)/sizeof(bit_table))
#endif


/*************************************************
*     Provide case-independent string match      *
*************************************************/

int
strncmpic(const unsigned char *s, const unsigned char *t, int n)
{
while (n--)
  {
  int c = tolower(*s++) - tolower(*t++);
  if (c != 0) return c;
  }
return 0;
}



/*************************************************
*        Debug & XML output option decoding      *
*************************************************/

/* This function decodes a string containing bit settings in the form of +name
and/or -name sequences, and sets/unsets bits in the selector bit string
accordingly.

Arguments:
  name           option name (for errors)
  string         the argument string
  bit_table      table of options, in alphabetical order
  btlen          length of table
  selector       the selector

Returns:         nothing; all errors are hard
*/

static void
decode_bitselector(const char *name, const char *string, bit_table *table,
  size_t btlen, uint32_t *selector)
{
for(;;)
  {
  BOOL adding;
  const char *s;
  int len;
  bit_table *start = table;
  bit_table *end = start + btlen;

  if (*string == 0) return;
  if (*string != '+' && *string != '-') error(ERR31, name, string);  /* Hard */

  adding = *string++ == '+';
  s = string;
  while (isalnum((unsigned char)*string) || *string == '_') string++;
  len = string - s;

  while (start < end)
    {
    bit_table *middle = start + (end - start)/2;
    int c = Ustrncmp(s, middle->name, len);
    if (c == 0)
      {
      if (middle->name[len] != 0) c = -1; else
        {
        uint32_t bit = middle->bit;
        if (adding) *selector |= bit; else *selector &= ~bit;
        break;  /* Out of loop to match selector name */
        }
      }
    if (c < 0) end = middle; else start = middle + 1;
    }  /* Loop to match selector name */

  if (start >= end) error(ERR32, name, adding? '+' : '-', len, s);  /* Hard */
  }           /* Loop for selector names */
}



/*************************************************
*        Display information about music         *
*************************************************/

/* This function is called after pagination if the -v option is present.

Arguments:  none
Returns:    nothing
*/

static void
display_info(void)
{
pagestr *p = main_pageanchor;
usint movt;
int laststave = -1;
BOOL usesharp = FALSE;
BOOL halfaccs[MAX_STAVE+1];        /* These variables are used to remember */
uint16_t toppitch[MAX_STAVE+1];    /* data from each stave over the course */
uint16_t botpitch[MAX_STAVE+1];    /* of multiple movements, so that overall */
uint32_t totalpitch[MAX_STAVE+1];  /* statistics can be given. */
uint32_t notecount[MAX_STAVE+1];

/* Display information about the staves in each movement */

for (movt = 0; movt < movement_count; movt++)
  {
  int stave;
  movtstr *m = movements[movt];

  eprintf("\nMOVEMENT %d\n\n", m->number);
  usesharp = (keysigtable[m->key][0] & 0xf0) != (ac_fl << 4);

  for (stave = 0; stave <= m->laststave; stave++)
    {
    stavestr *s = (m->stavetable)[stave];
    if (stave == 0 && s->barcount == 0) continue;  /* Omit stave 0 if empty */
    halfaccs[stave] = s->halfaccs;
    eprintf("Stave %2d: ", stave);

    if (m->barnocount == 0)
      eprintf("%d bar%s", s->barcount, (s->barcount == 1)? "":"s");
    else eprintf("%d(+%d) bars", s->barcount - m->barnocount, m->barnocount);

    if (stave > laststave)
      {
      laststave = stave;
      toppitch[stave] = 0;
      botpitch[stave] = 9999;
      notecount[stave] = totalpitch[stave] = 0;
      }

    if (s->notecount > 0)
      {
      uint32_t average = s->totalpitch/s->notecount;
      if (!s->halfaccs) average &= 0xfffffffeu;

      eprintf(";%s range %s to %s average %s",
        (s->barcount == 1)? " ":"",
          sfp(s->botpitch, usesharp), sfp(s->toppitch, usesharp),
          sfp(average, usesharp));

      if (s->toppitch > toppitch[stave]) toppitch[stave] = s->toppitch;
      if (s->botpitch < botpitch[stave]) botpitch[stave] = s->botpitch;
      totalpitch[stave] += s->totalpitch;
      notecount[stave] += s->notecount;
      }

    eprintf("\n");
    }
  }

/* If there is more than one movement, display overall information for each
stave. The sharp/flat decision comes from the last movement. */

if (movement_count > 1)
  {
  int stave;
  eprintf("\nOVERALL\n\n");
  for (stave = 1; stave <= laststave; stave++)
    {
    uint32_t average = totalpitch[stave]/notecount[stave];
    if (!halfaccs[stave]) average &= 0xfffffffeu;
    eprintf("Stave %2d: ", stave);
    if (notecount[stave] > 0)
      eprintf("range  %s to %s average %s",
        sfp(botpitch[stave], usesharp), sfp(toppitch[stave], usesharp),
          sfp(average, usesharp));
    eprintf("\n");
    }
  }

/* Display information about the page layout */

if (p != NULL) eprintf("\nPAGE LAYOUT\n\n");

while (p != NULL)
  {
  int count = 14;
  sysblock *s;
  eprintf("Page %d bars: ", p->number);

  for (s = p->sysblocks; s != NULL; s = s->next)
    {
    movtstr *m;
    if (!s->is_sysblock) continue;  /* Heading/footing */
    m = s->movt;

    if (count > 65)
      {
      eprintf("\n ");
      count = 1;
      }

    eprintf("%s-%s%s ", sfb(m->barvector[s->barstart]),
      sfb(m->barvector[s->barend]),
      (s->flags & sysblock_stretch)? "":"*");

    count += 6;
    if (s->overrun < 30000)
      {
      eprintf("(%s) ", sff(s->overrun));
      count += 5;
      }
    }

  eprintf("\n  Space left on page = %s", sff(p->spaceleft));
  if (p->overrun > 0 && p->overrun < 100000)
    eprintf(" Overrun = %s", sff(p->overrun));
  eprintf("\n");

  p = p->next;
  }
}



/*************************************************
*         Read a MIDI translation file           *
*************************************************/

/* These files are short: reading them twice in order to get the correct size
doesn't take much time and saves much hassle. The files contain translation
between names and MIDI voice numbers or names and MIDI "pitches" for untuned
percussion.

Arguments:
  anchor     where to build
  filename   the file name

Returns:     nothing; if the file fails to open, no action is taken
*/

static void
midi_translate(uschar **anchor, uschar *filename)
{
FILE *f = Ufopen(filename, "r");
int length = 0;
uschar *p;
uschar line[60];

if (f == NULL) return;

while (Ufgets(line, 60, f) != NULL)
  {
  line[Ustrlen(line)-1] = 0;
  if (!isdigit(line[0])) continue;  /* Ignore line not starting with a digit */
  length += Ustrlen(line+4) + 2;
  }

if (length == 0) return;            /* No usable text in the file */

/* We store the file in one long byte string. Each name is followed by a zero
byte and then a binary byte containing its number. */

*anchor = mem_get(length+1);
p = *anchor;

rewind(f);
while (Ufgets(line, 60, f) != NULL)
  {
  line[Ustrlen(line)-1] = 0;
  if (!isdigit(line[0])) continue;
  Ustrcpy(p, line+4);
  p += Ustrlen(p) + 1;
  *p++ = Uatoi(line);
  }

/* An empty name marks the end of the list */

*p = 0;
fclose(f);
}



/*************************************************
*                  Give help                     *
*************************************************/

#define PF(x) (void)printf(x)

static void
givehelp(void)
{
const char *b2pf, *pmwrc, *musicxml;

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
b2pf = "yes";
#else
b2pf = "no";
#endif

#if !defined NO_PMWRC || NO_PMWRC == 0
pmwrc = "yes";
#else
pmwrc = "no";
#endif

#if defined SUPPORT_XML && SUPPORT_XML != 0
musicxml = "yes";
#else
musicxml = "no";
#endif

(void)printf("PMW version %s\n%s\n\n", PMW_VERSION, COPYRIGHT);
(void)printf("Default output is:    %s\n", PDF? "PDF" : "PostScript");
(void)printf("B2PF support:         %s\n", b2pf);
(void)printf("~/.pmwrc support:     %s\n", pmwrc);
(void)printf("MusicXML support:     %s\n", musicxml);

PF("\nDefault output is <input>.ps or <input>.pdf when an input file name is given.\n");
PF("Default output is stdout if no input file name is given.\n");

PF("\nGENERAL OPTIONS\n\n");
PF("-a4ona3               arrange A4 images 2-up on A3\n");
PF("-a5ona4               arrange A5 images 2-up on A4\n");
PF("-C <arg>              show a compile-time option; exit with its value (0 or 1).\n");
PF("    b2pf              support for B2PF processing\n");
PF("    musicxml          support for MusicXML input and output\n");
PF("-drawbarlines or -dbl don't use characters for bar lines\n");
PF("-drawstavelines [<n>] don't use characters for stave lines\n");
PF("-dsl [<n>]            synonym for -drawstavelines\n");
PF("-eps                  output encapsulated PostScript\n");
PF("-F <directory-list>   specify fontmetrics, .utr, and font directories\n");
PF("-f <name>             specify format name\n");
PF("-help or --help       output this information, then exit\n");
PF("-MP <file>            specify MIDIperc file\n");
PF("-MV <file>            specify MIDIvoices file\n");
PF("-mb <range>           synonym for -midibars\n");
PF("-midi <file>          specify MIDI output file\n");
PF("-midibars <range>     limit MIDI output to given bar range\n");
PF("-midimovement <n>     specifies movement for MIDI output\n");
PF("-mm <n>               synonym for -midimovement\n");
#if !defined NO_PMWRC || NO_PMWRC == 0
PF("-norc or -nopmwrc     don't read .pmwrc (must be first option)\n");
#endif
PF("-norepeats or -nr     do not play repeats in MIDI output\n");
PF("-nowidechars or -nw   don't use 100-point stave chars\n");
PF("-o <file>             specify output file ('-' for stdout)\n");
PF("-p <list>             select pages\n");
PF("-pamphlet             output pages in pamphlet order\n");
PF("-pdf                  select PDF output\n");
PF("-printadjust <x> <y>  move on page by (x,y)\n");
PF("-printgutter <x>      move recto/verso pages by x/-x\n");
PF("-printscale <n>       scale the image by n\n");
PF("-printside <n>        output only odd or even sides\n");
PF("-ps                   select PostScript output\n");
PF("-reverse              output pages in reverse order\n");
PF("-s <list>             select staves\n");
PF("-t <number>           set transposition\n");
PF("-V or --version       output PMW version number, then exit\n");
PF("-v                    output verification information\n");

PF("\nPOSTSCRIPT-SPECIFIC OPTIONS\n\n");
PF("-a4sideways           assume A4 paper fed sideways\n");
PF("-c <number>           set number of copies\n");
PF("-duplex               set duplex printing in the PostScript\n");
PF("-H <file>             specify PostScript header file\n");
PF("-incPMWfont or -ipf   include PMW font in the output\n");
PF("-manualfeed           set manualfeed in the PostScript\n");
PF("-tumble               set tumble for duplex printing\n");

#if SUPPORT_XML
PF("\nMUSICXML OUTPUT OPTIONS\n\n");
PF("-musicxml <file>      specify MusicXML output file\n");
PF("-musicxmlmovement <n> select movement for MusicXML output\n");
PF("-x<selectors>         set option(s) for MusicXML output (see below)\n");
PF("-xm <n>               synonym for -musicxmlmovement\n");
PF("-xml <file>           synonym for -musicxml\n");
PF("-xmlmovement <n>      synonym for -musicxmlmovement\n");

PF("\nMusicXML option selectors (+ to add, - to subtract):");
for (usint i = 0, j = 0; i < XOUT_OPTIONS_COUNT; i++)
  {
  if ((j++ & 7) == 0) PF("\n ");
  (void)printf(" %s", xout_options[i].name);
  }
PF("\n");
#endif

PF("\nMAINTENANCE AND DEBUGGING OPTIONS\n\n");
PF("-d<selectors>         write debugging info to stderr (see below)\n");
PF("-dbd <m>,<s>,<b>      write debugging bar data (movement, stave, bar) \n");
PF("-dtp <bar>            write debugging position data (-1 for all bars)\n");
PF("-em <n>               synonym for -errormaximum\n");
PF("-errormaximum <n>     set maximum number of errors (for testing)\n");
PF("-MacOSapp <directory> resource directory when run from a MacOS app\n");
PF("-MF <directory-list>  specify music font directories\n");
PF("-SM <directory>       specify standard macros directory\n");
PF("-testing [<n>]        run in testing mode\n");

PF("\nDebug selectors (+ to add, - to subtract):");
for (usint i = 0, j = 0; i < DEBUG_OPTIONS_COUNT; i++)
  {
  if (Ustrcmp(debug_options[i].name, US "any") == 0 ||
      Ustrcmp(debug_options[i].name, US "macros") == 0)
    continue;
  if ((j++ & 7) == 0) PF("\n ");
  (void)printf(" %s", debug_options[i].name);
  }
PF("\n");

PF("\nEXAMPLES\n\n");
PF("pmw myscore\n");
PF("pmw -s 1,2-4 -p 3,6-10,11 -f small -c 2 k491.pmw\n");
PF("pmw -pdf -pamphlet -a5ona4 scorefile\n");
PF("pmw -s 1 -midi zz.mid -mm 2 -mb 10-20 sonata\n");
#if SUPPORT_XML
PF("pmw -xml quartet.xml -xm 2 quartet.pmw\n");
#endif
}



/*************************************************
*             Decode command line                *
*************************************************/

/* -V and -help act immediately; otherwise the values from the command line
options are placed in appropriate variables.

Arguments:
  argc        the (possibly modified) command line argc
  argv        the (possibly modified) command line argv

Returns:      nothing; errors are hard
*/

static void
decode_command(int argc, char **argv)
{
arg_result results[MAX_COMMANDARGS];
int rc = rdargs(argc, argv, arg_pattern, results, MAX_COMMANDARGS);

if (rc != 0)
  {
  if (results[0].text == NULL)
    error(ERR28, "", "", "", results[1].text);  /* Hard */
  else
    error(ERR28, "\"", results[0].text, "\" ", results[1].text);  /* Hard */
  }

/* -norc is invalid here. It must be the first option and if present is handled
earlier. Give an explanation. */

if (results[arg_norc].number != 0) error(ERR29);   /* Hard */

/* Deal with -V */

if (results[arg_V].number != 0)
  {
  printf("PMW version %s\n%s\n", PMW_VERSION, COPYRIGHT);
  exit(EXIT_SUCCESS);
  }

/* Deal with -C */

if (results[arg_C].text != NULL)
  {
  if (strcmp(results[arg_C].text, "b2pf") == 0)
    {
    printf("%d\n", SUPPORT_B2PF);
    exit(SUPPORT_B2PF);
    }

  if (strcmp(results[arg_C].text, "musicxml") == 0 ||
      strcmp(results[arg_C].text, "xml") == 0)
    {
    printf("%d\n", SUPPORT_XML);
    exit(SUPPORT_XML);
    }

  printf("** Unknown -C option '%s'\n", results[arg_C].text);
  exit(EXIT_FAILURE);
  }

/* Deal with -help */

if (results[arg_help].number != 0)
  {
  givehelp();
  exit(EXIT_SUCCESS);
  }

/* When PMW is being run as part of a MacOS app, the default locations for its
resources are held within the app instead of being in (e.g.) /usr/local/share.
The path to the relevant directory is passed via the -MacOSapp option. We
adjust all the defaults here. If the app sets any overrides, they will be
honoured. */

if (results[arg_MacOSapp].text != NULL)
  {
  uschar *resources = US results[arg_MacOSapp].text;
  size_t rlen = Ustrlen(resources);

  font_data_default = resources;
  font_music_default = resources;
  stdmacs_dir = resources;

  midi_perc = mem_get(rlen + 9);
  sprintf(CS midi_perc, "%s/MIDIperc", resources);

  midi_voices = mem_get(rlen + 11);
  sprintf(CS midi_voices, "%s/MIDIvoices", resources);

  ps_header = mem_get(rlen + 9);
  sprintf(CS ps_header, "%s/PSheader", resources);
  }

/* Deal with verifying and debugging */

if (results[arg_v].number != 0)
  {
  (void)eprintf( "PMW version %s\n", PMW_VERSION);
  main_showid = FALSE;
  main_verify = TRUE;
  }

if ((main_testing = results[arg_testing].number) != 0)
  main_showid = FALSE;

if (results[arg_d].text != NULL)
  {
  debug_selector |= D_any;
  decode_bitselector("-d", results[arg_d].text + 2, debug_options,
    DEBUG_OPTIONS_COUNT, &debug_selector);   /* Any errors are hard */
  }

if (results[arg_dbd].text != NULL)
  {
  char trail[8];
  if (strspn(results[arg_dbd].text, "0123456789,") !=
      strlen(results[arg_dbd].text)) error(ERR162);   /* Hard */
  switch (sscanf(results[arg_dbd].text, "%d,%d,%d%1s", &dbd_movement,
          &dbd_stave, &dbd_bar, trail))
    {
    case 1:                           /* One value is a bar number */
    dbd_bar = dbd_movement;
    dbd_movement = dbd_stave = 1;
    break;

    case 2:                           /* Two values are stave, bar */
    dbd_bar = dbd_stave;
    dbd_stave = dbd_movement;
    dbd_movement = 1;
    break;

    case 3:                           /* Three values are movt, stave, bar */
    break;

    default:
    error(ERR162);   /* Hard */
    break;
    }
  debug_selector |= D_bar;
  }

if (results[arg_dtp].presence != arg_present_not)
  main_tracepos = (results[arg_dtp].number < 0)?
    INT32_MAX : (uint32_t)(results[arg_dtp].number);

/* Deal with -from and -o */

if (results[arg_aa_input].text != NULL)
  main_filename = US results[arg_aa_input].text;

if (results[arg_o].text != NULL)
  out_filename = US results[arg_o].text;

/* Deal with overriding music fonts, fontmetrics, and psheader, MIDIperc,
MIDIvoices, and StdMacs files */

if (results[arg_F].text != NULL)
  font_data_extra = US results[arg_F].text;

if (results[arg_H].text != NULL)
  ps_header = CUS results[arg_H].text;

if (results[arg_MF].text != NULL)
  font_music_extra = US results[arg_MF].text;

if (results[arg_MP].text != NULL)
  midi_perc = US results[arg_MP].text;

if (results[arg_MV].text != NULL)
  midi_voices = US results[arg_MV].text;

if (results[arg_SM].text != NULL)
  stdmacs_dir = US results[arg_SM].text;

/* Deal with XML output */

if (results[arg_musicxml].text != NULL)
  {
#if SUPPORT_XML
  outxml_filename = US results[arg_musicxml].text;
#else
  error(ERR3, "MusicXML output");
#endif
  }

if (results[arg_musicxmlmovement].presence != arg_present_not)
  {
#if SUPPORT_XML
  if (outxml_filename == NULL)
    error(ERR193, "-xmlmovement");
  else
    {
    outxml_movement = results[arg_musicxmlmovement].number;
    xml_movement_set = TRUE;
    }
#else
  error(ERR3, "MusicXML output");
#endif
  }

if (results[arg_x].text != NULL)
  {
#if SUPPORT_XML
  decode_bitselector("-x", results[arg_x].text + 2, xout_options,
    XOUT_OPTIONS_COUNT, &main_xmloptions);   /* Any errors are hard */
#else
  error(ERR3, "MusicXML output");
#endif
  }

/* Deal with MIDI output */

if (results[arg_midi].text != NULL)
  midi_filename = US results[arg_midi].text;

if (results[arg_midibars].text != NULL)
  {
  uschar *endptr;
  midi_startbar = Ustrtoul(results[arg_midibars].text, &endptr, 10) << 16;
  if (*endptr == 0) midi_endbar = midi_startbar; else
    {
    if (*endptr == '.') midi_startbar |= Ustrtoul(endptr+1, &endptr, 10);
    if (*endptr == 0) midi_endbar = midi_startbar; else
      {
      if (*endptr != '-') error(ERR158);     /* Hard */
      midi_endbar = Ustrtoul(endptr+1, &endptr, 10) << 16;
      if (*endptr == '.') midi_endbar |= Ustrtoul(endptr+1, &endptr, 10);
      if (*endptr != 0) error(ERR158);         /* Hard */
      }
    }
  }

if (results[arg_midimovement].presence != arg_present_not)
  midi_movement = results[arg_midimovement].number;

/* Error limit is adjustable, mainly for testing */

if (results[arg_em].presence != arg_present_not)
  error_maximum = results[arg_em].number;

/* Some BOOL options */

if (results[arg_nowidechars].number != 0) stave_use_widechars = FALSE;
if (results[arg_drawbarlines].number != 0) bar_use_draw = TRUE;
if (results[arg_norepeats].number != 0) midi_repeats = FALSE;

/* Draw stave lines instead of using font characters: the thickness can
optionally be altered. */

if (results[arg_drawstavelines].presence != arg_present_not)
  stave_use_draw = results[arg_drawstavelines].number;

/* Deal with stave selection */

if (results[arg_s].text != NULL)
  {
  uschar *endptr;
  (void)read_stavelist(US results[arg_s].text, &endptr, &main_selectedstaves,
    NULL);
  main_selectedstaves |= 1;  /* Stave 0 is always selected */
  if (*endptr != 0) error(ERR30, "stave");  /* Hard error */
  }

/* Deal with page selection */

if (results[arg_p].text != NULL)
  {
  uschar *endptr;
  int errnum = read_stavelist((uschar *)(results[arg_p].text), &endptr, NULL,
    &print_pagelist);
  if (errnum != 0)
    {
    error_inoption = "-p";
    error(errnum);
    }
  else if (*endptr != 0) error(ERR30, "page");  /* Hard error */
  }

/* Deal with transposition */

if (results[arg_t].presence != arg_present_not)
  {
  main_transpose = results[arg_t].number;
  if (abs(main_transpose) > MAX_TRANSPOSE)
    error(ERR64, "", main_transpose, MAX_TRANSPOSE);  /* Hard error */
  main_transpose *= 2;  /* Convert semitones into quarter tones */
  active_transpose = main_transpose;
  }

/* Deal with format */

if (results[arg_f].text != NULL)
  {
  main_format = mem_copystring(US results[arg_f].text);
  for (uschar *s = main_format; *s != 0; s++) *s = tolower(*s);
  }

/* Deal with copies */

if (results[arg_c].presence != arg_present_not)
  print_copies = results[arg_c].number;

/* Deal with a number of printing configuration options */

if (results[arg_reverse].number != 0) print_reverse = TRUE;
if (results[arg_a4sideways].number != 0) print_pagefeed = pc_a4sideways;
if (results[arg_a4ona3].number != 0) print_imposition = pc_a4ona3;
if (results[arg_a5ona4].number != 0) print_imposition = pc_a5ona4;
if (results[arg_pamphlet].number != 0) print_pamphlet = TRUE;
if (results[arg_incPMWfont].number != 0) print_incPMWfont = TRUE;
if (results[arg_manualfeed].number != 0) print_manualfeed = TRUE;
if (results[arg_duplex].number != 0) print_duplex = TRUE;
if (results[arg_tumble].number != 0) print_tumble = TRUE;

if (results[arg_eps].number != 0)
  {
  print_imposition = pc_EPS;
  EPSforced = TRUE;
  }
if (results[arg_printadjustx].text != NULL)
  {
  float d;
  sscanf(results[arg_printadjustx].text, "%g", &d);
  print_image_xadjust = (int)(1000.0 * d);
  }

if (results[arg_printadjusty].text != NULL)
  {
  float d;
  sscanf(results[arg_printadjusty].text, "%g", &d);
  print_image_yadjust = (int)(1000.0 * d);
  }

if (results[arg_printgutter].text != NULL)
  {
  float d;
  sscanf(results[arg_printgutter].text, "%g", &d);
  print_gutter = (int)(1000.0 * d);
  }

if (results[arg_printscale].text != NULL)
  {
  float d;
  sscanf(results[arg_printscale].text, "%g", &d);
  print_magnification = (int)(1000.0 * d);
  if (print_magnification == 0) error(ERR140);  /* Hard */
  }

if (results[arg_printside].presence != arg_present_not)
  {
  int n = results[arg_printside].number;
  if (n == 1) print_side2 = FALSE;
    else if (n == 2) print_side1 = FALSE;
      else error(ERR141);  /* Hard */
  }

/* Only one of -ps or -pdf is allowed; default is set at build time. We need to
remember if the format is forced in order to handle the header directives that
are equivalent to the command line options. */

if (results[arg_pdf].number != 0)
  {
  if (results[arg_ps].number != 0) error(ERR181, "ps");  /* Hard */
  PDF = PDFforced = TRUE;
  }
else if (results[arg_ps].number != 0 || print_imposition == pc_EPS)
  {
  PDF = FALSE;
  PSforced = TRUE;
  }

/* Many PostScript-specific args are either ignored or cause a fatal error when
output is PDF. */

if (PDF)
  {
  if (print_imposition == pc_EPS) error(ERR181, "eps");  /* Hard */
  if (print_copies != 1) error(ERR182, "c"); /* Warning */
  if (print_pagefeed == pc_a4sideways)
    {
    error(ERR182, "a4sideways");                        /* Warning */
    print_pagefeed = pc_normal;
    }
  if (print_duplex) error(ERR182, "duplex");            /* Warning */
  if (print_tumble) error(ERR182, "tumble");            /* Warning */
  if (print_manualfeed) error(ERR182, "manualfeed");    /* Warning */
  if (results[arg_H].text != NULL) error(ERR182, "H");  /* Warning */
  if (print_incPMWfont) error(ERR182, "incPMWfont");    /* Warning */
  }
}



/*************************************************
*           Command line initialization          *
*************************************************/

/* This is called before argument decoding is done. It is passed the argument
list, and it has the opportunity of modifying that list as it copies it into a
new vector. Unless configured otherwise, we search for a .pmwrc file and stuff
it on the front of the arguments.

Arguments:
  argv          argv from main()
  nargv         where to return the possibly modified argv

Returns:        new argc value
*/

static int
init_command(char **argv, char **nargv)
{
int ap = 0;
int nargc = 0;

nargv[nargc++] = argv[ap++];    /* Program name */

if (argv[1] != NULL && strcmp(argv[1], "-norc") == 0)
  {
  (void)arg_pattern;   /* Not used on this path */
  ap++;                /* Skip over -norc if it's first; don't read the file */
  }

/* Processing ~/.pmwrc needs to be cut out on non-Unix-like systems. If -norc
is given, it doesn't matter because that's what is happening by default. */

#if !defined NO_PMWRC || NO_PMWRC == 0
else
  {
  struct passwd *pw = getpwuid(geteuid());
  if (pw != NULL)
    {
    uschar buff[256];
    struct stat statbuf;

    Ustrcpy(buff, pw->pw_dir);
    Ustrcat(buff, "/.pmwrc");

    if (stat(CS buff, &statbuf) == 0)
      {
      arg_result results[64];
      FILE *f = Ufopen(buff, "r");

      /* Failure to open a file that statted OK is a hard error */

      if (f == NULL) error(ERR23, buff, strerror(errno));

      /* Add items from the file */

      while (fgets(CS buff, sizeof(buff), f) != NULL)
        {
        uschar *p = buff;
        while (isspace(*p)) p++;
        while (*p != 0)
          {
          uschar *pp = p;
          while (*p != 0 && !isspace(*p)) p++;
          nargv[nargc] = mem_get(p - pp + 1);
          Ustrncpy(nargv[nargc], pp, p - pp);
          nargv[nargc++][p-pp] = 0;
          while (isspace(*p)) p++;
          }
        }
      fclose(f);

      /* Check that what we have obtained from the .pmwrc file is a complete
      set of options; we don't want to end up with one that expects a data
      value, because that would subvert the argument on the real command line,
      possibly doing damage. */

      if (rdargs(nargc, nargv, arg_pattern, results, MAX_COMMANDARGS) != 0)
        error(ERR26, results[0].text, results[1].text);  /* Hard */
      }

    /* stat() problem other than file not found is serious */

    else if (errno != ENOENT) error(ERR27, buff, strerror(errno));  /* Hard */
    }
  }
#endif  /* NO_PMWRC */

/* Copy the remaining stuff from the original command line, add the terminating
NULL argument, and return the new count. */

while (argv[ap] != NULL) nargv[nargc++] = argv[ap++];
nargv[nargc] = NULL;

return nargc;
}



/*************************************************
*              Initialize                        *
*************************************************/

/* Sets up memory management, certain buffers etc.

Arguments: none
Returns:   TRUE if all went well
*/

static BOOL
initialize(void)
{
usint i;

TRACE("Initialize\n");

/* Input buffers can expand if necessary. */

main_readbuffer = malloc(main_readbuffer_size);
main_readbuffer_previous = malloc(main_readbuffer_size);
main_readbuffer_raw = malloc(main_readbuffer_size);
if (main_readbuffer == NULL || main_readbuffer_previous == NULL ||
    main_readbuffer_raw == NULL)
  error(ERR0, "", "initial line buffers", main_readbuffer_size);  /* Hard */
main_readbuffer[0] = main_readbuffer_previous[0] = 0;

/* So can macro argument buffers */

for (i = 0; i < MAX_MACRODEPTH; i++)
  {
  main_argbuffer[i] = NULL;
  main_argbuffer_size[i] = 0;
  }

/* Get fixed-size memory blocks */

read_baraccs = mem_get(BARACCS_LEN * sizeof(int8_t));
read_baraccs_tp = mem_get(BARACCS_LEN * sizeof(int8_t));

read_beamstack = mem_get(BEAMSTACKSIZE * sizeof(b_notestr **));
read_stemstack = mem_get(STEMSTACKSIZE * sizeof(b_notestr **));

/* Set up the default fonts */

font_addfont(US"Times-Roman", font_rm, 0);
font_addfont(US"Times-Italic", font_it, 0);
font_addfont(US"Times-Bold", font_bf, 0);
font_addfont(US"Times-BoldItalic", font_bi, 0);
font_addfont(US"Symbol", font_sy, 0);
font_addfont(US"PMW-Music", font_mf, 0);

/* Initialize MIDI data */

midi_translate(&midi_voicenames, midi_voices);
midi_translate(&midi_percnames, midi_perc);

/* Initialize for B2PF if supported */

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
font_b2pf_contexts = mem_get(font_tablen * sizeof(b2pf_context *));
font_b2pf_options = mem_get(font_tablen * sizeof(uint32_t));
for (i = 0; i < font_tablen; i++)
  {
  font_b2pf_contexts[i] = NULL;
  font_b2pf_options[i] = 0;
  }
#endif

return TRUE;
}



/*************************************************
*           Exit tidy-up function                *
*************************************************/

/* Automatically called for any exit. Close the input file if it is open, then
free the extensible buffers and other memory.

Arguments: none
Returns:   nothing
*/

static void
tidy_up(void)
{
if (read_filehandle != NULL) fclose(read_filehandle);
free(font_list);

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
if (font_b2pf_contexts != NULL)
  {
  for (usint i = 0; i < font_tablen; i++)
    if (font_b2pf_contexts[i] != NULL) b2pf_context_free(font_b2pf_contexts[i]);
  }
#endif

free(main_readbuffer_raw);
free(main_readbuffer_previous);
free(main_readbuffer);
free(out_textqueue);
free(read_stringbuffer);

/* A NULL pointer marks the end of the macro argument expansion buffers. */

for (usint i = 0; i < MAX_MACRODEPTH; i++)
  if (main_argbuffer[i] != NULL) free(main_argbuffer[i]); else break;

/* Free expandable vectors in all movements, then the movements vector. */

for (usint i = 0; i < movement_count; i++)
  {
  curmovt = movements[i];
  free(curmovt->barvector);
  if (MFLAG(mf_midistart)) free(curmovt->midistart);
  for (int j = 0; j <= curmovt->lastreadstave; j++)
    if (curmovt->stavetable[j] != NULL) free(curmovt->stavetable[j]->barindex);
  }
free(movements);

/* An expandable XML buffer */

#if SUPPORT_XML
free(xml_layout_list);
#endif

/* PDF expandable data areas */

pdf_free_data();

/* Free the non-expandable memory blocks */

mem_free();
}



/*************************************************
*                   Entry point                  *
*************************************************/

/* The command line arguments can be modified by the contents of the user's
.pmwrc file, unless -norc is given first. This is handled in the
init_command() function, which hands back a possibly modified set of arguments.
All errors detected in init_command() and decode_command() are hard. */

int
main(int argc, char **argv)
{
(void) argc;

if (atexit(tidy_up) != 0) error(ERR25);  /* Hard */

newargv = mem_get(MAX_COMMANDARGS * sizeof(char *));
decode_command(init_command(argv, newargv), newargv);

if (!initialize()) exit(EXIT_FAILURE);

/* If there is a file name, open it. If no output file is specified, default it
to the input name with a .ps extension. */

if (main_filename != NULL)
  {
  read_filehandle = Ufopen(main_filename, "r");
  if (read_filehandle == NULL)
    error(ERR23, main_filename, strerror(errno));  /* Hard */
  read_filename = main_filename;

  if (out_filename == NULL)
    {
    uschar *p, *q;
    size_t len = Ustrlen(main_filename);
    out_filename = mem_get(len + 5);
    Ustrcpy(out_filename, main_filename);
    if ((p = Ustrrchr(out_filename, '.')) != NULL &&
        ((q = Ustrrchr(out_filename, '/')) == NULL || q < p))
      len = p - out_filename;
    Ustrcpy(out_filename + len, PDF? ".pdf" : ".ps");
    }
  }

else
  {
  read_filehandle = stdin;
  read_filename = US "<stdin>";
  }

/* Read the input file */

main_state = STATE_READ;
if (main_verify) eprintf( "Reading input file\n");
read_file(FT_AUTO);
main_truepagelength = main_pagelength;  /* Save unscaled value */

/* Give up if no data was supplied */

if (movements == NULL)
  {
  eprintf("** No input data supplied\n");
  main_suppress_output = TRUE;
  }

/* If all went well, set up the working continuation vector and do the
pagination. */

if (!main_suppress_output)
  {
  main_state = STATE_PAGINATE;
  wk_cont = mem_get_independent((main_maxstave+1)*sizeof(contstr));
  if (main_verify) eprintf( "Paginating\n");
  paginate();
  }

/* Give up after a serious error. */

if (main_suppress_output)
  {
  eprintf( "** No output generated\n");
  return(EXIT_FAILURE);
  }

/* Show pagination information if verifying */

if (main_verify) display_info();

/* If a file name other than "-" is set for the output, open it. Otherwise
we'll be writing to stdout. */

if (out_filename != NULL && Ustrcmp(out_filename, "-") != 0)
  {
  if (main_verify) eprintf( "\nWriting %s file \"%s\"\n",
    PDF? "PDF" : "PostScript", out_filename);
  out_file = Ufopen(out_filename, "w");
  if (out_file == NULL)
    error(ERR23, out_filename, strerror(errno));  /* Hard error */
  }
else
  {
  out_file = stdout;
  if (main_verify)
    eprintf( "\nWriting %s to stdout\n", PDF? "PDF" : "PostScript");
  }

/* Set up for printing, and go for it */

print_lastpagenumber = main_lastpagenumber;
if (print_pamphlet) print_lastpagenumber = (print_lastpagenumber + 3) & (-4);

/* This diagram shows the computed values and the positions where the origin
can go in each case. In practice we take the upper value where there are two
possibilities.

 ------------ Sideways -------------   |   ------------ Upright -----------
 ----- 1-up -----   ----- 2-up -----   |   ---- 1-up ----    ---- 2-up ----
  Port     Land      Port     Land     |    Port     Land     Port     Land
 x------  -------   -------  ---x---   |   -----    x----    x----    -----
 |  0  |  |  4  |   |  2  |  |  6  |   |   |   |    |   |    |   |    |   |
 ------x  x------   x------  ---x---   |   | 1 |    | 5 |    | 3 |    x 7 |
                                       |   |   |    |   |    |   |    |   |
                                       |   x----    -----    ----x    -----
*/

print_pageorigin =
  ((print_pagefeed == pc_a4sideways)? 0 : 1) +
  ((print_imposition == pc_normal)? 0 : 2) +
  (main_landscape? 4 : 0);

main_state = STATE_WRITE;
if (PDF) pdf_go(); else ps_go();
if (out_file != stdout) fclose(out_file);
main_state = STATE_ENDING;

DEBUG(D_barO) debug_bar("After writing main output");

/* Output warning if coupled staves were not spaced by a multiple of 4 */

if (main_error_136) error(ERR136);

/* Write MIDI output if required */

if (midi_filename != NULL)
  {
  if (main_verify) eprintf("Writing MIDI file \"%s\"\n", midi_filename);
  midi_write();
  }

/* Write MusicXML output if required. MusicXML supports only one movement per
file. The -xm option allows for a single movement selection. If this is not
set, and there is more than one movement, and the file name contains
%<digits>d, all movements are output to separate files with each movement
number inserted into the file name. */

#if SUPPORT_XML
if (outxml_filename != NULL)
  {
  BOOL multimovt = FALSE;

  if (unclosed_slurline) error(ERR198);  /* Hard */

  /* See if the file name conforms and -xm was not used */

  if (movement_count > 1 && !xml_movement_set)
    {
    char *p = strchr(CS outxml_filename, '%');
    if (p != NULL)
      {
      while (isdigit(*(++p))) {};
      if (*p == 'd') multimovt = TRUE;
      }
    }

  /* Write multiple files */

  if (multimovt)
    {
    uschar *basename = outxml_filename;
    uschar buffer[256];

    outxml_filename = buffer;

    for (outxml_movement = 1; outxml_movement <= (int)movement_count;
         outxml_movement++)
      {
      sprintf(CS buffer, CS basename, outxml_movement);
      if (main_verify) eprintf("Writing MusicXML file \"%s\"\n", outxml_filename);
      outxml_write(TRUE);
      }
    }

  /* Write a single file */

  else
    {
    if (main_verify) eprintf("Writing MusicXML file \"%s\"\n", outxml_filename);
    outxml_write(FALSE);
    }
  }

outxml_write_ignored();
#endif

if (main_verify) eprintf( "PMW done\n"); else TRACE("Done\n");

DEBUG(D_memory) debug_memory_usage();
exit(EXIT_SUCCESS);
}

/* End of main.c */
