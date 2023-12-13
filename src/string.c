/*************************************************
*         PMW string handling functions          *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: January 2021 */
/* This file last modified: December 2023 */

#include "pmw.h"

#define FSIM 8     /* Number of simultaneous fixed-format buffers */
#define PSIM 2     /* Number of simultaneous PMW string buffers */
#define BSIZ 64    /* Size of each buffer (bytes) */

static char        fbuffer[FSIM*BSIZ];
static usint       findex = 0;
static BOOL        fontwarned = FALSE;

static uint32_t    pbuffer[PSIM*BSIZ];  /* For converted PMW strings */
static usint       pindex = 0;

static const char *keysuffix[] = { "", "#", "$", "m", "#m", "$m" };

static const char *nlsharp[] = {
  "  C", "#-C", " #C", "$-D", "  D", "#-D",
  " #D", "$-E", "  E", "#-E", "  F", "#-F",
  " #F", "$-G", "  G", "#-G", " #G", "$-A",
  "  A", "#-A", " #A", "$-B", "  B", "#-B" };

static const char *nlflat[] = {
  "  C", "#-C", " $D", "$-D", "  D", "#-D",
  " $E", "$-E", "  E", "$-F", "  F", "#-F",
  " $G", "$-G", "  G", "#-G", " $A", "$-A",
  "  A", "#-A", " $B", "$-B", "  B", "$-C" };

static uschar *octavestring[] = {
  US"```", US"``", US"`", US"", US"'", US"''", US"'''", US"''''" };

static int32_t notelengths[] = {
  len_semibreve, len_minim, len_crotchet, len_quaver, len_squaver,
  len_dsquaver, len_hdsquaver };

static const char *notenames[] = {
  "semibreve", "minim", "crotchet", "quaver", "semiquaver",
  "demisemiquaver", "hemidemisemiquaver", "n128", "n256" };

static uschar notefactor[] = { 2, 3, 5, 7, 11 };

static uschar *music_escapes = US"bsmcQq#$%><udlr";
static uschar music_escape_values[] = {
  49, 50, 51, 53, 55, 57, 37, 39, 40, 122, 121, 126, 124, 123, 125 };

typedef struct escapedchars {
  const char *s;
  uint32_t len;
  uint32_t c;
} escapedchars;

static escapedchars esclist[] = {
  { "fi",   2, 0xfb01u },
  { "fl",   2, 0xfb02u },
  { "ss",   2, 0x00dfu },
  { "---",  3, 0x2014u },
  { "--",   2, 0x2013u },
  { "<<",   2, 0x201Cu },
  { ">>",   2, 0x201Du },
  { "?",    1, 0x00bfu },       /* Spanish question mark */
  { "\\",   1, '\\'    },       /* Backslash */
  { "p\\",  2, ss_page },       /* These special values are greater than */
  { "po\\", 3, ss_pageodd },    /* any valid Unicode code point. */
  { "pe\\", 3, ss_pageeven },
  { "so\\", 3, ss_skipodd },
  { "se\\", 3, ss_skipeven },
  { "r\\",  2, ss_repeatnumber },
  { "r2\\", 3, ss_repeatnumber2 },
  { "\'",   1, ss_asciiquote },    /* ASCII quote */
  { "`",    1, ss_asciigrave },    /* ASCII grave */
  { "-",    1, ss_escapedhyphen }, /* Escaped hyphen (for underlay) */
  { "=",    1, ss_escapedequals }, /* Escaped equals (ditto) */
  { "#",    1, ss_escapedsharp }   /* Escaped sharp (ditto) */
};

#define esclist_size (sizeof(esclist)/sizeof(escapedchars))

typedef struct escaccstr {
  uint32_t escape;
  uint32_t unicode;
  }
escaccstr;

/* Table of 2-character escape sequences. This table must be in order because
it is searched by binary chop.

Some available accented characters are omitted until I think of a suitable
escape for them. They are characters with these accents:

  dotaccent     - dot has been used for dieresis since the start of PMW
  commaaccent   - comma has been used for cedilla ditto
  ogonek
*/

static escaccstr escacc[] = {
  { ('A' << 8) + '\'',   0x00c1 },  /* Aacute */
  { ('A' << 8) + '-',    0x0100 },  /* Amacron */
  { ('A' << 8) + '.',    0x00c4 },  /* Adieresis */
  { ('A' << 8) + '^',    0x00c2 },  /* Acircumflex */
  { ('A' << 8) + '`',    0x00c0 },  /* Agrave */
  { ('A' << 8) + 'o',    0x00c5 },  /* Aring */
  { ('A' << 8) + 'u',    0x0102 },  /* Abreve */
  { ('A' << 8) + '~',    0x00c3 },  /* Atilde */

  { ('C' << 8) + '\'',   0x0106 },  /* Cacute */
  { ('C' << 8) + ')',    0x00a9 },  /* Copyright */
  { ('C' << 8) + ',',    0x00c7 },  /* Ccedilla */
  { ('C' << 8) + '^',    0x0108 },  /* Ccircumflex */
  { ('C' << 8) + 'v',    0x010c },  /* Ccaron */

  { ('D' << 8) + '-',    0x0110 },  /* Dcroat */
  { ('D' << 8) + 'v',    0x010e },  /* Dcaron */

  { ('E' << 8) + '\'',   0x00c9 },  /* Eacute */
  { ('E' << 8) + '-',    0x0112 },  /* Emacron */
  { ('E' << 8) + '.',    0x00cb },  /* Edieresis */
  { ('E' << 8) + '^',    0x00ca },  /* Ecircumflex */
  { ('E' << 8) + '`',    0x00c8 },  /* Egrave */
  { ('E' << 8) + 'u',    0x0114 },  /* Ebreve */
  { ('E' << 8) + 'v',    0x011a },  /* Ecaron */

  { ('G' << 8) + '^',    0x011c },  /* Gcircumflex */
  { ('G' << 8) + 'u',    0x011e },  /* Gbreve */

  { ('H' << 8) + '^',    0x0124 },  /* Hcircumflex */

  { ('I' << 8) + '\'',   0x00cd },  /* Iacute */
  { ('I' << 8) + '-',    0x012a },  /* Imacron */
  { ('I' << 8) + '.',    0x00cf },  /* Idieresis */
  { ('I' << 8) + '^',    0x00ce },  /* Icircumflex */
  { ('I' << 8) + '`',    0x00cc },  /* Igrave */
  { ('I' << 8) + 'u',    0x012C },  /* Ibreve */
  { ('I' << 8) + '~',    0x0128 },  /* Itilde */

  { ('J' << 8) + '^',    0x0134 },  /* Jcircumflex */

  { ('L' << 8) + '\'',   0x0139 },  /* Lacute */
  { ('L' << 8) + '/',    0x0141 },  /* Lslash */
  { ('L' << 8) + 'v',    0x013d },  /* Lcaron */

  { ('N' << 8) + '\'',   0x0143 },  /* Nacute */
  { ('N' << 8) + 'v',    0x0147 },  /* Ncaron */
  { ('N' << 8) + '~',    0x00d1 },  /* Ntilde */

  { ('O' << 8) + '\"',   0x0150 },  /* Odoubleacute */
  { ('O' << 8) + '\'',   0x00d3 },  /* Oacute */
  { ('O' << 8) + '-',    0x014c },  /* Omacron */
  { ('O' << 8) + '.',    0x00d6 },  /* Odieresis */
  { ('O' << 8) + '/',    0x00d8 },  /* Oslash */
  { ('O' << 8) + '^',    0x00d4 },  /* Ocircumflex */
  { ('O' << 8) + '`',    0x00d2 },  /* Ograve */
  { ('O' << 8) + 'u',    0x014e },  /* Obreve */
  { ('O' << 8) + '~',    0x00d5 },  /* Otilde */

  { ('R' << 8) + '\'',   0x0154 },  /* Racute */
  { ('R' << 8) + 'v',    0x0158 },  /* Rcaron */

  { ('S' << 8) + '\'',   0x015a },  /* Sacute */
  { ('S' << 8) + ',',    0x015e },  /* Scedilla */
  { ('S' << 8) + '^',    0x015c },  /* Scircumflex */
  { ('S' << 8) + 'v',    0x0160 },  /* Scaron */

  { ('T' << 8) + ',',    0x0162 },  /* Tcedilla */
  { ('T' << 8) + 'v',    0x0164 },  /* Tcaron */

  { ('U' << 8) + '\"',   0x0170 },  /* Udoubleacute */
  { ('U' << 8) + '\'',   0x00da },  /* Uacute */
  { ('U' << 8) + '-',    0x016a },  /* Umacron */
  { ('U' << 8) + '.',    0x00dc },  /* Udieresis */
  { ('U' << 8) + '^',    0x00db },  /* Ucircumflex */
  { ('U' << 8) + '`',    0x00d9 },  /* Ugrave */
  { ('U' << 8) + 'o',    0x016e },  /* Uring */
  { ('U' << 8) + 'u',    0x016c },  /* Ubreve */
  { ('U' << 8) + '~',    0x0168 },  /* Utilde */

  { ('W' << 8) + '^',    0x0174 },  /* Wcircumflex */

  { ('Y' << 8) + '\'',   0x00dd },  /* Yacute */
  { ('Y' << 8) + '.',    0x0178 },  /* Ydieresis */
  { ('Y' << 8) + '^',    0x0176 },  /* Ycircumflex */

  { ('Z' << 8) + '\'',   0x0179 },  /* Zacute */
  { ('Z' << 8) + 'v',    0x017d },  /* Zcaron */

  { ('a' << 8) + '\'',   0x00e1 },  /* aacute */
  { ('a' << 8) + '-',    0x0101 },  /* amacron */
  { ('a' << 8) + '.',    0x00e4 },  /* adieresis */
  { ('a' << 8) + '^',    0x00e2 },  /* acircumflex */
  { ('a' << 8) + '`',    0x00e0 },  /* agrave */
  { ('a' << 8) + 'o',    0x00e5 },  /* aring */
  { ('a' << 8) + 'u',    0x0103 },  /* abreve */
  { ('a' << 8) + '~',    0x00e3 },  /* atilde */

  { ('c' << 8) + '\'',   0x0107 },  /* cacute */
  { ('c' << 8) + ')',    0x00a9 },  /* copyright */
  { ('c' << 8) + ',',    0x00e7 },  /* ccedilla */
  { ('c' << 8) + '^',    0x0109 },  /* ccircumflex */
  { ('c' << 8) + 'v',    0x010d },  /* ccaron */

  { ('d' << 8) + '-',    0x0111 },  /* dcroat */
  { ('d' << 8) + 'v',    0x010f },  /* dcaron */

  { ('e' << 8) + '\'',   0x00e9 },  /* eacute */
  { ('e' << 8) + '-',    0x0113 },  /* emacron */
  { ('e' << 8) + '.',    0x00eb },  /* edieresis */
  { ('e' << 8) + '^',    0x00ea },  /* ecircumflex */
  { ('e' << 8) + '`',    0x00e8 },  /* egrave */
  { ('e' << 8) + 'u',    0x0115 },  /* ebreve */
  { ('e' << 8) + 'v',    0x011b },  /* ecaron */

  { ('g' << 8) + '^',    0x011d },  /* gcircumflex */
  { ('g' << 8) + 'u',    0x011f },  /* gbreve */

  { ('h' << 8) + '^',    0x0125 },  /* hcircumflex */

  { ('i' << 8) + '\'',   0x00ed },  /* iacute */
  { ('i' << 8) + '-',    0x012b },  /* imacron */
  { ('i' << 8) + '.',    0x00ef },  /* idieresis */
  { ('i' << 8) + '^',    0x00ee },  /* icircumflex */
  { ('i' << 8) + '`',    0x00ec },  /* igrave */
  { ('i' << 8) + 'u',    0x012d },  /* ibreve */
  { ('i' << 8) + '~',    0x0129 },  /* itilde */

  { ('j' << 8) + '^',    0x0135 },  /* jcircumflex */

  { ('l' << 8) + '\'',   0x013a },  /* Lacute */
  { ('l' << 8) + '/',    0x0142 },  /* Lslash */
  { ('l' << 8) + 'v',    0x013e },  /* Lcaron */

  { ('n' << 8) + '\'',   0x0144 },  /* nacute */
  { ('n' << 8) + 'v',    0x0148 },  /* ncaron */
  { ('n' << 8) + '~',    0x00f1 },  /* ntilde */

  { ('o' << 8) + '\"',   0x0151 },  /* odoubleacute */
  { ('o' << 8) + '\'',   0x00f3 },  /* oacute */
  { ('o' << 8) + '-',    0x014d },  /* omacron */
  { ('o' << 8) + '.',    0x00f6 },  /* odieresis */
  { ('o' << 8) + '/',    0x00f8 },  /* oslash */
  { ('o' << 8) + '^',    0x00f4 },  /* ocircumflex */
  { ('o' << 8) + '`',    0x00f2 },  /* ograve */
  { ('o' << 8) + 'u',    0x014f },  /* obreve */
  { ('o' << 8) + '~',    0x00f5 },  /* otilde */

  { ('r' << 8) + '\'',   0x0155 },  /* racute */
  { ('r' << 8) + 'v',    0x0159 },  /* rcaron */

  { ('s' << 8) + '\'',   0x015b },  /* sacute */
  { ('s' << 8) + ',',    0x015f },  /* scedilla */
  { ('s' << 8) + '^',    0x015d },  /* scircumflex */
  { ('s' << 8) + 'v',    0x0161 },  /* scaron */

  { ('t' << 8) + ',',    0x0163 },  /* tcedilla */
  { ('t' << 8) + 'v',    0x0165 },  /* tcaron */

  { ('u' << 8) + '\"',   0x0171 },  /* udoubleacute */
  { ('u' << 8) + '\'',   0x00fa },  /* uacute */
  { ('u' << 8) + '-',    0x016b },  /* umacron */
  { ('u' << 8) + '.',    0x00fc },  /* udieresis */
  { ('u' << 8) + '^',    0x00fb },  /* ucircumflex */
  { ('u' << 8) + '`',    0x00f9 },  /* ugrave */
  { ('u' << 8) + 'o',    0x016f },  /* uring */
  { ('u' << 8) + 'u',    0x016d },  /* ubreve */
  { ('u' << 8) + '~',    0x0169 },  /* utilde */

  { ('w' << 8) + '^',    0x0175 },  /* wcircumflex */

  { ('y' << 8) + '\'',   0x00fd },  /* yacute */
  { ('y' << 8) + '.',    0x00ff },  /* ydieresis */
  { ('y' << 8) + '^',    0x0177 },  /* ycircumflex */

  { ('z' << 8) + '\'',   0x017a },  /* zacute */
  { ('z' << 8) + 'v',    0x017e },  /* zcaron */
};

static int escacccount = sizeof(escacc)/sizeof(escaccstr);



/*************************************************
*     Convert an 8-bit string to a PMW string    *
*************************************************/

/* The result is returned in a static buffer. This function is used for
converting short internal strings such as the number for a plet or a musical
font string.

Arguments:
  s          the 8-bit string
  f          the font

Returns:     pointer to the PMW string
*/

uint32_t *
string_pmw(uschar *s, int f)
{
uint32_t ff = f << 24;
uint32_t *ss = pbuffer + pindex;
uint32_t *yield = ss;
pindex += BSIZ;
if (pindex >= sizeof(pbuffer)/sizeof(uint32_t)) pindex = 0;
while (*s != 0) *ss++ = *s++ | ff;
*ss = 0;
return yield;
}


/*************************************************
*  Format a fixed point number to a given buffer *
*************************************************/

/* This is used by the two functions that follow. A trailing zero is added, but
not counted.

Arguments:
  n         a fixed point number
  s         where to put the answer

Yield:      the number of characters
*/

static int
format_fixed(int32_t n, char *s)
{
int f;
int p = 0;

if (n < 0) s[p++] = '-';
n = abs(n);
f = n % 1000;
p += sprintf(s + p, "%d", n/1000);

if (f != 0)
  {
  const char *z = (f > 99)? "" : (f > 9)? "0" : "00";
  p += sprintf(s + p, ".%s%d", z, f);
  while (s[p-1] == '0') s[--p] = 0;
  }

return p;
}



/*************************************************
*           Format a fixed point number          *
*************************************************/

/* Each time called we use a fresh section of the buffer so that up to FSIM
results can be simultaneously available. */

char *
string_format_fixed(int32_t n)
{
char *s = fbuffer + findex;
findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;
(void)format_fixed(n, s);
return s;
}



/*************************************************
*       Format multiple fixed point numbers      *
*************************************************/

/* This handles format strings that contain %f for a PMW fixed-point number.
The only other escape recognized is %%. The yield is a pointer to the result
buffer. */

char *
string_format_multiple_fixed(const char *format, ...)
{
char *buff = fbuffer + findex;
char *s = buff;

findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;

va_list ap;
va_start(ap, format);

while (*format != 0)
  {
  if (*format != '%' || *(++format) == '%')
    {
    *s++ = *format++;
    continue;
    }
  if (*format++ != 'f') error (ERR139);   /* Hard internal error */
  s += format_fixed(va_arg(ap, int), s);
  }

va_end(ap);
*s = 0;
return buff;
}




/*************************************************
*                Format a pitch                  *
*************************************************/

/* This is called when output information about a piece. The usesharps argument
is a hint, taken from a key signature.

Arguments:
  pitch      the pitch to format
  usesharps  use sharps for accidentals

Returns:     pointer to formatted pitch
*/

char *
string_format_pitch(uint16_t pitch, BOOL usesharps)
{
char *s = fbuffer + findex;

findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;

if (pitch != 0)
  {
  int c = sprintf(s, "%s%s", (usesharps? nlsharp : nlflat)[pitch % OCTAVE],
    octavestring[pitch/OCTAVE]);
  while (c++ < 5) strcat(s, " ");
  }
else strcpy(s, "unset");

return s;
}




/*************************************************
*                Format a key                    *
*************************************************/

char *
string_format_key(uint32_t k)
{
char *s = fbuffer + findex;

findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;

if (k == key_N) sprintf(s, "N");
else if (k >= key_X)
  {
  sprintf(s, "X%d", k - key_X + 1);
  }
else
  {
  sprintf(s, "%c%s", (k % 7) + 'A', keysuffix[k/7]);
  }

return s;
}



/*************************************************
*            Format a note length                *
*************************************************/

/* This function is used to prepare text for error messages. It turns a note
length into text, e.g. "4 crotchets". Use the same buffers as for fixed point
numbers.

Argument: the note length
Returns:  pointer to a buffer
*/

char *
string_format_notelength(int32_t length)
{
size_t plen;
uint32_t d;
const char *name = NULL;
char *s = fbuffer + findex;

findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;

/* Search for a whole number of a particular note type. If so, it's a simple
print. */

for (size_t i = 0; i < sizeof(notelengths)/sizeof(int32_t); i++)
  {
  if (length % notelengths[i] == 0)
    {
    int number = length/notelengths[i];
    (void)sprintf(s, "%d %s%s", number, notenames[i], (number == 1)?"":"s");
    return s;
    }
  }

/* Otherwise, compute the fraction of a crotchet */

plen = 0;
d = len_crotchet;
name = "of a crotchet";

if (length > len_crotchet)
  {
  plen += sprintf(s, "%d + ", length/len_crotchet);
  length = length % len_crotchet;
  name = "crotchets";
  }

for (size_t i = 0; i < sizeof(notefactor); i++)
  {
  int x = notefactor[i];
  while (d % x == 0 && length % x == 0) { d /= x; length /= x; }
  }

/* Get rid of any common factors (optimized Euclid algorithm) */

for (;;)
  {
  uint32_t hcf;
  uint32_t a = d;
  uint32_t b = length;

  for (;;)
    {
    if (a > b) a %= b; else b %= a;
    if (a == 0) { hcf = b; break; }
    if (b == 0) { hcf = a; break; }
    }

  if (hcf < 2) break;
  d /= hcf;
  length /= hcf;
  }

(void)sprintf(s + plen, "%d/%d %s", length, d, name);
return s;
}



/*************************************************
*               Format a bar number              *
*************************************************/

/* This is used for formatting a logical bar number for error and debugging
messages. Logical bar numbera have the main number in the top 16 bits and the
fraction in the bottom.

Argument:  a logical bar number
Returns:   pointer to a buffer
*/

char *
string_format_barnumber(uint32_t bn)
{
uint32_t bf = bn & 0xffffu;
char *s = fbuffer + findex;

findex += BSIZ;
if (findex >= sizeof(fbuffer)) findex = 0;

bn >>= 16;
if (bf == 0) (void)sprintf(s, "%d", bn);
  else (void)sprintf(s, "%d.%d", bn, bf);

return s;
}



/*************************************************
*           Extend plain string buffer           *
*************************************************/

void
string_extend_buffer(void)
{
if (read_stringbuffer_size >= STRINGBUFFER_SIZELIMIT)
  error(ERR5, "string", STRINGBUFFER_SIZELIMIT);        /* Hard */
read_stringbuffer_size += STRINGBUFFER_CHUNKSIZE;
read_stringbuffer = realloc(read_stringbuffer, read_stringbuffer_size);
if (read_stringbuffer == NULL)
  error(ERR0, "re-", "string buffer", read_stringbuffer_size);  /* Hard */
}



/*************************************************
*            Relativize a file name              *
*************************************************/

/* If the name does not start with '/' and main_filename is not NULL (which
indicates stdin), we make the name relative to the current input file name. */

void
string_relativize(void)
{
usint im, in;
TRACE("string_relativize(%s) entered\n", read_stringbuffer);

if (read_stringbuffer[0] == '/' || main_filename == NULL) return;
im = Ustrlen(read_filename);
while (im > 0 && read_filename[--im] != '/') {}
if (im != 0) im++;
in = Ustrlen(read_stringbuffer);
while (im + in + 1 > read_stringbuffer_size) string_extend_buffer();
memmove(read_stringbuffer + im, read_stringbuffer, in + 1);
memcpy(read_stringbuffer, read_filename, im);

TRACE("relativized to %s\n", read_stringbuffer);
}


/*************************************************
*              Read a plain string               *
*************************************************/

/* This function is used for reading file names and the like in heading
directives. These are in quotes, but are not to be interpreted as PMW strings.
They are stored in read_stringbuffer, which is expanded if necessary. The
caller takes a copy if the string has to be kept.

Returns:     TRUE if OK, FALSE if starting quote missing
*/

BOOL
string_read_plain(void)
{
usint i = 0;
read_sigc();
if (read_c != '\"') return FALSE;
read_nextc();
while (read_c != '\"' && read_c != '\n')
  {
  if (i >= read_stringbuffer_size) string_extend_buffer();
  read_stringbuffer[i++] = read_c;
  read_nextc();
  }
if (i >= read_stringbuffer_size) string_extend_buffer();
read_stringbuffer[i] = 0;
if (read_c == '\"') read_nextc();
  else error(ERR13, "terminating quote missing");
return TRUE;
}



/*************************************************
*       Compare two PMW strings for equality     *
*************************************************/

BOOL
string_pmweq(uint32_t *s, uint32_t *t)
{
while (*s != 0 && *t != 0)
  {
  if (*s++ != *t++) return FALSE;
  }
return (*s == 0) & (*t == 0);
}



/*************************************************
*              Get width of a PMW string         *
*************************************************/

/*
Arguments:
  s            the string
  fdata        points to font instance block
  heightptr    where to return height if not NULL

Returns:       the width (fixed point)
*/

int32_t
string_width(uint32_t *s, fontinststr *fdata, int32_t *heightptr)
{
BOOL ignoreskip = FALSE;
int32_t yield = 0;
int32_t yield_height = 0;
uint32_t lastc = 0;
uint32_t spacecount = 0;

DEBUG(D_stringwidth)
  {
  eprintf("width of ");
  debug_string(s);
  }

while (*s != 0)
  {
  uint32_t c = *s++;
  uint32_t f = PFONT(c);
  int size = fdata->size;

  /* Adjust the point size for small caps and the reduced music font. */

  if (f >= font_small)
    {
    f -= font_small;
    if (f == font_mf) size = (size * 9) / 10;
      else size = (size * curmovt->smallcapsize) / 1000;
    }

  /* Count spaces. An unescaped vertical bar is not special here, nor is an
  escaped hyphen, equals sign, or sharp. */

  c = PCHAR(c);
  switch(c)
    {
    case ' ': spacecount++; break;
    case ss_verticalbar: c = '|'; break;
    case ss_escapedhyphen: c = '-'; break;
    case ss_escapedequals: c = '='; break;
    case ss_escapedsharp:  c = '#'; break;
    default: break;
    }

  /* Handle an ordinary character */

  if (c <= MAX_UNICODE)
    {
    int32_t h;
    yield += font_charwidth(c, lastc, f, size, &h);
    yield_height += h;
    lastc = c;
    }

  /* Handle a special escape character. This function is sometimes called
  before pagination, when there is no current page set. In those cases the
  page-based escapes shouldn't be present, but just in case we assume a page
  number zero. Similarly, assume bar repeat number zero when none is available.
  */

  else
    {
    int32_t pn = 0;
    char temp[12] = "";

    if (c == ss_repeatnumber || c == ss_repeatnumber2)
      {
      if (curstave >= 0 && curbarnumber >= 0)
        {
        stavestr *sts = curmovt->stavetable[curstave];
        if (sts != NULL && sts->barindex != NULL)
          pn = sts->barindex[curbarnumber]->repeatnumber;
        if (pn == 1 && c == ss_repeatnumber2) continue;  /* Nothing inserted */
        }
      }

    else
      {
      BOOL odd, skipodd;

      if (curpage != NULL) pn = curpage->number;
      odd = (pn & 1) != 0;
      skipodd = FALSE;

      switch (c)
        {
        case ss_page:
        break;

        case ss_pageodd:
        if (!odd) continue;  /* Next character */
        break;

        case ss_pageeven:
        if (odd) continue;   /* Next character */
        break;

        case ss_skipodd:
        skipodd = TRUE;
        /* Fall through */
        case ss_skipeven:
        if (ignoreskip) ignoreskip = FALSE;
        else if (odd == skipodd) while (*s != 0 && PCHAR(*s) != c) s++;
        else ignoreskip = TRUE;
        continue;            /* Next character */

        default:
        error(ERR98, c);  /* Hard */
        break;
        }
      }

    /* Page or bar repeat number wanted */

    sprintf(temp, "%d", pn);
    for (char *p = temp; *p != 0; p++)
      {
      int32_t h;
      yield += font_charwidth(*p, lastc, f, size, &h);
      yield_height += h;
      lastc = *p;
      }
    }
  }

/* Add in any space stretching. */

yield += spacecount * fdata->spacestretch;

/* Adjust for any character transformation or rotation. Historical relic: the
transform matrix operates in the units that RISC OS uses for its font
transformations. These were chosen so that the RISC OS screen display code
could use the matrix directly. There seems no reason to change this choice
gratuitously. The four matrix values have 16-bit fractions. */

if (fdata->matrix != NULL)
  {
  uint32_t h = yield_height;
  uint32_t w = yield;

  yield_height =
    mac_muldiv(w, fdata->matrix[1], 65536) +
    mac_muldiv(h, fdata->matrix[3], 65536);

  yield =
    mac_muldiv(w, fdata->matrix[0], 65536) +
    mac_muldiv(h, fdata->matrix[2], 65536);
  }

DEBUG(D_stringwidth)
  {
  eprintf(" = %s", sff(yield));
  if (yield_height != 0) eprintf(" height = %s", sff(yield_height));
  eprintf(" at size %s\n", sff(fdata->size));
  }

if (heightptr != NULL) *heightptr = yield_height;
return yield;
}



/*************************************************
*        Check string for valid characters       *
*************************************************/

/* The main purpose of this function is to check a string for unknown or
invalid characters, replacing them with a legal character and saving the
failures in a list for later reporting. At the same time, we handle any special
Unicode translations for non-standardly encoded fonts.

When B2PF is supported, before doing the check, we must process the string with
B2PF if any of its fonts is set up for it. This may involve getting a new
memory block if the length of the string increases, but in most cases this
won't be necessary. When this happens, the old memory is just abandoned. The
intermediate buffer is currently on the stack, and of fixed size. If ever there
is a complaint, we could change to an expandable heap buffer.

B2PF is applied to substrings of characters in the same font if that font is
configured for it. When the string is underlay, overlay, or a stave name
string, however, we also terminate substrings at syllable or line breaking
points because, when reversing character order, we don't want to reverse the
order of the syllables or lines. Note that the bseps argument containly only
ASCII characters.

Arguments:
  str          the string to check
  bseps        B2PF special separators, or NULL if none
  keepnl       retain newlines, otherwise turn into spaces

Returns:       the checked string
*/

uint32_t *
string_check(uint32_t *str, const char *bseps, BOOL keepnl)
{
#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
BOOL call_b2pf = FALSE;

/* To save calling B2PF unnecessarily, do a quick scan to see if any fonts are
set up for B2Pf. */

for (uint32_t *s = str; *s != 0; s++)
  {
  if (font_b2pf_contexts[PFONT(*s) & ~font_small] != NULL)
    {
    call_b2pf = TRUE;
    break;
    }
  }

if (call_b2pf)
  {
  uint32_t *s = str;
  uint32_t outbuff[B2PF_OUTSTACKSIZE];
  uint32_t *p = outbuff;
  size_t room = B2PF_OUTSTACKSIZE - 1;   /* Leave space for zero */
  size_t nlen;

  while (*s != 0)
    {
    size_t ilen;
    uint32_t *t = s;
    uint32_t f = PFTOP(*s);
    uint32_t bf = (f >> 24) & ~font_small;   /* Basic font */
    BOOL isb2pf = font_b2pf_contexts[bf] != NULL;

    /* Get the next substring whose characters are all in the same font. When
    special separators are supplied, sequences of them are copied as is.
    Complicating things, there is a special case where the separator is an
    unescaped vertical bar (for [name] strings). This appears in strings as a
    non-Unicode special character. We have to handle this case on its own. */

    if (bseps != NULL)
      {
      if (Ustrcmp(bseps, "|") == 0)
        {
        if (PCHAR(*s) == ss_verticalbar)
          {
          while (PCHAR(*(++s)) == ss_verticalbar) continue;
          isb2pf = FALSE;
          }
        else
          {
          while (*(++s) != 0 && PFTOP(*s) == f &&
            PCHAR(*s) != ss_verticalbar) continue;
          }
        }

      /* Not the special [name] string case (i.e. it's underlay/overlay) */

      else
        {
        uint32_t c = PCHAR(*s);

        /* Sequence of special separators */

        if (c < 128 && Ustrchr(bseps, c) != NULL)
          {
          isb2pf = FALSE;
          while (*(++s) != 0)
            {
            c = PCHAR(*s);
            if (c >= 128 || Ustrchr(bseps, c) == NULL) break;
            }
          }

        /* Sequence of normal characters */

        else
          {
          while (*(++s) != 0 && PFTOP(*s) == f)
            {
            c = PCHAR(*s);
            if (c < 128 && Ustrchr(bseps, c) != NULL) break;
            }
          }
        }
      }

    /* No special separators are defined. */

    else while (*(++s) != 0 && PFTOP(*s) == f) continue;

    /* Length of substring */

    ilen = s - t;

    /* If this font is not set up for B2PF, or we have a sequence of hyphens
    and equals in an underlay string, just copy the text. */

    if (!isb2pf)
      {
      if (ilen > room) error(ERR156);  /* Hard */
      memcpy(p, t, ilen*sizeof(uint32_t));
      p += ilen;
      room -= ilen;
      }

    /* This font has B2PF */

    else
      {
      int rc;
      size_t used, eoffset;

      /* Remove the font from the 32-bit characters before processing. Check
      whether any special character values are present. These have code points
      outside the Unicode range, which means that B2PF will complain that they
      are invalid. To avoid this, copy them to matching code points in the
      Unicode "private" range (which are hopefully not otherwise in use). */

      for (uint32_t *pp = t; pp < s; pp++)
        {
        *pp &= 0x00ffffffu;
        if (*pp > MAX_UNICODE) *pp = (*pp - MAX_UNICODE + UNICODE_PRIVATE);
        }

      rc = b2pf_format_string(font_b2pf_contexts[bf], (void *)t, ilen,
        (void *)p, room, &used, font_b2pf_options[bf], &eoffset);

      if (rc != B2PF_SUCCESS)
        {
        size_t buffused;
        char buffer[128];
        (void)b2pf_get_error_message(rc, buffer, sizeof(buffer), &buffused, 0);
        buffer[buffused] = 0;

        error(ERR157, buffer);  /* Not hard so we can show the string */
        for (uint32_t *pp = t; pp < s; pp++) *pp |= f;
        *s = 0;
        fprintf(stderr, "** While processing ");
        debug_string(t);
        fprintf(stderr, "\n** pmw processing abandoned\n");
        exit(EXIT_FAILURE);
        }

      /* Restore special character values and add the font back into the
      processed characters while advancing past them. */

      room -= used;
      while (used > 0)
        {
        if (*p > UNICODE_PRIVATE &&
            *p < UNICODE_PRIVATE + (ss_top - MAX_UNICODE))
          *p = (*p - UNICODE_PRIVATE) + MAX_UNICODE;
        *p++ |= f;
        used--;
        }
      }
    }

  *p++ = 0;

  /* Get new buffer if necessary and copy the processed string. */

  nlen = p - outbuff;
  if (nlen > (size_t)(s - str)) str = mem_get_independent(nlen*sizeof(uint32_t));
  memcpy(str, outbuff, nlen*sizeof(uint32_t));
  }

#else          /* No B2PF support */
(void)bseps;   /* Avoid compiler warning */
#endif         /* SUPPORT_B2PF */

/* Now check the string's characters and do any necessary Unicode translation. */

for (uint32_t *s = str; *s != 0; s++)
  {
  fontstr *fs;
  int f;
  uint32_t c = PCHAR(*s);

  /* Handle escaped ASCII quotes. We can't handle them as the string is read,
  because if we do, they get turned back into typographic quotes below for
  standardly-encoded strings. We can't do that until now because at reading
  time we may not know the font's encoding. */

  if (c == ss_asciiquote) { *s = PFTOP(*s) | '\''; continue; }
  if (c == ss_asciigrave) { *s = PFTOP(*s) | '`'; continue; }

  /* Other special encodings do not need checking. */

  if (c > MAX_UNICODE) continue;

  /* Handle a genuine character. */

  f = PFONT(*s) & ~font_small;
  fs = &(font_list[font_table[f]]);

  /* For non-standardly encoded fonts, either the character or its translation
  must be valid. */

  if ((fs->flags & ff_stdencoding) == 0)
    {
    uint32_t tc = font_utranslate(c, fs);

    if (tc == 0xffffffffu) tc = c;   /* No translation found */
    *s &= 0xff000000u;               /* Keep the current font */

    /* A valid, possibly translated, codepoint is either less than 512 or an
    escaped sharp, equals, or hyphen, whose code points are outside the Unicode
    range. */

    if (tc < 512 || tc > MAX_UNICODE) *s |= tc;

    /* Remember invalid code points, to be listed at the end of reading the
    input, and replace with whatever is set for the font, retaining the font
    number if the replacement's font is unset. */

    else
      {
      *s = (PFTOP(fs->invalid) == (font_unknown << 24))?
        (*s | PCHAR(fs->invalid)) : fs->invalid;
      if (read_uinvnext <= UUSIZE)
        {
        int i;
        for (i = 0; i < read_uinvnext; i++)
          if (read_invalid_unicode[i] == c) break;
        if (i >= read_uinvnext)
          {
          if (read_uinvnext < UUSIZE) read_invalid_unicode[read_uinvnext++] = c;
            else read_uinvoverflow = TRUE;
          }
        }
      }
    }

  /* For standardly encoded fonts, we first check for quotes, newline, and fi
  substitutions. */

  else
    {
    uint32_t tc;

    if (c == '`' || c == '\'' || (c == '\n' && !keepnl) ||
         (c == 'f' && PCHAR(s[1]) == 'i'))
      {
      switch (c)
        {
        case '`': c = QUOTE_LEFT; break;
        case '\'': c = QUOTE_RIGHT; break;
        case '\n': c = ' '; break;
        default:
        c = CHAR_FI;
        for (uint32_t *t = s + 1; *t != 0; t++) *t = t[1];
        break;
        }
      *s = PFTOP(*s) | c;
      }

    /* Check for a Unicode translation. If one is found, it will either be less
    than 512 or an escaped sharp, equals, or hyphen with a high code point.
    This is checked when the .utr file is read so no need to check here. */

    tc = font_utranslate(c, fs);
    if (tc != 0xffffffffu)   /* A translation was found */
      {
      *s = PFTOP(*s) | tc;
      }

    /* Handle an untranslated character in a standardly encoded font. Code
    points >= 256 and < LOWCHARLIMIT are encoded in the second of the two
    PostScript fonts, using the Unicode encoding less 256, so no more needs to
    be done. The remaining code points have to be converted to some of the
    remaining characters in the second PostScript font, which are encoded
    arbitrarily, i.e. not using the Unicode encoding (some of their Unicode
    values are quite large). To find this encoding, we search for the character
    in the tree for the font . */

    else if (c >= LOWCHARLIMIT)
      {
      tree_node *t;
      uschar utf[8];

      utf[misc_ord2utf8(c, utf)] = 0;
      t = tree_search(fs->high_tree, utf);
      *s &= 0xff000000u;   /* Keep the current font */

      /* If the character is unknown, leave it alone if less than 512.
      Otherwise replace it with whatever is set for the font, retaining the
      font number if the replacement's font is unset. */

      if (t != NULL) *s |= LOWCHARLIMIT + t->value;
      else if (c < 512) *s |= c;
      else
        {
        *s = (PFTOP(fs->invalid) == (font_unknown << 24))?
          (*s | PCHAR(fs->invalid)) : fs->invalid;
        if (read_uunext <= UUSIZE)
          {
          int i;
          for (i = 0; i < read_uunext; i++)
            if (read_unsupported_unicode[i] == c) break;
          if (i >= read_uunext)
            {
            if (read_uunext < UUSIZE) read_unsupported_unicode[read_uunext++] = c;
              else read_uuoverflow = TRUE;
            }
          }
        }
      }
    }
  }

return str;
}



/*************************************************
*          Check for a UTF-8 character           *
*************************************************/

/* Given a pointer to a byte in a zero-terminated byte string, check to see if
it is the start of a UTF-8 character, and if so, return the length. Note that
this checks for UTF-8 representations of values greater than the maximum
Unicode code point.

Argument:  pointer to the first byte
Returns:   the length of the character (1 - 6) or -1 if invalid UTF-8 start
*/

int
string_check_utf8(uschar *pp)
{
int ab;
int c = *pp++;
int n;

if (c < 0x80) return 1;
if (c < 0xc0) return -1;

n = ab = utf8_table4[c & 0x3f];  /* Number of additional bytes */

/* Check top bits in the second byte */
if ((*pp & 0xc0) != 0x80) return -1;

/* Check for overlong sequences for each different length */
switch (ab)
  {
  /* Check for xx00 000x */
  case 1:
  if ((c & 0x3e) == 0) return -1;
  return 2;   /* We know there aren't any more bytes to check */

  /* Check for 1110 0000, xx0x xxxx */
  case 2:
  if (c == 0xe0 && (*pp & 0x20) == 0) return -1;
  break;

  /* Check for 1111 0000, xx00 xxxx */
  case 3:
  if (c == 0xf0 && (*pp & 0x30) == 0) return -1;
  break;

  /* Check for 1111 1000, xx00 0xxx */
  case 4:
  if (c == 0xf8 && (*pp & 0x38) == 0) return -1;
  break;

  /* Check for leading 0xfe or 0xff, and then for 1111 1100, xx00 00xx */
  case 5:
  if (c == 0xfe || c == 0xff ||
     (c == 0xfc && (*pp & 0x3c) == 0)) return -1;
  break;
  }

/* Check for valid bytes after the 2nd, if any; all must have the top bit set */

while (--ab > 0)
  {
  if ((*(++pp) & 0xc0) != 0x80) return -1;
  }

return n + 1;
}



/*************************************************
*          Expand a string-reading buffer        *
*************************************************/

/* The first time this function is called from string_read(), *block == NULL,
which means we have just filled up the on-stack buffer (apart from one element
at the end which was left for the terminator). Start a malloc() buffer at twice
the size and copy the on-stack data into it. Otherwise, increase the malloc()
buffer. At the start of the memory block we leave space for a pointer so that
it can be passed to mem_register() and thus rememvered for freeing.

Arguments:
  block      where to put the pointer to a new memory block
  yield      pointer to pointer to the usable area, updated
  mem_size   pointer to size of memory block. updated
  string_max pointer to maximum string size, updated

Returns:     nothing
*/

static void
expand_string_buffer(void **block, uint32_t **yield, size_t *mem_size,
  size_t *string_max)
{
if (*block == NULL)
  {
  uint32_t *oldyield = *yield;
  *mem_size = 2 * STRINGBUFFER_CHUNKSIZE * sizeof(uint32_t) + sizeof(char *);
  *block = malloc(*mem_size);
  if (*block == NULL) error(ERR0, "", "PMW string", *mem_size);  /* Hard */
  *yield = (uint32_t *)((char *)(*block) + sizeof(char *));
  memcpy(*yield, oldyield, (STRINGBUFFER_CHUNKSIZE - 1) * sizeof(uint32_t));
  }

else
  {
  if (*mem_size >= STRINGBUFFER_SIZELIMIT)
    error(ERR5, "PMW string", STRINGBUFFER_SIZELIMIT);        /* Hard */
  *mem_size += STRINGBUFFER_CHUNKSIZE * sizeof(uint32_t);
  *block = realloc(*block, *mem_size);
  if (*block == NULL) error(ERR0, "re-", "PMW string", *mem_size);  /* Hard */
  *yield = (uint32_t *)((char *)(*block) + sizeof(char *));
  }

*string_max += STRINGBUFFER_CHUNKSIZE;
}



/*************************************************
*          Read a PMW string of any length       *
*************************************************/

/* The string may extend over more than one line; newlines count as spaces. PMW
strings are expected to be in UTF-8 format; any non-UTF-8 bytes are taken as
single 8-bit characters and converted to UTF-8, with a warning emitted. The
string is converted into an array of 32-bit values. The most significant byte
of each value is a font id. For the remaining 24 bits, values up to 0x10ffff
are Unicode code points. Larger values are used for special items such as
"insert page number here".

The fontid argument is the initial font. In the case of a stave string, this is
font_unknown, because the default for a stave string isn't known until the
options that follow the string are read. For this, and also for heading
strings, the string check is deferred till later via the string_check argument.

Memory usage: the string is first read into an on-stack buffer, which is large
enough for many short strings. If the string fits, a copy is made in a
mem_get() block. For longer strings, a private malloc() buffer is obtained and
expanded if necessary up to a (large) maximum. At the end, this is passed to
mem_register() so that it is freed on exit and its size is recorded for memory
statistics.

Arguments:  the starting font id
Returns:    pointer to memory containing the string or NULL on error
*/

uint32_t *
string_read(uint32_t fontid, BOOL check_string)
{
size_t p = 0;
size_t mem_size = 0;
size_t string_max = STRINGBUFFER_CHUNKSIZE - 2;
uint32_t set_fontid;
uint32_t stack_string[STRINGBUFFER_CHUNKSIZE * sizeof(uint32_t)];
uint32_t *yield = stack_string;
void *block = NULL;

read_sigc();
if (read_c != '\"')
  {
  error(ERR8, "string in quotes");
  return NULL;
  }

/* The font is held in the top 8 bits of the 32-bit words. Escapes that
change the font for just one character adjust the value of set_fontid. It gets
reset to fontid afterwards. */

set_fontid = fontid <<= 24;

/* Scan the input string */

for (read_nextc(); read_c != '\"' && read_c != ENDFILE; read_nextc())
  {
  int bot, top;
  int aa = string_check_utf8(main_readbuffer + read_i - 1) - 1; /* additional bytes */

  /* Pick up any additional UTF-8 bytes. If aa < 0 the byte is illegal UTF-8.
  Give a warning and accept it as a one-byte value. */

  if (aa > 0)
    {
    int ii;
    int ss = 6*aa;
    read_c = (read_c & utf8_table3[aa]) << ss;
    for (ii = 1; ii <= aa; ii++)
      {
      ss -= 6;
      read_c |= (main_readbuffer[read_i++] & 0x3f) << ss;
      }
    }
  else if (aa < 0) error(ERR66, read_c);   /* Warning */

  /* Vertical bar is used to separate left/middle/right text in headings, and
  also separates lines in stave names. We turn it into a special "character"
  for this reason. */

  if (read_c == '|') read_c = ss_verticalbar;

  /* Handle escape sequences */

  else if (read_c == '\\')
    {
    usint i;
    int16_t pitch, abspitch;
    uint8_t acc;

    /* Deal with font changes, first checking for \sc\, which adds the "small
    size" bit into the current font, and for \mu\ which adds the same bit into
    the music font. */

    if (memcmp(main_readbuffer + read_i, "sc\\", 3) == 0)
      {
      fontid |= font_small << 24;
      read_i += 3;
      goto NEXTSTRINGCHAR;
      }

    if (memcmp(main_readbuffer + read_i, "mu\\", 3) == 0)
      {
      fontid = (font_mf | font_small) << 24;
      read_i += 3;
      goto NEXTSTRINGCHAR;
      }

    /* Note that the string lengths are built-in here; they must be kept in
    step with the contents of font_IdStrings. */

    for (i = 0; i < font_tablen; i++)
      {
      size_t len = (i < 6)? 2 : (i < 15)? 3 : 4;
      if (memcmp(main_readbuffer + read_i, font_IdStrings[i], len) == 0 &&
          main_readbuffer[read_i + len] == '\\')
        {
        fontid = i << 24;
        read_i += len + 1;
        goto NEXTSTRINGCHAR;
        }
      }

    /* '\x' was overloaded: followed by a small number, it was a font change to
    an extra font. Otherwise it is a character number, specified in
    hexadecimal, which we deal with later on. Retain the font-change
    interpretation for compatibility, but give a warning. */

    if (main_readbuffer[read_i] == 'x' &&
        isdigit(main_readbuffer[read_i + 1]))
      {
      uschar *end;
      unsigned long int n = Ustrtoul(main_readbuffer + read_i + 1, &end, 10);
      if (n <= MaxExtraFont && *end == '\\')
        {
        fontid = (font_xx + n - 1) << 24;
        read_i = end - main_readbuffer + 1;
        if (!fontwarned)
          {
          error(ERR67);
          fontwarned = TRUE;
          }
        goto NEXTSTRINGCHAR;
        }
      }

    /* Deal with some fixed sequences such as \ff (no terminating \) but also
    those like \p\ which do end with a backslash. All generate a single value
    to add to the string. */

    for (i = 0; i < esclist_size; i++)
      {
      if (memcmp(main_readbuffer + read_i, esclist[i].s, esclist[i].len) == 0)
        {
        read_c = esclist[i].c;
        read_i += esclist[i].len;
        goto STORECHAR;
        }
      }

    /* Handle remaining escapes individually */

    read_nextc();
    switch (read_c)
      {
      case '@':       /* starts within-string comment */
      for (read_nextc(); read_c != '\\' && read_c != ENDFILE; read_nextc()) {}
      goto NEXTSTRINGCHAR;

      /* Double quotes and non-special vertical bar can be input by escaping. */

      case '|':
      case '"':
      goto STORECHAR;

      /* The escape into the music font can be repeated multiple times, e.g.
      \*23*24**25\ so we store characters directly here. */

      case '*':
      for (;;)
        {
        uschar *pm;

        read_nextc();
        if (read_c == '*')
          {
          set_fontid = font_mf << 24;
          read_nextc();
          }
        else set_fontid = (font_mf|font_small) << 24;

        if ((pm = Ustrchr(music_escapes, read_c)) != NULL)
          {
          size_t n = pm - music_escapes;

          if (p > string_max)
            expand_string_buffer(&block, &yield, &mem_size, &string_max);
          yield[p++] = music_escape_values[n] | set_fontid;

          /* Allow '.' after bsmcq (i.e. a note) */

          if (n <= 4 && main_readbuffer[read_i] == '.')
            {
            if (p > string_max)
              expand_string_buffer(&block, &yield, &mem_size, &string_max);
            yield[p++] = 63 | set_fontid;
            read_i++;
            }
          }

        else if (isdigit(read_c) ||
                  (read_c == 'x' && isxdigit(main_readbuffer[read_i])))
          {
          int base;
          uschar *end;
          unsigned long ul;
          if (read_c == 'x') base = 16;
            else { base = 10; read_i--; }
          ul = Ustrtoul(main_readbuffer + read_i, &end, base);
          if (p > string_max)
            expand_string_buffer(&block, &yield, &mem_size, &string_max);
          yield[p++] = ul | set_fontid;
          read_i = end - main_readbuffer;
          }

        else
          {
          error_skip(ERR68, '\"', "after \\* or \\** a music character code or number is");
          goto ENDSTRING;
          }

        /* Inspect the character after the \* escape. If it is backslash, we
        are done with this sequence. If it is * we expect another escape.
        Otherwise it's an error. */

        read_nextc();
        if (read_c == '\\') goto NEXTSTRINGCHAR;
        if (read_c  != '*')
          {
          error_skip(ERR68, '\"', "\"\\\" or \"*\"");
          goto ENDSTRING;
          }
        }    /* End of loop for \* escapes */
      break;

      /* A character from the Symbol font is of the form \s...\ or \sx...\ but
      if \s is not of these forms, it may be the start of an accented
      character. */

      case 's':
      read_nextc();
      if (isdigit(read_c) ||
           (read_c == 'x' && isxdigit(main_readbuffer[read_i])))
        {
        int base;
        uschar *end;
        unsigned long ul;
        if (read_c == 'x') base = 16;
          else { base = 10; read_i--; }
        ul = Ustrtoul(main_readbuffer + read_i, &end, base);
        read_i = end - main_readbuffer;
        read_nextc();
        if (read_c != '\\')
          {
          error_skip(ERR68, '\"', "\\");
          goto ENDSTRING;
          }
        set_fontid = font_sy << 24;
        read_c = ul;   /* Character to be stored */
        }
      else
        {
        read_c |= 's' << 8;
        goto ACCENTED;
        }
      break;

      /* The case of \x for a font change to an extra font is handled above;
      here we handle \x...\ as an escape for a Unicode character. Also handle \
      followed by a digit for a decimal character number. */

      case 'x':
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        {
        int base;
        uschar *end;
        unsigned long int ul;
        if (read_c != 'x')
          {
          base = 10;
          read_i--;
          }
        else base = 16;
        ul = Ustrtoul(main_readbuffer + read_i, &end, base);
        /* This error can't happen for decimal numbers */
        if (end == main_readbuffer + read_i)
          {
          error_skip(ERR68, '\"', "hexadecimal number");
          goto ENDSTRING;
          }
        read_i = end - main_readbuffer;
        read_nextc();
        if (read_c != '\\')
          {
          error_skip(ERR68, '\"', "\\");
          goto ENDSTRING;
          }
        read_c = ul;   /* Character to be stored */
        }
      break;

      /* The escape \t\ inserts the current transposition amount. Otherwise, if
      \t is followed by A-G it is a transposable key signature name. Otherwise
      it might be an accented letter. */

      case 't':
      read_nextc();

      /* The maximum transpostion is limited so if we allow for 8 characters
      that will be plenty. */

      if (read_c == '\\')
        {
        char buff[16];
        int t = (active_transpose == NO_TRANSPOSE)? 0 : active_transpose/2;
        if (p + 8 > string_max)
          expand_string_buffer(&block, &yield, &mem_size, &string_max);
        sprintf(buff, "%d", t);
        for (char *pp = buff; *pp != 0; pp++) yield[p++] = *pp | set_fontid;
        goto NEXTSTRINGCHAR;
        }

      /* Test for transposed key name */

      if (read_c < 'A' || read_c > 'G')
        {
        read_c |= 't' << 8;
        goto ACCENTED;
        }

      /* Set up an appropriate note in the middle C octave, and then transpose
      it if required. */

      pitch = MIDDLE_C + read_basicpitch[read_c - 'A'];
      read_nextc();
      switch(read_c)
        {
        case '#': acc = ac_sh; break;
        case '$': acc = ac_fl; break;
        case '%': acc = ac_nt; break;
        default:  acc = ac_no; read_i--; break;
        }
      abspitch = pitch + read_accpitch[acc];

      if (active_transpose != NO_TRANSPOSE)
        abspitch = transpose_note(
          abspitch,    /* input absolute pitch */
          &pitch,      /* for output new pitch */
          &acc,        /* for output accidental */
          ac_no,       /* output accidental - none set */
          TRUE,        /* force accidental - not relevant for text */
          FALSE,       /* note set force accidental - not relevant for text */
          TRUE,        /* single note - not relevant for text */
          TRUE,        /* texttranspose */
          0);          /* tie count - not relevant for text */

      /* Ensure room for up to 5 characters in the output buffer. */

      if (p > string_max - 4) expand_string_buffer(&block, &yield, &mem_size,
        &string_max);

      /* Retrieve a pitch within an octave and thence the note letter. */

      pitch %= 24;
      for (i = 0; i < 6; i++)
        {
        if (pitch == read_basicpitch[i]) break;
        }

      /* Double sharps and double flats are never used, so change to the
      appropriate enharmonic note. We should only ever get "sensible" double
      accidentals. */

      if (acc == ac_ds) i = (i == 6)? 0 : i + 1;
        else if (acc == ac_df) i = (i == 0)? 6 : i - 1;

      yield[p++] = set_fontid | ('A' + i);
      if (acc == ac_sh)
        {
        yield[p++] = ((font_mf|font_small) << 24) | 126;  /* Move up */
        yield[p++] = ((font_mf|font_small) << 24) | 37;   /* Sharp */
        yield[p++] = ((font_mf|font_small) << 24) | 124;  /* Move down */
        yield[p++] = ((font_mf|font_small) << 24) | 121;  /* Move left */
        }
      else if (acc == ac_fl)
        {
        yield[p++] = ((font_mf|font_small) << 24) | 39;   /* Flat */
        yield[p++] = ((font_mf|font_small) << 24) | 121;  /* Move left */
        }

      goto NEXTSTRINGCHAR;

      /* Handle accented character pairs. 's' and 't' are not in this list
      because of the use of \s<number>\ for a character in the Symbol font and
      the use of \t<letter> for transposing a key name. There will be a goto
      from above when \s or \t is followed by something else. */

      case 'a': case 'c': case 'd': case 'e': case 'g': case 'h': case 'i':
      case 'j': case 'l': case 'n': case 'o': case 'r': case 'u': case 'w':
      case 'y': case 'z':
      case 'A': case 'C': case 'D': case 'E': case 'G': case 'H': case 'I':
      case 'J': case 'L': case 'N': case 'O': case 'R': case 'S': case 'T':
      case 'U': case 'W': case 'Y': case 'Z':

      /* Get the pair of letters into a 16-bit field. */

      read_c = (read_c << 8) | main_readbuffer[read_i++];

      ACCENTED:  /* Come here from '\s' or '\t' processing above */

      /* \c] == \C] is special; it gives copyright from the Symbol font */

      if (read_c == ('c' << 8) + ']' || read_c == ('C' << 8) + ']')
        {
        set_fontid = font_sy;
        read_c = 211;
        break;
        }

      /* The rest can be handled by a table, which turns them into Unicode */

      bot = 0;
      top = escacccount;
      while (bot < top)
        {
        int mid = (bot + top)/2;
        if (read_c == escacc[mid].escape)
          {
          read_c = escacc[mid].unicode;
          goto STORECHAR;
          }
        if (read_c < escacc[mid].escape) top = mid; else bot = mid + 1;
        }

      /* For an undefined pair, put read_c back to a single character. */

      read_c &= 0xffu;

      /* Fall through */

      default:
      error(ERR69);   /* Unknown escape sequence */
      break;
      }
    }  /* End of escape handling */

  /* We have a value in read_c that is to be added to the string. Increase the
  string size if necessary. */

  STORECHAR:
  if (p > string_max) expand_string_buffer(&block, &yield, &mem_size,
    &string_max);
  yield[p++] = set_fontid | read_c;

  NEXTSTRINGCHAR:
  set_fontid = fontid;
  }

ENDSTRING:
if (read_c == ENDFILE) error(ERR65);  /* Hard */
yield[p++] = 0;                       /* Terminating zero */
mem_size = p * sizeof(uint32_t);      /* Actual bytes used */

/* If the string fitted into the on-stack buffer, copy it into a normal memory
block. Otherwise, adjust the size of the malloc() block and register it. */

if (block == NULL)
  {
  uint32_t *new = mem_get(mem_size);
  memcpy(new, yield, mem_size);
  yield = new;
  }

else
  {
  mem_size += sizeof(char *);
  block = realloc(block, mem_size);
  if (block == NULL) error(ERR0, "re-", "PMW string", mem_size);  /* Hard */
  mem_register(block, mem_size);
  yield = (uint32_t *)((char *)(block) + sizeof(char *));
  }

/* If requested, check the string for validity before returning. Some strings
defer this checking till later. */

if (check_string) yield = string_check(yield, NULL, FALSE);
read_nextc();
return yield;
}



/*************************************************
*           Read basic stave string              *
*************************************************/

/* This is called from string_stavestring() below, more than once in the case
of underlay/overlay strings that have additional auxiliary strings, which allow
only a restricted set of options.

Arguments:
  b         points to a structure to return data
  rehearse  TRUE for a rehearsal string
  opts      if not NULL, restrict to these options
  opterror  string for bad option error when opts not NULL

Returns:   TRUE if the options end with /" (current character is then ")
*/

typedef struct {
  uint32_t *string;
  uint32_t  flags;
   int32_t  offset;
   int32_t  adjustx;
   int32_t  adjusty;
   int32_t  halfway;
   int32_t  rotate;
   int32_t  size;
   BOOL     hadab;
} basestavestring;


static BOOL
read_basestring(basestavestring *b, BOOL rehearse, const char *opts,
  const char *opterror)
{
b->hadab = FALSE;
b->flags = rehearse? 0 : srs.textflags;
b->offset = b->adjustx = b->adjusty = b->halfway = b->rotate = 0;
b->size = -1;

/* Read the string with an initial unknown font. This will end up as 0xff in
the font byte of relevant characters. When we know the string type, we scan the
string and adjust the default font where necessary. Also, defer the string
checking. */

b->string = string_read(font_unknown, FALSE);
if (b->string == NULL) return FALSE;  /* Error */

/* Handle options. Note that two successive slashes are a caesura, not a bad
string option. The extra strings have a limited set of options. */

while (read_c == '/' && main_readbuffer[read_i] != '/')
  {
  read_nextc();
  if (read_c == '"') return TRUE;

  if (opts != NULL && Ustrchr(opts, (int)read_c) == NULL)
    {
    error_skip(ERR8, ' ', opterror);
    continue;
    }

  switch(read_c)
    {
    case 'a':
    b->hadab = TRUE;
    b->flags &= ~(text_ul|text_fb|text_middle|text_atulevel|text_absolute);
    b->flags |= text_above;
    read_nextc();
    if (read_c == 'o')
      {
      b->flags |= text_atulevel;
      read_nextc();
      }
    else if (isdigit(read_c))
      {
      b->flags |= text_absolute;
      b->adjusty = read_fixed();
      }
    break;

    case 'b':
    read_nextc();
    if (read_c == 'o' && main_readbuffer[read_i] == 'x')
      {
      b->flags |= text_boxed;
      read_i++;
      read_nextc();
      }
    else if (read_c == 'a' && main_readbuffer[read_i] == 'r')
      {
      b->flags &= ~text_barcentre;
      b->flags |= text_baralign;
      read_i++;
      read_nextc();
      }
    else   /* /b or /bu */
      {
      b->hadab = TRUE;
      b->flags &= ~(text_ul|text_fb|text_above|text_middle|text_atulevel|text_absolute);
      if (read_c == 'u')
        {
        b->flags |= text_atulevel;
        read_nextc();
        }
      else if (isdigit(read_c))
        {
        b->flags |= text_absolute;
        b->adjusty = -read_fixed();
        }
      }
    break;

    case 'c':
    read_nextc();
    if (read_c == 'b')
      {
      b->flags &= ~(text_baralign|text_centre|text_endalign|text_timealign);
      b->flags |= text_barcentre;
      read_nextc();
      }
    else
      {
      b->flags &= ~(text_endalign|text_barcentre);
      b->flags |= text_centre;
      }
    break;

    case 'd':
    b->adjusty -= read_movevalue();
    break;

    case 'e':
    b->flags &= ~(text_centre|text_barcentre);
    b->flags |= text_endalign;
    read_nextc();
    break;

    case 'F':
    if (!srs.string_followOK || rehearse) error(ERR93);
      else b->flags |= text_followon;
    read_nextc();
    break;

    case 'f':
    read_nextc();
    if (read_c == 'b')
      {
      b->flags &= ~(text_ul | text_above | text_middle | text_atulevel);
      b->flags |= text_fb;
      if (main_readbuffer[read_i] == 'u')
        {
        b->flags |= text_atulevel;
        read_i++;
        }
      }
    else error(ERR8, "/fb");
    read_nextc();
    break;

    case 'h':
    read_nextc();
    b->halfway = isdigit(read_c)? read_fixed() : 500;
    break;

    case 'l':
    if (main_readbuffer[read_i] == 'c')
      {
      read_nextc();
      b->offset -= read_movevalue();
      }
    else b->adjustx -= read_movevalue();
    break;

    case 'm':
    b->flags &= ~(text_ul|text_fb|text_above|text_atulevel|text_absolute);
    b->flags |= text_middle;
    read_nextc();
    break;

    case 'n':
    read_nextc();
    if (read_c == 'c') b->flags &= ~text_centre;
    else if (read_c == 'e') b->flags &= ~text_endalign;
    else error(ERR8, "/nc or /ne");
    read_nextc();
    break;

    case 'o':
    read_nextc();
    if (read_c == 'l')
      {
      b->flags &= ~text_fb;
      b->flags |= text_ul | text_above;
      }
    else error(ERR8, "/ol");
    read_nextc();
    break;

    case 'r':
    if (Ustrncmp(main_readbuffer + read_i, "box", 3) == 0)
      {
      b->flags |= text_boxed | text_boxrounded;
      read_i += 3;
      read_nextc();
      }
    else if (Ustrncmp(main_readbuffer + read_i, "ing", 3) == 0)
      {
      b->flags |= text_ringed;
      read_i += 3;
      read_nextc();
      }
    else if (Ustrncmp(main_readbuffer + read_i, "ot", 2) == 0)
      {
      read_i += 2;
      read_nextc();
      read_expect_integer(&b->rotate, TRUE, TRUE);
      }
    else if (main_readbuffer[read_i] == 'c')
      {
      read_nextc();
      b->offset += read_movevalue();
      }
    else b->adjustx += read_movevalue();
    break;

    case 'S':
    read_nextc();
    if (read_expect_integer(&b->size, FALSE, FALSE))
      {
      if (b->size == 0 || b->size - 1 >= FixedFontSizes)
        {
        error(ERR75, FixedFontSizes);
        b->size = 0;
        }
      else b->size += UserFontSizes - 1;
      }
    break;

    case 's':
    read_nextc();
    if (read_expect_integer(&b->size, FALSE, FALSE))
      {
      if (b->size == 0 || b->size - 1 >= UserFontSizes)
        {
        error(ERR75, UserFontSizes);
        b->size = 0;
        }
      else b->size--;  /* Base 0 offset */
      }
    break;

    case 't':
    read_nextc();
    if (read_c == 's')
      {
      b->flags &= ~text_barcentre;
      b->flags |= text_timealign;
      }
    else error(ERR8, "/ts");
    read_nextc();
    break;

    case 'u':
    if (isdigit(main_readbuffer[read_i])) b->adjusty += read_movevalue(); else
      {
      read_nextc();
      if (read_c == 'l')
        {
        b->flags &= ~(text_fb | text_above);
        b->flags |= text_ul;
        }
      else error(ERR8, "/u<number> or /ul");
      read_nextc();
      }
    break;

    default:
    error_skip(ERR8, ('/'<<8) | ' ', "/a, /ao, /b, /bar, /box, /bu, /d, /e, "
      "/F, /fb, /h, /l, /m, /ol, /r, /rbox, /ring, /S, /s, /u or /ul");
    break;
    }
  }

return FALSE;
}



/*************************************************
*        Set default font in a string            *
*************************************************/

/* When a stave string is read, the default font isn't known until the
following options have been considered. Therefore, string is read with
font_unknown as its default. This will cause the font bytes the string to be
0x7f. This function changes such bytes to a given font, taking care to preserve
the font_small bit. We can stop as soon as a character with a font other than
Symbol or Music is encountered; this means the string contained a font setting
so there will be no more characters with an 0x7f font. Individual Symbol and
Music characters can be set by escape sequences without changing the current
font, which is why we must carry on after such characters.

After setting the default font, we can check the validity of the string. If it
is underlay, a string of special syllable separators will be supplied.

Arguments:
  str         the string
  font        the default font
  bseps       special separators or NULL

Returns:      the (possibly modified) string
*/

static uint32_t *
set_default_font(uint32_t *str, uint32_t font, const char *bseps)
{
font = ~(font|font_small) << 24;
for (uint32_t *s = str; *s != 0; s++)
  {
  uint32_t f = PFTOP(*s) & ~(font_small << 24);
  if (f == font_unknown << 24) *s ^= font;
    else if (f != font_sy << 24 && f != font_mf << 24) break;
  }
return string_check(str, bseps, FALSE);
}



/*************************************************
*     Read string in stave and handle options    *
*************************************************/

/* A b_textstr item is added to the stave data or to the pending
underlay/overlay chain. This function is called when the initial quote is read.

Argument:  TRUE for a rehearsal string
Returns:   nothing
*/

void
string_stavestring(BOOL rehearse)
{
BOOL more;
BOOL undoverlay;
uint16_t default_font;
uint16_t htype = 0;
b_textstr *p;
basestavestring bss[3];
basestavestring *s1 = &bss[0];
basestavestring *s2 = NULL;
basestavestring *s3 = NULL;

/* Read the basic string. If it ends with /" there is special handling for
underlay/overlay. */

more = read_basestring(s1, rehearse, NULL, NULL);
if (s1->string == NULL) return;    /* There's been an error */

/* Non-movement options and size settings are ignored on rehearsal marks; they
always use the rehearsal marks style and size. Warn if any are present. */

if (rehearse && (s1->flags != 0 || s1->size >= 0)) 
  {
  error(ERR178);
  s1->flags = 0;  /* No flags */
  s1->size = -1;  /* Unset */ 
  }

/* Now that we know what type of text this is, we can set up the default font
for underlay, overlay, figured bass, or general text, and also set the size if
it is not set. */

undoverlay = (s1->flags & text_ul) != 0;  /* Save repeated testing */

if (undoverlay)
  {
  if ((s1->flags & text_above) == 0)
    {
    default_font = srs.ulfont;
    if (s1->size < 0) s1->size = srs.ulsize;
    }
  else
    {
    default_font = srs.olfont;
    if (s1->size < 0) s1->size = srs.olsize;
    }
  }

else if ((s1->flags & text_fb) != 0)
  {
  default_font = srs.fbfont;
  if (s1->size < 0) s1->size = srs.fbsize;
  }

/* Currently, the size of individual rehearsal marks cannot be changed. */

else if (rehearse)
  {
  default_font = curmovt->fonttype_rehearse;
  s1->size = 0;
  }

else
  {
  default_font = srs.textfont;
  if (s1->size < 0) s1->size = srs.textsize;
  }

/* Set the default font. This also performs the string character check; for
underlay/overlay special separators must be supplied. */

s1->string = set_default_font(s1->string, default_font,
  undoverlay? "- =\n" : NULL);

/* If the string ended with /" we process additional strings that may be
present for underlay/overlay. If such a string appears on any other kind of
string it is ignored (with a warning) - we nevertheless process it to try to
reduce error cascades. */

if (more)
  {
  uint32_t *s2a, *s2b;

  s2 = &bss[1];
  more = read_basestring(s2, FALSE, "dSsu", "/d, /S, /s, or /u");
  if (s2->string == NULL) return;             /* There's been an error */
  if (s2->size < 0) s2->size = s1->size;      /* Default to main string size */
  s2->string = set_default_font(s2->string, default_font, NULL);

  /* The second extra string is split into two at an unescaped vertical bar if
  there is one. If not, the second substring is NULL. */

  for (s2a = s2b = s2->string; *s2b != 0; s2b++)
     {
     if (PCHAR(*s2b) == ss_verticalbar)
       {
       *s2b++ = 0;
       break;
       }
     }
  if (*s2b == 0) s2b = NULL;

  /* Read the third string if present; arrange for an error if an apparent
  fourth. */

  if (more)
    {
    s3 = &bss[2];
    more = read_basestring(s3, FALSE, "Ss", "/s or /S");
    if (s3->string == NULL) return;          /* There's been an error */
    if (s3->size < 0) s3->size = s1->size;   /* Default to main string size */
    if (more)
      {
      read_i--;
      read_c = '/';
      }
    s3->string = set_default_font(s3->string, default_font, NULL);
    }

  /* The multiple string stuff applies only to {und,ov}erlay. Set up a
  new global hyphen-type block, or find the number of an identical one. */

  if (!undoverlay) error(ERR94); else
    {
    htypestr *h = main_htypes;
    htypestr **hh = &main_htypes;
    htype++;

    while (h != NULL)
      {
      if (string_pmweq(h->string1, s2a) &&
         ((h->string2 == NULL && s3 == NULL) ||
          (h->string2 != NULL && s3 != NULL &&
            string_pmweq(h->string2, s3->string))) &&
         ((h->string3 == NULL && s2b == NULL) ||
          (h->string3 != NULL && s2b != NULL &&
            string_pmweq(h->string3, s2b))) &&
         h->adjust == s2->adjusty &&
         h->size1 == s1->size && h->size2 == s2->size) break;
      hh = &(h->next);
      h = *hh;
      htype++;
      }

    if (h == NULL)
      {
      h = mem_get(sizeof(htypestr));
      *hh = h;
      h->next = NULL;
      h->string1 = s2a;
      h->string2 = (s3 == NULL)? NULL : s3->string;
      h->string3 = s2b;
      h->adjust = s2->adjusty;
      h->size1 = s1->size;
      h->size2 = (s3 == NULL)? s2->size : s3->size;
      }
    }
  }

/* If this is a follow-on string, allow only relative positioning options.
Follow-ons are forbidden for rehearsal strings above. */

if ((s1->flags & text_followon) != 0)
  {
  if (s1->hadab || s1->halfway != 0 || s1->offset != 0 ||
       ((s1->flags & (text_baralign|text_boxed|text_centre|text_endalign|
         text_fb|text_middle|text_ringed|text_timealign|text_ul)) != 0))
    {
    error(ERR95,
      ((s1->flags & (text_boxed|text_ringed)) != 0)? "boxed or ringed string" :
      "string with explicit positioning");
    s1->flags &= ~text_followon;
    }
  }

/* Warn if both halfway and offset were given. */

if (s1->halfway != 0 && s1->offset != 0) error(ERR96);

/* If the next significant character is double quote, the next string can be a
follow-on, unless this string is boxed or ringed. */

read_sigc();
srs.string_followOK = read_c == '"' &&
  (s1->flags & (text_boxed|text_ringed)) == 0;

/* If the absolute flag is set and we have not had /a or /b, then add in the
default absolute position. */

if ((s1->flags & text_absolute) != 0 && !s1->hadab)
  s1->adjusty += srs.textabsolute;

/* Override flags for rehearsal strings. */

if (rehearse) s1->flags = text_rehearse | text_above | curmovt->rehearsalstyle;

/* Set up a stave text block that is not (yet) connected to the current bar's
chain of items. */

p = mem_get(sizeof(b_textstr));
p->type = b_text;
p->string = s1->string;
p->x = s1->adjustx;
p->y = s1->adjusty;
p->rotate = s1->rotate;
p->halfway = s1->halfway;
p->offset = s1->offset;
p->flags = s1->flags;
p->size = s1->size;
p->htype = htype;
p->laylevel = 0;
p->laylen = 0;

/* If this is not underlay or overlay, connect it to the current bar's item
chain and we are done. */

if ((s1->flags & text_ul) == 0)
  {
  mem_connect_item((bstr *)p);
  }

/* For an underlay or overlay string, insert the block into a pending chain at
the relevant verse level. Such text is held on the chain until it is all
parcelled out to the notes that follow. Give an error if rotation is attempted.
*/

else
  {
  uint8_t level = 0;
  b_textstr *q, *qp;
  b_textstr **cptr = ((s1->flags & text_above) == 0)?
    &(srs.pendulay) : &(srs.pendolay);

  if (s1->rotate != 0) error(ERR97, "supported", "for underlay or overlay");

  for (qp = NULL, q = *cptr; q != NULL; qp = q, q = (b_textstr *)(q->next))
    {
    if (q->laylevel > level) break;
    level++;
    }

  p->laylevel = level;
  p->next = (bstr *)q;
  p->prev = (bstr *)qp;
  if (qp == NULL) *cptr = p; else qp->next = (bstr *)p;
  if (q != NULL) q->prev = (bstr *)p;
  }
}

/* End of string.c */
