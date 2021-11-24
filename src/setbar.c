/*************************************************
*              PMW output one bar                *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: June 2021 */
/* This file last modified: November 2021 */

#include "pmw.h"



/*************************************************
*             Static variables                   *
*************************************************/

static zerocopystr *bar_zerocopy;



/*************************************************
*       Check for and set up a stave 0 copy      *
*************************************************/

/* This is called from out_setbar() below, after stave 1 has been dealt with,
to set up for a copy of stave zero wherever needed. Stave zero can get printed
several times, with vertical adjustments. If bar_zerocopy is NULL, we have
completed the list. We have to maintain a separate contstr for each copy of
stave zero; there is a space in the zerocopystr block for doing this.

If this copy is required, set curstave = 1 and return; the loop in setbar()
will then iterate for stave 0 and then call this function again. If there is
more than one stave zero being printed, we must switch in the separate contstr
copies. */

static void
checkzerocopy(int zerocopycount)
{
while (bar_zerocopy != NULL)
  {
  if (bar_zerocopy->level >= 0)
    {
    out_ystave = out_yposition + bar_zerocopy->level - bar_zerocopy->adjust -
      bar_zerocopy->baradjust;
    bar_zerocopy->baradjust = 0;
    if (zerocopycount > 1) wk_cont = (contstr *)bar_zerocopy->cont;
    bar_zerocopy = bar_zerocopy->next;
    curstave = 1;
    return;
    }

  /* If this copy is not required, set stave = 0 so that if this loop
  now terminates, the setbar() loop will also end. */

  curstave = 0;
  bar_zerocopy = bar_zerocopy->next;
  }
}



/*************************************************
*             Output one bar in a system         *
*************************************************/

/* The bar number is in curbarnumber. The yield is the number of the last bar
output plus one, which may be greater than the value of out_bar plus one when
multiple rests are involved.

Arguments:  the number of copies of stave 0
Returns:    number of the next bar to be output
*/

int
out_setbar(int zerocopycount)
{
uint64_t notsuspend = out_sysblock->notsuspend;
barposstr *bp = curmovt->posvector + curbarnumber;
contstr *top_bar_cont = NULL;
contstr *wk_cont_save = wk_cont;
bstr *lastp = NULL;
uint32_t bn;
uint32_t barno_magn = 1000;
BOOL notfirst = FALSE;

TRACE("out_setbar() start: bar %d\n", curbarnumber);

bar_zerocopy = curmovt->zerocopy;

out_gracenotes = FALSE;
out_manyrest = bp->multi;
out_postable = out_posptr = bp->vector;
out_poslast = out_postable + bp->count - 1;
out_barlinex = out_barx + out_poslast->xoff;
out_ystave = out_yposition + out_sysblock->systemdepth;

out_lineendflag = (curbarnumber + out_manyrest - 1) == out_sysblock->barend;
out_repeatonbarline = out_lastbarwide;
out_lastbarwide = FALSE;

for (int i = 0; i < ACCENT_COUNT; i++) out_accentmove[i] = &no_accent_move;

/* Deal with the data on each stave. Do the printing from bottom to top so that
each stave's stuff gets printed before the bar line gets drawn into it from
above - otherwise the wiping stuff for beams may obliterate parts of the bar
lines. */

for (curstave = out_laststave; curstave >= 0; curstave--)
  {
  obeamstr *b;
  stavestr *ss = curmovt->stavetable[curstave];
  bstr *p = (bstr *)(ss->barindex[curbarnumber]);
  BOOL breakbarline;
  BOOL endrepeat;
  int barlinestyle;
  int barlinetype;
  int nextstave;

  TRACE("stave %d\n", curstave);

  /* Skip if stave is suspended or empty stave zero, but check for another
  stave 0 if this is stave 0 or stave 1. */

  if (mac_notbit(notsuspend, curstave) ||
       (curstave == 0 && p->next->type == b_barline))
    {
    if (curstave <= 1) checkzerocopy(zerocopycount);
    continue;
    }

  /* Initiate PostScript bar (generates a comment), and arrange for lastp to
  point to the data for the topmost real stave. */

  ps_startbar(curbarnumber, curstave);
  if (curstave > 0) lastp = p;

  /* Miscellaneous initialization */

  out_stavemagn = curmovt->stavesizes[curstave];
  out_pitchmagn = out_stavemagn/2;
  out_barlinemagn = (curmovt->barlinesize > 0)? curmovt->barlinesize:out_stavemagn;
  if (curstave > 0) barno_magn = out_stavemagn;

  breakbarline = mac_isbit(curmovt->breakbarlines, curstave);
  barlinestyle = 0;
  barlinetype = barline_normal;
  nextstave = 0;
  endrepeat = FALSE;

  /* Set the gaps for coupling - must find previous printing stave. As the gaps
  are in pitch units, they must be made relative to the stave magnification. */

  if (curstave == 0) out_upgap = 48; else
    {
    int previous;
    for (previous = curstave - 1; previous > 0; previous--)
      {
      if (mac_isbit(notsuspend, previous) &&
          out_sysblock->stavespacing[previous] != 0) break;
      }
    out_upgap = 2*out_sysblock->stavespacing[previous]/out_stavemagn;
    }

  out_downgap = 2*(out_sysblock->stavespacing[curstave])/out_stavemagn;
  if (out_downgap == 0)
    {
    for (int next = curstave + 1; next <= out_laststave; next++)
      {
      if (mac_isbit(notsuspend, next) && out_sysblock->stavespacing[next] != 0)
        {
        out_downgap = 2*out_sysblock->stavespacing[next]/out_stavemagn;
        break;
        }
      }
    }

  /* For all but the first to be printed (i.e. the bottom stave), move up by
  this stave's spacing */

  if (notfirst) out_ystave -= out_sysblock->stavespacing[curstave];
    else notfirst = TRUE;

  /* Set the bar_cont data before skip testing; we remember the value for the
  topmost stave for bar number printing. Also, we need to empty the beam
  continuation data if it set up. */

  bar_cont = wk_cont + curstave;
  if (curstave > 0) top_bar_cont = bar_cont;

  /* Set up beaming variables. The value of out_overbeam must only ever be true
  while out_setupbeam() is computing spanning beams. Hence we make sure it is
  FALSE here. */

  beam_forceslope = INT32_MAX;
  beam_offsetadjust = beam_accrit = 0;
  beam_overbeam = FALSE;

  /* If a beam has continued over a bar line, set up the various parameters;
  otherwise set no beaming. */

  b = bar_cont->overbeam;
  if (b != NULL && b->count > 0)
    {
    /* At the start of a line the beam has not been drawn. The continued flag
    will ensure that even one note gets a beam, and the longestnote value will
    be taken from this block. */

    if (out_startlinebar)
      {
      out_beaming = FALSE;
      beam_forceslope = b->slope;
      }

    /* For a non-start of line bar, the beam has already been drawn; set up the
    relevant parameters for the notes. */

    else
      {
      beam_firstX = b->firstX;
      beam_firstY = b->firstY;
      beam_slope = b->slope;
      beam_count = b->count;
      beam_Xcorrection = b->Xcorrection;
      beam_splitOK = b->splitOK;
      beam_upflag = b->upflag;
      out_beaming = TRUE;
      }

    beam_seq = 0;
    beam_continued = TRUE;
    b->count = -1;
    }
  else out_beaming = beam_continued = FALSE;

  /* Determine if there is a following printing stave, for the purpose of
  extending bar lines down. We have to look down to find it, in case it is an
  [omitempty] stave with no data. If any of the suspended staves we skip over
  have got their break barline bit set, we must break the barline here. If the
  current stavespacing is zero, we don't want to extend bar lines. */

  if (out_sysblock->stavespacing[curstave] > 0)
    {
    for (int i = curstave + 1; i <= out_laststave; i++)
      {
      if (mac_notbit(notsuspend, i))
        {
        if (mac_isbit(curmovt->breakbarlines, i)) breakbarline = TRUE;
        }
      else
        {
        stavestr *sss = curmovt->stavetable[i];
        if (sss == NULL || !sss->omitempty ||
          !mac_emptybar(sss->barindex[curbarnumber]) ||
            (!out_lineendflag && !mac_emptybar(sss->barindex[curbarnumber+1])))
              nextstave = i;
        break;
        }
      }
    }

  /* Initialize miscellaneous global variables. */

  n_gracecount = 0;       /* Counts a sequence of grace notes */
  n_masq = MASQ_UNSET;    /* Not masquerading */
  n_ornament = NULL;
  n_pitch = 0;            /* In case no items in the bar */

  out_gracenotes = FALSE;
  out_lastnotebeamed = FALSE;
  out_lastnotex = INT32_MIN/2;  /* large, negative (for accidental stretching) */
  out_keycount = 0;
  out_moff = out_lastmoff = 0;
  out_passedreset = FALSE;
  out_plet = NULL;
  out_pletnum = 1;
  out_pletden = 2;
  out_stavelines = ss->stavelines;
  out_textqueue_ptr = out_textnextabove = out_textnextbelow = 0;
  out_timecount = 0;
  out_tremolo = NULL;
  out_Xadjustment = out_Yadjustment = 0;

  out_stavetop = stave_tops[out_stavelines];
  out_stavebottom = stave_bottoms[out_stavelines];

  /* Calculate the bottom position of the bar line. We need to do this first
  because it is used within the bar sometimes (dotted lines, time signatures,
  repeats, etc.) If a [breakbarline] directive is encountered, the value will
  be reset to out_ystave; if [unbreakbarline] is found, the value will be reset
  to out_ybarenddeep. */

  out_ybarend = out_ybarenddeep = out_ystave;

  if (nextstave > 0)
    {
    out_ybarenddeep += out_sysblock->stavespacing[curstave] -
      16*curmovt->stavesizes[nextstave];
    if (!breakbarline) out_ybarend = out_ybarenddeep;
    }

  /* Loop through the items in the bar. After a non-note, switch off accidental
  stretching for the note that follows by setting out_lastnotex large and
  negative (but not INT32_MIN because that will cause overflow). */

  for (; p != NULL; p = p->next)
    {
    if (p->type == b_note)
      {
      p = out_setnote((b_notestr *)p);
      continue;
      }

    if (p->type == b_barline)
      {
      barlinestyle = ((b_barlinestr *)p)->barstyle;
      barlinetype = ((b_barlinestr *)p)->bartype;
      }
    else if (out_setother(p)) endrepeat = TRUE;

    out_lastnotex = INT32_MIN/2;  /* Last item was not a note */
    }

  /*  ---- End of loop through bar ---- */

  /* If this was the last bar on the line, deal with things which can be
  continued onto the next line. */

  if (out_lineendflag)
    {
    int32_t true_endline = out_barlinex;

    /* Slurs/lines and ties/glissandos may or may not cross warning signatures,
    depending on the configuration. Compute where any that are present will
    finish. */

    if (MFLAG(mf_tiesoverwarnings) && (out_sysblock->flags & sysblock_warn) != 0)
      {
      if ((out_sysblock->flags & sysblock_stretch) != 0)
        true_endline = curmovt->linelength;
      else if ((bp+1)->vector != NULL)     /* BUG FIX 04-Apr-2003 */
        true_endline += ((bp+1)->vector + 1)->xoff;
      }

    /* Draw outstanding slurs. */

    for (slurstr *s = bar_cont->slurs; s != NULL; )
      {
      slurstr *snext = s->next;
      slur_drawslur(s, true_endline - 3000 + curmovt->endlinesluradjust, 0, TRUE);
      mem_free_cached((void **)&main_freeslurblocks, s);
      s = snext;
      }
    bar_cont->slurs = NULL;

    /* Draw ties and glissandos to line end. For glissandos, invent a
    right-hand pitch that is in the right direction usually. */

    if ((n_prevtie = bar_cont->tie) != NULL)
      {
      int32_t x = true_endline - 4*out_stavemagn + curmovt->endlinetieadjust;
      uint32_t flags = n_prevtie->flags;

      n_flags = n_acflags = n_accleft = 0;
      n_notetype = breve;
      n_nexttie = NULL;

      /* Deal with ties. For chords we have to look ahead to the start of the
      next line to find the first note/chord, if any. */

      if ((flags & (tief_slur | tief_default)) != 0)
        {
        if (n_chordcount == 1) out_setnotetie(x, TRUE, flags); else
          {
          int count = 0;
          b_notestr *endnote = NULL;

          if (curbarnumber + 1 < curmovt->barcount)
            {
            for (bstr *pp = (bstr *)(ss->barindex[curbarnumber+1]);
                 pp != NULL;
                 pp = pp ->next)
              {
              if (pp->type == b_note)
                {
                endnote = (b_notestr *)pp;
                for (;;)
                  {
                  count++;
                  pp = pp->next;
                  if (pp->type != b_chord) break;
                  }
                break;
                }
              }
            }

          /* If there isn't a next bar, or if it is empty, draw ties on all the
          notes of the current chord. */

          if (count > 0) out_setchordtie(endnote, count, x, TRUE, flags);
            else out_setchordtie(n_chordfirst, n_chordcount, x, TRUE, flags);
          }
        }

      /* Deal with glissandos - single notes only */

      if ((flags & tief_gliss) != 0 && n_chordcount == 1)
        {
        b_notestr *pp = (curbarnumber + 1 >= curmovt->barcount)? NULL :
          misc_nextnote((b_notestr *)(ss->barindex[curbarnumber+1]));
        if (pp == NULL) n_pitch = (n_pitch > P_3L)? P_0S : P_5S;
          else n_pitch = pp->spitch;
        out_glissando(x + (45*out_stavemagn)/10, flags);
        }
      }

    /* If there is a hairpin outstanding, draw an incomplete one */

    if (bar_cont->hairpin != NULL)
      out_drawhairpin(NULL, out_barlinex - 4*out_stavemagn);

    /* If there is an outstanding nth-time requirement at the end of the last
    bar on a line, output the marking so far. We have to search the next bar to
    see if it starts a new one, to determine whether to draw a right-hand jog
    or not. */

    if (bar_cont->nbar != NULL)
      {
      BOOL flag = FALSE;
      if (curbarnumber + 1 >= curmovt->barcount) flag = TRUE; else
        {
        for (bstr *pp = (bstr *)(ss->barindex[curbarnumber + 1]);
             pp != NULL;
             pp = pp->next)
          {
          if (pp->type == b_nbar) { flag = TRUE; break; }
          }
        }
      (void)out_drawnbar(flag, out_barlinex);
      misc_freenbar();
      }

    /* Deal with outstanding underlay/overlay extensions or rows of hyphens at
    the end of the last bar on a line. In the case of hyphens, we must ensure
    that at least one hyphen is always output.

    Extension lines are drawn note by note, so a line to the last note on the
    stave will have been drawn. However, if the syllable continues on the next
    system, we would like to draw the line a bit longer. If the preceding note
    did not have an "=" associated with it, the extender won't have been drawn.
    We must cope with this case too. */

    while (bar_cont->uolay != NULL)
      {
      fontinststr fdata;
      uolaystr *u = bar_cont->uolay;
      BOOL contflag = FALSE;
      int32_t xx = u->x;
      int32_t yy = u->y;

      fdata = u->above?
        curmovt->fontsizes->fontsize_text[ff_offset_olay] :
        curmovt->fontsizes->fontsize_text[ff_offset_ulay];
      fdata.size = mac_muldiv(fdata.size, out_stavemagn, 1000);

      if (xx == 0)  /* A complete system of hyphens or extender */
        {
        xx = out_sysblock->firstnoteposition + out_sysblock->xjustify - 4000;
        yy = u->above?
          out_sysblock->olevel[curstave] :
          out_sysblock->ulevel[curstave];
        contflag = TRUE;
        }

      /* Remove this block from the chain */

      bar_cont->uolay = u->next;

      /* Deal with row of hyphens */

      if (u->type == '-')
        {
        int32_t xend = out_barlinex - 2000;
        if (xend - xx < 800) xend += 1000;
        if (xend - xx < 800) xend = xx + 801;  /* In case xx < 0 */
        if (u->htype == 0)
          out_hyphens(xx, xend, yy, &fdata, contflag);
        else
          out_repeatstring(xx, xend, yy, contflag, TRUE, u->htype);
        }

      /* Deal with extender line */

      else if (n_pitch != 0)
        {
        BOOL extender_continues = FALSE;
        int32_t xend;

        /* See if the syllable continues on to the next system; we assume it
        does if we find a note before any text; or if we find an underlay or
        overlay string beginning with "=". */

        if (curbarnumber + 1 < curmovt->barcount)
          {
          for (bstr *pp = (bstr *)(ss->barindex[curbarnumber + 1]);
               pp != NULL;
               pp = pp->next)
            {
            /* Note or chord => extender continues (unless rest) */

            if (pp->type == b_note)
              {
              extender_continues = ((b_notestr *)pp)->spitch != 0;
              break;
              }

            /* Underlay or overlay text (as appropriate) before any notes means
            the syllable does not continue, unless it is "=". */

            else if (pp->type == b_text)
              {
              b_textstr *t = (b_textstr *)pp;
              if ((t->flags & text_ul) != 0 &&
                  ((t->flags & text_above) != 0) == u->above)
                {
                extender_continues = PCHAR(t->string[0]) == '=';
                break;
                }
              }
            }
          }

        /* End depends on whether continuing or not */

        xend = extender_continues?
          out_barlinex - 4000 : n_x + 5*out_stavemagn;
        if (xend - xx > 4000) out_extension(xx, xend, yy, &fdata);
        }

      mem_free_cached((void **)(&main_freeuolayblocks), u);
      }
    }

  /* --- End of processing continued items --- */


  /* First of all, we must extend ybarend if it is not extended and the option
  to have full barlines at the end of each system is set. */

  if (out_lineendflag && out_ybarend == out_ystave && MFLAG(mf_fullbarend) &&
    nextstave > 0) out_ybarend = out_ybarenddeep;

  /* If we have just printed a multi-bar rest, we must use the appropriate kind
  of bar line for the *last* bar. This requires a scan of the last bar.

  The last bar is also permitted to contain a right-hand repeat mark. If this
  is found, we output it at the barline position, as we know there can be no
  notes in the bar.

  The last bar is also permitted to contain a clef at the end. */

  if (out_manyrest > 1)
    {
    for (bstr *pp = (bstr *)(ss->barindex[curbarnumber + out_manyrest - 1]);
         pp != NULL;
         pp = pp->next)
      {
      if (pp->type == b_barline)
        {
        barlinestyle = ((b_barlinestr *)pp)->barstyle;
        barlinetype = ((b_barlinestr *)pp)->bartype;
        }

      else if (pp->type == b_rrepeat)
        {
        out_lastbarwide = TRUE;
        out_writerepeat(out_barlinex -
          ((out_lineendflag? 68 : 50)*out_barlinemagn)/10, rep_right,
            out_barlinemagn);
        endrepeat = TRUE;
        }

      else if (pp->type == b_clef)
        {
        out_setother(pp);
        }
      }
    }

  /* Now we can output appropriate bits of bar line, except on stave 0. */

  if (curstave != 0)
    {
    BOOL finalbar = curbarnumber + out_manyrest >= curmovt->barcount;

    int32_t ytop = out_ystave +
      ((barlinestyle == 2 || barlinestyle == 3)? 16*out_stavemagn :
      (ss->stavelines == 6)? - 4*out_stavemagn : 0);
    int32_t ybarstart = ytop;

    /* Select the type of bar line. Need to do this first so the result can be
    used in the next test. */

    int barchar =
      (barlinetype == barline_double)? bar_double :
      (barlinetype == barline_ending ||
        (finalbar && !MFLAG(mf_unfinished)))? bar_thick :
      (barlinestyle == 1 || barlinestyle == 3)? bar_dotted : bar_single;

    /* If the bar finished with a right-hand repeat mark, in certain cases we
    can omit overprinting with a normal bar line. */

    if (endrepeat && (curmovt->repeatstyle == 0 || curmovt->repeatstyle == 4 ||
        (!finalbar && curmovt->repeatstyle != 3)))
      barlinetype = barline_invisible;

    /* Do not print an ending barline if set invisible, or for [omitempty] bars
    containing no data. */

    if (barlinetype != barline_invisible &&
         (!ss->omitempty || !mac_emptybar(ss->barindex[curbarnumber])) &&
         out_ybarend >= ybarstart)
      {

      /* Flag wide bar line for next bar */

      if (barchar == bar_double) out_lastbarwide = TRUE;

      /* Output an end-of-movement double thin/thick line. */

      if (barchar == bar_thick)
        {
        int32_t gap = mac_muldiv(3500, out_barlinemagn, 1000);
        ps_barline(out_barlinex - (3*out_barlinemagn)/2, ybarstart, out_ybarend,
          bar_thick, out_barlinemagn);
        ps_barline(out_barlinex - gap, ybarstart, out_ybarend, bar_single,
          out_barlinemagn);
        }

      /* Not a thick/thin pair */

      else
        {
        int32_t x = (barchar == bar_double)? 2*out_barlinemagn : 0;

        /* Bar lines that are on and/or between staves; a double bar overrides
        the style. */

        if (barlinestyle < 4 || barchar == bar_double)
          {
          ps_barline(out_barlinex - x, ybarstart, out_ybarend, barchar,
            out_barlinemagn);
          }

        /* Deal with the special bar line styles that involve markings only on
        the current stave. */

        else
          {
          ps_musstring((barlinestyle == 4)?
            US"~x\211yyyyyyx\211" : US"|\211yyyyyyxxxxx\211",
            10*out_stavemagn, out_barlinex, out_ystave);
          }
        }
      }

    /* If we have just output a non-empty bar on an [omitempty] stave, there
    are two more things to do. (1) If this is not the first bar in the system
    and the previous bar was empty, output a bit of bar line at the start of
    the bar, taking the type and style from the previous bar. As the style may
    be different from the bar line at the end of the bar (handled just above),
    we have to compute everything again. (2) Output stave lines for this
    non-empty bar. */

    if (ss->omitempty && !mac_emptybar(ss->barindex[curbarnumber]))
      {
      barstr *bs;
      int32_t x = 0;

      if (!out_startlinebar && mac_emptybar((bs = ss->barindex[curbarnumber-1])))
        {
        b_barlinestr *bl = (b_barlinestr *)(bs->next);
        int type = bl->bartype;

        if (type != barline_invisible)
          {
          int style = bl->barstyle;

          barchar =
            (type == barline_double)? bar_double :
            (type == barline_ending)? bar_thick :
            (style == 1 || style == 3)? bar_dotted : bar_single;

          ytop = (style == 2 || style == 3)?
            (out_ystave + 16*out_stavemagn) : out_ystave;
          ybarstart = ytop;

          if (out_ybarend >= ybarstart)
            {
            if (style < 4 || barchar == bar_double)
              {
              if (barchar == bar_double) x = 2*out_stavemagn;
              ps_barline(out_lastbarlinex - x, ybarstart, out_ybarend, barchar,
                out_barlinemagn);
              }
            else
              {
              ps_musstring((style == 4)?
                US"~x\211yyyyyyx\211" : US"|\211yyyyyyxxxxx\211",
                10*out_stavemagn, out_lastbarlinex, out_ystave);
              }
            }
          }
        }

      /* Generate a bit of stave for this bar at this point. For ordinary
      staves that do not have [omitempty] set, stave lines are printed for
      complete systems. */

      if (ss->stavelines > 0)
        {
        ps_stave((!out_startlinebar)? out_lastbarlinex - x :
          out_sysblock->startxposition + out_sysblock->xjustify,
            out_ystave, out_barlinex, ss->stavelines);
        }
      }
    }

  /* When we reach stave zero, we have to see which systems, if any, it is to
  be printed over. It can get printed several times, with vertical adjustments.
  The checkzerocopy() function sets this up, adjusting the value of curstave
  if necessary, to keep this loop repeating. */

  if (curstave <= 1) checkzerocopy(zerocopycount);
  }  /* End of per-stave loop */

/* Restore the contstr pointer that might have been changed when printing
copies of stave zero. */

wk_cont = wk_cont_save;

/* Output the bar number if required, above the top stave. Bar 1 never
gets a bar number, and [nocount] bars only do so if forced by [barnumber]. The
curmovt->barnumber_interval value is negative if numbers at the start of lines
are required, greater than zero for numbering every nth bar, or zero for no
automatic bar numbering. In any individual bar, [barnumber] can force a bar
number or no bar number. */

bn = curmovt->barvector[curbarnumber];

if (((bn & 0xffffu) == 0 ||         /* No fraction */
    bp->barnoforce == bnf_yes) &&   /* Or forced bar number */
    bn > 0x10000u &&                /* Greater than bar 1 */
    bp->barnoforce != bnf_no &&     /* Not disabled */
     (
       (curmovt->barnumber_interval > 0 &&
         (bn >> 16)  % curmovt->barnumber_interval == 0) ||
       (curmovt->barnumber_interval < 0 && out_startlinebar) ||
       bp->barnoforce == bnf_yes
     ))
  {
  uschar s[24];
  size_t n;
  int32_t x = bp->barnoX;
  int32_t y = 24000;

  /* Adjust position for start and non-start bars. Note that bar_cont will be
  set as for stave 0. */

  if (out_startlinebar && top_bar_cont != NULL)
    {
    x += out_sysblock->startxposition + out_sysblock->xjustify;
    if (top_bar_cont->clef == clef_trebledescant) x += 15000;
      else if (top_bar_cont->clef == clef_soprabass) x += 9000;
    }
  else
    {
    b_notestr *next;
    if (lastp == NULL) next = NULL; else mac_nextnote(next, lastp);
    if (next != NULL && next->spitch > P_5L) y += (next->spitch - P_5L)*500;
    x += out_lastbarlinex;
    }

  /* Format the bar number and output it at the appropriate position, using
  string_pmw() to convert to a PMW string in the barnumber font. */

  n = sprintf(CS s, "%d", bn >> 16);
  bn &= 0xffffu;
  if (bn != 0) sprintf(CS (s+n), ".%d", bn);

  y = (y + bp->barnoY)*barno_magn/1000 + curmovt->barnumber_level;
  out_string(string_pmw(s, curmovt->fonttype_barnumber),
    &(curmovt->fontsizes->fontsize_barnumber), x, out_yposition - y,
    curmovt->barnumber_textflags);
  }

/* Remember the position of the last barline, and return the number of the
following bar. */

out_lastbarlinex = out_barlinex;
TRACE("out_setbar() end\n");
return curbarnumber + out_manyrest;
}

/* End of setbar.c */
