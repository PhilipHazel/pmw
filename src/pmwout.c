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
*             Write PMW string                   *
*************************************************/

static void
write_pmw_string(uint32_t *s, const char *lead)
{
uint32_t settype = 0xffffffffu;

P("%s\"", lead);
for (; *s != 0; s++)
  {
  int i;
  uschar buffer[8];
  uint32_t type = PFONT(*s);
  if (type != settype)
    {
    P("\\%s\\", font_IdStrings[type]);
    settype = type;
    }
  i = misc_ord2utf8(PCHAR(*s), buffer);
  for (int j = 0; j < i; j++) P("%c", buffer[j]);
  }
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
*              Output stave list                 *
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

  /* --- Bracket --- */

  if (m->bracketlist != p->bracketlist)
    write_stavelist("bracket", m->bracketlist);

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
    write_pmw_string(m->trillstring, " ");
    P("\n");
    }

  /* --- End of font settings --- */




  /* Now the staves */

  for (int stave = 0; stave <= m->laststave; stave++)
    {
    st = m->stavetable[stave];
    if (st->barcount == 0) continue;

    barstr **barvector = st->barindex;

    P("\n[stave %d]\n", stave);


    P("[endstave]\n");
    }    /* End of loop through the staves */
  }

P("\n@ End\n");
if (fclose(pmw_file) != 0) error(ERR200, "PMW file", strerror(errno));
}

/* End of pmwout.c */
