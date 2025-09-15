/*************************************************
*              PMW font functions                *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: January 2021 */
/* This file last modified: September 2025 */

#include "pmw.h"


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

/* This function looks for a mandatory AFM file, containing character widths
and kerning information, and also checks for an optional UTR file.

Argument:   the font id (offset in font_list)
Returns:    nothing
*/

static void
font_initialize(uint32_t fontid)
{
FILE *fa, *fu;
int kerncount = 0;
int finalcount = 0;
kerntablestr *kerntable;
fontstr *fs = &(font_list[fontid]);
tree_node *treebase = NULL;
uschar *pp;
uschar filename[256];
uschar utrfilename[256];
uschar line[1024];       /* Some AFM files have very long lines */

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
fs->kerns = NULL;
fs->kerncount = 0;
fs->utr = NULL;
fs->utrcount = 0;
fs->encoding = NULL;
fs->high_tree = NULL;

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
  utrfilename, FALSE);

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
        error(ERR60, "font", lineno, utrfilename, line);
        continue;
        }
      if (c >= FONTWIDTHS_SIZE)
        {
        error(ERR61, FONTWIDTHS_SIZE, lineno, utrfilename, line);
        continue;
        }
      if (fs->encoding[c] != NULL)
        {
        error(ERR176, c, lineno, utrfilename, line);
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
        error(ERR175, tn->name, lineno, utrfilename, line);
      continue;
      }

    /* Handle an "unknown character" definition */

    if (*pp == '?')
      {
      uint32_t c = (uint32_t)Ustrtoul(++pp, &epp, 0);
      if (epp == pp)
        {
        error(ERR60, "font", lineno, utrfilename, line);
        continue;
        }
      if (c >= FONTWIDTHS_SIZE)
        {
        error(ERR61, FONTWIDTHS_SIZE, lineno, utrfilename, line);
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
      error(ERR59, utrfilename, MAX_UTRANSLATE);
      break;
      }

    if (Ustrncmp(pp, "U+", 2) == 0) pp += 2;
    utable[ucount].unicode = (unsigned int)Ustrtoul(pp, &epp, 16);
    if (epp == pp)
      {
      error(ERR60, "Unicode", lineno, utrfilename, line);
      continue;
      }
    pp = epp;
    utable[ucount].pscode = (unsigned int)Ustrtoul(pp, &epp, 0);
    if (epp == pp)
      {
      error(ERR60, "font", lineno, utrfilename, line);
      continue;
      }
    if (utable[ucount].pscode >= FONTWIDTHS_SIZE)
      {
      error(ERR61, FONTWIDTHS_SIZE, lineno, utrfilename, line);
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
        error(ERR62, utable[i].unicode, utrfilename);
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
   an additional same sized bearing on the other side.

   The bounding box is introduced by "B", but just looking for "B " (as was
   originally coded) is not enough because a character name may end with "B "
   (as was eventually realized), and indeed the character's name might just be
   "B". */

  ppb = pp;

  for (;;)
    {
    while (*ppb != 0 && Ustrncmp(ppb, "B ", 2) != 0) ppb++;
    if (*ppb == 0) break;
    ppb += 2;
    while (*ppb != 0 && *ppb == ' ') ppb++;
    if (*ppb == 0 || isdigit((int)(*ppb)) || *ppb == '-') break;
    }

  if (*ppb != 0)
    {
    int x0, x1;
    ppb = read_number(&x0, ppb);   /* x-left */
    ppb = read_number(&x1, ppb);   /* y-bottom */
    ppb = read_number(&x1, ppb);   /* x-right */
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

  /* If this is not a standardly encoded font, we need to ensure that there is
  an encoding vector for use in PDF output. One may have been created by
  reading a .utr file above - if so, just fill in any missing characters we
  find. */

  else if (PDF)
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
fs->flags = flags;
font_table[fontid] = font_count;
font_initialize(font_count++);
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
