/*************************************************
*        PMW code for setting non-note things    *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: June 2021 */
/* This file last modified: September 2021 */

#include "pmw.h"

/* Adjustments for righthand repeats */

static uint8_t rrepeat_adjust[] = { 66, 50, 50, 50, 66,  65, 85, 85, 85, 65 };

/* Caesura strings */

static uschar *caesurastrings[] = { US"V", US"\\" };


/*************************************************
*          Deal with start of hairpin            *
*************************************************/

/* This remembers the hairpin parameters for later use.

Arguments:
  h           the hairpin data for the start of the hairpin
  x           the x coordinate of the start of the hairpin

Returns:      nothing
*/

static void
setstarthairpin(b_hairpinstr *h, int32_t x)
{
hairpinstr *hh = mem_get_cached((void **)(&main_freehairpinstr),
  sizeof(hairpinstr));

hh->hairpin = h;
hh->x = x;
hh->maxy = INT32_MIN;
hh->miny = INT32_MAX;
bar_cont->hairpin = hh;

if ((h->flags & hp_bar) != 0)
  hh->x = out_startlinebar?
    (out_sysblock->timexposition + out_sysblock->xjustify) : out_lastbarlinex;

/* The /bar option overrides /h */

else if ((h->flags & hp_halfway) != 0) out_hairpinhalf = TRUE;

/* And /h overrides /lc and /rc. Adjust the x coordinate by looking for the
relevant musical offset. If x == 0 it means we are setting up for a
continuation hairpin while paginating, so don't do anything. */

else if (h->offset != 0 && x != 0)
  {
  int32_t offset = mac_muldiv(len_crotchet, h->offset, 1000);
  hh->x = out_barx + out_findAoffset(out_moff + offset);
  }
}



/*************************************************
*               Draw a hairpin                   *
*************************************************/

/* This actually draws the hairpin. It is also called from setbar in order to
draw end-of-line part hairpins.

Arguments:
  h1           the data for the end of the hairpin
  x1           the x coordinate of the end of the hairpin

Returns:       nothing
*/

void
out_drawhairpin(b_hairpinstr *h1, int32_t x1)
{
hairpinstr *hh = bar_cont->hairpin;
b_hairpinstr *h0 = hh->hairpin;
int32_t cwidth = h0->width/2;
int32_t dwidth = cwidth;
int32_t thickness = curmovt->hairpinlinewidth;
int32_t x0 = hh->x;
int32_t offset, y0, y1, y0hole, y1hole;
uint16_t flags = h0->flags;
BOOL abs = (flags & hp_abs) != 0;
BOOL skip = FALSE;

/* Compute basic offset from stave base */

if ((flags & hp_below) == 0)
  {
  offset = abs? 16000 : (((hh->maxy > 16000)? hh->maxy + 6000 : 22000) +
    (h0->width - 7000)/2);
  }

else
  {
  offset = abs? 0 : (((hh->miny < 0)? hh->miny - 6000 : -6000) -
    (h0->width - 7000)/2);

  if ((flags & hp_middle) != 0 && curstave < out_laststave)
    {
    int32_t gap = out_sysblock->stavespacing[curstave]/2;
    int nextstave = curstave + 1;

    while (gap == 0 && nextstave < out_laststave)
      if (mac_isbit(out_sysblock->notsuspend, nextstave))
        gap = out_sysblock->stavespacing[nextstave++]/2;

    gap -= 8 * out_stavemagn;
    if (-gap < offset) offset = -gap;
    }
  }

/* At start of line, start just before first note; also set small gap at start
of hairpin. If continued decrescendo, start at smaller width. */

if (x0 == 0)
  {
  x0 = out_sysblock->firstnoteposition - 4*out_stavemagn;
  y0 = h0->y + ((h1 == NULL)? 0 : h1->su);
  y0hole = out_stavemagn;
  dwidth = (80*dwidth)/100;
  }
else  /* not start of line */
  {
  x0 += h0->x;
  y0 = h0->y;
  y0hole = 0;
  }

/* Add manual right-hand adjustment; at end of line we use the left-hand value.
Set small gap in decrescendo at end of line. */

if (h1 != NULL)
  {
  if ((h1->flags & hp_bar) != 0)      /* /bar overrides /h */
    {
    x1 = out_barlinex;
    }
  else if ((h1->flags & hp_halfway) != 0)
    {
    x1 += mac_muldiv(out_barx + out_findXoffset(out_moff) - x1 -
      6*out_stavemagn, h1->halfway, 1000);
    }
  else if (h1->offset != 0)
    {
    int32_t xoffset = mac_muldiv(len_crotchet, h1->offset, 1000);
    x1 = out_barx + out_findAoffset(out_moff + xoffset);
    }
  x1 += h1->x;
  y1 = h0->y + h1->y;
  y1hole = 0;
  }

else  /* end of line; reduce crescendo width a bit */
  {
  y1 = h0->y + h0->su;
  y1hole = out_stavemagn;
  cwidth = (cwidth*80)/100;
  }

/* Final y values */

y0 = ((y0 + offset)*out_stavemagn)/1000;
y1 = ((y1 + offset)*out_stavemagn)/1000;

/* Draw the hairpin, enforcing a minimum length of 10 points, except at the end
of line where we can suppress a decrescendo. We can't suppress a crescendo -
the user will have to fix. */

if (x1 - x0 < 10*out_stavemagn)
  {
  if (h1 != NULL || (h0->flags & hp_cresc) != 0) x1 = x0 + 10*out_stavemagn;
    else skip = TRUE;
  }

if (!skip)
  {
  if ((h0->flags & hp_cresc) == 0)
    {
    ps_line(x0, y0 + dwidth, x1, y1 + y1hole, thickness, 0);
    ps_line(x0, y0 - dwidth, x1, y1 - y1hole, thickness, 0);
    }
  else
    {
    ps_line(x0, y0 + y0hole, x1, y1 + cwidth, thickness, 0);
    ps_line(x0, y0 - y0hole, x1, y1 - cwidth, thickness, 0);
    }
  }

/* Put the dynamic block on its free chain and clear the pointer. */

mem_free_cached((void **)(&main_freehairpinstr), hh);
bar_cont->hairpin = NULL;
}



/*************************************************
*      Deal with non-note item in a bar          *
*************************************************/

/* This function is used for all the various non-note items. When there's a lot
to do, a separate function is called. There's also a separate function for
those the are handled the same way in the pagination scan (see at the end).

Argument:
  p         the item's data
Returns:    TRUE if the bar ends with a right repeat
*/

BOOL
out_setother(bstr *p)
{
BOOL yield = FALSE;

TRACE("out_setother() start\n");

switch (p->type)
  {
  /* Set barline break just for this stave */

  case b_breakbarline:
  out_ybarend = out_ystave;
  break;

  /* Set no barline break just for this stave */

  case b_unbreakbarline:
  out_ybarend = out_ybarenddeep;
  break;

  /* Remember x adjustment for dots for next note */

  case b_dotright:
  n_dotxadjust = (((b_dotrightstr *)p)->value * out_stavemagn)/1000;
  break;

  /* Remember start of tie/short slur and/or glissando */

  case b_tie:
  bar_cont->tie = (b_tiestr *)p;
  bar_cont->tiex = out_lastnotex + n_dotxadjust;
  n_dotxadjust = 0;
  break;

  /* Remember start of long slur */

  case b_slur:
    {
    slurstr *s = slur_startslur((b_slurstr *)p);

    /* The x & y position of the start of the slur gets set when the next note
    is processed, as a result of the slurstarted flag. However, put in some
    defaults for the mad case when the slur does not cross any notes. */

    s->moff = out_moff;
    s->x = out_barx + out_findXoffset(out_moff);
    s->y = L_3L;

    /* The slurstarted flag tells the note processing routine to scan the slur
    data for any slurs with s->count set to 0, and to set their left
    x-coordinate according to that [moved] note, and also to set other
    parameters appropriately. */

    out_slurstarted = TRUE;
    }
  break;

  /* Draw a long slur or line. Note: there is no separate b_endline item. If
  there is no id, draw the most recent slur. Otherwise search for the correct
  identity, complaining if not found, though that should have been picked up
  earlier during the setting up of the cont structure. Note that slur_endslur()
  puts the structure back on its free chain, knowing that its data will be used
  before another [slur] is processed. */

  case b_endline:
  case b_endslur:
    {
    b_endslurstr *es = (b_endslurstr *)p;
    slurstr *s = slur_endslur(es);
    if (s == NULL)
      {
      char buff[8];
      if (es->value == 0) buff[0] = 0; else sprintf(buff, " \"%c\"", es->value);
      if (p->type == b_endslur) error(ERR132, "slur", buff, "[endslur]");
        else error(ERR132, "line", buff, "[endline]");
      }
    else
      slur_drawslur(s, n_x, n_pitch, FALSE);
    }
  break;

  /* Deal with gaps in lines and slurs. The coordinate data has to be saved,
  because the vertical position of the line or slur is not yet known. */

  case b_linegap:
  case b_slurgap:
    {
    b_slurgapstr *pg = (b_slurgapstr *)p;
    slurstr *s = bar_cont->slurs;
    gapstr *g;
    int slurid = pg->id;

    if (slurid != 0)
      {
      while (s != NULL)
        {
        if ((s->slur)->id == slurid) break;
        s = s->next;
        }
      }

    if (s == NULL)
      {
      char buff[8];
      if (pg->id == 0) buff[0] = 0; else sprintf(buff, "\"%c\"", pg->id);
      if (p->type == b_slurgap) error(ERR132, "slur", buff, "[slurgap]");
        else error(ERR132, "line", buff, "[linegap]" );
      break;
      }

    if ((s->slur->flags & sflag_l) == 0)
      {
      if (p->type == b_linegap)
        {
        error(ERR145, "linegap", "slur");
        break;
        }
      }
    else
      {
      if (p->type == b_slurgap)
        {
        error(ERR145, "slurgap", "line");
        break;
        }
      }

    g = mem_get_cached((void **)&main_freegapblocks, sizeof(gapstr));
    g->next = s->gaps;
    s->gaps = g;
    g->gap = pg;
    g->x = out_barx + out_findXoffset(out_moff) + pg->xadjust;

    /* Except at the end of a bar, move right to the centre of
    the next notehead. */

    if (misc_nextnote((b_notestr *)p) != NULL) g->x += 3*out_stavemagn;
    }
  break;

  /* Deal with hairpins. Take special action for a hairpin that ends before
  the first note of a bar. */

  case b_hairpin:
    {
    b_hairpinstr *h = (b_hairpinstr *)p;
    if ((h->flags & hp_end) == 0)
      {
      setstarthairpin(h, out_barx + out_findXoffset(out_moff));
      }
    else if (out_moff == 0)
      out_drawhairpin(h, out_barx - 4*out_stavemagn);
    else
      out_drawhairpin(h, n_x + 6*out_stavemagn);
    }
  break;

  /* Deal with position reset */

  case b_reset:
  out_moff = 0;
  out_passedreset = TRUE;
  break;

  /* Deal with inter-note tremolo; remember data for use after next note. */

  case b_tremolo:
  if (n_pitch != 0)
    {
    out_tremolo = (b_tremolostr *)p;
    out_tremupflag = n_upflag;
    out_tremx = n_x;
    out_tremy = misc_ybound(n_notetype < minim || !n_upflag, NULL, FALSE, FALSE);
    }
  break;

  /* Deal with plets */

  case b_plet:
  out_plet = (b_pletstr *)p;
  beam_seq = -1;                  /* Maintain beaming state */
  out_plet_x = -1;                /* Indicates plet starting */
  out_plet_highest = INT32_MIN;
  out_plet_lowest  = INT32_MAX;
  out_plet_highest_head = 0;
  out_pletden = out_plet->pletlen;
  if (out_pletden > 4) out_pletnum = 2;
  break;

  case b_endplet:
    {
    int flags = out_plet->flags;

    if (((bar_cont->flags & cf_triplets) == 0) != ((flags & plet_x) == 0))
      {
      fontinststr fdata = curmovt->fontsizes->fontsize_triplet;
      int32_t x0 = out_plet_x - (5*out_stavemagn)/10;
      int32_t x1 = n_x + 7*out_stavemagn;
      int32_t mid = (x0 + x1)/2;
      int32_t yy, yyl, yyr, sx, width;
      BOOL above, omitline;
      uschar s[10];
      uint32_t *ss;

      /* Determine whether above or below, either by forcing, or by beaming, or
      on the position of the highest notehead. */

      if ((flags & (plet_a | plet_b)) != 0) above = (flags & plet_a) != 0;
        else if (beam_seq == 1) above = beam_upflag;
          else above = out_plet_highest_head < P_3L;

      if ((flags & (plet_by | plet_bn)) != 0)
        omitline = (flags & plet_bn) != 0;
          else omitline = (beam_seq == 1 && above == beam_upflag);

      /* Compute y level for the number according to the highest and lowest
      bits of note in between, or take absolute level if given. */

      if ((flags & plet_abs) != 0) yy = above? 16000 : 0; else
        {
        if (above)
          yy = (out_plet_highest < 16000)? 18000 : out_plet_highest + 2000;
        else
          yy = ((out_plet_lowest > 0)? 0 : out_plet_lowest) - fdata.size;
        }

      yyl = ((yy + out_plet->yleft)*out_stavemagn)/1000;   /* Manual adjustment */
      yyr = ((yy + out_plet->yright)*out_stavemagn)/1000;  /* Manual adjustment */

      yy = (yyl + yyr)/2;   /* mid height */

      /* Scale the font size, create the string, and find half its width. We
      scale the font size according to the stave size and also according to the
      size of the previous note. */

      fdata.size = mac_muldiv(fdata.size*out_stavemagn, n_fontsize, 10000000);

      sprintf(CS s, "%d", out_plet->pletlen);
      ss = string_pmw(s, curmovt->fonttype_triplet);
      width = string_width(ss, &fdata, NULL)/2;

      /* If beamed and no line, compute text position according to the beam;
      otherwise compute it as central in the line */

      if (beam_seq == 1 && above == beam_upflag && omitline)
        {
        sx = (out_plet_x + n_x)/2 + out_plet->x;
        if (beam_upflag) sx += (51*out_stavemagn)/10;
        }
      else sx = mid + (omitline? out_plet->x : 0);

      /* Now output the text */

      out_string(ss, &fdata, sx - width, out_ystave - yy, 0);

      /* Draw the line if wanted */

      if (!omitline)
        {
        int32_t slope = mac_muldiv(yyr - yyl, 1000, x1 - x0);
        int32_t x[3], y[3];
        int32_t ly, ry;

        ly = ry = above? -2000 : 2000;

        if ((flags & plet_lx) != 0) ly = -ly;
        if ((flags & plet_rx) != 0) ry = -ry;

        yyl += (35*fdata.size)/100;
        yyr += (35*fdata.size)/100;

        x[0] = x[1] = x0;
        x[2] = mid - 1500 - width;
        y[0] = yyl + ly;
        y[1] = yyl;
        y[2] = yyl + mac_muldiv(slope, x[2] - x0, 1000);
        ps_lines(x, y, 3, curmovt->tripletlinewidth);

        x[0] = mid + 1500 + width;
        x[1] = x[2] = x1;
        y[0] = yyr - mac_muldiv(slope, x1 - x[0], 1000);
        y[1] = yyr;
        y[2] = yyr + ry;
        ps_lines(x, y, 3, curmovt->tripletlinewidth);
        }
      }
    }

  out_plet = NULL;
  out_pletnum = 1;
  out_pletden = 2;
  break;

  /* Clefs, keys, and times */

  case b_clef:
    {
    b_clefstr *c = (b_clefstr *)p;
    if (curstave != 0 && !c->assume && !c->suppress)
      out_writeclef(out_barx + out_findXoffset(out_moff + posx_clef) +
        out_Xadjustment, out_ystave - out_Yadjustment, c->clef,
          curmovt->fontsizes->fontsize_midclefs.size, TRUE);
    bar_cont->clef = c->clef;
    out_Xadjustment = out_Yadjustment = 0;
    }
  break;

  case b_key:
    {
    b_keystr *k = (b_keystr *)p;
    if (curstave != 0 && !k->suppress && !k->assume)
      {
      int32_t x = out_barx +
        out_findXoffset(out_moff + posx_keyfirst + out_keycount++) +
          out_Xadjustment;
      if (!out_startlinebar && out_moff == 0 &&
          MFLAG(mf_keydoublebar) && !out_repeatonbarline &&
          curbarnumber != curmovt->startbracketbar)
        ps_barline(out_lastbarlinex, out_ystave, out_ybarend, bar_double,
          ((curmovt->barlinesize > 0)? curmovt->barlinesize : out_stavemagn));
      out_writekey(x, out_ystave - out_Yadjustment, bar_cont->clef, k->key);
      }
    out_Xadjustment = out_Yadjustment = 0;
    }
  break;

  case b_time:
    {
    b_timestr *t = (b_timestr *)p;
    if (curstave != 0 && !t->suppress && !t->assume)
      out_writetime(out_barx +
        out_findXoffset(out_moff + posx_timefirst + out_timecount++) +
          out_Xadjustment, out_ystave - out_Yadjustment, t->time);
    bar_cont->time = t->time;
    out_Xadjustment = out_Yadjustment = 0;
    }
  break;

  /* Dotted bar line in mid-bar */

  case b_dotbar:
  ps_barline(out_barx+out_findXoffset(out_moff+posx_dotbar)+out_Xadjustment,
    out_ystave, out_ybarend, bar_dotted, out_barlinemagn);
  out_Xadjustment = out_Yadjustment = 0;
  break;

  /* Repeat marks. For a right repeat we have to check for a following left
  repeat, because some styles are different when the two are simultaneous. */

  case b_rrepeat:
  if (misc_nextnote((b_notestr *)p) == NULL)   /* check for end of bar */
    {
    int32_t xadjust;
    int reptype = rep_right;

    if (out_lineendflag) xadjust = (curbarnumber + 1 >= curmovt->barcount)?
      (rrepeat_adjust[curmovt->repeatstyle + (MFLAG(mf_unfinished)? 0:5)]) : 68;
    else
      {
      barstr *pp = ((curmovt->stavetable)[curstave])->barindex[curbarnumber+1];
      if (pp->next->type == b_lrepeat)
        {
        reptype = rep_dright;
        bar_cont->flags |= cf_rdrepeat;   /* Remember for next start */
        }
      xadjust = 50;
      }
    out_lastbarwide = TRUE;
    out_writerepeat(out_barlinex - (xadjust*out_barlinemagn)/10, reptype,
      out_barlinemagn);
    yield = TRUE;   /* Return bar ends with :) */
    }

  /* Not at end of bar; we look at the next item to see if it is a left repeat. */

  else
    {
    out_writerepeat(out_barx +
      out_findXoffset(out_moff + posx_RR) + out_Xadjustment,
      (p->next->type == b_lrepeat)? rep_dright : rep_right, out_barlinemagn);
     }

  out_Xadjustment = out_Yadjustment = 0;
  break;

  /* See if there is also a right repeat here. If not, we put the sign on the
  bar line for non-start of line bars, if there is no position for it. If the
  previous bar ended with a repeat, this repeat will have been found and the
  flag set. Note that the variable out_posxRL contains the relevant posx_RLxxx
  value this this particular bar. */

  case b_lrepeat:
  if (out_findTentry(out_moff + posx_RR) == NULL)
    {
    int posxRL = curmovt->posvector[curbarnumber].posxRL;

    if (!out_startlinebar && out_findTentry(out_moff - posxRL) == NULL)
      {
      out_writerepeat(out_lastbarlinex,
        ((bar_cont->flags & cf_rdrepeat) == 0)? rep_left : rep_dleft,
        out_barlinemagn);
      bar_cont->flags &= ~cf_rdrepeat;
      out_repeatonbarline = TRUE;
      }

    else out_writerepeat(out_barx +
      out_findXoffset(out_moff - posxRL) + out_Xadjustment, rep_left,
      out_barlinemagn);
    }

  /* Right repeat exists */

  else
    {
    if (misc_nextnote((b_notestr *)p) == NULL)
      out_writerepeat(out_barlinex -
        (out_lineendflag? (18*out_stavemagn)/10 : 0), rep_dleft, out_barlinemagn);
    else
      out_writerepeat(out_barx + out_findXoffset(out_moff + posx_RR) +
        5*out_stavemagn + out_Xadjustment, rep_dleft, out_barlinemagn);
    }

  out_Xadjustment = out_Yadjustment = 0;
  break;

  /* Set ornament for next note */

  case b_ornament:
  if (n_ornament == NULL) n_ornament = ((b_ornamentstr *)p);
  break;

  /* Deal with dynamic move and/or bracketing for next note */

  case b_accentmove:
    {
    b_accentmovestr *am = (b_accentmovestr *)p;
    out_accentmove[am->accent] = am;
    }
  break;

  /* Pauses and breaths */

  case b_comma:
    {
    int y;
    uschar *s;
    b_notestr *pp = misc_nextnote((b_notestr *)p);
    if (main_righttoleft)
      {
      y = 19;
      s = US"\301";
      }
    else
      {
      y = 22;
      s = US"N";
      }
    if (pp != NULL && pp->spitch > P_6L) y += (pp->spitch - P_6L)/2;

    out_ascstring(s, font_mf, 10*out_stavemagn,
      out_barx + out_findXoffset(out_moff + posx_comma) + out_Xadjustment,
        out_ystave - y*out_stavemagn - out_Yadjustment);
    }
  out_Xadjustment = out_Yadjustment = 0;
  break;

  case b_tick:
    {
    int y = 18;
    b_notestr *pp = misc_nextnote((b_notestr *)p);
    if (pp != NULL && pp->spitch > P_6L) y += (pp->spitch - P_6L)/2;
    out_ascstring(US"\200", font_mf, 10*out_stavemagn,
      out_barx + out_findXoffset(out_moff + posx_tick) + out_Xadjustment,
        out_ystave - y*out_stavemagn - out_Yadjustment);
    }
  out_Xadjustment = out_Yadjustment = 0;
  break;

  case b_caesura:
  out_ascstring(caesurastrings[curmovt->caesurastyle], font_mf,
    10*out_stavemagn,
      out_barx + out_findXoffset(out_moff + posx_caesura) + out_Xadjustment,
        out_ystave - 12*out_stavemagn - out_Yadjustment);
  out_Xadjustment = out_Yadjustment = 0;
  break;

  /* Deal with nth time bar markings; if there is already a current block, we
  must differentiate between a number of different cases:

    (1) This is an additional marking to be added to the current one
    (2) This is the start of the next iteration at the start of a line
    (3) This is the start of the next iteration in the middle of a line

  In case (3) we have to close off the previous marking; in case (2) we
  replace the previous block; in case (1) we add an additional block. */

  case b_nbar:
    {
    int miny = 0;

    if (bar_cont->nbar != NULL)
      {
      nbarstr *bb = bar_cont->nbar;

      /* (1) Deal with additional marking; misc_startnbar() will hang an
      additional block onto the bar_cont->nbar list. The position arguments are
      not relevant in these extra blocks, hence zeros. */

      if (out_lastbarlinex == bb->x)
        {
        misc_startnbar((b_nbarstr *)p, 0, 0);
        break;    /* That's all */
        }

      /* (2) If this is a line start, the current data is superflous. Note
      that there will only ever be one block at the start of a line if its
      x-value doesn't match (i.e. it's continued). */

      if (out_startlinebar) misc_freenbar();

      /* (3) This is the middle of a line; close off the previous */

      else
        {
        miny = out_drawnbar(TRUE, out_lastbarlinex - 1500);
        misc_freenbar();
        }
      }

    /* In cases (2) and (3) remember the start of a new marking. */

    misc_startnbar((b_nbarstr *)p, out_lastbarlinex, miny);
    }
  break;

  case b_all:
  if (!out_startlinebar && bar_cont->nbar != NULL)
    out_drawnbar(FALSE, out_lastbarlinex);
  misc_freenbar();
  break;

  /* Set offset adjustment for next beam */

  case b_beammove:
  beam_offsetadjust = ((b_intvaluestr *)p)->value;
  break;

  /* Set next beam as accellerando */

  case b_beamacc:
  beam_accrit = ((b_beamaccritstr *)p)->value;
  break;

  /* Set next beam as ritardando */

  case b_beamrit:
  beam_accrit = -((b_beamaccritstr *)p)->value;
  break;

  /* Text at the end of a bar must be handled now; othewise we queue it to be
  done with the next note, when the bounding box is known. After setting n_x to
  the barline for the text, we must restore its previous value in case
  something that follows needs it, for example, an end slur. */

  case b_text:
  if (misc_nextnote((b_notestr *)p) == NULL)
    {
    int32_t savex = n_x;
    n_x = out_barlinex;
    out_text((b_textstr *)p, TRUE);
    n_x = savex;
    }
  else
    {
    if (out_textqueue_ptr >= out_textqueue_size)
      {
      if (out_textqueue_size >= TEXTQUEUE_SIZELIMIT)   /* Hard error */
        error(ERR5, "text queue", TEXTQUEUE_SIZELIMIT * sizeof(b_textstr *));
      out_textqueue_size += TEXTQUEUE_CHUNKSIZE;
      out_textqueue = realloc(out_textqueue,
        out_textqueue_size * sizeof(b_textstr *));
      if (out_textqueue == NULL)
        error(ERR0, "re-", "text queue buffer", out_textqueue_size);  /* Hard */
      }
    out_textqueue[out_textqueue_ptr++] = (b_textstr *)p;
    }
  break;

  /* Draw items associated with notes must be saved, as for text items. */

  case b_draw:
  if (misc_nextnote((b_notestr *)p) == NULL)
    {
    b_drawstr *d = (b_drawstr *)p;
    draw_ox = out_barlinex;
    draw_oy = 0;
    out_dodraw(d->drawing, d->drawargs, d->overflag);
    }
  else
    {
    if (out_drawqueue_ptr >= out_drawqueue_size)
      {
      if (out_drawqueue_size >= DRAWQUEUE_SIZELIMIT)   /* Hard error */
        error(ERR5, "draw queue", DRAWQUEUE_SIZELIMIT * sizeof(b_drawstr *));
      out_drawqueue_size += DRAWQUEUE_CHUNKSIZE;
      out_drawqueue = realloc(out_drawqueue,
        out_drawqueue_size * sizeof(b_drawstr *));
      if (out_drawqueue == NULL)
        error(ERR0, "re-", "draw queue buffer", out_drawqueue_size);  /* Hard */
      }
    out_drawqueue[out_drawqueue_ptr++] = (b_drawstr *)p;
    }
  break;

  /* Positioning adjustments - horizontal distances may or may not be scaled;
  vertical distances are always scaled. */

  case b_move:
    {
    b_movestr *bm = (b_movestr *)p;
    if (bm->relative) out_Xadjustment = (bm->x * out_stavemagn)/1000;
      else out_Xadjustment = bm->x;
    out_Yadjustment = (bm->y * out_stavemagn)/1000;
    }
  break;


  /* Stave zero copy level adjustment - applies to copies of stave 0 being
  printed at this stave's level. */

  case b_zerocopy:
    {
    zerocopystr *z = curmovt->zerocopy;
    while (z != NULL)
      {
      if (z->level == out_ystave - out_yposition)
        {
        z->baradjust = ((b_zerocopystr *)p)->value;
        break;
        }
      z = z->next;
      }
    }
  break;

  /* Handle all items that are common between this scan and the pre-scan during
  pagination. */

  default:
  misc_commoncont(p);
  break;
  }

TRACE("out_setother() end\n");

return yield;
}

/* End of setother.c */
