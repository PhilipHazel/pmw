/*************************************************
*             PMW PostScript functions           *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: May 2021 */
/* This file last modified: June 2022 */

#include "pmw.h"

/* This file contains code for outputting things in PostScript */


/************************************************
*             Static variables                  *
************************************************/

static BOOL ps_EPS = FALSE;
static BOOL ps_slurA = FALSE;

static uint32_t ps_caj;
static int  ps_chcount;
static int  ps_curfont;
static BOOL ps_curfontX;

static fontinststr ps_mfdata = { NULL, 0, 0 };
static fontinststr ps_curfontdata = { NULL, 0, 0 };
static int32_t ps_fmatrix[6];

static stavelist *ps_curlist;
static uint32_t ps_curnumber;

static int32_t  ps_gray;
static int32_t  ps_ymax;

static uschar  *ps_IdStrings[font_tablen+1];


/************************************************
*             Data tables                       *
************************************************/

/* Characters in the music font for stave fragments with different numbers of
lines, both 100 points long and 10 points long. */

static uint8_t stavechar10[] = { 0, 'G', 247, 248, 249, 'F', 250 };
static uint8_t stavechar1[] =  { 0, 'D', 169, 170, 171, 'C', 172 };

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



/************************************************
*                 Macros                        *
************************************************/

/* Coordinate translation for character output */

#define psxtran(x) ((x) + print_xmargin)

#define psytran(y) (ps_ymax - (y))



/*************************************************
*        Initialize page list data               *
*************************************************/

/* This function is called at the start of the output phase. Its job is to set
up the page number previous to the the first page to be printed, in
ps_curnumber, and to set ps_curlist to point the first page selection item.

Argument:    TRUE if printing is to be in reverse order
Returns:     nothing
*/

static void
setup_pagelist(BOOL reverse)
{
ps_curlist = print_pagelist;
if (reverse)
  {
  if (ps_curlist == NULL)
    ps_curnumber = print_lastpagenumber + 1;
  else
    {
    while (ps_curlist->next != NULL) ps_curlist = ps_curlist->next;
    ps_curnumber = ps_curlist->last + 1;
    }
  }
else
  {
  if (ps_curlist == NULL) ps_curnumber = page_firstnumber - 1;
    else ps_curnumber = ps_curlist->first - 1;
  }
}



/*************************************************
*            Get next page to print              *
*************************************************/

/* This is called from get_pages() to get the next specified page, skipping any
that do not exist. We have to handle both backwards and forward movement.

Argument:  nothing
Returns:   pointer to a pagestr, or NULL if no more.
*/

static pagestr *
nextpage(void)
{
for (;;)
  {
  pagestr *yield;

  /* Start by getting the next page's number into ps_curnumber. If no page
  selection was specified it's just an increment or decrement of the current
  number. Otherwise, seek the next page from the list of required pages. */

  /* Find next page number in forwards order. */

  if (!print_reverse)
    {
    ps_curnumber++;
    if (ps_curlist == NULL)  /* No page selection was specified */
      {
      if (ps_curnumber > print_lastpagenumber) return NULL;
      }
    else if (ps_curnumber > ps_curlist->last)
      {
      if (ps_curlist->next == NULL) return NULL;
      ps_curlist = ps_curlist->next;
      ps_curnumber = ps_curlist->first;
      }
    }

  /* Find next page number in reverse order. */

  else
    {
    ps_curnumber--;
    if (ps_curlist == NULL)  /* No page selection was specified */
      {
      if (ps_curnumber < page_firstnumber) return NULL;
      }
    else if (ps_curnumber < ps_curlist->first)
      {
      if (ps_curlist->prev == NULL) return NULL;
      ps_curlist = ps_curlist->prev;
      ps_curnumber = ps_curlist->last;
      }
    }

  /* Search for a page with the require number; if not found, loop to look for
  the next one. If we are in pamphlet mode with no explicit page list and the
  page number is past halfway and the mate exists, don't return the page. */

  for (yield = main_pageanchor; yield != NULL; yield = yield->next)
    {
    if (yield->number == ps_curnumber) break;
    }
  if (yield == NULL) continue;   /* Page not found */

  if (print_pamphlet &&
      ps_curlist == NULL &&
      ps_curnumber > print_lastpagenumber/2)
    {
    pagestr *p = main_pageanchor;
    uint32_t mate = print_lastpagenumber - ps_curnumber + 1;
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

/* The function called from ps_go(). It returns page structures for one or two
pages, depending on the imposition. The yield is FALSE if there are no more
pages.

Arguments:
  p1         where to put a pointer to the first page
  p2         where to put a pointer to the second page

Returns:     FALSE if there are no more pages
*/

static BOOL
get_pages(pagestr **p1, pagestr **p2)
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
  if ((*p2 = nextpage()) == NULL) ps_curnumber--;  /* To get correct display */
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
*         Output ASCII formatted string          *
*************************************************/

/* This handles standard C formatting strings. To keep the PostScript readable
we wrap it around 72 characters wide. Omit space characters at the starts of
lines. */

static void
ps_printf(const char *format, ...)
{
int len;
char buff[256];
char *p = buff;

va_list ap;
va_start(ap, format);
len = vsprintf(buff, format, ap);
va_end(ap);

if (ps_chcount > 0 && ps_chcount + len > 72)
  {
  fputc('\n', ps_file);
  ps_chcount = 0;
  }

while (ps_chcount == 0 && *p == ' ') { p++; len--; }
fputs(p, ps_file);
ps_chcount = (p[len-1] == '\n')? 0 : ps_chcount + len;
}



/*************************************************
*         Check whether font needs changing      *
*************************************************/

/* The X argument is used when the character is > 255, indicating that we want
the second version of the font, with the alternative encoding.

Arguments:
  f              font number
  fdata          the font's size and matrix
  X              TRUE if it's the extended font we want

Returns:         TRUE if the font needs changing
*/

static BOOL
ps_needchangefont(int f, fontinststr *fdata, BOOL X)
{
if (f != ps_curfont || X != ps_curfontX ||
    fdata->size != ps_curfontdata.size) return TRUE;

/* The font and size are the same; check transformation. */

if (fdata->matrix == NULL) return ps_curfontdata.matrix != NULL;
if (ps_curfontdata.matrix == NULL) return TRUE;

for (int i = 0; i <= 3; i++)
  if (fdata->matrix[i] != ps_curfontdata.matrix[i]) return TRUE;

return FALSE;
}


/*************************************************
*           Make a given font current            *
*************************************************/

/* This function is called when we know that a font change is needed.

Arguments:
  f              current font number
  fdata          the font's size and matrix
  X              TRUE if it's the extended font we want

Returns:         nothing
*/

static void
ps_setfont(int f, fontinststr *fdata, BOOL X)
{
int32_t *matrix = fdata->matrix;
int32_t size = fdata->size;

ps_curfont = f;
ps_curfontX = X;

ps_curfontdata = *fdata;

if (fdata->matrix != NULL)
  {
  ps_curfontdata.matrix = ps_fmatrix;  /* Local copy of matrix */
  memcpy(ps_fmatrix, fdata->matrix, 6*sizeof(int32_t));
  }

/* If the transformation is null don't waste time/space by writing it. */

if (matrix == NULL)
  {
  ps_printf(" %s%s %s ss%s", ps_IdStrings[f], X? "X" : "", sff(size),
    main_righttoleft? "r" : "");
  }

/* There is a transform */

else
  {
  ps_printf(" %s%s %s sm%s", ps_IdStrings[f], X? "X" : "",
    SFF("[%f %f %f %f 0 0]",
    mac_muldiv(matrix[0], size, 65536),
    mac_muldiv(matrix[1], size, 65536),
    mac_muldiv(matrix[2], size, 65536),
    mac_muldiv(matrix[3], size, 65536)),
    main_righttoleft? "r" : "");
  }
}



/*************************************************
*             End a text string                  *
*************************************************/

/* This function writes the terminating ')' and an appropriate command.

Arguments:
  absolute      TRUE if the coordinates are absolute, else relative
  w             extra space width
  x             x-coordinate
  y             y-coordinate
*/

static void
ps_endstring(BOOL absolute, int32_t w, int32_t x, int32_t y)
{
fputc(')', ps_file);   /* Does not check ps_chcount */
ps_chcount++;

if (absolute)
  {
  if (w != 0) ps_printf("%s", SFF("%f %f %f ws", w, psxtran(x), psytran(y)));
    else ps_printf("%s", SFF("%f %f s", psxtran(x), psytran(y)));
  }
else if (x == 0 && y == 0)  /* Relative, but no movement */
  {
  if (w != 0) ps_printf("%s wsh", sff(w)); else ps_printf("sh");
  }
else
  {
  if (w != 0) ps_printf("%s", SFF("%f %f %f wrs", w, x, y));
    else ps_printf("%s", SFF("%f %f rs", x, y));
  }
}



/*************************************************
*               Basic string output code         *
*************************************************/

/* This function outputs a PMW string, all of whose characters have the same
font. The font is passed explicitly rather than taken from the characters so
that font_mu can become font_mf at smaller size, and font_sc can have the small
caps bit removed. All code points were translated earlier if necessary.

Arguments:
  s            the PMW string
  f            the font
  fdata        points to font instance size etc data
  absolute     TRUE if the coordinates are absolute, else relative
  x            the x coordinate
  y            the y coordinate

Returns:       nothing
*/

static void
ps_basic_string(uint32_t *s, usint f, fontinststr *fdata, BOOL absolute,
  int32_t x, int32_t y)
{
fontstr *fs = &(font_list[font_table[f & ~font_small]]);
kerntablestr *ktable = fs->kerns;
fontinststr tfd = *fdata;
BOOL instring = FALSE;
uint32_t *p, *endp;

/* Initialize start and end. */

endp = p = s;
while (*endp != 0) endp++;

/* Adjust the point size in the temporary font instance structure for small
caps and the reduced music font. */

if (f >= font_small)
  {
  f -= font_small;
  if (f == font_mf) tfd.size = (tfd.size * 9) / 10;
    else tfd.size = (tfd.size * curmovt->smallcapsize) / 1000;
  }

/* Check top/bottom of bbox for EPS. Allow 0.4 times the point size for
descenders below and the point size above. */

if (ps_EPS)
  {
  int32_t descender = (4 * tfd.size)/10;
  if (y + descender > out_bbox[1]) out_bbox[1] = y + descender;
  if (y - tfd.size < out_bbox[3]) out_bbox[3] = y - tfd.size;
  }

/* When outputting right-to-left, we need to find the length of the string so
that we can output it from the other end, because the fonts still work
left-to-right. We also need the length for EPS output, in order to set the
bounding box. By this stage there are no special escape characters left in the
string and we know that it's all in the same font. */

if (main_righttoleft || ps_EPS)
  {
  int32_t swidth = 0;
  int32_t last_width = 0;
  int32_t last_r2ladjust = 0;

  for (p = s; *p != 0; p++)
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

  /* For right-to-left, adjust the printing position for the string by the
  length of the string, adjusted for the actual bounding box of the final
  character, and scaled to the font size. For EPS output, adjust the bounding
  box. Both may, of course, happen. */

  if (main_righttoleft)
    x += mac_muldiv(swidth - last_width + last_r2ladjust, tfd.size, 1000);

  if (ps_EPS)
    {
    swidth = mac_muldiv(swidth, tfd.size, 1000);
    if (x + swidth > out_bbox[2]) out_bbox[2] = x + swidth;
    }
  }

/* Generate the output. For a non-standardly encoded font, the values are now
less than 256, whereas for a standardly encoded font they are less than
LOWCHARLIMIT plus some extras. Values above 255 use the second encoding of such
fonts. */

for (p = s; *p != 0; p++)
  {
  uint32_t c = PCHAR(*p);    /* c is the original code point */
  uint32_t pc = c;           /* pc is the code value to print */
  BOOL extended = FALSE;

  if ((fs->flags & ff_stdencoding) != 0)
    {
    if (c >= 256)
      {
      pc -= 256;
      extended = TRUE;
      }
    }

  /* Change between base and extended font if necessary */

  if (ps_needchangefont(f, &tfd, extended))
    {
    if (instring)
      {
      ps_endstring(absolute, tfd.spacestretch, x, y);
      x = y = 0;
      absolute = instring = FALSE;
      }
    ps_setfont(f, &tfd, extended);
    }

  /* Output the code, starting a new string if necessary. If not at the start
  of a line, insert a newline before a new string if this string will make the
  line overlong. */

  if (!instring)
    {
    if (ps_chcount > 0 && ps_chcount + endp - p > 73)
      {
      fputc('\n', ps_file);
      ps_chcount = 0;
      }
    fputc('(', ps_file);
    ps_chcount++;
    instring = TRUE;
    }

  if (pc == '(' || pc == ')' || pc == '\\')
    ps_chcount += fprintf(ps_file, "\\%c", pc);
  else if (pc >= 32 && pc <= 126)
    {
    fputc(pc, ps_file);
    ps_chcount++;
    }
  else ps_chcount += fprintf(ps_file, "\\%03o", pc);

  /* If there is another character, scan the kerning table */

  if (main_kerning && fs->kerncount > 0 && p[1] != 0)
    {
    int32_t xadjust = 0, yadjust = 0;
    int bot = 0;
    int top = fs->kerncount;
    uint32_t cc = PCHAR(p[1]);
    uint32_t pair;

    pair = (c << 16) | cc;
    while (bot < top)
      {
      int mid = (bot + top)/2;
      kerntablestr *k = &(ktable[mid]);
      if (pair == k->pair)
        {
        xadjust = k->kwidth;
        break;
        }
      if (pair < k->pair) top = mid; else bot = mid + 1;
      }

    /* If a kern was found, scale the adjustment to the font size, and for the
    string rotation and transformation, if any. Then close the previous
    substring and arrange that the next be output relative if this is the
    first. */

    if (xadjust != 0)
      {
      xadjust = mac_muldiv(xadjust, tfd.size, 1000);
      if (tfd.matrix != NULL)
        {
        yadjust = mac_muldiv(xadjust, tfd.matrix[1], 65536);
        xadjust = mac_muldiv(xadjust, tfd.matrix[0], 65536);
        }
      ps_endstring(absolute, tfd.spacestretch, x, y);
      absolute = FALSE;
      instring = FALSE;
      x = main_righttoleft? -xadjust : xadjust;
      y = yadjust;
      }
    }
  }

if (instring) ps_endstring(absolute, tfd.spacestretch, x, y);
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
from strings in this font. Annoyingly, OpenType fonts do not permit negative
horizontal advance values, nor both horizontal and vertical movement in the
same font, and an OpenType font is now desirable because Type 1 fonts are
becoming obsolete.

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
  absolute      TRUE for absolute coordinaates, FALSE for relative
  update        if TRUE, update the x,y positions

Returns:        nothing
*/

void
ps_string(uint32_t *s, fontinststr *fdata, int32_t *xu, int32_t *yu,
  BOOL absolute, BOOL update)
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
      ps_basic_string(p, f, fdata, absolute, x, y);
      *t = save2;
      }
    }

  /* The font is not the music font */

  else
    {
    while (*s != 0 && PFONT(*s) == f) s++;
    save1 = *s;
    *s = 0;          /* Temporary terminator */
    ps_basic_string(p, f, fdata, absolute, x, y);
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
*             Output a bar line                  *
*************************************************/

/* Normally, solid barlines and dashed ones of a single stave's depth are done
with characters from the music font, except when the barline size is greater
than the stave magnification and it's just one stave deep. However, the
bar_use_draw option forces all bar lines to be drawn.

Arguments:
  x       the x coordinate
  ytop    the top of the barline
  ybot    the bottom of the barline
  type    the type of barline
  magn    the appropriate magnification

Returns:     nothing
*/

void
ps_barline(int32_t x, int32_t ytop, int32_t ybot, int type, int32_t magn)
{
/* Use music font characters if appropriate. */

if (!bar_use_draw &&
    (type != bar_dotted || ytop == ybot) &&
    (magn <= out_stavemagn || ytop != ybot))
  {
  if (main_righttoleft)
    x += mac_muldiv(font_list[font_mf].r2ladjusts[type], 10*magn, 1000);

  ps_mfdata.size = 10 * magn;
  if (ps_needchangefont(font_mf, &ps_mfdata, FALSE))
    ps_setfont(font_mf, &ps_mfdata, FALSE);

  ytop += 16*(magn - out_stavemagn);

  if (ytop != ybot)  /* The barline is more than one character deep. */
    ps_printf(" %s(%c)%s", SFF("%f %f", 16*magn, psytran(ybot)), type,
      SFF("%f %f b", psxtran(x), psytran(ytop)));
  else  /* A single barline character is sufficient. */
    ps_printf("(%c)%s", type, SFF("%f %f s", psxtran(x), psytran(ytop)));
  }

/* Long dashed lines have to be drawn, as do other lines if they are shorter
than the character - this happens if barlinesize is greater than the stave
magnification - or if bar_use_draw is set. */

else
  {
  int32_t half_thickness = (type == bar_thick)? magn :
    (type == bar_dotted)? magn/5 : (magn*3)/20;
  int32_t yadjust = out_stavemagn/5;

  x += half_thickness;

  if (type == bar_dotted)
    ps_printf("%s", SFF(" %f %f %f %f %f [%f %f] dl", psxtran(x),
      psytran(ytop - 16*out_stavemagn - yadjust),
        psxtran(x), psytran(ybot - yadjust), 2*half_thickness, 7*half_thickness,
          7*half_thickness));

  else
    {
    ps_printf(" %s", SFF("%f %f %f %f %f l", psxtran(x),
      psytran(ytop - 16*out_stavemagn - yadjust),
        psxtran(x), psytran(ybot - yadjust), 2*half_thickness));
    if (type == bar_double)
      {
      int32_t xx = x + 2*magn;
      ps_printf(" %s", SFF("%f %f %f %f %f l", psxtran(xx),
        psytran(ytop - 16*out_stavemagn - yadjust),
          psxtran(xx), psytran(ybot - yadjust), 2*half_thickness));
      }
    }
  }
}




/*************************************************
*             Output a brace                     *
*************************************************/

/*
Arguments:
  x          the x coordinate
  ytop       the y coordinate of the top of the brace
  ybot       the y coordinate of the bottom of the brace
  magn       the magnification

Returns:     nothing
*/

void
ps_brace(int32_t x, int32_t ytop, int32_t ybot, int32_t magn)
{
ps_printf(" %s br%s", SFF("%f %f %f", ((ybot-ytop+16*magn)*23)/12000,
  psxtran(x)+1500, psytran((ytop-16*magn+ybot)/2)),
  (curmovt->bracestyle)? "2":"");
}




/*************************************************
*             Output a bracket                   *
*************************************************/

/*
Arguments:
  x          the x coordinate
  ytop       the y coordinate of the top of the bracket
  ybot       the y coordinate of the bottom of the bracket
  magn       the magnification

Returns:     nothing
*/

void
ps_bracket(int32_t x, int32_t ytop, int32_t ybot, int32_t magn)
{
ps_printf("%s", SFF(" %f %f %f k", psxtran(x), psytran(ytop)+16*magn,
  psytran(ybot)));
}



/*************************************************
*            Output a stave's lines              *
*************************************************/

/* The stavelines parameter will always be > 0. There is now an option to
draw the stave lines rather than using characters for them (the default). This
helps with screen displays that are using anti-aliasing.

It has been reported that some PostScript interpreters can't handle the
100-point wide characters, so there is an option to use only the 10-point
characters. Assume staves are always at least one character long.

Arguments:
  leftx        the x-coordinate of the stave start
  y            the y-coordinate of the stave start
  rightx       the x-coordinate of the stave end
  stavelines   the number of stave lines

Returns:       nothing
*/

void
ps_stave(int32_t leftx, int32_t y, int32_t rightx, int stavelines)
{
uschar sbuff[16];
uschar buff[256];
int ch, i;
int32_t chwidth = 0;
int32_t x = leftx;

/* Output the stave using PostScript drawing primitives. */

if (stave_use_draw > 0)
  {
  int32_t gap;
  int32_t thickness = (stave_use_draw * out_stavemagn)/10;

  if (ps_chcount > 0) ps_printf("\n");
  switch(stavelines)
    {
    case 1: y -= 4 * out_stavemagn;
    /* Fall through */
    case 2: y -= 4 * out_stavemagn;
    /* Fall through */
    case 3: gap = 8 * out_stavemagn;
    break;

    default: gap = 4 * out_stavemagn;
    break;
    }

  ps_printf("%s %d ST\n", SFF("%f %f %f %f %f", psxtran(x), psytran(y),
    rightx - leftx, thickness, gap), stavelines);
  return;
  }

/* Output the stave using music font characters */

if (stave_use_widechars)
  {
  ch = stavechar10[stavelines];
  i = 100;
  }
else
  {
  ch = stavechar1[stavelines];
  i = 10;
  }

/* Select appropriate size of music font */

ps_mfdata.size = 10 * out_stavemagn;
if (ps_needchangefont(font_mf, &ps_mfdata, FALSE))
  ps_setfont(font_mf, &ps_mfdata, FALSE);

/* Build character string of (optionally) 100-point & 10-point chars; some of
them are non-printing and have to be octal-escaped. */

Ustrcpy(buff, "(");
for (; i >= 10; i /= 10)
  {
  if (ch < 127) { sbuff[0] = ch; sbuff[1] = 0; }
    else sprintf(CS sbuff, "\\%03o", ch);
  chwidth = i * out_stavemagn;
  while (rightx - x >= chwidth) { Ustrcat(buff, sbuff); x += chwidth; }
  ch = stavechar1[stavelines];
  }

/* Now print it, forcing it onto a separate line (for human legibility). We use
INT_MAX/2 because the routine adds the length to ps_chcount to check for
overflow. */

Ustrcat(buff, ")");
if (ps_chcount > 0) ps_chcount = INT_MAX/2;

ps_printf("%s%s s", buff, SFF("%f %f", psxtran(main_righttoleft? x:leftx),
  psytran(y)));

/* If there's a fraction of 10 points left, deal with it */

if (x < rightx)
  ps_printf(" (%s)%s s", sbuff, SFF("%f %f",
    psxtran(main_righttoleft? rightx : (rightx - chwidth)), psytran(y)));

ps_printf("\n");
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

Returns:     nothing
*/

void
ps_muschar(int32_t x, int32_t y, uint32_t ch, int32_t pointsize)
{
int32_t xfudge = 0;

/* Use a local font data structure which has no rotation. */

ps_mfdata.size = pointsize;

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

  ps_basic_string(s, font_mf, &ps_mfdata, TRUE,
    x + mac_muldiv(p->x, pointsize, 10000) + xfudge,
    y - mac_muldiv(p->y, pointsize, 10000));

  xfudge += nxfudge;
  }
}



/*************************************************
*     Output an ASCII string in the music font   *
*************************************************/

/* There are two versions, one with absolute coordinates, and one with relative
coordinates, to save having to pass a flag each time (most of the calls are
with absolute coordinates). The strings are always quite short; we have to
convert to 32-bits.

Arguments:
  s          the string
  pointsize  the pointsize for the font
  x          the absolute x coordinate
  y          the absolute y coordinate

Returns:     nothing
*/

static void
musstring(uschar *s, int32_t pointsize, int32_t x, int32_t y, BOOL absolute)
{
ps_mfdata.size = pointsize;
ps_string(string_pmw(s, font_mf), &ps_mfdata, &x, &y, absolute, FALSE);
}



void
ps_musstring(uschar *s, int32_t pointsize, int32_t x, int32_t y)
{
musstring(s, pointsize, x, y, TRUE);

//ps_mfdata.size = pointsize;
//ps_basic_string(string_pmw(s, 0), font_mf, &ps_mfdata, TRUE, x, y);
}


void
ps_relmusstring(uschar *s, int32_t pointsize, int32_t x, int32_t y)
{
musstring(s, pointsize, x, y, FALSE);

//ps_mfdata.size = pointsize;
//ps_basic_string(string_pmw(s, 0), font_mf, &ps_mfdata, FALSE, x, y);
}



/*************************************************
*            Output a beam line                  *
*************************************************/

/* This function is called several times for a multi-line beam, with the level
number increasing each time. Information about the slope and other features is
in beam_* variables.

Arguments:
  x0            starting x coordinate, relative to start of bar
  x1            ending x coordinate, relative to start of bar
  level         level number
  levelchange   set nonzero for accellerando and ritardando beams

Returns:        nothing
*/

void
ps_beam(int32_t x0, int32_t x1, int level, int levelchange)
{
int sign = (beam_upflag)? (+1) : (-1);
int32_t depth = -out_stavemagn*((n_fontsize * sign *
  (int)(((double)curmovt->beamthickness) /
    cos(atan((double)beam_slope/1000.0))))/10000)/1000;
int32_t y0, y1;

y1 = y0 = out_ystave - beam_firstY +
  mac_muldiv(n_fontsize, (level - 1) * sign * 3 * out_stavemagn, 10000);

y0 -= mac_muldiv(x0-beam_firstX, beam_slope, 1000);
y1 -= mac_muldiv(x1-beam_firstX, beam_slope, 1000);

/* For accellerando and ritardando beams, adjust the ends, and make a little
bit thinner. */

if (levelchange != 0)
  {
  int32_t adjust = mac_muldiv(n_fontsize,
    abs(levelchange) * sign * 4 * out_stavemagn, 10000);
  depth = (depth*17)/20;
  if (levelchange < 0)
    {
    y0 += adjust;
    y1 += adjust/8;
    }
  else
    {
    y0 += adjust/8;
    y1 += adjust;
    }
  }

/* Get absolute x values and write the PostScript */

x0 += out_barx;
x1 += out_barx;

/* When printing right-to-left, adjust by one note's printing adjustment.
The value can't just be read from the font, as it needs fiddling, so we
just fudge a fixed value. */

if (main_righttoleft)
  {
  int32_t adjust = sign * mac_muldiv(n_fontsize/2, out_stavemagn, 1000);
  x0 -= adjust;
  x1 -= adjust;
  }

ps_printf("%s", SFF(" %f %f %f %f %f m",
  depth, psxtran(x1), psytran(y1), psxtran(x0), psytran(y0)));
}



/*************************************************
*            Output a slur                       *
*************************************************/

/* This was the original way of drawing all slurs. Additional complication in
slurs has resulted in a function called out_slur() that uses more primitive
output functions, and which could in principle be used for all slurs. However,
we retain ps_slur for complete, non-dashed, curved slurs, for compatibility and
to keep the size of the PostScript down in many common cases.

To avoid too many arguments, the rare left/right control point adjustment
parameters are placed in global variables (they are usually zero).

Arguments:
  x0         start x coordinate
  y0         start y coordinate
  x1         end x coordinate
  y1         end y coordinate
  flags      slur flags
  co         "centre out" adjustment

Returns:     nothing
*/

void
ps_slur(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t flags,
  int32_t co)
{
int32_t length = x1 - x0;

y0 = out_ystave - y0;
y1 = out_ystave - y1;

x0 += 3*out_stavemagn;
x1 += 3*out_stavemagn;

co = ((co + ((length > 20000)? 6000 : (length*6)/20)) * out_stavemagn)/1000;

if ((out_slurclx | out_slurcly | out_slurcrx | out_slurcry) != 0)
  {
  ps_printf("%s", SFF(" %f %f %f %f cA", out_slurclx, out_slurcly, out_slurcrx,
    out_slurcry));
  ps_slurA = TRUE;
  }
else if (ps_slurA)
  {
  ps_printf(" 0 0 0 0 cA");     /* default extra control movements */
  ps_slurA = FALSE;
  }

/* Keeping these as two separate calls enables the output to be split. */

ps_printf("%s", SFF(" %f %f %f %f", psxtran(x0), psytran(y0), psxtran(x1),
  psytran(y1)));
ps_printf(" %s cv%s%s", sff(((flags & sflag_b) != 0)? (-co) : co),
  ((flags & sflag_w) == 0)? "" : "w",
  ((flags & sflag_e) == 0)? "" : "e");
}



/*************************************************
*            Output a straight line              *
*************************************************/

/* The origin for y coordinates is in out_ystave, typically the bottom line of
a stave.

Arguments:
  x0          start x coordinate
  y0          start y coordinate
  x1          end x coordinate
  y1          end y coordinate
  thickness   line thickness
  flags       for various kinds of line

Returns:      nothing
*/

void
ps_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t thickness,
  uint32_t flags)
{
uschar *reset = US"";
double xx = (double)((int32_t)(x1 - x0));
double yy = (double)((int32_t)(y1 - y0));
double zz = sqrt(xx*xx + yy*yy);
int32_t len = (int32_t)zz;  /* Don't cast sqrt; it gives a compiler warning */
int32_t dashlength = 0;
int32_t gaplength = 0;
int dashcount, spacecount;

/* Handle "editorial" lines: won't exist if dashed or dotted */

if ((flags & tief_editorial) != 0)
  {
  ps_printf("%s", SFF(" GS %f %f T %f R 0 2.0 Mt 0 -2.0 Lt S GR",
   psxtran((x0+x1)/2), psytran(out_ystave - (y0+y1)/2),
   (int32_t)(atan2(yy, xx)*180000.0/3.14159)));
  }

/* Compute new dash parameters if required */

if ((flags & tief_dashed) != 0)
  {
  dashlength = 3*out_stavemagn;
  dashcount = (len/dashlength) | 1;
  spacecount = dashcount/2;
  if (dashcount != 1)
    {
    gaplength = (len - ((dashcount+1)*dashlength)/2)/spacecount;
    ps_printf("%s", SFF("[%f %f] 0 Sd", dashlength, gaplength));
    reset = US"[] 0 Sd";
    }
  }

else if ((flags & tief_dotted) != 0)
  {
  dashlength = 100;
  dashcount = (len + 4*out_stavemagn)/(4*out_stavemagn + dashlength);
  if (dashcount > 1)
    {
    gaplength = (len - dashcount * dashlength)/(dashcount - 1);
    ps_printf("%s", SFF(" 1 Slc[%f %f] 0 Sd", dashlength, gaplength));
    thickness = out_stavemagn;
    reset = US" 0 Slc[] 0 Sd";
    }
  }

/* If just set dash parameters, take note of the save flag. */

if (gaplength > 0)
  {
  if ((flags & tief_savedash) != 0)
    {
    reset = US"";
    out_dashlength = dashlength;
    out_dashgaplength = gaplength;
    }
  else out_dashlength = out_dashgaplength = 0;
  }

/* Do the line */

ps_printf(" %s l%s", SFF("%f %f %f %f %f", psxtran(x1), psytran(out_ystave - y1),
  psxtran(x0), psytran(out_ystave - y0), thickness), reset);
}



/*************************************************
*         Output a series of lines               *
*************************************************/

/* This is only used for sequences of plain lines (no dashes, etc.)

Arguments:
  x           vector of x coordinates
  y           vector of y coordinates
  count       number of vector elements
  thickness   line thickness

Returns:      nothing
*/

void
ps_lines(int32_t *x, int32_t *y, int count, int32_t thickness)
{
for (int i = count - 1; i > 0; i--)
  ps_printf("%s", SFF(" %f %f", psxtran(x[i]), psytran(out_ystave - y[i])));
ps_printf(" %d %s", count - 1, SFF("%f %f %f ll", psxtran(x[0]),
  psytran(out_ystave - y[0]), thickness));
}



/*************************************************
*         Output and stroke or fill a path       *
*************************************************/

/* The path can contain moves, lines, and curves. We generate in-line
PostScript for this one, using the saved grey level.

Arguments:
  x            vector of x coordinates
  y            vector of y coordinates
  c            vector of move/line/curve operators
  thickness    thickness of the lines for stroke; negative for fill

Returns:       nothing
*/

void
ps_path(int32_t *x, int32_t *y, int *c, int32_t thickness)
{
while (*c != path_end) switch(*c++)
  {
  case path_move:
  ps_printf("%s", SFF(" %f %f Mt", psxtran(*x++), psytran(out_ystave - *y++)));
  break;

  case path_line:
  ps_printf("%s", SFF(" %f %f Lt", psxtran(*x++), psytran(out_ystave - *y++)));
  break;

  case path_curve:
  ps_printf("%s", SFF(" %f %f %f %f %f %f Ct",
    psxtran(x[0]), psytran(out_ystave - y[0]),
      psxtran(x[1]), psytran(out_ystave - y[1]),
        psxtran(x[2]), psytran(out_ystave - y[2])));
  x += 3;
  y += 3;
  break;
  }

if (ps_gray != 0) ps_printf(" %s Sg", sff(ps_gray));
if (thickness >= 0) ps_printf(" %s Slw S", sff(thickness));
  else ps_printf(" F");
if (ps_gray != 0) ps_printf(" 0 Sg");
}



/*************************************************
*   Output and stroke or fill an absolute path   *
*************************************************/

/* This function (similar to the one above) is used for fancy slurs, when the
coordinate system has been rotated and translated so that its origin is at the
centre of the slur with the x axis joining the endpoints. The coordinates must
therefore not use psxtran/psytran.

Arguments:
  x            vector of x coordinates
  y            vector of y coordinates
  c            vector of move/line/curve operators
  thickness    thickness of the lines for stroke; negative for fill only

Returns:       nothing
*/

void
ps_abspath(int32_t *x, int32_t *y, int *c, int32_t thickness)
{
while (*c != path_end) switch(*c++)
  {
  case path_move:
  ps_printf("%s", SFF(" %f %f Mt", *x++, *y++));
  break;

  case path_line:
  ps_printf("%s", SFF(" %f %f Lt", *x++, *y++));
  break;

  case path_curve:
  ps_printf("%s", SFF(" %f %f %f %f %f %f Ct", x[0], y[0], x[1], y[1], x[2], y[2]));
  x += 3;
  y += 3;
  break;
  }

if (ps_gray != 0) ps_printf(" %s Sg", sff(ps_gray));
if (thickness >= 0) ps_printf(" %s Slw S", sff(thickness));
  else ps_printf(" F");
if (ps_gray != 0) ps_printf(" 0 Sg");
}



/*************************************************
*            Set gray level                      *
*************************************************/

/* All that happens here is that the gray level is remembered for later use.

Argument:  the gray level
Returns:   nothing
*/

void
ps_setgray(int32_t gray)
{
ps_gray = gray;
}



/*************************************************
*            Set dash and capandjoin             *
*************************************************/

/* The set values are remembered so that repetition is avoided.

Arguments:
  dashlength    the dash length
  gaplength     the gap length
  caj           the cap-and-join value

Returns:        nothing
*/

void
ps_setdash(int32_t dashlength, int32_t gaplength, uint32_t caj)
{
if (dashlength != out_dashlength || gaplength != out_dashgaplength)
  {
  if (dashlength == 0 && gaplength == 0) ps_printf("[] 0 Sd");
    else ps_printf("%s", SFF("[%f %f] 0 Sd", dashlength, gaplength));
  out_dashlength = dashlength;
  out_dashgaplength = gaplength;
  }

if (caj != ps_caj)
  {
  if ((caj & caj_round) != 0) ps_printf(" 1 Slc");
    else if ((caj & caj_square) != 0) ps_printf(" 2 Slc");
      else ps_printf(" 0 Slc");

  if ((caj & caj_round_join) != 0) ps_printf(" 1 Slj");
    else if ((caj & caj_bevel_join) != 0) ps_printf(" 2 Slj");
      else ps_printf(" 0 Slj");

  ps_caj = caj;
  }
}



/*************************************************
*            Gsave and Grestore                  *
*************************************************/

/* These functions are called from setslur.c when the coordinate system is
translated and rotated for the drawing of a fancy slur. They translate directly
into PostScript shorthand for gsave and grestore.

Arguments:  none
Returns:    nothing
*/

void
ps_gsave(void)
{
ps_printf(" GS");
}

void
ps_grestore(void)
{
ps_printf(" GR");
}



/*************************************************
*                 Rotate                         *
*************************************************/

/* This function rotates the coordinate system.

Argument:   the amount to rotate, in radians
Returns:    nothing
*/

void
ps_rotate(double r)
{
if (r != 0.0) ps_printf(" %s R", sff((int32_t)((r/(4.0 * atan(1.0)))*180000.0)));
}



/*************************************************
*                  Translate                     *
*************************************************/

/* This function translates the coordinate system.

Arguments:
  x          x coordinate of the new origin
  y          y coordinate of the new origin

Returns:     nothing
*/

void
ps_translate(int32_t x, int32_t y)
{
ps_printf("%s", SFF(" %f %f T", psxtran(x), psytran(out_ystave - y)));
}



/*************************************************
*       Start a given bar for a given stave      *
*************************************************/

/* Force a new line and output an identifying comment.

Arguments:
  barnumber    the absolute bar number
  stave        the stave
*/

void
ps_startbar(int barnumber, int stave)
{
if (ps_chcount != 0) ps_chcount = INT_MAX/2;
ps_printf("%%%s/%d\n", sfb(curmovt->barvector[barnumber]), stave);
}



/*************************************************
*     Include a file in the PostScript output    *
*************************************************/

/* This function is currently called only for the main header file. Certain
lines in the header are included only when we are generating an EPS file. They
are flagged in the header file with %EPS. Otherwise, if a line starts with %,
it is copied only if it starts with %%. Blank lines are omitted.

This function used also to be called when there were facilities for including
arbitrary PostScript files, prior to release 5. This feature has never been
re-instated, but could be if there is ever a demand. For this reason the
following logic has not been removed: If the included file is an EPS file, the
insert is wrapped in save/restore and the a4, showpage, and copypage commands
are disabled.

Arguments:
  s           the file name
  relativize  if TRUE, relativize non-absolute path to the current input file

Returns:      nothing
*/

static void
ps_include(const uschar *s, BOOL relativize)
{
FILE *f;
BOOL line1 = TRUE;
BOOL insert_eps = FALSE;

while (Ustrlen(s) + 1 > read_stringbuffer_size) string_extend_buffer();
Ustrcpy(read_stringbuffer, s);
if (relativize) string_relativize();
f = Ufopen(read_stringbuffer, "r");

if (f == NULL) error(ERR23, read_stringbuffer, strerror(errno));  /* Hard */

while (Ufgets(read_stringbuffer, read_stringbuffer_size, f) != NULL)
  {
  if (line1 && Ustrncmp(read_stringbuffer, "%!PS-Adobe", 10) == 0 &&
               Ustrstr(read_stringbuffer, "EPSF-") != NULL)
    {
    insert_eps = TRUE;
    fputs("/epspicsave save def/a4{null pop}def\n", ps_file);
    fputs("/showpage{initgraphics}def/copypage{null pop}def\n", ps_file);
    }
  else
    {
    if (ps_EPS && Ustrncmp(read_stringbuffer, "%EPS ", 5) == 0)
      Ufputs(read_stringbuffer+5, ps_file);
    else if (read_stringbuffer[0] != '\n' &&
      (read_stringbuffer[0] != '%' || read_stringbuffer[1] == '%'))
        Ufputs(read_stringbuffer, ps_file);
    }

  line1 = FALSE;
  }

if (read_stringbuffer[Ustrlen(read_stringbuffer)-1] != '\n')
  fputc('\n', ps_file);
if (insert_eps) fputs("epspicsave restore\n", ps_file);
fclose(f);
ps_chcount = 0;
}



/*************************************************
*          Include a font in the output          *
*************************************************/

/*
Argument:
  name     the name of the font
  ext      a file extension or empty string

Returns:   nothing
*/

static void
include_font(uschar *name, const char *ext)
{
FILE *f = NULL;
uschar *s;
uschar *fextra, *fdefault;
uschar buff[256];

/* If this is one of the PMW fonts, seek it in the psfonts directories,
otherwise look in the general fonts directories. */

if (Ustrncmp(name, "PMW-", 4) == 0)
  {
  fextra = font_music_extra;
  fdefault = font_music_default;
  }
else
  {
  fextra = font_data_extra;
  fdefault = font_data_default;
  }

/* font_finddata(..., TRUE) gives a hard error if the file cannot be found. */

f = font_finddata(name, ext, fextra, fdefault, buff, TRUE);

/* Copy from "%%BeginResource:" or the start of the file to "%%EndResource" or
the end of the file. */

while ((s = Ufgets(buff, sizeof(buff), f)) != NULL)
  if (Ustrncmp(buff, "%%BeginResource:", 16) == 0) break;

if (s == NULL)
  {
  fprintf(ps_file, "\n%%%%BeginResource: font %s\n", name);
  rewind(f);
  }
else fprintf(ps_file, "%s", CS buff);

while ((s = Ufgets(buff, sizeof(buff), f)) != NULL)
  {
  fprintf(ps_file, "%s", CS buff);
  if (Ustrncmp(buff, "%%EndResource", 13) == 0) break;
  }

if (s == NULL) fprintf(ps_file, "\n%%%%EndResource\n\n");
fclose(f);
}


/*************************************************
*           Produce PostScript output            *
*************************************************/

/* This is the controlling function for generating PostScript output. If the
print_imposition has the special value pc_EPS, we are producing EPS PostScript,
and a number of page-related parameters are then ignored.

Arguments: none
Returns:   nothing
*/

void
ps_go(void)
{
time_t timer;
int32_t w = 0, d = 0;
int count = 0;
int fcount = 1;

int fonts_to_include[font_tablen];
int fonts_to_include_count = 0;

int32_t scaled_main_sheetwidth =
  mac_muldiv(main_sheetwidth, print_magnification, 1000);

/* Initialize the current page number and page list data */

ps_EPS = (print_imposition == pc_EPS);
setup_pagelist(ps_EPS? FALSE : print_reverse);

/* Set the top of page y coordinate; the PostScript is relative to the usual
bottom of page origin. Before the invention of the imposition parameter, we
computed this from the pagelength, but with some minima imposed. For
compatibility, keep this unchanged for cases when imposition is defaulted. For
EPS, we use the sheetsize, whatever it may be. */

if (ps_EPS) ps_ymax = main_truepagelength + 50000; else
  {
  if (main_landscape)
    {
    if (main_truepagelength < 492000)
      ps_ymax = mac_muldiv(526000, 1000, print_magnification);
        else ps_ymax = main_truepagelength + 34000;
    }
  else
    {
    if (main_truepagelength < 720000)
      ps_ymax = mac_muldiv(770000, 1000, print_magnification);
        else ps_ymax = main_truepagelength + 50000;
    }

  /* Take the opportunity of setting true paper sizes for imposing */

  switch(print_imposition)
    {
    case pc_a5ona4:
    w = 595000;
    d = 842000;
    ps_ymax = main_truepagelength + 50000;
    break;

    case pc_a4ona3:
    w = 842000;
    d = 1190000;
    ps_ymax = main_truepagelength + 50000;
    break;
    }
  }

/* Adjust paper size to the magnification */

print_sheetwidth = mac_muldiv(main_sheetwidth, 1000, main_magnification);
ps_ymax = mac_muldiv(ps_ymax, 1000, main_magnification);

/* Initializing stuff at the start of the PostScript file. We are attempting to
keep to the 3.0 structuring conventions. Initial comments ("header") come
first. */

if (!main_testing)
  {
  time (&timer);
  fprintf(ps_file, "%%!PS-Adobe-3.0%s\n", ps_EPS? " EPSF-3.0" : "");
  fprintf(ps_file, "%%%%Creator: Philip's Music Writer (PMW) %s\n", PMW_VERSION);
  fprintf(ps_file, "%%%%CreationDate: %s", ctime(&timer));
  }

if (ps_EPS) fprintf(ps_file, "%%%%BoundingBox: (atend)\n");
  else fprintf(ps_file, "%%%%Pages: (atend)\n");
fprintf(ps_file, "%%%%DocumentNeededResources: font ");

/* Scan the fonts, set the ps id and process each unique one, remembering those
that are to be included in the output. */

for (int i = 0; i < font_tablen; i++)
  {
  fontstr *fs;
  int fontid, j;

  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  ps_IdStrings[i] = font_IdStrings[j];

  if (j != i) continue;   /* Seen this one already */

  if (++fcount > 3)
    {
    fprintf(ps_file, "\n%%%%+ font ");
    fcount = 1;
    }

  fontid = font_table[i];
  fs = font_list + fontid;
  fprintf(ps_file, "%s ", fs->name);

  /* Remember which fonts are to be included. If -incPMWfont was set, do
  this automatically for music fonts. */

  if ((fs->flags & ff_include) != 0 || (print_incPMWfont &&
       (Ustrcmp(fs->name, "PMW-Music") == 0 ||
        Ustrcmp(fs->name, "PMW-Alpha") == 0)))
    fonts_to_include[fonts_to_include_count++] = fontid;
  }

fprintf(ps_file, "\n");

/* List the included fonts */

if (fonts_to_include_count > 0)
  {
  fcount = 1;
  fprintf(ps_file, "%%%%DocumentSuppliedResources: font");
  for (int i = 0; i < fonts_to_include_count; i++)
    {
    if (++fcount > 3)
      {
      fprintf(ps_file, "\n%%%%+ font");
      fcount = 1;
      }
    fprintf(ps_file, " %s", (font_list[fonts_to_include[i]]).name);
    }
  fprintf(ps_file, "\n");
  }

if (!ps_EPS) fprintf(ps_file,
  "%%%%Requirements: numcopies(%d)\n", print_copies);
fprintf(ps_file, "%%%%EndComments\n\n");

/* Deal with a known paper size */

switch (main_sheetsize)
  {
  case sheet_A3:
  fprintf(ps_file, "%%%%BeginPaperSize: a3\na3\n%%%%EndPaperSize\n\n");
  break;

  case sheet_A4:
  fprintf(ps_file, "%%%%BeginPaperSize: a4\na4\n%%%%EndPaperSize\n\n");
  break;

  case sheet_A5:
  fprintf(ps_file, "%%%%BeginPaperSize: a5\na5\n%%%%EndPaperSize\n\n");
  break;

  case sheet_B5:
  fprintf(ps_file, "%%%%BeginPaperSize: b5\nb5\n%%%%EndPaperSize\n\n");
  break;

  case sheet_letter:
  fprintf(ps_file, "%%%%BeginPaperSize: letter\nletter\n%%%%EndPaperSize\n\n");
  break;

  default:
  break;
  }

/* Next, the file's prologue, except in testing mode (to save space in the test
output files). The name of the header file is NOT relative to the main input
file. If it is not absolute, it is taken relative to the current directory. */

if (!main_testing)
  {
  fprintf(ps_file, "%%%%BeginProlog\n");
  ps_include(ps_header, FALSE);
  fprintf(ps_file, "%%%%EndProlog\n\n");
  }

/* The setup section sets up the printing device. We include the font finding
in here, as it seems the right place. Include any relevant fonts in the output
file. */

fprintf(ps_file, "%%%%BeginSetup\n");

for (int i = 0; i < fonts_to_include_count; i++)
  {
  uschar *name = font_list[fonts_to_include[i]].name;
  include_font(name, (Ustrcmp(name, "PMW-Alpha") == 0)? "" : ".pfa");
  }

/* Now set up the fonts */

for (int i = 0; i < font_tablen; i++)
  {
  int j;
  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  if (j == i)
    {
    fontstr *f = font_list + font_table[i];
    uschar *s = f->name;
    fprintf(ps_file, "%%%%IncludeResource: font %s\n", s);
    fprintf(ps_file, "/%s /%sX /%s inf\n", font_IdStrings[i],
      font_IdStrings[i], s);
    }
  }

/* Unless EPS, we used to select A4 paper, but only once (to allow concatenated
files). However, this seems to give trouble with Ghostview for doing magnify
windows, and it doesn't seem to affect modern PostScript printers anyway. So it
is no longer done.

Select the number of copies if not 1, set manual feed if the flag is set, deal
with duplex and tumble options, and end the setup section. */

if (!ps_EPS)
  {
  /*********
  fprintf(ps_file,
    "currentdict /a4_done known not {a4 /a4_done true def} if\n");
  **********/

  if (print_copies != 1) fprintf(ps_file, "/#copies %d def\n", print_copies);
  if (print_manualfeed || print_duplex)
    {
    fprintf(ps_file, "statusdict begin");
    if (print_manualfeed) fprintf(ps_file, " /manualfeed true def");
    if (print_duplex)
      {
      fprintf(ps_file, " true setduplexmode");
      if (print_tumble) fprintf(ps_file, " true settumble");
      }
    fprintf(ps_file, " end\n");
    }
  }

fprintf(ps_file, "%%%%EndSetup\n\n");

/* Now the requested pages. The get_pages() function returns one or two
pages. When printing 2-up either one of them may be null. Start with curmovt
set to NULL so that a "change of movement" happens at the start. */

curmovt = NULL;

for (;;)
  {
  pagestr *ps_1stpage, *ps_2ndpage;
  int32_t scaled = 1000;
  BOOL recto = FALSE;

  if (!get_pages(&ps_1stpage, &ps_2ndpage)) break;

  if (ps_1stpage != NULL && ps_2ndpage != NULL)
    fprintf(ps_file, "%%%%Page: %d&%d %d\n", ps_1stpage->number,
      ps_2ndpage->number, ++count);
  else if (ps_1stpage != NULL)
    {
    fprintf(ps_file, "%%%%Page: %d %d\n", ps_1stpage->number, ++count);
    recto = (ps_1stpage->number & 1) != 0;
    }
  else
    {
    fprintf(ps_file, "%%%%Page: %d %d\n", ps_2ndpage->number, ++count);
    recto = (ps_2ndpage->number & 1) != 0;
    }

  fprintf(ps_file, "%%%%BeginPageSetup\n/pagesave save def\n");

  ps_chcount = 0;    /* No characters in the current line. */
  ps_curfont = -1;   /* No font currently set. */
  ps_caj = 0;        /* Cap-and-join setting */

  /* Not much to do for EPS */

  if (ps_EPS)
    {
    if (main_righttoleft)
      ps_printf("%s 0 T -1 1 scale\n", sff(main_sheetwidth));
    if (main_magnification != 1000)
      ps_printf("%s dup scale\n", sff(main_magnification));
    }

  /* Not EPS. Swap pages for righttoleft. Then move the origin to the desired
  position. The values 1 (upright, 1-up, portrait), 2 (sideways, 2-up,
  portrait), and 4 (sideways, 1-up, landscape) use bottom left, i.e. no
  translation, but we have to generate an adjustment for type 2 if sheetwidth
  isn't half the paper size. The gutter facility is available only when
  printing 1-up. */

  else
    {
    if (main_righttoleft)
      {
      pagestr *temp = ps_1stpage;
      ps_1stpage = ps_2ndpage;
      ps_2ndpage = temp;
      }

    switch (print_pageorigin)
      {
      case 0: /* A4 Sideways, 1-up, portrait */
      ps_printf("0 595 T -90 R\n");
      if (print_gutter != 0)
        ps_printf("%s 0 T\n", sff(recto? print_gutter : -print_gutter));
      break;

      case 1: /* Upright, 1-up, portrait */
      if (print_gutter != 0)
        ps_printf("%s 0 T\n", sff(recto? print_gutter : -print_gutter));
      break;

      case 2: /* Sideways, 2-up, portrait */
      if (d/2 != scaled_main_sheetwidth)
        ps_printf("%s 0 T\n",
          sff((d/2 - scaled_main_sheetwidth)/(print_pamphlet? 1:2)));
      break;

      case 3: /* Upright, 2-up, portrait */
      ps_printf("0 %s T -90 R\n",
        sff(d - (d/2 - scaled_main_sheetwidth)/(print_pamphlet? 1:2)));
      break;

      case 4: /* A4 Sideways, 1-up, landscape */
      if (print_gutter != 0)
        ps_printf("%s 0 T\n", sff(recto? print_gutter : -print_gutter));
      break;

      case 5: /* Upright, 1-up, landscape; page size defined by sheetsize */
              /* Sheetwidth is original sheet height */
      ps_printf("0 %s T -90 R\n", sff(scaled_main_sheetwidth));
      break;

      case 6: /* A4 Sideways, 2-up, landscape */
      ps_printf("%s %s T -90 R\n", sff(d/2), sff(w));
      break;

      case 7: /* Upright, 2-up, landscape */
      ps_printf("0 %s T\n", sff(d/2));
      break;
      }

    if (print_image_xadjust != 0 || print_image_yadjust != 0)
      ps_printf("%s", SFF("%f %f T\n", print_image_xadjust, print_image_yadjust));

    if (main_righttoleft)
      ps_printf("%s 0 T -1 1 scale\n", sff(scaled_main_sheetwidth));

    if (main_magnification != 1000 || print_magnification != 1000)
      {
      scaled = mac_muldiv(main_magnification, print_magnification, 1000);
      ps_printf("%s dup scale\n", sff(scaled));
      }
    }

  /* End of setup */

  fprintf(ps_file, "%%%%EndPageSetup\n");

  /* When printing 2-up, we may get one or both pages; when not printing 2-up,
  we may get either page given, but not both. */

  if (ps_1stpage != NULL)
    {
    curpage = ps_1stpage;
    out_page();
    }

  if (ps_2ndpage != NULL)
    {
    if (ps_chcount > 0)
      {
      fprintf(ps_file, "\n");
      ps_chcount = 0;
      }
    if (print_imposition == pc_a5ona4 || print_imposition == pc_a4ona3)
      {
      int sign = main_righttoleft? -1 : +1;
      int32_t dd = mac_muldiv(d, 500, scaled);
      if (main_landscape) ps_printf("0 %s T\n", sff(-dd)); else
        ps_printf("%s 0 T\n", sff(sign * (print_pamphlet?
          mac_muldiv(main_sheetwidth, 1000, main_magnification) : dd)));
      }
    curpage = ps_2ndpage;
    out_page();
    }

  /* EPS files are permitted to contain showpage, and this is actually useful
  because it means an EPS file can be printed or displayed. So we don't cut out
  showpage. */

  fprintf(ps_file, "\npagesave restore showpage\n\n");
  }

/* Do PostScript trailer */

fprintf(ps_file, "%%%%Trailer\n");

if (ps_EPS)
  {
  if (main_righttoleft)
    ps_printf("%s", SFF("%%%%BoundingBox: %f %f %f %f\n",
      main_sheetwidth -
        mac_muldiv(psxtran(out_bbox[2]), main_magnification, 1000),
      mac_muldiv(psytran(out_bbox[1]), main_magnification, 1000),
      main_sheetwidth -
        mac_muldiv(psxtran(out_bbox[0]), main_magnification, 1000),
      mac_muldiv(psytran(out_bbox[3]), main_magnification, 1000)));
  else
    ps_printf("%s", SFF("%%%%BoundingBox: %f %f %f %f\n",
      mac_muldiv(psxtran(out_bbox[0]), main_magnification, 1000),
      mac_muldiv(psytran(out_bbox[1]), main_magnification, 1000),
      mac_muldiv(psxtran(out_bbox[2]), main_magnification, 1000),
      mac_muldiv(psytran(out_bbox[3]), main_magnification, 1000)));
  }
else fprintf(ps_file, "%%%%Pages: %d\n", count);
}

/* End of ps.c */
