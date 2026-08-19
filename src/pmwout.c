/*************************************************
*            PMW source output generation        *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: April 2026 */
/* This file last modified: August 2026 */

#include "pmw.h"

#define PENDULAY_MAX 20

static FILE      *pmw_file;
static int        outcount = 0;
static uint32_t   barlength = 0;
static BOOL       disable_outcount = FALSE;

static const char *acnames[] = { "", "%", "#-", "#", "##", "$-", "$", "$$" };
static int accspacingorder[] = { ac_ds, ac_fl, ac_df, ac_nt, ac_sh, ac_no };

static const char *fonttype_names[] = {
  "roman", "italic", "bold", "bolditalic", "symbol", "music"};

const char *font_defaults[]= { "Times-Roman", "Times-Italic", "Times-Bold",
  "Times-BoldItalic", "Symbol" };

/* This vector is used for a reverse Unicode translation table, which is
constructed the first time it is needed. It does not need to be reset for a new
movement. */

static uint32_t   unihigh[50] = { 0 };

/* Types for tables of fields in the movement structure. */

typedef struct movtfield {
  const char *name;
  size_t offset;
} movtfield;

typedef struct movtflag {
  const char *name;
  uint32_t flag;
} movtflag;

/* Table of straightforward signed 32-bit dimension values in the movement
structure and the directives that are used to set them. */

static movtfield movt32dimfields[] = {
  { "barlinesize",       offsetof(movtstr, barlinesize) },
  { "dotspacefactor",    offsetof(movtstr, dotspacefactor) },
  { "endlinesluradjust", offsetof(movtstr, endlinesluradjust) },
  { "endlinetieadjust",  offsetof(movtstr, endlinetieadjust) },
  { "extenderlevel",     offsetof(movtstr, extenderlevel) },
  { "footnotesep",       offsetof(movtstr, footnotesep) },
  { "hyphenthreshold",   offsetof(movtstr, hyphenthreshold) },
  { "leftmargin",        offsetof(movtstr, leftmargin) },
  { "linelength",        offsetof(movtstr, truelinelength) },
  { "midkeyspacing",     offsetof(movtstr, midkeyspacing) },
  { "midtimespacing",    offsetof(movtstr, midtimespacing) },
  { "overlaydepth",      offsetof(movtstr, overlaydepth) },
  { "shortenstems",      offsetof(movtstr, shortenstems) },
  { "underlaydepth",     offsetof(movtstr, underlaydepth) }
};

#define movt32dimfieldscount (sizeof(movt32dimfields)/sizeof(movtfield))

/* Table of unsigned 32-bit dimension values in the movement structure and the
directives that are used to set them. */

static movtfield movtu32dimfields[] = {
  { "beamflaglength",    offsetof(movtstr, beamflaglength) },
  { "beamthickness",     offsetof(movtstr, beamthickness) },
  { "bottommargin",      offsetof(movtstr, bottommargin) },
  { "breveledgerextra",  offsetof(movtstr, breveledgerextra) },
  { "hairpinlinewidth",  offsetof(movtstr, hairpinlinewidth) },
  { "hairpinwidth",      offsetof(movtstr, hairpinwidth) },
  { "smallcapsize",      offsetof(movtstr, smallcapsize) },
  { "systemgap",         offsetof(movtstr, systemgap) },
  { "topmargin",         offsetof(movtstr, topmargin) },
  { "tupletlinewidth",   offsetof(movtstr, tripletlinewidth) }
};

#define movtu32dimfieldscount (sizeof(movtu32dimfields)/sizeof(movtfield))

/* Table of signed 32-bit numerical values in the movement structure and the
directives that are used to set them. */

static movtfield movt32numfields[] = {
  { "startbracketbar",   offsetof(movtstr, startbracketbar) }
};

#define movt32numfieldscount (sizeof(movt32numfields)/sizeof(movtfield))

#ifdef NEVER
/* Table of unsigned 32-bit numerical values in the movement structure and the
directives that are used to set them. */

static movtfield movtu32numfields[] = {

};

#define movtu32numfieldscount (sizeof(movtu32numfields)/sizeof(movtfield))
#endif

/* Table of unsigned 8-bit numerical values in the movement structure and the
directives that are used to set them. */

static movtfield movtu8numfields[] = {
  { "barlinestyle",      offsetof(movtstr, barlinestyle) },
  { "bracestyle",        offsetof(movtstr, bracestyle) },
  { "caesurastyle",      offsetof(movtstr, caesurastyle) },
  { "clefstyle",         offsetof(movtstr, clefstyle) },
  { "endlineslurstyle",  offsetof(movtstr, endlineslurstyle) },
  { "endlinetiestyle",   offsetof(movtstr, endlinetiestyle) },
  { "gracestyle",        offsetof(movtstr, gracestyle) },
  { "halfflatstyle",     offsetof(movtstr, halfflatstyle) },
  { "halfsharpstyle",    offsetof(movtstr, halfsharpstyle) },
  { "ledgerstyle",       offsetof(movtstr, ledgerstyle) },
  { "repeatstyle",       offsetof(movtstr, repeatstyle) },
  { "underlaystyle",     offsetof(movtstr, underlaystyle) }
};

#define movtu8numfieldscount (sizeof(movtu8numfields)/sizeof(movtfield))


/* Table of movement flag bits and the names of directives to set them when
there is a matching "no..." directive. */

static movtflag movtflags[] = {
  { "beamendrests",      mf_beamendrests },
  { "beamrests",         mf_beamrests },
  { "breverests",        mf_breverests },
  { "codemultirests",    mf_codemultirests },
  { "keywarn",           mf_keywarn },
  { "sluroverwarnings",  mf_tiesoverwarnings },
  { "spreadunderlay",    mf_spreadunderlay },
  { "timebase",          mf_showtimebase },
  { "timewarn",          mf_timewarn },
  { "underlayextenders", mf_underlayextenders },
};

#define movtflagscount (sizeof(movtflags)/sizeof(movtflag))



/*************************************************
*              Write to output file              *
*************************************************/

/* This saves typing, but also checks the output is successful. */

static void
P(const char *format, ...)
{
int rc;
va_list ap;
va_start(ap, format);

if (!disable_outcount && outcount > 70 && format[0] == ' ')
  {
  fprintf(pmw_file, "\n ");
  outcount = 0;
  }
rc = vfprintf(pmw_file, format, ap);
va_end(ap);
if (rc < 0) error(ERR201, "vfprintf", strerror(errno));  /* Hard */
outcount += rc;
if (!disable_outcount) switch (format[strlen(format) - 1])
  {
  case ' ':
  if (outcount <= 70) break;
  fprintf(pmw_file, "\n  ");
  /* Fall through */
  case '\n':
  outcount = 0;
  break;
  }
}



/*************************************************
*              Format bar number                 *
*************************************************/

static void
format_barnumber(uint32_t bar, char *buffer)
{
uint32_t n = bar >> 16;
uint32_t f = bar & 0xffffu;
char *p = buffer;
p += sprintf(p, "%d", n);
if (f != 0) (void)sprintf(p, ".%d", f);
}


/*************************************************
*              Write bar number                  *
*************************************************/

static void
write_barnumber(uint32_t bar, const char *lead)
{
char buffer[32];
format_barnumber(bar, buffer);
P("%s%s", lead, buffer);
}


/*************************************************
*              Write font name                   *
*************************************************/

static void
write_fontname(int type, const char *lead)
{
if (type < font_xx) P("%s%s", lead, fonttype_names[type]);
  else P("%sextra %d", lead, type - font_xx + 1);
}



/*************************************************
*             Write time signature               *
*************************************************/

/* This also computes a bar length. */

static void
write_time(uint32_t mts)
{
int mul = mts >> 16;
P("time");
if (mul > 1) P(" %d*", mul);
mts &= 0x0000ffffu;
if (mts == time_common) 
  {
  P(" C");
  barlength = mul * len_semibreve; 
  } 
else if (mts == time_cut) 
  {
  P(" A");
  barlength = mul * len_semibreve; 
  } 
else 
  {
  int num = mts >> 8;
  int den = mts & 0xff; 
  P(" %d/%d", num, den);
  barlength = mul * ((num * 4 * len_crotchet)/den);
  } 
}



/*************************************************
*        Format xy movement given options        *
*************************************************/

static char *
format_move_opt(int32_t x, int32_t y, const char *right, const char *left,
const char *up, const char *down, char *cp)
{
if (x > 0) cp += sprintf(cp, "%s%s", right, sff(x));
  else if (x < 0) cp += sprintf(cp, "%s%s", left, sff(-x));
if (y > 0) cp += sprintf(cp, "%s%s", up, sff(y));
  else if (y < 0) cp += sprintf(cp, "%s%s", down, sff(-y));
return cp;
}



/*************************************************
*               Format xy movement               *
*************************************************/

static char *
format_move(int32_t x, int32_t y, char *cp)
{
return format_move_opt(x, y, "/r", "/l", "/u", "/d", cp);
}



/*************************************************
*       Output xy movement given options         *
*************************************************/

static void
write_move_opt(int32_t x, int32_t y, const char *right, const char *left,
  const char *up, const char *down)
{
char buffer[64];
buffer[0] = 0;
format_move_opt(x, y, right, left, up, down, buffer);
if (buffer[0] != 0) P("%s", buffer);
}



/*************************************************
*            Output xy movement                  *
*************************************************/

static void
write_move(int32_t x, int32_t y)
{
char buffer[64];
buffer[0] = 0;
format_move(x, y, buffer);
if (buffer[0] != 0) P("%s", buffer);
}



/*************************************************
*            Compare PMW strings                 *
*************************************************/

/* A PMW string is a sequence of 32-bit values, with the top byte containing a
font type, terminated by a zero. */

static BOOL
compare_pmw_strings(uint32_t *a, uint32_t *b)
{
for (;;)
  {
  if (*a != *b) return FALSE;
  if (*a == 0 && *b == 0) return TRUE;
  if (*a == 0 || *b == 0) return FALSE;
  a++;
  b++;
  }
return FALSE;  /* Though control should never get here. */
}



/*************************************************
*         Write PMW string contents              *
*************************************************/

/* The code for characters above LOWCHARLIMIT is much the same as in xmlout.c,
but I'm repeating it to avoid dependency between the two forms of output. */

static uint32_t
write_pmw_string_contents(uint32_t *s, uint32_t settype)
{
for (; *s != 0; s++)
  {
  uschar buffer[8];
  uint32_t type = PFONT(*s);
  uint32_t c = PCHAR(*s);

  if (type != settype)
    {
    settype = type;
    if ((type & font_small) != 0)
      {
      type &= ~font_small;
      P("\\%s\\", (type == font_mf)? "mu" : "sc");
      }
    if (settype != (font_mf | font_small)) P("\\%s\\", font_IdStrings[type]);
    }

  /* Handle '&' specially */

  if (c == '&')
    {
    P("&&");
    continue;
    }

  /* Handle special characters above the Unicode limit. */

  if (c > MAX_UNICODE)
    {
    switch(c)
      {
      case ss_verticalbar:   c = '|'; break;
      case ss_asciiquote:    c = '\''; break;
      case ss_asciigrave:    c = '`'; break;

      case ss_escapedhyphen: c = '-'; P("\\-"); continue;
      case ss_escapedequals: c = '='; P("\\="); continue;
      case ss_escapedsharp:  c = '#'; P("\\#"); continue;

      case ss_page:     P("\\p\\");  continue;
      case ss_pageodd:  P("\\po\\"); continue;
      case ss_pageeven: P("\\pe\\"); continue;
      case ss_skipodd:  P("\\so\\"); continue;
      case ss_skipeven: P("\\se\\"); continue;

      default:
      error(ERR203, c);
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

  /* A few other characters need special handling. */

  else switch(c)
    {
    case '|': P("\\|"); continue;   /* Escaped vertical bar */
    case '"': P("\\\""); continue;  /* Double quote */
    default: break;                 /* Carry on */
    }

  /* Output a single UTF-8 character. */

  int k = misc_ord2utf8(c, buffer);
  for (int j = 0; j < k; j++) P("%c", buffer[j]);
  }

return settype;
}



/*************************************************
*             Write PMW string                   *
*************************************************/

/*
Arguments:
  s          the string
  lead       leading text
  settype    font type to assume at start (negative if unknown)

Returns:     nothing
*/

static void
write_pmw_string(uint32_t *s, const char *lead, uint32_t settype)
{
P("%s\"", lead);
(void)write_pmw_string_contents(s, settype);
P("\"");
}



/*************************************************
*           Write font size etc.                 *
*************************************************/

/* Note that string_format_fixed() aka sff() allows up to 5 simultaneous
outputs to exist. The reconstituted stretch and shear often end up as x.y99
when the original had just one decimal place, so we round them up. */

static void
write_fontsize(fontinststr *f, const char *lead, int divideby)
{
P("%s %s", lead, sff(f->size/divideby));
if (f->matrix != NULL)
  {
  int32_t stretch = mac_muldiv(f->matrix[0], 1000, 65536);
  int32_t shear = (int32_t)(
    atan((double)(f->matrix[2])/65536.0)/atan(1.0)*45000);
  if ((stretch + 1) % 100 == 0) stretch++;
  if ((shear + 1) % 100 == 0) shear++;
  P("/%s/%s", sff(stretch), sff(shear));
  }
}



/*************************************************
*             Comparison of fonts                *
*************************************************/

/* Check if two font specifications match. WHen a type comparison is unnecesary
t1 & t1 can just be any identical value.

Arguments:
  check_matrix   TRUE to include matrix checking
  m & p          two font sizes to compare
  t1 & t2        two font types to compare

Returns:         TRUE or FALSE
*/

static BOOL
compare_fonts(BOOL check_matrix, fontinststr *m, fontinststr *p, int t1, int t2)
{
BOOL same = m->size == p->size && t1 == t2;
if (same && check_matrix && (m->matrix != NULL || p->matrix != NULL))
  {
  if (m->matrix == NULL || p->matrix == NULL) same = FALSE;
    else for (int i = 0; i < 6; i++)
      if (m->matrix[i] != p->matrix[i])
        { same = FALSE; break; }
  }
return same;
}



/*************************************************
*    Compare fonts; output first if different    *
*************************************************/

/* If no type comparison is needed, t1 and t2 can be set negative.

Arguments:
  check_matrix   TRUE to include matrix checking
  directive      which directive to output
  divideby       scaling for output
  m & p          two font sizes to compare
  t1 & t2        two font types to compare

Returns:         nothing
*/

static void
write_diff_font(BOOL check_matrix, const char *directive, int divideby,
  fontinststr *m, fontinststr *p, int t1, int t2)
{
if (!compare_fonts(check_matrix, m, p, t1, t2))
  {
  write_fontsize(m, directive, divideby);
  if (t1 >= 0) write_fontname(t1, " ");
  P("\n");
  }
}



/*************************************************
*               Write stave list                 *
*************************************************/

static void
write_stavelist(const char *s, stavelist *sl)
{
P("%s", s);
while (sl != NULL)
  {
  P(" %d", sl->first);
  if (sl->last != sl->first) P("-%d", sl->last);
  sl = sl->next;
  }
P("\n");
}



/*************************************************
*            Write stave bit map                 *
*************************************************/

static void
write_stavebits(uint64_t stavebits, const char *lead)
{
uint64_t bit = 1;
P("%s", lead);
for (int i= 0; i < 64; i++)
  {
  if ((stavebits & bit) != 0) P(" %d", i);
  bit <<= 1;
  }
P("\n");
}



/*************************************************
*             Write headings/footings            *
*************************************************/

static void
write_headfoot(headstr *p, const char *directive)
{
for (; p != NULL; p = p->next)
  {
  if (p->drawing == NULL)  /* Text heading */
    {
    uint32_t settype = font_rm;
    P("%s", directive);
    write_fontsize(&(p->fdata), "", 1);
    P(" \"");
    for (int i = 0; i < 3; i++)
      {
      if (p->string[i] != NULL)
        settype = write_pmw_string_contents(p->string[i], settype);
      if (i != 2) P("|");
      }
    P("\"");
    if (p->space != p->fdata.size) P(" %s", sff(p->space));
    P("\n");
    }
  else                  /* Drawing heading */
    {
    fprintf(stderr, "** Draw heading/footing not yet supported\n");
    }
  }
}



/*************************************************
*               Write PMW source file            *
*************************************************/

/* This is the main external entry to this set of functions. The data is all in
memory and global variables. Writing a PMW source file is triggered by the use
of the -pmw command line option, which sets outpmw_filename non-NULL.

Arguments:  none
Returns:    nothing
*/

void
outpmw_write(void)
{
time_t now;
int max_stave_used = 0;
char datebuff[100];

TRACE("outpmw_write()\n");

pmw_file = Ufopen(outpmw_filename, "w");
if (pmw_file == NULL) error(ERR23, outpmw_filename, strerror(errno));  /* Hard */
if (main_verify) eprintf("Writing PMW source file \"%s\"\n", outpmw_filename);

if (main_testing == 0)
  {
  now = time(NULL);
  strftime(datebuff, sizeof(datebuff), "%Y-%m-%d", localtime(&now));
  P("@ Generated by PMW %s %s\n\n", PMW_VERSION, datebuff);
  }
else P("@ Generated by PMW\n\n");

/* Always set no check because there's no way of identifying it from individual
bars. */

// P("nocheck\n");
        
/* Global settings that are allowed only in the first movement. */

if (main_magnification != 1000)
  P("magnification %s\n", sff(main_magnification));
if (main_righttoleft) P("righttoleft\n");

/* Allow for the adjustments made by landscape */

if (main_landscape)
  {
  P("landscape\n");
  if (main_sheetdepth != DEFAULT_SHEETWIDTH)
    P("sheetdepth %s\n", sff(main_sheetdepth));
  if (main_sheetwidth != DEFAULT_SHEETDEPTH)
    P("sheetwidth %s\n", sff(main_sheetwidth));
  if (main_pagelength != DEFAULT_PAGELENGTH)
    P("pagelength %s\n", sff(main_truepagelength));
  }
else
  {
  if (main_sheetdepth != DEFAULT_SHEETDEPTH)
    P("sheetdepth %s\n", sff(main_sheetdepth));
  if (main_sheetwidth != DEFAULT_SHEETWIDTH)
    P("sheetwidth %s\n", sff(main_sheetwidth));
  if (main_pagelength != DEFAULT_PAGELENGTH)
    P("pagelength %s\n", sff(main_truepagelength));
  }

if (bar_use_draw) P("drawbarlines\n");
if (stave_use_draw != 0) P("drawstavelines %d\n", stave_use_draw);
if (print_incPMWfont) P("includePMWfont\n");
if (main_maxvertjustify != DEFAULT_MAXVERTJUSTIFY)
  P("maxvertjustify %s\n", sff(main_maxvertjustify));
if (main_midifornotesoff) P("midifornotesoff\n");
if (!main_kerning) P("nokerning\n");
if (!stave_use_widechars) P("nowidechars\n");
if (page_firstnumber != 1 || page_increment != 1)
  P("page %d %d\n", page_firstnumber, page_increment);

/* Write non-default font names. First check the standard roman, italic, bold,
bolditalic, and symbol fonts. */

for (int i = 0; i < 5; i++)
  {
  fontstr *fs = font_list + font_table[i];
  if (Ustrcmp(fs->name, font_defaults[i]) != 0)
    P("textfont %s%s \"%s\"\n", fonttype_names[i],
      ((fs->flags & ff_include) == 0)? "" : " include", fs->name);
  }

/* Now output any extra fonts that are not Times-Roman (which will always be
the first defined font). */

for (int i = 6; i < font_tablen; i++)
  {
  if (font_table[i] != 0)
    {
    fontstr *fs = font_list + font_table[i];
    P("textfont extra %d%s \"%s\"\n", i - 5,
      ((fs->flags & ff_include) == 0)? "" : " include", fs->name);
    }
  }

/* Write custom key signatures */

for (int i = 0; i < MAX_XKEYS; i++)
  {
  uint8_t *kp = &(keysigtable[key_X + i][0]);
  if (*kp == ks_end) continue;
  P("makekey X%d", i+1);
  while (*kp != ks_end)
    {
    uint8_t k = *kp++;
    P(" %s%d", acnames[k>>4], (k & 0x0f) - 1);
    }
  P("\n");
  }

/* Write key transpostion special rules. This may be wrong if there are
different rules specified in different movements in the original. */

for (trkeystr *k = main_transposedkeys; k != NULL; k = k->next)
  P("transposedkey %s use %s\n", string_format_key(k->oldkey),
    string_format_key(k->newkey));

/* Find the highest stave used in any movement */

for (usint movt = 0; movt < movement_count; movt++)
  {
  movtstr *m = movements[movt];
  if (m->laststave > max_stave_used) max_stave_used = m->laststave;
  }

/* Settings that can be changed in all movements. */

for (usint movt = 0; movt < movement_count; movt++)
  {
  BOOL same;
  movtstr *p;
  movtstr *m = movements[movt];
  uint32_t mts = m->time;

  if (movt == 0)
    {
    p = &default_movtstr;
    }
  else
    {
    p = movements[movt - 1];
    P("\n\n[newmovement%s]\n", ((m->flags & mf_newpage) != 0)? " newpage":"");
    }


  /* These directives do not carry over between movements: bar, doublenotes,
  halvenotes, key, layout, notime, startbracketbar, startnotime, suspend, time,
  transpose, unfinished. */

  /* ---- Bar ---- */

  if (m->baroffset != 0) P("bar %d\n", m->baroffset + 1);

  /* ---- Key ---- */

  P("key %s\n", string_format_key(m->key));

  /* ---- Time ---- */

  write_time(mts);
  P("\n");

  /* ---- Layout ---- */

  if (m->layout != NULL)   /* Layout does not carry over to a new movement. */
    {
    uint16_t *lp = m->layout;
    const char *sp = " ";

    P("layout");

    for (;;)
      {
      switch (*lp++)
        {
        case lv_barcount:
        P("%s%d", sp, *lp++);
        sp = ", ";
        break;

        case lv_repeatcount:
        P(" %d(", *lp++);
        sp = "";
        break;

        case lv_repeatptr:
        if (*lp++ == 0)
          {
          P("\n");
          goto ENDLAYOUT;
          }
        P(")");
        sp = " ";
        break;

        case lv_newpage:
        P(";");
        sp = " ";
        break;

        default:
        error(ERR47);  /* Hard */
        break;
        }
      }
    ENDLAYOUT:
    }

  /* ---- Suspend ---- */

  if (m->suspend_staves != 0) write_stavebits(m->suspend_staves, "suspend");

  /* ---- Transpose ---- */

  if (m->transpose != NO_TRANSPOSE) P("transpose %d\n", m->transpose/2);

  /* ---- Bits in the movement flags field that apply only to this movement ---- */

  if ((m->flags & mf_showtime) == 0) P("notime\n");
  if ((m->flags & mf_startnotime) != 0) P("startnotime\n");
  if ((m->flags & mf_unfinished) != 0) P("unfinished\n");


  /* Some things are known not to be supported (yet?) */

  if (font_call_b2pf) error(ERR202, "B2PFfont");
  if (draw_tree != NULL) error(ERR202, "draw");
  if (main_keytranspose != NULL) error(ERR202, "keytranspose");


  /* Other header stuff. Output only if there's a change from a default value
  (1st movement) or the previous movement (subsequent movements). */

  /*--- Accadjusts --- */

  if (memcmp(m->accadjusts, p->accadjusts, sizeof(int32_t)*NOTETYPE_COUNT) != 0)
    {
    int last;
    for (last = NOTETYPE_COUNT - 1; last != 0; last--)
      if (m->accadjusts[last] != p->accadjusts[last]) break;
    P("accadjusts");
    for (int i = 0; i <= last; i++)
      P(" %s", sff(m->accadjusts[i]));
    P("\n");
    }

  /* --- Accspacing --- */

  /* The order of the values in the accspacing directive is different to the
  order they appear in the vector. */

  if (memcmp(m->accspacing, p->accspacing, sizeof(uint32_t)*6) != 0)
    {
    P("accspacing");
    for (usint i = 0; i < sizeof(accspacingorder)/sizeof(int); i++)
      P(" %s", sff(m->accspacing[accspacingorder[i]]));
    P("\n");
    }

  /* ---- Barlinespace ---- */

  if (m->barlinespace != ((p->barlinespace != FIXED_UNSET)? p->barlinespace :
      misc_default_barlinespace(m)))
    P("barlinespace %s\n", sff(m->barlinespace));

  /* ---- Barnumberlevel --- */

  if (m->barnumber_level != p->barnumber_level)
    P("barnumberlevel %s%s\n", (m->barnumber_level >= 0)? "+":"",
      sff(m->barnumber_level));

  /* --- Brace --- */

  if (m->bracelist != p->bracelist)
    write_stavelist("brace", m->bracelist);

  /* --- Bracket and ThinBracket--- */

  if (m->bracketlist != p->bracketlist)
    write_stavelist("bracket", m->bracketlist);
  if (m->thinbracketlist != p->thinbracketlist)
    write_stavelist("thinbracket", m->thinbracketlist);

  /* ---- Clef widths ---- */

  same = TRUE;
  for (int i = 0; i < CLEF_COUNT; i++)
    {
    if (m->clefwidths[i] != p->clefwidths[i])
      {
      same = FALSE;
      break;
      }
   }

  if (!same)
    P("clefwidths %d %d %d %d %d\n", m->clefwidths[clef_treble],
      m->clefwidths[clef_bass], m->clefwidths[clef_alto],
      m->clefwidths[clef_hclef],m->clefwidths[clef_none]);

  /* --- Font sizes only --- */

  /* The first argument specifies whether or not to compare the matrix. */

  /* Music size can't be changed. */

  write_diff_font(FALSE, "gracesize", 1,
    &(m->fontsizes->fontsize_grace), &(p->fontsizes->fontsize_grace), -1, -1);

  write_diff_font(FALSE, "cuesize", 1,
    &(m->fontsizes->fontsize_cue), &(p->fontsizes->fontsize_cue), -1, -1);

  write_diff_font(FALSE, "cuegracesize", 1,
    &(m->fontsizes->fontsize_cuegrace), &(p->fontsizes->fontsize_cuegrace), -1, -1);

  /* (Mid)clefsize is stored as an absolute, but the directive specifies it as
  relative, so we have to set a scaling factor. */

  write_diff_font(FALSE, "clefsize", 10,
    &(m->fontsizes->fontsize_midclefs), &(p->fontsizes->fontsize_midclefs), -1, -1);

  write_diff_font(FALSE, "footnotesize", 1,
    &(m->fontsizes->fontsize_footnote), &(p->fontsizes->fontsize_footnote), -1, -1);

  write_diff_font(FALSE, "vertaccsize", 1,
    &(m->fontsizes->fontsize_vertacc), &(p->fontsizes->fontsize_vertacc), -1, -1);

  /* Font sizes for certain types of string */

  write_diff_font(FALSE, "fbsize", 1,
    &(m->fontsizes->fontsize_text[ff_offset_fbass]),
    &(p->fontsizes->fontsize_text[ff_offset_fbass]), -1, -1);

  write_diff_font(FALSE, "overlaysize", 1,
    &(m->fontsizes->fontsize_text[ff_offset_olay]),
    &(p->fontsizes->fontsize_text[ff_offset_olay]), -1, -1);

  write_diff_font(FALSE, "underlaysize", 1,
    &(m->fontsizes->fontsize_text[ff_offset_ulay]),
    &(p->fontsizes->fontsize_text[ff_offset_ulay]), -1, -1);

  /* --- Font sizes and types --- */

  write_diff_font(TRUE, "longrestfont", 1,
    &(m->fontsizes->fontsize_restct), &(p->fontsizes->fontsize_restct),
    m->fonttype_longrest, p->fonttype_longrest);

  write_diff_font(TRUE, "repeatbarfont", 1,
    &(m->fontsizes->fontsize_repno), &(p->fontsizes->fontsize_repno),
    m->fonttype_repeatbar, p->fonttype_repeatbar);

  write_diff_font(TRUE, "timefont", 1,
    &(m->fontsizes->fontsize_text[ff_offset_ts]),
    &(p->fontsizes->fontsize_text[ff_offset_ts]),
    m->fonttype_time, p->fonttype_time);

  write_diff_font(TRUE, "tupletfont", 1,
    &(m->fontsizes->fontsize_triplet), &(p->fontsizes->fontsize_triplet),

    m->fonttype_triplet, p->fonttype_triplet);

  /* --- More complicated cases --- */

  same = m->barnumber_textflags == p->barnumber_textflags &&
         m->barnumber_interval == p->barnumber_interval &&
         compare_fonts(TRUE, &(m->fontsizes->fontsize_barnumber),
          &(p->fontsizes->fontsize_barnumber), m->fonttype_barnumber,
          p->fonttype_barnumber);

  if (!same)
    {
    P("barnumbers");
    if ((m->barnumber_textflags & text_boxrounded) != 0) P(" roundboxed");
    else if ((m->barnumber_textflags & text_boxed) != 0) P(" boxed");
    else if ((m->barnumber_textflags & text_ringed) != 0) P(" ringed");
    if (m->barnumber_interval < 0) P(" line");
      else P(" %d", m->barnumber_interval);
    write_fontsize(&(m->fontsizes->fontsize_barnumber), "", 1);
    write_fontname(m->fonttype_barnumber, " ");
    P("\n");
    }

  /* Rehearsal mark configuration is also more than just a font size/type */

  same = (m->flags & mf_rehearsallsleft) == (p->flags & mf_rehearsallsleft) &&
         m->rehearsalstyle == p->rehearsalstyle &&
         compare_fonts(TRUE, &(m->fontsizes->fontsize_rehearse),
          &(p->fontsizes->fontsize_rehearse), m->fonttype_rehearse,
          p->fonttype_rehearse);

  if (!same)
    {
    P("rehearsalmarks");
    if ((m->flags & mf_rehearsallsleft) != (p->flags & mf_rehearsallsleft))
      P(" %slinestartleft", ((m->flags & mf_rehearsallsleft) == 0)? "no":"");
    if ((m->rehearsalstyle & text_boxrounded) != 0) P(" roundboxed");
    else if ((m->rehearsalstyle & text_boxed) != 0) P(" boxed");
    else if ((m->rehearsalstyle & text_ringed) != 0) P(" ringed");
    else P(" plain");
    write_fontsize(&(m->fontsizes->fontsize_rehearse), "", 1);
    write_fontname(m->fonttype_rehearse, " ");
    P("\n");
    }

  /* Trill string font size; also check the string. */

  if (!compare_fonts(TRUE, &(m->fontsizes->fontsize_trill),
      &(p->fontsizes->fontsize_trill), -1, -1) ||
      !compare_pmw_strings(m->trillstring, p->trillstring))
    {
    P("trillstring");
    write_fontsize(&(m->fontsizes->fontsize_trill), "", 1);
    write_pmw_string(m->trillstring, " ", 0xffffffffu);
    P("\n");
    }

  /* --- End of font settings --- */

  /* --- Gracespacing ---- */

  if (m->gracespacing[0] != p->gracespacing[0] ||
      m->gracespacing[1] != p->gracespacing[1])
    P("gracespacing %s %s\n", sff(m->gracespacing[0]),
      sff(m->gracespacing[1]));


  /* --- Headings and footings --- */

  if (m->heading != p->heading)
    write_headfoot(m->heading, "heading");
  if (m->pageheading != p->pageheading)
    write_headfoot(m->pageheading, "pageheading");

  if (m->footing != p->footing)
    write_headfoot(m->footing, "footing");
  if (m->pagefooting != p->pagefooting)
    write_headfoot(m->pagefooting, "pagefooting");
  if (m->lastfooting != p->lastfooting)
    write_headfoot(m->lastfooting, "lastfooting");

  /* --- Hyphenstring --- */

  if (!compare_pmw_strings(m->hyphenstring, p->hyphenstring))
    {
    write_pmw_string( m->hyphenstring, "hyphenstring ", font_rm);
    P("\n");
    }

  /* ---- Join and Joindotted ---- */

  if (m->joinlist != p->joinlist)
    write_stavelist("join", m->joinlist);

  if (m->joindottedlist != p->joindottedlist)
    write_stavelist("joindotted", m->joindottedlist);

  /* ---- Justify ---- */

  if (m->justify != p->justify)
    {
    P("justify");
    if (m->justify == just_all) P(" all"); else
      {
      if ((m->justify & just_top) != 0) P(" top");
      if ((m->justify & just_bottom) != 0) P(" bottom");
      if ((m->justify & just_left) != 0) P(" left");
      if ((m->justify & just_right) != 0) P(" right");
      }
    P("\n");
    }

  /* ---- MIDIstart ---- */

  if (m->midistart != p->midistart)
    {
    P("midistart");
    for (int i = 1; i <= m->midistart[0]; i++)
      P(" %d", m->midistart[i]);
    P("\n");
    }

  /* ---- MIDItempo ---- */

  if (m->miditempo != p->miditempo || m->miditempochanges != NULL)
    {
    uint32_t *t = m->miditempochanges;
    P("miditempo %d", m->miditempo);
    if (t != NULL) while (*t != UINT32_MAX)
      {
      write_barnumber(*t++, " ");
      P("/%d", *t++);
      }
    P("\n");
    }

  /* ---- Stavesizes ---- */

  if (memcmp(m->stavesizes, p->stavesizes, sizeof(uint32_t)*(MAX_STAVE+1)) != 0)
    {
    P("stavesizes");
    for (int i = 0; i < 64; i++)
      if (m->stavesizes[i] != p->stavesizes[i])
        P(" %d/%s", i, sff(m->stavesizes[i]));
    P("\n");
    }

  /* ---- Copyzero ---- */

  if (m->zerocopy != p->zerocopy)
    {
    P("copyzero");
    for (zerocopystr *z = m->zerocopy; z != NULL; z = z->next)
      P(" %d/%s", z->stavenumber, sff(z->adjust));
    P("\n");
    }

  /* ---- Breakbarlines(x) ---- */

  if (m->breakbarlines != 0 &&
       (m->breakbarlines != p->breakbarlines ||
       (m->flags & mf_fullbarend) != (p->flags & mf_fullbarend)))
    {
    const char *directive = ((m->flags & mf_fullbarend) == 0)?
      "breakbarlines" : "breakbarlinesx";
    if (m->breakbarlines == 0xffffffffffffffffl) P("%s\n", directive);
      else write_stavebits(m->breakbarlines, directive);
    }

  /* ---- Selectstaves ---- */

  if (m->select_staves != p->select_staves)
    write_stavebits(m->select_staves, "selectstaves");

  /* ---- Maxbeamslope ---- */

  if (m->maxbeamslope[0] != p->maxbeamslope[0] ||
      m->maxbeamslope[1] != p->maxbeamslope[1])
    P("maxbeamslope %s %s\n", sff(m->maxbeamslope[0]), sff(m->maxbeamslope[1]));

  /* ---- Notespacing ---- */

  if (memcmp(m->note_spacing, p->note_spacing, 8*sizeof(int32_t)) != 0)
    {
    P("notespacing");
    for (int i = 0; i < 8; i++) P(" %s", sff(m->note_spacing[i]));
    P("\n");
    }

  /* ---- Printkey ---- */

  for (pkeystr *pk = main_printkey; pk != NULL; pk = pk->next)
    {
    if (pk->movt_number != movt + 1) continue;
    P("printkey %s %s", string_format_key(pk->key), clef_names[pk->clef]);
    write_pmw_string(pk->string, " ", font_mf);
    if (pk->cstring[0] != 0) write_pmw_string(pk->cstring, " ", font_mf);
    P("\n");
    }

  /* ---- Printtime ---- */

  for (ptimestr *pt = main_printtime; pt != NULL; pt = pt->next)
    {
    if (pt->movt_number != movt + 1) continue;
    uint32_t ts = pt->time;
    P("printtime");
    if (ts > 0x0000ffffu) P(" %d*", ts >> 16);
    ts &= 0x0000ffffu;
    if (ts == time_common) P("C");
    else if (ts == time_cut) P("A");
    else P("%d/%d", (ts >> 8) & 0xff, ts & 0xff);

    write_pmw_string(pt->top, " ", m->fonttype_time);
    if (pt->sizetop != AllFontSizes && pt->top[0] != 0)
      {
      if (pt->sizetop > UserFontSizes)
        P("/S%d", pt->sizetop - UserFontSizes);
      else
        P("/s%d", pt->sizetop + 1);
      }

    write_pmw_string(pt->bot, " ", m->fonttype_time);
    if (pt->sizebot != AllFontSizes && pt->bot[0] != 0)
      {
      if (pt->sizebot > UserFontSizes)
        P("/S%d", pt->sizebot - UserFontSizes);
      else
        P("/s%d", pt->sizebot + 1);
      }

    P("\n");
    }

  /* ---- Startlinespacing ---- */

  if (memcmp(m->startspace, p->startspace, 4*sizeof(int32_t)) != 0)
    P("startlinespacing %s %s %s %s\n", sff(m->startspace[0]),
      sff(m->startspace[1]), sff(m->startspace[2]), sff(m->startspace[3]));

  /* --- Stavespacing ---- */

  if (memcmp(m->stave_spacing, p->stave_spacing,
       (max_stave_used + 1)*sizeof(int32_t)) != 0 ||
      memcmp(m->stave_ensure, p->stave_ensure,
        (max_stave_used + 1)*sizeof(int32_t)) != 0)
    {
    int index = 0;
    int maxcount = 0;

    for (int i = 1; i <= max_stave_used; i++)
      {
      int count = 1;
      for (int j = i+1; j <= max_stave_used; j++)
        if (m->stave_spacing[j] == m->stave_spacing[i] &&
            m->stave_ensure[j] == 0) count++;
      if (count > maxcount)
        {
        maxcount = count;
        index = i;
        }
      }

    P("stavespacing %s", sff(m->stave_spacing[index]));
    for (int i = 1; i <= max_stave_used; i++)
      {
      if (m->stave_spacing[i] != m->stave_spacing[index] ||
          m->stave_ensure[i] != 0)
        {
        P(" %d", i);
        if (m->stave_ensure[i] != 0) P("/%s", sff(m->stave_ensure[i]));
        P("/%s", sff(m->stave_spacing[i]));
        }
      }

    P("\n");
    }

  /* ---- Stemlengths ---- */

  if (memcmp(m->stemadjusts, p->stemadjusts,
        (NOTETYPE_COUNT - 2)*sizeof(int32_t)) != 0)
    {
    P("stemlengths");
    for (int i = 2; i < NOTETYPE_COUNT; i++) P(" %s", sff(m->stemadjusts[i]));
    P("\n");
    }

  /* ---- Stemswap ---- */

  if (m->stemswaptype != p->stemswaptype) switch(m->stemswaptype)
    {
    case stemswap_default: P("stemswap default\n"); break;
    case stemswap_up:      P("stemswap up\n"); break;
    case stemswap_down:    P("stemswap down\n"); break;
    case stemswap_left:    P("stemswap left\n"); break;
    case stemswap_right:   P("stemswap right\n"); break;
    }

  /* --- Stemswaplevel ---- */

  if (memcmp(m->stemswaplevel, p->stemswaplevel,
       (max_stave_used + 1)*sizeof(int32_t)) != 0)
    {
    int index = 0;
    int maxcount = 0;

    for (int i = 1; i <= max_stave_used; i++)
      {
      int count = 1;
      for (int j = i+1; j <= max_stave_used; j++)
        if (m->stemswaplevel[j] == m->stemswaplevel[i] &&
            m->stave_ensure[j] == 0) count++;
      if (count > maxcount)
        {
        maxcount = count;
        index = i;
        }
      }

    P("stemswaplevel %s", sff(m->stemswaplevel[index]));
    for (int i = 1; i <= max_stave_used; i++)
      {
      if (m->stemswaplevel[i] != m->stemswaplevel[index])
        P(" %d/%d", i, m->stemswaplevel[i]);
      }
    P("\n");
    }

  /* ---- Systemseparator ---- */

  if (m->systemseplength != p->systemseplength ||
      m->systemsepwidth != p->systemsepwidth ||
      m->systemsepangle != p->systemsepangle ||
      m->systemsepposx != p->systemsepposx ||
      m->systemsepposy != p->systemsepposy)
    {
    P("systemseparator");
    if (m->systemseplength == 0) P(" 0\n"); else
      P(" %s %s %s %s %s\n", sff(m->systemseplength), sff(m->systemsepwidth),
        sff(m->systemsepangle), sff(m->systemsepposx), sff(m->systemsepposy));
    }

  /* ---- Textsizes ---- */

  /* Font size settings in the first movement change the default structure. In
  subsequent movements we check for changes from the previous movement. */

  if (movt == 0)
    {
    int last;
    for (last = UserFontSizes - 1; last >= 0; last--)
      {
      fontinststr *f = &(m->fontsizes->fontsize_text[last]);
      if (f->size != 10000 || f->matrix != NULL) break;
      }
    if (last >= 0)
      {
      P("textsizes");
      for (int i = 0; i <= last; i++)
        write_fontsize(&(m->fontsizes->fontsize_text[i]), "", 1);
      P("\n");
      }
    }

  else if (memcmp(m->fontsizes->fontsize_text, p->fontsizes->fontsize_text,
      UserFontSizes * sizeof(fontinststr)) != 0)
    {
    int last;
    P("textsizes");
    for (last = UserFontSizes - 1; last > 0; last--)
      if (memcmp(&(m->fontsizes->fontsize_text[last]),
            &(p->fontsizes->fontsize_text[last]), sizeof(fontinststr)) != 0)
        break;
    for (int i = 0; i <= last; i++)
      write_fontsize(&(m->fontsizes->fontsize_text[i]), "", 1);
    P("\n");
    }

  /* ---- Various int32_t dimension fields in movtstr ---- */

  for (usint i = 0; i < movt32dimfieldscount; i++)
    {
    movtfield *f = &(movt32dimfields[i]);
    size_t offset = f->offset;
    if (*((int32_t *)((char *)m + offset)) !=
        *((int32_t *)((char *)p + offset)))
      P("%s %s\n", f->name, sff(*((int32_t *)((char *)m + offset))));
    }

  /* ---- Various int32_t numerical fields in movtstr ---- */

  for (usint i = 0; i < movt32numfieldscount; i++)
    {
    movtfield *f = &(movt32numfields[i]);
    size_t offset = f->offset;
    if (*((int32_t *)((char *)m + offset)) !=
        *((int32_t *)((char *)p + offset)))
      P("%s %d\n", f->name, *((int32_t *)((char *)m + offset)));
    }

  /* ---- Various uint32_t dimension fields in movtstr ---- */

  for (usint i = 0; i < movtu32dimfieldscount; i++)
    {
    movtfield *f = &(movtu32dimfields[i]);
    size_t offset = f->offset;
    if (*((uint32_t *)((char *)m + offset)) !=
        *((uint32_t *)((char *)p + offset)))
      P("%s %s\n", f->name, sff(*((uint32_t *)((char *)m + offset))));
    }

#ifdef NEVER
  /* ---- Various uint32_t numerical fields in movtstr ---- */

  for (usint i = 0; i < movtu32numfieldscount; i++)
    {
    movtfield *f = &(movtu32numfields[i]);
    size_t offset = f->offset;
    if (*((uint32_t *)((char *)m + offset)) !=
        *((uint32_t *)((char *)p + offset)))
      P("%s %d\n", f->name, *((uint32_t *)((char *)m + offset)));
    }
#endif

  /* ---- Various uint8_t numerical fields in movtstr ---- */

  for (usint i = 0; i < movtu8numfieldscount; i++)
    {
    movtfield *f = &(movtu8numfields[i]);
    size_t offset = f->offset;
    if (*((uint8_t *)((char *)m + offset)) !=
        *((uint8_t *)((char *)p + offset)))
      P("%s %d\n", f->name, *((uint8_t *)((char *)m + offset)));
    }


  /* ---- Bits in the movement flags field that have on/off directives ---- */

  for (usint i = 0; i < movtflagscount; i++)
    {
    movtflag *f = &(movtflags[i]);
    if ((m->flags & f->flag) != (p->flags & f->flag))
      P("%s%s\n", ((m->flags & f->flag) == 0)? "no" : "", f->name);
    }

  if ((m->flags & mf_keydoublebar) != (p->flags & mf_keydoublebar))
    P("key%sbar\n", ((m->flags & mf_keydoublebar) == 0)? "single" : "double");


  /* ---- Now deal with the staves  ----*/

  for (int stave = 0; stave <= m->laststave; stave++)
    {
    int clefadjust = 0;
    int couple_up = 0;
    int couple_down = 0;
    int couple_state = 0;   
    int note_total = 0;
    int note_su = 0;
    int note_sd = 0;
    int note_suw = 0;
    int note_sdw = 0;
    int chordcount = 0;
    int stemdirection = 0;   /* Auto */
    int barno;
    int octave = 0;
    int octave_counts[8];
    int max_octave_count = 0;
    int max_fontsize = 0;
    int hp_abovecount = 0;
    int hp_belowcount = 0;
    int hp_ulcount = 0;
    int textcount_above = 0;
    int textcount_below = 0;
    int textcount_overlay = 0;
    int textcount_underlay = 0;
    int textcount_fb = 0;
    int textcount_fbu = 0;
    int textcount_max;
    int textsize = -1;
    int tiesabovecount = 0;
    int tiesbelowcount = 0;
    int tiesset = 0;
    int fontsize_counts[UserFontSizes];
    BOOL wasnote;
    BOOL incue = FALSE;
    barstr **barvector, *b;
    b_ornamentstr *ornament = NULL;
    b_textstr *pendulay[PENDULAY_MAX];
    int pendulay_count = 0;
    const char *text_type= "";
    uint32_t text_type_flags = 0;
    uint32_t hp_type_flags = hp_below;
    uint32_t uleqstarted = 0;
    uint32_t oleqstarted = 0;
    uint32_t bar_hwm = 0;
    uint32_t bar_noteslength = 0;
    uint32_t bar_lastnotelength = 0;   

    st = m->stavetable[stave];
    if (st->barcount == 0) continue;

    P("\n[stave %d%s", stave, st->omitempty? " omitempty":"");

    for (snamestr *sn = st->stave_name; sn != NULL; sn = sn->next)
      {
      if (sn->text == NULL) continue;
      snamestr *snt = sn;
      snamestr **snnext = &(sn->extra);

      P(" ");
      for(;;)
        {
        write_pmw_string(snt->text, "", font_rm);
        if ((snt->flags & snf_hcentre) != 0) P("/c");
        if ((snt->flags & snf_rightjust) != 0) P("/e");
        if ((snt->flags & snf_vcentre) != 0) P("/m");
        if ((snt->flags & snf_vertical) != 0) P("/v");

        if (snt->size != ff_offset_init)
          {
          if (snt->size > UserFontSizes)
            P("/S%d", snt->size - UserFontSizes + 1);
          else P("/s%d", snt->size + 1);
          }
        write_move(snt->adjustx, snt->adjusty);

        snt = *snnext;
        if (snt == NULL) break;
        P("/");
        snnext = &(snt->next);
        }
      }

    P("]\n");

    /* First make a pass through the stave to collect information such as the
    numbers of notes with various stem conditions, so that certain defaults can
    be set. */

    memset(octave_counts, 0, sizeof(octave_counts));
    memset(fontsize_counts, 0, sizeof(fontsize_counts));

    barvector = st->barindex;
    barno = 0;
    b = barvector[barno];

    while (b != NULL)
      {
      if (b->type == b_clef)
        {
        switch (((b_clefstr *)b)->clef)
          {
          case clef_contrabass:
          case clef_trebletenor:
          case clef_trebletenorB:
          clefadjust = +1;
          break;

          case clef_soprabass:
          case clef_trebledescant:
          clefadjust = -1;
          break;

          default:
          clefadjust = 0;
          break;
          }
        }

      else if (b->type == b_note)
        {
        b_notestr *n = (b_notestr *)b;
        if (n->spitch != 0)             /* Not a rest */
          {
          note_total++;
          octave_counts[n->abspitch/24 + clefadjust] += 1;
           
          if ((n->flags & nf_stem) != 0)
            {
            if ((n->flags & nf_stemup) != 0)
              {
              note_su++;
              if (n->spitch > P_3L) note_suw++;
              }
            else
              {
              note_sd++;
              if (n->spitch < P_3L) note_sdw++;
              }
            }
            
           if ((n->flags & nf_coupleU) != 0) couple_up++;
           if ((n->flags & nf_coupleD) != 0) couple_down++;

          /* If there was previous underlay or overlay see if it is immediately
          before this note, and if not, move it in the chain to be immediately
          before. This can happen with XML input, though not with PMW input
          unless there was a default ornament. We want underlay be adjacent to
          the note because a re-process of this output will put it there and we
          want another round of PMW output to generate the same output.
          Examples have been seen that have multiple syllables that are not
          contiguous, so code for that possibility by treating each syllable on
          its own. */

          for (int i = 0; i < pendulay_count; i++)
            {
            b_textstr *pt = pendulay[i];
            if ((barstr *)pt->next != b)
              {
              bstr *prev, *next;

              /* Cut the sequence out of the chain */

              prev = pt->prev;
              next = pt->next;
              prev->next = next;
              next->prev = prev;

              /* Insert just before this note */

              prev = b->prev;
              prev->next = (bstr *)pt;
              pt->prev = prev;
              ((bstr *)pt)->next = (bstr *)b;
              b->prev = (bstr *)pt;
              }
            }

          pendulay_count = 0;
          }

        /* This is a rest. For PMW input there can never be underlay or overlay
        preceding a rest because PMW distributes it to the following notes,
        skipping rests. However, MusicXML is capable of doing this. If we do
        nothing, it will get moved to just before the next note when this
        output is processed, so we change it to be not underlay, but at the
        underlay level. Give a warning. */

        else for (int i = 0; i < pendulay_count; i++)
          {
          char buffer[32];
          b_textstr *pt = pendulay[i];

          while (pt != NULL)
            {
            const char *which = ((pt->flags & text_above) == 0)?
              "under":"over";
            format_barnumber(m->barvector[barno], buffer);
            error(ERR204, which, buffer, stave, which);
            pt->flags = (pt->flags & ~text_ul) | text_atulevel;
            pt->laylen = 0;

            pt = (b_textstr *)pt->next;
            if (pt->type != b_text || (pt->flags & text_ul) == 0) break;
            }

          pendulay_count = 0;
          }
        }

      else if (b->type == b_text)
        {
        b_textstr *t = (b_textstr *)b;

        /* Remember underlay or overlay items that precede a note. */

        if ((t->flags & text_ul) != 0)
          pendulay[pendulay_count++] = (b_textstr *)b;

        /* Collect statistics */

        if ((t->flags & text_rehearse) == 0)
          {
          if ((t->flags & (text_fb|text_atulevel)) == (text_fb|text_atulevel))
            textcount_fbu++;
          else if ((t->flags & text_fb) != 0) textcount_fb++;
          else if ((t->flags & (text_ul|text_above)) == (text_ul|text_above))
            textcount_overlay++;
          else if ((t->flags & text_ul) != 0) textcount_underlay++;
          else
            {
            if ((t->flags & text_above) != 0) textcount_above++;
              else textcount_below++;
            if (t->size < UserFontSizes) fontsize_counts[t->size]++;
            }
          }
        }

      else if (b->type == b_hairpin)
        {
        b_hairpinstr *h = (b_hairpinstr *)b;
        if ((h->flags & hp_end) == 0)
          {
          if ((h->flags & (hp_below|hp_underlay)) == (hp_below|hp_underlay))
            hp_ulcount++;
          else if ((h->flags & hp_below) != 0) hp_belowcount++;
          else hp_abovecount++;
          }
        }

      else if (b->type == b_tie)
        {
        b_tiestr *tie = (b_tiestr *)b;
        if (tie->abovecount > 0 && tie->belowcount == 0) tiesabovecount++;
        else if (tie->belowcount > 0 && tie->abovecount == 0) tiesbelowcount++;
        }

      b = (barstr *)b->next;
      if (b == NULL && ++barno < st->barcount) b = barvector[barno];
      }  /* End of preliminary scan */

    /* Set a hairpin default if most are not just "below". */

    if (hp_belowcount < hp_abovecount || hp_belowcount < hp_ulcount)
      {
      if (hp_abovecount > hp_ulcount)
        {
        P("[hairpins above]\n");
        hp_type_flags = 0;
        }
      else
        {
        P("[hairpins underlay]\n");
        hp_type_flags = hp_below | hp_underlay;
        }
      }

    /* Set a ties default. */

    if (tiesabovecount > tiesbelowcount && tiesabovecount > 0)
      {
      P("[ties above]\n");
      tiesset = +1;
      }
    else if (tiesbelowcount > 0)
      {
      P("[ties below]\n");
      tiesset = -1;
      }

    /* Set a default size for text that is neither underlay nor overlay nor
    figured bass. */

    for (int i = 0; i < UserFontSizes; i++)
      {
      if (fontsize_counts[i] > max_fontsize)
        {
        max_fontsize = fontsize_counts[i];
        textsize = i;
        }
      }

    /* Texsize will be netgative if there is no text in the stave. */

    if (textsize >= 0) P("[textsize %d]\n", textsize + 1);

    /* Set a default text type */

    textcount_max = textcount_below;
    if (textcount_above > textcount_max)
      {
      textcount_max = textcount_above;
      text_type = "above";
      text_type_flags = text_above;
      }
    if (textcount_overlay > textcount_max)
      {
      textcount_max = textcount_overlay;
      text_type = "overlay";
      text_type_flags = text_ul|text_above;
      }
    if (textcount_underlay > textcount_max)
      {
      textcount_max = textcount_underlay;
      text_type = "underlay";
      text_type_flags = text_ul;
      }
    if (textcount_fb > textcount_max)
      {
      textcount_max = textcount_fb;
      text_type = "fb";
      text_type_flags = text_fb;
      }
    if (textcount_fbu > textcount_max)
      {
      textcount_max = textcount_fbu;
      text_type = "fbu";
      text_type_flags = text_fb|text_ul;
      }

    if (text_type[0] != 0) P("[text %s]\n", text_type);

    /* Select the octave with the most notes */

    for (int i = 0; i < 8; i++)
      {
      if (octave_counts[i] > max_octave_count)
        {
        max_octave_count = octave_counts[i];
        octave = i;
        }
      }

    P("[octave %d]\n", octave - 3);

    /* Decide whether to force a stem direction */

(void)note_total;  // Not currently used

    if (note_su > 2*note_sd &&  /* Substantially more up than down */
        note_suw > note_su/4)   /* More than 1/4 are "wrong" */
      {
      P("[stems up]\n");
      stemdirection = +1;
      }
    else if (note_sd > 2*note_su &&  /* Substantially more down than up */
             note_sdw > note_sd/4)   /* More than 1/4 are "wrong" */
      {
      P("[stems down]\n");
      stemdirection = -1;
      }
      
    /* Handle coupling */
    
    if (couple_up > couple_down)
      {
      P("[couple up]\n");
      couple_state = +1;  
      }
    else if (couple_down > 0)
      {
      P("[couple down]\n");
      couple_state = -1;  
      }         

    /* Now process the stave's data */

    wasnote = FALSE;
    barno = 0;
    clefadjust = 0; 
    b = barvector[barno];

    while (b != NULL)
      {
      if (wasnote && b->type != b_tie && b->type != b_beambreak) P(" ");
      wasnote = FALSE;

      switch (b->type)
        {
        /* At the start of a bar, look ahead to see if this is a replicated
        bar and arrange to insert a count and a skip to the last of the
        replicated group if it is. A replicated bar is identified by having the
        same next pointer in its initial b_start structure. */

        case b_start:
        int rcount = 1;
        for (int barnext = barno + 1; barnext < st->barcount; barnext++)
          {
          if (barvector[barnext]->next != b->next) break;
          rcount++;
          }
        if (rcount > 1)
          {
          P("[%d] ", rcount);
          barno += rcount - 1;
          b = barvector[barno];
          }
        incue = FALSE;
        break;

        case b_all:
        P("[all] ");
        break;

        case b_barline:
        b_barlinestr *bl = (b_barlinestr *)b;
        if (m->barvector[barno] == 0 || (m->barvector[barno] & 0xffffu) != 0)
          P("[nocount] ");
        if (bar_noteslength > bar_hwm) bar_hwm = bar_noteslength;
        if (bar_hwm != barlength) P("[nocheck] ");
        bar_hwm = bar_noteslength = bar_lastnotelength = 0;    
        switch(bl->bartype)
          {
          case barline_normal: P("|"); break;
          case barline_double: P("||"); break;
          case barline_ending: P("|||"); break;
          case barline_invisible: P("|?"); break;
          }
        if (bl->barstyle != 0) P("%d", bl->barstyle);
        write_barnumber(m->barvector[barno], " @");
        P("\n");
        break;

        /* Some fudge code is needed here to deal with [tremolo], which inserts
        an "all" beam break of its own. To distinguish this from an explicit
        beam break in the original, we look at the previous note. If it is not
        marked as a free upstemmed quaver or shorter, the beambreak is not
        explicit because if it had been, the note would have been so marked. */

        case b_beambreak:
        int x = ((b_beambreakstr *)b)->value;
        if (x == BEAMBREAK_ALL)
          {
          BOOL ok = TRUE;
          if (b->next->type == b_tremolo)
            {
            for (barstr *pp = (barstr *)b->prev; pp != NULL;
                 pp = (barstr *)pp->prev)
              {
              if (pp->type == b_note)
                {
                ok = !(((b_notestr *)pp)->notetype >= quaver &&
                  (((b_notestr *)pp)->flags & nf_fuq) == 0);
                if (!ok) P(" ");
                break;
                }
              }
            }
          if (ok) P("; ");
          }
        else if (x == 1) P(", ");
        else P(",%d ", x);
        break;

        case b_caesura:
        P("// ");
        break;

        case b_clef:
        b_clefstr *clef = (b_clefstr *)b;
        P("[");
        if (clef->assume) P("assume ");
        P("%s] ", clef_names[clef->clef]);
        
        switch (clef->clef)
          {
          case clef_contrabass:
          case clef_trebletenor:
          case clef_trebletenorB:
          clefadjust = +1;
          break;

          case clef_soprabass:
          case clef_trebledescant:
          clefadjust = -1;
          break;

          default:
          clefadjust = 0;
          break;
          }
 
        break;

        case b_comma:
        P("[comma] ");
        break;

        case b_endplet:
        P("} ");
        break;

        case b_endslur:
        case b_endline:
        b_endslurstr *bes = (b_endslurstr *)b;
        P("[%s", (b->type == b_endslur)? "es" : "el");
        if (bes->value != 0) P("/=%c", bes->value);
        P("] ");
        break;

        /* Omit key in the first bar if it's the same as the movement key.
        Also omit naturalizing keys, which are an artefact of processing and
        are not needed (or indeed allowed) in PMW input. */

        case b_key:
        b_keystr *key = (b_keystr *)b;
        if (key->key < 0x80 && (barno != 0 || key->key != m->key))
          {
          P("[");
          if (key->assume) P("assume ");
          P("key %s", string_format_key(key->key));
          if (((m->flags & mf_keywarn) != 0) != key->warn)
            P(" %swarn", key->warn? "" : "no");
          P("] ");
          }
        break;

        case b_lrepeat:
        P("(: ");
        break;

        case b_move:
        b_movestr *mv = (b_movestr *)b;
        P("[%smove %s", mv->relative? "r" : "", sff(mv->x));
        if (mv->y != 0) P(",%s", sff(mv->y));
        P("] ");
        break;

        case b_nbar:
        b_nbarstr *nb = (b_nbarstr *)b;
        P("[%d%s", nb->n, (nb->n == 1)? "st" : (nb->n == 2)? "nd" :
          (nb->n == 3)? "rd" : "th");
        if (nb->s != NULL) write_pmw_string(nb->s, "/", font_rm);
        write_move(nb->x, nb->y);
        P("] ");
        break;

        case b_newline:
        P("[newline] ");
        break;

        case b_ornament:
        ornament = (b_ornamentstr *)b;
        break;

        case b_reset:
        if (bar_noteslength > bar_hwm) bar_hwm = bar_noteslength;
        if (((b_resetstr *)b)->moff == 0)
          {
          P("[reset] ");
          bar_noteslength = bar_lastnotelength = 0;   
          }
        else
          {
          P("[backup] "); 
          bar_noteslength -= bar_lastnotelength;
          /* Backup can't be repeated, so not backing up lastnotelength is not
          a problem. */   
          }       
 
//        P("[%s] ", (((b_resetstr *)b)->moff == 0)? "reset" : "backup");
        break;

        case b_resume:
        P("[resume] ");
        break;

        case b_rrepeat:
        P(":) ");
        break;

        case b_sgabove:
        P("[sgabove ");
        goto SGDATA;

        case b_sghere:
        P("[sghere ");
        goto SGDATA;

        case b_sgnext:
        P("[sgnext ");
        SGDATA:
        b_sgstr *sg = (b_sgstr *)b;
        if (sg->relative && sg->value >= 0) P("+");
        P("%s] ", sff(sg->value));
        break;

        case b_space:
        b_spacestr *sp = (b_spacestr *)b;
        P("[%sspace %s] ", sp->relative? "r" : "", sff(sp->x));
        break;

        case b_ssabove:
        P("[ssabove ");
        goto SSDATA;

        case b_sshere:
        P("[sshere ");
        goto SSDATA;

        case b_ssnext:
        P("[ssnext ");
        SSDATA:
        b_ssstr *ss = (b_ssstr *)b;
        if (ss->stave != stave) P("%d/", ss->stave);
        if (ss->relative && ss->value >= 0) P("+");
        P("%s] ", sff(ss->value));
        break;

        case b_suspend:
        P("[suspend] ");
        break;

        case b_tick:
        P("[tick] ");
        break;

        case b_tie:
        b_tiestr *tie = (b_tiestr *)b;
        P("_");

        if (tie->abovecount > 0)
          {
          if (tie->belowcount != 0) P("/%da", tie->abovecount);
            else if (tiesset <= 0) P("/a");
          }
        if (tie->belowcount > 0)
          {
          if (tie->abovecount != 0) P("/%db", tie->belowcount);
            else if (tiesset >= 0) P("/b");
          }

        if ((tie->flags & tief_gliss) != 0)
          {
          P("/g");
          if ((tie->flags & tief_slur) != 0) P("/s");
          }

        if ((tie->flags & tief_editorial) != 0) P("/e");
        if ((tie->flags & tief_dotted) != 0) P("/ip");
        if ((tie->flags & tief_dashed) != 0) P("/i");
        if (b->next->type != b_beambreak) P(" ");
        break;

        case b_time:
        b_timestr *tm = (b_timestr *)b;
        if (barno != 0 || tm->time != m->time)
          {
          P("[");
          if (tm->assume) P("assume ");
          write_time(tm->time);
          if (((m->flags & mf_timewarn) != 0) != tm->warn)
            P(" %swarn", tm->warn? "" : "no");
          P("] ");
          }
        break;

        case b_tremolo:
        b_tremolostr *trem = (b_tremolostr *)b;
        P("[tremolo");
        if (trem->count != 2) P("/x%d", trem->count);
        if (trem->join != 0) P("/j%d", trem->join);
        P("] ");
        break;

        /* -------- Hairpins -------- */

        case b_hairpin:
        b_hairpinstr *hp = (b_hairpinstr *)b;

        P("%s", ((hp->flags & hp_cresc) != 0)? "<" : ">");

        /* Vertical positioning only at start. It is possible for an absolute
        above hairpin position to have a negative yvalue or a below one to have
        a positive yvalue as a result of relative movements. */

        if ((hp->flags & hp_end) == 0 &&
            ((hp->flags & hp_abs) != 0 ||
            (hp->flags & (hp_below|hp_underlay)) != hp_type_flags))

          {
          switch (hp->flags & (hp_below|hp_middle|hp_underlay))
            {
            case 0:
            P("/a");
            if ((hp->flags & hp_abs) != 0)
              {
              if (hp->y >= 0) P("%s", sff(hp->y));
                else P("0/d%s", sff(-hp->y));
              }
            break;

            case hp_below:
            P("/b");
            if ((hp->flags & hp_abs) != 0)
              {
              if (hp->y <= 0) P("%s", sff(-hp->y));
                else P("0/u%s", sff(hp->y));
              }
            break;

            case hp_below|hp_underlay:
            P("/bu");
            break;

            case hp_middle:
            P("/m");
            break;
            }
          }

        if ((hp->flags & hp_abs) == 0) write_move(hp->x, hp->y);

        if((hp->flags & hp_bar) != 0) P("/bar");
        if((hp->flags & hp_halfway) != 0)
          {
          P("/h");
          if (hp->halfway != 500) P("%s", sff(hp->halfway));
          }

        write_move_opt(hp->offset, 0, "/rc", "/lc", NULL, NULL);

        /* Starting hairpin has extra things. */

        if ((hp->flags & hp_end) == 0)
          {
          int tempbarno = barno;
          barstr *tempb = b;

          if (hp->width != (int32_t)(m->hairpinwidth))
            P("/w%s", sff(hp->width));
          write_move_opt(0, hp->su, NULL, NULL, "/slu", "/sld");

          /* Look ahead for the ending hairpin, and output the su field as
          sru/srd. Hairpins are not allowed to nest, so the next one must be
          the end that matches this one. */

          for (;;)
            {
            tempb = (barstr *)tempb->next;
            if (tempb == NULL && ++tempbarno < st->barcount)
              tempb = barvector[tempbarno];
            if (tempb == NULL) break;
            if (tempb->type == b_hairpin)
              {
              write_move_opt(0, ((b_hairpinstr *)tempb)->su, NULL, NULL, "/sru",
                "/srd");
              break;
              }
            }
          }

        P(" ");
        break;

        /* -------- Text -------- */

        case b_text:
        b_textstr *txt = (b_textstr *)b;
        uint32_t fl = txt->flags;
        int32_t yvalue = txt->y;
        BOOL addsize = TRUE;

        /* Underlay or overlay */

        if (txt->laylen != 0)
          {
          uint32_t c;
          uint32_t *eptr = ((fl & text_above) == 0)? &uleqstarted:&oleqstarted;
          uint32_t levelbit = 1 << txt->laylevel;
          usint len = txt->laylen;

          /* There is complication here. If a syllable is not the end of a
          word, we must include the hyphen in the written text. If a syllable
          that is not just a single '=' ends in '=' (with or without a
          preceding hyphen) we must include that in the output, and then
          arrange that the single '=' that will automatically have been
          generated for the next note is suppressed. */

          if (txt->string[len] == '-') len++;
          if (txt->string[len] == '=' && (len > 1 || txt->string[0] != '='))
            {
            *eptr |= levelbit;
            len++;
            }
          else if (txt->string[0] == '=' && (*eptr & levelbit) != 0)
            {
            *eptr &= ~levelbit;
            break;  /* Skip this text item */
            }

          /* Output the underlay or overlay syllable. */

          c = txt->string[len];
          txt->string[len] = 0;
          write_pmw_string(txt->string, "", font_rm);
          txt->string[len] = c;

          if ((fl & text_above) != 0)
            {
            if (text_type_flags != (text_ul|text_above)) P("/ol");
            addsize = txt->size != ff_offset_olay;
            }
          else
            {
            if (text_type_flags != text_ul) P("/ul");
            addsize = txt->size != ff_offset_ulay;
            }
          }

        /* Figured bass */

        else if ((fl & text_fb) != 0)
          {
          write_pmw_string(txt->string, "", font_rm);
          P("/fb");
          if ((fl & text_atulevel) != 0) P("u");
          addsize = txt->size != ff_offset_fbass;
          }

        /* Rehearsal mark; only allowed options are for movement */

        else if ((fl & text_rehearse) != 0)
          {
          P("[");
          write_pmw_string(txt->string, "", font_it);
          if ((fl & text_absolute) != 0) P("/a0");
          write_move(txt->x, yvalue);
          P("] ");
          break;
          }

        /* Neither underlay nor overlay nor figured bass */

        else
          {
          write_pmw_string(txt->string, "", font_it);
          if ((fl & text_type_flags) != text_type_flags ||
              (fl & text_absolute) != 0)
            {
            if ((fl & (text_above|text_atulevel)) == (text_above|text_atulevel))
              P("/ao");

            /* It is possible for absolute above text to have a negative yvalue
            or below text to have positive yvalue as a result of relative
            movements. */

            else if ((fl & text_above) != 0)
              {
              P("/a");
              if ((fl & text_absolute) != 0)
                {
                if (yvalue >= 0) P("%s", sff(yvalue));
                  else P("0/d%s", sff(-yvalue));
                yvalue = 0;
                }
              }

            else if ((fl & text_atulevel) != 0) P("/bu");
            else
              {
              P("/b");
              if ((fl & text_absolute) != 0)
                {
                if (yvalue <= 0) P("%s", sff(-yvalue));
                  else P("0/u%s", sff(yvalue));
                yvalue = 0;
                }
              }
            }
          addsize = txt->size != textsize;
          }

        /* Add size if necessary */

        if (addsize)
          {
          switch(txt->size)
            {
            case ff_offset_ulay: P("/su"); break;
            case ff_offset_olay: P("/so"); break;
            case ff_offset_fbass: P("/sf"); break;
            default:
            if (txt->size > UserFontSizes)
              P("/S%d", txt->size - UserFontSizes + 1);
            else P("/s%d", txt->size + 1);
            break;
            }
          }

        /* Other options */

        if ((fl & text_baralign) != 0) P("/bar");
        if ((fl & text_barcentre) != 0) P("/cb");
        if ((fl & text_boxrounded) != 0) P("/rbox");
          else if ((fl & text_boxed) != 0) P("/box");
        if ((fl & text_centre) != 0) P("/c");
        if ((fl & text_endalign) != 0) P("/e");
        if ((fl & text_followon) != 0) P("/F");
        if ((fl & text_middle) != 0) P("/m");
        if ((fl & text_ringed) != 0) P("/ring");
        if ((fl & text_timealign) != 0) P("/ts");

        if (txt->rotate != 0) P("/rot%s", sff(txt->rotate));
        if (txt->halfway != 0) P("/h%s", sff(txt->halfway));
        if (txt->offset > 0) P("/rc%s", sff(txt->offset));
          else if (txt->offset < 0) P("/lc%s", sff(-txt->offset));
        write_move(txt->x, yvalue);
        P(" ");
        break;

        /* -------- Tuplet start -------- */

        case b_plet:
        b_pletstr *ps = (b_pletstr *)b;
        uint32_t pf = ps->flags;
        P("{");
        if (ps->pletnum != 2 || ps->pletlen != 3)
          P("%d/%d", ps->pletnum, ps->pletlen);

        /* Vertical position options */

        if ((pf & plet_a) != 0)
          {
          P("/a");
          if ((pf & plet_abs) != 0)
            {
            if (ps->yleft == ps->yright)
              P("%s", sff(ps->yleft));
            else if (ps->yleft > ps->yright)
              P("%s/lu%s", sff(ps->yright), sff(ps->yleft - ps->yright));
            else
              P("%s/ru%s", sff(ps->yleft), sff(ps->yright - ps->yleft));
            }
          else
            {
            if (ps->yleft == ps->yright)
              {
              if (ps->yleft > 0) P("/u%s", sff(ps->yleft));
                else if (ps->yleft < 0) P("/d%s", sff(-ps->yleft));
              }
            else
              {
              if (ps->yleft > 0) P("/lu%s", sff(ps->yleft));
                else if (ps->yleft < 0) P("/ld%s", sff(-ps->yleft));
              if (ps->yright > 0) P("/ru%s", sff(ps->yright));
                else if (ps->yright < 0) P("/rd%s", sff(-ps->yright));
              }
            }
          }

        else if ((pf & plet_b) != 0)
          {
          P("/b");
          if ((pf & plet_abs) != 0)
            {
            if (ps->yleft == ps->yright)
              P("%s", sff(ps->yleft));
            else if (ps->yleft > ps->yright)
              P("%s/ld%s", sff(ps->yright), sff(ps->yleft - ps->yright));
            else
              P("%s/rd%s", sff(ps->yleft), sff(ps->yright - ps->yleft));
            }
          else
            {
            if (ps->yleft == ps->yright)
              {
              if (ps->yleft > 0) P("/u%s", sff(ps->yleft));
                else if (ps->yleft < 0) P("/d%s", sff(-ps->yleft));
              }
            else
              {
              if (ps->yleft > 0) P("/lu%s", sff(ps->yleft));
                else if (ps->yleft < 0) P("/ld%s", sff(-ps->yleft));
              if (ps->yright > 0) P("/ru%s", sff(ps->yright));
                else if (ps->yright < 0) P("/rd%s", sff(-ps->yright));
              }
            }
          }

        /* Horizontal position options */

        if (ps->x > 0) P("/r%s", sff(ps->x));
          else if (ps->x < 0) P("/l%s", sff(-ps->x));

        /* Other options */

        if ((pf & plet_bn) != 0) P("/n");
        if ((pf & plet_x) != 0) P("/x");
        if ((pf & plet_lx) != 0) P("/lx");
        if ((pf & plet_rx) != 0) P("/rx");

        P(" ");
        break;

        /* -------- Slur or line start -------- */

        case b_slur:
        b_slurstr *bs = (b_slurstr *)b;

        P("[%s%s", ((bs->flags & sflag_x) != 0)? "x" : "",
          ((bs->flags & sflag_l) != 0)? "line" : "slur");

        /* Basic options */

        if (bs->id != 0) P("/=%c", bs->id);

        if ((bs->flags & sflag_b) == 0)  /* Slur above */
          {
          if ((bs->flags & sflag_lay) != 0) P("/ao");
            else if ((bs->flags & sflag_abs) != 0) P("/a%s", sff(bs->ally));
          }

        else  /* slur below */
          {
          P("/b");
          if ((bs->flags & sflag_lay) != 0) P("u");
            else if ((bs->flags & sflag_abs) != 0) P("%s", sff(-bs->ally));
          }

        if ((bs->flags & sflag_abs) == 0) write_move(0, bs->ally);

        if ((bs->flags & sflag_e) != 0) P("/e");
        if ((bs->flags & sflag_h) != 0) P("/h");
        if ((bs->flags & sflag_idot) != 0) P("/ip");
          else if ((bs->flags & sflag_i) != 0) P("/i");
        if ((bs->flags & sflag_w) != 0) P("/w");
        if ((bs->flags & sflag_ol) != 0) P("/ol");
        if ((bs->flags & sflag_or) != 0) P("/or");
        if ((bs->flags & sflag_cx) != 0) P("/cx");

        /* Loop for modifications that might apply to different parts of a
        split slur. */

        for (b_slurmodstr *sm = bs->mods; sm != NULL; sm = sm->next)
          {
          if (sm->sequence != 0) P("/%d", sm->sequence);
          if (sm->lxoffset > 0) P("/lrc%s", sff(sm->lxoffset));
            else if (sm->lxoffset < 0) P("/llc%s", sff(-sm->lxoffset));
          if (sm->rxoffset > 0) P("/rrc%s", sff(sm->rxoffset));
            else if (sm->rxoffset < 0) P("/rlc%s", sff(-sm->rxoffset));

          write_move_opt(sm->lx, sm->ly, "/lr", "/ll", "/lu", "/ld");
          write_move_opt(sm->rx, sm->ry, "/rr", "/rl", "/ru", "/rd");

          write_move_opt(sm->clx, sm->cly, "/clr", "/cll", "/clu", "/cld");
          write_move_opt(sm->crx, sm->cry, "/crr", "/crl", "/cru", "/crd");

          write_move_opt(sm->c, 0, "/co", "/ci", "", "");
          }

        P("] ");
        break;

        /* -------- Notes and chords -------- */

        case b_chord:
        case b_note:
        char options[100];
        char *cp = options;
        b_notestr *n = (b_notestr *)b;
        int octvar = n->abspitch/24 - octave + clefadjust;
        uint32_t acflags = n->acflags;

        /* Change cue only on single notes and the first notes of chords. */

        if (b->type == b_note)  /* Not 2nd or subsequent note of a chord */
          {
          bar_noteslength += n->length; 
          if ((n->flags & nf_cuesize) != 0)
            {
            if (!incue)
              {
              P("[cue] ");
              incue = TRUE;
              }
            }
          else if (incue)
            {
            P("[endcue] ");
            incue = FALSE;
            }
          }

        /* Handle start of chord. In PMW input, accents must be on the first
        note of a chord, but in the processed data they are moved to the
        non-stem end, so we have to deal with that. */

        if (b->type == b_note && (n->flags & nf_chord) != 0)
          {
          P("(");
          chordcount = 1;
          for (barstr *bb = (barstr *)b->next; bb != NULL;
               bb = (barstr *)bb->next)
            {
            if (bb->type != b_chord) break;
            acflags |= ((b_notestr *)bb)->acflags & (af_accents | af_opposite);
            chordcount++;
            }
          }
        else if (b->type == b_chord) acflags = 0;

        /* Handle an individual note */

        if (n->acc != ac_no)
          {
          P("%s", acnames[n->acc]);
          if ((n->flags & nf_accleft) != 0)
            {
            int32_t accleft = n->accleft - m->accspacing[n->acc] +
              m->accadjusts[n->notetype];
            P("<");
            if (accleft != 5000) P("%s", sff(accleft));
            }
          if ((n->flags & nf_accinvis) != 0) P("?");
            else if ((n->flags & nf_accrbra) != 0) P(")");
              else if ((n->flags & nf_accsbra) != 0) P("]");
          }

        /* Because we have calculated the octave from the absolute pitch, we
        must adjust the octvar value if an accidental has moved the original
        absolute pitch over an octave boundary, as the octave setting applies
        to the note without an accidental. */

        if (tolower(n->char_orig) == 'c' && (n->abspitch % 24) > 20) octvar++;
          else if (tolower(n->char_orig) == 'b' && (n->abspitch % 24) < 3)
            octvar--;

        /* XML input has rests as Z (can't remember why) */

        if (n->char_orig == 'Z' || n->char_orig == 'z') n->char_orig = 'r';
        if (n->notetype < crotchet) P("%c", toupper(n->char_orig));
          else P("%c", tolower(n->char_orig));

        if (n->spitch != 0)
          {
          if (octvar > 0) while (octvar-- > 0) P("'");
            else while (octvar++ < 0) P("`");
          }

        switch (n->notetype)
          {
          case semibreve:
          P("%s", (n->spitch == 0 && (n->flags & nf_centre) != 0)? "!":"+");
          break;
          case breve:     P("++"); break;
          case quaver:    P("-");  break;
          case squaver:   P("=");  break;
          case dsquaver:  P("=-"); break;
          case hdsquaver: P("=="); break;
          }

        if (n->dots_orig == 255) P(".+");
          else for (int i = 0; i < n->dots_orig; i++) P(".");

        /* Handle options */

        options[0] = 0;

        /* A notional stem direction is needed for all notes. Apart from
        anything else, it affects the sorting order. PMW assumes stem up for
        all grace notes that do not have an explicit stem-down option.
        Therefore, always set an explicit stem down for grace notes. */

        if (n->spitch != 0)
          {
          if (n->length == 0)  /* Grace note */
            {
            cp += sprintf(cp, " g%s", ((n->flags & nf_appogg) == 0)? "":"/");
            if ((n->flags & nf_stemup) == 0) cp += sprintf(cp, " sd");
            }

          /* Not a grace note */

          else if ((n->flags & nf_stemup) != 0)
            {
            if (stemdirection < 0 || (stemdirection == 0 && n->spitch >= P_3L))
              cp += sprintf(cp, " su");
            }
          else
            {
            if (stemdirection > 0 || (stemdirection == 0 && n->spitch <= P_3L))
              cp += sprintf(cp, " sd");
            }
          }

        /* Accents */

        while (acflags != 0)
          {
          for (accent *a = accent_chars; a->flag > 255; a++)
            {
            if ((acflags & a->flag) != 0)
              {
              cp += sprintf(cp, " %s", a->string);
              acflags &= ~a->flag;
              break;
              }
            }
          }

        /* Ornaments */

        if (ornament != NULL)
          {
          for (accent *a = accent_chars; a->string != NULL; a++)
            {
            if (a->flag == ornament->ornament)
              {
              cp += sprintf(cp, " %s", a->string);
              switch(ornament->bflags & (orn_rbra|orn_rket|orn_sbra|orn_sket))
                {
                case orn_rbra|orn_rket: cp += sprintf(cp, "/b"); break;
                case orn_sbra|orn_sket: cp += sprintf(cp, "/B"); break;
                case orn_rbra: cp += sprintf(cp, "/("); break;
                case orn_sbra: cp += sprintf(cp, "/["); break;
                case orn_rket: cp += sprintf(cp, "/)"); break;
                case orn_sket: cp += sprintf(cp, "/]"); break;
                }
              cp = format_move(ornament->x, ornament->y, cp);
              break;
              }
            }
          ornament = NULL;
          }

        /* Masquerade */

        if (n->masq != MASQ_UNSET)
          {
          cp += sprintf(cp, " %c", (n->masq < crotchet)? 'M':'m');
          switch (n->masq)
            {
            case breve:     cp += sprintf(cp, "++"); break;
            case semibreve: cp += sprintf(cp, "+");  break;
            case quaver:    cp += sprintf(cp, "-");  break;
            case squaver:   cp += sprintf(cp, "=");  break;
            case dsquaver:  cp += sprintf(cp, "=-"); break;
            case hdsquaver: cp += sprintf(cp, "=="); break;
            }
          if (n->dots == 255) cp += sprintf(cp, ".+");
            else for (int i = 0; i < n->dots; i++) cp += sprintf(cp, ".");
          }

        /* Notehead style */

        switch (n->noteheadstyle)
          {
          case nh_normal: break;
          case nh_cross: cp += sprintf(cp, " nx"); break;
          case nh_harmonic: cp += sprintf(cp, " nh"); break;
          case nh_none: cp += sprintf(cp, " nz"); break;
          case nh_direct: cp += sprintf(cp, " nd"); break;
          case nh_circular: cp += sprintf(cp, " nc"); break;
          }

        /* Handle note with no stem */

        if (n->spitch != 0 && (n->flags & nf_stem) == 0 && n->notetype >= minim)
          cp += sprintf(cp, " no");

        /* Stem length adjustment */

        if (n->yextra != 0) cp += sprintf(cp, " sl%s", sff(n->yextra));
        
        /* Check for explicit coupling needed */
        
        if ((n->flags & nf_couple) != 0)  /* Note is coupled */
          {
          if (((n->flags & nf_coupleU) != 0 && couple_state != +1) ||
              ((n->flags & nf_coupleD) != 0 && couple_state != -1))
            cp += sprintf(cp, " c");    
          }   

        /* Output any options that have been set. */

        if (options[0] != 0) P("\\%s\\", options + 1);
        if (--chordcount == 0) P(")");
        wasnote = TRUE;
        break;  /* End of note handling */



/* As yet unsupported:

  b_accentmove, b_barnum, b_beamacc, b_beammove,
  b_beamrit, b_beamslope, b_bowing, b_breakbarline,
  b_dotbar, b_dotright, b_draw, b_endslur,
  b_ens, b_ensure, b_footnote, b_justify, b_linegap,
  b_midichange, b_name, b_newpage,
  b_noteheads, b_notes, b_ns, b_nsm, b_olevel, b_olhere,
  b_overbeam, b_page, b_pagebotmargin, b_pagetopmargin,
  b_slurgap,
  b_transpose, b_tripsw, b_ulevel, b_ulhere,
  b_unbreakbarline, b_zerocopy,
*/

        }

      b = (barstr *)b->next;
      if (b == NULL && ++barno < st->barcount) b = barvector[barno];
      }

    P("[endstave]\n");
    }    /* End of loop through the staves */
  }      /* End of loop through the movements */

P("\n@ End\n");
if (fclose(pmw_file) != 0) error(ERR200, "PMW file", strerror(errno));
}

/* End of pmwout.c */
