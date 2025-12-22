/*************************************************
*    PMW code for setting tie/gliss/short slur   *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: July 2021 */
/* This file last modified: December 2025 */


#include "pmw.h"


/* This file contains code for drawing ties (short slurs) and glissandos.
Different actions are required for single notes and for chords. It is simplest
to separate these out into entirely separate functions. When computing vertical
positions in units of pitch, fractions of the "tone" value P_T may be used. */



/************************************************
*        Output a tie (or short slur)           *
************************************************/

/* This function is called to draw a tie between two single notes. They need
not have the same pitch, in which case it is really a slur. The variable
n_prevtie points to the tie block for the first note, and n_nexttie points to
the ongoing tie block, if any.

Arguments:
  x1          the x coordinate of the end of the tie/slur
  endline     TRUE if at end of line
  flags       tie flags specifying type of tie (editorial, dashed, dotted)

Returns:      nothing
*/

void
out_setnotetie(int32_t x1, BOOL endline, uint8_t flags)
{
b_notestr *left = n_prevtie->noteprev;
BOOL startline = FALSE;
BOOL above = n_prevtie->abovecount > 0;
BOOL leftup = out_laststemup[curstave];
uint32_t slurflags;
uint32_t accinside;
int32_t adjustL = 0;
int32_t adjustR = 0;
int32_t dstart = 0;
int32_t dstop = 1000;
int32_t co = 0;
int32_t x0 = bar_cont->tiex;
int32_t y0, y1;
int tietype;
int p0 = left->spitch;
int p1 = n_pitch;
int pp0, pp1;

/* If this note is further tied, arrange to leave a bit of a gap in the tie
marks. */

int32_t jointiegap =
  (n_nexttie != NULL && n_prevtie->abovecount == n_nexttie->abovecount)?
    out_stavemagn : 0;

/* Check for coupling of first note */

mac_couplepitch(p0, left->flags);

/* If the final pitch is zero, make the tie horizontal. This can arise at the
end of a line if a tie continues over a totally empty bar. */

if (p1 == 0) p1 = p0;

/* Set slur flags and initialize tie type; raise the pitch when the tie is
above. */

if (above)
  {
  slurflags = tietype = 0;
  p0 += 4*P_T;
  p1 += 4*P_T;
  }
else
  {
  slurflags = sflag_b;
  tietype = 1;
  }

if ((flags & tief_editorial) != 0) slurflags |= sflag_e;
if ((flags & tief_dashed) != 0) slurflags |= sflag_i;
if ((flags & tief_dotted) != 0) slurflags |= sflag_i | sflag_idot;

/* Save basic levels for accidental checking */

pp0 = p0;
pp1 = p1;

/* Compute the rest of the "tie type" value. There are eight possibilities:

     0  stems up, tie above           1  stems up, tie below
     2  stems down, tie above         3  stems down, tie below
     4  first stem up, tie above      5  first stem up, tie below
     6  first stem down, tie above    7  first stem down, tie below

This value is used for checking on stem crossings, etc. Continuation ties can
only be types 0-3. Also, certain checks on the left end are skipped for
continued ties. Remember the fact for style handling. */

if (x0 == 0)
  {
  startline = TRUE;
  if (!n_upflag) tietype += 2;
  x0 = out_barx - 10*out_stavemagn;
  p0 = pp0 = p1;
  }

/* A non-continued tie */

else
  {
  uint32_t acflags = left->acflags;

  if (leftup != n_upflag) tietype += 4;
  if (!leftup) tietype += 2;

  /* Check for stem crossing at the left-hand end */

  if (left->notetype >= minim &&
    (tietype == 0 || tietype == 4)) adjustL += 5*out_stavemagn;

  /* Check for dotted note at the left-hand end */

  if (above && leftup && left->dots != 0)
    {
    adjustL += 3*out_stavemagn;
    if (left->dots != 255)
      for (int i = 1; i < left->dots; i++) adjustL += (35*out_stavemagn)/10;
    }

  /* Check for staccato etc. on the left-hand note. Allow for one other accent
  with a tenuto (does sometimes happen). */

  accinside = acflags & af_accinside;
  if ((tietype == 1 || tietype == 2 || tietype == 5 || tietype == 6) &&
    accinside != 0 && (acflags & af_opposite) == 0)
      {
      int z = 3*P_T - (p0 & P_M);
      int zz = ((acflags & af_ring) == 0)? P_T : 3*P_T/2;
      int extra = ((accinside & af_bar) != 0 && (accinside - af_bar) != 0)?
        P_T : 0;

      if (leftup)
        {
        if (p0 <= P_2L) extra = 2*extra;
        p0 -= extra + ((p0 == P_2L)? 2*P_T : (p0 <= P_1S)? zz : z);
        }
      else
        {
        if (p0 >= P_4L) extra = 2*extra;
        p0 += extra + ((p0 == P_6L)? 2*P_T : (p0 >= P_6S)? zz : z);
        }
      }
  }

/* Check on stem crossing at the right-hand end, unless this is an invisible
note. */

if (n_notetype >= minim && (tietype == 3 || tietype == 5) &&
  (bar_cont->flags & cf_notes) != 0)
    adjustR -= 4*out_stavemagn - jointiegap;

/* Check for staccato etc. at the right-hand end (of a slur, not a tie, of
course). */

accinside = n_acflags & af_accinside;
if ((tietype == 1 || tietype == 2 || tietype == 4 || tietype == 7) &&
  accinside != 0 && (n_acflags & af_opposite) == 0)
    {
    int z = 3*P_T - (p1 & P_M);
    int zz = ((n_acflags & af_ring) == 0)? P_T : 3*P_T/2;
    int extra = ((accinside & af_bar) != 0 && (accinside - af_bar) != 0)?
      P_T : 0;

    if (n_upflag)
      {
      if (p1 <= P_2L) extra = 2*extra;
      p1 -= extra + ((p1 == P_2L)? 2*P_T : (p1 <= P_1S)? zz : z);
      }
    else
      {
      if (p1 >= P_4L) extra = 2*extra;
      p1 += extra + ((p1 == P_6L)? 2*P_T : (p1 >= P_6S)? zz : z);
      }
    }

/* Check for enough space if the final note has an accidental. Then ensure that
ties do not start or end on or near stave lines. Masking with P_T instead of
P_M implements "on or near". */

if (above)
  {
  if (!endline && (n_flags & nf_accinvis) == 0 && n_acc != ac_no &&
      n_acc != ac_ds && p1 <= pp1)
    {
    p1 += P_T/2;
    if (x1 - x0 <= 16000 && pp1 > pp0) { p1 += P_T/2; p0 += P_T/2; }
    }

  if (p0 <= P_6L && (p0 & P_T) == 0) p0 += P_T/2;
  if (p1 <= P_6L && (p1 & P_T) == 0) p1 += P_T/2;
  }

else
  {
  if (!endline && (n_flags & nf_accinvis) == 0 && n_acc != ac_no &&
      n_acc <= ac_sh && p1 >= pp1)
    {
    p1 -= 3*P_T/2;
    if (x1 - x0 < 16000 && pp1 < pp0) co += 1000;
    }

  if (p0 >= P_2L && (p0 & P_T) == 0) p0 -= P_T/2;
  if (p1 >= P_2L && (p1 & P_T) == 0) p1 -= P_T/2;
  }

/* If the slur is very steep, make it a bit more curvy, and adjust the right
hand end in some cases. Otherwise, for very short slurs, make flatter. */

if (abs(p0 - p1) > 5*P_T) co += 2000;
  else if (x1 - x0 > 10000) co -= 1000;

/* If this is really a tie (the two notes have the same pitch) then it should
be horizontal. One end may have been moved to avoid accents, etc. If this is
the case, we adjust the other end to keep the tie horizontal. Also, for the
benefit of XML output, set a flag on both notes. */

if (n_pitch == left->spitch)
  {
  if ((above && p0 > p1) || (!above && p0 < p1)) p1 = p0; else p0 = p1;
  left->flags |= nf_wastied;
  n_lastnote->flags |= nf_wastied;
  }

/* If this is really a slur, make sure that we haven't negated its sense by
moving one end to account for accents. If we have, make it at least horizontal.
Which end to move depends on whether the tie is above or below. */

else if (n_pitch > left->spitch)
  {
  if (p1 < p0)
    {
    if ((tietype & 1) == 0) p1 = p0; else p0 = p1;
    }
  }
else
  {
  if (p1 > p0)
    {
    if ((tietype & 1) == 0) p0 = p1; else p1 = p0;
    }
  }

/* When printing right-to-left, certain ties need some horizontal adjustment to
cope with stem positions. */

if (main_righttoleft) switch (tietype)
  {
  case 0:
  adjustL -= 4*out_stavemagn;
  adjustR -= 4*out_stavemagn;
  break;

  case 3:
  adjustL += 4*out_stavemagn;
  adjustR += 4*out_stavemagn;
  break;

  case 5:
  adjustR += 4*out_stavemagn;
  break;

  case 6:
  adjustR -= 4*out_stavemagn;
  break;

  case 7:
  adjustL += 4*out_stavemagn;
  break;

  default:
  break;
  }

/* Set up the final coordinates, taking into account the style of continued
ties, and then output it. */

x0 += adjustL;
y0 = (p0 - P_2L)*out_pitchmagn;
x1 += adjustR - jointiegap;
y1 = (p1 - P_2L)*out_pitchmagn;

if (endline && curmovt->endlinetiestyle != 0)
  {
  x1 += x1 - x0;
  y1 += y1 - y0;
  dstop = 500;
  }
else if (startline && curmovt->endlinetiestyle != 0)
  {
  x0 -= x1 - x0;
  y0 -= y1 - y0;
  dstart = 500;
  }

out_slur(x0, y0, x1, y1, slurflags, co, dstart, dstop);
}



/************************************************
*          Output ties on a chord               *
************************************************/

/* This function is called when the right-hand chord has just been output. The
variable tiecount is set to the number of ties which are in the "abnormal"
direction for the stem direction. A zero value for x0 means we are continuing
at the start of a line.

Arguments:
  right       pointer to the first note of the right-hand chord
  notecount   number of notes in the chord
  x1          the x coordinate of the end of the ties
  endflag     TRUE if drawing to end of line
  tieflags    type of tie

Returns:      nothing
*/

void
out_setchordtie(b_notestr *right, int notecount, int32_t x1, BOOL endflag,
   uint8_t tieflags)
{
b_notestr *leftbase, *left;
int tiecount = n_upflag? n_prevtie->abovecount : n_prevtie->belowcount;
int leftcount = 0;
uint16_t slurflags = 0;
uint32_t acflags = 0;
uint32_t flags = 0;
uint32_t dots = 0;
int32_t x0 = bar_cont->tiex;
BOOL leftup = out_laststemup[curstave];
BOOL continued;

/* If this chord is further tied, arrange to leave a bit of a gap in the tie
marks. This value gets scaled to the stave magnification later. */

int32_t jointiegap =
  (n_nexttie != NULL && n_prevtie->abovecount == n_nexttie->abovecount)?
    1000 : 0;

/* Translate tie flags into slur flags. */

if ((tieflags & tief_editorial) != 0) slurflags |= sflag_e;
if ((tieflags & tief_dashed) != 0) slurflags |= sflag_i;
if ((tieflags & tief_dotted) != 0) slurflags |= sflag_i | sflag_idot;

/* Find the first note of the lefthand chord. The chord may have been sorted
since its first note was remembered. */

leftbase = n_prevtie->noteprev;
while (leftbase->type == b_chord) leftbase = (b_notestr *)(leftbase->prev);

/* Collect all the flags from the left-hand chord, for staccato etc. We have to
do this because the notes may be in either order. */

left = leftbase;
dots = left->dots;
do
  {
  leftcount++;
  flags |= left->flags;
  acflags |= left->acflags;
  left = (b_notestr *)(left->next);
  }
while (left->type == b_chord);

/* Set start position for continuations at start of line */

if (x0 == 0)
  {
  x0 = out_barx - 10*out_stavemagn;
  continued = TRUE;
  }
else continued = FALSE;

/* Process each note in the chord. For each one, scan the left-hand chord, and
tie only those notes that correspond in pitch. Each time round the loop adjust
the count of those on the "abnormal" side, whether or not we actually printed a
tie. */

for (int count = 0;
     count < notecount;
     right = (b_notestr *)(right->next), count++, tiecount--)
  {
  int i, p0, type;
  int intervalN, intervalP;
  BOOL below;
  int32_t y0, xx0, xx1;
  int32_t adjustL, adjustR;
  int32_t dstart, dstop;

  /* Scan the left-hand chord, looking for a note of the same pitch. */

  left = leftbase;
  for (i = 0; i < leftcount; i++)
    {
    if (left->abspitch == right->abspitch) break;
    left = (b_notestr *)(left->next);
    }
  if (i >= leftcount) continue;   /* No matching note found */

  /* Notes match; for the benefit of XML output, set a flag on both notes, the
  output a tie. */

  left->flags |= nf_wastied;
  right->flags |= nf_wastied;

  below = (tiecount > 0 && !n_upflag) || (tiecount <= 0 && n_upflag);
  p0 = right->spitch;
  adjustL = 0;
  adjustR = 0;
  dstart = 0;
  dstop = 1000;

  /* Compute intervals to the next and previous notes */

  intervalN = (count == notecount - 1)? 0 :
    right->spitch - ((b_notestr *)(right->next))->spitch;
  intervalP = (count == 0)? 0 :
    right->spitch - ((b_notestr *)(right->prev))->spitch;

  /* Adjust pitch for coupling */

  mac_couplepitch(p0, right->flags);
  if (!below) p0 += 4*P_T;

  /* For each tie, we must determine whether it is inside or outside the
  chord. A tie is outside if it is EITHER

    (a) the last one, and in the "normal" direction, unless the stems
        are in opposite directions and the left one is up, in which
        case it is "half outside"

    (b) the first one, and in the abnormal direction, AND
        EITHER the first note has no stem
          OR the first note's stem is down or we are at a line start
        AND
        EITHER the second note has no stem
          OR the stem is up
          OR we are at a line end (for which "no stem" is in fact set)

  There are two cases where notes are "half outside", which occur when case
  (b) is true except that a note required not to have a stem does in fact
  have one. We encode the cases in the variable "type" as follows:

    0 => outside
    1 => inside
    2 => right inside
    3 => left inside

  because otherwise the code is tortuous and repetitious.

  Another "half outside" case has come to light because an end note of
  the right-hand chord may match a middle note of the left-hand chord. */

  /* Handle tie in normal direction for the last note */

  if ((count == notecount - 1 && tiecount <= 0))
    {
    type = (leftup == n_upflag || !leftup || (flags & nf_stem) == 0)? 0 : 3;

    /* Deal with non-end left-hand note */

    if (type == 0 && leftup && left->next->type == b_chord) type = 3;
    }

  /* Handle tie in abnormal direction for the first note */

  else if (count == 0 && tiecount > 0)
    {
    if ((flags & nf_stem) == 0 || !leftup || continued)
      {
      type = ((n_flags & nf_stem) == 0 || n_upflag)? 0 : 2;
      if (type == 0 && !endflag && intervalN == -4) type = 2;
      }
    else type = !n_upflag? 2 : 3;
    }

  /* All other cases are inside */

  else type = 1;

  /* When printing right-to-left, things change! */

  if (main_righttoleft) switch (type)
    {
    case 0:
    if (n_upflag && !leftup) type = below? 3: 2;
    break;

    case 2:
    if (!n_upflag) type = leftup? 0 : 3;
    break;

    case 3:
    if (leftup) type = n_upflag? 2 : 0;
    break;

    default:
    break;
    }

  /* Now make adjustments according to the type */

  switch (type)
    {
    case 0:                   /* outside */
    case 2:                   /* right inside */
    adjustR = (type == 0)? -jointiegap : -3750;

    /* Check for accents on the first chord and adjust the position if
    necessary. (We assume no accents on second chord). */

    if ((count == 0 && leftup != n_upflag) ||
        (count == notecount - 1 && leftup == n_upflag))
      {
      if ((acflags & af_accinside) != 0 && (acflags & af_opposite) == 0)
        {
        int z = 3*P_T - (p0 & P_M);
        if (leftup) p0 -= (p0 <= P_1L)? 2*P_T : z;
          else p0 += (p0 >= P_7L)? 2*P_T : z;
        }
      }
    break;

    case 1:                   /* inside */
    case 3:                   /* left inside */
    if (!continued)
      {
      adjustL = 4500;
      if (dots == 255) adjustL += 8000;
      else if (dots != 0)
        {
        adjustL += 4000;
        for (usint ii = 1; ii < dots; ii++) adjustL += 3500;
        }
      }

    if (type == 3)
      {
      adjustR = -jointiegap;
      break;       /* that's all for left inside */
      }

    if (!endflag) adjustR = -3750;

    /* Deal with dots moved right (will apply only to stems up) */

    if (!continued && dots != 0 && (flags & nf_dotright) != 0)
      adjustL += 5500;

    /* Else deal with intervals of a second */

    else if (n_upflag)
      {
      if (!continued && intervalN == 4) adjustL += 5500;
      }
    else
      {
      if (!endflag && (intervalN == -4 || (tiecount > 0 && intervalP == 4)))
        adjustR -= 5500;
      }

    /* Inside ties are adjusted for pitch */

    p0 += below? 3*P_T/2 : -3*P_T/2;
    break;
    }

  /* If notes are currently switched off, we have just printed an invisible
  note. Cancel any horizontal adjustment. This fudge is a cunning way of
  printing hanging ties. */

  if ((bar_cont->flags & cf_notes) == 0) adjustR = 0;

  /* Correct adjustments for stave magnification */

  adjustL = (adjustL*out_stavemagn)/1000;
  adjustR = (adjustR*out_stavemagn)/1000;

  /* Now make sure the tie does not end on a staff line. */

  if ((p0 & P_M) == 0)
    {
    if (below)
      {
      if (p0 >= P_1L) p0 -= P_T/2;
      }
    else if (p0 <= P_5L) p0 += P_T/2;
    }

  /* Convert from a pitch value to a points offset. */

  y0 = (p0 - P_2L)*out_pitchmagn;

  /* Set up the final x coordinates, taking into account the style of continued
  ties, and then output it. */

  xx0 = x0 + adjustL;
  xx1 = x1 + adjustR;

  if (endflag && curmovt->endlinetiestyle != 0)
    {
    xx1 += xx1 - xx0;
    dstop = 500;
    }
  else if (continued && curmovt->endlinetiestyle != 0)
    {
    xx0 -= xx1 - xx0;
    dstart = 500;
    }

  out_slur(xx0, y0, xx1, y0,
    (below? sflag_b : 0) | slurflags,
      (xx1 - xx0 > 10000)? (-out_stavemagn) : 0, dstart, dstop);
  }
}



/*************************************************
*               Output a glissando mark          *
*************************************************/

/* The positions of the ends of the line are adjusted according to whether the
notes are on lines or spaces, and whether or not the right hand one has
accidentals.

Arguments:
  x1          the x coordinate of the end of the mark
  flags       type of line required (editorial, dashed, dotted)

Returns:      nothing
*/

void
out_glissando(int32_t x1, uint8_t flags)
{
b_notestr *left = n_prevtie->noteprev;

int32_t x0 = bar_cont->tiex;
int p0 = left->spitch;
int p1 = n_pitch;

/* Check for coupling of first note */

mac_couplepitch(p0, left->flags);

if (x0 == 0)    /* at start of line */
  {
  x0 = out_barx - 6*out_stavemagn;
  p0 = (p0 + p1)/2;
  }
else x0 += 8*out_stavemagn;

if (left->dots != 0)
  {
  x0 += 4*out_stavemagn;
  if (left->dots != 255)
    for (int i = 1; i < left->dots; i++) x0 += (35*out_stavemagn)/10;
  }

x1 -= (15*out_stavemagn)/10 + n_accleft;

if ((p0 & P_M) == 0) p0 += (p1 > p0)? (+P_T) : (-P_T);
if ((p1 & P_M) == 0) p1 += (p1 > p0)? (-P_T) : (+P_T);

ofi_line(x0, (p0 - P_1L)*out_pitchmagn,
  x1, (p1 - P_1L)*out_pitchmagn, (3*out_stavemagn)/10, flags);
}

/* End of settie.c */
