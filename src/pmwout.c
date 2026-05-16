/*************************************************
*            PMW source output generation        *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: April 2026 */
/* This file last modified: May 2026 */

#include "pmw.h"

static FILE      *pmw_file;

static int accspacingorder[] = { ac_ds, ac_fl, ac_df, ac_nt, ac_sh, ac_no };

static const char *fonttype_names[] = {
  "roman", "italic", "bold", "bolditalic", "symbol", "music"};

/* This vector is used for a reverse Unicode translation table, which is
constructed the first time it is needed. It does not need to be reset for a new
movement. */

static uint32_t   unihigh[50] = { 0 };

/* Type for tables of fields in the movement structure. */

typedef struct movtfield {
  const char *name;
  size_t offset;
} movtfield;

/* Table of signed 32-bit dimension values in the movement structure and the
directives that are used to set them. */

static movtfield movt32dimfields[] = {
  { "barlinesize",       offsetof(movtstr, barlinesize) },
  { "barlinespace",      offsetof(movtstr, barlinespace) },
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
  { "overlsydepth",      offsetof(movtstr, overlaydepth) },
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
  { "underlaystyle",       offsetof(movtstr, underlaystyle) }
};

#define movtu8numfieldscount (sizeof(movtu8numfields)/sizeof(movtfield))




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
char datebuff[100];

TRACE("outpmw_write()\n");

pmw_file = Ufopen(outpmw_filename, "w");
if (pmw_file == NULL) error(ERR23, outpmw_filename, strerror(errno));  /* Hard */
if (main_verify) eprintf("Writing PMW source file \"%s\"\n", outpmw_filename);

now = time(NULL);
strftime(datebuff, sizeof(datebuff), "%Y-%m-%d", localtime(&now));
P("@ Generated by PMW %s %s\n\n", PMW_VERSION, datebuff);

for (int movt = 0; movt < (int)movement_count; movt++)
  {
  BOOL same;
  movtstr *p;
  movtstr *m = movements[movt];

  /* Header stuff. Output only if there's a change from a default value (1st
  movement) or the previous movement (subsequent movements). */

  if (movt == 0)
    {
    p = &default_movtstr;
    }
  else
    {
    p = movements[movt - 1];
    P("\n\n[newmovement]\n");
    }

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

  /* --- Brace --- */

  if (m->bracelist != p->bracelist)
    write_stavelist("brace", m->bracelist);

  /* --- Bracket and ThinBracket--- */

  if (m->bracketlist != p->bracketlist)
    write_stavelist("bracket", m->bracketlist);
  if (m->thinbracketlist != p->thinbracketlist)
    write_stavelist("thinbracket", m->thinbracketlist);

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

  /* --- Font sizes and types --- */

  write_diff_font(TRUE, "tupletfont", 1,
    &(m->fontsizes->fontsize_triplet), &(p->fontsizes->fontsize_triplet),
    m->fonttype_triplet, p->fonttype_triplet);

  write_diff_font(TRUE, "repeatbarfont", 1,
    &(m->fontsizes->fontsize_repno), &(p->fontsizes->fontsize_repno),
    m->fonttype_repeatbar, p->fonttype_repeatbar);

  write_diff_font(TRUE, "longrestfont", 1,
    &(m->fontsizes->fontsize_restct), &(p->fontsizes->fontsize_restct),
    m->fonttype_longrest, p->fonttype_longrest);

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

  /* ---- Breakbarlines ---- */

  if (m->breakbarlines != p->breakbarlines)
    write_stavebits(m->breakbarlines, "breakbarlines");

  /* ---- Selectstaves ---- */

  if (m->select_staves != p->select_staves)
    write_stavebits(m->select_staves, "selectstaves");

  /* ---- Suspend ---- */

  if (m->suspend_staves != p->suspend_staves)
    write_stavebits(m->suspend_staves, "suspend");

  /* ---- Bar ---- */

  if (m->baroffset != 0) P("bar %d\n", m->baroffset + 1);

  /* ---- Transpose ---- */

  if (m->transpose != 0) P("transpose %d\n", m->transpose/2);




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




  /* Now the staves */

  for (int stave = 0; stave <= m->laststave; stave++)
    {
    st = m->stavetable[stave];
    if (st->barcount == 0) continue;

//    barstr **barvector = st->barindex;

    P("\n[stave %d]\n", stave);


    P("[endstave]\n");
    }    /* End of loop through the staves */
  }      /* End of loop through the movements */

P("\n@ End\n");
if (fclose(pmw_file) != 0) error(ERR200, "PMW file", strerror(errno));
}

/* End of pmwout.c */
