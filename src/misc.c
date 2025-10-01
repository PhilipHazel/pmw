/*************************************************
*       PMW miscellaneous utility functions      *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: January 2021 */
/* This file last modified: October 2025 */

#include "pmw.h"


/* Tables of accidental height/depths */

                             /* -    %    #-    #   ##   $-     $    $$ */
static int32_t accdowntab[] = { 0, 4000, 4000, 4000, 0,    0,    0,    0 };
static int32_t accuptab[]   = { 0, 4000, 4000, 4000, 0, 4000, 4000, 4000 };


/* Tables used by the ybound function */

static int accboundtable[] = { 4000, 0, 0, 4000, 0, -4000, -4000, 0 };


static int32_t resttable[] = {
  12000, 12000, 10000, 16000, 12000, 12000, 16000, 16000, 16000,  16000,
   8000, 10000,  8000,  2000,  4000,     0,     0, -4000, -8000, -12000 };



/*************************************************
*           Get range from bit map               *
*************************************************/

BOOL
misc_get_range(uint64_t bits, usint x, usint *first, usint *last)
{
uint64_t m;

if (x > MAX_STAVE) return FALSE;
m = 1 << x;

for (;;)
  {
  if ((bits & m) != 0)
    {
    usint y = x;
    uint64_t n = m;
    for (;;)
      {
      if (y == MAX_STAVE) break;
      if ((bits & (n << 1)) == 0) break;
      y++;
      n = n << 1;
      }
    *first = x;
    *last = y;
    return TRUE;
    }

  if (x == MAX_STAVE) break;
  x++;
  m = m << 1;
  }

return FALSE;
}



/*************************************************
*       Convert character value to UTF-8         *
*************************************************/

/* This function takes an integer value in the range 0 - 0x7fffffff
and encodes it as a UTF-8 character in 0 to 6 bytes.

Arguments:
  cvalue     the character value
  buffer     pointer to buffer for result - at least 6 bytes long

Returns:     number of characters placed in the buffer
*/

int
misc_ord2utf8(uint32_t cvalue, uschar *buffer)
{
register int i, j;
for (i = 0; i < 6; i++) if (cvalue <= utf8_table1[i]) break;
buffer += i;
for (j = i; j > 0; j--)
  {
  *buffer-- = 0x80 | (cvalue & 0x3f);
  cvalue >>= 6;
  }
*buffer = utf8_table2[i] | cvalue;
return i + 1;
}



/*************************************************
*       Sprintf and convert to PMW string        *
*************************************************/

/* This function runs sprintf() and then converts the result to a 32-bit PMW
string.

Arguments:
  buffer     for the output
  font       the font to use
  format     sprintf format
  ...        sprintf arguments

Returns:     nothing
*/

void
misc_psprintf(uint32_t *buffer, int font, const char *format, ...)
{
va_list ap;
char temp[256];
char *p = temp;

va_start(ap, format);
vsprintf(temp, format, ap);
va_end(ap);

font <<= 24;
while (*p != 0) *buffer++ = *p++ | font;
*buffer = 0;
}



/*************************************************
*          Return width of key signature         *
*************************************************/

/* This function checks for a special string for the key signature; otherwise
it finds the number of accidentals in a key signature, and returns an
appropriate printing width. In both cases this is the width for a 10-point
stave (appropriate adjustment happens in the calling function when needed).

Arguments:
  key       the key signature ORed with key_reset if restting with naturals
  clef      the clef - used only to check for a printkey setting

Returns:    the printing width
*/

int32_t
misc_keywidth(uint32_t key, uint16_t clef)
{
pkeystr *pk;
int32_t width;
BOOL reset = (key & key_reset) != 0;

key &= ~key_reset;
for (pk = main_printkey; pk != NULL; pk = pk->next)
  {
  if (key == pk->key && clef == pk->clef && pk->movt_number <= curmovt->number)
    break;
  }

/* Printkey applies */

if (pk != NULL)
  {
  fontinststr fdata = { NULL, 10000, 0 };
  width = string_width((reset? pk->cstring : pk->string), &fdata, NULL);
  }

/* Scan the key signature table and add up the widths of all the accidentals.
For a key reset all are replaced by naturals. When half sharps are in Egyptian
style they have a special (narrower) width. */

else
  {
  uint8_t *k;
  int kk = ac_nt;
  width = 0;
  for (k = keysigtable[key]; *k != ks_end; k++)
    {
    if (!reset)
      {
      kk = *k >> 4;
      if (kk == ac_hs && curmovt->halfsharpstyle == 0) kk = ac_no;
      }
    width += curmovt->accspacing[kk];
    }
  }

DEBUG(D_stringwidth)
  eprintf("key=%d clef=%d width=%s%s\n", key, clef, sff(width),
    reset? " reset" : "");

return width;
}



/*************************************************
*          Return width of time signature        *
*************************************************/

/* This function sets up the appropriate strings for a time signature and then
returns the width of the longer one.

Argument:    the time signature
Returns:     the printing width
*/

int32_t
misc_timewidth(uint32_t ts)
{
ptimestr *pt;
uint8_t sizetop, sizebot;
uint32_t *topstring, *botstring;
int32_t yieldtop, yieldbot;
fontinststr *fdvector;

/* If not printing time signatures, return zero width */

if (!MFLAG(mf_showtime)) return 0;

/* First see if this time signature has special strings specified for its
printing. The specification must have happened in this movement or earlier to
be applicable. */

for (pt = main_printtime; pt != NULL; pt = pt->next)
  if (pt->time == ts && pt->movt_number <= curmovt->number) break;

/* If found a special case, get strings and sizes from it */

if (pt != NULL)
  {
  sizetop = pt->sizetop;
  sizebot = pt->sizebot;
  topstring = pt->top;
  botstring = pt->bot;
  }

/* Default printing for this time signature. First mask off the multiplier,
then check for the special cases of C and A. */

else
  {
  uschar temp[16];

  ts &= 0xffff;

  /* C and A have a known width */

  if (ts == time_common || ts == time_cut) return 7500;

  /* Non-special case - set up numerator and denominator, in the time signature
  font. First format as chars, then convert to PMW strings. Note that
  string_pmw() does allow 2 simultaneous instances. */

  sprintf(CS temp, "%d", ts >> 8);
  topstring = string_pmw(temp, font_rm);
  sprintf(CS temp, "%d", ts & 255);
  botstring = string_pmw(temp, font_rm);

  sizebot = sizetop = ff_offset_ts;
  }

/* We now have in topstring and botstring two strings to print. The yield
is the greater of the two lengths, except when only the numerator
is to be printed. */

fdvector = curmovt->fontsizes->fontsize_text;
yieldtop = string_width(topstring, &(fdvector[sizetop]), NULL);
if (!MFLAG(mf_showtimebase)) return yieldtop;
yieldbot = string_width(botstring, &(fdvector[sizebot]), NULL);
return (yieldtop > yieldbot)? yieldtop : yieldbot;
}



/*************************************************
*      Find next note in a bar (usually)         *
*************************************************/

/* Always advance by at least one item. The initial item need not be a note.
The first ornament encountered is saved in main_nextnoteornament. When beaming
over a barline is happening, look for the next note in the next bar. There will
always be another bar. The beam_overbeam variable is TRUE only in this
situation.

Arguments:  pointer to the current item
Returns:    pointer to the next note item or NULL if there isn't one
*/

b_notestr *
misc_nextnote(void *p)
{
bstr *pp = (bstr *)p;
main_nextnoteornament = NULL;

for (;;)
  {
  pp = pp->next;
  if (pp == NULL || pp->type == b_note) break;
  if (pp->type == b_ornament && main_nextnoteornament == NULL)
    main_nextnoteornament = (b_ornamentstr *)pp;
  else if (pp->type == b_overbeam && beam_overbeam)
    pp = (bstr *)(((b_overbeamstr *)pp)->nextbar);
  else if (pp->type == b_barline) return NULL;
  }

return (b_notestr *)pp;
}



/*************************************************
*      Make copy of continued data structures    *
*************************************************/

/* This is called at the start of formatting or outputting a system. It
remembers the parameters that are current at that point. At the start of a new
system, incslur is passed as TRUE, causing the section numbers of slurs to be
increased. (At the start of the first system, it is FALSE.)

Arguments:
  q          pointer to destination vector
  p          pointer to source vector
  count      the highest stave number that we need to save (can be 0)
  incslur    when TRUE, increment the section number for each slur

Returns:     nothing
*/

void
misc_copycontstr(contstr *q, contstr *p, int count, BOOL incslur)
{
/* Copy the whole thing, then, for each element in the vector (i.e. for each
stave), copy any chained-on data blocks. */

memcpy(q, p, (count+1)*sizeof(contstr));

for (int i = 0; i <= count; i++)
  {
  nbarstr *nb = p->nbar;
  nbarstr **nbprev = &(q->nbar);
  slurstr *s = p->slurs;
  slurstr **sprev = &(q->slurs);
  uolaystr *u = p->uolay;
  uolaystr **uoprev = &(q->uolay);

  /* Copy the hairpin structure (there is only ever one per stave) */

  if (p->hairpin != NULL)
    {
    q->hairpin = mem_get_cached((void **)(&main_freehairpinstr),
      sizeof(hairpinstr));
    *(q->hairpin) = *(p->hairpin);
    }

  /* Copy the overbeam structure (ditto) */

  if (p->overbeam != NULL)
    {
    q->overbeam = mem_get_cached((void **)(&main_freeobeamstr),
      sizeof(obeamstr));
    *(q->overbeam) = *(p->overbeam);
    }

  /* Copy the nth time bar structures */

  while (nb != NULL)
    {
    nbarstr *nbb = mem_get_cached((void **)(&main_freenbarblocks),
      sizeof(nbarstr));
    *nbb = *nb;
    *nbprev = nbb;
    nbprev = &(nbb->next);
    nb = nb->next;
    }
  *nbprev = NULL;

  /* Copy the chain of slur structures. Each one may contain a sub-chain of gap
  structures. Increment the section number for each slur when requested - this
  happens when moving to a new system. */

  while (s != NULL)
    {
    slurstr *ss = mem_get_cached((void **)(&main_freeslurblocks),
      sizeof(slurstr));
    gapstr *g = s->gaps;
    gapstr **gprev = &(ss->gaps);
    if (incslur) s->section += 1;
    *ss = *s;

    while (g != NULL)
      {
      gapstr *gg = mem_get_cached((void **)(&main_freegapblocks),
        sizeof(gapstr));
      *gg = *g;
      *gprev = gg;
      gprev = &(gg->next);
      g = g->next;
      }
    *gprev = NULL;
    *sprev = ss;
    sprev = &(ss->next);
    s = s->next;
    }
  *sprev = NULL;

  /* Copy the chain of underlay/overlay structures */

  while (u != NULL)
    {
    uolaystr *uu = mem_get_cached((void **)(&main_freeuolayblocks),
      sizeof(uolaystr));
    *uu = *u;
    *uoprev = uu;
    uoprev = &(uu->next);
    u = u->next;
    }
  *uoprev = NULL;

  /* Advance to the next pair of vector entries */

  p++;
  q++;
  }
}



/*************************************************
*         Tidy a continued data structure        *
*************************************************/

/* This is called at the end of outputting a system. Auxiliary blocks that have
been attached are put on their free chains.

Arguments:
  p         pointer to the contstr vector
  count     highest stave saved (may be 0)

Returns:    nothing
*/

void
misc_tidycontstr(contstr *p, int count)
{
for (int i = 0; i <= count; p++, i++)
  {
  if (p->hairpin != NULL) mem_free_cached((void **)(&main_freehairpinstr),
    p->hairpin);
  if (p->overbeam != NULL) mem_free_cached((void **)(&main_freeobeamstr),
    p->overbeam);

  for (nbarstr *nb = p->nbar; nb != NULL; )
    {
    nbarstr *nbb = nb->next;
    mem_free_cached((void **)(&main_freenbarblocks), nb);
    nb = nbb;
    }

  for (slurstr *s = p->slurs; s != NULL; )
    {
    slurstr *ss = s->next;
    for (gapstr *g = s->gaps; g != NULL; )
      {
      gapstr *gg = g->next;
      mem_free_cached((void **)(&main_freegapblocks), g);
      g = gg;
      }
    mem_free_cached((void **)(&main_freeslurblocks), s);
    s = ss;
    }

  for (uolaystr *u = p->uolay; u != NULL; )
    {
    uolaystr *uu = u->next;
    mem_free_cached((void **)(&main_freeuolayblocks), u);
    u = uu;
    }
  }
}



/*************************************************
*          Deal with start of nth time bar       *
*************************************************/

/* This function remembers the data for the start of an nth-time bar. It is
called while paginating and also while actually outputting. Multiple nth-time
bar blocks may be in use; the active ones are chained off bar_cont->nbar.

Arguments:
  nb           the data for the start
  x            the x coordinate of the start
  miny         minimum y coordinate - used to align multiple such markings

Returns:       nothing
*/

void
misc_startnbar(b_nbarstr *nb, int32_t x, int32_t miny)
{
nbarstr *nbb = mem_get_cached((void **)(&main_freenbarblocks), sizeof(nbarstr));
nbb->next = NULL;
nbb->nbar = nb;
nbb->x = x;
nbb->maxy = -INT32_MAX;
nbb->miny = miny;
if (bar_cont->nbar == NULL) bar_cont->nbar = nbb; else
  {
  nbarstr *bb;
  for (bb = bar_cont->nbar; bb->next != NULL; bb = bb->next);
  bb->next = nbb;
  }
}



/*************************************************
*         Free blocks for nth time markings      *
*************************************************/

/* This puts all the remembered data (possibly from several numbers) onto the
free chain of nbar blocks.

Arguments:  none
Returns:    nothing
*/

void
misc_freenbar(void)
{
nbarstr *nb;
for (nb = bar_cont->nbar; nb->next != NULL; nb = nb->next);
nb->next = main_freenbarblocks;
main_freenbarblocks = bar_cont->nbar;
bar_cont->nbar = NULL;
}



/*************************************************
*          Common actions on cont data           *
*************************************************/

/* Certain items require the same actions when encountered in the advance scan
of each system by setcont() as when encountered while actually outputting a
system. These actions are handled in this function.

Argument:    pointer to the item
Returns:     nothing
*/

void
misc_commoncont(bstr *p)
{
switch(p->type)
  {
  /* Adjust bowing flags */

  case b_bowing:
  bar_cont->flags &= ~cf_bowingabove;
  if (((b_bowingstr *)p)->value) bar_cont->flags |= cf_bowingabove;
  break;

  /* Switch notes on/off */

  case b_notes:
  bar_cont->flags &= ~cf_notes;
  if (((b_notesstr *)p)->value) bar_cont->flags |= cf_notes;
  break;

  /* Switch triplets on/off */

  case b_tripsw:
  bar_cont->flags &= ~cf_triplets;
  if (((b_tripswstr *)p)->value) bar_cont->flags |= cf_triplets;
  break;

  /* Set slope for next beam */

  case b_beamslope:
  beam_forceslope = ((b_beamslopestr *)p)->value;
  break;

  /* Values that have not been dealt with can be ignored, but check for a
  wholly ridiculous value. */

  default:
  if (p->type >= b_baditem) error(ERR128, p->type);
  break;
  }
}



/**************************************************
* Compute bounding y value, with accents and ties *
**************************************************/

/* The parameters of the note are in the n_* variables. The yield is a y value
relative to the staff base, with positive values going upwards. At the notehead
end of a note, it is one point away from the notehead. We calculate in
stave-points. The accentflag variable request inclusion of *all* accents;
otherwise include only those that go inside the stave (staccat(issim)o, ring,
bar). This call is used when outputting those that go outside the stave.

Arguments:
  below       TRUE for the bottom bound, FALSE for the top bound
  tie         a tiestr for the note, or NULL if not tied
  accflag     TRUE if there's an accidental
  accentflag  TRUE if there are dynamics

Returns:      the y value
*/

int32_t
misc_ybound(BOOL below, b_tiestr *tie, BOOL accflag, BOOL accentflag)
{
int32_t yield;
int32_t extra = 0;
int32_t accextra = 0;
uint32_t flags = n_flags;
uint32_t acflags = n_acflags;

/* If this is a rest, the only parameter of interest is the rest level. */

if (n_pitch == 0)
  return n_restlevel + resttable[n_notetype + (below? NOTETYPE_COUNT:0)];

/* Deal with a note; first calculate additional length for stem, if any. */

if ((flags & nf_stem) != 0)
  {
  extra = mac_muldiv(n_stemlength + 12000, n_fontsize, 10000);
  if (n_beamed)
    {
    extra += 1000;

    /* Extra for all but the first note of steep downward beams when stems
    are up. */

    if (n_upflag && beam_slope < 0 && n_lastnote != beam_first)
      extra += 5*abs(beam_slope);
    }
  }

/* The basic value takes account of the appropriate pitch and, if relevant, any
accidental. We remember, in accextra, additional space added here. It will be
taken away if there is subsequent space added for an accent. */

if (below)
  {
  if (n_upflag)
    {
    extra = accflag? accdowntab[n_lastacc] : 0;
    accextra = -extra;
    }
  else
    {
    if (accflag && extra == 0)
      {
      extra = accdowntab[n_firstacc];
      accextra = -extra;
      }
    }
  if (extra == 0)
    {
    extra = 1000;
    accextra = -extra;
    }
  yield = 1000*(n_minpitch - 260)/2 - extra;
  }

else
  {
  if (n_upflag)
    {
    if (accflag && extra == 0)
      {
      extra = accuptab[n_firstacc];
      accextra = extra;
      }
    }
  else
    {
    extra = accflag? accuptab[n_lastacc] : 0;
    accextra = extra;
    }
  if (extra == 0)
    {
    extra = 1000;
    accextra = extra;
    }
  yield = 1000*(n_maxpitch - 252)/2 + extra;
  }

/* Allow for ties */

if (tie != NULL)
  {
  if (below)
    {
    if (tie->belowcount > 0 && (n_upflag || (flags & nf_stem) == 0))
      yield -= 4000;
    }
  else
    {
    if (tie->abovecount > 0 && (!n_upflag || (flags & nf_stem) == 0))
      yield += 4000;
    }
  }


/* Allow for accents. First of all, get rid of bowing marks if they are not
relevant. */

if (((bar_cont->flags & cf_bowingabove) != 0) == below)
  acflags &= ~(af_up | af_down);

if ((acflags & af_accents) != 0 && (accentflag || (acflags & af_accinside) != 0))
  {
  int oppflag;
  int32_t accentextra;

  if ((acflags & (af_accents - af_up - af_down)) != 0)
    {
    oppflag = ((acflags & af_opposite) == 0)? 0:1;
    }
  else
    {
    oppflag = ((bar_cont->flags & cf_bowingabove) == 0)?
      (n_upflag? 0:1) : (n_upflag? 1:0);
    }

  accentextra = accboundtable[oppflag + (n_upflag? 2:0) + (below? 4:0)];

  /* If adding space for an accent, retract space for an accidental */

  if (accentextra != 0) yield += accentextra - accextra;

  /* That's all if no relevant accent, or the accent falls inside the staff;
  otherwise make sure the pitch is suitably outside the staff. */

  if (accentextra != 0 && accentflag && (acflags & af_accoutside) != 0)
    {
    yield += accentextra;   /* these are bigger accents */

    if (below)
      {
      if (yield > -10000) yield = -10000;
      }
    else if (yield < 22000) yield = 22000;
    }
  }

return yield;
}


/* End of misc.c */
