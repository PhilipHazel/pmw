/*************************************************
*            PMW code for setting slurs          *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: July 2021 */
/* This file last modified: October 2023 */

#include "pmw.h"



/*************************************************
*         Set up start of slur processing        *
*************************************************/

/* This is called when processing [slur], both while paginating and also while
actually outputting. Active slurs are held on a chain; when the slur is
complete, the block is added to a free chain so that it can be re-used.

Argument:   the slur start item
Returns:    the slur processing structure that was set up
*/

slurstr *
slur_startslur(b_slurstr *p)
{
slurstr *s = mem_get_cached((void **)(&main_freeslurblocks), sizeof(slurstr));

s->slur = p;
s->maxy = INT32_MIN;
s->miny = INT32_MAX;
s->gaps = NULL;
s->section = 1;
s->lastx = s->x = s->count = s->slopeleft = s->sloperight = 0;

/* Put in a default y value in case the slur doesn't cross anything visible. */

s->lasty = s->y = ((p->flags & sflag_b) == 0)? 16000 : 0;

/* If this was a crossing slur, we place it second on the stack, if possible.
Otherwise put it at the top. */

if ((p->flags & sflag_x) != 0 && bar_cont->slurs != NULL)
  {
  s->next = (bar_cont->slurs)->next;
  (bar_cont->slurs)->next = s;
  }
else
  {
  s->next = bar_cont->slurs;
  bar_cont->slurs = s;
  }
return s;
}



/*************************************************
*        Find and unchain given slur             *
*************************************************/

/* This is called when processing [endslur] or [endline], both while setting up
the cont structure, and while actually outputting. A slur will always be in
progress; an isolated [endslur] is detected at reading time, causing an error
that stops further processing. If [endslur] has an id, we seek that slur,
yielding NULL if not found; otherwise the first on the chain is yielded.

Argument:   the slur end item
Returns:    the active slur block or NULL if not found
*/

slurstr *
slur_endslur(b_endslurstr *p)
{
slurstr *s = bar_cont->slurs;
int slurid = p->value;

if (slurid == 0) bar_cont->slurs = s->next; else
  {
  slurstr **ss = &(bar_cont->slurs);
  while (s != NULL)
    {
    if ((s->slur)->id == slurid)
      {
      *ss = s->next;
      break;
      }
    ss = &(s->next);
    s = *ss;
    }
  if (s == NULL) return NULL;
  }

/* We can put the slur block back onto its free chain, because its contents
will be used straight away, before any subsequent [slur] is processed. */

mem_free_cached((void **)&main_freeslurblocks, s);
return s;
}



/*************************************************
*         Find Bezier parameter fraction         *
*************************************************/

/* Given the coefficients of a Bezier curve, its x coordinate end points, and a
fraction, compute the value of t (0 <= t <= 1) for which the x coordinate on
the curve is the same as the x coordinate of the point that is the fraction of
the x-distance between the end points. This is used when drawing partial
curves.

The code in this function had to be massaged carefully to ensure that exactly
the same result is obtained when run native and under valgrind, in order to
make the tests run clean. The differences were only in the 3rd decimal place,
insignificant in the actual output, but of course the comparisons failed.

Arguments:
  f          the fraction
  a, b, c    the Bezier coefficients
  x0, x1     the endpoint x coordinates

Returns:     the t value
*/

static double
bezfraction(double f, double a, double b, double c, double x0, double x1)
{
double wanted = x0 + (x1 - x0)*f;   /* The wanted x coordinate */
double start = 0.0;
double stop = 1.0;
double step = 0.1;

/* Outer repeat loop */

for (;;)
  {
  double t;

  /* Inner loop covers the current range */

  for (t = start; t < stop + step; t += step)
    {
    double x = ((a*t + b)*t + c)*t + x0;

    /* Make sure that -0.0 is actually 0.0; this can be different when running
    valgrind. */

    if (fabs(x) < 0.000001) x = 0.0;

    /* If stepped past the wanted point, set new bounds and a smaller step
    unless we are close enough. */

    if (x >= wanted)
      {
      if (fabs(x - wanted) < 10.0 || step < 0.001) return t;
      start = t - step;
      stop = t;
      step /= 10.0;
      break;
      }
    }

  /* If didn't reach it, return the right point */

  if (t >= stop + step) return stop;
  }

return f;    /* Should never be obeyed */
}


/*************************************************
*          Get coordinates of slur gap           *
*************************************************/

/* For a drawing function we need the coordinates of the end points and the
midpoint of a gap in the normal coordinate system. This is very tedious to
compute. The results are returned in a static vector. We have to repeat a lot
of the work needed for actually drawing the relevant portion of the slur.

Arguments:
  ix0, iy0   the start coordinates of the part-slur
  ix1, iy1   the end coordinates of the part slur
  flags      slur flags
  co         co parameter
  start      t-value for start of gap (fixed point)
  stop       t-value for end of gap (fixed point)

Returns:     vector of left, middle, right coordinates
*/

static int32_t coords[6];

static int *
getgapcoords(int32_t ix0, int32_t iy0, int32_t ix1, int32_t iy1, uint32_t flags,
  int32_t co, int32_t start, int32_t stop)
{
int above, wiggly;
int32_t ox, oy;
double xx, yy, w, v;
double x0, x1, x2, x3, y0, y1, y2, y3;
double a, b, c, f, g;
double xxl, xxr, yyl, yyr, sa, ca;

/* Compute values needed for curved slur drawing. */

above  = ((flags & sflag_b) == 0)? (+1) : (-1);
wiggly = ((flags & sflag_w) == 0)? (+1) : (-1);

xx = (double)ix1 - (double)ix0;
yy = (double)iy1 - (double)iy0;
w = sqrt(xx*xx + yy*yy);

sa = yy/w;    /* sine */
ca = xx/w;    /* cosine */

w /= 2.0;
v = w*0.6667;

if (v > 10000.0) v = 10000.0;
co = (above * (co + ((xx > 20000)? 6000 : (int)(xx * 0.3))) * out_stavemagn)/1000;

f = ((double)start)/1000.0;
g = ((double)stop)/1000.0;

/* Calculate the origin of the coordinate system where the slur would be drawn.
We don't actually have to translate or rotate, since we are not actually going
to draw anything here. */

ox = (ix0+ix1+6*out_stavemagn)/2;
oy = (iy0+iy1)/2;

/* Set up traditional Bezier coordinates for the complete slur. */

x0 = -w;
x1 = v - w + (double)out_slurclx;
x2 = w - v + (double)out_slurcrx;
x3 = +w;

y0 = 0.05;
y1 = (double)(int)(co + out_slurcly);
y2 = (double)(int)(co*wiggly + out_slurcry);
y3 = 0.05;

/* Calculate the coefficients for the original x parametric equation. */

a = x3 - x0 + 3.0*(x1 - x2);
b = 3.0*(x2 - 2.0*x1 + x0);
c = 3.0*(x1 - x0);

/* The given fractions are fractions along the line joining the two end points.
These do not correspond linearly with the parameter t of the complete curve, so
we have to calculate new fractional values. */

if (f > 0.0 && f < 1.0) f = bezfraction(f, a, b, c, x0, x3);
if (g > 0.0 && g < 1.0) g = bezfraction(g, a, b, c, x0, x3);

/* Now calculate the new Bezier point coordinates for ends of the portion of
the slur that we want. */

xxl = x0 + ((a*f + b)*f + c)*f;
xxr = x0 + ((a*g + b)*g + c)*g;

/* Now do exactly the same for the y points */

a = y3 - y0 + 3.0*(y1 - y2);
b = 3.0*(y2 - 2.0*y1 + y0);
c = 3.0*(y1 - y0);

yyl = y0 + ((a*f + b)*f + c)*f;
yyr = y0 + ((a*g + b)*g + c)*g;

/* Now we have to get those coordinates back into the normal coordinate system.
First rotate, then remove the effect of the translation. */

coords[0] = (int)(xxl * ca - yyl * sa) + ox;
coords[1] = (int)(yyl * ca + xxl * sa) + oy;
coords[4] = (int)(xxr * ca - yyr * sa) + ox;
coords[5] = (int)(yyr * ca + xxr * sa) + oy;

/* Set up the mid point values and return the vector. */

coords[2] = (coords[0] + coords[4])/2;
coords[3] = (coords[1] + coords[5])/2;

return coords;
}



/*************************************************
*      Output text in a slur or line gap         *
*************************************************/

/*
Arguments:
  gt           the gap text structure
  x, y         coordinates of the middle of the gap
  num, den     slope parameters for the gap

Returns:       nothing
*/

static void
slur_gaptext(b_slurgapstr *sg, int32_t x, int32_t y, int32_t num, int32_t den)
{
int32_t adjusthy, width, sn, cs;
fontinststr localfdata, *fdata;
fontinststr *origfdata = curmovt->fontsizes->fontsize_text + sg->textsize;

if (num != 0)    /* Not horizontal */
  {
  double hy = sqrt((double)num*(double)num + (double)den*(double)den);
  fdata = font_rotate(origfdata,
    (int)((45000.0*atan2((double)num, (double)den))/atan(1.0)));
  sn = (int)((1000.0*(double)num)/hy);
  cs = (int)((1000.0*(double)den)/hy);
  }
else
  {
  localfdata = *origfdata;
  fdata = &localfdata;
  sn = 0;
  cs = 1000;
  den = 1;   /* To stop division by 0 below */
  }

fdata->size = mac_muldiv(fdata->size, out_stavemagn, 1000);
adjusthy = sg->texty - fdata->size/4;
width = string_width(sg->gaptext, fdata, NULL);

x = x - width/2 +
  mac_muldiv(sg->textx, cs, 1000) - mac_muldiv(adjusthy, sn, 1000);

y = y - (width*num)/(2*den) +
  mac_muldiv(sg->textx, sn, 1000) + mac_muldiv(adjusthy, cs, 1000);

out_string(sg->gaptext, fdata, x, out_ystave - y, sg->textflags);
}



/*************************************************
*   Compute parameters & draw a slur or line     *
*************************************************/

/* This function is called when [endslur] or [endline] is reached or if the end
of a line is reached in the middle of a slur. The active slur structure s has
already been placed at the head of the chain of free slur blocks, knowing that
its data will be used before any subsequent [slur] is processed, so we do not
need to dispose of it here.

Arguments:
  s          the active slur structure
  x1         the x coordinate of the end of the slur
  npitch     pitch of the final note or 0 if drawing to end of line
  eol        TRUE if this slur goes to the end of the line

Returns:     nothing
*/

void
slur_drawslur(slurstr *s, int x1, int npitch, BOOL eol)
{
b_slurstr *ss = s->slur;
b_slurmodstr *sm = NULL;
b_slurmodstr *smm = ss->mods;

uint32_t slurflags = ss->flags;

BOOL absolute = (slurflags & sflag_abs) != 0;
BOOL below = (slurflags & sflag_b) != 0;
BOOL lay = (slurflags & sflag_lay) != 0;
BOOL laststemup = out_laststemup[curstave];
BOOL lineslur = (slurflags & sflag_l) != 0;

int32_t adjustco = 0;
int32_t use_endmoff = ((slurflags & sflag_cx) != 0)? out_moff : out_lastmoff;
int32_t line_rc_adjust = lineslur? 3000 : 0;
int32_t x0 = s->x;
int32_t y0 = s->y;
int32_t y1;

BOOL sol = x0 == 0;

/* For continued slurs in style 0 we have to adjust the apparent end points and
then draw only part of the resulting slur. These are the start and end
fractional values. */

int32_t dstart = 0;
int32_t dstop = 1000;

/* Note: for absolute and {und,ov}erlay positioning, all this computation is
later over-ridden. We leave it in place so as not to disturb things for the x
values. */

/* End of line slurs take their vertical end from the max/min under them. Allow
for the fact that a line slur gets its end moved 3 pts "past" the note
position. Also, suppress wiggles for the first section - the other parts do the
suppression with the sol test and need to have the flag still set in order to
flip the above/below bit. Note that the final note has not yet been
incorporated into the max/min. */

if (eol)
  {
  if (below) y1 = (s->lasty < s->miny)? s->lasty : s->miny;
    else y1 = (s->lasty > s->maxy)? s->lasty : s->maxy;
  if (lineslur) x1 -= 3000;
  if (x0 != 0) slurflags &= ~sflag_w;   /* No wiggle */
  slurflags |= sflag_or;                /* Open on right if line */
  }

/* Not end of line slur. Set the vertical position according to the last note.
If the last note was beamed, and the slur is on the same side as the beam, or
if the slur is on the opposite side to the stem, we need to put in an
additional bit of space for clearance. */

else
  {
  y1 = (npitch == 0)? (below? L_2L : L_5L) :
    misc_ybound(below, n_prevtie, TRUE, TRUE);

  if (below)
    {
    if (laststemup || out_lastnotebeamed) y1 -= 1000;
    }
  else
    {
    if (!laststemup || out_lastnotebeamed) y1 += 1000;
    }
  }

/* Set up left position at line start; if x1 is also zero it means we have hit
an [es] or [el] immediately after a bar line at a line break. This is not an
error; we just invent a small distance. Turn off the wiggle flag, but if it was
set, flip the above/below status. We must also do a vertical adjustment for the
final part of a split wiggly slur. */

if (x0 == 0)
  {
  slurflags |= sflag_ol;              /* Open on left if line */
  if ((slurflags & sflag_w) != 0)
    {
    slurflags &= ~sflag_w;            /* No wiggle */
    below = !below;                   /* Other direction */
    slurflags ^= sflag_b;             /* Must do this too */
    y1 = (npitch == 0)? L_2L : misc_ybound(below, n_prevtie, TRUE, TRUE);
    }

  x0 = out_sysblock->firstnoteposition + out_sysblock->xjustify - 10500;

  if (x1 == 0)
    {
    x1 = x0 + 10500;
    if (lineslur) x0 -= 10000;   /* lines normally start 10 pts right */
    }
  }

/* For wiggly slurs, move the final point to the other end of the last note. We
don't attempt any other fancy adjustments for these slurs. */

if ((slurflags & sflag_w) != 0)
  {
  y1 = (npitch == 0)? L_2L : misc_ybound(!below, n_prevtie, TRUE, TRUE);
  if (!below && !laststemup && (n_flags & nf_stem) != 0)
    x1 -= 5*out_stavemagn;
  }

/* For non-wiggly slurs, make adjustments according to the starting and ending
slopes if there are more than three notes in the slur. The "starting" slope
actually looks at more than the first note, but we aren't clever enough to do
the same at the end. The curvature is adjusted according to the max/min pitch
under the slur. */

else if (below)    /* slur below */
  {
  int32_t miny = s->miny;

  if (!laststemup &&               /* Note has down stem */
      (n_flags & nf_stem) != 0 &&  /* Note has a stem */
      !lineslur &&                 /* Not a line (i.e. a slur) */
      !eol &&                      /* Not an end of line slur */
      !main_righttoleft)           /* Not right-to-left */
    x1 -= 5*out_stavemagn;

  if (s->count > 3)
    {
    if (s->slopeleft < -400)
      y0 -= (s->slopeleft < -600)? 4000: 2000;
    if (s->sloperight > 400)
      y1 -= (s->sloperight > 600)? 4000: 2000;
    }

  if (miny < y0 && miny < y1)
    {
    int32_t adjust = (y0 + y1)/2 - miny;
    if (lineslur)
      {
      if ((slurflags & sflag_h) != 0) y0 = y1 = miny;
        else { y0 -= adjust; y1 -= adjust; }
      }
    else adjustco += adjust;
    }
  }

else    /* slur above */
  {
  int32_t maxy = s->maxy;
  if (s->count > 3)
    {
    if (s->slopeleft > 400)
      y0 += (s->slopeleft > 600)? 4000 : 2000;
    if (s->sloperight < -400)
      y1 += (s->sloperight < -600)? 4000 : 2000;
    }

  if (maxy > y0 && maxy > y1)
    {
    int adjust = maxy - (y0 + y1)/2;
    if (lineslur)
      {
      if ((slurflags & sflag_h) != 0) y0 = y1 = maxy;
        else { y0 += adjust; y1 += adjust; }
      }
    else adjustco += adjust;
    }
  }

/* Deal with the horizontal option (horizontal line slurs handled above, but
other cases not yet). */

if ((slurflags & sflag_h) != 0)
  {
  if ((!below && y1 > y0) || (below && y1 < y0)) y0 = y1; else y1 = y0;
  }

/* If this is a curved slur, arrange that the end points are not on staff
lines, and ensure that for longish slurs, the centre adjustment is at least 2.
Also do this for steep slurs, but not if absolute or at underlay level or if
the slur is short. */

if (!lineslur)
  {
  int32_t ay0 = abs(y0);
  int32_t ay1 = abs(y1);

  if (below)
    {
    if (y0 >= L_1S && (ay0 % 4000) < 500) y0 -= 1000;
    if (y1 >= L_1S && (ay1 % 4000) < 500) y1 -= 1000;
    }
  else
    {
    if (y0 <= L_5S && (ay0 % 4000) < 500) y0 += 1000;
    if (y1 <= L_5S && (ay1 % 4000) < 500) y1 += 1000;
    }

  if (x1 - x0 > 72000 && adjustco < 2000) adjustco = 2000;

  else if (!absolute && !lay && x1 - x0 > 24000)
    {
    int32_t overall_slope = mac_muldiv(y1 - y0, 1000, x1 - x0);
    if (abs(overall_slope) > 500 && adjustco < 2000) adjustco = 2000;
    }
  }

/* If this is a line-type "slur", ensure that the default pitches (i_e. before
adding user movement) are above or below the staff, as necessary. */

else
  {
  if (below)
    {
    if (y0 > L_1L) y0 = L_1L;
    if (y1 > L_1L) y1 = L_1L;
    }
  else
    {
    if (y0 < L_6L) y0 = L_6L;
    if (y1 < L_6L) y1 = L_6L;
    }
  }

/* If the slur or line is marked as "absolute", then the vertical positions are
specified without reference to any intervening notes. We allow all the above to
happen, so that the correct x values get computed, and also this feature was
added later and it is easier not to disturb the above code. Ditto for
slurs/lines that are drawn at the underlay or overlay level. */

if (absolute)
  {
  y0 = y1 = below? 0 : 16000;
  }
else if (lay)
  {
  y0 = y1 = below?
    out_sysblock->ulevel[curstave] : out_sysblock->olevel[curstave];
  }

/* Finally, apply manual adjustments. All endpoints of all sections are
affected by the "ally" value. */

y0 += ss->ally;
y1 += ss->ally;

/* Most slurs appear on at most two lines; we need the slurmod structure for
sequence number zero for endpoint adjustments in all cases except for the
middle sections of a slur that exists on more than 2 lines, so get it in all
cases (if it exists). */

while (smm != NULL && smm->sequence != 0) smm = smm->next;

/* If this is part of a split slur, we need the slurmod structure that matches
this section, and in all cases its values are used. */

if (sol || eol)
  {
  sm = ss->mods;
  while (sm != NULL && sm->sequence != s->section) sm = sm->next;
  if (sm != NULL)
    {
    if (sm->lxoffset != 0)
      {
      int32_t offset = mac_muldiv(len_crotchet, sm->lxoffset, 1000);
      x0 = out_barx + out_findAoffset(s->moff + offset);
      }

    /* When x1 default for a line is set by musical offset, lose the additional
    3pts that are added to get past the final note in the default case. */

    if (sm->rxoffset != 0)
      {
      int32_t offset = mac_muldiv(len_crotchet, sm->rxoffset, 1000);
      x1 = out_barx + out_findAoffset(use_endmoff + offset) - line_rc_adjust;
      }
    x0 += sm->lx;
    x1 += sm->rx;
    y0 += sm->ly;
    y1 += sm->ry;
    adjustco += sm->c;
    out_slurclx = (sm->clx * out_stavemagn)/1000;
    out_slurcly = (sm->cly * out_stavemagn)/1000;
    out_slurcrx = (sm->crx * out_stavemagn)/1000;
    out_slurcry = (sm->cry * out_stavemagn)/1000;
    }
  }

/* Other values depend on which bit of a split slur is being drawn. If it is
neither the starting section nor the ending section (that is, it's a whole line
of middle slur), use just the values from this section's data block. In other
words, there's no more to do. */

if (sol && eol)
  {
  }

/* The final portion of a split slur. Use values from its block, plus values
from the zero block for the right-hand end, which is also applied to the
vertical movement of the left-hand end. */

else if (sol)
  {
  if (smm != NULL)
    {
    if (smm->rxoffset != 0)
      {
      int32_t offset = mac_muldiv(len_crotchet, smm->rxoffset, 1000);
      x1 = out_barx + out_findAoffset(use_endmoff + offset) - line_rc_adjust;
      }
    x1 += smm->rx;
    y1 += smm->ry;
    y0 += smm->ry;
    }
  }

/* The first section of a split slur. Use values from its block plus values
from the zero block for the left-hand end, which is also applied to the
vertical movement of the right-hand end. */

else if (eol)
  {
  if (smm != NULL)
    {
    if (smm->lxoffset != 0)   /* Relative to start of slur */
      {
      int32_t offset = mac_muldiv(len_crotchet, smm->lxoffset, 1000);
      x0 = out_barx + out_findAoffset(s->moff + offset);
      }
    x0 += smm->lx;
    y0 += smm->ly;
    y1 += smm->ly;
    }
  }

/* An unsplit slur. Use values from the zero block. */

else if (smm != NULL)
  {
  if (smm->lxoffset != 0)  /* Relative to start of slur */
    {
    int32_t offset = mac_muldiv(len_crotchet, smm->lxoffset, 1000);
    x0 = out_barx + out_findAoffset(s->moff + offset);
    }
  if (smm->rxoffset != 0)
    {
    int32_t offset = mac_muldiv(len_crotchet, smm->rxoffset, 1000);
    x1 = out_barx + out_findAoffset(use_endmoff + offset) - line_rc_adjust;
    }
  x0 += smm->lx;
  x1 += smm->rx;
  y0 += smm->ly;
  y1 += smm->ry;
  adjustco += smm->c;
  out_slurclx = (smm->clx * out_stavemagn)/1000;
  out_slurcly = (smm->cly * out_stavemagn)/1000;
  out_slurcrx = (smm->crx * out_stavemagn)/1000;
  out_slurcry = (smm->cry * out_stavemagn)/1000;
  }

/* Need to correct for the jog length for absolute line slurs, so that the line
is at the height specified. Also need to move above slurs up. */

if (lineslur && (absolute || lay))
  {
  int32_t x = adjustco + 3000;
  if (!below) x = -x;
  y0 += x;
  y1 += x;
  }

/* Adjust the verticals for the stave magnification */

y0 = mac_muldiv(y0, out_stavemagn, 1000);
y1 = mac_muldiv(y1, out_stavemagn, 1000);

/* Make adjustments and fudges for continued curved slurs if the style is not
the default. We do this by drawing only half a slur. Such slurs may be
continued at either end. */

if (!lineslur && curmovt->endlineslurstyle != 0 && (sol || eol))
  {
  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;

  /* If this is both as starting and ending part (hopefully a rare occurrence),
  make the slur a bit longer and draw from 0.1 to 0.9 of its length. */

  if (sol && eol)
    {
    x0 -= dx/8;
    x1 += dx/8;
    dstart = 100;
    dstop = 900;
    }

  /* If this is a starting part or an ending part, adjust the values so that
  we draw exactly half a slur, which will have the desired look. */

  else
    {
    adjustco += 4*dy/5;

    if (sol)
      {
      x0 -= dx;
      y0 -= dy/5;
      dstart = 500;
      }
    else
      {
      x1 += dx;
      y1 += dy/5;
      dstop = 500;
      }
    }
  }

/* There may be a chain of gaps in a line or slur. Note that the out_slur()
function adds 7*out_stavemagn to the x1 value, to get it past the final note,
so we have to compensate for that here. */

if (s->gaps != NULL)
  {
  BOOL done = FALSE;
  int32_t fudge = 7*out_stavemagn;
  int32_t xlength = x1 + fudge - x0;
  int32_t start = dstart;

  /* First scan through and compute positions for any that are specified as a
  fraction of the way along. */

  for (gapstr *pg = s->gaps; pg != NULL; pg = pg->next)
    {
    int hfraction = pg->gap->hfraction;
    if (hfraction >= 0)
      pg->x = pg->gap->xadjust + x0 + mac_muldiv(xlength, hfraction, 1000);
    }

  /* The gaps may be in any order, horizontally. We could sort them before
  processing, but it is just as easy to pick out the leftmost and process it
  until there are none left. */

  while (s->gaps != NULL)
    {
    b_slurgapstr *sg;
    gapstr **gg = &(s->gaps);      /* Parent ptr */
    gapstr *g = *gg;               /* Active block ptr */
    int32_t firstx = g->x;

    /* Scan to find the leftmost */

    for (gapstr *pg = g; pg->next != NULL; pg = pg->next)
      {
      if ((pg->next)->x < firstx)
        {
        gg = &(pg->next);
        g = *gg;
        firstx = g->x;
        }
      }
    sg = g->gap;

    /* Process a gap in a line. The values of the starting x/y coordinates are
    adjusted for each part of the line - the start/stop parameters of
    out_slur() are not used when drawing lines. */

    if (lineslur)
      {
      int32_t xg0, yg0, xwidth;
      int32_t num = y1 - y0;
      int32_t den = x1 + fudge - x0;;

      /* Compute end coordinates for the part of the line up to this gap. */

      if (num == 0) /* Optimize the common case (horizontal) */
        {
        xwidth = sg->width;
        xg0 = g->x - xwidth/2;
        yg0 = y0;
        }
      else
        {
        double dnum = (double)num;
        double dden = (double)den;
        xwidth = (int)(((double)(sg->width) * dden)/sqrt(dnum*dnum + dden*dden));
        xg0 = g->x - xwidth/2;
        yg0 = y0 + mac_muldiv(xg0 - x0, num, den);
        }

      /* Draw the line to this gap, unless it has negative length. */

      if (x0 < xg0)
        out_slur(x0, y0, xg0 - fudge, yg0, slurflags|sflag_or, adjustco, 0, 0);

      /* Update the starting position for the next section. */

      x0 = xg0 + xwidth;
      y0 = yg0 + mac_muldiv(x0 - xg0, num, den);
      slurflags |= sflag_ol;
      if (x0 >= x1) done = TRUE;  /* No final bit at the end */

      /* If there is an associated draw function, set up the coordinates and
      call it. Note that lines are always drawn 3 points above or below the
      given y value, to leave space for the jog. */

      if (sg->drawing != NULL)
        {
        draw_lgx = xwidth / 2;
        draw_lgy = (y0 - yg0)/2;
        draw_ox = xg0 + draw_lgx;
        draw_oy = yg0 + draw_lgy + (below? (-3000) : (3000));
        draw_gap = below? -1000 : +1000;
        out_dodraw(sg->drawing, sg->drawargs, FALSE);
        draw_lgx = draw_lgy = draw_gap = 0;
        }

      /* If there is an associated text string, arrange to print it centred
      in the gap. */

      if (sg->gaptext != NULL)
        {
        slur_gaptext(sg, (xg0 + x0)/2,
          (y0 + yg0)/2 + (below? (-3000) : (3000)), num, den);
        }
      }

    /* Process a gap in a curved slur. In this case, the call to out_slur()
    always has the original x/y coordinates, but uses the start/stop values to
    control which part is to be drawn. */

    else
      {
      int32_t stop = mac_muldiv(g->x - sg->width/2 - x0, 1000, xlength);

      /* Draw the current piece if it has a positive length */

      if (stop > start)
        out_slur(x0, y0, x1, y1, slurflags, adjustco, start, stop);

      /* Compute start of next piece */

      start =  mac_muldiv(g->x + sg->width/2 - x0, 1000, xlength);
      if (start >= 1000) done = TRUE;  /* No final bit at the end */

      /* If there is an associated draw function, set up the coordinates and
      call it. */

      if (sg->drawing != NULL)
        {
        int *c = getgapcoords(x0, y0, x1, y1, slurflags, adjustco, stop, start);
        draw_ox = c[2];
        draw_oy = c[3];
        draw_lgx = c[2] - c[0];
        draw_lgy = c[3] - c[1];
        draw_gap = below? -1000 : +1000;
        out_dodraw(sg->drawing, sg->drawargs, FALSE);
        draw_lgx = draw_lgy = draw_gap = 0;
        }

      /* If there's associated text, output it. */

      if (sg->gaptext != NULL)
        {
        int32_t *c = getgapcoords(x0, y0, x1, y1, slurflags, adjustco, stop, start);
        slur_gaptext(sg, c[2], c[3], c[5] - c[1], c[4] - c[0]);
        }
      }

    /* Extract the block from the chain and put it on its free chain. */

    *gg = g->next;
    mem_free_cached((void **)&main_freegapblocks, g);
    }   /* End loop through gap blocks */

  /* Draw the rest of the line or slur, if there is any left. */

  if (!done) out_slur(x0, y0, x1, y1, slurflags, adjustco, start, dstop);
  }

/* The slur or line has no gaps specified. Output it, provided it has some
positive horizontal length. If there isn't enough length, generate an error and
invent some suitable length. */

else
  {
  if (x0 >= x1)
    {
    error(ERR144);
    if (x0 == x1) x1 = x0 + 10000; else
      { int32_t temp = x0; x0 = x1; x1 = temp; }
    }
  out_slur(x0, y0, x1, y1, slurflags, adjustco, dstart, dstop);
  }

/* Reset the globals that hold control point adjustments. */

out_slurclx = out_slurcly = out_slurcrx = out_slurcry = 0;
}



/*************************************************
*      Draw slur or line from given values       *
*************************************************/

/* Originally there were only simple slurs, and these were drawn by the
ps_slur() function. When things got more complicated, additional work would
have had to be done in the PostScript header file. However, in the meanwhile,
the ps_path() function had been invented for drawing arbitrary shapes at the
logical (non-device) level. This function (out_slur()) is now called where
ps_slur() used to be called. In principle, it could do all the output. However,
to keep the size of PostScript down and for compatibility with the previous
PostScript, it still calls ps_slur() for PostScript output of complete,
non-dashed, curved slurs that can be handled by the old code.

New functionality is added in here, and in time I may remove the special
PostScript into here as well. Each change will cause the PostScript to change,
and hence the tests to fail to validate...

Note that as well as the parameters passed as arguments, there are also
parameter values in the global variables out_slurclx, out_slurcly, out_slurcrx,
and out_slurcry for corrections to the control points.

The t-values are the Bezier parameter values for drawing part slurs, given as
fixed point values between 0 and 1.0 respectively. For a whole slur, their
int32 values are therefore 0 and 1000.

Parts of the code in this function had to be massaged carefully to ensure that
exactly the same result is obtained when run native and under valgrind, in
order to make the tests run clean. The differences were only in the 3rd decimal
place, insignificant in the actual output, but of course the comparisons
failed.

Arguments:
  ix0, iy1     coordinates of the start of the slur
  ix1, iy1     coordinates of the end of the slur
  flag         the slur flags
  co           the co ("centre out") value
  start        the t-value at slur start (fixed point)  ) for drawing partial
  stop         the t-value at slur end (fixed point)    )   curved slurs

Returns:       nothing
*/

void
out_slur(int32_t ix0, int32_t iy0, int32_t ix1, int32_t iy1, uint32_t flags,
  int32_t co, int32_t start, int32_t stop)
{
int above  = ((flags & sflag_b) == 0)? (+1) : (-1);

if (ix1 == ix0 && iy1 == iy0) return;   /* Avoid crash */

/* Use ps_slur() to output complete, curved, non-dashed slurs to maintain
compatibility and smaller PostScript files. */

if (start == 0 && stop == 1000 && (flags & (sflag_l | sflag_i)) == 0)
  {
  ps_slur(ix0, iy0, ix1, iy1, flags, co);
  return;
  }

/* Handle straight-line "slurs". For these, the start and stop values are not
used, as partial lines are drawn as separate whole lines with appropriate jog
flags. */

if ((flags & sflag_l) != 0)
  {
  uint32_t lineflags = 0;
  int32_t adjust = 0;
  int32_t thickness = (3*out_stavemagn)/10;

  ix1 += 7*out_stavemagn;
  co = mac_muldiv((co + 3000)*above, out_stavemagn, 1000);

  /* Convert the flags to the tie flags used by the ps_line function, then
  output the main portion of the line. Set the savedash flag if drawing a
  dotted line so that the jogs are drawn with the same dash settings. */

  if ((flags & sflag_i) != 0)
    lineflags |= ((flags & sflag_idot) == 0)?
      tief_dashed : tief_dotted | tief_savedash;
  if ((flags & sflag_e) != 0) lineflags |= tief_editorial;
  ps_line(ix0, iy0 + co, ix1, iy1 + co, thickness, lineflags);

  /* Don't pass any flag settings for drawing the jogs; for dotted lines the
  previous savedash ensures that the same setting is used for them. For dashed
  lines the jogs shouldn't be dashed. For dotted lines we may need to lengthen
  the jog to ensure at least one extra dot is drawn, and we change the
  thickness. Also, reduce the gap length slightly because there's an optical
  illusion that makes it look bigger than it is. Avoid redrawing the dot at the
  joining point. */

  if ((flags & sflag_idot) != 0)
    {
    thickness = out_stavemagn;
    ps_setdash(out_dashlength, (out_dashgaplength*95)/100);
    ps_setcapandjoin(caj_round);
    if (abs(co) < 2*out_dashlength + out_dashgaplength)
      adjust = above*(2*out_dashlength + out_dashgaplength) - co;
    }
  else  co += (above*thickness)/2;

  if ((flags & sflag_ol) == 0)
    ps_line(ix0, iy0 + co - above*(out_dashlength+out_dashgaplength), ix0,
      iy0 - adjust, thickness, 0);
  if ((flags & sflag_or) == 0)
    ps_line(ix1, iy1 + co - above*(out_dashlength+out_dashgaplength), ix1,
      iy1 - adjust, thickness, 0);

  ps_setdash(0, 0);             /* Clear saved setting if no jogs */
  ps_setcapandjoin(caj_butt);
  return;
  }


/* Handle the more complicated types of curved slur: partial, dashed, wiggly,
etc. */

int wiggly = ((flags & sflag_w) == 0)? (+1) : (-1);
int32_t ed_adjust;
int32_t x[10], y[10], cc[10];

double zz[4];
double temp;
double x0, x1, x2, x3, y0, y1, y2, y3;
double ax, ay, bx, by, cx, cy, f, g;
double xx = (double)ix1 - (double)ix0;
double yy = (double)iy1 - (double)iy0;
double w = sqrt(xx*xx + yy*yy)/2.0;
double v = w*0.6667;

if (v > 10000.0) v = 10000.0;

/* It is necessary to use floor() in the conversion of xx*0.3 to an integer in
the next statement in order to get the same value under valgrind. We know that
xx is positive, so we don't need to test whether to use floor() or ceil().
Using the (int) cast only on a variable (not on a function) avoids a compiler
warning. */

temp = floor(xx * 0.3);
co = (above * (co + ((xx > 20000)? 6000 : (int)temp)) * out_stavemagn)/1000;

f = ((double)start)/1000.0;
g = ((double)stop)/1000.0;

/* Preserve current coordinate system, translate and rotate so that the end
points of the slur lie on the x-axis, symetrically about the origin. For
ps_translate, the y value is relative to the stave base. Thereafter use
ps_abspath() for absolute values. */

ps_gsave();
ps_translate((ix0+ix1+6*out_stavemagn)/2, (iy0+iy1)/2);
ps_rotate(atan2(yy, xx));

/* Set up traditional Bezier coordinates for the complete slur. */

x0 = -w;
x1 = v - w + (double)out_slurclx;
x2 = w - v + (double)out_slurcrx;
x3 = +w;

y0 = 50.0;
y1 = (double)(int)(co + out_slurcly);
y2 = (double)(int)(co*wiggly + out_slurcry);
y3 = 50.0;

/* Calculate the coefficients for the original x parametric equation. */

ax = x3 - x0 + 3.0*(x1 - x2);
bx = 3.0*(x2 - 2.0*x1 + x0);
cx = 3.0*(x1 - x0);

/* The given fractions are fractions along the line joining the two end points.
These do not correspond linearly with the parameter t of the complete curve, so
we have to calculate new fractional values. */

if (f > 0.0 && f < 1.0) f = bezfraction(f, ax, bx, cx, x0, x3);
if (g > 0.0 && g < 1.0) g = bezfraction(g, ax, bx, cx, x0, x3);

/* Now calculate the new Bezier point coordinates for the portion of the slur
that we want, and set up the first path to be that portion. We used to compute
the x values with just an (int) cast, but this gave slightly different values
under valgrind. Using floor() or ceil() with a rounding value solves that
problem. We must also avoid using an (int) cast directly on these functions,
because it provokes a compiler warning when -Wbad-function-cast is set. */

zz[0] = x0 + ((ax*f + bx)*f + cx)*f;
zz[1] = x0 + (((3.0*ax*g + bx)*f + 2.0*(bx*g + cx))*f + cx*g)/3.0;
zz[2] = x0 + (((3.0*ax*g + 2.0*bx)*g + cx)*f + 2.0*cx*g + bx*g*g)/3.0;
zz[3] = x0 + ((ax*g + bx)*g + cx)*g;

temp = (zz[0] >= 0.0)? floor(zz[0] + 0.0001) : ceil(zz[0] - 0.0001);
x[0] = (int)temp;
temp = (zz[1] >= 0.0)? floor(zz[1] + 0.0001) : ceil(zz[1] - 0.0001);
x[1] = (int)temp;
temp = (zz[2] >= 0.0)? floor(zz[2] + 0.0001) : ceil(zz[2] - 0.0001);
x[2] = (int)temp;
temp = (zz[3] >= 0.0)? floor(zz[3] + 0.0001) : ceil(zz[3] - 0.0001);
x[3] = (int)temp;

/* Now do exactly the same for the y points */

ay = y3 - y0 + 3.0*(y1 - y2);
by = 3.0*(y2 - 2.0*y1 + y0);
cy = 3.0*(y1 - y0);

zz[0] = y0 + ((ay*f + by)*f + cy)*f;
zz[1] = y0 + (((3.0*ay*g + by)*f + 2.0*(by*g + cy))*f + cy*g)/3.0;
zz[2] = y0 + (((3.0*ay*g + 2.0*by)*g + cy)*f + 2.0*cy*g + by*g*g)/3.0;
zz[3] = y0 + ((ay*g + by)*g + cy)*g;

temp = (zz[0] >= 0.0)? floor(zz[0] + 0.0001) : ceil(zz[0] - 0.0001);
y[0] = (int)temp;
temp = (zz[1] >= 0.0)? floor(zz[1] + 0.0001) : ceil(zz[1] - 0.0001);
y[1] = (int)temp;
temp = (zz[2] >= 0.0)? floor(zz[2] + 0.0001) : ceil(zz[2] - 0.0001);
y[2] = (int)temp;
temp = (zz[3] >= 0.0)? floor(zz[3] + 0.0001) : ceil(zz[3] - 0.0001);
y[3] = (int)temp;

cc[0] = path_move;
cc[1] = path_curve;

/* Deal with dashed slurs. The only way to do a decent job is to calculate the
actual length of the slur. This has to be done the hard way by numerically
integrating along the path, as the formulae don't give an analytic answer. To
make sure that a full dash ends the slur when there are gaps in the slur,
arrange for the slur to be drawn backwards if it doesn't start at the
beginning, but does end at the end. (We could compute separate dashes for the
individual parts, but that would probably look odd. Slurgaps in dashes slurs
should be most rare, anyway.) */

if ((flags & sflag_i) != 0)
  {
  int dashcount;
  int32_t dashlength, gaplength, thickness, length;
  double dlength = 0.0;
  double xp = x0;
  double yp = y0;
  double t;

  /* Compute the curve length by integration. */

  for (t = 0.0; t < 1.04; t += 0.05)
    {
    double xxc = ((ax*t + bx)*t + cx)*t + x0;
    double yyc = ((ay*t + by)*t + cy)*t + y0;
    dlength += sqrt((xxc-xp)*(xxc-xp) + (yyc-yp)*(yyc-yp));
    xp = xxc;
    yp = yyc;
    }
  length = (int)dlength;

  /* Choose a dash length, spacing parameter, and line thickness */

  if ((flags & sflag_idot) == 0)
    {
    dashlength = length/14;
    if (dashlength < 3000) dashlength = 3000;
    gaplength = (dashlength * 875)/1000;
    thickness = 500;
    }
  else
    {
    dashlength = 100;
    gaplength = ((length < 20000)? 3 : 4) * out_stavemagn;
    thickness = out_stavemagn;
    }

  /* Compute the number of dashes; if greater than one, compute the accurate
  gaplength and set dashing. */

  dashcount = (length + gaplength + (dashlength + gaplength)/2) /
    (dashlength + gaplength);
  if (dashcount > 1)
    {
    gaplength = (length - dashcount * dashlength)/(dashcount - 1);
    ps_setdash(dashlength, gaplength);
    ps_setcapandjoin(((flags & sflag_idot) == 0)? caj_butt : caj_round);
    }

  /* Invert drawing order of partial curve that ends at the full end */

  if (start > 0 && stop == 1000)
    {
    int tt;
    tt = x[0]; x[0] = x[3]; x[3] = tt;
    tt = x[1]; x[1] = x[2]; x[2] = tt;
    tt = y[0]; y[0] = y[3]; y[3] = tt;
    tt = y[1]; y[1] = y[2]; y[2] = tt;
    }

  /* Draw the dashed curve, and set editorial line adjustment to zero. */

  cc[2] = path_end;
  ps_abspath(x, y, cc, thickness);
  ps_setdash(0, 0);                /* Reset default */
  ps_setcapandjoin(caj_butt);
  ed_adjust = 0;
  }

/* Deal with a non-dashed slur. For the other boundary of the slur, only the y
coordinates of the control points change. */

else
  {
  double aay, bby, ccy;
  ed_adjust = (9*out_stavemagn)/10;

  x[4] = x[3];
  x[5] = x[2];
  x[6] = x[1];
  x[7] = x[0];

  y0 = -50.0;
  y1 = (double)(int)(co + above*out_stavemagn + out_slurcly);
  y2 = (double)(int)(co*wiggly + above*out_stavemagn + out_slurcry);
  y3 = -50.0;

  aay = y3 - y0 + 3.0*(y1 - y2);
  bby = 3.0*(y2 - 2.0*y1 + y0);
  ccy = 3.0*(y1 - y0);

  y[7] = (int)(y0 + ((aay*f + bby)*f + ccy)*f);
  y[6] = (int)(y0 + (((3.0*aay*g + bby)*f + 2.0*(bby*g + ccy))*f + ccy*g)/3.0);
  y[5] = (int)(y0 + (((3.0*aay*g + 2.0*bby)*g + ccy)*f + 2.0*ccy*g + bby*g*g)/3.0);
  y[4] = (int)(y0 + ((aay*g + bby)*g + ccy)*g);

  cc[2] = path_line;
  cc[3] = path_curve;

  x[8] = x[0];
  y[8] = y[0];

  cc[4] = path_line;
  cc[5] = path_end;

  /* Fill the path (thickness = -1) */

  ps_abspath(x, y, cc, -1);
  }

/* Deal with editorial slurs - only draw the mark when drawing the middle
portion. */

if ((flags & sflag_e) != 0 && start < 500 && stop > 500)
  {
  int32_t xxm, yym;
  double dx, dy, theta, cs, sn;

  /* Calculate the midpoint of the curve from the parametric equations taking
  t = 0.5, and also calculate the slope at that point. */

  xxm = (int)(((ax*0.5 + bx)*0.5 + cx)*0.5 - w);
  dx = (3.0*ax*0.5 + 2.0*bx)*0.5 + cx;

  yym = (int)(((ay*0.5 + by)*0.5 + cy)*0.5);
  dy = (3.0*ay*0.5 + 2.0*by)*0.5 + cy;

  /* Draw the editorial mark. Translate and rotate gave rounding errors, so do
  it by steam. */

  theta = atan2(dy, dx);
  cs = cos(theta);
  sn = sin(theta);

  if (above > 0)
    {
    x[0] = xxm - (int)((2000.0+(double)ed_adjust)*sn);
    y[0] = yym + (int)((2000.0+(double)ed_adjust)*cs);
    x[1] = xxm + (int)(2000.0*sn);
    y[1] = yym - (int)(2000.0*cs);
    }
  else
    {
    x[0] = xxm - (int)(2000.0*sn);
    y[0] = yym + (int)(2000.0*cs);
    x[1] = xxm + (int)((2000.0+(double)ed_adjust)*sn);
    y[1] = yym - (int)((2000.0+(double)ed_adjust)*cs);
    }

  cc[0] = path_move;
  cc[1] = path_line;
  cc[2] = path_end;
  ps_abspath(x, y, cc, 400);
  }

/* Restore the former coordinate system. */

ps_grestore();
}

/* End of setslur.c */
