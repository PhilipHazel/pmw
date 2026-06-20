/*************************************************
*            PMW source output generation        *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: April 2026 */
/* This file last modified: June 2026 */

#include "pmw.h"

static FILE      *pmw_file;

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
  { "barnumberlevel",    offsetof(movtstr, barnumber_level) },
  { "dotspacefactor",    offsetof(movtstr, dotspacefactor) },
  { "endlinesluradjust", offsetof(movtstr, endlinesluradjust) },
  { "endlinetieadjust",  offsetof(movtstr, endlinetieadjust) },
  { "extenderlevel",     offsetof(movtstr, extenderlevel) },
  { "footnotesep",       offsetof(movtstr, footnotesep) },
  { "hyphenthreshold",   offsetof(movtstr, hyphenthreshold) },
  { "leftmargin",        offsetof(movtstr, leftmargin) },
  { "linelength",        offsetof(movtstr, linelength) },
  { "midkeyspacing",     offsetof(movtstr, midkeyspacing) },
  { "midtimespacing",    offsetof(movtstr, midtimespacing) },
  { "overlaydepth",      offsetof(movtstr, overlaydepth) },
  { "shortenstems",      offsetof(movtstr, shortenstems) },
  { "underlsydepth",     offsetof(movtstr, underlaydepth) }
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
  { "startbracketbar",   offsetof(movtstr, startbracketbar) },
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
va_list ap;
va_start(ap, format);
Vvfprintf(pmw_file, format, ap);
va_end(ap);
}



/*************************************************
*              Write bar number                  *
*************************************************/

static void
write_barnumber(uint32_t bar, const char *lead)
{
uint32_t n = bar >> 16;
uint32_t f = bar & 0xffffu;
P("%s%d", lead, n);
if (f != 0) P(".%d", f);
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
*               Format xy movement               *
*************************************************/

static char *
format_move(int32_t x, int32_t y, char *cp)
{
if (x > 0) cp += sprintf(cp, "/r%s", sff(x));
  else if (x < 0) cp += sprintf(cp, "/l%s", sff(-x));
if (y > 0) cp += sprintf(cp, "/u%s", sff(y));
  else if (y < 0) cp += sprintf(cp, "/d%s", sff(-y));
return cp;
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
    P("\\%s\\", font_IdStrings[type]);
    settype = type;
    }

  /* Handle special characters above the Unicode limit. */

  if (c > MAX_UNICODE)
    {
    switch(c)
      {
      case ss_verticalbar:   c = '|'; break;
      case ss_asciiquote:    c = '\''; break;
      case ss_asciigrave:    c = '`'; break;
      case ss_escapedhyphen: c = '-'; break;
      case ss_escapedequals: c = '='; break;
      case ss_escapedsharp:  c = '#'; break;

      default:
      error(ERR191, "Unknown special character in string");
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
int bit = 1;
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
uint32_t settype = font_rm;

for (; p != NULL; p = p->next)
  {
  if (p->drawing == NULL)  /* Text heading */
    {
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

now = time(NULL);
strftime(datebuff, sizeof(datebuff), "%Y-%m-%d", localtime(&now));
P("@ Generated by PMW %s %s\n\n", PMW_VERSION, datebuff);

/* Always set no check because there's no way of identifying it from individual
bars. */

P("nocheck\n");

/* Global settings that are allowed only in the first movement. */

if (main_magnification != 1000)
  P("magnification %s\n", sff(main_magnification));
if (main_righttoleft) P("righttoleft\n");
if (main_sheetdepth != DEFAULT_SHEETDEPTH)
  P("sheetdepth %s\n", sff(main_sheetdepth));
if (main_sheetwidth != DEFAULT_SHEETWIDTH)
  P("sheetwidth %s\n", sff(main_sheetwidth));
if (main_pagelength != DEFAULT_PAGELENGTH)
  P("pagelength %s\n", sff(main_pagelength));
if (main_landscape) P("landscape\n");
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
    P("\n\n[newmovement]\n");
    }


  /* These directives do not carry over between movements: bar, doublenotes,
  halvenotes, key, layout, notime, startbracketbar, startnotime, suspend, time,
  transpose, unfinished. */

  /* ---- Bar ---- */

  if (m->baroffset != 0) P("bar %d\n", m->baroffset + 1);

  /* ---- Key ---- */

  P("key %s\n", string_format_key(m->key));

  /* ---- Time ---- */

  P("time");
  if (mts > 0x0001ffffu) P(" %d*", mts >> 16);
  mts &= 0x0000ffffu;
  if (mts == time_common) P(" C");
  else if (mts == time_cut) P(" A");
  else P(" %d/%d", (mts >> 8) & 0xff, mts & 0xff);
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

  if (m->breakbarlines != p->breakbarlines ||
      (m->flags & mf_fullbarend) != (p->flags & mf_fullbarend))
    write_stavebits(m->breakbarlines,
      ((m->flags & mf_fullbarend) == 0)? "breakbarlines" : "breakbarlinesx");

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

    write_pmw_string(pt->top, " ", font_bf);
    if (pt->sizetop != AllFontSizes)
      {
      if (pt->sizetop > UserFontSizes)
        P("/S%d", pt->sizetop - UserFontSizes);
      else
        P("/s%d", pt->sizetop + 1);
      }

    if (pt->bot[0] != 0)
      {
      write_pmw_string(pt->bot, " ", font_bf);
      if (pt->sizetop != AllFontSizes)
        {
        if (pt->sizebot > UserFontSizes)
          P("/S%d", pt->sizebot - UserFontSizes);
        else
          P("/s%d", pt->sizebot + 1);
        }
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

  if (memcmp(m->fontsizes->fontsize_text, p->fontsizes->fontsize_text,
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
    barstr **barvector, *b;
    b_ornamentstr *ornament = NULL;

    st = m->stavetable[stave];
    if (st->barcount == 0) continue;

    P("\n[stave %d]\n", stave);

    /* First make a pass through the stave to collect information such as the
    numbers of notes with various stem conditions, so that certain defaults can
    be set. */

    memset(octave_counts, 0, sizeof(octave_counts));
    barvector = st->barindex;
    barno = 0;
    b = barvector[barno];

    while (b != NULL)
      {
      if (b->type == b_note)
        {
        b_notestr *n = (b_notestr *)b;
        if (n->spitch != 0)             /* Not a rest */
          {
          note_total++;
          octave_counts[n->abspitch/24] += 1;
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
          }
        }

      b = (barstr *)b->next;
      if (b == NULL && ++barno < st->barcount) b = barvector[barno];
      }

    /* Select the octave with the most notes */

    for (int i = 0; i < 8; i++)
      if (octave_counts[i] > max_octave_count)
        {
        max_octave_count= octave_counts[i];
        octave = i;
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

    /* Now process the stave's data */

    barno = 0;
    b = barvector[barno];

    while (b != NULL)
      {
      switch (b->type)
        {
        case b_barline:
        write_barnumber(m->barvector[barno], " | @");
        P("\n");
        break;

        case b_clef:

        break;

        case b_ornament:
        ornament = (b_ornamentstr *)b;
        break;

        case b_chord:
        case b_note:
        char options[100];
        char *cp = options;
        b_notestr *n = (b_notestr *)b;
        int octvar = n->abspitch/24 - octave;
        uint32_t acflags = n->acflags;

        /* Handle start of chord */

        if (b->type == b_note && (n->flags & nf_chord) != 0)
          {
          P("(");
          chordcount = 1;
          for (barstr *bb = (barstr *)b->next; bb != NULL;
               bb = (barstr *)bb->next)
            {
            if (bb->type != b_chord) break;
            chordcount++;
            }
          }

        /* Handle individual note */

        if (n->acc != ac_no)
          {
          P("%s", acnames[n->acc]);
          if ((n->flags & nf_accinvis) != 0) P("?");
            else if ((n->flags & nf_accrbra) != 0) P(")");
              else if ((n->flags & nf_accsbra) != 0) P("]");

          if ((n->flags & nf_accleft) != 0)
            {
            int32_t accleft = n->accleft - m->accspacing[n->acc] +
              m->accadjusts[n->notetype];
            P("<");
            if (accleft != 5000) P("%s", sff(accleft));
            }
          }

        if (n->notetype < crotchet) P("%c", toupper(n->char_orig));
          else P("%c", n->char_orig);
        if (n->spitch != 0)
          {
          if (octvar > 0) while (octvar-- > 0) P("'");
            else while (octvar++ < 0) P("`");
          }

        switch (n->notetype)
          {
          case breve:     P("++"); break;
          case semibreve: P("+");  break;
          case quaver:    P("-");  break;
          case squaver:   P("=");  break;
          case dsquaver:  P("=-"); break;
          case hdsquaver: P("=="); break;
          }

        if (n->dots_orig == 255) P(".+");
          else for (int i = 0; i < n->dots_orig; i++) P(".");

        /* Handle options */

        options[0] = 0;

        /* In auto mode, this takes no special action for notes at the stem
        swap level. */

        if ((n->flags & nf_stem) != 0)
          {
          if ((n->flags & nf_stemup) != 0)
            {
            if (stemdirection < 0 || (stemdirection == 0 && n->spitch > P_3L))
              cp += sprintf(cp, " su");
            }
          else
            {
            if (stemdirection > 0 || (stemdirection == 0 && n->spitch < P_3L))
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

        /* Output any options that have been set. */

        if (options[0] != 0) P("\\%s\\", options + 1);
        if (--chordcount == 0) P(")");
        P(" ");
        break;  /* End of note handling */


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
