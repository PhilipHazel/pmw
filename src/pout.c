/*************************************************
*       PMW Common PostScript/PDF functions      *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2024 */
/* This file last modified: January 2025 */

#include "pmw.h"

/* This file contains functions that are common to both PostScript and PDF
output. Some are called directly from ps.c and pdf.c; others are called via the
indirection interface. */


/*************************************************
*              Static variables                  *
*************************************************/

/* In order to support the PMW-Music font in OpenType format, we have to treat
some characters specially. In the .pfa font these characters move leftwards or
up or down after printing, a facility that isn't available in OpenType. These
strings define the characters that need special treatment:

  umovechars are movement-only characters unsupported in OpenType
  amovechars are all movement-only characters
  vshowchars are printing characters with vertical movement
*/

static const uschar *umovechars = CUS"\166\167\170\171\173\174\176\271\272\273\274";
static const uschar *amovechars = CUS"\040\166\167\170\171\172\173\174\175\176\271\272\273\274";
static const uschar *vshowchars = CUS"\221\222\244\245";



/*************************************************
*        Initialize page list data               *
*************************************************/

/* This function is called at the start of the output phase. Its job is to set
up the page number previous to the the first page to be printed, in
ps_curnumber, and to set ps_curlist to point the first page selection item.

Argument:    TRUE if printing is to be in reverse order
Returns:     nothing
*/

void
pout_setup_pagelist(BOOL reverse)
{
pout_curlist = print_pagelist;
if (reverse)
  {
  if (pout_curlist == NULL)
    pout_curnumber = print_lastpagenumber + 1;
  else
    {
    while (pout_curlist->next != NULL) pout_curlist = pout_curlist->next;
    pout_curnumber = pout_curlist->last + 1;
    }
  }
else
  {
  if (pout_curlist == NULL) pout_curnumber = page_firstnumber - 1;
    else pout_curnumber = pout_curlist->first - 1;
  }
}



/*************************************************
*         Set ymax and origin translation        *
*************************************************/

/* Both PostScript and PDF work from the usual bottom of page origin, but PMW
data runs downwards from the top. This function sets pout_ymax to the y
coordinate of the top of the page for use by the pouty() macro, which does the
conversion.

If an imposition is set, take the opportunity of setting the relevant values
for origin translation.

HISTORY:
Before the invention of the imposition parameter, we computed this from the
pagelength, but with some minima imposed. For compatibility, this was
originally unchanged unchanged for cases when imposition is defaulted. This was
the code:

  if (main_landscape)
    {
    if (main_truepagelength < 492000)
      pout_ymax = mac_muldiv(526000, 1000, print_magnification);
        else pout_ymax = main_truepagelength + 34000;
    }
  else
    {
    if (main_truepagelength < 720000)
      pout_ymax = mac_muldiv(770000, 1000, print_magnification);
        else pout_ymax = main_truepagelength + 50000;
    }

With the advent of PDF output this caused a lot of problems, so we change back
to computing pout_ymax from the page length.
END HISTORY.

Arguments:
  pw        pointer to imposition width translation
  pd        pointer to imposition depth translation

Returns:    nothing
*/

void
pout_set_ymax_etc(int32_t *pw, int32_t *pd)
{
pout_ymax = main_truepagelength + (main_landscape? 34000 : 50000);

switch(print_imposition)
  {
  case pc_a5ona4:
  *pw = 595000;
  *pd = 842000;
  pout_ymax = main_truepagelength + 50000;
  break;

  case pc_a4ona3:
  *pw = 842000;
  *pd = 1190000;
  pout_ymax = main_truepagelength + 50000;
  break;
  }
}



/*************************************************
*            Get next page to print              *
*************************************************/

/* This is called from pout get_pages() below to get the next specified page,
skipping any that do not exist. We have to handle both backwards and forward
movement.

Argument:  nothing
Returns:   pointer to a pagestr, or NULL if no more.
*/

static pagestr *
nextpage(void)
{
for (;;)
  {
  pagestr *yield;

  /* Start by getting the next page's number into pout_curnumber. If no page
  selection was specified it's just an increment or decrement of the current
  number. Otherwise, seek the next page from the list of required pages. */

  /* Find next page number in forwards order. */

  if (!print_reverse)
    {
    pout_curnumber++;
    if (pout_curlist == NULL)  /* No page selection was specified */
      {
      if (pout_curnumber > print_lastpagenumber) return NULL;
      }
    else if (pout_curnumber > pout_curlist->last)
      {
      if (pout_curlist->next == NULL) return NULL;
      pout_curlist = pout_curlist->next;
      pout_curnumber = pout_curlist->first;
      }
    }

  /* Find next page number in reverse order. */

  else
    {
    pout_curnumber--;
    if (pout_curlist == NULL)  /* No page selection was specified */
      {
      if (pout_curnumber < page_firstnumber) return NULL;
      }
    else if (pout_curnumber < pout_curlist->first)
      {
      if (pout_curlist->prev == NULL) return NULL;
      pout_curlist = pout_curlist->prev;
      pout_curnumber = pout_curlist->last;
      }
    }

  /* Search for a page with the require number; if not found, loop to look for
  the next one. If we are in pamphlet mode with no explicit page list and the
  page number is past halfway and the mate exists, don't return the page. */

  for (yield = main_pageanchor; yield != NULL; yield = yield->next)
    {
    if (yield->number == pout_curnumber) break;
    }
  if (yield == NULL) continue;   /* Page not found */

  if (print_pamphlet &&
      pout_curlist == NULL &&
      pout_curnumber > print_lastpagenumber/2)
    {
    pagestr *p = main_pageanchor;
    uint32_t mate = print_lastpagenumber - pout_curnumber + 1;
    while (p != NULL)
      {
      if (p->number == mate) break;
      p = p->next;
      }
    if (p != NULL) continue;     /* Skip this page */
    }

  return yield;
  }
}



/*************************************************
*          Get next page(s) to print             *
*************************************************/

/* The function called from ps_go() and pdf_go(). It returns page structures
for one or two pages, depending on the imposition. The yield is FALSE if there
are no more pages.

Arguments:
  p1         where to put a pointer to the first page
  p2         where to put a pointer to the second page

Returns:     FALSE if there are no more pages
*/

BOOL
pout_get_pages(pagestr **p1, pagestr **p2)
{
usint n;
BOOL swap = FALSE;
*p2 = NULL;

/* Loop for skipping unwanted pages (side selection). For pamphlet printing,
side 1 contains odd-numbered pages less than half the total, and even-numbered
pages greater than half. We may get either kind of page given. */

for (;;)
  {
  BOOL nodd;

  if ((*p1 = nextpage()) == NULL) return FALSE;
  n = (*p1)->number;
  nodd = (n & 1) != 0;

  if (!print_pamphlet || n <= print_lastpagenumber/2)
    {
    if ((print_side1 && nodd) || (print_side2 && !nodd)) break;
    }
  else
    {
    if ((print_side1 && !nodd) || (print_side2 && nodd)) break;
    }
  }

/* Handle 1-up printing: nothing more to do */

if (print_imposition != pc_a5ona4 && print_imposition != pc_a4ona3)
  return TRUE;

/* Handle 2-up printing. For non-pamphlet ordering, just get the next page and
set the order swap flag if required. */

if (!print_pamphlet)
  {
  if ((*p2 = nextpage()) == NULL) pout_curnumber--;  /* To get correct display */
  swap = print_reverse;
  }

/* For pamphlet printing, find the mate of the first page, and set the swap
flag if necessary, to ensure the odd-numbered page is on the right. */

else
  {
  n = print_lastpagenumber - n + 1;
  swap = (n & 1) == 0;
  *p2 = main_pageanchor;
  while (*p2 != NULL)
    {
    if ((*p2)->number == n) break;
    *p2 = (*p2)->next;
    }
  }

/* Swap page order if necessary */

if (swap)
  {
  pagestr *temp = *p1;
  *p1 = *p2;
  *p2 = temp;
  }

return TRUE;
}



/*************************************************
*            Set gray level or colour            *
*************************************************/

/* All that happens here is that the gray level or colour is remembered for
later use.

Argument:  the colour or the gray level
Returns:   nothing
*/

void
pout_setcolour(int32_t *colour)
{
memcpy(pout_wantcolour, colour, 3 * sizeof(int32_t));
pout_changecolour =
    pout_wantcolour[0] != pout_curcolour[0] ||
    pout_wantcolour[1] != pout_curcolour[1] ||
    pout_wantcolour[2] != pout_curcolour[2];
}

void
pout_setgray(int32_t gray)
{
int temp[3];
temp[0] = temp[1] = temp[2] = gray;
pout_setcolour(temp);
}



/*************************************************
*            Retrieve current colour             *
*************************************************/

/* Used to preserve the colour over a drawing.

Argument: pointer to where to put the colour values
Returns:  nothing
*/

void
pout_getcolour(int32_t *colour)
{
memcpy(colour, pout_wantcolour, 3 * sizeof(int32_t));
}



/*************************************************
*   Get length of a string that is to be output  *
*************************************************/

int32_t
pout_getswidth(uint32_t *s, usint f, fontstr *fs, int32_t *plast_width,
  int32_t *plast_r2ladjust)
{
int32_t swidth = 0;
int32_t last_width = 0;
int32_t last_r2ladjust = 0;
kerntablestr *ktable = fs->kerns;

for (uint32_t *p = s; *p != 0; p++)
  {
  uint32_t c = PCHAR(*p);

  /* Non-standardly encoded font */

  if ((fs->flags & ff_stdencoding) == 0)
    {
    int cj = c;

    /* Fudges for various special cases in the music font, where the
    adjustment bounding box is taken from another character. */

    if (f == font_mf)
      {
      switch(c)
        {
        case 'J':       /* Additional stem characters - adjust as for note */
        case 'K':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        cj = '5';
        break;

        case '7':       /* Up quaver - adjust as for down quaver */
        cj = '8';
        break;

        case '9':       /* Up semiquaver - adjust as for down semiquaver */
        cj = ':';
        break;

        default:
        break;
        }
      }

    last_width = fs->widths[c];
    last_r2ladjust = fs->r2ladjusts[cj];
    }

  /* Standardly encoded font */

  else
    {
    last_width = fs->widths[c];
    last_r2ladjust = fs->r2ladjusts[c];
    }

  /* Amass the total string width */

  swidth += last_width;

  /* If there is another character, scan the kerning table. At present,
  kerning is supported only for characters whose code points are no greater
  than 0xffff. */

  if (main_kerning && fs->kerncount > 0 && p[1] != 0 && c <= 0xffffu)
    {
    uint32_t cc = PCHAR(p[1]);
    if (cc <= 0xffffu)
      {
      int bot = 0;
      int top = fs->kerncount;
      uint32_t pair = (c << 16) | cc;

      while (bot < top)
        {
        int mid = (bot + top)/2;
        kerntablestr *k = &(ktable[mid]);
        if (pair == k->pair)
          {
          swidth += k->kwidth;
          break;
          }
        if (pair < k->pair) top = mid; else bot = mid + 1;
        }
      }
    }
  }

*plast_width = last_width;
*plast_r2ladjust = last_r2ladjust;
return swidth;
}


/*************************************************
*  Output a text string and change current point *
*************************************************/

/* The x and y coordinates are updated if requested - note that y goes
downwards.

Special action is needed for the music font. The PostScript Type 1 font
contains a few characters that output nothing but whose horizontal spacial
increment is negative and also some characters (both blank and non-blank) with
vertical movement - features which are useful for building composite things
from strings in this font. Annoyingly, TrueType/OpenType fonts do not permit
negative horizontal advance values, nor both horizontal and vertical movement
in the same font, and a Truetype or OpenType font is now desirable because Type
1 fonts are becoming obsolete.

We get round this restriction by separating out substrings that start with an
unsupported moving character and consist only of position moving characters.
The already-existing code that suppresses the output of a substring that just
moves the position then kicks in. Also, we terminate a substring after any
printing character that has vertical movement.

Arguments:
  s             the PMW string
  fdata         points to font instance data
  xu            pointer to the x coordinate
  yu            pointer to the y coordinate
  update        if TRUE, update the x,y positions
  basic_string  the PS or PDF function that does basic string output

Returns:        nothing
*/

void
pout_string(uint32_t *s, fontinststr *fdata, int32_t *xu, int32_t *yu,
  BOOL update, void (*basic_string)(uint32_t *, usint, fontinststr *,
  int32_t, int32_t))
{
int32_t x = *xu;
int32_t y = *yu;

/* Process the string in substrings in which all the characters have the same
font, handling the music font specially, as noted above. */

while (*s != 0)
  {
  int32_t stringheight;
  uint32_t save1;
  uint32_t *p = s;
  usint f = PFONT(*s);

  /* Handle strings in the music font */

  if ((f & ~font_small) == font_mf)
    {
    BOOL skip = TRUE;

    /* Starts with an unsupported mover, leave skip TRUE so no output will
    happen, but the current point will be adjusted. */

    if (Ustrchr(umovechars, PCHAR(*s)) != NULL)
      {
      while (*s != 0 && PFONT(*s) == f &&
             Ustrchr(amovechars, PCHAR(*s)) != NULL) s++;
      }

    /* Does not start with an unsupported mover; end when one is encountered.
    Also end (but include the character) if a vertical movement is needed. */

    else
      {
      while (*s != 0 && PFONT(*s) == f)
        {
        uint32_t c = PCHAR(*s);
        if (Ustrchr(umovechars, c) != NULL) break;
        if (Ustrchr(amovechars, c) == NULL) skip = FALSE;
        s++;
        if (Ustrchr(vshowchars, c) != NULL) break;
        }
      }

    /* End substring */

    save1 = *s;
    *s = 0;          /* Temporary terminator */

    /* If this substring consists entirely of printing-point-moving characters
    (skip == TRUE) we do not need actually to output it. Otherwise, we can
    temporarily remove any trailing moving characters while outputting as they
    don't achieve anything useful. Of course they need to be restored
    afterwards so that the position update below works correctly. */

    if (!skip)
      {
      uint32_t save2;
      uint32_t *t = s - 1;
      while (t > p && Ustrchr(amovechars, PCHAR(*t)) != NULL) t--;
      save2 = *(++t);
      *t = 0;
      basic_string(p, f, fdata, x, y);
      *t = save2;
      }
    }

  /* The font is not the music font */

  else
    {
    while (*s != 0 && PFONT(*s) == f) s++;
    save1 = *s;
    *s = 0;          /* Temporary terminator */
    basic_string(p, f, fdata, x, y);
    }

  /* Update the position. Note that we must use the original size (in fdata)
  because string_width() does its own adjustment for small caps and the small
  music font. */

  x += string_width(p, fdata, &stringheight);
  y -= stringheight;
  *s = save1;   /* Restore */
  }

/* Pass back the end position if required. */

if (update)
  {
  *xu = x;
  *yu = y;
  }
}



/*************************************************
*       Output one virtual musical character     *
*************************************************/

/* Certain musical characters are given identity numbers in a virtual music
font that may or may not correspond directly to characters in the actual music
font. The table called out_mftable[] defines how they are to be printed.

Arguments:
  x          the x coordinate
  y          the y coordinate
  ch         the character's identity number
  pointsize  the point size
  basic_string  the PS or PDF function that does basic string output

Returns:     nothing
*/

void
pout_muschar(int32_t x, int32_t y, uint32_t ch, int32_t pointsize,
  void (*basic_string)(uint32_t *, usint, fontinststr *, int32_t, int32_t))
{
int32_t xfudge = 0;

/* Use a local font data structure which has no rotation. */

pout_mfdata.size = pointsize;

/* There may be a chain of strings/displacements */

for (mfstr *p = out_mftable[ch]; p != NULL; p = p->next)
  {
  int i = 0;
  int32_t nxfudge = 0;
  uint32_t c = p->ch;
  uint32_t s[8];

  /* Nasty fudge for bracketed accidentals in right-to-left mode: when the
  brackets come as individual values, swap them round and fudge the spacing of
  the remaining chars. This is needed for flats, in practice. */

  if (main_righttoleft) switch (c)
    {
    case 139: c = 140; nxfudge = -1600; break;
    case 140: c = 139; break;
    case 141: c = 142; nxfudge = -1600; break;
    case 142: c = 141; break;
    default: break;
    }

  /* Extract up to 4 music font characters from c */

  while (c != 0)
    {
    s[i++] = c & 255;
    c >>= 8;
    }
  s[i] = 0;

  basic_string(s, font_mf, &pout_mfdata,
    x + mac_muldiv(p->x, pointsize, 10000) + xfudge,
    y - mac_muldiv(p->y, pointsize, 10000));

  xfudge += nxfudge;
  }
}



/*************************************************
*        Computation for drawing a beam          *
*************************************************/

/* This function is called several times for a multi-line beam, with the level
number increasing each time. Information about the slope and other features is
in beam_* variables.

Arguments:
  px0           pointer to starting x coordinate, relative to start of bar
  px1           pointer to ending x coordinate, relative to start of bar
  py0           where to return start y coordinate
  py1           where to return end y coordinate
  pdepth        where to return depth value
  level         level number
  levelchange   set nonzero for accellerando and ritardando beams

The x coordinates are updated.

Returns:        nothing
*/

void
pout_beam(int32_t *px0, int32_t *px1, int32_t *py0, int32_t *py1,
  int32_t *pdepth, int level, int levelchange)
{
int sign = (beam_upflag)? (+1) : (-1);
*pdepth = -out_stavemagn*((n_fontsize * sign *
  (int)(((double)curmovt->beamthickness) /
    cos(atan((double)beam_slope/1000.0))))/10000)/1000;

*py1 = *py0 = out_ystave - beam_firstY +
  mac_muldiv(n_fontsize, (level - 1) * sign * 3 * out_stavemagn, 10000);

*py0 -= mac_muldiv(*px0-beam_firstX, beam_slope, 1000);
*py1 -= mac_muldiv(*px1-beam_firstX, beam_slope, 1000);

/* For accellerando and ritardando beams, adjust the ends, and make a little
bit thinner. */

if (levelchange != 0)
  {
  int32_t adjust = mac_muldiv(n_fontsize,
    abs(levelchange) * sign * 4 * out_stavemagn, 10000);
  *pdepth = (*pdepth*17)/20;
  if (levelchange < 0)
    {
    *py0 += adjust;
    *py1 += adjust/8;
    }
  else
    {
    *py0 += adjust/8;
    *py1 += adjust;
    }
  }

/* Get absolute x values */

*px0 += out_barx;
*px1 += out_barx;

/* When printing right-to-left, adjust by one note's printing adjustment.
The value can't just be read from the font, as it needs fiddling, so we
just fudge a fixed value. */

if (main_righttoleft)
  {
  int32_t adjust = sign * mac_muldiv(n_fontsize/2, out_stavemagn, 1000);
  *px0 -= adjust;
  *px1 -= adjust;
  }
}

/* End of pout.c */
