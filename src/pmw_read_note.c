/*************************************************
*        PMW native input reading functions     *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: March 2021 */
/* This file last modified: July 2025 */

/* This file contains the code for reading one note in PMW notation. */

#include "pmw.h"

/* This structure is used when positioning accidentals on chords. */

typedef struct {
   int32_t accleft;
   int32_t orig_accleft;
  uint16_t pitch;
  uint8_t  acc;
  CBOOL    inverted;
} accstr;

/* Pitch differences that are necessary for a tuck-in offset when positioning
accidentals on chords. This table is indexed by accidental number. A value of
100 is used for "never". */

static uint16_t tuckoffset[] = {
/* no  nt   hs   sh  ds  hf  fl  df */
  100, 16, 100, 100, 12, 12, 12, 12,
  100, 12, 100, 100, 12, 12, 12, 12 };  /* When bottom is a flat */

/* A number of static variables are used for communication between read_note()
and post_note(). */

static uint32_t pn_inchord;  /* Non-zero if a chord is read */
static BOOL pn_seconds;      /* TRUE if chord contains a seconds interval */
static int pn_stemforce;     /* 0 = none, > 0 = up, < 0 = down */
static int pn_notetype;      /* Can become negative if too many '+' (diagnosed) */

static uint32_t pn_notelength;
static uint16_t pn_maxpitch, pn_minpitch, pn_chordcount;
static tiedata pn_tiedata[MAX_CHORDSIZE];

/* This variable is set true when the only notes in a bar so far are notated
using "p" and "x" and are tied to their predecessors. It is used to determine
whether a following "p" or "x" should have its accidentals printed or not. */

static BOOL pn_onlytieddup;



/*************************************************
*        Read pitch for [printpitch]             *
*************************************************/

/* This function reads a stave-relative pitch for use by the [printpitch]
directive. It is placed here because it uses the tables in this module. The
character is already current on entry.

Arguments:  none
Returns:    the pitch
*/

uint16_t
read_stavepitch(void)
{
uint16_t pitch;
read_c = tolower(read_c);
if ('a' > read_c || read_c > 'g')
  {
  error(ERR8, "note letter");
  read_c = 'a';
  }
pitch = MIDDLE_C - OCTAVE + read_basicpitch[read_c - 'a'] + srs.octave;
read_nextc();
while (read_c == '\'') { pitch += OCTAVE; read_nextc(); }
while (read_c == '`' ) { pitch -= OCTAVE; read_nextc(); }
return pitch_stave[pitch] + pitch_clef[srs.clef];
}



/*************************************************
*             Read optional accidental           *
*************************************************/

static uint8_t
read_accidental(void)
{
uint8_t accid;
if (read_c == '#')
  {
  read_nextc();
  accid = (read_c == '#')? ac_ds : (read_c == '-')? ac_hs : ac_sh;
  if (accid != ac_sh) read_nextc();
  }
else if (read_c == '$')
  {
  read_nextc();
  accid = (read_c == '$')? ac_df : (read_c == '-')? ac_hf : ac_fl;
  if (accid != ac_fl) read_nextc();
  }
else if (read_c == '%')
  {
  read_nextc();
  accid = ac_nt;
  }
else accid = ac_no;
return accid;
}



/*************************************************
*   Read optional move or bracket for ornament   *
*************************************************/

/* This is also used for dynamics (accents).

Arguments:
  ax        address of the x movement, or NULL if not allowed
  ay        address of the y movement
  af        address of the bflags field, or NULL if not allowed

Returns:    nothing
*/

static void
ornopts(int32_t *ax, int32_t *ay, uint8_t *af)
{
*ay = 0;
if (ax != NULL) *ax = 0;
if (af != NULL) *af = 0;
while (read_c == '/')
  {
  read_nextc();
  switch (read_c)
    {
    case 'u': *ay += read_movevalue(); break;
    case 'd': *ay -= read_movevalue(); break;

    case 'l':
    case 'r':
    if (ax == NULL)
      error_skip(ERR8, '\\', "/u or /d");
    else
      *ax += ((read_c == 'r')?(+1):(-1)) * read_movevalue();
    break;

    default:
    if (af == NULL)
      error_skip(ERR8, '\\', "/u, /d, /l, or /r");
    else switch (read_c)
      {
      case 'b': *af = (*af & ~(orn_sbra|orn_sket)) | (orn_rbra|orn_rket); break;
      case 'B': *af = (*af & ~(orn_rbra|orn_rket)) | (orn_sbra|orn_sket); break;
      case '(': *af = (*af & ~orn_sbra) | orn_rbra; break;
      case '[': *af = (*af & ~orn_rbra) | orn_sbra; break;
      case ')': *af = (*af & ~orn_sket) | orn_rket; break;
      case ']': *af = (*af & ~orn_rket) | orn_sket; break;

      default:
      error_skip(ERR8, '\\', "/u, /d, /l, /r, /b, /B, /(, /), /[, or /]");
      break;
      }
    if (read_c != '\\') read_nextc();  /* \ means there was an error */
    break;
    }
  }
}



/*************************************************
*    Set up underlay/overlay text for one note   *
*************************************************/

/* This function processes underlay or overlay strings that have been saved up,
inserting an appropriate b_text item into the data, with text(s) for one note.

Arguments:  pointer to base of saved texts chain
Returns:    nothing
*/

static void
do_undoverlay(b_textstr **pp)
{
b_textstr *p = *pp;

/* If there has been a [reset] and we haven't yet caught up, do nothing. */

if (brs.barlength <= brs.maxbarlength) return;

/* Loop for each "verse" */

while (p != NULL)
  {
  uint32_t *s = p->string;
  b_textstr *q = mem_duplicate_item(p, sizeof(b_textstr));

  p->halfway = 0;
  p->offset = 0;

  /* If we are at an equals sign, just output the one equals character;
  otherwise search for the end of the syllable. */

  if (PCHAR(*s) == '=') s++;
    else while(Ustrchr("- =", PCHAR(*s)) == NULL) s++;

  /* Set string count - don't include a minus sign, but skip over it */

  q->laylen = s - p->string;
  if (PCHAR(*s) == '-') s++;

  /* Skip spaces between syllables */

  while (PCHAR(*s) == ' ') s++;

  /* Advance to next verse, taking the current control block off the list if
  we've reached the end of the string. */

  if (*s == 0)
    {
    *pp = (b_textstr *)p->next;
    }
  else
    {
    p->string = s;
    pp = (b_textstr **)(&(p->next));
    }

  p = *pp;
  }
}



/*************************************************
*             Sort the notes in a chord          *
*************************************************/

/* For a chord, quite a lot of work must be done once the stem direction is
known. We must sort the notes into the correct order, so that the first one is
the one that gets the stems, we must arrange for certain notes to be printed
on the "wrong" side of the stem, and we must arrange the positioning of any
accidentals. If we are sorting the last chord that has been read, we must sort
stave_tiedata along with it. We must NOT do this when sorting other chords
(those that were stacked up).

This function is global because it is called at the end of a bar when handling
beam termination. It isn't called explicitly, but from the macro
mac_setstackedstems.

Arguments:
  w             point to the first note of the chord
  upflag        0 for stem down, nf_stemup for stem up

Returns:        nothing
*/

void
read_sortchord(b_notestr *w, uint32_t upflag)
{
uint8_t masq = w->masq;                 /* Masquerade value */
uint32_t dotflags = w->flags & (nf_dot|nf_dot2|nf_plus);  /* Extension flags */
uint32_t dynamics = 0;                  /* Collected dynamics flags */
uint32_t fuq = 0;                       /* Free upstemmed quaver flag */
int p, pp;                              /* Working indices */
int increment;                          /* Working scan increment */
int sorttop = 0;                        /* End of list pointer */
usint acc_count = 0;                    /* Count of accidentals */
BOOL acc_explicit = FALSE;              /* Accidental explicitly positioned */
BOOL SecondsExist = FALSE;              /* Seconds exist in this chord */
bstr *after = NULL;                     /* Item after last note */
bstr *before = w->prev;                 /* Item before first note */
b_notestr *ww = w;                      /* Working pointer */
b_notestr *sortvec[MAX_CHORDSIZE];
tiedata sorttievec[MAX_CHORDSIZE];
tiedata *tt = read_tiedata;             /* Working tiedata pointer */

ww->type = b_chord;     /* Ensure all are flagged chord pro tem */

/* Get pointers to the notes of the chord into sortvec, in ascending order, by
using a simple insertion (there won't be many of them). At the same time, sort
the current tiedata values into a temporary vector, in the same order, in case
we need this later. Collect the dynamics and fuq flags as we go (to be put on
the sorted first note), and set the stem direction flag on each note. Also, set
the flags for extension dots the same as on the first note. They might be
different if the first note is masqueraded. Set the same masquerade on all
notes too. */

while (ww != NULL && ww->type == b_chord)
  {
  uint16_t pitch = ww->spitch;
  uint32_t flags = (ww->flags & ~(nf_dot|nf_dot2|nf_plus)) | dotflags;
  uint32_t acflags = ww->acflags;

  if (ww->acc != ac_no) acc_count++;
  if ((flags & nf_accleft) != 0) acc_explicit = TRUE;

  dynamics |= acflags & (af_accents | af_opposite);
  fuq |= flags & nf_fuq;

  /* Retain flags that are not first-note only */

  ww->flags = upflag | (flags & ~(nf_dotright | nf_invert | nf_stemup));
  ww->acflags = acflags & ~(af_accents | af_opposite);
  ww->masq = masq;

  /* Find where to insert this note's pointer. */

  for (p = 0; p < sorttop; p++) if (pitch < sortvec[p]->spitch) break;

  /* Make space, insert the pointer, and deal with the corresponding tiedata. */

  memmove(sortvec + p + 1, sortvec + p, (sorttop - p) * sizeof(b_notestr *));
  sortvec[p] = ww;

  memmove(sorttievec + p + 1, sorttievec + p, (sorttop - p) * sizeof(tiedata));
  sorttievec[p] = *tt++;

  sorttop++;
  ww = (b_notestr *)ww->next;
  }

after = (bstr *)ww;   /* This is what follows the last note. */

DEBUG(D_sortchord)
  {
  eprintf("\n");
  for (p = 0; p < sorttop; p++)
    {
    ww = sortvec[p];
    eprintf("%d %d\n", ww->acc, ww->spitch);
    }
  }

/* Scan the sorted notes to see if any of them need to be printed with their
heads on the "wrong" side of their stems. (The same logic works for stemless
notes.) At the same time, force the flags for augmentation dots for notes
forming intervals of a second. */

p = 0;
while (p < sorttop - 1)
  {
  int count = 1;
  int lowp;
  BOOL samelevel;
  b_notestr *wwA;

  ww = sortvec[p];
  wwA = sortvec[p + 1];

  /* Check for an interval of a second. It doesn't count if one note is coupled
  and the other isn't. We can also cope with two notes at the same horizontal
  level (usually these will have different accidentals). */

  if (wwA->spitch - ww->spitch > P_1S - P_1L ||
      (wwA->flags & nf_couple) != (ww->flags & nf_couple))
    {
    p++;
    continue;  /* Not a second */
    }

  /* Found a second */

  samelevel = wwA->spitch == ww->spitch;

  /* Find the number of successive seconds; if the one pair are actually at
  the same level, we can't handle any more. */

  for (pp = p + 2; pp < sorttop; pp++)
    {
    b_notestr *wwB = sortvec[pp];
    if (wwB->spitch - wwA->spitch > P_1S - P_1L ||
         (wwB->flags & nf_couple) != (wwA->flags & nf_couple))
      break;
    count++;
    wwA = wwB;
    }

  if (count > 1 && samelevel) error(ERR127);
  count = (count + 1)/2;   /* Number of pairs to consider */

  /* Process all the intervals, working up if the stem is up, and down if the
  stem is down. This ensures that the note at the end of the stem is on the
  normal side of the stem. */

  if (upflag)
    {
    increment = 2;
    lowp = p;
    }
  else
    {
    increment = -2;
    lowp = pp - 2;
    }

  /* Loop through the pairs. Either note may not exist if there are an even
  number of seconds, that is, an odd number of notes. */

  while (count-- > 0)
    {
    b_notestr *wwL = (lowp >= 0)? sortvec[lowp] : NULL;
    b_notestr *wwH = (lowp + 1 < sorttop)? sortvec[lowp + 1] : NULL;

    /* Flag higher note of a 2nd for inverting if stem up. Unless the upper
    note is already flagged for dot raising, remove any flag for dot lowering,
    and if the lower note exists flag it for dot lowering. (The forcible
    removal is for the case of re-processing after failure to print on two
    sides of a beam.) */

    if (wwH != NULL)
      {
      if (upflag) wwH->flags |= nf_invert;
      if ((wwH->flags & nf_highdot) == 0)
        {
        wwH->flags &= ~nf_lowdot;
        if (wwL != NULL) wwL->flags |= nf_lowdot;
        }
      }

    /* Flag lower note of a 2nd for inverting if stem down. Count this as an
    accidental, so as to cause accidental positioning to happen if there is
    at least one actual accidental on the chord. */

    if (wwL != NULL && !upflag)
      {
      wwL->flags |= nf_invert;
      acc_count++;
      }

    /* Advance to the next pair of notes */

    lowp += increment;
    }

  /* Note that intervals of a 2nd exist in this chord */

  SecondsExist = TRUE;

  /* Advance to check the rest of the chord */

  p = pp;
  }

/* If the stem is up and there are any seconds in the chord, flag all the notes
to print with any dots moved right. */

if (SecondsExist && upflag)
  for (p = 0; p < sorttop; p++) sortvec[p]->flags |= nf_dotright;

/* Scan the chord to arrange the positioning of the accidentals. This is done
by using a matrix of positions which are filled in as the chord is scanned from
top to bottom. We do this only if there were no explicitly positioned
accidentals anywhere in the chord and there is more than one accidental, or at
least one accidental and one inverted note. */

if (!acc_explicit && acc_count > 1)
  {
  usint state = 0;
  accstr a_matrix[MAX_CHORDSIZE];
  accstr *row = a_matrix;
  accstr *a_end;

  /* First initialize the matrix, in descending order. Copy only those notes
  that have accidentals or inverted noteheads. */

  for (p = sorttop - 1; p >= 0; p--)
    {
    uint32_t flags;

    ww = sortvec[p];
    flags = ww->flags;
    if (ww->acc == ac_no && (flags & nf_invert) == 0) continue;

    row->pitch = ww->spitch;

    if ((flags & nf_couple) != 0)
      row->pitch += ((flags & nf_coupleU) != 0)? 32 : -32;

    row->inverted = !upflag && ((flags & nf_invert) != 0);
    row->acc = ww->acc;

    row->accleft = row->orig_accleft = ww->accleft;
    row++;
    }
  a_end = row;

  DEBUG(D_sortchord)
    {
    eprintf("Initialized matrix\n");
    for (row = a_matrix; row < a_end; row++)
      {
      eprintf("%d %d %d %s\n", row->pitch, row->inverted,
        row->acc, string_format_fixed(row->accleft));
      eprintf(" %s\n", string_format_fixed(row->orig_accleft));
      }
    }

  /* Scan inverted and accidentalled notes from top to bottom and determine
  offset. This algorithm works in two states. In state 0, there is clear space
  above, while in state 1 there may be clashes. */

  for (row = a_matrix; row < a_end; row++)
    {
    BOOL shortacc = row->acc > ac_sh;    /* less tall accidental */
    accstr *nrow = row + 1;              /* pointer to next row */

    DEBUG(D_sortchord) eprintf("STATE=%d row->acc=%d\n", state, row->acc);

    /* Deal with the case when all is clear above. If there is no accidental we
    just have an inverted note. */

    /* ---- STATE = 0 ---- */

    if (state == 0)
      {
      if (row->acc != ac_no)    /* There is an accidental */
        {
        /* If note is inverted, just position the accidental to clear it.
        Otherwise, search down for the next inversion and see if it is clear.
        */

        if (row->inverted) row->accleft += 6000; else /* add for invert */
          {
          accstr *nnrow = nrow;
          while (nnrow < a_end)
            {
            if (nnrow->inverted)
              {
              if ((row->pitch - nnrow->pitch) < (shortacc? 12:16))
                {
                row->accleft += ((row->pitch - nnrow->pitch) <= 8)?
                  (shortacc? 4500 : 6000) : 4500;
                }
              break;
              }
            nnrow++;
            }
          }
        }

      /* Change to state 1 if the next note is close enough */

      if (nrow < a_end && (row->pitch - nrow->pitch) < (shortacc? 20:24))
        state = 1;
      }

    /* ---- STATE = 1 ---- */

    /* Deal with the case when not clear above. If there is no accidental we
    are at an inverted note. Accidentals above should have been positioned
    clear of it. We merely need to change state if we can. */

    else if (row->acc == ac_no)
      {
      if (nrow < a_end && (row->pitch - nrow->pitch) >= 20) state = 0;
      }

    /* There is an accidental -- we have to scan up and move it clear of
    previous accidentals where necessary. There will always be at least one
    previous row, as we can't get into state 1 when row is pointing to
    a_matrix. */

    else
      {
      BOOL OK = FALSE;
      int32_t offset = row->accleft;       /* basic offset */
      if (row->inverted) offset += 6000;   /* plus extra if inverted note */

      while (!OK)
        {
        accstr *prow = row - 1;  /* previous row */

        /* Loop, checking previous accidental positions for any overlap
        with the current accidental. */

        for(;;)   /* inner loop */
          {
          int thistop = row->pitch +
            ((row->acc == ac_ds)? 6 : (row->acc <= ac_sh)? 12 : 14);
          int thatbot = prow->pitch - ((prow->acc > ac_sh)? 6 : 12);
          int32_t thisleft = offset;
          int32_t thisright = offset - row->orig_accleft;
          int32_t thatleft = prow->accleft;
          int32_t thatright = thatleft - prow->orig_accleft;

          #ifdef SORTCHORD
          debug_printf("thistop=%d thatbot=%d\n", thistop, thatbot);
          debug_printf("thisleft=%f thisright=%f\n", thisleft, thisright);
          debug_printf("thatleft=%f thatright=%f\n", thatleft, thatright);
          #endif

          if (thistop > thatbot &&
              ((thatleft >= thisleft && thisleft > thatright) ||
               (thatleft > thisright && thisright >= thatright)))

          /* There is an overlap. Adjust the offset and break from the inner
          loop with OK still set FALSE. This will cause a repeat of the outer
          loop to check the new position. Note we insert an extra quarter point
          over and above the specified width. */

            { offset = thatleft + row->orig_accleft + 250; break; }

          /* We are clear of the accidental on the previous note, but need to
          check if we are clear of an inverted notehead. */

          if (prow->inverted)
            {
            thatbot = prow->pitch - 4;
            thatleft = 4500;           /* extra for notehead */
            thatright = 0;

            if (thistop > thatbot &&
                ((thatleft >= thisleft && thisleft > thatright) ||
                 (thatleft > thisright && thisright >= thatright)))

              { offset = thatleft + row->orig_accleft; break; }
            }

          /* Go back one more row; if no more, or if we have gone far enough,
          all is well, so break the inner loop with OK set TRUE. */

          if (--prow < a_matrix ||
            prow->pitch - row->pitch > (shortacc? 20 : 24))
              { OK = TRUE; break; }
          }

        /* If we have come out with OK set, we are clear above, but this ain't
        enough. If the offset is small, we must check that the accidental will
        clear any subsequent inverted notehead. */

        if (OK && offset < row->orig_accleft + 4500)
          {
          accstr *nnrow = nrow;

          #ifdef SORTCHORD
          debug_printf("check invert below: offset=%f row->orig_accleft=%f\n",
            offset, row->orig_accleft);
          #endif

          while (nnrow < a_end)
            {
            if (nnrow->inverted)
              {
              if ((row->pitch - nnrow->pitch) < (shortacc? 12:20))
                {
                offset = row->orig_accleft +
                  ((row->pitch - nnrow->pitch <= 8)?
                    (shortacc? 4500 : 6000) : 4500);
                OK = FALSE;  /* unset OK so that the outer loops once more */
                }
              break;
              }
            nnrow++;
            }
          }
        }   /* End of while NOT OK loop */

      /* We have now positioned the accidental successfully. Check to see
      whether the next note is far down, and if so, reset the state. */

      row->accleft = offset;

      if (nrow < a_end &&
          (row->pitch - nrow->pitch) >= (shortacc? 20 : 24)) state = 0;
      }
    }

  /* We now have the basic positioning, but there is still a little
  optimization that can be helpful. If a natural or a (double/half) flat is to
  the left of another natural or (double/half) flat that is a bit above, and
  there is nothing in the way to the right below, we can move the accidental
  (and everything below it) a bit right, to "tuck it in". This code does not
  cope with all cases, but it catches the most common. */

  for (row = a_matrix; row < a_end; row++)
    {
    BOOL OK;
    accstr *nrow;

    if (row->accleft <= row->orig_accleft + 250 ||
        (row->acc != ac_nt && row->acc < ac_hf))
      continue;

    /* Check no inverted notes or rightwards accidentals here or below */

    OK = TRUE;
    for (nrow = row; nrow < a_end; nrow++)
      if (nrow->inverted || nrow->accleft < row->accleft)
        { OK = FALSE; break; }

    /* If clear below, find the rightwards accidental above */

    if (OK)
      {
      accstr *prow;
      for (prow = row - 1; prow >= a_matrix; prow--)
        {
        int32_t x;
        if (prow->pitch - row->pitch > 20) break;
        x = row->accleft - prow->accleft;

        if (/* Check for the nearest rightwards accidental above */
            (prow->acc != ac_no && x > 0 && x < 10000) ||
            /* Check if it's an inverted note just above */
            (prow->inverted && row->accleft < 9500))
          {
          int flatbottom = (row->acc >= ac_hf)? 8 : 0;
          if (prow->pitch - row->pitch >= tuckoffset[prow->acc + flatbottom])
            {
            accstr *xrow;
            for (xrow = row; xrow < a_end; xrow++) xrow->accleft -= 2000;
            }
          break;
          }
        }
      }
    }


  DEBUG(D_sortchord)
    {
    eprintf("Modified matrix\n");
    for (row = a_matrix; row < a_end; row++)
      {
      eprintf("%d %d %d %s", row->pitch, row->inverted,
        row->acc, string_format_fixed(row->accleft));
      eprintf(" %s\n", string_format_fixed(row->orig_accleft));
      }
    }

  /* Set the information in the accleft field. */

  row = a_matrix;
  for (p = sorttop - 1; p >= 0; p--)
    {
    ww = sortvec[p];
    if (ww->acc == 0 && (ww->flags & nf_invert) == 0) continue;
    ww->accleft = (row++)->accleft;
    }
  }

/* Rebuild the chain of notes that form the chord in the correct order --
descending for stem up and ascending for stem down. If we are dealing with the
last-read chord, also adjust the order of the read_tiedata vector. */

if (upflag)
  {
  p = sorttop - 1;
  pp = 0;
  increment = -1;
  }
else
  {
  p = 0;
  pp = sorttop - 1;
  increment = +1;
  }

/* Adjustments for the first and last notes. The first note has a "note" as
opposed to a "chord" type; ensure it has the "first note of chord" bit and the
fuq bit if any of the notes in the chord had it. */

ww = sortvec[p];  /* First note */
ww->type = b_note;
ww->flags |= fuq | nf_chord;

/* Dynamics to non-stem end (the last note) in the normal case; to stem end if
flagged. */

if ((dynamics & af_opposite) == 0) sortvec[pp]->acflags |= dynamics;
  else ww->acflags |= dynamics;

/* Adjust the chain pointers on the notes, and copy the re-ordered tiedata
blocks if necessary. */

tt = read_tiedata;

for (b_notestr *www = ww;; www = sortvec[p])
  {
  before->next = (bstr *)www;
  www->prev = before;
  before = (bstr *)www;
  if (w == srs.firstnoteptr) *tt++ = sorttievec[p];
  if (p == pp) break;
  p += increment;
  }

before->next = after;
if (after != NULL) after->prev = before; else read_lastitem = before;

/* Update the saved pointers for the most recent note/chord if necessary, and
if this is the start of a chord for copying or a beam or the first item for bar
repeating, that pointer must also be updated. */

if (w == srs.firstnoteptr) srs.firstnoteptr = ww;
if (w == srs.lastnoteptr) srs.lastnoteptr = ww;
if (w == srs.beamfirstnote) srs.beamfirstnote = ww;

if (w == (b_notestr *)(brs.repeatstart)) brs.repeatstart = (bstr *)ww;
}



/*************************************************
*        Set stem directions for unforced beam   *
*************************************************/

/* This function is called at the end of a beam in all cases. It is global
because it is also called after [reset] and at the end of a bar if there is an
unfinished beam. For beams whose stem direction is forced, there is nothing on
the beam stack. This procedure is even called for single notes that might have
been the start of a beam, so we use the call to set the fuq (free upstemmed
quaver) flag when the stem direction is known.

If the option for the stem swap level is "right", we can't take a decision
here, so the notes are transferred on to the ordinary note pending stack.

Arguments:  none
Returns:    nothing
*/

void
read_setbeamstems(void)
{
if (brs.beamstackptr > 0)
  {
  int i;
  uint32_t flag = 0;

  if (srs.maxaway == P_3L + 4*curmovt->stemswaplevel[srs.stavenumber])
    {
    switch (curmovt->stemswaptype)
      {
      case stemswap_default:
      case stemswap_left:
      if (srs.laststemup) flag = nf_stemup;
      break;

      case stemswap_up:
      flag = nf_stemup;
      break;

      case stemswap_down:
      break;

      case stemswap_right:
      for (i = 0; i < brs.beamstackptr; i++)
        read_stemstack[brs.stemstackptr++] = read_beamstack[i];
      brs.beamstackptr = 0;
      return;
      }
    }

  else if (srs.maxaway < P_3L + 4*curmovt->stemswaplevel[srs.stavenumber])
    flag = nf_stemup;

  for (i = 0; i < brs.beamstackptr; i++)
    mac_setstemflag(read_beamstack[i], flag);

  brs.beamstackptr = 0;
  srs.laststemup = (flag != 0);
  mac_setstackedstems(flag);
  }

srs.beaming = FALSE;
if (srs.beamcount == 1 && (srs.beamfirstnote->flags & nf_stemup) != 0)
  srs.beamfirstnote->flags |= nf_fuq;  /* Free upstemmed quaver */
}



/*************************************************
*    Processing after reading a note or chord    *
*************************************************/

/* The flag is TRUE if called after an original note/chord, after one created
with 'p', or the final one of a set of 'x' copies, in which case things that
can follow a note/chord are processed. The flag is FALSE for intermediates in a
multiple 'x' replication. We first have to choose a stem direction, which is
tied in with the beaming and which we can't always complete at this point.

Argument:  TRUE for original note or last in a set of replications
Returns:   nothing
*/

static void
post_note(BOOL followOK)
{
usint i;
uint16_t stempitch = (pn_maxpitch + pn_minpitch)/2;     /* zero for rests */
BOOL pletending = FALSE;

if (!srs.lastwastied) pn_onlytieddup = FALSE;

/* Set up the tie data for this note before any chord sorting, because it has
to be sorted along with the chord. */

for (i = 0; i < pn_chordcount; i++) read_tiedata[i] = pn_tiedata[i];

/* A note or rest longer than a quaver terminates a beam, unless it is a grace
note. */

if (pn_notetype < quaver && srs.beaming && pn_notelength > 0)
  read_setbeamstems();

/* Deal with non-rests and non-grace notes */

if (stempitch > 0 && pn_notelength != 0)
  {
  brs.lastgracestem = 0;      /* unset grace note forcing */

  /* If already beaming, count notes in the beam */

  if (srs.beaming) srs.beamcount++;

  /* Else a note shorter than a crotchet starts a beam */

  else if (pn_notetype > crotchet)
    {
    srs.beaming = TRUE;
    srs.beamfirstnote = srs.firstnoteptr;    /* remember first note */
    srs.beamcount = 1;

    if (pn_stemforce == 0) pn_stemforce = srs.stemsdirection;

    if (pn_stemforce != 0)
      {
      srs.beamstemforce = pn_stemforce;
      mac_setstackedstems((srs.beamstemforce > 0)? nf_stemup : 0);
      }
    else
      {
      srs.beamstemforce = 0;
      srs.maxaway = stempitch;
      }
    }

  /* Deal with beamed and non-beamed notes which have their stem direction
  forced. Note that we must call setstemflag even for down stems, because it
  does other work for chords. */

  if (pn_stemforce != 0 || (!srs.beaming && srs.stemsdirection != 0))
    {
    uint32_t flag;
    if (pn_stemforce == 0) pn_stemforce = srs.stemsdirection;
    flag = (pn_stemforce > 0)? nf_stemup : 0;
    mac_setstemflag(srs.firstnoteptr, flag);

    /* For non-beamed notes, set the flag for any pending queued notes,
    and remember the direction. We don't remember the direction for
    forced notes in the middle of beams -- these are usually eccentric. */

    if (!srs.beaming)
      {
      mac_setstackedstems(flag);
      srs.laststemup = flag != 0;
      }
    }

  /* Deal with a beamed note that does not have a forced stem. If the previous
  note was tied and we are at the start of a beam, copy the stem direction of
  the previous note, if known. Then, if the beam's stem direction was forced,
  set this note's direction. Otherwise use its pitch in computing the maxaway
  value, and add it to the beam stack. */

  else if (srs.beaming)
    {
    if (srs.lastwastied && srs.beamcount == 1 && brs.stemstackptr == 0)
      srs.beamstemforce = srs.laststemup? 1 : -1;

    if (srs.beamstemforce != 0)
      {  /* Necessary because macro ends with "}" */
      mac_setstemflag(srs.firstnoteptr, (srs.beamstemforce > 0)? nf_stemup : 0);
      }
    else
      {
      if (abs((int)stempitch - P_3L) > abs((int)srs.maxaway - P_3L))
        srs.maxaway = stempitch;
      read_beamstack[brs.beamstackptr++] = srs.firstnoteptr;
      }
    }

  /* Deal with non-beamed note that does not have a forced stem - if the stem
  direction is immediately decidable, use it and empty the stack of any pending
  notes awaiting a decision. Otherwise add this note to the stack. Note that we
  must call setstemflag, even with a zero flag, because it also sorts chords
  and deals with inverted notes. */

  else if (srs.lastwastied && brs.stemstackptr == 0 &&
           (pn_chordcount > 1 || srs.lasttiepitch == srs.firstnoteptr->spitch))
    {  /* Necessary because macro ends with "}" */
    mac_setstemflag(srs.firstnoteptr, srs.laststemup? nf_stemup : 0);
    }

  else if (stempitch != P_3L + 4*curmovt->stemswaplevel[srs.stavenumber])
    {
    uint32_t flag;
    srs.laststemup = stempitch < P_3L + 4*curmovt->stemswaplevel[srs.stavenumber];
    flag = (srs.laststemup)? nf_stemup : 0;
    mac_setstemflag(srs.firstnoteptr, flag);
    mac_setstackedstems(flag);
    }

  /* What happens to notes that are on the stemswap level depends on the type
  of stemswapping specified. */

  else switch (curmovt->stemswaptype)
    {
    case stemswap_default:
    if (brs.firstnoteinbar || brs.stemstackptr > 0)
      read_stemstack[brs.stemstackptr++] = srs.firstnoteptr;
    else mac_setstemflag(srs.firstnoteptr, srs.laststemup? nf_stemup : 0);
    break;

    case stemswap_up:
    mac_setstemflag(srs.firstnoteptr, nf_stemup);
    break;

    case stemswap_down:
    mac_setstemflag(srs.firstnoteptr, 0);
    break;

    case stemswap_left:
    mac_setstemflag(srs.firstnoteptr, srs.laststemup? nf_stemup : 0);
    break;

    case stemswap_right:
    read_stemstack[brs.stemstackptr++] = srs.firstnoteptr;
    break;
    }

  /* Subsequent notes are no longer the first in the bar */

  brs.firstnoteinbar = FALSE;
  }

/* Grace notes are always stem up unless explicitly marked, but a single forced
grace note forces all immediately following. */

else if (pn_notelength == 0)
  {
  if (pn_stemforce == 0) pn_stemforce = brs.lastgracestem;
  mac_setstemflag(srs.firstnoteptr, (pn_stemforce >= 0)? nf_stemup : 0);
  brs.lastgracestem = pn_stemforce;
  }

/* For a rest, unset grace stem forcing */

else
  {
  brs.lastgracestem = 0;
  }

/* If this is an intermediate note in a set replicated by 'x', we are done. */

if (!followOK) return;

/* Now need deal with ties, glissando marks, beam breaks, and the ends of plet
groups. We permit the plet group ending to come before the other items. */

if (read_c == '}')
  {
  pletending = TRUE;
  read_nextc();
  }

/* Deal with ties and glissandos */

if (read_c == '_')
  {
  uint8_t acount = 0;
  uint8_t bcount = 0;
  uint8_t flags = tief_default;

  if (srs.tiesplacement > 0) acount = 255;
    else if (srs.tiesplacement < 0) bcount = 255;

  read_nextc();

  while (read_c == '/')
    {
    read_nextc();
    if (read_c == 'g')
      {
      flags &= ~tief_default;
      if (pn_inchord == 0) flags |= tief_gliss; else error(ERR166);
      }
    else if (read_c == 's') flags |= tief_slur;
    else if (read_c == 'e') flags |= tief_editorial;
    else if (read_c == 'i')
      {
      if (main_readbuffer[read_i] == 'p')
        {
        flags |= tief_dotted;
        read_i++;
        }
      else flags |= tief_dashed;
      }
    else
      {
      usint count = 255;
      if (isdigit(read_c)) count = read_usint();
      if (read_c == 'b') { bcount = count; acount = 0; }
        else if (read_c == 'a') { acount = count; bcount = 0; }
          else error(ERR8, "/a /b /e /g /i /p or /s");
      flags |= tief_slur;
      }
    read_nextc();
    }

  /* Editorial marks are not supported on dotted and dashed ties/slurs. */

  if ((flags & tief_editorial) != 0)
    {
    if ((flags & (tief_dotted | tief_dashed)) != 0) error(ERR125);
    }

  /* Unless this follows a rest, set up a tie structure. Remember the first
  note of a chord. This might get sorted and no longer be the first note; that
  is checked for later. */

  if (pn_minpitch == 0) error(ERR13, "tie after rest ignored"); else
    {
    b_tiestr *p = mem_get_item(sizeof(b_tiestr), b_tie);
    p->flags = flags;
    p->abovecount = acount;
    p->belowcount = bcount;
    p->noteprev = srs.firstnoteptr;
    srs.lastwastied = TRUE;
    srs.lasttiepitch = srs.firstnoteptr->spitch;
    }

  brs.resetOK = FALSE;

  /* Allow a plet ending to appear before any potential beam break. */

  if (!pletending && read_c == '}')
    {
    pletending = TRUE;
    read_nextsigc();
    }
  }

/* Note not followed by tie or glissando */

else
  {
  srs.lastwastied = FALSE;
  brs.resetOK = TRUE;
  }

/* If a relevant note is followed by a semicolon, set a primary beam break. If
followed by a comma, set secondary beam break. We used to give an error when
either of these characters did not follow a quaver or shorter note. This makes
it annoying to use doublenotes or halvenotes to set a piece in different ways.
We now allow comma and semicolon after any note at this point, only generating
the relevant break for short enough notes. Any other occurrences still give an
error. */

if (read_c == ';')
  {
  read_nextc();
  if (pn_notetype >= quaver)
    {
    b_beambreakstr *b = mem_get_item(sizeof(b_beambreakstr), b_beambreak);
    b->value = BEAMBREAK_ALL;
    if (srs.beaming) read_setbeamstems();
    }
  }

else if (read_c == ',')
  {
  uint8_t v;
  read_nextc();
  if (isdigit(read_c))
    {
    v = read_c - '0';
    read_nextc();
    }
  else v = 1;
  if (pn_notetype >= quaver)
    {
    b_beambreakstr *b = mem_get_item(sizeof(b_beambreakstr), b_beambreak);
    b->value = v;
    }
  }

/* This is another place where and end to a plet group is allowed. */

read_sigc();
if (!pletending && read_c == '}')
  {
  read_nextc();
  pletending = TRUE;
  }

/* Deal with ending a plet group */

if (pletending)
  {
  if (brs.pletlen == 0) error(ERR126); else
    {
    brs.pletlen = 0;
    (void)mem_get_item(sizeof(bstr), b_endplet);
    }
  }

/* If there was a [smove] before the note, insert the appropriate
space directive. */

if (brs.smove != 0)
  {
  b_spacestr *s = mem_get_item(sizeof(b_spacestr), b_space);
  s->x = brs.smove;
  s->relative = brs.smove_isrelative;
  brs.smove = 0;
  }

/* If we had a tied chord containing seconds, generate an implicit [ensure].
The item is remembered so that it can be cancelled if this turns out to be the
last note of the bar. */

if (pn_seconds && srs.lastwastied)
  {
  b_ensurestr *pe = mem_get_item(sizeof(b_ensurestr), b_ensure);
  pe->value = 20000;
  brs.lasttieensure = pe;
  }
else brs.lasttieensure = NULL;

/* Finally, update the count of notes in this chord */

srs.chordcount = pn_chordcount;
}




/*************************************************
*           Read one note or chord               *
*************************************************/

/* This function is called if the stave scanner cannot interpret the current
character as the start of a directive or any other non-note construction, and
the character is one that can start a note.

Arguments:  none
Returns:    nothing
*/

void
read_note(void)
{
b_notestr *old_lastnoteptr;
b_ornamentstr *old_lastnote_ornament;

uint32_t ornset = 0;                /* Remembers ornaments */
uint32_t chordlength = 0;
int32_t yextra;                     /* For restlevel or stemlength */
uint16_t prevpitch = 0;
uint8_t ornament = srs.ornament;    /* Default ornament */

srs.firstnoteptr = NULL;            /* Reading first note in a chord */
pn_maxpitch = 0;
pn_minpitch = UINT16_MAX;
pn_stemforce = 0;                   /* Stem not forced */
pn_inchord = 0;                     /* Not in a chord */
pn_seconds = FALSE;                 /* Chord does not contain seconds */

/* If this is the first note in a bar, behave for "p" and "x" as if all
previous notes were tied duplicated (i.e. show accidentials unless this note is
tied to its predecessor). */

if (brs.firstnoteinbar) pn_onlytieddup = TRUE;

/* Keep track of the first note in the bar that is not tied from the previous
bar, in order to help with transpositions in key N. */

if (brs.firstnontiedset) brs.firstnontied = FALSE;
  else if (!srs.lastwastied) brs.firstnontied = brs.firstnontiedset = TRUE;

/* Handle exact duplication of the previous note or chord. Duplication of a
previous note's pitch(es) only is handled by the 'p' letter, and is mixed up
with other interpretation below. This code leaves srs.lastnoteptr and
srs.lastnote_ornament unchanged. */

if (read_c == 'x')
  {
  usint count = 1;
  read_nextsigc();
  if (isdigit(read_c)) count = read_usint();

  if (read_c == '\\')  /* No options are allowed */
    {
    read_nextc();
    error_skip(ERR108, '\\');
    read_nextc();
    }

  if (srs.lastnoteptr == NULL)
    {
    error(ERR109);
    return;
    }

  /* Set data for post_note() from the note to be copied. Can't assume this
  already set because a rest with different characteristics may have
  intervened. */

  pn_notelength = srs.lastnoteptr->length;
  pn_notetype = srs.lastnoteptr->notetype;

  /* Replicate a note or chord as many times as required. We have to reset the
  relevant pointers each time because they are advanced through a chord. */

  do
    {
    b_notestr *new;
    b_notestr *old = srs.lastnoteptr;
    b_ornamentstr *p = srs.lastnote_ornament;
    BOOL show_accidental = pn_onlytieddup && !srs.lastwastied;

    do_undoverlay(&srs.pendulay);
    do_undoverlay(&srs.pendolay);
    pn_chordcount = 0;

    /* If the copied note had one or more ornaments, copy them. Disable any
    above/below accidentals if they are not to be displayed. We do not just
    ignore them, because a subsequent 'x' or 'p' after a bar line might need to
    restore them. */

    if (p != NULL) while ((bstr *)p != (bstr *)old)
      {
      if (p->type == b_ornament)
        {
        b_ornamentstr *pp = mem_duplicate_item(p, sizeof(b_ornamentstr));
        if (pp->ornament >= or_nat)
          {
          if (show_accidental)
            {
            pp->bflags &= ~orn_invis;
            show_accidental = FALSE;   /* Disable normal accidental */
            }
          else pp->bflags |= orn_invis;
          }
        }
      p = (b_ornamentstr *)(p->next);
      }

    /* Copy the first note and any subsequent notes of a chord. */

    do
      {
      new = mem_duplicate_item(old, sizeof(b_notestr));
      if (pn_chordcount++ == 0) srs.firstnoteptr = new;

      /* At bar start, or after only tied duplicates, show the accidental;
      otherwise don't. Accidental spacing is already set, so force the accleft
      flag. */

      new->flags = (new->flags & ~nf_accinvis) | nf_accleft;
      if (!show_accidental) new->flags |= nf_accinvis;

      if (new->spitch > pn_maxpitch) pn_maxpitch = new->spitch;
      if (new->spitch < pn_minpitch) pn_minpitch = new->spitch;

      srs.pitchtotal += new->abspitch;
      srs.pitchcount++;

      old = (b_notestr *)(old->next);
      }
    while (old->type == b_chord);     /* End of chord copy loop */

    brs.barlength += pn_notelength;
    post_note(count <= 1);
    }
  while (--count > 0);  /* End of replication loop */

  return;
  }

/* Not an exact note repetition: handle the reading of a new note or chord. */

old_lastnote_ornament = srs.lastnote_ornament;  /* For if this is 'p' */
old_lastnoteptr = srs.lastnoteptr;              /* Ditto */

srs.lastnote_ornament = NULL;
pn_chordcount = 0;

/* Deal with the start of a chord */

if (read_c == '(') { pn_inchord = nf_chord; read_nextsigc(); }

/* Loop to read a single note or all the notes of a chord. */

for (;;)
  {
  b_notestr *noteptr;

  BOOL acc_onenote = FALSE;
  BOOL duplicating = FALSE;

  int32_t accleft = 0;
  int tiedcount = -1;
  int masq = MASQ_UNSET;
  usint dup_octave = 0;   /* Octave for 'p' note */

  uint32_t acflags = (pn_chordcount == 0)? srs.accentflags : 0;
  uint32_t flags = srs.noteflags | pn_inchord;
  uint32_t explicit_couple = 0;

  int16_t pitch, abspitch, pitch_orig;

  uint8_t *acc_above = NULL;
  uint8_t acc;
  uint8_t acc_orig, char_orig;
  uint8_t transposedacc = ac_no;
  uint8_t noteheadstyle = srs.noteheadstyle;
  BOOL    transposedaccforce = active_transposedaccforce;
  BOOL    note_set_taf = FALSE;

  /* Read an accidental, if present, and save the original accidental (prior to
  transposition) for use if this note has to be re-created for 'p'. The note
  letter is also saved later. */

  acc_orig = acc = read_accidental();

  /* Deal with requests for transposed accidentals */

  if (read_c == '^')
    {
    read_nextc();
    if (read_c == '-')
      {
      transposedaccforce = FALSE;
      note_set_taf = TRUE;
      read_nextc();
      }
    else if (read_c == '+')
      {
      transposedaccforce = TRUE;
      note_set_taf = TRUE;
      read_nextc();
      }
    transposedacc = read_accidental();
    }

  /* Deal with special forms of accidental: invisible, above/below, bracketed,
  and moved. An invisible accidental may have no further options. */

  if (acc != ac_no)
    {
    if (read_c == '?')
      {
      flags |= nf_accinvis;
      read_nextc();
      }

    /* Deal with a request to print the accidental above or below the note.
    We do this by setting it invisible, and generating a suitable ornament.
    This is only allowed on the first note of a chord. */

    else if (read_c == 'o' || read_c == 'u')
      {
      if (pn_chordcount == 0)
        {
        b_ornamentstr *p = mem_get_item(sizeof(b_ornamentstr), b_ornament);
        if (srs.lastnote_ornament == NULL) srs.lastnote_ornament = p;
        p->ornament = ((read_c == 'o')? or_nat : or_accbelow);
        p->bflags = 0;
        read_nextc();
        ornopts(&(p->x), &(p->y), NULL);
        if (read_c == ')') { p->ornament += 1; read_nextc(); }
          else if (read_c == ']') { p->ornament += 2; read_nextc(); }
        acc_above = &(p->ornament);   /* To fill in after transposition */
        flags |= nf_accinvis;
        acc_onenote = TRUE;
        }
      else error(ERR111);
      }

    /* Deal with visible, normally placed, accidentals, which can be in round
    or square brackets, and which can be moved left. */

    else
      {
      if (read_c == ')') { flags |= nf_accrbra; read_nextc(); }
        else if (read_c == ']') { flags |= nf_accsbra; read_nextc(); }
      while (read_c == '<')
        {
        read_nextc();
        accleft += (isdigit(read_c))? read_fixed() : 5000;
        flags |= nf_accleft;
        }
      }
    }

  /* Now process a note letter. Non-letters will be assigned as crotchets. */

  if (isupper(read_c))
    {
    pn_notetype = minim;
    pn_notelength = len_minim;
    }
  else
    {
    pn_notetype = crotchet;
    pn_notelength = len_crotchet;
    }

  char_orig = read_c = tolower(read_c);

  /* Deal with duplication of previous pitch(es). This must not be in a chord
  and there must be no accidentals. We regenerate the pitch letter from the
  previous note, and the accidental if this is the start of a bar, unless after
  a tie. */

  if (read_c == 'p')
    {
    if (acc != ac_no) error(ERR112);
    if (pn_inchord != 0) { error(ERR52); read_c = 'a'; }
    else if (srs.lastnoteptr == NULL) { error(ERR109); read_c = 'r'; }
    else
      {
      b_ornamentstr *p = old_lastnote_ornament;
      usint x = srs.lastnoteptr->abspitch;
      BOOL show_accidental = pn_onlytieddup && !srs.lastwastied;

      if (active_transpose != NO_TRANSPOSE) x -= active_transpose; /* Pre-transpose */
      dup_octave = OCTAVE - srs.octave;
      while (x <  4*OCTAVE) { x += OCTAVE; dup_octave -= OCTAVE; }
      while (x >= 5*OCTAVE) { x -= OCTAVE; dup_octave += OCTAVE; }

      /* Set the accidental - needed for transposition, even if we aren't going
      to print it, for cases of forcing - and retrieve the original note
      letter. Accidental spacing is already set, so force nf_accleft. */

      acc = acc_orig = srs.lastnoteptr->acc_orig;
      transposedacc = srs.lastnoteptr->acc;

      accleft = srs.lastnoteptr->accleft;
      flags |= nf_accleft |
        (srs.lastnoteptr->flags & (nf_accrbra | nf_accsbra));

      char_orig = read_c = srs.lastnoteptr->char_orig;

      /* At bar start, or after only tied duplicates, retain the accidental;
      otherwise make it invisible. Accidentals that are printed above or below
      are handled as ornaments, so we must scan for them and make them visible
      or invisible as necessary. They must be retained, as the next 'x' or 'p'
      may be in the next bar. */

      if (!show_accidental) flags |= nf_accinvis;

      if (p != NULL) while ((bstr *)p != (bstr *)srs.lastnoteptr)
        {
        if (p->type == b_ornament && ((b_ornamentstr *)p)->ornament >= or_nat)
          {
          b_ornamentstr *pp = mem_duplicate_item(p, sizeof(b_ornamentstr));
          if (show_accidental) pp->bflags &= ~orn_invis;
            else pp->bflags |= orn_invis;
          if (srs.lastnote_ornament == NULL) srs.lastnote_ornament = pp;
          flags |= nf_accinvis;  /* Don't show normal accidental */
          }
        p = (b_ornamentstr *)(p->next);
        }

      duplicating = TRUE;    /* Cause rest of chord to be copied at end */
      }
    }

  /* Deal with non-rests, i.e. notes. Get a pitch independent of any clef. The
  units are quartertones and the origin is such that middle C has the value 96
  (defined as MIDDLE_C), which should cope with all requirements. This pitch
  doesn't yet contain the accidental (if any). */

  if ('a' <= read_c && read_c <= 'g')
    {
    pitch_orig = pitch = srs.octave + 3*OCTAVE + read_basicpitch[read_c - 'a'];
    yextra = srs.stemlength;     /* Default stem length */
    if (!duplicating) pn_onlytieddup = FALSE;
    }

  /* Deal with rests. R is a normal rest, Q is a quiet (invisible) rest, S is
  like R but whole bars do not pack up, and T is a "repeat beat" mark (looks
  like '/') that is handled as a differently printed crotchet rest. */

  else
    {
    pitch_orig = pitch = 0;
    acflags &= ~af_accents;         /* Ignore any default dynamics */
    yextra = srs.rlevel;
    if (read_c == 'q') flags |= nf_hidden;
    else if (read_c == 's') flags |= nf_nopack;
    else if (read_c == 't') flags |= (nf_restrep | nf_nopack);
    else if (read_c != 'r')
      {
      if (pn_inchord != 0) error_skip(ERR8, (')'<<8) | ' ',
        (pn_chordcount == 0)? "note letter " : "note letter or end of chord");
          else error(ERR8, "note letter");
      pitch = 60;                 /* Take a random pitch */
      pn_inchord = 0;             /* Terminate a chord, in case missing ')' */
      brs.checklength = FALSE;    /* Disable length check for this bar */
      }
    if (pitch == 0)
      {
      if (pn_inchord != 0) error(ERR113);
        else if (srs.lastwastied) error(ERR118, "rest");
      }
    }

  /* If suspended and not a rest, resume automatically */

  if (srs.suspended && pitch > 0)
    {
    (void)mem_get_item(sizeof(bstr), b_resume);
    srs.suspended = FALSE;
    }

  /* Pitch adjustment for octave indicators, transposition and clef, except for
  rests, of course. If we are handling 'p', "duplicating" is true and the
  octave is in dup_octave. */

  read_nextc();
  if (pitch == 0) abspitch = pn_minpitch = 0; else   /* min = max = 0 for rests */
    {
    if (duplicating) pitch += dup_octave; else
      {
      while (read_c == '\'') { pitch += OCTAVE; read_nextc(); }
      while (read_c == '`' ) { pitch -= OCTAVE; read_nextc(); }
      }

    /* Get a true absolute pitch which includes the accidental. Update the
    table which keeps track of the current accidental state for a given input
    pitch. After a tie, take the default accidental from the previous note,
    which may have been in the previous bar. Save the information in case this
    note is tied. */

    if (pn_chordcount >= MAX_CHORDSIZE) error(ERR110, MAX_CHORDSIZE); /* Hard */
    pn_tiedata[pn_chordcount].pitch = pitch;

    if (acc != ac_no)
      {
      int x = read_accpitch[acc];
      if (!acc_onenote) read_baraccs[pitch] = x;
      pn_tiedata[pn_chordcount].acc = x;
      abspitch = pitch + x;
      }
    else
      {
      if (srs.lastwastied)
        {
        int i;
        for (i = 0; i < srs.chordcount; i++)
          {
          if (pitch == read_tiedata[i].pitch)
            {
            int x = read_tiedata[i].acc;
            abspitch = pitch + x;
            pn_tiedata[pn_chordcount].acc = x;
            tiedcount = i;
            flags |= nf_accinvis;   /* Could come from transpose if previous is invisible */
            break;
            }
          }
        }

      /* Last note not tied, or tie not matched up (slur) */

      if (tiedcount < 0)
        {
        abspitch = pitch + read_baraccs[pitch];
        pn_tiedata[pn_chordcount].acc = read_baraccs[pitch];
        }
      }

    /* Transpose the note and its accidental if required */

    if (active_transpose != NO_TRANSPOSE)
      {
      abspitch = transpose_note(abspitch, &pitch, &acc, transposedacc,
        transposedaccforce, note_set_taf, acc_onenote, FALSE, tiedcount);
      pn_tiedata[pn_chordcount].acc_tp = read_baraccs_tp[pitch];
      }

    /* Adjust absolute pitch for transposing clefs */

    abspitch += srs.clef_octave;

    if (abspitch < 0 || abspitch > 200) error(ERR173);  /* Hard */

    /* Keep track of the absolute pitch range, maintain data for tessitura
    computation, and remember if any half accidentals are used. This covers
    both explicit half accidentals and those in a key signature. */

    if (abspitch > srs.maxpitch) srs.maxpitch = abspitch;
    if (abspitch < srs.minpitch) srs.minpitch = abspitch;
    if ((abspitch & 1) != 0) st->halfaccs = TRUE;

    srs.pitchtotal += abspitch;
    srs.pitchcount++;

    /* Now adjust the printing pitch to make it relative to the current clef.
    However, if the printing pitch is explicitly forced (e.g. for percussion
    staves), the accidental must be cancelled. */

    if (srs.printpitch != 0)
      {
      pitch = srs.printpitch;
      acc = ac_no;
      }
    else pitch = pitch_stave[pitch] + pitch_clef[srs.clef];

    /* Save max/min pitch per chord */

    if (pitch > pn_maxpitch) pn_maxpitch = pitch;
    if (pitch < pn_minpitch) pn_minpitch = pitch;

    /* Set flags for potentially auto-coupled notes; these may be unset
    later by the \h\ option. */

    if (pitch > P_6L) flags |= nf_coupleU;
      else if (pitch < P_0L) flags |= nf_coupleD;

    /* Flag chords containing seconds */

    if (abs(pitch - prevpitch) == 4) pn_seconds = TRUE;
    prevpitch = pitch;
    }

  /* If the accidental is to be printed above or below, add in its value, which
  is not known till after transposition. */

  if (acc_above != NULL) *acc_above += 3*(acc - 1);

  /* Note or rest length adjustment by modifiers. The ! modifier must follow a
  minim. It turns the note/rest into a centred semibreve, leaving the
  nf_restrep flag if set for T!*/

  if (read_c == '!')  /* Note or rest fills the bar */
    {
    if (pn_notetype != minim) error(ERR114);
    pn_notetype = semibreve;
    pn_notelength = srs.required_barlength;
    if (pitch == 0) flags |= nf_centre;
    read_nextc();
    }

  /* Process other modifiers. Too long or short notes are diagnosed later,
  after halving/doubling. */

  else
    {
    while (read_c == '=' )
      { read_nextc(); pn_notetype += 2; pn_notelength /= 4; }
    while (read_c == '-' )
      { read_nextc(); pn_notetype += 1; pn_notelength /= 2; }
    while (read_c == '+')
      { read_nextc(); pn_notetype -= 1; pn_notelength *= 2; }
    }

  /* Deal with non-standard lengths. */

  if (brs.pletlen != 0)
    pn_notelength = (pn_notelength*brs.pletnum)/(brs.pletlen*brs.pletden);

  /* Deal with dotted notes. For the first note of a chord, a movement of the
  dot is permitted. */

  if ((read_c == '>' && main_readbuffer[read_i] == '.') || isdigit(read_c))
    {
    if (pn_chordcount == 0)
      {
      b_dotrightstr *d = mem_get_item(sizeof(b_dotrightstr), b_dotright);
      if (read_c == '>')
        {
        read_nextc();
        d->value = 5000;  /* Default 5 points */
        }
      else
        {
        d->value = read_fixed();
        if (read_c != '>' || main_readbuffer[read_i] != '.')
          error(ERR8, "\">.\" (augmentation dot movement)");
        read_nextc();
        }
      }
    else error(ERR116, "augmentation dot movement");
    }

  /* Lengthen the note according to .+ . or .. */

  if (read_c == '.' )
    {
    read_nextc();
    if (read_c == '+')
      {
      read_nextc();
      flags |= nf_plus;
      pn_notelength = (pn_notelength*5)/4;
      }
    else
      {
      flags |= nf_dot;
      pn_notelength = (pn_notelength*3)/2;
      if (read_c == '.' )
        {
        read_nextc();
        flags |= nf_dot2;
        pn_notelength = (pn_notelength*7)/6;
        }
      }
    }

  /* Handle doubled or halved note lengths - note that full bar rests must be
  skipped (the centre flag is an indicator). */

  if ((srs.notenum != 1 || srs.noteden != 1) && (flags & nf_centre) == 0)
    {
    int i = srs.notenum;
    while (i > 1) { pn_notelength += pn_notelength; pn_notetype--; i /= 2; }
    i = srs.noteden;
    while (i > 1) { pn_notelength /= 2; pn_notetype++; i /= 2; }
    }

  /* Can't handle anything longer than a breve or shorter than a
  hemidemisemiquaver. */

  if (pn_notetype < breve || pn_notetype > hdsquaver) error(ERR115);  /* Hard */

  /* The use of 't' for a repeat sign as a special kind of rest applies only to
  crotchets, but the nf_restrep flag must be left alone for whole bar rests, as
  it signals a conventional whole bar repeat. */

  if (pn_notetype != crotchet && (flags & nf_centre) == 0) flags &= ~nf_restrep;

  /* Set left movement for any accidental. We can't do this earlier, because
  transposition can alter which accidental is printed, and we need to have
  pn_notetype set correctly for accadjusts. If duplicating, the movement has
  already been copied. */

  if (acc != ac_no && !duplicating)
    {
    accleft += curmovt->accspacing[acc] - curmovt->accadjusts[pn_notetype];
    if ((flags & nf_accrbra) != 0) accleft += rbra_left[acc];
      else if ((flags & nf_accsbra) != 0) accleft += sbra_left[acc];
    }

  /* Now we have the final length for the note, adjust it if necessary to allow
  for triplet vs duplet time signatures, etc. */

  if (srs.matchnum > 0 && (flags & nf_centre) == 0)
    pn_notelength = mac_muldiv(pn_notelength, srs.matchnum, srs.matchden);

  /* If a whole bar rest, flag it for centring unless we are in an unchecked
  bar. (If the rest was specified using the ! notation it will already be
  flagged for centring. This code covers other cases.) We must do this before
  checking the options so that \C\ can be used to turn off centring. */

  if (pitch == 0 && pn_notelength == srs.required_barlength && brs.checklength)
    flags |= nf_centre;

  /* Handle options enclosed in \...\ that follow a note. These include accents
  and ornaments, stem direction control, local level for rests, etc. */

  if (read_c == '\\')
    {
    uint32_t accororn;
    uschar *end;

    /* Search for the terminating backslash in the current line, stopping at
    real bar line, to try to minimise error cascades when the terminator is
    missing. */

    for (end = main_readbuffer + read_i; *end != '\\'; end++)
      {
      if (*end == '\n' || (*end == '|' && end[-1] != '~' && end[-1] != 't'))
        {
        error_skip(ERR117, '|');
        brs.checklength = FALSE;   /* Don't check this bar */
        goto ENDOPTIONS;
        }
      }

    /* Now process the options */

    read_nextsigc();

    while (read_c != '\\')
      {
      switch (read_c)
        {

        /* ----- Handle things that are not accents or ornaments ----- */

        case ':':
        read_nextc();
        if (read_c == ':')
          {
          read_nextc();
          flags |= nf_highdot;
          }
        else flags ^= nf_lowdot;
        break;

        case ')':
        read_nextc();
        flags |= nf_headbra;
        break;

        case 'c':
        read_nextc();
        explicit_couple = nf_couple;
        break;

        /* This flag flips the centring flag iff this is the first note in the
        bar and has the barlength. */

        case 'C':
        read_nextc();
        if (brs.barlength == 0 && pn_notelength == srs.required_barlength)
          flags ^= nf_centre;
        break;

        case 'g':
        pn_notelength = 0;
        read_nextc();
        if (read_c == '/')
          {
          flags |= nf_appogg;
          read_nextc();
          }
        break;

        case 'h':
        read_nextc();
        flags &= ~nf_couple;
        explicit_couple = 0;
        break;

        case 'l':  /* local rest level */
        read_nextc();
          {
          int32_t y;
          if (read_expect_integer(&y, TRUE, TRUE)) yextra += y;
          }
        break;

        case 'M':
        case 'm':
        if (pn_chordcount != 0) error(ERR116, "masquerade setting");
        masq = (read_c == 'm')? crotchet : minim;
        flags &= ~(nf_dot|nf_dot2|nf_plus);
        for (;;)
          {
          read_nextc();
          if (read_c == '-') masq++;
          else if (read_c == '=') masq += 2;
          else if (read_c == '+') masq--;
          else break;
          }
        if (masq < breve || masq > hdsquaver)
          {
          error_skip(ERR115, '\\');
          masq = MASQ_UNSET;
          }
        if (read_c == '.')
          {
          read_nextc();
          if (read_c == '+')
            {
            read_nextc();
            flags |= nf_plus;
            }
          else
            {
            flags |= nf_dot;
            if (read_c == '.')
              {
              read_nextc();
              flags |= nf_dot2;
              }
            }
          }
        break;

        case 'n':
        read_nextc();
        if (read_c == 'o') flags &= ~nf_stem; else
          {
          noteheadstyle &= ~nh_mask;
          flags |= nf_stem;
          switch(read_c)
            {
            case 'c':
            noteheadstyle |= nh_circular;
            flags &= ~nf_stem;
            break;

            case 'd':
            noteheadstyle |= nh_direct;
            flags &= ~nf_stem;
            break;

            case 'h':
            noteheadstyle |= nh_harmonic;
            break;

            case 'n':
            noteheadstyle |= nh_normal;
            break;

            case 'x':
            noteheadstyle |= nh_cross;
            break;

            case 'z':
            noteheadstyle |= nh_none;
            break;

            default:
            error(ERR8, "nc, nd, nh, nn, no, nx, or nz");
            break;
            }
          }
        read_nextc();
        break;

        case 's':
        read_nextc();
        if (read_c == 'u')
          {
          if (pn_stemforce < 0)
            error(ERR120, (pn_inchord != 0)? "in chord" : "");
          pn_stemforce = 1;
          read_nextc();
          }
        else if (read_c == 'd')
          {
          if (pn_stemforce > 0)
            error(ERR120, (pn_inchord != 0)? "in chord" : "");
          pn_stemforce = -1;
          read_nextc();
          }
        else if (read_c == 'w')
          {
          if (srs.beaming && pn_notetype >= quaver && srs.beamstemforce != 0)
            {
            if (pn_stemforce != 0)
              error(ERR120, (pn_inchord != 0)? "in chord" : "");
            srs.beamstemforce = -srs.beamstemforce;
            if (abs(srs.beamstemforce) == 1) srs.beamstemforce *= 2;
            }
          else error(ERR121);
          read_nextc();
          }
        else if (read_c == 'l')
          {
          int32_t y;
          read_nextc();
          if (read_expect_integer(&y, TRUE, TRUE)) yextra += y;
          }
        else if (read_c == 'm')
          {
          read_nextc();
          noteheadstyle |= nhf_smallhead;
          }
        else if (read_c == 'p')   /* sp is an ornament */
          {
          read_c = 's';
          read_i--;
          goto ACCORORN;
          }
        else
          {
          error_skip(ERR8, '\\', "su, sd, sw, sl, sm, or sp");
          goto ENDOPTIONS;
          }
        break;

        case 'x':       /* Cancel default accents and ornament */
        read_nextc();
        acflags &= ~srs.accentflags;
        ornament = or_unset;
        break;

        /* ----- Handle accents and ornaments ----- */

        default:
        ACCORORN:
        accororn = read_accororn('\\');            /* Unset will have given */
        if (accororn == or_unset) goto ENDOPTIONS; /* error and skipped to \ */

        /* Ignore unless the first note of a chord */

        if (pn_chordcount != 0) error(ERR116, "accents and ornaments");

        /* Values < 256 are ornament identifiers. Greater values are accent
        flag bits. Use a set of flag bits to ignore duplicate ornaments.
        Movement data is included within the ornament structure. */

        else if (accororn < 256)
          {
          if ((ornset & (1 << accororn)) == 0)
            {
            b_ornamentstr *p = mem_get_item(sizeof(b_ornamentstr), b_ornament);
            int32_t *px = &(p->x);
            uint8_t *pf = &(p->bflags);

            p->x = 0;            /* Default no horizontal movement */
            p->y = 0;            /* Default no vertical movement */
            p->bflags = 0;       /* Default no bracketing */
            p->troffset = 0;     /* Offset used only for trill */

            /* Tremolos have no horizontal movement or bracketing; arpeggios
            and spread have no bracketing, but can have arbitrary movement.
            Because tremolos use '/' as their character we have to do some
            fudging. */

            switch(accororn)
              {
              case or_trem2:
              case or_trem3:
              if (read_c == 'u' || read_c == 'd')
                {
                accororn--;     /* Reduce tremolo */
                read_c = '/';   /* Back up one character */
                read_i--;
                }
              /* Fall through */
              case or_trem1:
              if (ornament >= or_trem1 && ornament <= or_trem3)
                ornament = or_unset;  /* Cancel default tremolo */
              px = NULL;
              break;

              case or_arp:
              case or_arpu:
              case or_arpd:
              case or_spread:
              pf = NULL;
              break;

              /* For trills we have to figure out the trill offset. */

              case or_tr:
              case or_trsh:
              case or_trfl:
              case or_trnat:
                {
                int offset = (char_orig == 'b' || char_orig == 'e')? 2 : 4;
                int nextacc;

                switch(accororn)
                  {
                  case or_tr:
                  nextacc = read_baraccs[pitch_orig + offset];
                  break;

                  case or_trsh:
                  nextacc = 2;
                  break;

                  case or_trfl:
                  nextacc = -2;
                  break;

                  default:      /* Avoids compiler warning */
                  case or_nat:
                  nextacc = 0;
                  break;
                  }

                offset += nextacc - read_baraccs[pitch_orig];
                p->troffset = offset;
                }
              break;

              default:
              break;
              }

            p->ornament = accororn;
            ornopts(px, &(p->y), pf);
            ornset |= 1 << accororn;  /* Remember to avoid duplication */
            if (srs.lastnote_ornament == NULL) srs.lastnote_ornament = p;
            }
          }

        /* Accents are coded as a set of bits within the note structure, but
        movements (which are rare) require a preceding b_accentmovestr to be
        created. This holds the accent as a number, the same as the
        user-visible accent number. We can compute this by shifting - as this
        is a rare case it is not worth trying to be more efficient. */

        else
          {
          acflags |= accororn;
          if (read_c == '/')
            {
            b_accentmovestr *am =
              mem_get_item(sizeof(b_accentmovestr), b_accentmove);
            am->accent = 0;
            while (accororn <= af_staccato)
              {
              am->accent++;
              accororn <<= 1;
              }
            ornopts(&(am->x), &(am->y), &(am->bflags));
            }
          }
        }

      if ((acflags & (af_staccato|af_staccatiss)) ==
        (af_staccato|af_staccatiss)) error_skip(ERR91, '\\');
      read_sigc();
      }                                /* end options loop */

    ENDOPTIONS:                        /* some error states jump here */
    if (read_c == '\\') read_nextc();
    }                                  /* end options handling */

  /* Rests should not have ornaments (except fermata) or accidentals or certain
  options. */

  if (pitch == 0 && (
      ((ornset & ~(1 << or_ferm)) != 0 && (flags & nf_hidden) == 0) ||
      (flags & (nf_stemup|nf_headbra)) != 0 ||
      (noteheadstyle & nhf_smallhead) != 0 ||
      acc != ac_no ||
      pn_stemforce != 0 ||
      (acflags & af_accents) != 0))
    error(ERR122);

  /* A small notehead may not be specified for a grace note. */

  if (pn_notelength == 0 && (noteheadstyle & nhf_smallhead) != 0) error(ERR123);

  /* Accumulate bar length and check that chord notes are all same length. If
  not, and the current one is a crotchet (i.e. just a lower case letter) force
  it to be the same as the first note. Other lengths cause an error. Output an
  underlay and/or overlay syllable if one is pending on the first (or only)
  note of a chord. Allow the "#" syllable on a rest. Can't do this earlier
  because of grace notes. */

  if (pn_chordcount == 0)  /* First note of chord */
    {
    brs.barlength += pn_notelength;
    chordlength = pn_notelength;

    if (pn_notelength != 0)  /* Not grace note */
      {
      if (srs.pendulay != NULL)
        {
        uint32_t *s = srs.pendulay->string;
        if (pitch != 0 || (PCHAR(s[0]) == '#' && s[1] == 0))
          do_undoverlay(&srs.pendulay);
        }
      if (srs.pendolay != NULL)
        {
        uint32_t *s = srs.pendolay->string;
        if (pitch != 0 || (PCHAR(s[0]) == '#' && s[1] == 0))
          do_undoverlay(&srs.pendolay);
        }
      }
    }

  /* Second or subsequent note in a chord */

  else if (pn_notelength != chordlength)
    {
    if (pn_notetype == crotchet && (flags & nf_dotted) == 0)
      {
      pn_notelength = chordlength;
      pn_notetype = srs.firstnoteptr->notetype;
      flags |= srs.firstnoteptr->flags & nf_dotted;
      }
    else error(ERR124);
    }

  /* Allow automatic coupling flags through only if appropriately coupled, but
  allow explicit coupling at any time. */

  if (srs.couplestate == 0)
    {
    flags = (flags & ~nf_couple) | explicit_couple;
    flags &= (pitch >= P_3L)? ~nf_coupleD : ~nf_coupleU;
    }
  else
    {
    flags |= explicit_couple;
    flags &= (srs.couplestate > 0)? ~nf_coupleD : ~nf_coupleU;
    }

  /* Adjust the stem flag */

  if (pitch == 0 || ((masq == MASQ_UNSET)? pn_notetype : masq) < minim)
    flags &= ~nf_stem;

  /* Output any default ornament (can only happen on first note) */

  if (ornament != or_unset)
    {
    b_ornamentstr *p = mem_get_item(sizeof(b_ornamentstr), b_ornament);
    if (srs.lastnote_ornament == NULL) srs.lastnote_ornament = p;
    p->ornament = ornament;
    p->x = p->y = 0;
    p->bflags = 0;
    ornament = or_unset;
    }

  /* Now create a data block for the note or rest, saving the address of the
  first one in a chord for exact duplications ('x') and the first one in a
  non-p-duplicated chord for subsequent 'p' duplications, except in the case of
  above/below accidentals. */

  noteptr = mem_get_item(sizeof(b_notestr), b_chord);

  if (pn_chordcount == 0)  /* Rest, single, or first note in a chord */
    {
    noteptr->type = b_note;
    srs.firstnoteptr = noteptr;
    if (pitch != 0) srs.lastnoteptr = noteptr;
    }

  /* Insert the data for this note, and count it (within a chord). */

  noteptr->notetype = pn_notetype;
  noteptr->masq = (uint8_t)masq;
  noteptr->acc = acc;
  noteptr->acc_orig = acc_orig;
  noteptr->char_orig = char_orig;
  noteptr->spitch = pitch;
  noteptr->flags = flags;
  noteptr->acflags = acflags;
  noteptr->length = pn_notelength;
  noteptr->yextra = yextra;
  noteptr->accleft = accleft;
  noteptr->abspitch = abspitch;
  noteptr->noteheadstyle = noteheadstyle;
  pn_chordcount++;

  /* If we are duplicating as a result of 'p', pn_inchord will be 0 because
  'p' is not recognized after '('. We must now copy the remaining notes of the
  original chord, if any, replacing the note type and adjusting the visibility
  of accidentals where necessary. We must also set the relevant tiedata values.
  */

  if (duplicating && (old_lastnoteptr->flags & nf_chord) != 0)
    {
    int i = 1;
    noteptr->flags |= nf_chord;  /* Mark original as start of chord */

    for (b_notestr *next = (b_notestr *)(old_lastnoteptr->next);
         next->type == b_chord;
         i++, next = (b_notestr *)(next->next))
      {
      noteptr = mem_duplicate_item(next, sizeof(b_notestr));
      noteptr->notetype = pn_notetype;
      noteptr->acflags &= ~af_accents;
      noteptr->length = pn_notelength;
      noteptr->flags = (noteptr->flags & ~nf_dotted & ~nf_stem) |
        (flags & (nf_dotted | nf_stem));

      if (noteptr->spitch > pn_maxpitch) pn_maxpitch = noteptr->spitch;
      if (noteptr->spitch < pn_minpitch) pn_minpitch = noteptr->spitch;

      /* At bar start, or after only tied duplicates, retain the accidental;
      otherwise don't. */

      if (pn_onlytieddup && !srs.lastwastied)
        {
        noteptr->flags &= ~nf_accinvis;
        }
      else
        {
        noteptr->flags |= nf_accinvis;
        }

      srs.pitchtotal += noteptr->abspitch;
      srs.pitchcount++;

      pn_chordcount++;
      pn_tiedata[i] = read_tiedata[i];
      }
    }

  /* Check for end of chord. */

  read_sigc();
  if (pn_inchord == 0 || read_c == ENDFILE) break;
  if (read_c == ')')
    {
    read_nextc();
    break;
    }
  }   /* Loop to read the remaining notes in a chord. */

/* We have now reached the end of a chord, having output all the notes therein
contiguously, with any ornaments or masquerade items preceding. The remaining
code is put into a separate function so it can also be called from the note-
copying code. */

post_note(TRUE);
}

/* End of pmw_read_note.c */
