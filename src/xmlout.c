/*************************************************
*          PMW MusicXML output generation        *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: August 2025 */
/* This file last modified: August 2025 */

#include "pmw.h"


/* This file contains code for writing a MusicXML file. */

#define SEPARATOR \
  "<!--===============================================================-->"


/*************************************************
*             Local variables                    *
*************************************************/

static int       indent = 0;
static FILE     *xml_file;
static movtstr  *xml_movt;
static uint64_t  xml_staves = ~0uL;



/*************************************************
*            Write text with indent              *
*************************************************/

/* Optionally adjust the indent, then write the text at that indent to the XML
output file, followed by a newline. The central function is called from three
wrappers for common cases.

Arguments:
  delta     indent adjustment
  format    format string
  ...       arguments

Returns:    nothing
*/

static void
xprintf(int delta, const char *format, va_list ap)
{
indent += delta;
for (int i = 0; i < indent; i++) fputc(' ', xml_file);
(void)vfprintf(xml_file, format, ap);
va_end(ap);
(void)fprintf(xml_file, "\n");
}

static void
P0(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(0, format, ap);
}

static void
P2(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(2, format, ap);
}

static void
PB(const char *format, ...)
{
va_list ap;
va_start(ap, format);
xprintf(-2, format, ap);
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

/* Write header boilerplate */

now = time(NULL);
strftime(datebuff, sizeof(datebuff), "%Y-%m-%d", localtime(&now));

fprintf(xml_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.1 Partwise//EN\""
  " \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
  "<score-partwise version=\"3.1\">\n");

/* Identification. Note that P2 increases indent by 2, P0 does not change
indent, and PB backs up by reducing the indent by 2. There is always an
automatic newline. */

P2("<identification>");
P2("<encoding>");
P2("<software>PMW %s</software>", PMW_VERSION);
P0("<encoding-date>%s</encoding-date>", datebuff);
PB("</encoding>");
PB("</identification>");

/* Next comes various defaults */

P0("<defaults>");

/* The MusicXML staff is set at 40 tenths and the millimeters item specifies
how many millimeters this should scale to. We set this value from the PMW
magnification, knowing that the unmagnified PMW staff is 16 points (16*0.3528
millimeters). */

P2("<scaling>");
P2("<millimeters>%.4f</millimeters>",
  16.0*0.3528*(double)main_magnification/1000.0);
P0("<tenths>40</tenths>");
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

P0("<page-layout>");
P2("<page-height>%d</page-height>", converted_pagelength);
P0("<page-width>%d</page-width>", converted_linelength);

P0("<page-margins type=\"both\">");
P2("<left-margin>%d</left-margin>", hmargin);
P0("<right-margin>%d</right-margin>", hmargin);
P0("<top-margin>%d</top-margin>", vmargin);
P0("<bottom-margin>%d</bottom-margin>", vmargin);
PB("</page-margins>");
PB("</page-layout>");

// TODO Any need for system-layout?
// TODO Any need for appearance?
// TODO Any need for music-font?

// TODO May need to set word-font and lyric-font

PB("</defaults>");

/* Output a list of parts, mapping each PMW stave to a part. */

P0("<part-list>");

for (int stave = 1; stave <= xml_movt->laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  P2("<score-part id=\"P%d\">", stave);

// TODO score-instrument - get stave name
// TODO midi-instrument - if anything set

  P0("</score-part>");
  }

PB("</part-list>");
P0(SEPARATOR);

/* Now we can output each stave as a "part". */

for (int stave = 1; stave <= xml_movt->laststave; stave++)
  {
  if (mac_notbit(xml_staves, stave)) continue;

  P0("<part id=\"P%d\">", stave);

// HERE WE NEED EACH MEASURE

  P0("</part>");
  P0(SEPARATOR);
  }


#ifdef NEVER

part id="P1"
  measure number="1" width="??"
    print
      system-layout
        top-system-distance
      /system-layout
      measure-numbering
    /print
    attributes
      divisions
      key
        fifths
        mode
      /key
      time
        beats
        beat-type
      time
      clef
        sign
        line
      /clef
    /attributes
    sound-tempo
    note
      rest measure="yes"
      duration
      voice
    /note

    note default-x
      pitch
        step
        octave
      pitch
      duration
      voice
      type
      stem default
      lyric default-y justify number
        syllabic
        text
      /lyric
    /note
  /measure

... other measures

/part
#endif






/* Write ending boilerplate and close the file. */

PB("</score-partwise>");
fclose(xml_file);
}

/* End of xmlout.c */
