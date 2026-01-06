/*************************************************
*             PMW PostScript functions           *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: May 2021 */
/* This file last modified: January 2026 */

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

static fontinststr ps_curfontdata = { NULL, 0, 0 };
static int32_t ps_fmatrix[6];

static uschar  *ps_IdStrings[font_tablen+1];


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
  fputc('\n', out_file);
  ps_chcount = 0;
  }

while (ps_chcount == 0 && *p == ' ') { p++; len--; }
fputs(p, out_file);
ps_chcount = (p[len-1] == '\n')? 0 : ps_chcount + len;
}



/*************************************************
*            Manage colour setting               *
*************************************************/

static void
setcolour(void)
{
if (pout_wantcolour[0] == pout_wantcolour[1] &&
    pout_wantcolour[1] == pout_wantcolour[2])
  ps_printf(" %s Sg", sff(pout_wantcolour[0]));
else
  ps_printf(" %s %s %s Sc", sff(pout_wantcolour[0]), sff(pout_wantcolour[1]),
    sff(pout_wantcolour[2]));
memcpy(pout_curcolour, pout_wantcolour, 3 * sizeof(int32_t));
pout_changecolour = FALSE;
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
  absolute      TRUE if absolute, FALSE if relative
  w             extra space width
  x             x-coordinate
  y             y-coordinate
*/

static void
ps_endstring(BOOL absolute, int32_t w, int32_t x, int32_t y)
{
fputc(')', out_file);   /* Does not check ps_chcount */
ps_chcount++;

if (absolute)
  {
  if (w != 0) ps_printf("%s", SFF("%f %f %f ws", w, poutx(x), pouty(y)));
    else ps_printf("%s", SFF("%f %f s", poutx(x), pouty(y)));
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
  x            the x coordinate
  y            the y coordinate
  startadjust  TRUE in RTL mode if start needs adjusting

Returns:       nothing
*/

static void
ps_basic_string(uint32_t *s, usint f, fontinststr *fdata, int32_t x, int32_t y,
  BOOL startadjust)
{
fontstr *fs = &(font_list[font_table[f & ~font_small]]);
kerntablestr *ktable = fs->kerns;
fontinststr tfd = *fdata;
BOOL instring = FALSE;
BOOL absolute = TRUE;
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

if (startadjust || ps_EPS)
  {
  int32_t last_width, last_r2ladjust;
  int32_t swidth = pout_getswidth(s, f, fs, &last_width, &last_r2ladjust);

  /* For right-to-left, adjust the printing position for the string by the
  length of the string, adjusted for the actual bounding box of the final
  character, and scaled to the font size. For EPS output, adjust the bounding
  box. Both may, of course, happen. */

  if (startadjust)
    x += mac_muldiv(swidth - last_width + last_r2ladjust, tfd.size, 1000);

  if (ps_EPS)
    {
    swidth = mac_muldiv(swidth, tfd.size, 1000);
    if (x + swidth > out_bbox[2]) out_bbox[2] = x + swidth;
    }
  }

/* Generate the output. Values are always less than FONTWIDTHS_SIZE (512);
those above 255 use the second font encoding. */

for (p = s; *p != 0; p++)
  {
  uint32_t c = PCHAR(*p);    /* c is the original code point */
  uint32_t pc = c;           /* pc is the code value to print */
  BOOL extended = FALSE;

  if (c >= 256)
    {
    pc -= 256;
    extended = TRUE;
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
      fputc('\n', out_file);
      ps_chcount = 0;
      }
    fputc('(', out_file);
    ps_chcount++;
    instring = TRUE;
    }

  if (pc == '(' || pc == ')' || pc == '\\')
    ps_chcount += Cfprintf(out_file, "\\%c", pc);
  else if (pc >= 32 && pc <= 126)
    {
    fputc(pc, out_file);
    ps_chcount++;
    }
  else ps_chcount += Cfprintf(out_file, "\\%03o", pc);

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
      absolute = instring = FALSE;
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
downwards. Change colour if required, then call the common PS/PDF output
function with the PS basic string output function.

Arguments:
  s             the PMW string
  fdata         points to font instance data
  xu            pointer to the x coordinate
  yu            pointer to the y coordinate
  update        if TRUE, update the x,y positions

Returns:        nothing
*/

void
ps_string(uint32_t *s, fontinststr *fdata, int32_t *xu, int32_t *yu,
  BOOL update)
{
if (pout_changecolour) setcolour();
pout_string(s, fdata, xu, yu, update, ps_basic_string);
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
if (pout_changecolour) setcolour();

/* Use music font characters if appropriate. */

if (!bar_use_draw &&
    (type != bar_dotted || ytop == ybot) &&
    (magn <= out_stavemagn || ytop != ybot))
  {
  if (main_righttoleft)
    x += mac_muldiv(font_list[font_mf].r2ladjusts[type], 10*magn, 1000);

  pout_mfdata.size = 10 * magn;
  if (ps_needchangefont(font_mf, &pout_mfdata, FALSE))
    ps_setfont(font_mf, &pout_mfdata, FALSE);

  ytop += 16*(magn - out_stavemagn);

  if (ytop != ybot)  /* The barline is more than one character deep. */
    ps_printf(" %s(%c)%s", SFF("%f %f", 16*magn, pouty(ybot)), type,
      SFF("%f %f b", poutx(x), pouty(ytop)));
  else  /* A single barline character is sufficient. */
    ps_printf("(%c)%s", type, SFF("%f %f s", poutx(x), pouty(ytop)));
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
    ps_printf("%s", SFF(" %f %f %f %f %f [%f %f] dl", poutx(x),
      pouty(ytop - 16*out_stavemagn - yadjust),
        poutx(x), pouty(ybot - yadjust), 2*half_thickness, 7*half_thickness,
          7*half_thickness));

  else
    {
    ps_printf(" %s", SFF("%f %f %f %f %f l", poutx(x),
      pouty(ytop - 16*out_stavemagn - yadjust),
        poutx(x), pouty(ybot - yadjust), 2*half_thickness));
    if (type == bar_double)
      {
      int32_t xx = x + 2*magn;
      ps_printf(" %s", SFF("%f %f %f %f %f l", poutx(xx),
        pouty(ytop - 16*out_stavemagn - yadjust),
          poutx(xx), pouty(ybot - yadjust), 2*half_thickness));
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
if (pout_changecolour) setcolour();
ps_printf(" %s br%s", SFF("%f %f %f", ((ybot-ytop+16*magn)*23)/12000,
  poutx(x)+1500, pouty((ytop-16*magn+ybot)/2)),
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
if (pout_changecolour) setcolour();
ps_printf("%s", SFF(" %f %f %f k", poutx(x), pouty(ytop)+16*magn,
  pouty(ybot)));
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

pout_setgray(0);
if (pout_changecolour) setcolour();

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

  ps_printf("%s %d ST\n", SFF("%f %f %f %f %f", poutx(x), pouty(y),
    rightx - leftx, thickness, gap), stavelines);
  return;
  }

/* Output the stave using music font characters */

if (stave_use_widechars)
  {
  ch = pout_stavechar10[stavelines];
  i = 100;
  }
else
  {
  ch = pout_stavechar1[stavelines];
  i = 10;
  }

/* Select appropriate size of music font */

pout_mfdata.size = 10 * out_stavemagn;
if (ps_needchangefont(font_mf, &pout_mfdata, FALSE))
  ps_setfont(font_mf, &pout_mfdata, FALSE);

/* Build character string of (optionally) 100-point & 10-point chars; some of
them are non-printing and have to be octal-escaped. */

Ustrcpy(buff, "(");
for (; i >= 10; i /= 10)
  {
  if (ch < 127) { sbuff[0] = ch; sbuff[1] = 0; }
    else sprintf(CS sbuff, "\\%03o", ch);
  chwidth = i * out_stavemagn;
  while (rightx - x >= chwidth) { Ustrcat(buff, sbuff); x += chwidth; }
  ch = pout_stavechar1[stavelines];
  }

/* Now print it, forcing it onto a separate line (for human legibility). We use
INT_MAX/2 because the routine adds the length to ps_chcount to check for
overflow. */

Ustrcat(buff, ")");
if (ps_chcount > 0) ps_chcount = INT_MAX/2;

ps_printf("%s%s s", buff, SFF("%f %f", poutx(main_righttoleft? x:leftx),
  pouty(y)));

/* If there's a fraction of 10 points left, deal with it */

if (x < rightx)
  ps_printf(" (%s)%s s", sbuff, SFF("%f %f",
    poutx(main_righttoleft? rightx : (rightx - chwidth)), pouty(y)));

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
if (pout_changecolour) setcolour();
pout_muschar(x, y, ch, pointsize, ps_basic_string);
}



/*************************************************
*     Output an ASCII string in the music font   *
*************************************************/

/* The strings are always quite short; we have to convert to 32-bits by calling
string_pmw().

Arguments:
  s          the string
  pointsize  the pointsize for the font
  x          the x coordinate
  y          the y coordinate

Returns:     nothing
*/

void
ps_musstring(uschar *s, int32_t pointsize, int32_t x, int32_t y)
{
pout_mfdata.size = pointsize;
ps_string(string_pmw(s, font_mf), &pout_mfdata, &x, &y, FALSE);
}



/*************************************************
*            Output a beam line                  *
*************************************************/

/* This function is called several times for a multi-line beam, with the level
number increasing each time. Information about the slope and other features is
in beam_* variables. Preparatory computation is now moved into a pout function
that is common with PDF.

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
int32_t y0, y1, depth;
if (pout_changecolour) setcolour();
pout_beam(&x0, &x1, &y0, &y1, &depth, level, levelchange);
ps_printf("%s", SFF(" %f %f %f %f %f m",
  depth, poutx(x1), pouty(y1), poutx(x0), pouty(y0)));
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

if (pout_changecolour) setcolour();

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

ps_printf("%s", SFF(" %f %f %f %f", poutx(x0), pouty(y0), poutx(x1),
  pouty(y1)));
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

if (pout_changecolour) setcolour();

/* Handle "editorial" lines: won't exist if dashed or dotted */

if ((flags & tief_editorial) != 0)
  {
  ps_printf("%s", SFF(" GS %f %f T %f R 0.4 Slw 0 2.0 Mt 0 -2.0 Lt S GR",
   poutx((x0+x1)/2), pouty(out_ystave - (y0+y1)/2),
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

ps_printf(" %s l%s", SFF("%f %f %f %f %f", poutx(x1), pouty(out_ystave - y1),
  poutx(x0), pouty(out_ystave - y0), thickness), reset);
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
if (pout_changecolour) setcolour();
for (int i = count - 1; i > 0; i--)
  ps_printf("%s", SFF(" %f %f", poutx(x[i]), pouty(out_ystave - y[i])));
ps_printf(" %d %s", count - 1, SFF("%f %f %f ll", poutx(x[0]),
  pouty(out_ystave - y[0]), thickness));
}



/*************************************************
*         Stroke or fill a path                  *
*************************************************/

/* Used by ps_path() and ps_abspath() below.

Argument: line thickness for stroke; -1 for fill
Returns:  nothing
*/

static void
strokeorfill(int32_t thickness)
{
if (pout_changecolour) setcolour();
if (thickness >= 0) ps_printf(" %s Slw S", sff(thickness));
  else ps_printf(" F");
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
  ps_printf("%s", SFF(" %f %f Mt", poutx(*x++), pouty(out_ystave - *y++)));
  break;

  case path_line:
  ps_printf("%s", SFF(" %f %f Lt", poutx(*x++), pouty(out_ystave - *y++)));
  break;

  case path_curve:
  ps_printf("%s", SFF(" %f %f %f %f %f %f Ct",
    poutx(x[0]), pouty(out_ystave - y[0]),
      poutx(x[1]), pouty(out_ystave - y[1]),
        poutx(x[2]), pouty(out_ystave - y[2])));
  x += 3;
  y += 3;
  break;
  }

strokeorfill(thickness);
}



/*************************************************
*   Output and stroke or fill an absolute path   *
*************************************************/

/* This function (similar to the one above) is used for fancy slurs, when the
coordinate system has been rotated and translated so that its origin is at the
centre of the slur with the x axis joining the endpoints. The coordinates must
therefore not use poutx/pouty.

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

strokeorfill(thickness);
}



/*************************************************
*                   Set dash                     *
*************************************************/

/* The set values are remembered so that repetition is avoided.

Arguments:
  dashlength    the dash length
  gaplength     the gap length

Returns:        nothing
*/

void
ps_setdash(int32_t dashlength, int32_t gaplength)
{
if (dashlength != out_dashlength || gaplength != out_dashgaplength)
  {
  if (dashlength == 0 && gaplength == 0) ps_printf("[] 0 Sd");
    else ps_printf("%s", SFF("[%f %f] 0 Sd", dashlength, gaplength));
  out_dashlength = dashlength;
  out_dashgaplength = gaplength;
  }
}



/*************************************************
*                Set cap and join                *
*************************************************/

/* The set value is remembered so that repetition is avoided.

Argument: the cap and join flag bits
Returns:  nothing
*/

void
ps_setcapandjoin(uint32_t caj)
{
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
ps_printf("%s", SFF(" %f %f T", poutx(x), pouty(out_ystave - y)));
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
    fputs("/epspicsave save def/a4{null pop}def\n", out_file);
    fputs("/showpage{initgraphics}def/copypage{null pop}def\n", out_file);
    }
  else
    {
    if (ps_EPS && Ustrncmp(read_stringbuffer, "%EPS ", 5) == 0)
      Ufputs(read_stringbuffer+5, out_file);
    else if (read_stringbuffer[0] != '\n' &&
      (read_stringbuffer[0] != '%' || read_stringbuffer[1] == '%'))
        Ufputs(read_stringbuffer, out_file);
    }

  line1 = FALSE;
  }

if (read_stringbuffer[Ustrlen(read_stringbuffer)-1] != '\n')
  fputc('\n', out_file);
if (insert_eps) fputs("epspicsave restore\n", out_file);
if (fclose(f) != 0) error(ERR200, "included PostScript file", strerror(errno));
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
  fprintf(out_file, "\n%%%%BeginResource: font %s\n", name);
  rewind(f);
  }
else fprintf(out_file, "%s", CS buff);

while ((s = Ufgets(buff, sizeof(buff), f)) != NULL)
  {
  fprintf(out_file, "%s", CS buff);
  if (Ustrncmp(buff, "%%EndResource", 13) == 0) break;
  }

if (s == NULL) fprintf(out_file, "\n%%%%EndResource\n\n");
if (fclose(f) != 0) error(ERR200, "included font file", strerror(errno));
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
int32_t w = 0, d = 0;
int count = 0;
int fcount = 1;

int fonts_to_include[font_tablen];
int fonts_to_include_count = 0;

int32_t scaled_main_sheetwidth =
  mac_muldiv(main_sheetwidth, print_magnification, 1000);

/* Initialize the indirect function pointers for PostScript output. */

ofi_abspath = ps_abspath;
ofi_barline = ps_barline;
ofi_beam = ps_beam;
ofi_brace = ps_brace;
ofi_bracket = ps_bracket;
ofi_getcolour = pout_getcolour;
ofi_grestore = ps_grestore;
ofi_gsave = ps_gsave;
ofi_line = ps_line;
ofi_lines = ps_lines;
ofi_muschar = ps_muschar;
ofi_musstring = ps_musstring;
ofi_path = ps_path;
ofi_rotate = ps_rotate;
ofi_setcapandjoin = ps_setcapandjoin;
ofi_setcolour = pout_setcolour;
ofi_setdash = ps_setdash;
ofi_setgray = pout_setgray;
ofi_slur = ps_slur;
ofi_startbar = ps_startbar;
ofi_stave = ps_stave;
ofi_string = ps_string;
ofi_translate = ps_translate;

/* Initialize the current page number and page list data */

ps_EPS = (print_imposition == pc_EPS);
pout_setup_pagelist(ps_EPS? FALSE : print_reverse);

/* Set the top of page y coordinate and width and depth for translation; the
PostScript is relative to the usual bottom of page origin. */

if (ps_EPS) pout_ymax = main_truepagelength + 50000;
  else pout_set_ymax_etc(&w, &d);

/* Adjust paper size to the magnification */

print_sheetwidth = mac_muldiv(main_sheetwidth, 1000, main_magnification);
pout_ymax = mac_muldiv(pout_ymax, 1000, main_magnification);

/* Initializing stuff at the start of the PostScript file. We are attempting to
keep to the 3.0 structuring conventions. Initial comments ("header") come
first. */

if (main_testing == 0)  /* Any non-zero value discards this */
  {
  time_t timer;
  time (&timer);
  fprintf(out_file, "%%!PS-Adobe-3.0%s\n", ps_EPS? " EPSF-3.0" : "");
  fprintf(out_file, "%%%%Creator: Philip's Music Writer (PMW) %s\n", PMW_VERSION);
  fprintf(out_file, "%%%%CreationDate: %s", ctime(&timer));
  }

if (ps_EPS) fprintf(out_file, "%%%%BoundingBox: (atend)\n");
  else fprintf(out_file, "%%%%Pages: (atend)\n");
fprintf(out_file, "%%%%DocumentNeededResources: font ");

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
    fprintf(out_file, "\n%%%%+ font ");
    fcount = 1;
    }

  fontid = font_table[i];
  fs = font_list + fontid;
  fprintf(out_file, "%s ", fs->name);

  /* Remember which fonts are to be included. If -incPMWfont was set, do
  this automatically for music fonts. */

  if ((fs->flags & ff_include) != 0 || (print_incPMWfont &&
       (Ustrcmp(fs->name, "PMW-Music") == 0 ||
        Ustrcmp(fs->name, "PMW-Alpha") == 0)))
    fonts_to_include[fonts_to_include_count++] = fontid;
  }

fprintf(out_file, "\n");

/* List the included fonts */

if (fonts_to_include_count > 0)
  {
  fcount = 1;
  fprintf(out_file, "%%%%DocumentSuppliedResources: font");
  for (int i = 0; i < fonts_to_include_count; i++)
    {
    if (++fcount > 3)
      {
      fprintf(out_file, "\n%%%%+ font");
      fcount = 1;
      }
    fprintf(out_file, " %s", (font_list[fonts_to_include[i]]).name);
    }
  fprintf(out_file, "\n");
  }

if (!ps_EPS) fprintf(out_file,
  "%%%%Requirements: numcopies(%d)\n", print_copies);
fprintf(out_file, "%%%%EndComments\n\n");

/* Deal with a known paper size */

switch (main_sheetsize)
  {
  case sheet_A3:
  fprintf(out_file, "%%%%BeginPaperSize: a3\na3\n%%%%EndPaperSize\n\n");
  break;

  case sheet_A4:
  fprintf(out_file, "%%%%BeginPaperSize: a4\na4\n%%%%EndPaperSize\n\n");
  break;

  case sheet_A5:
  fprintf(out_file, "%%%%BeginPaperSize: a5\na5\n%%%%EndPaperSize\n\n");
  break;

  case sheet_B5:
  fprintf(out_file, "%%%%BeginPaperSize: b5\nb5\n%%%%EndPaperSize\n\n");
  break;

  case sheet_letter:
  fprintf(out_file, "%%%%BeginPaperSize: letter\nletter\n%%%%EndPaperSize\n\n");
  break;

  default:
  break;
  }

/* Next, the file's prologue, which contains any bespoke font encodings and the
standard PostScript header. */

fprintf(out_file, "%%%%BeginProlog\n");

for (int i = 0; i < font_tablen; i++)
  {
  int j;
  for (j = 0; j < i; j++) if (font_table[i] == font_table[j]) break;
  if (j == i)
    {
    fontstr *f = font_list + font_table[i];
    if (f->encoding != NULL)
      {
      for (int k = 0; k < FONTWIDTHS_SIZE; k += 256)
        {
        uschar name[128];
        sprintf(CS name, "%sEnc%c", f->name, (k == 0)? 'L':'U');

        if (k == 0) fprintf(out_file, "%%%%Custom encodings for %s\n", f->name);
        fprintf(out_file, "/%s 256 array def\n", name);

        /* Scan the relevant half of the encoding */

        for (int cc = k; cc < k + 256; )
          {
          int ce, cn;
          int notdefcount = 0;

          /* Look for next group of more than 16 .notdefs */

          for (cn = cc; cn < k + 256; cn++)
            {
            if (f->encoding[cn] == NULL)
              {
              int ct;
              for (ct = cn + 1; ct < k + 256; ct++)
                if (f->encoding[ct] != NULL) break;
              if (ct - cn >= 16)
                {
                notdefcount = ct - cn;
                break;
                }
              cn = ct;
              }
            }

          /* Set the end of a non-long-notdef sequence */

          ce = (cn >= k + 256)? k + 256 : cn;

          /* Output a sequence */

          if (ce - cc == 1)
            {
            fprintf(out_file, "%s %d /%s put\n", name, cc - k,
              (f->encoding[cc] == NULL)? US".notdef" : f->encoding[cc]);
            }

          else if (ce - cc > 0)
            {
            int n = 0;
            fprintf(out_file, "%s %d [\n", name, cc - k);

            for (; cc < ce; cc++)
              {
              if (n++ > 7)
                {
                fprintf(out_file, "\n");
                n = 1;
                }
              fprintf(out_file, "/%s", (f->encoding[cc] == NULL)?
                US".notdef" : f->encoding[cc]);
              }

            fprintf(out_file, "] putinterval\n");
            }

          /* Set start of next sequence */

          cc = ce;

          /* If we stopped at a sequence of .notdefs, output them */

          if (notdefcount > 0)
            {
            fprintf(out_file, "%d 1 %d {%s exch /.notdef put} for\n",
              cc - k, cc - k + notdefcount - 1, name);
            cc += notdefcount;
            }
          }   /* End handling one sequence */
        }     /* 2 x loop for two encodings */
      }       /* End handling encoding */
    }         /* End unique font handling */
  }           /* End font scan */

/* In any testing mode we omit the standard header (to save space in the test
output files). The name of the header file is NOT relative to the main input
file. If it is not absolute, it is taken relative to the current directory. */

if (main_testing == 0) ps_include(ps_header, FALSE);
  else fprintf(out_file, "%%%%Standard Header Omitted (testing)\n");
fprintf(out_file, "%%%%EndProlog\n\n");

/* The setup section sets up the printing device. We include the font finding
in here, as it seems the right place. Include any relevant fonts in the output
file. */

fprintf(out_file, "%%%%BeginSetup\n");

for (int i = 0; i < fonts_to_include_count; i++)
  {
  uschar *name = font_list[fonts_to_include[i]].name;
  include_font(name, ".pfa");
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
    fprintf(out_file, "%%%%IncludeResource: font %s\n", s);
    fprintf(out_file, "/%s /%sX /%s inf\n", font_IdStrings[i],
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
  fprintf(out_file,
    "currentdict /a4_done known not {a4 /a4_done true def} if\n");
  **********/

  if (print_copies != 1) fprintf(out_file, "/#copies %d def\n", print_copies);
  if (print_manualfeed || print_duplex)
    {
    fprintf(out_file, "statusdict begin");
    if (print_manualfeed) fprintf(out_file, " /manualfeed true def");
    if (print_duplex)
      {
      fprintf(out_file, " true setduplexmode");
      if (print_tumble) fprintf(out_file, " true settumble");
      }
    fprintf(out_file, " end\n");
    }
  }

fprintf(out_file, "%%%%EndSetup\n\n");

/* Now the requested pages. The pout_get_pages() function returns one or two
pages. When printing 2-up either one of them may be null. Start with curmovt
set to NULL so that a "change of movement" happens at the start. */

curmovt = NULL;

for (;;)
  {
  pagestr *ps_1stpage, *ps_2ndpage;
  int32_t scaled = 1000;
  BOOL recto = FALSE;

  if (!pout_get_pages(&ps_1stpage, &ps_2ndpage)) break;

  if (ps_1stpage != NULL && ps_2ndpage != NULL)
    fprintf(out_file, "%%%%Page: %d&%d %d\n", ps_1stpage->number,
      ps_2ndpage->number, ++count);
  else if (ps_1stpage != NULL)
    {
    fprintf(out_file, "%%%%Page: %d %d\n", ps_1stpage->number, ++count);
    recto = (ps_1stpage->number & 1) != 0;
    }
  else
    {
    fprintf(out_file, "%%%%Page: %d %d\n", ps_2ndpage->number, ++count);
    recto = (ps_2ndpage->number & 1) != 0;
    }

  fprintf(out_file, "%%%%BeginPageSetup\n/pagesave save def\n");

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

  fprintf(out_file, "%%%%EndPageSetup\n");

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
      fprintf(out_file, "\n");
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

  fprintf(out_file, "\npagesave restore showpage\n\n");
  }

/* Do PostScript trailer */

fprintf(out_file, "%%%%Trailer\n");

if (ps_EPS)
  {
  if (main_righttoleft)
    ps_printf("%s", SFF("%%%%BoundingBox: %f %f %f %f\n",
      main_sheetwidth -
        mac_muldiv(poutx(out_bbox[2]), main_magnification, 1000),
      mac_muldiv(pouty(out_bbox[1]), main_magnification, 1000),
      main_sheetwidth -
        mac_muldiv(poutx(out_bbox[0]), main_magnification, 1000),
      mac_muldiv(pouty(out_bbox[3]), main_magnification, 1000)));
  else
    ps_printf("%s", SFF("%%%%BoundingBox: %f %f %f %f\n",
      mac_muldiv(poutx(out_bbox[0]), main_magnification, 1000),
      mac_muldiv(pouty(out_bbox[1]), main_magnification, 1000),
      mac_muldiv(poutx(out_bbox[2]), main_magnification, 1000),
      mac_muldiv(pouty(out_bbox[3]), main_magnification, 1000)));
  }
else fprintf(out_file, "%%%%Pages: %d\n", count);
}

/* End of ps.c */
