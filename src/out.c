/*************************************************
*        PMW main output control functions       *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: May 2021 */
/* This file last modified: January 2025 */

#include "pmw.h"


/* Clef style can currently take values 0, 1, 2, or 3; the columns below
correspond to this. */

static usint clef_chars[] = {
  mc_altoclef,      mc_altoclef,         mc_oldaltoclef,    mc_oldaltoclef,
  mc_baritoneclef,  mc_oldbaritoneclef,  mc_baritoneclef,   mc_oldbaritoneclef,
  mc_bassclef,      mc_oldbassclef,      mc_bassclef,       mc_oldbassclef,
  mc_cbaritoneclef, mc_oldcbaritoneclef, mc_cbaritoneclef,  mc_oldcbaritoneclef,
  mc_Cbassclef,     mc_oldCbassclef,     mc_Cbassclef,      mc_oldCbassclef,
  mc_deepbassclef,  mc_olddeepbassclef,  mc_deepbassclef,   mc_olddeepbassclef,
  mc_hclef,         mc_hclef,            mc_hclef,          mc_hclef,
  mc_mezzoclef,     mc_mezzoclef,        mc_oldmezzoclef,   mc_oldmezzoclef,
  0,                0,                   0,                 0,
  mc_Sbassclef,     mc_oldSbassclef,     mc_Sbassclef,      mc_oldSbassclef,
  mc_sopranoclef,   mc_sopranoclef,      mc_oldsopranoclef, mc_oldsopranoclef,
  mc_tenorclef,     mc_tenorclef,        mc_oldtenorclef,   mc_oldtenorclef,
  mc_trebleclef,    mc_trebleclef,       mc_trebleclef,     mc_trebleclef,
  mc_trebleDclef,   mc_trebleDclef,      mc_trebleDclef,    mc_trebleDclef,
  mc_trebleTclef,   mc_trebleTclef,      mc_trebleTclef,    mc_trebleTclef,
  mc_trebleTBclef,  mc_trebleTBclef,     mc_trebleTBclef,   mc_trebleTBclef
};

/* These are vertical adjustments, in points, for positioning the musical
characters on the stave. */

static uint8_t clef_adjusts[] = {
  8,   /* Alto */
  12,  /* Baritone */
  12,  /* Bass */
  16,  /* Cbaritone */
  12,  /* Contrabass */
  16,  /* Deepbass */
  0,   /* Hclef */
  4,   /* Mezzo */
  0,   /* None */
  12,  /* Soprabass */
  0,   /* Soprano */
  12,  /* Tenor */
  4,   /* Treble */
  4,   /* Trebledescant */
  4,   /* Trebletenor */
  4 }; /* TrebletenorB */

/* Virtual accidental characters in the music font. This table sets the default
half accidentals - for the alternate set there is a check in the code. */

static uint8_t ac_chars[] = {
  0, mc_natural,
  mc_hsharp1, mc_sharp, mc_dsharp,
  mc_hflat1, mc_flat, mc_dflat };

/* Underlay extension string */

static uint32_t extension[] = { (font_rm << 24) | '_', 0 };



/************************************************
*            Static variables                   *
************************************************/

static fontinststr out_fdata1 = { NULL, 0, 0 };
static fontinststr out_fdata2 = { NULL, 0, 0 };

static int32_t out_joinxposition;




/*************************************************
*           Output vocal underlay extension      *
*************************************************/

/* Currently, an underline in the roman font is forced. There is a global flag
that suppresses extenders.

Arguments:
  x0          x coordinate of start of extension
  x1          x coordinate of end of extension
  y           y coordinate of the extension
  fdata       font size etc

Returns:      nothing
*/

void
out_extension(int32_t x0, int32_t x1, int32_t y, fontinststr *fdata)
{
int32_t uwidth;
int32_t length;
int count, remain;
uint32_t s[256];

if (!MFLAG(mf_underlayextenders)) return;

length = x1 - x0;
uwidth = string_width(extension, fdata, NULL);
count = length/uwidth;
remain = length - count*uwidth;

if (count <= 0)
  {
  if (length > (uwidth*3)/4) { count = 1; remain = 0; } else return;
  }

y = out_ystave - ((y + curmovt->extenderlevel) * out_stavemagn)/1000;
x0 += out_stavemagn;

for (int i = 0; i < count; i++) s[i] = extension[0];
s[count] = 0;
ofi_string(s, fdata, &x0, &y, FALSE);

/* Deal with a final part-line */

if (remain >= uwidth/5)
  {
  x1 -= uwidth;
  ofi_string(extension, fdata, &x1, &y, FALSE);
  }
}


/*************************************************
*           Output vocal underlay hyphens        *
*************************************************/

/* This is very heuristic. If the width is very small, output nothing. If less
than the threshold, output a single, centred hyphen. Otherwise output hyphens
spaced at 1/3 of the threshold, centred in the space. Take care not to have
spaces that are too small at the ends. Ensure that a continuation hyphen at the
start of a line is always printed, moving it left if necessary.

Arguments:
  x0          x coordinate of start of extension
  x1          x coordinate of end of extension
  y           y coordinate of the extension
  fdata       font size etc
  contflag    TRUE if continuing from the previous line

Returns:      nothing
*/

void
out_hyphens(int32_t x0, int32_t x1, int32_t y, fontinststr *fdata, BOOL contflag)
{
uint32_t *hyphen = curmovt->hyphenstring;
int32_t unit = curmovt->hyphenthreshold/3;
int32_t hwidth = string_width(hyphen, fdata, NULL);
int32_t width = x1 - x0;
int32_t minwidth = 800 + (hwidth - string_width(default_hyphen, fdata, NULL));

TRACE("out_hyphens() start\n");

if (contflag) minwidth += 3200;

if (width < minwidth)
  {
  if (!contflag) return;
  width = minwidth;
  x0 = x1 - width;
  }

y = out_ystave - (y * out_stavemagn)/1000;

/* Deal with the case when the width is less than the threshold */

if (width < curmovt->hyphenthreshold)
  {
  if (contflag) out_string(hyphen, fdata, x0, y, 0);
  if (!contflag || width > unit)
    {
    out_string(hyphen, fdata, (x0 + x1 - hwidth)/2, y, 0);
    }
  }

/* Deal with widths greater than the threshold */

else
  {
  int32_t count = width/unit;      /* count is the number of gaps */

  if (width - count*unit < unit/2) count -= 2; else count--;
  if (width - count*unit > unit) unit += unit/(3*count);

  if (contflag)
    {
    if (width - count*unit > (3*unit)/2) count++;
    }
  else x0 += (width - count*unit - hwidth)/2;

  /* We have special code that generates a "widthshow" command to do the whole
  thing with one string. However, we can do this only if all characters are in
  the same font. For the moment, just do it if the hyphen string is a single
  character. */

  if (PCHAR(hyphen[1]) == 0)
    {
    uint32_t s[256];
    uint32_t *pp = s;
    uint32_t hchar = hyphen[0];
    uint32_t space = PFTOP(hchar) | ' ';
    uint32_t swidth = font_charwidth(' ', 0, PFONT(hchar), fdata->size, NULL);
    fontinststr local_fdata = *fdata;

    while (count-- >= 0)
      {
      *pp++ = hchar;
      *pp++ = space;
      }
    *pp = 0;

    local_fdata.spacestretch = unit - hwidth - swidth;
    ofi_string(s, &local_fdata, &x0, &y, FALSE);
    }

  /* Otherwise we have to output each hyphen individually */

  else for (int i = 0; i <= count; i++)
    {
    ofi_string(hyphen, fdata, &x0, &y, FALSE);
    x0 += unit;
    }
  }

TRACE("out_hyphens() end\n");
}



/*************************************************
*           Output repeating string              *
*************************************************/

/* This function fills up the space with repeat copies of the string from the
htypestr. These are customized hyphen fillers, specified as extra strings for
"underlay" or "overlay" that is in reality something else, such as a 8va
marking.

Arguments:
  x0          x coordinate of start of extension
  x1          x coordinate of end of extension
  y           y coordinate of the extension
  contflag    TRUE if continuing from the previous line
  eolflag     TRUE if continuing on to the next line
  htype       the number of the htypestr that has the relevant strings

Returns:      nothing
*/

void
out_repeatstring(int32_t x0, int32_t x1, int32_t y, BOOL contflag,
  BOOL eolflag, int htype)
{
htypestr *h = main_htypes;
int32_t width = x1 - x0;
int32_t swidth;

TRACE("out_repeatstring() start htype=%d\n", htype);

while (--htype > 0 && h != NULL) h = h->next;
if (h == NULL) error(ERR146);  /* Hard error */

/* Deal with special string at continuation line start */

if (contflag && h->string2 != NULL)
  {
  int32_t xw;
  out_fdata2.size = mac_muldiv(curmovt->fontsizes->fontsize_text[h->size2].size,
    out_stavemagn, 1000);
  xw = string_width(h->string2, &out_fdata2, NULL);
  out_string(h->string2, &out_fdata2, x0,
    out_ystave - (y * out_stavemagn)/1000, 0);
  width -= xw;
  x0 += xw;
  }

/* Set up size for main and terminating string. */

out_fdata1.size = mac_muldiv(curmovt->fontsizes->fontsize_text[h->size1].size,
  out_stavemagn, 1000);

/* Adjust width for special terminating string (but not at eol) */

if (!eolflag && h->string3 != NULL)
  width -= string_width(h->string3, &out_fdata1, NULL);

/* Handle main string */

swidth = string_width(h->string1, &out_fdata1, NULL);
if (swidth > 0)
  {
  int scount;
  int count = width/swidth;  /* Max needed for the width */
  int slen = 0;

  for (uint32_t *p = h->string1; *p != 0; p++) slen++;
  scount = 255/slen;         /* Max in 256 buffer, just to stop overflow */

  y = out_ystave - ((y + h->adjust) * out_stavemagn)/1000;

  /* Loop for the count, outputting whenever the buffer fills up. */

  while (count > 0)
    {
    uint32_t buff[256];
    uint32_t *p = buff;
    int32_t nx0 = x0;

    for (int i = 0; i < scount; i++)
      {
      memcpy(p, h->string1, slen * sizeof(uint32_t));
      p += slen;
      nx0 += swidth;
      if (--count <= 0) break;
      }
    *p = 0;

    out_string(buff, &out_fdata1, x0, y, 0);
    x0 = nx0;
    }
  }

/* Output special final string, but not at end of line */

if (!eolflag && h->string3 != NULL)
  out_string(h->string3, &out_fdata1, x0, y, 0);

TRACE("out_repeatstring() end\n");
}



/*************************************************
*            Handle a text item                  *
*************************************************/

/* This function is called after a note has been set up for output, so the
max & min pitches etc. are known. It must be called before out_moff is reset.
It is also called for text at the end of a bar, in which case "atbar" is true.

Arguments:
  p             the text item
  atbar         TRUE if at end of a bar

Returns:        nothing
*/

void
out_text(b_textstr *p, BOOL atbar)
{
fontinststr local_fdata, *fdata;

uint16_t flags = p->flags;
BOOL above = (flags & text_above) != 0;
BOOL endalign = (flags & text_endalign) != 0;
BOOL rehearse = (flags & text_rehearse) != 0;

int32_t stringwidth;
int32_t unscaled_fontsize;
int32_t six = 6*out_stavemagn;
int32_t x = n_x - out_Xadjustment;
int32_t y = above? 20000 : -10000;

uint32_t *s = p->string;
uint32_t ss[256];            /* For building continuation strings */

TRACE("out_text()%s\n", ((flags & text_ul) != 0)? " underlay" :"");

/* Make a local copy of the relevant font data, and keep a copy of the unscaled
size before scaling the font. */

local_fdata = rehearse? curmovt->fontsizes->fontsize_rehearse :
                        curmovt->fontsizes->fontsize_text[p->size];

unscaled_fontsize = local_fdata.size;
local_fdata.size = mac_muldiv(local_fdata.size, out_stavemagn, 1000);

/* Point working fdata at the local copy - this will change if there's
rotation. */

fdata = &local_fdata;

/* Find the string width before any rotation (used for centring and right-
aligning). */

stringwidth = string_width(s, fdata, NULL);

/* Handle special positioning. If the "halfway" value is set, adjust the x
coordinate if there is a note that follows in the bar. Otherwise, if a
note-related offset is set, adjust the x coordinate by looking for the relevant
musical offset. */

if (p->halfway != 0)
  {
  if (out_moff < out_poslast->moff)
    x += (endalign? 0 : ((flags & text_centre) != 0)? six/2 : six) +
      mac_muldiv(out_barx + out_findXoffset(out_moff + n_length) -
        n_x - six, p->halfway, 1000);
  }

/* The offset value units are crotchets. */

else if (p->offset != 0)
  x = out_barx + out_findAoffset(out_moff +
    mac_muldiv(len_crotchet, p->offset, 1000));

/* Handle rotation, but not for underlay or overlay */

if ((flags & text_ul) == 0 && p->rotate != 0)
  fdata = font_rotate(fdata, p->rotate);


/* ======================= Underlay and Overlay ======================= */

/* Underlay and overlay text is centred. If it ends in '=' the position is
remembered for starting the extender; if it ends in '-' the position is
remembered for starting the row of hyphens.

Note that an underlay/overlay string is represented as a pointer and a count,
not as a PMW string, because the pointer may point into the longer, original
input string.

If the string consists solely of '=' and an extender is outstanding, an
extender is drawn to the current position and the start extender position is
updated. If we are at the start of a line (indicated by a zero xstart value),
draw the line at the current level, not the saved one.

Note that # characters in underlay and overlay strings must be converted to
spaces. */

if ((flags & text_ul) != 0)
  {
  int i;
  uint32_t *pp, *qq, *cc;
  uolaystr **uu = &bar_cont->uolay;
  uolaystr *u = *uu;
  BOOL overlay = (flags & text_above) != 0;

  /* Find pending extension or hyphen data at the correct (verse) level. */

  while (u != NULL && (overlay != u->above || u->level != p->laylevel))
    {
    uu = &u->next;
    u = *uu;
    }

  /* Compute vertical level for this syllable */

  y = (!overlay)?
    out_sysblock->ulevel[curstave] - p->laylevel * curmovt->underlaydepth :
    out_sysblock->olevel[curstave] + p->laylevel * curmovt->overlaydepth;

  /* Deal with an extension line. There should always be an extension block,
  but we check, just in case. */

  if (PCHAR(s[0]) == '=' && p->laylen == 1)
    {
    if (u != NULL && u->type == '=')
      {
      int32_t xx, yy;
      if (u->x == 0)
        {
        xx = out_sysblock->firstnoteposition + out_sysblock->xjustify - 4000;
        yy = y;
        }
      else
        {
        xx = u->x;
        yy = u->y;
        }
      out_extension(xx, x + 5*out_stavemagn, yy, fdata);
      if (x > u->x) u->x = x;  /* Set up for next part */
      u->y = yy;
      }
    return;
    }

  /* Not an extension line - there is text to be output. Copy it into a working
  string, turning # characters into spaces, and stopping at the character '^',
  which indicates the end of the text to be centred, or the start of it, if
  there are two '^' characters in the string. In the latter case, find the
  length of the left hang, set the start of the centering bit, and read to the
  next '^'. */

  pp = s;
  cc = qq = ss;
  i = 0;

  for (int j = 0; j < 2; j++)
    {
    int k;
    uint32_t c;

    /* Search for next '^' in string while copying into a temporary buffer and
    turning # into space. */

    for (; i < p->laylen && (c = PCHAR(*pp)) != '^'; i++)
      *qq++ = (c == '#')? (' ' | PFTOP(*pp++)) : *pp++;
    *qq = 0;

    /* If hit end of string or the second '^', break */

    if (i >= p->laylen || j == 1) break;

    /* See if there's another '^' in the string; if not, break */

    for (k = i+1; k < p->laylen; k++) if (PCHAR(s[k]) == '^') break;
    if (k >= p->laylen) break;

    /* Left shift by the left hand width, adjust the start of the centred
    string, and continue on to the second '^'. */

    x -= string_width(ss, fdata, NULL);
    cc = qq;
    pp++;
    i++;
    }

  /* There are two underlay styles. In style 0, all syllables are centred, with
  ^ indicating the end of the centred text. In style 1, syllables that extend
  over more than one note are left-justified, unless they contain ^, which
  indicates centring. At present a single style applies to both underlay and
  overlay. */

  if (curmovt->underlaystyle == 0 ||
       (PCHAR(*pp) != '=' && (PCHAR(*pp) != '-' || PCHAR(pp[1]) != '=')))
    {
    int32_t xorig = x;
    x -= string_width(cc, fdata, NULL)/2;

    /* We have calculated a centring position for the string based on the
    normal position, which is the left-hand edge of the note. If in fact there
    has been no change to the position, (i.e. the centred part of the string
    has zero width), leave the position as it is, for left-alignment. If there
    has been a change, however, we must add 3 points to make the centring
    relative to the middle of the notehead. However, if this is a grace note in
    default format, always left-align. */

    if (x != xorig && (n_length != 0 || curmovt->gracestyle != 0))
      x += 3*out_stavemagn;

    /* Copy the rest of the string if stopped at '^'; else leave pp pointing at
    the following character. */

    if (PCHAR(*pp) == '^')
      {
      pp++;
      for (i++; i < p->laylen; i++)
        *qq++ = (PCHAR(*pp) == '#')? (' ' | PFTOP(*pp++)) : *pp++;
      *qq = 0;
      }
    }

  /* Deal with printing a row of hyphens up to this syllable. If continuing at
  the start of a new line, use the current level rather than the saved level.
  Remember to take note of any leading spaces at the start of the current text.
  */

  if (u != NULL && u->type == '-')
    {
    BOOL contflag;
    int32_t x1 = x;
    int32_t xx, yy;

    if (PCHAR(ss[0]) == ' ')
      {
      int k;
      uint32_t spaces[80];
      for (k = 0; PCHAR(ss[k]) == ' '; k++) spaces[k] = ss[k];
      spaces[k] = 0;
      x1 += string_width(spaces, fdata, NULL);
      }

    if (u->x == 0)
      {
      xx = out_sysblock->firstnoteposition + out_sysblock->xjustify - 4000;
      yy = y + p-> y;
      contflag = TRUE;
      }
    else
      {
      xx = u->x;
      yy = u->y;
      contflag = FALSE;
      }

    if (u->htype == 0) out_hyphens(xx, x1 + p->x, yy, fdata, contflag);
      else out_repeatstring(xx, x1 + p->x, yy, contflag, FALSE, u->htype);
    }

  /* Free up the hyphen or extender block. Extender blocks live till the next
  non "=" syllable, but are not drawn that far. */

  if (u != NULL)
    {
    *uu = u->next;
    mem_free_cached((void **)&main_freeuolayblocks, u);
    }

  /* Set up a new hyphen or extender block if required. We need to find the end
  of the current syllable, excluding any trailing spaces. There is no harm in
  just removing the trailing spaces now - if they are not printed, it won't be
  noticed! */

  if (PCHAR(*pp) == '=' || PCHAR(*pp) == '-')
    {
    u = mem_get_cached((void **)&main_freeuolayblocks, sizeof(uolaystr));
    u->next = bar_cont->uolay;
    bar_cont->uolay = u;

    while (qq > ss && PCHAR(qq[-1]) == ' ') qq--;  /* Find first trailing space */
    *qq = 0;                                       /* Terminate the string there */

    /* Set up the data values */

    u->x = x + p->x + string_width(ss, fdata, NULL);
    u->y = y + p->y;
    u->type = *pp;
    u->level = p->laylevel;
    u->htype = p->htype;
    u->above = overlay;
    }

  /* The string to be printed has been built in ss */

  s = ss;
  }


/* ======================= General text ======================= */

/* Deal with non-underlay, non-followon text. Adjust the x position if end or
bar alignment is required, and adjust level according to the pitch of the just
printed note/chord, if any, for the first text for any given note. Otherwise,
the position is above or below the previous. Non-underlay text can be rotated;
this was handled above, but we retained the string width before rotating the
font so that centring and end-alignment can be done first. */

else if ((flags & text_followon) == 0)
  {
  BOOL baralign = (flags & text_baralign) != 0;
  BOOL barcentre = (flags & text_barcentre) != 0;
  BOOL timealign = (flags & text_timealign) != 0;

  /* Align with start of bar */

  if (baralign) x = out_startlinebar?
    (out_sysblock->startxposition + out_sysblock->xjustify) : out_lastbarlinex;

  /* Centre in the bar */

  else if (barcentre) x = (out_lastbarlinex + out_barlinex - stringwidth)/2;

  /* Time signature alignment. If not found, use the first musical event in the
  bar. */

  else if (timealign)
    {
    if (out_startlinebar && out_sysblock->showtimes != 0)
      x = out_sysblock->timexposition + out_sysblock->xjustify;

    /* In mid-line, or if no stave is printing a time signature, search for an
    appropriate position, defaulting to the first thing in the bar that can
    follow a time signature. There must be something! */

    else
      {
      for (posstr *pt = out_postable; pt < out_poslast; pt++)
        {
        if (pt->moff >= posx_timefirst)
          { x =  out_barx + pt->xoff; break; }
        }
      }
    }

  /* Handle /e and /c which will never be set with barcentre. Note that
  stringwidth is the horizontal width, ignoring any font rotation. This means
  that rotation occurs after the text is positioned. */

  if (endalign)
    x += ((atbar || baralign || timealign)? 1000 : six) - stringwidth;
  else if ((flags & text_centre) != 0)
    x += ((atbar || baralign || timealign)? 500 : six/2) - stringwidth/2;

  /* At the bar end, we adjust as for the last note or rest if end-aligned,
  because usually such text sticks back as far as the last note. Don't adjust
  for the next note when printing a rehearsal mark at the left of the stave. */

  if (above)     /* text above */
    {
    if (out_textnextabove != 0) y = out_textnextabove;
    else if ((!atbar || endalign) && (!rehearse || !MFLAG(mf_rehearsallsleft)))
      {
      int32_t pt  = misc_ybound(FALSE, n_nexttie, TRUE, TRUE);  /* ties going out */
      int32_t ppt = misc_ybound(FALSE, n_prevtie, TRUE, TRUE);  /* ties coming in */
      if (ppt > pt) pt = ppt;
      if (pt + 2000 > y) y = pt + 2000;
      }

    /* Deal with above at overlay level */

    if ((flags & text_atulevel) != 0) y = out_sysblock->olevel[curstave];

    /* Deal with text at absolute position above the stave */

    else if ((flags & text_absolute) != 0) y = 16000;

    /* Save for another immediately following text. */

    out_textnextabove = y + p->y + unscaled_fontsize;
    }

  else           /* text below */
    {
    if (out_textnextbelow != 0) y = out_textnextbelow;
    else if (!atbar || endalign)
      {
      int32_t pb  = misc_ybound(TRUE, n_nexttie, TRUE, TRUE);
      int32_t ppb = misc_ybound(TRUE, n_prevtie, TRUE, TRUE);
      if (ppb < pb) pb = ppb;
      if (pb - fdata->size + 1000 < y) y = pb - fdata->size + 1000;
      }

    /* Deal with "middle" text */

    if ((flags & text_middle) != 0 && curstave < out_laststave)
      {
      int32_t my;
      int32_t gap = out_sysblock->stavespacing[curstave];
      int stt = curstave;
      while (gap == 0 && ++stt < out_laststave)
        {
        if (mac_isbit(out_sysblock->notsuspend, stt))
          gap = out_sysblock->stavespacing[stt];
        }
      my = - (gap/2 - 6000);
      if (my < y) y = my;
      }

    /* Deal with below at underlay level */

    else if ((flags & text_atulevel) != 0)
      y = out_sysblock->ulevel[curstave];

    /* Deal with text at absolute position below the stave */

    else if ((flags & text_absolute) != 0) y = 0;

    /* Save value for an immediately following text. */

    out_textnextbelow = y + p->y - unscaled_fontsize;
    }
  }


/* ======================= Follow-on text ======================= */

/* This outputs at the ending point of the previous text string, which has
already been through the stave magnification, though any adjustment must be so
scaled. */

else
  {
  out_string(s, fdata, out_string_endx + p->x,
    out_string_endy - (p->y * out_stavemagn)/1000, 0);
  s = NULL;   /* Stops normal output */
  }


/* ======================= Generate output ======================= */

/* Parameters are now set up -- omit empty strings (to keep the PostScript
smaller). */

if (s != NULL && *s != 0)
  {
  /* Deal with rehearsal letters */

  if (rehearse)
    {
    usint style = flags;
//    usint style = curmovt->rehearsalstyle;
    int32_t yextra = ((style & text_boxed) != 0)? 2000 :
                     ((style & text_ringed) != 0)? 4000 : 0;

    /* At the start of a bar, unless an offset is provided, if we are at the
    start of a line, align with the very start if rehearsallsleft is set or
    else with the first note. If not the start of a line, align with the
    previous bar line. Relative p->x is then added. */

    if (out_moff == 0 && p->offset == 0)
      {
      if (out_startlinebar)
        {
        if (MFLAG(mf_rehearsallsleft))
          {
          x = out_sysblock->startxposition + out_sysblock->xjustify;
          switch (bar_cont->clef)
            {
            case clef_trebledescant:
            x += 15000;
            /* Fall through */
            case clef_trebletenor:
            case clef_trebletenorB:
            case clef_treble:
            yextra += 3000;
            if (style == text_boxed) yextra += 1000;
            break;

            case clef_soprabass:
            x += 9000;
            yextra += 1000;
            break;

            default: break;
            }
          }
        else x = out_sysblock->firstnoteposition + out_sysblock->xjustify;
        }
      else x = out_lastbarlinex;
      }

    out_string(s, fdata, x + p->x,
      out_ystave - ((y + yextra + p->y)*out_stavemagn)/1000, style);
    }

  /* Deal with normal text */

  else
    {
    if ((flags & (text_boxed | text_ringed)) != 0)
      y += (above? 2 : (-2))*out_stavemagn;
    out_string(s, fdata, x + p->x, out_ystave - ((y + p->y)*out_stavemagn)/1000,
      flags);
    }
  }

TRACE("out_text() end\n");
}



/*************************************************
*               Draw an nth time marking         *
*************************************************/

/* The yield is the unmagnified y level, which is set as a minimum for a
subsequent marking. We don't free the data here, as sometimes nothing is drawn
(e.g. when bar lines descend), so the freeing happens elsewhere.

Arguments:
  rightjog      TRUE if a right jog is required
  x1            the x coordinate of the end of the mark

Returns:        the y level
*/

int32_t
out_drawnbar(BOOL rightjog, int32_t x1)
{
int32_t x[4], y[4];
int n = 0;
nbarstr *nb = bar_cont->nbar;
b_nbarstr *b = nb->nbar;

int32_t yield;
int32_t x0 = nb->x;
int32_t yy = (nb->maxy > 18000)? nb->maxy + 11000 : 29000;

/* Minimum y keeps it aligned with previous if this is not the first */

if (yy < nb->miny) yy = nb->miny;

/* Add in manual adjustment and scale to stave */

yield = yy + b->y;
yy = (yield * out_stavemagn)/1000;

/* Sort out the left hand end at the start of a system for a continued line. */

if (x0 == 0)
  x0 = out_sysblock->firstnoteposition - 2000 + out_sysblock->xjustify;

/* Start of a new iteration; set up for a jog and output the numbers and/or
texts. */

else
  {
  int32_t xt, yt;
  uint32_t *comma = NULL;
  uint32_t commaspace[3];

  x0 += 1500 + b->x;
  x[n] = x0;
  y[n++] = yy - 10*out_stavemagn;

  xt = x0 + 4000;
  yt = out_ystave - yy + 9*out_stavemagn;

  out_fdata1.size =
    mac_muldiv(curmovt->fontsizes->fontsize_repno.size, out_stavemagn, 1000);

  for (;;)
    {
    uint32_t commafont;
    b = nb->nbar;
    if (comma != NULL) ofi_string(comma, &out_fdata1, &xt, &yt, TRUE);
    if (b->s != NULL)
      {
      ofi_string(b->s, &out_fdata1, &xt, &yt, TRUE);
      commafont = PFTOP(b->s[0]);
      }
    else
      {
      uint32_t *pmws;
      uschar buff[24];
      (void)sprintf(CS buff, "%d", b->n);
      pmws = string_pmw(buff, curmovt->fonttype_repeatbar);
      ofi_string(pmws, &out_fdata1, &xt, &yt, TRUE);
      commafont = PFTOP(pmws[0]);
      }
    nb = nb->next;
    if (nb == NULL) break;
    if (comma == NULL) comma = commaspace;
    comma[0] = commafont | ',';
    comma[1] = commafont | ' ';
    comma[2] = 0;
    }
  }

/* Draw the lines and return the basic level. */

x[n] = x0;
y[n++] = yy;

x[n] = x1;
y[n++] = yy;

if (rightjog)
  {
  x[n] = x1;
  y[n++] = yy - 10*out_stavemagn;
  }

ofi_lines(x, y, n, 400);
return yield;
}



/*************************************************
*             Output warning bar                 *
*************************************************/

/* We have to take care that multiple items on different sized staves line up.
This is somewhat messy, but it's easier to isolate the whole thing here in one
function than to spread it about with zillions of conditionals in the normal
setting code.

Arguments:  none
Returns:    nothing
*/

static void
warnbar(void)
{
BOOL done = FALSE;
int32_t x = out_barx;

/* Loop for each column of signatures. Each time round the loop we scan the bar
stave by stave for key and time signatures that precede the first note. In the
first iteration we output the first signature that is found; in the second
iteration it's the second signature that is found, and so on. Whenever we
process a signature we end the scan on that stave. The loop stops after an
iteration that outputs nothing. */

for (int count = 0; !done; count++)
  {
  int32_t maxwidth = 0;
  int32_t ystave = out_yposition;

  done = TRUE;   /* Not output anything */

  /* Scan the staves, increment y position each time. */

  for (int stave = 1; stave <= out_laststave; stave++)
    {
    int thiscount;

    if (mac_notbit2(curmovt->select_staves, out_sysblock->notsuspend, stave))
      continue;  /* Ignore if not selected or if suspended. */

    /* Scan for time and/or key signatures, stopping at the first note. */

    thiscount = count;

    for (bstr *p = (bstr *)((curmovt->stavetable[stave])->barindex[curbarnumber]);
         p != NULL; p = p->next)
      {
      if (p->type == b_note) break;

      if (p->type == b_time)
        {
        b_timestr *t = (b_timestr *)p;
        if (t->warn && thiscount-- <= 0)
          {
          int32_t xx = x;
          int32_t spacing = (count == 0)? curmovt->midtimespacing :
                                          curmovt->startspace[2];
          int32_t width = spacing + mac_muldiv(curmovt->stavesizes[stave],
            misc_timewidth(t->time) + 1000, 1000);
          if (width > maxwidth) maxwidth = width;
          if (count == 0) xx -= 2000;
          out_writetime(xx + spacing, ystave, t->time);
          done = FALSE;
          break;
          }
        }

      /* If the key signature has zero width (C major, A minor, or a custom
      empty signature), ignore it completely. This occurs only when there has
      been a change from some other key, and it follows the cancellation
      signature. Leaving it in messes up the spacing for any subsequent time
      signature. */

      else if (p->type == b_key)
        {
        b_keystr *k = (b_keystr *)p;
        if (misc_keywidth(k->key, wk_cont[stave].clef) != 0 && k->warn &&
            thiscount-- <= 0)
          {
          int32_t xx = x;
          int32_t spacing = curmovt->midkeyspacing;
          int32_t width = spacing + mac_muldiv(curmovt->stavesizes[stave],
            misc_keywidth(k->key, wk_cont[stave].clef) + 1000, 1000);
          if (width > maxwidth) maxwidth = width;

          /* Output a double bar line if required. If there is a following
          unsuspended stave, extend the barline down unless this stave has zero
          spacing or barlines are broken for this stave or any suspended staves
          we skip over while searching to see if any staves follow. */

          if (MFLAG(mf_keydoublebar) && !out_lastbarwide)
            {
            BOOL extend = FALSE;
            int32_t ybarend = ystave;

            if (mac_notbit(curmovt->breakbarlines, stave) &&
                out_sysblock->stavespacing[stave] > 0)
              {
              for (int i = stave + 1; i <= out_laststave; i++)
                {
                if (mac_notbit(out_sysblock->notsuspend, i))
                  {
                  if (mac_isbit(curmovt->breakbarlines, i)) break;
                  }
                else
                  {
                  stavestr *sss = curmovt->stavetable[i];
                  if (!sss->omitempty ||
                      !mac_emptybar(sss->barindex[curbarnumber]))
                  extend = TRUE;
                  break;
                  }
                }
              }

            if (extend) ybarend += out_sysblock->stavespacing[stave];
            ofi_barline(out_lastbarlinex, ystave, ybarend, bar_double,
              curmovt->stavesizes[stave]);
            }
          else xx -= 1000;  /* No double bar - move left a bit. */

          out_writekey(xx + spacing, ystave, wk_cont[stave].clef, k->key);
          done = FALSE;
          break;
          }
        }
      }   /* End of bar scan loop */

    ystave += out_sysblock->stavespacing[stave];
    }     /* End of stave scan loop */

  /* Add in the widest signature in this column */

  x += maxwidth;
  }       /* End of column loop */

/* Set a "last bar line" value as it is used for the stave length. */

out_lastbarlinex = (out_sysblock->flags & sysblock_stretch)?
  curmovt->linelength : x + 2000;
}



/************************************************
*         Output a stave joining sign           *
************************************************/

/* This function is used for outputting lines, brackets, or braces at the
start of a system. It returns a bit map of the staves it referenced. For braces
and thin brackets, prev is the bit map of the (thick) bracketed staves. */


/* Local subroutine to determine if a stave is being printed.

Arguments:
  stave      stave number
  bar        bar number

Returns:     TRUE if the bar is to be printed
*/

static BOOL
is_printing(int stave, int bar)
{
stavestr *ss = curmovt->stavetable[stave];
return mac_isbit(out_sysblock->notsuspend, stave) &&
  (!ss->omitempty || !mac_emptybar(ss->barindex[bar]));
}


/* The actual stave-joining function.

Arguments:
  list         pointer to chain of stave selections
  depthvector  vector of stave depths
  prev         bitvector of previous thick bracket for thin bracket and brace;
                 0 for other types (not used)
  which        type of join sign, e.g. join_brace
  bartype      barline type when which == join_barline
  bar          number of the first bar in the system

Returns:       bit map of referenced staves
*/

static uint64_t
dojoinsign(stavelist *list, int32_t *depthvector, uint64_t prev, int which,
  int bartype, int bar)
{
uint64_t yield = 0;

for (; list != NULL; list = list->next)
  {
  stavestr *ss;
  int32_t xx;
  int8_t pb1 = list->first;
  int8_t pb2 = list->last;

  if (pb1 > curmovt->laststave) continue;
  if (pb2 > curmovt->laststave) pb2 = curmovt->laststave;
  while (pb1 < out_laststave && pb1 < pb2 && !is_printing(pb1, bar)) pb1++;
  while (pb2 > pb1 && !is_printing(pb2, bar)) pb2--;

  if (!is_printing(pb1, bar) && !is_printing(pb2, bar)) continue;

  /* Take magnification from the top stave (this is default in the case of
  barline join). */

  out_stavemagn = curmovt->stavesizes[pb1];
  ss = curmovt->stavetable[pb1];
  xx = out_stavemagn * ((ss->stavelines == 6)? 4 : (ss->stavelines == 4)? -4 : 0);

  switch(which)
    {
    case join_thinbracket:
    case join_brace:

    /* Should never be printed for one stave; there is some old code
    that gets a brace positioned right if it ever is, but at present that
    code is never triggered. */

    if (depthvector[pb1] != depthvector[pb2])
      {
      BOOL overlap = FALSE;
      for (int i = pb1; i <= pb2; i++)
        if (mac_isbit(prev, i)) { overlap = TRUE; break; }

      if (which == join_brace)
        {
        int32_t adjust = (depthvector[pb1] == depthvector[pb2])?
          7000 : 8500;
        if (overlap) adjust += 1500;
        ofi_brace(out_joinxposition - adjust,
          out_yposition + depthvector[pb1] - xx,
            out_yposition + depthvector[pb2], out_stavemagn);
        }
      else    /* thin bracket */
        {
        int32_t x[4], y[4];
        out_ystave = out_yposition;
        x[0] = x[3] = out_joinxposition;
        x[1] = x[2] = x[0]  - (overlap? 2500:1000) - 3000;
        y[0] = y[1] = - depthvector[pb1] + 16*out_stavemagn + xx;
        y[2] = y[3] = - depthvector[pb2];
        ofi_lines(x, y, 4, 400);
        }
      }
    break;

    case join_bracket:
    ofi_bracket(out_joinxposition-3500, out_yposition + depthvector[pb1] - xx,
      out_yposition + depthvector[pb2], out_stavemagn);
    break;

    case join_barline:
    ofi_barline(out_joinxposition, out_yposition + depthvector[pb1] - xx,
      out_yposition + depthvector[pb2], bartype,
      (curmovt->barlinesize > 0)? curmovt->barlinesize : out_stavemagn);
    break;
    }

  for (int i = pb1; i <= pb2; i++) mac_setbit(yield, i);
  }

return yield;
}



/************************************************
*               Output a clef                   *
************************************************/

/* The clef characters all print a little to the right of the given position.
The reason is historical. It does not matter at all, except when staves of
different sizes are printed - then the different size causes an uneven spacing
at the start of a line. So attempt to correct for this.

We must also adjust the vertical position for clefs that are not of full size,
so that they appear at the correct position on the stave.

Arguments:
  x           x-position
  y           y-position
  clef        which clef
  size        size
  midbar      TRUE if a mid-bar clef

Returns:      nothing
*/

void
out_writeclef(int32_t x, int32_t y, int clef, int32_t size, BOOL midbar)
{
switch(clef)
  {
  case clef_none:
  return;

  case clef_cbaritone:
  case clef_tenor:
  case clef_alto:
  case clef_soprano:
  case clef_mezzo:
  x += (2000 * (1000 - out_stavemagn))/1000;
  break;

  case clef_baritone:
  case clef_deepbass:
  case clef_bass:
  case clef_contrabass:
  case clef_soprabass:
  x -= ((midbar? 15:5)*out_stavemagn)/10;    /* move left 0.5 pt or 1.5 pt in mid bar */
  x += (750 * (1000 - out_stavemagn))/1000;
  break;

  case clef_hclef:
  break;

  case clef_treble:
  case clef_trebletenor:
  case clef_trebletenorB:
  case clef_trebledescant:
  x += (700 * (1000 - out_stavemagn))/1000;
  break;
  }

ofi_muschar(x,
  y - mac_muldiv(10000 - size, clef_adjusts[clef] * out_stavemagn, 10000),
  clef_chars[clef * 4 + curmovt->clefstyle],
  (size * out_stavemagn)/1000);
}



/************************************************
*              Output a key                     *
************************************************/

/* This takes note of any "printkey" settings. The key_reset bit is set when a
naturalizing key is required.

Arguments:
  x         x-position
  y         y-position
  clef      the current clef
  key       the key signature

Returns:    nothing
*/

void
out_writekey(int32_t x, int32_t y, uint32_t clef, uint32_t key)
{
uint32_t key63 = key & 63;
BOOL use_naturals = (key & key_reset) != 0;
pkeystr *pk;

for (pk = main_printkey; pk != NULL; pk = pk->next)
  {
  if (key63 == pk->key && clef == pk->clef &&
      pk->movt_number <= curmovt->number)
    break;
  }

/* There is a special string for this key. */

if (pk != NULL)
  {
  out_fdata1.size = 10 * out_stavemagn;
  out_string((key > 63)? pk->cstring : pk->string, &out_fdata1, x, y, 0);
  }

/* Get the key data from the table. */

else
  {
  uint8_t *kca = keyclefadjusts + clef * 3;   /* Entry in ajusts table */
  uint8_t adjust = kca[0];                    /* Clef adjustment */
  uint8_t slimit = kca[1];                    /* Lower limit for sharp */
  uint8_t flimit = kca[2];                    /* Lower limit for flat */

  /* Each item in the 8-bit key signature table consists of an accidental in
  the top 4 bits and a position on a 5-line stave in the lower 4. The positions
  run from 0 (below the bottom line) to 10 (above the top line). */

  for (uint8_t *k = keysigtable[key63]; *k != ks_end; k++)
    {
    uint32_t ch;
    int32_t offset = (*k & 0x0fu) - adjust;
    uint8_t ac = *k >> 4;

    /* Raise by an octave if too low */

    if (ac <= ac_ds)  /* Sharps or naturals */
      {
      if (offset < slimit) offset += 7;
      }
    else              /* Flats */
      {
      if (offset < flimit) offset += 7;
      }

    /* Now convert the offset into a dimension relative to the bottom line of a
    5-line stave (there are 4 points between lines. */

    offset = (2 * offset - 4) * 1000;

    /* If it's a resetting signature, we must print a natural. Then select the
    relevant virtual music character - the half accidentals have two different
    versions. */

    if (use_naturals) ac = ac_nt;

    switch(ac)
      {
      default:
      ch = ac_chars[ac];
      break;

      case ac_hs:
      if (curmovt->halfsharpstyle == 0)
        {
        ch = mc_hsharp1;
        ac = ac_no;  /* Fudge for narrow one */
        }
      else ch = mc_hsharp2;
      break;

      case ac_hf:
      ch = (curmovt->halfflatstyle == 0)? mc_hflat1 : mc_hflat2;
      break;
      }

    /* Output the character and adjust the position. */

    ofi_muschar(x, y - (offset * out_stavemagn)/1000, ch, 10 * out_stavemagn);
    x += mac_muldiv(curmovt->accspacing[ac], out_stavemagn, 1000);
    }
  }
}



/************************************************
*              Output a time                    *
************************************************/

/*
Arguments:
  x         x-position
  y         y-position
  ts        time signature

Returns:    nothing
*/

void
out_writetime(int32_t x, int32_t y, usint ts)
{
ptimestr *pt;
fontinststr *fdatavector;
int8_t sizen, sized;
uint32_t vn[16];
uint32_t vd[16];
uint32_t *topstring, *botstring;

/* If not printing time signatures, return */

if (!MFLAG(mf_showtime)) return;

fdatavector = (curmovt->fontsizes)->fontsize_text;

/* First see if this time signature has special strings specified for its
printing. The printtime directive must have happened in this movement or
earlier for it to be applicable. */

for (pt = main_printtime; pt != NULL; pt = pt->next)
  {
  if (pt->time == ts && pt->movt_number <= curmovt->number) break;
  }

/* If found special case, get strings and sizes from it */

if (pt != NULL)
  {
  sizen = pt->sizetop;
  sized = pt->sizebot;
  topstring = pt->top;
  botstring = pt->bot;
  }

/* Default printing for this time signature. First mask off the multiplier,
then check for the special cases of C and A. */

else
  {
  topstring = vn;
  botstring = vd;

  ts &= 0xffffu;  /* Lose the multiplier */

  /* C and A are special cases */

  if (ts == time_common || ts == time_cut)
    {
    ofi_muschar(x, y - 4 * out_stavemagn, ((ts == time_common)?
      mc_common : mc_cut), 10 * out_stavemagn);
    return;
    }

  /* Non-special case - set up numerator and denominator, in the
  time signature font. */

  misc_psprintf(vn, curmovt->fonttype_time, "%d", ts >> 8);
  misc_psprintf(vd, curmovt->fonttype_time, "%d", ts & 255);
  sizen = sized = ff_offset_ts;
  }

/* We now have in topstring and botstring two strings to print. Arrange that
they are centred with respect to each other when both are to be printed. Also
arrange to adjust the heights according to the font size. We assume that at
12-points, the height is 8 points, which is true for the default bold font.
However, it is not true for the music font, so there is a fudge to check for
that case which will catch the common cases. */

out_fdata1.size = (fdatavector[sizen].size * out_stavemagn)/1000;
out_fdata2.size = (fdatavector[sized].size * out_stavemagn)/1000;

/* NOTE: Because the sizes are unsigned, an (int) cast is necessary when they
are subtracted in the code below to prevent what should be a negative number
becoming a large positive one. */

if (MFLAG(mf_showtimebase) && botstring[0] != 0)
  {
  uint32_t stdsize = out_stavemagn *
    (((PFONT(botstring[0]) & ~font_small) == font_mf)? 10 : 12);
  int32_t nx = 0;
  int32_t dx = 0;
  int32_t widthn = string_width(topstring, &out_fdata1, NULL);
  int32_t widthd = string_width(botstring, &out_fdata2, NULL);

  if (widthn > widthd) dx = (widthn - widthd)/2; else
    nx = (widthd - widthn)/2;
  out_string(topstring, &out_fdata1, x + nx, y - (8000 * out_stavemagn)/1000, 0);
  out_string(botstring, &out_fdata2, x + dx,
    y + ((3 * (int)(out_fdata2.size - stdsize)/4) * out_stavemagn)/1000, 0);
  }

else
  {
  uint32_t stdsize = out_stavemagn *
    (((PFONT(topstring[0]) & ~font_small) == font_mf)? 10 : 12);
  out_string(topstring, &out_fdata1, x,
    y - ((4000 - (int)(out_fdata1.size - stdsize)/3) * out_stavemagn)/1000, 0);
  }
}



/*************************************************
*           Output repeat marks                  *
*************************************************/

static int repspacing[] = {

/* righthand repeats */
/* thick  thin  dots */
    50,    31,    6,        /* repeatstyle = 0 */
    -1,    50,   25,        /* repeatstyle = 1 */
    -1,    50,   25,        /* repeatstyle = 2 */
    -1,    -1,   25,        /* repeatstyle = 3 */
    50,    31,    6,        /* repeatstyle = 4 */

/* lefthand repeats */
/* thick  thin  dots */
     0,    35,   51,        /* repeatstyle = 0 */
    -1,     0,   15,        /* repeatstyle = 1 */
    -1,     0,   15,        /* repeatstyle = 2 */
    -1,    -1,   15,        /* repeatstyle = 3 */
     0,    35,   51,        /* repeatstyle = 4 */

/* righthand double repeats */
/* thick  thin  dots */
    50,    31,    6,        /* repeatstyle = 0 */
    -1,    50,   25,        /* repeatstyle = 1 */
    -1,    50,   25,        /* repeatstyle = 2 */
    -1,    -1,   25,        /* repeatstyle = 3 */
    34,    -1,    6,        /* repeatstyle = 4 */

/* lefthand double repeats */
/* thick  thin  dots */
     0,    35,   51,        /* repeatstyle = 0 */
    -1,     0,   15,        /* repeatstyle = 1 */
    -1,     0,   15,        /* repeatstyle = 2 */
    -1,    -1,   15,        /* repeatstyle = 3 */
    17,    -1,   51         /* repeatstyle = 4 */
};

/*
Arguments:
  x          x-position
  type       type of repeat
  magn       magnification

Returns:     nothing
*/

void
out_writerepeat(int32_t x, int type, int32_t magn)
{
int style = curmovt->repeatstyle;
int *xx = repspacing + type + style * 3;

if (xx[0] >= 0)
  ofi_barline(x + (xx[0]*magn)/10, out_ystave, out_ybarend, bar_thick, magn);

if (xx[1] >= 0)
  ofi_barline(x + (xx[1]*magn)/10, out_ystave, out_ybarend,
    (style == 2)? bar_dotted : bar_single, magn);

out_ascstring((style != 3)? US"xI" : US"IxxyyyyyyI", font_mf, 10*out_stavemagn,
  x + (xx[2]*magn)/10 + (65*(magn - out_stavemagn))/100, out_ystave);

/* Output "wings" if requested, positioned at the thick line, if any, else at
the thin line if any, else at the dots. */

if (MFLAG(mf_repeatwings))
  {
  int32_t dx = ((type == rep_left || type == rep_dleft)? 7 : -7) * magn;
  int32_t dy = 4*magn;
  int32_t thick = magn/2;

  if (xx[0] >= 0) x += ((xx[0]+10)*magn)/10;
    else if (xx[1] >= 0) x += (xx[1]*magn)/10;
    else x += (xx[2]*magn)/10;

  if (curstave == out_topstave)
    {
    int32_t y = 16*magn;
    ofi_line(x, y, x + dx, y + dy, thick, 0);
    }

  if (curstave == out_botstave)
    {
    int32_t y = 0;
    ofi_line(x, y, x + dx, y - dy, thick, 0);
    }
  }
}



/*************************************************
*        Find X offset for given M offset        *
*************************************************/

/* The search starts at the current out_posptr. We never search for a value
that is less than a previous one. When setting up a beam over a bar line, the
moff can be greater than the bar length. In this case, and also in the case
when it is equal to the bar length, we must search the NEXT bar.

Argument:  the musical offset in the bar
Returns:   the x offset in the bar
*/

int32_t
out_findXoffset(int32_t moff)
{
if (!beam_overbeam || moff < out_poslast->moff)
  {
  while (moff > out_posptr->moff && out_posptr < out_poslast) out_posptr++;
  while (moff < out_posptr->moff && out_posptr > out_postable) out_posptr--;
  if (moff == out_posptr->moff) return out_posptr->xoff;
  }

/* Handle the beam over bar line case */

else
  {
  int newmoff = moff - out_poslast->moff;
  posstr *new_postable, *new_posptr, *new_poslast;
  barposstr *bp = curmovt->posvector + curbarnumber + 1;
  new_postable = new_posptr = bp->vector;
  new_poslast = new_postable + bp->count - 1;

  while (newmoff > new_posptr->moff && new_posptr < new_poslast) new_posptr++;
  while (newmoff < new_posptr->moff && new_posptr > new_postable) new_posptr--;

  if (newmoff == new_posptr->moff)
    return new_posptr->xoff + out_poslast->xoff + out_sysblock->barlinewidth;
  }

/* Cannot find a position for this moff. This error is hard, but need to keep
the compiler happy. */

error(ERR142, moff, sfn(moff));
return 0;
}



/*************************************************
*  Find X offset for one of two given M offsets  *
*************************************************/

/* This is used only in the non-overbeaming case. It returns the x offset of
the first moff if it exists, otherwise the x offset of the second moff.

Arguments:
  moff1       the first music offset
  moff2       the second music offset

Returns:      the x offset
*/

int32_t
out_findGoffset(int32_t moff1, int32_t moff2)
{
while (moff1 > out_posptr->moff && out_posptr < out_poslast) out_posptr++;
while (moff1 < out_posptr->moff && out_posptr > out_postable) out_posptr--;

if (moff1 == out_posptr->moff) return out_posptr->xoff;
return out_findXoffset(moff2);
}



/*************************************************
*     Find postable entry for given M offset     *
*************************************************/

/*
Argument: the music offset
Returns:  pointer to the postable entry, or NULL if not found
*/

posstr *
out_findTentry(int32_t moff)
{
while (moff > out_posptr->moff && out_posptr < out_poslast) out_posptr++;
while (moff < out_posptr->moff && out_posptr > out_postable) out_posptr--;
return (moff == out_posptr->moff)? out_posptr : NULL;
}



/*************************************************
*   Find X offset or interpolate in current bar  *
*************************************************/

/* This is used to implement MusicXML-styole "offsets", which may be positive
or negative. Don't alter out_posptr. If we don't find an exact moff,
interpolate between two entries or, if off the end, fudge assuming 16 points
per crotchet.

Argument: the music offset
Returns:  an x offset
*/

int32_t
out_findAoffset(int32_t moff)
{
posstr *a, *b;
posstr *p = out_posptr;
while (moff > p->moff && p < out_poslast) p++;
while (moff < p->moff && p > out_postable) p--;
if (moff == p->moff) return p->xoff;

if (moff > p->moff)
  {
  if (p == out_poslast)  /* After last entry */
    return p->xoff + mac_muldiv(16000, moff - p->moff, len_crotchet);
  a = p;
  b = p + 1;
  }

else
  {
  if (p == out_postable)  /* Before first entry */
    return p->xoff - mac_muldiv(16000, p->moff - moff, len_crotchet);
  b = p;
  a = p - 1;
  }

/* In between two entries; interpolate. */

return a->xoff +
  mac_muldiv(b->xoff - a->xoff, moff - a->moff, b->moff - a->moff);
}



/*************************************************
*              Output one string                 *
*************************************************/

/* The y values are absolute positions downwards from the top of the page. This
function deals with special escape characters in the string, and remembers the
string's end point in case there is a follow-on string.

Arguments:
  s           a PMW string
  fdata       pointer to font instance data
  x           x-coordinate for the start
  y           y-coordinate for the start
  boxring     text flags

Returns:      nothing
*/

void
out_string(uint32_t *s, fontinststr *fdata, int32_t x, int32_t y,
  uint32_t flags)
{
BOOL rotated;
int32_t y0, y1;
int32_t *matrix;
int32_t xstart = x;
int32_t ystart = y;
int32_t magn;
int i = 0;
int nonskip = 0;
uint32_t boxring = flags & (text_boxed | text_boxrounded | text_ringed);
uint32_t buff[256];

/* Make a copy of the string, interpreting the specials, and outputting as we
go if the local buffer gets too full (ensure space for a page number). */

for (uint32_t *ss = s; *ss != 0; ss++)
  {
  uint32_t c = PCHAR(*ss);

  /* Ensure enough space for a page or bar repeat number */

  if (i > 250)
    {
    buff[i] = 0;
    ofi_string(buff, fdata, &x, &y, TRUE);
    i = 0;
    }

  if (c <= MAX_UNICODE) buff[i++] = *ss; else
    {
    BOOL doinsert = FALSE;
    uint32_t f = PFTOP(*ss);
    uint32_t pn = 0;

    if (c == ss_repeatnumber || c == ss_repeatnumber2)
      {
      doinsert = TRUE;
      if (curstave >= 0 && curbarnumber >= 0)
        {
        stavestr *sts = curmovt->stavetable[curstave];
        if (sts != NULL && sts->barindex != NULL)
          pn = sts->barindex[curbarnumber]->repeatnumber;
        if (pn == 1 && c == ss_repeatnumber2) doinsert = FALSE;
        }
      }

    else
      {
      BOOL isodd;

      pn = curpage->number;
      isodd = (pn & 1) != 0;

      switch(c)
        {
        case ss_verticalbar:   /* Unescaped vertical bar */
        buff[i++] = f | '|';
        break;

        case ss_escapedhyphen: /* Escaped hyphen */
        buff[i++] = f | '-';
        break;

        case ss_escapedequals: /* Escaped equals */
        buff[i++] = f | '=';
        break;

        case ss_escapedsharp:  /* Escaped sharp */
        buff[i++] = f | '#';
        break;

        case ss_page:          /* Unconditional page number */
        doinsert = TRUE;
        break;

        case ss_pageodd:       /* Page number if odd */
        doinsert = isodd;
        break;

        case ss_pageeven:      /* Page number if even */
        doinsert = !isodd;
        break;

        case ss_skipodd:       /* Skip if page number is odd */
        if (nonskip == ss_skipodd) nonskip = 0; else
          {
          nonskip = ss_skipodd;
          if (isodd) while (ss[1] != 0 && PCHAR(ss[1]) != ss_skipodd) ss++;
          }
        break;

        case ss_skipeven:      /* Skip if page number is even */
        if (nonskip == ss_skipeven) nonskip = 0; else
          {
          nonskip = ss_skipeven;
          if (!isodd) while (ss[1] != 0 && PCHAR(ss[1]) != ss_skipeven) ss++;
          }
        break;
        }
      }

    /* Insert page or bar repeat number if required */

    if (doinsert)
      {
      char nbuff[16];
      sprintf(nbuff, "%d", pn);
      for (char *p = nbuff; *p != 0; p++) buff[i++] = f | *p;
      }
    }
  }

/* Output final part of the string */

if (i > 0)
  {
  buff[i] = 0;
  ofi_string(buff, fdata, &x, &y, TRUE);
  }

/* Retain final ending position */

out_string_endx = x;
out_string_endy = y;

if (boxring == 0) return;  /* All done for plain string */

/* Deal with boxed and/or ringed strings. If this is a barnumber, it is
independent of any stave so curstave will be negative and there is no stave
magnification. */

magn = (curstave < 0)? 1000 : curmovt->stavesizes[curstave];

y0 = out_ystave - ystart - 2*magn;
y1 = y0 + fdata->size + magn;

matrix = fdata->matrix;
rotated = matrix != NULL && matrix[4] != 0;

/* Compute length of string along string by Pythagoras if rotated. Putting the
(int32_t) cast next to the sqrt function gets a compiler warning. */

if (rotated)
  {
  double xx = (double)x - (double)xstart;
  double yy = (double)y - (double)ystart;
  double zz = sqrt(xx*xx + yy*yy);
  x = xstart + (int32_t)zz;
  }

x += 2*magn - fdata->size/20;

/* Boxed string - note the lines() routine is stave-relative. Draw 5 lines
for the box to get the corner right at the starting position. */

if ((boxring & text_boxed) != 0)
  {
  int32_t xx[6], yy[6];

  xx[0] = xx[3] = xx[4] = xstart - 2000;
  xx[1] = xx[2] = xx[5] = x;

  yy[0] = yy[1] = yy[4] = yy[5] = y0;
  yy[2] = yy[3] = y1;

  /* If text is rotated, rotate about the initial point */

  if (rotated)
    {
    for (int j = 0; j <= 5; j++)
      {
      int32_t xxx = xx[j] - xstart;
      int32_t yyy = yy[j] - out_ystave + ystart;
      int32_t xxxx = mac_muldiv(xxx, matrix[5], 1000) - mac_muldiv(yyy,
        matrix[4], 1000);
      int32_t yyyy = mac_muldiv(yyy, matrix[5], 1000) + mac_muldiv(xxx,
        matrix[4], 1000);
      xx[j] = xxxx + xstart;
      yy[j] = yyyy + out_ystave - ystart;
      }
    }

  /* This is how to get rounded or bevelled corners if we set up options for
  this feature. Might also need to adjust 15 below somehow. */

  /* Set rounded corners if wanted */

  if ((boxring & text_boxrounded) != 0) ofi_setcapandjoin(caj_round_join);
  ofi_lines(xx, yy, 6, fdata->size/15);
  ofi_setcapandjoin(caj_mitre_join);
  }

/* Ringed string - the paths routine is also stave-relative */

if ((boxring & text_ringed) != 0)
  {
  int32_t xx[13], yy[13], cc[6];
  int32_t d = (2*fdata->size)/7;
  int32_t w = (2*(x - xstart + 2000))/7;

  cc[0] = path_move;
  cc[1] = cc[2] = cc[3] = cc[4] = path_curve;
  cc[5] = path_end;

  xx[0] = xx[9] = xx[12] = xstart - 2000;
  xx[3] = xx[6] = x;
  xx[1] = xx[8] = xx[0] + w;
  xx[2] = xx[7] = xx[3] - w;
  xx[4] = xx[5] = xx[3] + d;
  xx[10] = xx[11] = xx[0] - d;

  yy[0] = yy[3]  = yy[12] = y1;
  yy[6] = yy[9]  = y0;
  yy[1] = yy[2]  = yy[0] + w;
  yy[4] = yy[11] = yy[0] - d;
  yy[5] = yy[10] = yy[6] + d;
  yy[7] = yy[8]  = yy[6] - w;

  /* If text is rotated, rotate about the initial point */

  if (rotated)
    {
    for (int j = 0; j <= 12; j++)
      {
      int32_t xxx = xx[j] - xstart;
      int32_t yyy = yy[j] - out_ystave + ystart;
      int32_t xxxx = mac_muldiv(xxx, matrix[5], 1000) - mac_muldiv(yyy,
        matrix[4], 1000);
      int32_t yyyy = mac_muldiv(yyy, matrix[5], 1000) + mac_muldiv(xxx,
        matrix[4], 1000);
      xx[j] = xxxx + xstart;
      yy[j] = yyyy + out_ystave - ystart;
      }
    }

  ofi_path(xx, yy, cc, fdata->size/15);
  }
}



/*************************************************
*           Output ASCII string                  *
*************************************************/

/* This is mostly used for strings in the music font.

Arguments:
  s        an ASCII string
  font     a font id
  size     the font size
  x, y     the position

Returns:   nothing
*/

void
out_ascstring(uschar *s, int font, int32_t size, int32_t x, int32_t y)
{
out_fdata1.size = size;
out_string(string_pmw(s, font), &out_fdata1, x, y, 0);
}



/*************************************************
*             Output one head/foot line          *
*************************************************/

/* Called from out_heading() below.

Argument:   pointer to headstr
Returns:    nothing
*/

static void
out_headfootline(headstr *p)
{
if (p->string[0] != NULL)
  {
  int32_t w = string_width(p->string[0], &(p->fdata), NULL);
  out_string(p->string[0], &(p->fdata), 0, out_yposition, 0);
  if (-2000 < out_bbox[0]) out_bbox[0] = -2000;
  if (w + 2000 > out_bbox[2]) out_bbox[2] = w + 2000;
  }

if (p->string[1] != NULL)
  {
  int32_t w = string_width(p->string[1], &(p->fdata), NULL);
  int32_t x = (curmovt->linelength - w)/2;
  out_string(p->string[1], &(p->fdata), x, out_yposition, 0);
  if (x - 2000 < out_bbox[0]) out_bbox[0] = x - 2000;
  if (x + w + 2000 > out_bbox[2]) out_bbox[2] = x + w + 2000;
  }

if (p->string[2] != NULL)
  {
  int32_t w = string_width(p->string[2], &(p->fdata), NULL);
  out_string(p->string[2], &(p->fdata), curmovt->linelength - w,
    out_yposition, 0);
  if (curmovt->linelength - w - 2000 < out_bbox[0])
    out_bbox[0] = curmovt->linelength - w - 2000;
  if (curmovt->linelength + 4000 > out_bbox[2])
    out_bbox[2] = curmovt->linelength + 4000;
  }
}



/*************************************************
*              Output heading texts              *
*************************************************/

/* Called from out_page() below.

Argument:   pointer to chain of heading blocks
Returns:    nothing
*/

static void
out_heading(headblock *h)
{
TRACE("out_heading() start\n");

for (headstr *p = h->headings; p != NULL; p = p->next)
  {
  /* Deal with a drawing. The drawing code is set up for use on staves; hence
  we must set up curstave as well as the origin. We set curstave negative to
  control error messages. */

  if (p->drawing != NULL)
    {
    draw_ox = draw_oy = 0;
    curstave = -1;
    out_ystave = out_yposition;
    out_dodraw(p->drawing, p->drawargs, FALSE);
    out_yposition += p->space;
    }

  /* Deal with textual heading/footing */

  else
    {
    int32_t descender = (4 * p->fdata.size)/10;
    out_yposition += p->spaceabove;
    if (out_yposition + descender > out_bbox[1])
      out_bbox[1] = out_yposition + descender;
    if (out_yposition - p->fdata.size < out_bbox[3])
      out_bbox[3] = out_yposition - p->fdata.size;
    out_headfootline(p);
    out_yposition += p->space;
    }
  }


TRACE("out_heading() end\n");
}


/*************************************************
*              Output a system                   *
*************************************************/

/* Called from out_page() below. The system to be output is set in
out_sysblock.

Argument:    TRUE for the first system of a movement
Returns:     nothing
*/

static void
out_system(BOOL firstsystem)
{
int lastystave;
int zerocopycount = 0;
int32_t previous_stavedepth = INT32_MAX;   /* Non-zero is what matters */
int32_t depthvector[MAX_STAVE+1];
int32_t save_colour[3];
snamestr **stavenames = out_sysblock->stavenames;

TRACE("out_system() start\n");

/* out_yposition is the position for the system; out_ystave is the position for
the stave we are working on. */

out_ystave = out_yposition;

/* Frequently used values */

curbarnumber = out_sysblock->barstart;
out_laststave = curmovt->laststave;

/* Make a copy of the continuation data - but see later for multiple stave
zero copies. */

misc_copycontstr(wk_cont, out_sysblock->cont, out_laststave, FALSE);

/* Output the start-of-line matter on each stave, and at the same time compute
the relative position of each stave. */

depthvector[0] = 0;
for (curstave = 1; curstave <= out_laststave; curstave++)
  {
  stavestr *ss;

  TRACE("start of line matter for stave %d\n", curstave);

  depthvector[curstave] = out_ystave - out_yposition;
  if (mac_notbit(out_sysblock->notsuspend, curstave)) continue;

  ss = curmovt->stavetable[curstave];
  out_stavemagn = curmovt->stavesizes[curstave];
  out_pitchmagn = out_stavemagn/2;

  /* Deal with stave name; there may be additional strings hung off the
  "extra" field. */

  for (snamestr *sname = (stavenames == NULL)? NULL : stavenames[curstave];
       sname != NULL;
       sname = sname->extra)
    {
    /* Deal with textual stave name. Note: sname->adjusty is positive for
    upwards, but out_string() has y doing downwards. */

    if (sname->text != NULL)
      {
      BOOL vertical = (sname->flags & snf_vertical) != 0;
      int32_t adjustx = sname->adjustx;
      int32_t adjusty = -8 * curmovt->stavesizes[curstave] - sname->adjusty;
      fontinststr *fdata = &((curmovt->fontsizes->fontsize_text)[sname->size]);

      /* For text which is vertically centred between two staves, there is a
      fudge to get it in the middle of a brace. */

      if ((sname->flags & snf_vcentre) != 0)
        {
        /* Scan from the current stave, looking for a stave with a non-zero
        stavespacing. That is, look for the distance down from this stave.
        We use this for additional vertical spacing only if neither it nor
        the stave that follows are suspended. */

        for (int stv = curstave; stv < out_laststave; stv++)
          {
          int32_t gap = out_sysblock->stavespacing[stv];
          if (gap != 0)
            {
            if (mac_isbit(out_sysblock->notsuspend, stv) &&
                mac_isbit(out_sysblock->notsuspend, (stv+1)))
              adjusty += gap/2 - (vertical? 0 : 2000);
            break;
            }
          }
        }

      /* Output the single vertical line, always centred, with a small
      additional adjustment. */

      if (vertical)
        {
        adjusty += string_width(sname->text, fdata, NULL)/2;
        fdata = font_rotate(fdata, 90000);
        out_string(sname->text, fdata, out_sysblock->xjustify + adjustx,
          out_ystave + adjusty, 0);
        }

      /* Output horizontal lines, adjusting the vertical position according to
      the number of lines; the line depth is equal to the font size. */

      else
        {
        int32_t maxw = 0;

        adjusty += (fdata->size*4)/10 - ((sname->linecount - 1)*fdata->size)/2;

        /* If both centre & right adjust flags are set, we need to find the
        length of the longest line of the text. Tedious, but there's no other
        way of doing it as far as I can see... */

        if ((sname->flags & (snf_hcentre | snf_rightjust)) ==
                            (snf_hcentre | snf_rightjust))
          {
          for (uint32_t *t = sname->text; *t != 0; )
            {
            int32_t w;
            uint32_t tsave;
            uint32_t *tt = t;
            while (*t != 0 && PCHAR(*t) != ss_verticalbar) t++;
            tsave = *t;
            *t = 0;
            w = string_width(tt, fdata, NULL);
            *t = tsave;
            if (w > maxw) maxw = w;
            if (tsave != 0) t++;
            }
          }

        /* Now output the lines */

        for (uint32_t *t = sname->text; *t != 0; )
          {
          int32_t adjustline = 0;
          uint32_t tsave;
          uint32_t *tt = t;

          while (*t != 0 && PCHAR(*t) != ss_verticalbar) t++;
          tsave = *t;
          *t = 0;

          if ((sname->flags & (snf_hcentre | snf_rightjust)) != 0)
            {
            int32_t w = string_width(tt, fdata, NULL);
            adjustline = out_sysblock->startxposition - 6000 - w;
            if (curmovt->bracelist != NULL) adjustline -= 6500;
              else if (curmovt->thinbracketlist != NULL) adjustline -= 4000;
            if ((sname->flags & snf_hcentre) != 0)
              {
              if ((sname->flags & snf_rightjust) == 0) adjustline /= 2;
                else adjustline -= (maxw - w)/2;
              }
            }

          out_string(tt, fdata, out_sysblock->xjustify + adjustline + adjustx,
            out_ystave + adjusty, 0);
          adjusty += fdata->size;

          *t = tsave;
          if (tsave != 0) t++;
          }
        }
      }

    /* Deal with stave name drawing */

    if (sname->drawing != NULL)
      {
      draw_ox = draw_oy = 0;
      out_dodraw(sname->drawing, sname->drawargs, FALSE);
      }
    }

  /* Output clef, key, etc if the bar contains any data or if this stave is not
  overprinting the one above or below and is not an omitempty stave. That is,
  omit for an empty overprinting or omitempty stave. */

  if (!mac_emptybar(ss->barindex[out_sysblock->barstart]) ||
       (!ss->omitempty && out_sysblock->stavespacing[curstave] != 0 &&
        previous_stavedepth != 0))
    {
    out_writeclef(out_sysblock->startxposition + out_sysblock->xjustify +
      curmovt->startspace[0], out_ystave, wk_cont[curstave].clef, 10000, FALSE);

    if (misc_keywidth(wk_cont[curstave].key, wk_cont[curstave].clef) != 0)
      out_writekey(out_sysblock->keyxposition + out_sysblock->xjustify,
        out_ystave, wk_cont[curstave].clef, wk_cont[curstave].key);

    if (mac_isbit(out_sysblock->showtimes, curstave))
      out_writetime(out_sysblock->timexposition + out_sysblock->xjustify,
        out_ystave, wk_cont[curstave].time);
    }

  /* Advance down to next stave */

  previous_stavedepth = out_sysblock->stavespacing[curstave];
  out_ystave += previous_stavedepth;
  }

/* Compute the levels for copies of stave 0 that are to be printed. If two or
more share a level (because of suspension), keep only the last (-1 => no
print). Don't print one below the system depth. There will always be at least
one block on the list. */

for (zerocopystr *zerocopy = curmovt->zerocopy; zerocopy != NULL;
     zerocopy = zerocopy->next)
  {
  if (zerocopy->stavenumber <= out_laststave)
    {
    zerocopy->level = depthvector[zerocopy->stavenumber];
    if (zerocopy->level > out_sysblock->systemdepth) zerocopy->level = -1; else
      {
      zerocopystr *zz = curmovt->zerocopy;
      zerocopycount++;
      while (zz != zerocopy)
        {
        if (zz->level == zerocopy->level) { zz->level = -1; zerocopycount--; }
        zz = zz->next;
        }
      }
    }
  else zerocopy->level = -1;
  }

/* If we are outputting more than one copy of stave zero, we must set up
private contstr blocks for each one. */

if (zerocopycount > 1)
  {
  for (zerocopystr *zerocopy = curmovt->zerocopy; zerocopy != NULL;
       zerocopy = zerocopy->next)
    {
    if (zerocopy->level >= 0)
      {
      zerocopy->cont = mem_get_cached((void **)(&main_freezerocontblocks),
        sizeof(contstr));

      /* The cast of the first argument here avoids the silly compiler error
      message "expected 'contstr *' {aka 'struct constr *'} but argument is of
      type 'struct contstr *'", which makes no sense. */

      misc_copycontstr((contstr *)(zerocopy->cont), out_sysblock->cont, 0,
        FALSE);
      }
    }
  }

/* Output the joining signs required at the left hand side of the system of
staves, unless there is only one stave. */

if (out_sysblock->systemdepth > 0)
  {
  uint64_t bracketed;
  int bar = curbarnumber;

  TRACE("joining signs\n");

  /* This applies to the set of staves; no specific stave magnification. */

  out_stavemagn = 1000;
  out_pitchmagn = out_stavemagn/2;

  /* If there is an indent set, do true lefthand joins if required. Then adjust
  the bar number to point to the one where the rest of the joins will appear.
  */

  if (out_sysblock->joinxposition != out_sysblock->startxposition)
    {
    if (MFLAG(mf_startjoin))
      {
      out_joinxposition = out_sysblock->startxposition + out_sysblock->xjustify;
      (void)dojoinsign(curmovt->joinlist, depthvector, 0, join_barline,
        bar_single, bar);
      (void)dojoinsign(curmovt->joindottedlist, depthvector, 0, join_barline,
        bar_dotted, bar);
      }
    bar += curmovt->startbracketbar;
    }

  /* Set x position for all remaining signs */

  out_joinxposition = out_sysblock->joinxposition + out_sysblock->xjustify;

  /* Deal with solid and dotted lines */

  (void)dojoinsign(curmovt->joinlist, depthvector, 0, join_barline, bar_single,
    bar);
  (void)dojoinsign(curmovt->joindottedlist, depthvector, 0, join_barline,
    bar_dotted, bar);

  /* Deal with (thick) brackets; bracketed gets set to the bracketed staves */

  bracketed = dojoinsign(curmovt->bracketlist, depthvector, 0, join_bracket, 0,
    bar);

  /* Deal with thin brackets */

  (void)dojoinsign(curmovt->thinbracketlist, depthvector, bracketed,
    join_thinbracket, 0, bar);

  /* Deal with braces */

  (void)dojoinsign(curmovt->bracelist, depthvector, bracketed, join_brace, 0,
    bar);

  /* Deal with system separators; note that ofi_line() has its y origin at
  out_ystave. */

  if (!firstsystem && curmovt->systemseplength != 0)
    {
    double rangle = ((double)curmovt->systemsepangle)*atan(1.0)/45000.0;
    int32_t x0 = out_joinxposition + curmovt->systemsepposx;
    int32_t y0 = 28000 + curmovt->systemsepposy;
    int32_t dx = (int32_t)(((double)curmovt->systemseplength) * cos(rangle));
    int32_t dy = (int32_t)(((double)curmovt->systemseplength) * sin(rangle));

    out_ystave = out_yposition;   /* Top stave */
    ofi_line(x0, y0, x0 + dx, y0 + dy, curmovt->systemsepwidth, 0);
    y0 -= 2* curmovt->systemsepwidth;
    ofi_line(x0, y0, x0 + dx, y0 + dy, curmovt->systemsepwidth, 0);
    }
  }

/* Now go through the bars, outputting all the staves for each in turn. */

out_startlinebar = TRUE;
out_barx = out_sysblock->firstnoteposition + out_sysblock->xjustify;
out_lastbarlinex = out_barx;                 /* for continued nth time marks */
out_lastbarwide = FALSE;

for (;;)
  {
  curbarnumber = out_setbar(zerocopycount);
  out_barx = out_lastbarlinex + out_sysblock->barlinewidth;
  if (curbarnumber > out_sysblock->barend) break;
  out_startlinebar = FALSE;
  }

/* Output a key or time change at line end if required, adjusting the position
for a non-stretched barline. */

if ((out_sysblock->flags & sysblock_warn) != 0)
  {
  out_barx += curmovt->barlinespace - out_sysblock->barlinewidth;
  warnbar();
  }

/* Tidy the main cont data structure and any copies that have been set up for
multiple stave zeros. */

misc_tidycontstr(wk_cont, out_laststave);
if (zerocopycount > 1)
  {
  for (zerocopystr *zerocopy = curmovt->zerocopy; zerocopy != NULL;
       zerocopy = zerocopy->next)
    {
    if (zerocopy->level >= 0)
      {
      /* The cast of the first argument here avoids the silly compiler error
      message "expected 'contstr *' {aka 'struct constr *'} but argument is of
      type 'struct contstr *'", which makes no sense. */

      misc_tidycontstr((contstr *)(zerocopy->cont), 0);
      mem_free_cached((void **)(&main_freezerocontblocks), zerocopy->cont);
      }
    }
  }

/* Now we know the final x position, we can output the staves. Nothing is
output for stave 0, as it is always overprinted. Also, nothing is output for a
stave with "omitempty" set, as it deals with its own stave lines. */

out_ystave = out_yposition;
lastystave = -1;

int32_t leftbarx = out_sysblock->startxposition + out_sysblock->xjustify;
int32_t rightbarx = out_lastbarlinex;

if (rightbarx > leftbarx) for (curstave = 1; curstave <= out_laststave;
    curstave++)
  {
  TRACE("lines for stave %d\n", curstave);

  if (mac_isbit(out_sysblock->notsuspend, curstave))
    {
    if (out_ystave != lastystave)
      {
      stavestr *ss = curmovt->stavetable[curstave];
      if (!ss->omitempty && ss->stavelines > 0)
        {
        out_stavemagn = curmovt->stavesizes[curstave];
        ofi_stave(leftbarx, out_ystave, rightbarx, ss->stavelines);
        lastystave = out_ystave;
        }
      }
    out_ystave += out_sysblock->stavespacing[curstave];
    }
  }

/* If any drawing items have been saved up for execution after the stave lines
have been drawn, do them now. The overdrawstr blocks are cached for re-use, but
the x, y, c vectors, being of different sizes, are not. */

ofi_getcolour(save_colour);
while (out_overdraw != NULL)
  {
  overdrawstr *this = out_overdraw;
  out_overdraw = this->next;

  if (this->texttype)
    {
    ofi_setcolour(this->d.t.colour);
    out_string(this->d.t.text, &(this->d.t.fdata), this->d.t.xx,
      this->d.t.yy, this->d.t.flags);
    }
  else
    {
    ofi_setcolour(this->d.g.colour);
    ofi_setdash(this->d.g.dash[0], this->d.g.dash[1]);
    out_ystave = this->d.g.ystave;
    ofi_path(this->d.g.x, this->d.g.y, this->d.g.c, this->d.g.linewidth);
    mem_free_cached((void **)(&main_freeoverdrawstr), this);
    }
  }
ofi_setcolour(save_colour);

TRACE("out_system() end\n");
}



/*************************************************
*              Output one page                   *
*************************************************/

/*
Arguments:  none
Return:     nothing
*/

void
out_page(void)
{
BOOL firstsystem = TRUE;
BOOL lastwasheading = FALSE;
int32_t topspace = curpage->topspace;

TRACE("out_page() start\n");

/* Initialize bounding box - note in y-downwards coordinates */

out_bbox[0] = INT32_MAX;
out_bbox[2] = 0;
out_bbox[1] = 0;
out_bbox[3] = INT32_MAX;

/* Initialize for outputting the music. There's a testing option that forces
the output to be red for visual comparison against black. */

if ((main_testing & mtest_forcered) != 0)
  {
  int32_t red[] = { 1000, 0, 0 };
  ofi_setcolour(red);
  }
else ofi_setgray(0);

ofi_setdash(0, 0);
ofi_setcapandjoin(caj_butt);
out_yposition = 0;
out_drawstackptr = 0;

/* Output headings and systems. Note that we must insert a stave's gap (plus
one) between the last heading line and the first system (to account for the
system depth). Note also that we insert the topspace *after* pageheadings, but
*before* non-page headings. */

for (out_sysblock = curpage->sysblocks;
     out_sysblock != NULL;
     out_sysblock = out_sysblock->next)
  {
  /* On a change of movement, check for a change of margin, and re-initialize
  the list of last-used barline styles. */

  if (out_sysblock->movt != curmovt)
    {
    curmovt = out_sysblock->movt;
    if (curmovt->leftmargin >= 0) print_xmargin = curmovt->leftmargin; else
      {
      print_xmargin = (print_sheetwidth - curmovt->linelength)/2 +
        13000000/(2*main_magnification);
      if (print_xmargin < 20000) print_xmargin = 20000;
      }
    }

  /* Deal with a heading */

  if (!out_sysblock->is_sysblock)
    {
    curbarnumber = -1;
    out_stavemagn = 1000;
    out_pitchmagn = out_stavemagn/2;
    if (!((headblock *)out_sysblock)->pageheading)
      {
      out_yposition += topspace;
      topspace = 0;
      }
    out_heading((headblock *)out_sysblock);
    lastwasheading = firstsystem = TRUE;
    }

  /* Deal with a system */

  else
    {
    if (lastwasheading) out_yposition += 17000;
    out_yposition += topspace;
    topspace = 0;

    if (out_yposition - 48000 < out_bbox[3]) out_bbox[3] = out_yposition - 48000;
    if (out_yposition + out_sysblock->systemdepth + 32000 > out_bbox[1])
      out_bbox[1] = out_yposition + out_sysblock->systemdepth + 32000;

    out_system(firstsystem);
    firstsystem = FALSE;

    if (out_sysblock->xjustify - 10000 < out_bbox[0])
      out_bbox[0] = out_sysblock->xjustify - 10000;
    if (out_lastbarlinex + 4000 > out_bbox[2])
      out_bbox[2] = out_lastbarlinex + 4000;

    if ((out_sysblock->flags & sysblock_noadvance) == 0)
      out_yposition += out_sysblock->systemdepth + out_sysblock->systemgap;
    lastwasheading = FALSE;
    }
  }

/* Deal with any footings */

if (curpage->footing != NULL)
  {
  curbarnumber = -1;
  out_stavemagn = 1000;
  out_pitchmagn = out_stavemagn/2;
  out_yposition = main_pagelength + 20000000/main_magnification;
  out_heading(curpage->footing);
  }

TRACE("out_page() end\n");
}

/* End of out.c */
