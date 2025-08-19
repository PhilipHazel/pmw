/*************************************************
*            PMW error message handling          *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: August 2025 */

#include "pmw.h"

#define STRING(a)  # a
#define XSTRING(s) STRING(s)


/*************************************************
*             Static variables                   *
*************************************************/

static usint  error_count = 0;
static usint  warning_count = 0;
static BOOL   suppress_warnings = FALSE;



/*************************************************
*   Error and warning texts   *
*************************************************/

/* Error severities are defined in pmw.h:

ec_warning      Sic
ec_minor        Soft error - can continue and produce output
ec_major        Soft error - can continue, but no output
ec_failed       Hard error - cannot continue

HOWEVER: all drawing errors are marked ec_major rather than ec_failed so
that the drawing function can output the drawing stack afterwards, before it
aborts the program. */

typedef struct {
  uschar ec;
  const char *text;
} error_struct;

static error_struct error_data[] = {
/* 0-4 */
{ ec_failed,  "failed to %sallocate memory for %s (%u bytes)" },
{ ec_failed,  "internal error: memory request too large (%lu bytes, max %lu)" },
{ ec_major,   "binary zero encountered in input: ignored" },
{ ec_failed,  "%s files are not supported by this version of PMW" },
{ ec_failed,  "%s file not allowed here" },
/* 5-9 */
{ ec_failed,  "%s buffer size limit reached (%u bytes)" },
{ ec_major,   "pre-processing directive expected" },
{ ec_failed,  "%s is too long (max %d)" },
{ ec_major,   "%s expected" },  /* Can carry on */
{ ec_failed,  "%s expected" },  /* Cannot carry on */
/* 10-14 */
{ ec_major,   "stave number too large - maximum is " XSTRING(MAX_STAVE) },
{ ec_major,   "unexpected %s" },
{ ec_major,   "unknown pre-processing directive \"%s\"" },
{ ec_warning, "%s" },
{ ec_failed,  "too many default macro arguments (max %d)" },
/* 15-19 */
{ ec_major,   "closing bracket missing in macro argument list" },
{ ec_major,   "the name \"%s\" is already defined" },
{ ec_major,   "the name \"%s\" has not been defined" },
{ ec_major,   "macro name or string repetition expected after \"&\"" },
{ ec_failed,  "too many macro arguments (max %d)" },
/* 20-24 */
{ ec_major,   "a &* replication must have only one argument - others ignored" },
{ ec_major,   "missing \"*fi\" at end of file" },
{ ec_failed,  "macro calls nested too deep (max %d)" },
{ ec_failed,  "unable to open file \"%s\": %s" },
{ ec_major,   "unknown header directive \"%s\"" },
/* 25-29 */
{ ec_failed,  "call to atexit() failed" },
{ ec_failed,  "malformed options in .pmwrc file: \"%s\" %s" },
{ ec_failed,  "unable to access %s: %s" },
{ ec_failed,  "failed to decode command line: %s%s%s%s"
              "\n** Use -help for a list of options" },
{ ec_failed,  "-norc must be given as the first option" },
/* 30-34 */
{ ec_failed,  "malformed %s selection on command line" },
{ ec_failed,  "malformed -d option: + or - expected but found \"%s\"" },
{ ec_failed,  "unknown debug selector setting: %c%.*s" },
{ ec_failed,  "included files too deeply nested (max depth is %d)" },
{ ec_major,   "numbers out of order" },
/* 35-39 */
{ ec_major,   "font stretching or shearing is not %s %s" },
{ ec_major,   "%s overflow" },
{ ec_failed,  "extra font number is too big (max %d)" },
{ ec_major,   "%s value is too big (max %d)" },
{ ec_warning, "\"%s\" is obsolete and has no effect" },
/* 40-44 */
{ ec_major,   "\"%s\" can be specified only in the first movement" },
{ ec_failed,  "cannot halve C or A time signature" },
{ ec_failed,  "invalid time signature" },
{ ec_major,   "unsupported key signature" },
{ ec_major,   "custom key name X1 or X2 or ... X%d expected" },
/* 45-49 */
{ ec_major,   "too many accidentals in custom key signature (max %d)" },
{ ec_major,   "letter change value %d is too large for transpose value %d" },
{ ec_failed,  "internal error: unknown layout value %d" },
{ ec_major,   "incorrect MIDI %s number %d (must be between 1 and %d inclusive)" },
{ ec_major,   "unrecognized MIDI %s name: \"%s\"" },
/* 50-54 */
{ ec_major,   "too many tempo changes (maximum number is %d)" },
{ ec_major,   "tempo changes must be in ascending order of bar numbers" },
{ ec_major,   "'p' cannot be used as part of a chord" },
{ ec_failed,  "incorrect PMW version (%c %s expected, %s used)" },
{ ec_warning, "stave %d specified (or defaulted) more than once in \"%s\" directive" },
/* 55-59 */
{ ec_minor,   "setting %s for stave 0 is not allowed" },
{ ec_failed,  "%s file for font \"%s\" not found in \"%s\"" },
{ ec_failed,  "%s file for font \"%s\" not found in \"%s\" or \"%s\"" },
{ ec_failed,  "error while reading font metrics for \"%s\": %s%s" },
{ ec_major,   "too many Unicode translations in %s (max %d)" },
/* 60-64 */
{ ec_major,   "missing %s code value in line %d of %s\n%s" },
{ ec_major,   "invalid font code value (> %d) in line %d of %s\n%s" },
{ ec_major,   "duplicate Unicode code point U+%04x in %s\n   Output will be unpredictable" },
{ ec_major,   "too many text sizes (maximum %d)" },
{ ec_failed,  "%stransposition value (%d) is too large (max %d)\n" },
/* 65-69 */
{ ec_failed,  "unexpected end of file while reading string" },
{ ec_warning, "invalid UTF-8 byte 0x%02X interpreted as a single-byte character" },
{ ec_warning, "\\x1\\ etc in a string is obsolete; please use \\xx1\\ etc instead" },
{ ec_major,   "%s expected - skipping to end of string" },
{ ec_major,   "unrecognized escape sequence" },
/* 70-74 */
{ ec_major,   "the drawing function \"%s\" has not been defined" },
{ ec_failed,  "internal error - transposition (%d, %d, %d, %d)" },
{ ec_failed,  "letter change value %d is too large for transpose value %d" },
{ ec_failed,  "cannot transpose custom key X%d by %d without KeyTranspose instruction" },
{ ec_failed,  "cannot transpose key signature by quarter tone without KeyTranspose instruction" },
/* 75-79 */
{ ec_major,   "invalid text size (a number in the range 1 to %d was expected)" },
{ ec_failed,  "this version of PMW was compiled without B2PF support" },
{ ec_failed,  "font is already configured for B2PF" },
{ ec_major,   "unknown B2PF option \"%s\"" },
{ ec_failed,  "B2PF context creation failed: %s" },
/* 80-84 */
{ ec_failed,  "B2PF processing failed for \"%s\": %s" },
{ ec_major,   "unmatched closing curly bracket in drawing function \"%s\"" },
{ ec_major,   "too many draw variables defined (limit is %d)" },
{ ec_major,   "\"%s\" is not a known variable or operator name in a draw item" },
{ ec_failed,  "stave number %d is too large - maximum is " XSTRING(MAX_STAVE) },
/* 85-89 */
{ ec_failed,  "stave %d is supplied twice" },
{ ec_major,   "unknown stave directive \"%s\"" },
{ ec_warning, "%s repeat at %s of bar - misplaced bar line?" },
{ ec_major,   "mis-placed beam break '%c' (does not immediately follow a note)" },
{ ec_major,   "unexpected character '%c'" },
/* 90-94 */
{ ec_major,   "mis-matched curly brackets" },
{ ec_major,   "can't have both staccato and staccatissimo" },
{ ec_major,   "error in note expression or ornament" },
{ ec_major ,  "follow-on string not permitted here" },
{ ec_warning, "hyphen string setting on non-underlay/overlay string ignored" },
/* 95-99 */
{ ec_warning, "follow-on ignored for %s" },
{ ec_warning, "/h and /rc or /lc specified - the latter ignored" },
{ ec_major,   "font rotation is not %s %s" },
{ ec_failed,  "internal error: special character 0x%08x unknown" },
{ ec_major,   "/%c may not appear in data for a split section of a slur" },
/* 100-104 */
{ ec_major,   "wiggly %s are not supported" },
{ ec_major,   "incorrect bar length: too %s by %s" },
{ ec_failed,  "unexpected [newmovement] - has an [endstave] been omitted?" },
{ ec_warning, "[skip] should normally be at the start of a bar - barline missing?" },
{ ec_warning, "note spacing changed for breves only - is this really what\n"
                "   was intended? (Perhaps \"*\" has been omitted?)" },
/* 105-109 */
{ ec_major,   "[%s] may not occur before the first note of a bar" },
{ ec_major,   "[%s] may not follow an item that relates to the following note" },
{ ec_major,   "[%s] may not appear inside an irregular note group" },
{ ec_major,   "no options are allowed after \"x\" note repetition" },
{ ec_major,   "no previous note to copy, or previous note cannot be copied" },
/* 110-114 */
{ ec_failed,  "too many notes in a chord (%d maximum)" },
{ ec_major,   "accidentals for printing above or below must be on the first note of a chord" },
{ ec_warning, "accidental ignored before \"p\"" },
{ ec_major,   "a chord may not contain a rest" },
{ ec_major,   "! may only follow an upper case letter" },
/* 115-119 */
{ ec_failed,  "cannot handle notes longer than a breve or shorter than a hemidemisemiquaver" },
{ ec_major,   "%s must be on the first note of a chord" },
{ ec_major,   "missing backslash after note options - skipping to end of bar or newline" },
{ ec_minor,   "tie or glissando precedes %s (will carry over to next note)" },
{ ec_major,   "SPARE ERROR" },
/* 120-124 */
{ ec_major,   "conflicting stem direction requests %s" },
{ ec_major,   "the \\sw\\ option is only available for beamed notes when the\n"
              "   stem direction of the first note is forced" },
{ ec_major,   "accidentals, dynamics, ornaments, and irrelevant options may not be specified for rests" },
{ ec_major,   "a small note head may not be specified for a grace note" },
{ ec_major,   "the notes of a chord must all be the same length" },
/* 125-129 */
{ ec_major,   "editorial marks on intermittent slurs or lines are not supported" },
{ ec_major,   "mis-placed curly bracket" },
{ ec_major,   "too many noteheads at the same or adjacent levels in a chord" },
{ ec_failed,  "internal error - unexpected item %d in bar data" },
{ ec_warning, "unexpected bar length of %s\n"
              "   On an earlier stave this bar's length is %s" },
/* 130-134 */
{ ec_warning, "there is insufficient space to print a cautionary key or\n"
              "   time signature at the end of the line" },
{ ec_warning, "a bar is too wide for the page at %s points.\n"
              "   Clefs, keys, etc. at the start occupy %s points. The bar will\n"
              "   be compressed to fit within the line width of %s points." },
{ ec_major,   "%s%s not found for %s" },
{ ec_warning, "cannot decrease page number: attempt to set page %d on page %d" },
{ ec_major,   "the musical system starting at bar %s of movement %d is deeper\n"
              "   than the page length (by %s point%s) and cannot be handled" },
/* 135-139 */
{ ec_major,   "only one of \"newpage\", \"thispage\", or \"thisline\" may be specified" },
{ ec_warning, "one or more coupled notes were encountered where the stave spacing\n"
              "was not a multiple of 4 points (scaled to the size of the staves)\n" },
{ ec_failed,  "malformed page list on command line" },
{ ec_failed,  "page range out of order on command line" },
{ ec_failed,  "internal error: SFF() or SFD() called with invalid %% escape" },
/* 140-144 */
{ ec_failed,  "bad -printscale value on command line" },
{ ec_failed,  "-printside must specify 1 or 2 on command line" },
{ ec_failed,  "internal failure - position data missing for musical offset %d, which is %s.\n" },
{ ec_major,   "insufficient space to print notes on opposite sides of beam" },
{ ec_major,   "attempt to draw slur or line of zero or negative length" },
/* 145-149 */
{ ec_major,   "[%s] cannot be applied to a %s" },
{ ec_failed,  "internal failure - hyphen type not found" },

/* These drawing errors are flagged major so that error() returns control to
its caller in the draw_error() function. However, that function subsequently
aborts the run after outputting additional information. */

{ ec_major,   "draw subroutines too deeply nested%swhile drawing \"%s\"%s" },
{ ec_major,   "stack overflow for \"%s\" while drawing \"%s\"%s" },
{ ec_major,   "stack underflow for \"%s\" while drawing \"%s\"%s" },
/* 150-154 */
{ ec_major,   "wrong data type on stack for \"%s\" while drawing \"%s\"%s" },
{ ec_major,   "closing curly bracket missing%swhile drawing \"%s\"%s" },
{ ec_major,   "invalid argument for conditional or looping command%swhile drawing \"%s\"%s" },
{ ec_major,   "no current point for \"%s\" command while drawing \"%s\"%s" },
{ ec_major,   "%sMisused \"def\" operator while drawing \"%s\"%s" },
/* 155-159 */
{ ec_major,   "%s while drawing \"%s\"%s" },  /* Div by 0 or sqrt -ve */

/* End of drawing errors */

{ ec_failed,  "internal error: buffer full during B2PF processing" },
{ ec_major,   "B2PF processing failed: %s" },
{ ec_failed,  "malformed MIDI bar selection on command line" },
{ ec_major,   "MIDI %s bar %s not found in movement %d: no MIDI data written" },
/* 160-164 */
{ ec_major,   "there are no bars in movement %d: no %s data written" },
{ ec_major,   "incomplete irregular note group at end of bar" },
{ ec_failed,  "malformed -dbd option data (needs 1-3 comma-separated numbers)" },
{ ec_failed,  "-dbd error: %s %d does not exist" },
{ ec_warning, "there is underlay or overlay text left over at the end of stave %d" },
/* 165-169 */
{ ec_minor,   "\"omitempty\" must immediately follow \"stave <n>\"" },
{ ec_minor,   "glissando is not supported after a chord - ignored" },
{ ec_major,   "slur/line identifier must be an ASCII alphanumeric character" },
{ ec_major,   "octave %d is out of PMW's range (-3 to 4)" },
{ ec_warning, "-s on the command line is overridden by \"selectstaves\"" },
/* 170-174 */
{ ec_warning, "[stavelines <n>] is deprecated; use [stave m/<n>] instead" },
{ ec_minor,   "unexpected end of slur or line - ignored" },
{ ec_minor,   "MIDI pitch %d is outside supported range 0-127 in "
              "bar %s stave %d" },
{ ec_failed,  "note pitch is not within supported range" },
{ ec_major,   "invalid repeat style (must be 0-%d or 10-%d)" },
/* 175-179 */
{ ec_major,   "duplicate glyph name \"%s\" in line %d of %s\n%s" },
{ ec_major,   "duplicate font encoding value %d in line %d of %s\n%s" },
{ ec_major,   "[backup] must follow a note" },
{ ec_warning, "non-movement options on rehearsal marks are ignored" },
{ ec_major,   "'&' at end of line while reading macro or repetition argument" },
/* 180-184 */
{ ec_failed,  "repetition count is too large (max %d)" },
{ ec_failed,  "-%s and -pdf are mutually exclusive" },
{ ec_warning, "-%s is ignored with -pdf" },
{ ec_failed,  "internal error: invalid output layout for PDF" },
{ ec_warning, "could not find font \"%s\" (use -F option?)" },
/* 185-189 */
{ ec_failed,  "PMW in PDF mode does not support %s fonts (%s)" },
{ ec_failed,  "File %s is not a recognized font file" },
{ ec_failed,  "Type 3 font (%s) is not supported in PDF mode" },
{ ec_failed,  "the \"%s\" directive is incompatible with a previous %s\n"
              "   setting (either a directive or a command line option)" },
{ ec_major,   "movement %d does not exist: no %s data written" },
/* 190-194 */
{ ec_major,   "misplaced [tremolo]: %s" },
{ ec_major,   "XML output internal error: %s" }
};

#define ERROR_MAXERROR (int)(sizeof(error_data)/sizeof(error_struct))



/*************************************************
*             Error message generator            *
*************************************************/

/* These functions output an error or warning message, and may abandon the
process if the error is sufficiently serious, or if there have been too many
less serious errors. If there are too many warnings, subsequent ones are
suppressed. For convenience, there are two external functions (below), to save
typing for hard errors and those that do not have a skip character. Each
external function calls error_basic() to do the actual work.

Arguments:
  n         the error number
  skip      input character(s) to skip to, or zero for no skip
  ap        va_list arguments to fill into message

Returns:    FALSE, because that's sometimes useful when continuing,
              but really serious errors do not return
*/

static BOOL
error_basic(enum error_number n, uint32_t skip, va_list ap)
{
int ec;

if (main_showid)
  {
  (void)fprintf(stderr, "PMW version %s\n", PMW_VERSION);
  main_showid = FALSE;
  }

if (error_inoption != NULL)
  {
  (void)fprintf(stderr, "** While decoding %s command line option:\n",
    error_inoption);
  error_inoption = NULL;
  }

if (n > ERROR_MAXERROR)
  {
  (void)fprintf(stderr, "** Error: unknown error number %d", n);
  ec = ec_failed;
  }
else
  {
  ec = error_data[n].ec;
  if (ec == ec_warning)
    {
    if (suppress_warnings) return FALSE;
    (void)fprintf(stderr, "** Warning: ");
    }
  else
    {
    (void)fprintf(stderr, "** Error: ");
    }
  (void)vfprintf(stderr, error_data[n].text, ap);
  }

(void)fprintf(stderr, "\n");
va_end(ap);

/* Additional information when in the reading phase. */

if (main_state == STATE_READ)
  {
  uschar *buffer;
  size_t in;

  (void)fprintf(stderr, "   Detected near line %d of %s\n", read_linenumber,
    read_filename);

  /* If we are expanding macros, show the raw input buffer */

  if (macro_expanding)
    {
    buffer = main_readbuffer_raw;
    in = macro_in;
    }

  /* Otherwise, show the input line and, if we are at the start of it, the
  previous line. */

  else
    {
    if (read_linenumber > 1 && (read_i == 0 || main_readbuffer[0] == '\n'))
        (void)fprintf(stderr, "%s", main_readbuffer_previous);
    buffer = main_readbuffer;
    in = read_i;
    }

  (void)fprintf(stderr, "%s", buffer);

  /* Unless we are at the end of the file, show where in the line we are. Then,
  for certain input errors we skip along the input to one or two designated
  characters. In all cases, stop at the end of the line. The skip setting
  should never happen while expanding macros, but double-check just in case. */

  if (read_filehandle != NULL && in != 0)
    {
    for (usint i = 0; i < in - 1; i++) (void)fprintf(stderr, "-");
    (void)fprintf(stderr, ">\n");

    if (!macro_expanding && skip != 0)
      {
      if (skip == '\n')   /* Can optimize newline (alone) case */
        {
        read_c = '\n';
        read_i = main_readlength;
        }
      else
        {
        int32_t skip1 = skip & 0xff;
        int32_t skip2 = skip >> 8;
        while (read_c != ENDFILE && read_c != skip1 &&
               read_c != skip2 && read_c != '\n')
           read_nextc();
        }
      }
    }
  }

/* Additional information when outputting */

else if ((main_state == STATE_PAGINATE || main_state == STATE_WRITE) &&
         (curbarnumber >= 0 || movement_count > 1))
  {
  (void)fprintf(stderr, "   Detected in");
  if (curbarnumber >= 0)
    {
    (void)fprintf(stderr, " bar %s",
      sfb(curmovt->barvector[curbarnumber]));
    if (curstave >= 0) (void)fprintf(stderr, " stave %d", curstave);
    }
  if (movement_count > 1)
    (void)fprintf(stderr, " movement %d", curmovt->number);
  (void)fprintf(stderr, "\n");
  }

/* Major errors in input allow more input to be read, but suppress the
generation of any output. This also applies to major errors detected during
pagination. */

if (ec > ec_minor) main_suppress_output = TRUE;

if (ec == ec_warning)
  {
  warning_count++;
  if (warning_count > 40)
    {
    (void)fprintf(stderr, "** Too many warnings - subsequent ones suppressed\n");
    suppress_warnings = TRUE;
    }
  }

else if (ec < ec_failed)  /* Major or minor error */
  {
  error_count++;
  if (error_count > error_maximum)
    {
    (void)fprintf(stderr, "** Too many errors\n");
    ec = ec_failed;
    }
  }

if (ec >= ec_failed)
  {
  (void)fprintf(stderr, "** PMW processing abandoned\n");
  exit(EXIT_FAILURE);
  }

(void)fprintf(stderr, "\n");          /* blank before next output */
return FALSE;
}


/*************************************************
*        Non-skipping external interface         *
*************************************************/

/*
Arguments:
  n           error number
  ...         arguments to fill into message

Returns:      FALSE, because that's useful when continuing,
                but really serious errors do not return
*/

BOOL
error(enum error_number n, ...)
{
va_list ap;
va_start(ap, n);
return error_basic(n, 0, ap);
}


/*************************************************
*        Skipping error external interface       *
*************************************************/

/*
Arguments:
  n           error number
  skip        character to skip to, or zero for no skip
  ...         arguments to fill into message

Returns:      FALSE, because that's useful when continuing,
                but really serious errors do not return
*/

BOOL
error_skip(enum error_number n, uint32_t skip, ...)
{
va_list ap;
va_start(ap, skip);
return error_basic(n, skip, ap);
}

/* End of error.c */
