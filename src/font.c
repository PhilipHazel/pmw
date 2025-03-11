/*************************************************
*              PMW font functions                *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: January 2021 */
/* This file last modified: February 2025 */

#include "pmw.h"


/* Entries in the character name to Unicode code point table */

typedef struct an2uencod {
  uschar *name;                /* Adobe character name */
  int code;                    /* Unicode code point */
  int poffset;                 /* Offset for printing certain chars */
} an2uencod;

/* This table translates character names from PostScript fonts that use Adobe's
standard encoding into Unicode. In addition, for characters whose Unicode
values are greater than LOWCHARLIMIT, it includes the offset above LOWCHARLIMIT
that we use for printing these characters (essentially a private encoding). The
current highest offset is 43. The value of FONTWIDTHS_SIZE must be large enough
to accommodate all these characters. */

static an2uencod an2ulist[] = {
  { US"A",               0x0041, -1 },
  { US"AE",              0x00c6, -1 },
  { US"Aacute",          0x00c1, -1 },
  { US"Abreve",          0x0102, -1 },
  { US"Acircumflex",     0x00c2, -1 },
  { US"Adieresis",       0x00c4, -1 },
  { US"Agrave",          0x00c0, -1 },
  { US"Amacron",         0x0100, -1 },
  { US"Aogonek",         0x0104, -1 },
  { US"Aring",           0x00c5, -1 },
  { US"Atilde",          0x00c3, -1 },
  { US"B",               0x0042, -1 },
  { US"C",               0x0043, -1 },
  { US"Cacute",          0x0106, -1 },
  { US"Ccaron",          0x010c, -1 },
  { US"Ccedilla",        0x00c7, -1 },
  { US"Ccircumflex",     0x0108, -1 },
  { US"Cdotaccent",      0x010a, -1 },
  { US"D",               0x0044, -1 },
  { US"Dcaron",          0x010e, -1 },
  { US"Dcroat",          0x0110, -1 },
  { US"Delta",           0x0394, +0 },
  { US"E",               0x0045, -1 },
  { US"Eacute",          0x00c9, -1 },
  { US"Ebreve",          0x0114, -1 },
  { US"Ecaron",          0x011a, -1 },
  { US"Ecircumflex",     0x00ca, -1 },
  { US"Edieresis",       0x00cb, -1 },
  { US"Edotaccent",      0x0116, -1 },
  { US"Egrave",          0x00c8, -1 },
  { US"Emacron",         0x0112, -1 },
  { US"Eng",             0x014a, -1 },
  { US"Eogonek",         0x0118, -1 },
  { US"Eth",             0x00d0, -1 },
  { US"Euro",            0x20ac, +1 },
  { US"F",               0x0046, -1 },
  { US"G",               0x0047, -1 },
  { US"Gbreve",          0x011e, -1 },
  { US"Gcircumflex",     0x011c, -1 },
  { US"Gcommaaccent",    0x0122, -1 },
  { US"Gdotaccent",      0x0120, -1 },
  { US"H",               0x0048, -1 },
  { US"Hbar",            0x0126, -1 },
  { US"Hcircumflex",     0x0124, -1 },
  { US"I",               0x0049, -1 },
  { US"IJ",              0x0132, -1 },
  { US"Iacute",          0x00cd, -1 },
  { US"Ibreve",          0x012c, -1 },
  { US"Icircumflex",     0x00ce, -1 },
  { US"Idieresis",       0x00cf, -1 },
  { US"Idotaccent",      0x0130, -1 },
  { US"Igrave",          0x00cc, -1 },
  { US"Imacron",         0x012a, -1 },
  { US"Iogonek",         0x012e, -1 },
  { US"Itilde",          0x0128, -1 },
  { US"J",               0x004a, -1 },
  { US"Jcircumflex",     0x0134, -1 },
  { US"K",               0x004b, -1 },
  { US"Kcommaaccent",    0x0136, -1 },
  { US"L",               0x004c, -1 },
  { US"Lacute",          0x0139, -1 },
  { US"Lcaron",          0x013d, -1 },
  { US"Lcommaaccent",    0x013b, -1 },
  { US"Ldot",            0x013f, -1 },
  { US"Lslash",          0x0141, -1 },
  { US"M",               0x004d, -1 },
  { US"N",               0x004e, -1 },
  { US"NBspace",         0x00a0, -1 },  /* Added Nov-2019 */
  { US"Nacute",          0x0143, -1 },
  { US"Ncaron",          0x0147, -1 },
  { US"Ncommaaccent",    0x0145, -1 },
  { US"Ntilde",          0x00d1, -1 },
  { US"O",               0x004f, -1 },
  { US"OE",              0x0152, -1 },
  { US"Oacute",          0x00d3, -1 },
  { US"Obreve",          0x014e, -1 },
  { US"Ocircumflex",     0x00d4, -1 },
  { US"Odieresis",       0x00d6, -1 },
  { US"Ograve",          0x00d2, -1 },
  { US"Ohungarumlaut",   0x0150, -1 },
  { US"Omacron",         0x014c, -1 },
  { US"Oslash",          0x00d8, -1 },
  { US"Otilde",          0x00d5, -1 },
  { US"P",               0x0050, -1 },
  { US"Q",               0x0051, -1 },
  { US"R",               0x0052, -1 },
  { US"Racute",          0x0154, -1 },
  { US"Rcaron",          0x0158, -1 },
  { US"Rcommaaccent",    0x0156, -1 },
  { US"S",               0x0053, -1 },
  { US"Sacute",          0x015a, -1 },
  { US"Scaron",          0x0160, -1 },
  { US"Scedilla",        0x015e, -1 },
  { US"Scircumflex",     0x015c, -1 },
  { US"Scommaaccent",    0x0218, +2 },
  { US"T",               0x0054, -1 },
  { US"Tbar",            0x0166, -1 },
  { US"Tcaron",          0x0164, -1 },
  { US"Tcedilla",        0x0162, -1 },
  { US"Tcommaaccent",    0x021a, +3 },
  { US"Thorn",           0x00de, -1 },
  { US"U",               0x0055, -1 },
  { US"Uacute",          0x00da, -1 },
  { US"Ubreve",          0x016c, -1 },
  { US"Ucircumflex",     0x00db, -1 },
  { US"Udieresis",       0x00dc, -1 },
  { US"Ugrave",          0x00d9, -1 },
  { US"Uhungarumlaut",   0x0170, -1 },
  { US"Umacron",         0x016a, -1 },
  { US"Uogonek",         0x0172, -1 },
  { US"Uring",           0x016e, -1 },
  { US"Utilde",          0x0168, -1 },
  { US"V",               0x0056, -1 },
  { US"W",               0x0057, -1 },
  { US"Wcircumflex",     0x0174, -1 },
  { US"X",               0x0058, -1 },
  { US"Y",               0x0059, -1 },
  { US"Yacute",          0x00dd, -1 },
  { US"Ycircumflex",     0x0176, -1 },
  { US"Ydieresis",       0x0178, -1 },
  { US"Z",               0x005a, -1 },
  { US"Zacute",          0x0179, -1 },
  { US"Zcaron",          0x017d, -1 },
  { US"Zdotaccent",      0x017b, -1 },
  { US"a",               0x0061, -1 },
  { US"aacute",          0x00e1, -1 },
  { US"abreve",          0x0103, -1 },
  { US"acircumflex",     0x00e2, -1 },
  { US"acute",           0x00b4, -1 },
  { US"adieresis",       0x00e4, -1 },
  { US"ae",              0x00e6, -1 },
  { US"agrave",          0x00e0, -1 },
  { US"amacron",         0x0101, -1 },
  { US"ampersand",       0x0026, -1 },
  { US"aogonek",         0x0105, -1 },
  { US"aring",           0x00e5, -1 },
  { US"asciicircum",     0x005e, -1 },
  { US"asciitilde",      0x007e, -1 },
  { US"asterisk",        0x002a, -1 },
  { US"at",              0x0040, -1 },
  { US"atilde",          0x00e3, -1 },
  { US"b",               0x0062, -1 },
  { US"backslash",       0x005c, -1 },
  { US"bar",             0x007c, -1 },
  { US"braceleft",       0x007b, -1 },
  { US"braceright",      0x007d, -1 },
  { US"bracketleft",     0x005b, -1 },
  { US"bracketright",    0x005d, -1 },
  { US"breve",           0x0306, +4 },
  { US"brokenbar",       0x00a6, -1 },
  { US"bullet",          0x00b7, -1 },
  { US"c",               0x0063, -1 },
  { US"cacute",          0x0107, -1 },
  { US"caron",           0x030c, +5 },
  { US"ccaron",          0x010d, -1 },
  { US"ccedilla",        0x00e7, -1 },
  { US"ccircumflex",     0x0109, -1 },
  { US"cdotaccent",      0x010b, -1 },
  { US"cedilla",         0x00b8, -1 },
  { US"cent",            0x00a2, -1 },
  { US"circumflex",      0x0302, +6 },
  { US"colon",           0x003a, -1 },
  { US"comma",           0x002c, -1 },
  { US"commaaccent",     0x0326, +7 },
  { US"copyright",       0x00a9, -1 },
  { US"currency",        0x00a4, -1 },
  { US"d",               0x0064, -1 },
  { US"dagger",          0x2020, +8 },
  { US"daggerdbl",       0x2021, +9 },
  { US"dcaron",          0x010f, -1 },
  { US"dcroat",          0x0111, -1 },
  { US"degree",          0x00b0, -1 },
  { US"dieresis",        0x00a8, -1 },
  { US"divide",          0x00f7, -1 },
  { US"dollar",          0x0024, -1 },
  { US"dotaccent",       0x0307, 10 },
  { US"dotlessi",        0x0131, -1 },
  { US"e",               0x0065, -1 },
  { US"eacute",          0x00e9, -1 },
  { US"ebreve",          0x0115, -1 },
  { US"ecaron",          0x011b, -1 },
  { US"ecircumflex",     0x00ea, -1 },
  { US"edieresis",       0x00eb, -1 },
  { US"edotaccent",      0x0117, -1 },
  { US"egrave",          0x00e8, -1 },
  { US"eight",           0x0038, -1 },
  { US"ellipsis",        0x2026, 11 },
  { US"emacron",         0x0113, -1 },
  { US"emdash",          0x2014, 12 },
  { US"endash",          0x2013, 13 },
  { US"eng",             0x014b, -1 },
  { US"eogonek",         0x0119, -1 },
  { US"equal",           0x003d, -1 },
  { US"eth",             0x00f0, -1 },
  { US"exclam",          0x0021, -1 },
  { US"exclamdown",      0x00a1, -1 },
  { US"f",               0x0066, -1 },
  { US"fi",              0xfb01, 14 },
  { US"five",            0x0035, -1 },
  { US"fl",              0xfb02, 15 },
  { US"florin",          0x0192, 16 },
  { US"four",            0x0034, -1 },
  { US"fraction",        0x2044, 17 },
  { US"g",               0x0067, -1 },
  { US"gbreve",          0x011f, -1 },
  { US"gcircumflex",     0x011d, -1 },
  { US"gcommaaccent",    0x0123, -1 },
  { US"gdotaccent",      0x0121, -1 },
  { US"germandbls",      0x00df, -1 },
  { US"grave",           0x0060, -1 },
  { US"greater",         0x003e, -1 },
  { US"greaterequal",    0x2265, 18 },
  { US"guillemotleft",   0x00ab, -1 },
  { US"guillemotright",  0x00bb, -1 },
  { US"guilsinglleft",   0x2039, 19 },
  { US"guilsinglright",  0x203a, 20 },
  { US"h",               0x0068, -1 },
  { US"hbar",            0x0127, -1 },
  { US"hcircumflex",     0x0125, -1 },
  { US"hungarumlaut",    0x030b, 21 },
  /* 002d is "hyphen-minus"; Unicode also has separate codes for hyphen and
  for minus. We use the latter below. */
  { US"hyphen",          0x002d, -1 },
  { US"i",               0x0069, -1 },
  { US"iacute",          0x00ed, -1 },
  { US"ibreve",          0x012d, -1 },
  { US"icircumflex",     0x00ee, -1 },
  { US"idieresis",       0x00ef, -1 },
  { US"igrave",          0x00ec, -1 },
  { US"ij",              0x0133, -1 },
  { US"imacron",         0x012b, -1 },
  { US"infinity",        0x221e, 43 },
  { US"iogonek",         0x012f, -1 },
  { US"itilde",          0x0129, -1 },
  { US"j",               0x006a, -1 },
  { US"jcircumflex",     0x0135, -1 },
  { US"k",               0x006b, -1 },
  { US"kcommaaccent",    0x0137, -1 },
  { US"kgreenlandic",    0x0138, -1 },
  { US"l",               0x006c, -1 },
  { US"lacute",          0x013a, -1 },
  { US"lcaron",          0x013e, -1 },
  { US"lcommaaccent",    0x013c, -1 },
  { US"ldot",            0x0140, -1 },
  { US"less",            0x003c, -1 },
  { US"lessequal",       0x2264, 22 },
  { US"logicalnot",      0x00ac, -1 },
  { US"longs",           0x017f, -1 },
  { US"lozenge",         0x25ca, 23 },
  { US"lslash",          0x0142, -1 },
  { US"m",               0x006d, -1 },
  { US"macron",          0x00af, -1 },
  { US"minus",           0x2212, 24 },
  { US"mu",              0x00b5, -1 },
  { US"multiply",        0x00d7, -1 },
  { US"n",               0x006e, -1 },
  { US"nacute",          0x0144, -1 },
  { US"napostrophe",     0x0149, -1 },
  { US"ncaron",          0x0148, -1 },
  { US"ncommaaccent",    0x0146, -1 },
  { US"nine",            0x0039, -1 },
  { US"notequal",        0x2260, 25 },
  { US"ntilde",          0x00f1, -1 },
  { US"numbersign",      0x0023, -1 },
  { US"o",               0x006f, -1 },
  { US"oacute",          0x00f3, -1 },
  { US"obreve",          0x014f, -1 },
  { US"ocircumflex",     0x00f4, -1 },
  { US"odieresis",       0x00f6, -1 },
  { US"oe",              0x0153, -1 },
  { US"ogonek",          0x0328, 26 },
  { US"ograve",          0x00f2, -1 },
  { US"ohungarumlaut",   0x0151, -1 },
  { US"omacron",         0x014d, -1 },
  { US"one",             0x0031, -1 },
  { US"onehalf",         0x00bd, -1 },
  { US"onequarter",      0x00bc, -1 },
  { US"onesuperior",     0x00b9, -1 },
  { US"ordfeminine",     0x00aa, -1 },
  { US"ordmasculine",    0x00ba, -1 },
  { US"oslash",          0x00f8, -1 },
  { US"otilde",          0x00f5, -1 },
  { US"p",               0x0070, -1 },
  { US"paragraph",       0x00b6, -1 },
  { US"parenleft",       0x0028, -1 },
  { US"parenright",      0x0029, -1 },
  { US"partialdiff",     0x2202, 27 },
  { US"percent",         0x0025, -1 },
  { US"period",          0x002e, -1 },
  { US"periodcentered",  0x2027, 28 },
  { US"perthousand",     0x2031, 29 },
  { US"plus",            0x002b, -1 },
  { US"plusminus",       0x00b1, -1 },
  { US"q",               0x0071, -1 },
  { US"question",        0x003f, -1 },
  { US"questiondown",    0x00bf, -1 },
  { US"quotedbl",        0x0022, -1 },
  { US"quotedblbase",    0x201e, 30 },
  { US"quotedblleft",    0x201c, 31 },
  { US"quotedblright",   0x201d, 32 },
  { US"quoteleft",       0x2018, 33 },
  { US"quoteright",      0x2019, 34 },
  { US"quotesinglbase",  0x201a, 35 },
  { US"quotesingle",     0x0027, -1 },
  { US"r",               0x0072, -1 },
  { US"racute",          0x0155, -1 },
  { US"radical",         0x221a, 36 },
  { US"rcaron",          0x0159, -1 },
  { US"rcommaaccent",    0x0157, -1 },
  { US"registered",      0x00ae, -1 },
  { US"ring",            0x030a, 37 },
  { US"s",               0x0073, -1 },
  { US"sacute",          0x015b, -1 },
  { US"scaron",          0x0161, -1 },
  { US"scedilla",        0x015f, -1 },
  { US"scircumflex",     0x015d, -1 },
  { US"scommaaccent",    0x0219, 38 },
  { US"section",         0x00a7, -1 },
  { US"semicolon",       0x003b, -1 },
  { US"seven",           0x0037, -1 },
  { US"six",             0x0036, -1 },
  { US"slash",           0x002f, -1 },
  { US"space",           0x0020, -1 },
  { US"sterling",        0x00a3, -1 },
  { US"summation",       0x2211, 39 },
  { US"t",               0x0074, -1 },
  { US"tbar",            0x0167, -1 },
  { US"tcaron",          0x0165, -1 },
  { US"tcedilla",        0x0163, -1 },
  { US"tcommaaccent",    0x021b, 40 },
  { US"thorn",           0x00fe, -1 },
  { US"three",           0x0033, -1 },
  { US"threequarters",   0x00be, -1 },
  { US"threesuperior",   0x00b3, -1 },
  { US"tilde",           0x0303, 41 },
  { US"trademark",       0x2122, 42 },
  { US"two",             0x0032, -1 },
  { US"twosuperior",     0x00b2, -1 },
  { US"u",               0x0075, -1 },
  { US"uacute",          0x00fa, -1 },
  { US"ubreve",          0x016d, -1 },
  { US"ucircumflex",     0x00fb, -1 },
  { US"udieresis",       0x00fc, -1 },
  { US"ugrave",          0x00f9, -1 },
  { US"uhungarumlaut",   0x0171, -1 },
  { US"umacron",         0x016b, -1 },
  { US"underscore",      0x005f, -1 },
  { US"uogonek",         0x0173, -1 },
  { US"uring",           0x016f, -1 },
  { US"utilde",          0x0169, -1 },
  { US"v",               0x0076, -1 },
  { US"w",               0x0077, -1 },
  { US"wcircumflex",     0x0175, -1 },
  { US"x",               0x0078, -1 },
  { US"y",               0x0079, -1 },
  { US"yacute",          0x00fd, -1 },
  { US"ycircumflex",     0x0177, -1 },
  { US"ydieresis",       0x00ff, -1 },
  { US"yen",             0x00a5, -1 },
  { US"z",               0x007a, -1 },
  { US"zacute",          0x017a, -1 },
  { US"zcaron",          0x017e, -1 },
  { US"zdotaccent",      0x017c, -1 },
  { US"zero",            0x0030, -1 }
};

static int an2ucount = sizeof(an2ulist)/sizeof(an2uencod);



/*************************************************
*     Convert character name to Unicode value    *
*************************************************/

/*
Arguments:
  cname    the character name
  mcptr    if not NULL, where to put the special encoding value

Returns:   a Unicode code point, or -1 if not found
*/

static int
an2u(uschar *cname, int *mcptr)
{
int top = an2ucount;
int bot = 0;
if (mcptr != NULL) *mcptr = -1;

while (top > bot)
  {
  int mid = (top + bot)/2;
  an2uencod *an2u = an2ulist + mid;
  int c = Ustrcmp(cname, an2u->name);
  if (c == 0)
    {
    if (mcptr != NULL) *mcptr = an2u->poffset;
    return an2u->code;
    }
  if (c > 0) bot = mid + 1; else top = mid;
  }

return -1;
}



/*************************************************
*        Kern table sorting comparison           *
*************************************************/

/* This is the auxiliary routine used for comparing kern table entries when
sorting them.

Arguments:
  a          pointer to kerntable structure
  b          pointer to kerntable structure

Returns      difference between their "pair" values
*/

static int
kern_table_cmp(const void *a, const void *b)
{
kerntablestr *ka = (kerntablestr *)a;
kerntablestr *kb = (kerntablestr *)b;
return (int)(ka->pair - kb->pair);
}



/*************************************************
*         UTR table sorting comparison           *
*************************************************/

/* This is the auxiliary routine used for comparing UTR table entries when
sorting them.

Arguments:
  a          pointer to utrtable structure
  b          pointer to utrtable structure

Returns      difference between their "pair" values
*/

static int
utr_table_cmp(const void *a, const void *b)
{
utrtablestr *ka = (utrtablestr *)a;
utrtablestr *kb = (utrtablestr *)b;
return (int)(ka->unicode - kb->unicode);
}



/*************************************************
*          Number reader for AFM files           *
*************************************************/

/*
Arguments:
  value       where to return the value
  p           character pointer

Returns:      new value of p
*/

static uschar *
read_number(int *value, uschar *p)
{
int n = 0;
int sign = 1;
while (*p != 0 && *p == ' ') p++;
if (*p == '-') { sign = -1; p++; }
while (isdigit(*p)) n = n * 10 + *p++ - '0';
*value = n * sign;
return p;
}



/*************************************************
*         Find AFM or other file for a font      *
*************************************************/

/* This is an externally-called function, also called from below. Look first in
any additional font directories, then in the default one(s).

Arguments:
  name       the font's name
  ext        ".afm", ".utr", ".pfa", etc., or ""
  fextras    list of extra directories to search
  fdefault   the default directory(s)
  filename   where to return the successful file name
  mandatory  if TRUE, give hard error on failure

Returns:   the opened file or NULL (when not mandatory)
*/

FILE *
font_finddata(uschar *name, const char *ext, uschar *fextras,
  uschar *fdefault, uschar *filename, BOOL mandatory)
{
FILE *f = NULL;
uschar *list = (fextras != NULL)? fextras : fdefault;

for (;;)
  {
  uschar *pp = list;
  while (*pp != 0)
    {
    int len;
    uschar *ep = Ustrchr(pp, ':');
    if (ep == NULL) ep = Ustrchr(pp, 0);

    /* Using sprintf() for the whole string causes a compiler warning if
    -Wformat-overflow is set. */

    len = ep - pp;
    (void)memcpy(filename, pp, len);
    sprintf(CS (filename + len), "/%s%s", name, ext);
    f = Ufopen(filename, "r");
    if (f != NULL) return f;
    if (*ep == 0) break;
    pp = ep + 1;
    }

  if (list == fdefault) break;
  list = fdefault;
  }

if (mandatory && f == NULL)
  {
  const char *s = (*ext == 0)? "Data" : ext;
  if (fextras == NULL)
    error(ERR56, s, name, fdefault);           /* Hard error */
  else
    error(ERR57, s, name, fextras, fdefault);  /* Hard error */
  }

return f;
}



/*************************************************
*        Check character value in std font       *
*************************************************/

/* In a standardly encoded font, characters less than LOWCHARLIMIT are encoded
as their Unicode values. There are also around 40 much higher-valued characters
that are translated to the code points immediately above LOWCHARLIMIT so that
they can be accessed from the second encoding of the font. This function checks
and translates characters when necessary. It is called when reading kerning
tables for a standardly-encoded font. Just ignore anything unrecognized.

Arguments:
  c         the code point
  fs        the font structure

Returns:    possibly changed character
*/

static uint32_t
check_lowchar(uint32_t c, fontstr *fs)
{
tree_node *t;
uschar utf[8];
if (c < LOWCHARLIMIT || (fs->flags & ff_stdencoding) == 0) return c;
utf[misc_ord2utf8(c, utf)] = 0;
t = tree_search(fs->high_tree, utf);
return (t == NULL)? c : (uint32_t)(LOWCHARLIMIT + t->value);
}



/***********************************************************
*  Load width, kern, encoding, & Unicode tables for a font *
***********************************************************/

/* This is an externally-callable function. It looks for a mandatory AFM file,
containing character widths and kerning information, and also checks for an
optional UTR file if the font is not standardly-encoded.

Argument:   the font id (offset in font_list)
Returns:    nothing
*/

void
font_loadtables(uint32_t fontid)
{
FILE *fa, *fu;
int kerncount = 0;
int finalcount = 0;
kerntablestr *kerntable;
fontstr *fs = &(font_list[fontid]);
tree_node *treebase = NULL;
uschar *pp;
uschar filename[256];
uschar line[256];

/* Find and open the AFM file */

fa = font_finddata(fs->name, ".afm", font_data_extra, font_data_default,
  filename, TRUE);   /* Hard error if not found */

/* Initialize the font structure */

fs->widths = mem_get_independent(FONTWIDTHS_SIZE * sizeof(int32_t));
memset(fs->widths, 0xff, FONTWIDTHS_SIZE * sizeof(int32_t));

fs->r2ladjusts = mem_get_independent(FONTWIDTHS_SIZE * sizeof(int32_t));
memset(fs->r2ladjusts, 0, FONTWIDTHS_SIZE * sizeof(int32_t));

fs->used = mem_get_independent(FONTWIDTHS_SIZE/8);
memset(fs->used, 0, FONTWIDTHS_SIZE/8);

fs->heights = NULL;
fs->kerncount = 0;

/* These are used only by PDF output */

fs->firstcharL = 255;
fs->lastcharL = 0;
fs->firstcharU = 512;
fs->lastcharU = 256;

fs->ascent = 0;
fs->descent = 0;
fs->capheight = 0;
fs->italicangle = 0;
fs->stemv = 0;
memset(fs->bbox, 0, 4*sizeof(int32_t));

/* Find the start of the metrics in the AFM file; on the way, check for the
standard encoding scheme and for fixed pitch. */

for (;;)
  {
  if (Ufgets(line, sizeof(line), fa) == NULL)
    error(ERR58, filename, "no metric data found", "");  /* Hard */

  if (memcmp(line, "EncodingScheme AdobeStandardEncoding", 36) == 0)
    fs->flags |= ff_stdencoding;

  else if (memcmp(line, "IsFixedPitch true", 17) == 0)
    fs->flags |= ff_fixedpitch;

  else if (memcmp(line, "Ascender", 8) == 0)
    (void)read_number(&(fs->ascent), line + 8);

  else if (memcmp(line, "Descender", 9) == 0)
    (void)read_number(&(fs->descent), line + 9);

  else if (memcmp(line, "CapHeight", 9) == 0)
    (void)read_number(&(fs->capheight), line + 9);

  else if (memcmp(line, "ItalicAngle", 11) == 0)
    (void)read_number(&(fs->italicangle), line + 11);

   else if (memcmp(line, "StdVW", 5) == 0)
    (void)read_number(&(fs->stemv), line + 5);

  else if (memcmp(line, "FontBBox", 8) == 0)
    {
    uschar *p = line + 8;
    for (int i = 0; i < 4; i++)
      p = read_number(&(fs->bbox[i]), p);
    }

  else if (memcmp(line, "StartCharMetrics", 16) == 0) break;
  }

/* Now we know whether or not the font has standard encoding we can set up the
unsupported character substitution. */

if ((fs->flags & ff_stdencoding) == 0)
  fs->invalid = UNKNOWN_CHAR_N | (font_mf << 24);  /* Default */
else
  fs->invalid = UNKNOWN_CHAR_S | (font_unknown << 24);

/* Look for a .utr file, which may contain Unicode translations, choice of
unsupported character, and/or font encodings. */

fu = font_finddata(fs->name, ".utr", font_data_extra, font_data_default,
  filename, FALSE);

if (fu != NULL)
  {
  int ucount = 0;
  int lineno = 0;
  utrtablestr utable[MAX_UTRANSLATE];

  TRACE("Loading UTR for %s\n", fs->name);

  while (Ufgets(line, sizeof(line), fu) != NULL)
    {
    uschar *epp;

    lineno++;
    pp = line;
    while (isspace(*pp)) pp++;
    if (*pp == 0 || *pp == '#') continue;

    /* A line beginning with '/' is an encoding definition line. */

    if (*pp == '/')
      {
      tree_node *tn;
      uschar *nb, *ne;
      uint32_t c;

      if (fs->encoding == NULL)
        {
        fs->encoding = mem_get_independent(FONTWIDTHS_SIZE * sizeof(char *));
        for (int i = 0; i < FONTWIDTHS_SIZE; i++) fs->encoding[i] = NULL;
        }

      nb = ++pp;
      while (isalnum(*pp)) pp++;
      ne = pp;

      while (isspace(*pp)) pp++;
      c = (uint32_t)Ustrtoul(pp, &epp, 0);
      if (epp == pp)
        {
        error(ERR60, "font", lineno, filename, line);
        continue;
        }
      if (c >= FONTWIDTHS_SIZE)
        {
        error(ERR61, FONTWIDTHS_SIZE, lineno, filename, line);
        continue;
        }
      if (fs->encoding[c] != NULL)
        {
        error(ERR176, c, lineno, filename, line);
        continue;
        }

      /* We now have a name and a code. The name is saved and pointed to from
      the encoding vector for appropriate PostScript/PDF generation. We also
      create a tree of names so that the name and its code can be quickly found
      when reading the AFM file below. */

      *ne = 0;
      tn = mem_get(sizeof(tree_node));
      tn->name = fs->encoding[c] = mem_copystring(nb);
      tn->value = c;
      if (!tree_insert(&treebase, tn))
        error(ERR175, tn->name, lineno, filename, line);
      continue;
      }

    /* Handle an "unknown character" definition */

    if (*pp == '?')
      {
      uint32_t c = (uint32_t)Ustrtoul(++pp, &epp, 0);
      if (epp == pp)
        {
        error(ERR60, "font", lineno, filename, line);
        continue;
        }
      if (c >= FONTWIDTHS_SIZE)
        {
        error(ERR61, FONTWIDTHS_SIZE, lineno, filename, line);
        continue;
        }

      /* Change an underlay special character to its escaped version. */

      switch (c)
        {
        case '#': c = ss_escapedsharp; break;
        case '=': c = ss_escapedequals; break;
        case '-': c = ss_escapedhyphen; break;
        default: break;
        }

      fs->invalid = c | (font_unknown << 24);
      continue;
      }

    /* Handle a translation definition */

    if (ucount >= MAX_UTRANSLATE)
      {
      error(ERR59, filename, MAX_UTRANSLATE);
      break;
      }

    if (Ustrncmp(pp, "U+", 2) == 0) pp += 2;
    utable[ucount].unicode = (unsigned int)Ustrtoul(pp, &epp, 16);
    if (epp == pp)
      {
      error(ERR60, "Unicode", lineno, filename, line);
      continue;
      }
    pp = epp;
    utable[ucount].pscode = (unsigned int)Ustrtoul(pp, &epp, 0);
    if (epp == pp)
      {
      error(ERR60, "font", lineno, filename, line);
      continue;
      }
    if (utable[ucount].pscode >= FONTWIDTHS_SIZE)
      {
      error(ERR61, FONTWIDTHS_SIZE, lineno, filename, line);
      continue;
      }

    /* Change underlay special characters to escaped versions. */

    switch (utable[ucount].pscode)
      {
      case '#': utable[ucount].pscode = ss_escapedsharp; break;
      case '=': utable[ucount].pscode = ss_escapedequals; break;
      case '-': utable[ucount].pscode = ss_escapedhyphen; break;
      default: break;
      }

    ucount++;
    }

  (void)fclose(fu);

  /* Sort the data by Unicode value, check for duplicates, and remember with
  the font. */

  if (ucount > 0)
    {
    int i;
    qsort(utable, ucount, sizeof(utrtablestr), utr_table_cmp);
    for (i = 1; i < ucount; i++)
      {
      if (utable[i].unicode == utable[i-1].unicode)
        {
        error(ERR62, utable[i].unicode, filename);
        while(i < ucount - 1 && utable[i].unicode == utable[i+1].unicode) i++;
        }
      }
    fs->utr = mem_get_independent(ucount * sizeof(utrtablestr));
    memcpy(fs->utr, utable, ucount * sizeof(utrtablestr));
    fs->utrcount = ucount;
    }
  }

/* Now we can process the metric lines in the AFM file. */

TRACE("Loading AFM for %s\n", fs->name);

for (;;)
  {
  uschar *cname;
  uschar *ppb;
  int cnumber;
  int width, code;
  int poffset = -1;
  int r2ladjust = 0;
  BOOL widthset = FALSE;

  if (Ufgets(line, sizeof(line), fa) == NULL)
    error(ERR58, filename, "unexpected end of metric data", "");  /* Hard */
  if (memcmp(line, "EndCharMetrics", 14) == 0) break;

  if (memcmp(line, "C ", 2) != 0)
    error(ERR58, filename, "unrecognized metric data line: ", line); /* Hard */

  pp = read_number(&cnumber, line + 2);
  while (memcmp(pp, "WX", 2) != 0) pp++;
  pp = read_number(&width, pp+2);

  /* Look for a bounding box, but use a new pointer, because N comes first. If
  a bounding box is found, compute a value by which to adjust the printing
  position of this character when printing right-to-left. This is used for the
  last character of every string, instead of the stringwidth character.

        |          |--------|
        |          | char   |
        |          |  glyph |
   ^    |<-- x0 -->|--------|     x0 is the side bearing (LH bbox value)
   ^--->|<-- x1 ------------>     x1 is the right hand bbox value
   ^    ^
   ^    ^
   ^    Original print point
   ^
   New print point is x0 + x1 to the left of the old. If it were just x1,
   the edge of the character would abut the original point; instead we add
   an additional same sized bearing on the other side. */

  ppb = pp;
  while (*ppb != 0 && Ustrncmp(ppb, "B ", 2) != 0) ppb++;
  if (*ppb != 0)
    {
    int x0, x1;
    ppb = read_number(&x0, ppb+2);   /* x-left */
    ppb = read_number(&x1, ppb);     /* y-bottom */
    ppb = read_number(&x1, ppb);     /* x-right */
    r2ladjust = x1 + x0;
    }

  /* Get the character's name */

  while (memcmp(pp, "N ", 2) != 0) pp++;
  cname = (pp += 2);
  while (*pp != ' ') pp++;
  *pp = 0;

  /* If this is a StandardEncoding font, scan the list of characters so as to
  get the Unicode value for this character. If the code point is greater than
  or equal to LOWCHARLIMIT, the offset above LOWCHARLIMIT that is used for
  these characters is returned.  */

  if ((fs->flags & ff_stdencoding) != 0)
    {
    code = an2u(cname, &poffset);
    if (code >= 0)
      {
      /* Remember that this font has certain characters */

      if (code == CHAR_FI) fs->flags |= ff_hasfi;

      /* If the Unicode code point is not less than LOWCHARLIMIT, remember it
      and its special offset in a tree so that it can be translated when
      encountered in a string. Then set the translated value for saving the
      width. */

      if (code >= LOWCHARLIMIT)
        {
        tree_node *tc = mem_get(sizeof(tree_node));
        tc->name = mem_get(8);
        tc->name[misc_ord2utf8(code, tc->name)] = 0;
        tc->value = poffset;
        (void)tree_insert(&(fs->high_tree), tc);
        code = LOWCHARLIMIT + poffset;
        }

      /* Set the data for the standard code point */

      fs->widths[code] = width;
      fs->r2ladjusts[code] = r2ladjust;
      widthset = TRUE;
      }
    }

  /* If this is not a standardly encoded font, and not the Music
  font, we need to ensure that there is an encoding vector for use in PDF
  output. One may have been created by reading a .utr file above - if so, just
  fill in any missing characters we find. */

  else if (PDF && Ustrcmp(fs->name, "PMW-Music") != 0)
    {
    if (fs->encoding == NULL)
      {
      fs->encoding = mem_get_independent(FONTWIDTHS_SIZE * sizeof(char *));
      for (int i = 0; i < FONTWIDTHS_SIZE; i++) fs->encoding[i] = NULL;
      }

    /* Some AFM files have character numbers greater than 511. */

    if (cnumber >= 0 && cnumber < 512 && fs->encoding[cnumber] == NULL)
      fs->encoding[cnumber] = mem_copystring(cname);
    }

  /* If the font has a .utr file, it will have been read above, and if there is
  encoding information in it, a tree of character names will exist that
  translates from glyph name to encoding value. This might override the
  standard encoding, or it might duplicate a character on another code point.
  Ignore unknown names. */

  if (treebase != NULL)
    {
    tree_node *tn;
    tn = tree_search(treebase, cname);
    if (tn != NULL)
      {
      code = tn->value;
      fs->widths[code] = width;
      fs->r2ladjusts[code] = r2ladjust;
      widthset = TRUE;
      }
    }

  /* If we haven't managed to find a code point from the name, use the
  character number directly, unless the width has already been set via a
  different name. If there are unencoded characters, or characters with code
  points greater than the widths table, ignore them. These fonts include the
  PMW-Music font, which has some characters with vertical height movements. */

  if (!widthset)
    {
    (void)read_number(&code, line+1);
    if (code < 0 || code >= FONTWIDTHS_SIZE || fs->widths[code] >= 0) continue;
    for (pp = line + 2; *pp != 0 && memcmp(pp, "WY", 2) != 0; pp++) {}
    if (*pp != 0)
      {
      int32_t height;
      pp += 2;
      (void)read_number(&height, pp);
      if (fs->heights == NULL)
        {
        fs->heights = mem_get_independent(256 * sizeof(int32_t));
        memset(fs->heights, 0, 256 * sizeof(int));
        }
      fs->heights[code] = height;
      }
    fs->widths[code] = width;
    fs->r2ladjusts[code] = r2ladjust;
    }
  }   /* End of loop for processing character lines */

/* If this is a standardly encoded font, character 173 (U+00AD) is a "soft
hyphen". This is not a standard character, so our encoding uses a normal
hyphen. However, it's width won't be set, so we have to fudge it here. */

if ((fs->flags & ff_stdencoding) != 0 && fs->widths[173] < 0)
  fs->widths[173] = fs->widths[45];

/* Process kerning data (if any); when this is done, we are finished with the
AFM file. */

for (;;)
  {
  if (Ufgets(line, sizeof(line), fa) == NULL) goto ENDKERN;
  if (memcmp(line, "StartKernPairs", 14) == 0) break;
  }

/* Find size of kern table, and get space for it. In the past, some of Adobe's
AFM files had a habit of containing a large number of kern pairs with
zero amount of kern. We leave these out of the table and adjust the count for
searching, but don't bother to free up the unused memory (it isn't a vast
amount). */

pp = line + 14;
while (*pp != 0 && *pp == ' ') pp++;
(void)read_number(&kerncount, pp);
fs->kerns = kerntable = mem_get_independent(kerncount*sizeof(kerntablestr));

finalcount = 0;
while (kerncount--)
  {
  uschar *x;
  int sign = 1;
  int value;
  int a = -1;
  int b = -1;

  if (Ufgets(line, sizeof(line), fa) == NULL)
    error(ERR58, filename, "unexpected end of kerning data");  /* Hard */
  if (memcmp(line, "EndKernPairs", 12) == 0) break;

  /* Skip blank lines */

  if (Ustrlen(line) <= 1)
    {
    kerncount++;
    continue;
    }

  /* Process each kern */

  pp = line + 4;

  x = pp;
  while (*pp != 0 && *pp != ' ') pp++;
  *pp++ = 0;
  a = an2u(x, NULL);

  while (*pp != 0 && *pp == ' ') pp++;
  x = pp;
  while (*pp != 0 && *pp != ' ') pp++;
  *pp++ = 0;
  b = an2u(x, NULL);

  /* Read the kern value only if we have found the characters; otherwise ignore
  the kern */

  if (a >= 0 && b >= 0)
    {
    a = check_lowchar(a, fs);
    b = check_lowchar(b, fs);

    kerntable[finalcount].pair = (a << 16) + b;
    while (*pp != 0 && *pp == ' ') pp++;
    if (*pp == '-') { sign = -1; pp++; }
    (void)read_number(&value, pp);
    if (value != 0) kerntable[finalcount++].kwidth = value*sign;
    }
  }

/* Adjust the count and sort the table into ascending order */

fs->kerncount = finalcount;  /* true count */
qsort(kerntable, fs->kerncount, sizeof(kerntablestr), kern_table_cmp);

/* Finished with the AFM file */

ENDKERN:
(void)fclose(fa);

/* Early checking debugging code; retained in the source in case it is ever
needed again. */

#ifdef NEVER
debug_printf("FONT %s\n", fs->name);
  {
  int i;

  for (i = 0; i < LOWCHARLIMIT; i++)
    {
    if (fs->widths[i] != 0) debug_printf("%04x %5d\n", i, fs->widths[i]);
    }

  for (i = LOWCHARLIMIT; i < 0xffff; i++)
    {
    tree_node *t;
    uschar key[4];
    key[0] = (i >> 8) & 255;
    key[1] = i & 255;
    key[2] = 0;
    t = tree_search(fs->high_tree, key);
    if (t != NULL) debug_printf("%04x %5d %5d %5d\n", i, t->data.val[0],
      t->data.val[1], t->data.val[2]);
    }

  debug_printf("KERNS %d\n", fs->kerncount);
  for (i = 0; i < fs->kerncount; i++)
    {
    kerntablestr *k = &(fs->kerns[i]);
    int a = (k->pair >> 16) & 0xffff;
    int b = (k->pair) & 0xffff;
    debug_printf("%04x %04x %5d\n", a, b, k->kwidth);
    }
  }
#endif
}



/*************************************************
*         Add a font to the base list            *
*************************************************/

/* This function is called from font_init() below, and also by the textfont and
musicfont commands, to add a typeface to the list of fonts.

Arguments:
  name     name of font
  fontid   offset in font_table (font_rm etc)
  flags    font flags

Returns:   nothing
*/

void
font_addfont(uschar *name, uint32_t fontid, uint32_t flags)
{
fontstr *fs;

if (font_count >= font_list_size)
  {
  font_list_size += FONTLIST_CHUNKSIZE;
  font_list = realloc(font_list, font_list_size * sizeof(fontstr));
  if (font_list == NULL) error(ERR0, "re-", "font list vector",
    font_list_size);  /* Hard */
  }

fs = &(font_list[font_count]);
fs->name = mem_copystring(name);
fs->widths = NULL;
fs->high_tree = NULL;
fs->utr = NULL;
fs->encoding = NULL;
fs->utrcount = 0;
fs->heights = NULL;
fs->kerns = NULL;
fs->kerncount = -1;
fs->flags = flags;

font_table[fontid] = font_count;
font_loadtables(font_count++);
}


/*************************************************
*                Read a font type                *
*************************************************/

/*
Argument:  TRUE to not give an error if the word is unrecognised
Returns:   font_rm, font_it, etc, or font_unknown if not recognized
*/

uint32_t
font_readtype(BOOL soft)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "roman") == 0) return font_rm;
if (Ustrcmp(read_wordbuffer, "italic") == 0) return font_it;
if (Ustrcmp(read_wordbuffer, "bold") == 0) return font_bf;
if (Ustrcmp(read_wordbuffer, "bolditalic") == 0) return font_bi;
if (Ustrcmp(read_wordbuffer, "symbol") == 0) return font_sy;

if (Ustrcmp(read_wordbuffer, "extra") == 0)
  {
  int32_t x;
  if (!read_expect_integer(&x, FALSE, FALSE)) return font_unknown;
  if (x >= 1 && x <= MaxExtraFont) return font_xx + x - 1;
  error(ERR37, MaxExtraFont);
  }
else if (!soft)
  error(ERR8, "\"roman\", \"italic\", \"bold\", \"bolditalic\", \"symbol\", "
    "or \"extra\"");
return font_unknown;
}



/*************************************************
*              Search for given font             *
*************************************************/

/* This function searches the font list by the name of the font.

Argument:  the font name (e.g. "Times-Roman")
Returns:   the font id (offset in font_list) or font_unknown if not found
*/

uint32_t
font_search(uschar *name)
{
uint32_t i;
for (i = 0; i < font_count; i++)
  if (Ustrcmp(name, font_list[i].name) == 0) return i;
return font_unknown;
}



/*************************************************
*      Manually translate Unicode code point     *
*************************************************/

/* If there is no translation table, or if the character is absent from the
table, return 0xffffffff.

Arguments:
  c           the Unicode code point
  fs          the font structure

Returns:      possibly a different code point
*/

uint32_t
font_utranslate(uint32_t c, fontstr *fs)
{
int bot = 0;
int top = fs->utrcount;

while (bot < top)
  {
  int mid = (bot + top)/2;
  if (c == fs->utr[mid].unicode) return fs->utr[mid].pscode;
  if (c > fs->utr[mid].unicode) bot = mid + 1;
    else top = mid;
  }

return 0xffffffffu;   /* Not found */
}



/*************************************************
*         Find the width of a character          *
*************************************************/

/* The "height" (distance current point moves vertically) is also returned. For
ordinary text fonts, this is only non-zero when there's a rotation, but some
characters in the music font move vertically. Such heights apply only to
low-numbered characters in certain fonts.

All code points in PMW strings are checked for validity at read-in time and
translated if necessary. By this stage, except for the special non-Unicode
values, the code points will always be less than 256 for non-standardly-encoded
fonts, and less than LOWCHARLIMIT plus a few extras for standardly-encoded
fonts. Either way, the values are less than the size of the widths vector.

This function might be called with a non-Unicode special character if an escape
such as \p\ or \so\ is used in the first part of a heading. A zero width is
returned because this is documented not to work when a long heading is being
split into multiple lines.

Arguments:
  c            the character
  lastc        the previous character or 0 (for kerning)
  font         the font number
  pointsize    the font size
  hptr         where to return the height or NULL

Returns:       the string width
*/

int32_t
font_charwidth(uint32_t c, uint32_t lastc, uint32_t font, int32_t pointsize,
  int32_t *hptr)
{
fontstr *fs;
kerntablestr *ktable;
int32_t yield;

DEBUG(D_font) eprintf("font_charwidth %d %s \'%c\'\n", font, sff(pointsize), c);

/* Adjust the font size for small caps and reduced music font. */

if (font >= font_small)
  {
  font -= font_small;
  if (font == font_mf) pointsize = (pointsize * 9) / 10;
    else pointsize = (pointsize * curmovt->smallcapsize) / 1000;
  }

/* Get the basic width from the font information. */

fs = &(font_list[font_table[font]]);

if (c <= MAX_UNICODE)
  {
  yield = fs->widths[c];
  if (hptr != NULL) *hptr = (fs->heights != NULL && c < 256)?
    mac_muldiv(fs->heights[c], pointsize, 1000) : 0;
  }

else
  {
  yield = 0;
  if (hptr != NULL) *hptr = 0;
  }

/* Deal with kerning. At the moment, this is supported only for characters
whose code points are less than 0xffff. It should be easy to extend by
converting to 64-bit combinations. */

ktable = fs->kerns;
if (main_kerning && ktable != NULL &&
    lastc > 0 && lastc <= 0xffff && c <= 0xffff)
  {
  uint32_t pair = (lastc << 16) | c;
  int top = fs->kerncount;
  int bot = 0;

  while (top > bot)
    {
    int mid = (top + bot)/2;
    kerntablestr *k = &(ktable[mid]);
    if (pair == k->pair)
      {
      yield += k->kwidth;
      break;
      }
    if (pair > k->pair) bot = mid + 1; else top = mid;
    }
  }

return mac_muldiv(yield, pointsize, 1000);
}



/*************************************************
*              Rotate a font matrix              *
*************************************************/

/* This function is called when a text string is to be rotated when output. It
returns a pointer to a static copy of the input font instance block, with a
suitably rotated matrix setting.

Arguments:
  fdata       pointer to a font instance block
  angle       rotation angle in degrees

Returns:      pointer to a copy of fdata with rotation set
*/

static fontinststr copyfdata;
static int32_t copymatrix[6];
static int32_t nullmatrix[6] = { 65536, 0, 0, 65536, 0, 1000 };

fontinststr *
font_rotate(fontinststr *fdata, int32_t angle)
{
int32_t newmatrix[4];
double sr = sin(((double)angle)*atan(1.0)/45000.0);
double cr = cos(((double)angle)*atan(1.0)/45000.0);

copyfdata = *fdata;
copyfdata.matrix = copymatrix;

memcpy(copymatrix, (fdata->matrix == NULL)? nullmatrix : fdata->matrix,
  4 * sizeof(int32_t));

newmatrix[0] =
  (int)(((double)copymatrix[0])*cr - ((double)copymatrix[1])*sr);
newmatrix[1] =
  (int)(((double)copymatrix[0])*sr + ((double)copymatrix[1])*cr);
newmatrix[2] =
  (int)(((double)copymatrix[2])*cr - ((double)copymatrix[3])*sr);
newmatrix[3] =
  (int)(((double)copymatrix[2])*sr + ((double)copymatrix[3])*cr);

memcpy(copymatrix, newmatrix, 4*sizeof(int32_t));
copymatrix[4] = (int32_t)(sr * 1000.0);
copymatrix[5] = (int32_t)(cr * 1000.0);
return &copyfdata;
}


/* End of font.c */
