/*************************************************
*             PMW pagination functions           *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: April 2021 */
/* This file last modified: May 2022 */

#include "pmw.h"


/* State values used to control what happens in the main loop. */

enum { page_state_newmovt, page_state_newsystem,
       page_state_insystem, page_state_donesystem,
       page_state_donemovt };

/* Initializing values for page structures */

static pagestr init_curpage = {
  NULL,    /* next */
  NULL,    /* sysblocks */
  NULL,    /* footing */
  0,       /* topspace */
  0,       /* spaceleft */
  0,       /* overrun */
  0        /* number */
};

/* Initializing values for cont structures at movement start. */

static contstr init_cont = {
  NULL,         /* slurs */
  NULL,         /* hairpin */
  NULL,         /* nbar */
  NULL,         /* overbeam */
  NULL,         /* tie */
  NULL,         /* ulay */
  0,            /* tiex */
  0,            /* time - reset from curmovt */
  nh_normal,    /* noteheadstyle */
  cf_default,   /* flags */
  clef_treble,  /* clef */
  0             /* key - reset from curmovt */
};

/* Tables of accidental heights/depths */
                             /* -   %     #-    #   ##   $-      $    $$ */
static int32_t accdowntab[] = { 0, 4000, 4000, 4000, 0,    0,     0,    0 };
static int32_t accuptab[]   = { 0, 4000, 4000, 4000, 0, 4000,  4000, 4000 };

/* Table for adjusting overlay and underlay levels, depending on stem direction
and whether a dynamic is on that side or not. */

static int32_t accboundtable[] = { 4000, 0, 0, 4000, 0, -4000, -4000, 0 };

/* Widths of long rest bars, in whole points. First value is for a long rest
sign; the next are for the coded rest signs (1 is not relevant, then 2-8). */

static int32_t longrest_barwidths[] = { 50, 0, 30, 35, 30, 35, 35, 45, 40 };

/* Flags used while creating the position table, for remembering what precedes
a note */

#define xf_caesura    0x0001u
#define xf_comma      0x0002u
#define xf_tick       0x0004u
#define xf_dotbar     0x0008u
#define xf_rrepeat    0x0010u
#define xf_clef       0x0020u
#define xf_lrepeat    0x0040u
#define xf_keytime    0x0080u
#define xf_grace      0x0100



/*************************************************
*              Local static variables            *
*************************************************/

/* Static variables are used for communication between the main paginating
function and various subroutines in this module. Three versions of pagedatastr
are kept: prevdata points to the version that was current after accepting the
previous bar; accepteddata points to the current structure after accepting a
bar; nextdata points to the values that have changed as a result of measuring
the bar that is being checked. We use pointers so that they can easily be
swapped round. */

static pagedatastr  data1;
static pagedatastr  data2;
static pagedatastr  data3;

static pagedatastr *accepteddata = &data1;
static pagedatastr *nextdata = &data2;
static pagedatastr *prevdata = &data3;

/* The pl_barnumber variable keeps track of where we are in the pagination.
However, we often have to process extra bars when there are multiple bars rest,
so the processing is done using the global curbarnumber, which shows up in
error messages. At other times, curbarnumber == pl_barnumber. */

static int32_t      pl_accexistedavail;
static uint64_t     pl_allstavebits;
static int32_t      pl_barlinewidth;
static int32_t      pl_barnumber;
static BOOL         pl_barstartrepeat;
static uint32_t     pl_botmargin;
static barposstr   *pl_bp;
static usint        pl_countsystems;
static int32_t      pl_footnotespacing;
static uint8_t      pl_justify;
static uint8_t      pl_justifyLR;
static BOOL         pl_lastenddouble;
static BOOL         pl_lastendwide;
static headstr     *pl_lastnewfootnote;
static sysblock    *pl_lastsystem;
static uint32_t     pl_manyrest;
static int32_t      pl_newfootnotedepth;
static headstr     *pl_newfootnotes;
static BOOL         pl_newpagewanted;
static int32_t      pl_olaysize;
static int32_t     *pl_olevel;
static int32_t     *pl_olhere;
static headstr     *pl_pagefooting;
static int32_t      pl_pagefootnotedepth;
static headstr     *pl_pagefootnotes;
static workposstr  *pl_posptr;
static workposstr  *pl_postable;
static uint32_t     pl_sgnext;
static uint64_t     pl_showtimes;
static int32_t     *pl_ssehere;
static int32_t     *pl_ssenext;
static int32_t     *pl_ssnext;
static BOOL         pl_startlinebar;
static uint64_t     pl_stavemap;
static int32_t      pl_stretchd;
static int32_t      pl_stretchn;
static sysblock    *pl_sysblock;
static uint8_t      pl_sysclef[MAX_STAVE + 1];
static sysblock   **pl_sysprevptr;
static int32_t      pl_sys_botmargin;
static int32_t      pl_sys_justify;
static int32_t      pl_sys_topmargin;
static int32_t      pl_topmargin;
static int32_t      pl_ulaysize;
static int32_t     *pl_ulevel;
static int32_t     *pl_ulhere;
static BOOL         pl_warnkey;
static BOOL         pl_warntime;
static int32_t      pl_xxwidth;



/*************************************************
*       Update times/keys for start of line      *
*************************************************/

/* This function is called at the start of a system, to see if the first bar
contains clef, key, or time items that should be transferred into the basic
setting for the bar.

Arguments: none
Returns:   nothing
*/

static void
setsignatures(void)
{
int stave;

for (stave = 0; stave <= curmovt->laststave; stave++)
  {
  stavestr *s;
  bstr *p;
  BOOL hadclef, hadkey, hadtime;

  if ((curmovt->select_staves & (1 << stave)) == 0) continue; /* Unselected */
  s = curmovt->stavetable[stave];

  hadclef = hadkey = hadtime = FALSE;

  /* Scan bar items until the first note, left repeat, or end */

  for (p = (bstr *)(s->barindex[pl_barnumber]); p != NULL; p = p->next)
    {
    switch (p->type)
      {
      case b_clef:
      if (!hadclef)
        {
        b_clefstr *c = (b_clefstr *)p;
        (pl_sysblock->cont[stave]).clef = c->clef;
        c->suppress = hadclef = TRUE;
        }
      break;

      case b_key:
      if (!hadkey || pl_sysblock->cont[stave].key == key_C ||
                     pl_sysblock->cont[stave].key == key_Am)
        {
        b_keystr *k = (b_keystr *)p;
        if ((k->key & key_reset) == 0 || !k->warn)
          {
          (pl_sysblock->cont[stave]).key = k->key;
          hadkey = TRUE;
          }
        k->suppress = TRUE;
        }
      break;

      case b_time:
      if (!hadtime)
        {
        b_timestr *t = (b_timestr *)p;
        (pl_sysblock->cont[stave]).time = t->time;
        t->suppress = hadtime = TRUE;
        if (!t->assume) mac_setbit(pl_showtimes, stave);
        }
      break;

      case b_barline:
      case b_note:
      case b_lrepeat:
      goto NEXTSTAVE;

      default:
      break;
      }
    }

  NEXTSTAVE: continue;
  }
}


/*************************************************
*         Calculate typographic width            *
*************************************************/

/* This function calculates the magnified typographic width for a note of
given length, with given flags. For a chord, the flags should be the 'or' of
those for all the notes.

Arguments:
  used       the basic minimum width for the note (unmagnified)
  length     the note's musical length (identifies the note)
  flags      the note's flags

Returns:     the valued of "used" plus the calculated width
*/

static int32_t
pos_typewidth(int32_t used, int32_t length, uint32_t flags)
{
/* Invisible notes use nothing, breves and semibreves need some extra, as do
freestanding upquavers and shorter, to allow for the tail on the right. */

if ((flags & nf_hidden) != 0) return 0;

if (length >= len_breve) used += 3300;
  else if (length >= len_semibreve) used += 800;
    else if ((flags & nf_fuq) != 0) used += 5000;

/* Extra width for chord with inverted note and stem up, and even more if
dotted. */

if ((flags & (nf_stemup|nf_invert)) == (nf_stemup|nf_invert) && used < 12400)
  {
  used = 12400;
  if ((flags & (nf_dot | nf_plus)) != 0) used += 2000;
  }

/* Extra width for dots or plus */

if ((flags & nf_plus) != 0) used += 8000;
  else if ((flags & nf_dot) != 0)
    {
    used += 3000;
    if ((flags & nf_dot2) != 0) used += 3500;
    }

/* Allow for magnification */

return mac_muldiv(used, curmovt->stavesizes[curstave], 1000);
}



/*************************************************
*          Calculate horizontal width            *
*************************************************/

/* This function calculates the magnified basic horizontal width required for
a note of a given musical length.

Argument:   the note's musical length
Returns:    the magnified width
*/

static int32_t
pos_notewidth(int32_t length)
{
int32_t thislength = len_breve;
int32_t width;
usint type = breve;

while (length < thislength)
  {
  type++;
  thislength /= 2;
  }

/* Notes shorter than the shortest note type can be created only by triplets or
the like. These are rare; ensure something non-crashy happens. NOTE: the
tuplets test contains some of these. If ever shorter notes are added, causing
the width here to become non-zero, the spacing in that test will change. */

if (type >= NOTETYPE_COUNT) { width = 0; type = NOTETYPE_COUNT; }
  else width = nextdata->note_spacing[type];

/* If we don't have an exact note length, first check for a single or double
dot; otherwise it's a note in an irregular group. */

if (length != thislength)
  {
  int32_t extra = length - thislength;
  if (extra == thislength/2)
    {
    width = mac_muldiv(width, curmovt->dotspacefactor, 1000);
    }
  else if (extra == (3*thislength)/4)
    width = mac_muldiv(width, 3*curmovt->dotspacefactor - 1000, 2000);

  /* Breve tuplets are rarer than hen's teeth, so fudge the "next note" spacing
  to make things work. We then set the width as a pro rata amount between the
  relevant two kinds of note. */

  else
    {
    int32_t nextup = (type == breve)?
      (3*width)/2 : nextdata->note_spacing[type-1];
    width += mac_muldiv(nextup - width, extra, thislength);
    }
  }

/* When we are re-evaluating bar widths because a system is being squashed, it
works best if the notewidths themselves are squashed at this point. The layout
of the underlay can then be better computed. */

if (pl_stretchn < pl_stretchd)
  width = mac_muldiv(width, pl_stretchn, pl_stretchd);

/* Now apply the magnification factor and return */

return mac_muldiv(width, curmovt->stavesizes[curstave], 1000);
}



/*************************************************
*      Make space for non-note or grace note     *
*************************************************/

/* The musical offset supplied is always negative. We have to check whether a
position for the auxiliary item already exists, and if so, to adjust its place
if necessary. The yield is the new value for this position. The "here" flag
determines whether the item can be put to the left of the current position if
there is space.

Arguments:
  previous     pointer to postable entry for the previous position, or NULL
  this         pointer to current postable entry
  auxoffset    offset for the non-note item (always negative)
  Rwidth       unmagnified width for the item
  used         space used by previous note, if any
  here         see above

Returns:       pointer to the postable entry after "this"
*/

static workposstr *
pos_insertXpos(workposstr *previous, workposstr *this, int auxoffset,
  int32_t Rwidth, int used, BOOL here)
{
workposstr *aux = NULL;
workposstr *insertpoint, *t;

Rwidth = mac_muldiv(Rwidth, curmovt->stavesizes[curstave], 1000);

/* See if any auxiliaries already exist; find the leftmost. */

for (t = this - 1; t >= pl_postable && t->auxid != 0; t--) aux = t;

/* If there are no auxiliaries, or the previously existing auxiliaries are to
the right of this one, the space available is the sum from the previous
position (if any) to this position. We can insert a new position with an
appropriate offset in both cases. */

if (aux == NULL || auxoffset < aux->auxid)
  {
  insertpoint = (aux == NULL)? this : aux;
  memmove(insertpoint + 1, insertpoint, (pl_posptr + 1 - insertpoint) *
    sizeof(workposstr));   /* Move up to make space */
  pl_posptr++;             /* Increase the end */
  this++;                  /* For yielding */

  aux = insertpoint++;
  aux->moff = this->moff + auxoffset;
  aux->auxid = auxoffset;              /* identifies the aux */
  aux->auxstaves = 1u << curstave;     /* identifies which stave(s) */
  insertpoint->space = 0;              /* leave space on the new insert */

  if (here) aux->xoff = insertpoint->xoff; else
    {
    int32_t avail = aux->xoff;
    if (previous != NULL)
      {
      t = previous + 1;
      while (t < aux) avail += (t++)->xoff;
      }
    avail -= Rwidth + used;
    if (avail < 0) insertpoint->xoff -= avail;
    aux->xoff = insertpoint->xoff - Rwidth;
    }

  insertpoint->xoff = Rwidth;
  }

/* Either there are existing auxiliaries to the left of this one, or this one
already exists. */

else
  {
  for (insertpoint = aux; insertpoint != this; insertpoint++)
    if (insertpoint->auxid >= auxoffset) break;

  /* If this auxiliary already exists, test that there is enough space between
  it and the previous note. This works because we process the auxiliaries for
  one note from right to left. In the case of accidentals, we must adjust the
  space by the difference between this accidental's requirements and whatever
  is already there. There is one complicated case -- see below. */

  if (insertpoint->auxid == auxoffset)
    {
    int32_t avail = 0;
    workposstr *next = insertpoint + 1;

    t = (previous == NULL)? pl_postable : previous + 1;
    while (t <= insertpoint) avail += (t++)->xoff;
    avail -= used;

    /* For accidentals, all we need to do is check that there is enough space
    on the left, since accidental printing doesn't actually use a calculated
    position - chords need several positions, for a start. However, we do need
    to remember how much space was available in case some of it gets taken up
    by an auxiliary to the left (which is handled subsequently). */

   if (auxoffset == posx_acc)
      {
      avail += next->xoff - Rwidth;
      pl_accexistedavail = avail;
      }

    /* For non-accidentals, check that there is enough space to the right,
    and increase if necessary. If the next thing is an accidental whose
    original position was set in a previous stave, we must adjust in case it
    intruded into the space where this item is going. */

    else
      {
      if (Rwidth > next->xoff) next->xoff = Rwidth;
      if (next->moff == posx_acc && pl_accexistedavail > 0 &&
          Rwidth > pl_accexistedavail)
        next->xoff += Rwidth - pl_accexistedavail;
      }

    /* If there is insufficient space, move *all* the auxiliaries
    to the right. */

    if (avail < 0) aux->xoff -= avail;

    /* Remember which staves */

    mac_setbit(insertpoint->auxstaves, curstave);
    }

  /* If this auxiliary does not exist, we must insert it. See if there is space
  on *all* staves between the first auxiliary and the previous note position.
  If there is, we can move those auxiliaries to the left of this one to the
  left. */

  else
    {
    workposstr *new = insertpoint;

    memmove(insertpoint + 1, insertpoint, (pl_posptr + 1 - insertpoint) *
      sizeof(workposstr));   /* Move up to make space */
    pl_posptr++;             /* List is one longer now */
    this++;                  /* For yielding */
    insertpoint++;

    new->space = 0;
    new->moff = this->moff + auxoffset;
    new->auxid = auxoffset;              /* identifies the aux */
    new->auxstaves = 1u << curstave;     /* identifies which stave(s) */

    /* Distance from previous aux is its standard distance */

    new->xoff = insertpoint->xoff;
    insertpoint->xoff = Rwidth;

    if (!here)
      {
      int32_t avail = aux->xoff - Rwidth - used;
      if (avail > -Rwidth)
        aux->xoff -= (avail < 0)? Rwidth + avail : Rwidth;
      }
    }
  }

return this;
}



/*************************************************
*           Insert non-note items                *
*************************************************/

/* This function is called to insert entries into the postable for non-note
things such as accidentals, grace notes, and other markings.

Arguments:
  moff          music offset for the note
  xflags        flags for various marks (comma, caesura, etc)
  accleft       width required for accidental
  keyvector
  timevector
  gracevector
  previous      pointer to the previous position item
  prevlength
  prevflags

Returns:        pointer to final inserted item
*/

static workposstr *
pos_insertextras(int32_t moff, uint32_t xflags, int32_t accleft, int *keyvector,
  int *timevector, int *gracevector, workposstr *previous, int32_t prevlength,
  uint32_t prevflags)
{
workposstr *this;
int rrepeatextra = 0;
int used;
int i;

/* Find the postable entry for this note */

this = (previous == NULL)? pl_postable : previous;
while (this->moff < moff) this++;

/* Now process the auxiliary items, in order from right to left. This makes it
possible to do the best thing when more than one exists on a single note. They
may come out of order when on different notes, and in this case the spacing may
not be as perfect. But the likelihood is pretty rare.

They are given conventional "musical offsets" that are just prior to that of
the note they precede.

First handle accidentals. Because these are much more common than anything
else, it's worth a special test to avoid the rest of the function if there's
nothing else to do. Note that "this" might already be pointing at an accidental
entry -- so that extra space isn't inserted after it. Preserve this state of
affairs. We compute "used" here specially, with a different default to the
rest. */

pl_accexistedavail = -1;  /* Unset existing accidental available space */
if (accleft > 0)
  {
  used = (prevlength < 0)? 0 : pos_typewidth(7250, prevlength, prevflags);
  this = pos_insertXpos(previous, this, posx_acc, accleft, used, FALSE);
  if (xflags == 0) return this;
  }

/* Insert positions for grace notes. This is messy. We give the widths as
including the accidental widths so as to space them out correctly, but then we
have to go back afterwards and correct the positions. We also have to take
special action if there are already grace notes at this position, in case the
accidentals are different (rare). "Used" also gets a special value for grace
notes. */

if (gracevector[0] > 0)
  {
  int32_t lastspacing = curmovt->gracespacing[0];
  int first = 0;

  workposstr *pp = this;
  workposstr *p = this - 1;

  if (this >= pl_posptr)
    lastspacing += ((xflags & xf_rrepeat) == 0)? 4000 : 8000;

  used = (prevlength < 0)? 0 : pos_typewidth(7250, prevlength, prevflags);

  /* As we process them from right to left, any new ones will be inserted
  first. When we get to pre-existing ones, save where to stop the moving scan,
  and instead just check existing widths. */

  for (i = gracevector[0]; i >= 1; i--)
    {
    int id = posx_gracefirst + i - 1;
    if (pp == pl_postable || p->auxid != id)
      {
      this = pos_insertXpos(previous, this, id,
        ((i == gracevector[0])? lastspacing : curmovt->gracespacing[1]) +
          gracevector[i], used, FALSE);
      }
    else
      {
      int32_t spacing = (p->xoff == 0)? 0 : curmovt->gracespacing[1];

      /* When we hit what was the last (rightmost) gracenote, but is no longer,
      adjust the appropriate spacing, just in case the gracespacing values are
      different. This is the first of the pre-existing grace notes. Remember
      which it was. */

      if (first == 0)
        {
        int32_t adjust = curmovt->gracespacing[1] - curmovt->gracespacing[0];
        (p+1)->xoff += adjust;
        first = i;
        }

      if (p->xoff < spacing + gracevector[i])
        p->xoff = spacing + gracevector[i];
      p--;
      pp--;
      }
    }

  /* Go back from right to left and adjust positions, starting at the first
  additional grace note, and stopping when we get to any that were there
  before. */

  p = this - 1;
  while (p->auxid == posx_acc) p--;
  pp = p + 1;

  for (i = gracevector[0]; i > first; i--)
    {
    p->xoff += gracevector[i];
    pp->xoff -= gracevector[i];
    pp = p--;
    }
  }

/* Compute the space used by the previous note, if any, for the rest of the
items. */

used = (prevlength < 0)? 0 : pos_typewidth(11000, prevlength, prevflags);

/* Time Signature(s) */

for (i = timevector[0]; i >= 1; i--)
  this = pos_insertXpos(previous, this, posx_timefirst + i - 1,
    timevector[i], used, FALSE);

/* Key Signature(s) */

for (i = keyvector[0]; i >= 1; i--)
  this = pos_insertXpos(previous, this, posx_keyfirst + i - 1,
    keyvector[i], used, FALSE);

/* Left repeat. Check whether it is going to print on a bar line, and if not,
insert a position for it, unless it is going to coincide with a right repeat.
If it is on a barline, set a flag so that space can be inserted. (Can't insert
here, because it will then do it several times for multiple staves.) */

if ((xflags & xf_lrepeat) != 0)
  {
  if (previous == NULL && !pl_startlinebar && pl_bp->posxRL == -posx_RLleft)
    pl_barstartrepeat = TRUE;
  else if (previous != NULL && (xflags & xf_rrepeat) != 0)
    rrepeatextra = 6500;
  else
    this = pos_insertXpos(previous, this, -pl_bp->posxRL, 12500, used, TRUE);
  }

if ((xflags & xf_clef) != 0)
  this = pos_insertXpos(previous, this, posx_clef,
    (14 * (curmovt->fontsizes)->fontsize_midclefs.size)/10, used, FALSE);

if ((xflags & xf_rrepeat) != 0)
  {
  int32_t x = 7500;
  if (this == pl_posptr) pl_lastendwide = TRUE; else x += 5100;
  this = pos_insertXpos(previous, this, posx_RR, x+rrepeatextra, used, TRUE);
  }

if ((xflags & xf_dotbar) != 0)
  this = pos_insertXpos(previous, this, posx_dotbar, 6000, used, FALSE);

if ((xflags & xf_tick) != 0)
  this = pos_insertXpos(previous, this, posx_tick, 6000, used, FALSE);

if ((xflags & xf_comma) != 0)
  this = pos_insertXpos(previous, this, posx_comma, 6000, used, FALSE);

if ((xflags & xf_caesura) != 0)
  this = pos_insertXpos(previous, this, posx_caesura, 13000, used, FALSE);

return this;    /* return for next previous */
}



/*************************************************
*          Compute width of stave names          *
*************************************************/

/* This function returns the width needed for names to be printed at the start
of staves on a page. Two stave bitmaps are supplied: the name is counted only
if the stave is selected in both bitmaps. Typically, the first map gives the
staves that are active in a system, and the second is those that are currently
not suspended.

Arguments:
  pagedata    the page data block
  map         the first bitmap
  map2        the second bitmap

Returns:      the maximum width needed
*/

static int32_t
startwidth(pagedatastr *pagedata, uint64_t map, uint64_t map2)
{
int i;
int32_t x;
fontinststr *fdata;

if (pagedata->stavenames == NULL) return 0;
map &= map2;
x = 0;
fdata = (curmovt->fontsizes)->fontsize_text;

/* For each existing stave */

for (i = 1; i <= curmovt->laststave; i++)
  {
  snamestr *s;
  if ((map & (1 << i)) == 0) continue;  /* Not currently printing */

  /* For each stavename string on this stave */

  for (s = pagedata->stavenames[i]; s != NULL; s = s->extra)
    {
    int32_t w = 0;

    if ((s->flags & snf_vertical) != 0) w = 6000; else if (s->text != NULL)
      {
      uint32_t *t = s->text;

      /* Multiple substrings are separated by unescaped vertical bars. */

      for (;;)
        {
        int32_t ww;
        uint32_t tsave;
        uint32_t *tt = t;

        while (*t != 0 && PCHAR(*t) != ss_verticalbar) t++;
        tsave = *t;
        *t = 0;
        ww = string_width(tt, &(fdata[s->size]), NULL);
        if (ww > w) w = ww;
        if (tsave == 0) break;
        *t++ = tsave;
        }

      if (w > 0) w += 6000;  /* Final space if non-empty */
      }

    if (w > x) x = w;
    }
  }

/* If there are stave names, add extra if the system is braced or has a thin
bracket, to leave space between them and the names. */

if (x != 0)
  {
  if (curmovt->bracelist != NULL) x += 6500;
    else if (curmovt->thinbracketlist != NULL) x += 4000;
  }

return x;
}



/*************************************************
*              Deal with a page's heading        *
*************************************************/

/* This function sets up a headblock and adds it to the chain of headblocks and
system blocks for the page. It also subtracts the space needed for the heading
lines from the space left on the page. Remember to include an additional stave
depth + 1pt after any headings at the top of a page. This function is not
called if there are no headings.

Argument:   a heading structure
Returns:    nothing
*/

static void
do_pageheading(headstr *page_heading)
{
int32_t used = 0;
headblock *h = mem_get(sizeof(headblock));

h->is_sysblock = FALSE;
h->pageheading = (page_heading != curmovt->heading);  /* Not a movt heading */
h->next = NULL;
h->movt = curmovt;
h->headings = page_heading;

*pl_sysprevptr = (sysblock *)h;
pl_sysprevptr = (sysblock **)(&(h->next));

while (page_heading != NULL)
  {
  used += page_heading->space;
  page_heading = page_heading->next;
  }

if (curpage->spaceleft == main_pagelength) curpage->spaceleft -= 17000;
curpage->spaceleft -= used;
}



/*************************************************
*    Justify a heading or footing or footnote    *
*************************************************/

/* This function scans the text and splits up the left hand part into two or
more lines. For all but the last line the spacestretch field is set to cause
the text to be printed with right justification. (Actually the last line is
justified too, if it is near enough to the end.)

Argument:  a heading structure
Returns:   nothing
*/

static void
justifyheading(headstr *h)
{
for (; h != NULL; h = h->next)
  {
  uint32_t *lastsplit;
  uint32_t *p;
  uint32_t  spacecount;
  uint32_t  lastspacecount;
   int32_t  width;
   int32_t  lastwidth;

  /* Ignore drawing */

  if (h->drawing != NULL) continue;

  /* This is a text heading */

  lastsplit = NULL;
  lastwidth = 0;
  p = h->string[0];   /* Left hand part */
  spacecount = lastspacecount = 0;

  /* Scan and split. We must use string_width() for each word rather than add
  individual character widths so that kerning happens. */

  for (;;)
    {
    headstr *new;
    uint32_t c = *p;

    if (c != 0 && PCHAR(c) != ' ')
      {
      p++;
      continue;
      }

    *p = 0;
    width = string_width(h->string[0], &(h->fdata), NULL);
    *p = c;

    /* Fragment fits on the line */

    if (width <= curmovt->linelength)
      {
      if (c == 0)
        {
        if (curmovt->linelength - width < 5000 && spacecount > 0)
          h->fdata.spacestretch = (curmovt->linelength - width)/spacecount;
        break;
        }

      /* Not end of line */

      lastspacecount = spacecount++;
      lastsplit = p;
      lastwidth = width;
      while (PCHAR(*(++p)) == ' ') spacecount++;
      continue;
      }

    /* Fragment is greater than the line length */

    if (lastsplit == NULL) break;  /* No break points; give up */

    *lastsplit = 0;            /* Terminate first fragment */
    if (lastspacecount > 0)    /* Compute stretch amount */
      h->fdata.spacestretch = (curmovt->linelength - lastwidth)/lastspacecount;

    /* Set up a new headstr for the next line, copying the middle and right
    parts of the heading. */

    new = mem_get(sizeof(headstr));
    memcpy(new, h, sizeof(headstr));
    new->spaceabove = 0;
    new->fdata.spacestretch = 0;

    p = lastsplit + 1;
    while (PCHAR(*p) == ' ') p++;
    new->string[0] = p;
    h->string[1] = h->string[2] = NULL;
    h->next = new;
    h = new;

    spacecount = 0;
    lastsplit = NULL;
    lastwidth = 0;
    }
  }
}



/*************************************************
*              Set up a new page                 *
*************************************************/

/* This is not called for the very first page; curpage is always set to the
current page.

Arguments:
  heading        a movement heading, or NULL if not a movement start
  page_heading   a page heading, or NULL if no page heading

Returns:         nothing; curpage is updated
*/

static void
do_newpage(headstr *heading, headstr *page_heading)
{
pagestr *newpage = mem_get(sizeof(pagestr));
curpage->next = newpage;
newpage->number = main_lastpagenumber = curpage->number + page_increment;

curpage = newpage;
curpage->next = NULL;
curpage->spaceleft = main_pagelength;
curpage->overrun = 0;

curpage->sysblocks = NULL;
curpage->footing = NULL;
pl_sysprevptr = &(curpage->sysblocks);
pl_countsystems = 0;
pl_lastsystem = NULL;
pl_pagefootnotes = NULL;
pl_pagefootnotedepth = 0;

pl_justify = curmovt->justify;
pl_topmargin = curmovt->topmargin;
pl_botmargin = curmovt->bottommargin;

if (page_heading != NULL) do_pageheading(page_heading);
if (heading != NULL) do_pageheading(heading);
}



/*************************************************
*           Finish off a page                    *
*************************************************/

/* Correct the final spaceleft value (for information use), then justify
the page if necessary, and set up the footnotes & footing.

Argument:   TRUE for the final page; causes it to use lastfooting
Returns:    nothing
*/

static void
do_endpage(BOOL final)
{
headstr *footing;
uint32_t justbits = pl_justify & just_vert;
int32_t spaceleft = (curpage->spaceleft +=
  ((pl_lastsystem == NULL)? 0 : pl_lastsystem->systemgap));

/* Set up any footnotes, making sure the systemgap field on the last system is
the last accepted spacing below stave value, plus 10 points. Adjust the
spaceleft to allow for footnotes when justifying. */

if (pl_pagefootnotes != NULL)
  {
  headblock *h = mem_get(sizeof(headblock));
  h->is_sysblock = FALSE;
  h->pageheading = FALSE;
  h->next = NULL;
  h->movt = curmovt;
  h->headings = pl_pagefootnotes;
  *pl_sysprevptr = (sysblock *)h;
  pl_lastsystem->systemgap = pl_footnotespacing + 10000;
  spaceleft -= pl_pagefootnotedepth + pl_footnotespacing;
  }

if (spaceleft < 0) spaceleft = 0;    /* can happen on deep systems */

/* Deal with vertical justification data. If there are no spreading points
top+bottom is the same as bottom. There is a limit to the distance between
systems. */

if (justbits == just_vert)
  {
  int32_t topmargin = pl_topmargin;
  int32_t botmargin = pl_botmargin;
  int32_t margin = topmargin + botmargin;

  if (margin > spaceleft)
    {
    topmargin = mac_muldiv(topmargin, spaceleft, margin);
    botmargin = mac_muldiv(botmargin, spaceleft, margin);
    }
  spaceleft -= botmargin + topmargin;
  curpage->topspace = topmargin;

  if (spaceleft <= main_pagelength/2 && pl_countsystems > 1)
    {
    usint count = pl_countsystems - 1;
    int32_t insert = spaceleft/count;
    sysblock *s = curpage->sysblocks;

    if (insert > main_maxvertjustify) insert = main_maxvertjustify;

    while (s != NULL)             /* defensive programming */
      {
      if (s->is_sysblock && (s->flags & sysblock_noadvance) == 0)
        {
        s->systemgap += insert;
        if (--count == 0) break;
        }
      s = s->next;
      }
    }
  }

/* Top-only justification */

else if (justbits == just_top)
  curpage->topspace = (pl_topmargin > spaceleft)? spaceleft : pl_topmargin;

/* Bottom-only justification */

else if (justbits == just_bottom)
  {
  curpage->topspace = spaceleft - pl_botmargin;
  if (curpage->topspace < 0) curpage->topspace = 0;
  }

/* No justification => centred vertically */

else curpage->topspace = spaceleft/2;

/* Set up the page footing */

if (pl_pagefooting != NULL)     /* Start of movement page footing */
  {
  footing = pl_pagefooting;
  pl_pagefooting = NULL;
  }
else
  {
  footing = (final && curmovt->lastfooting != NULL)?
    curmovt->lastfooting : curmovt->pagefooting;
  }

if (footing != NULL)
  {
  headblock *f = mem_get(sizeof(headblock));
  f->next = NULL;
  f->movt = curmovt;
  f->headings = footing;
  curpage->footing = f;
  }
}



/*************************************************
*    See if there is pending {und,ov}erlay       *
*************************************************/

/* These functions are called by setcont() below.

Arguments:  none
Returns:    TRUE if there is pending underlay
*/

static BOOL
pendulay(void)
{
uolaystr *u = bar_cont->uolay;
while (u != NULL)
  {
  if (!u->above) return TRUE;
  u = u->next;
  }
return FALSE;
}


/*
Arguments:  none
Returns:    TRUE if there is pending overlay
*/

static BOOL
pendolay(void)
{
uolaystr *u = bar_cont->uolay;
while (u != NULL)
  {
  if (u->above) return TRUE;
  u = u->next;
  }
return FALSE;
}



/*************************************************
*        Shorten stem of note if appropriate     *
*************************************************/

/* This function is called from setcont(). We have to flag the note and not do
it twice, to cope with repeated bars (which use the same bar data).

Arguments:
  p            the note
  upflag       TRUE if the stem is up

Returns:       nothing
*/

static void
shorten_stem(b_notestr *tp, BOOL upflag)
{
int32_t shorten = 0;

if ((tp->flags & nf_shortened) != 0) return;

if (upflag)
  {
  if (tp->spitch > P_3L) shorten = 125*(tp->spitch - P_3L);
  }
else
  {
  if (tp->spitch < P_3L) shorten = 125*(P_3L - tp->spitch);
  }

tp->flags |= nf_shortened;
tp->yextra -= (shorten <= curmovt->shortenstems)?
  shorten : curmovt->shortenstems;
}



/*************************************************
*      Advance cont data to end of system        *
*************************************************/

/* This is called when the end of a system has been chosen. The working cont
structure is in wk_cont. A "snapshot" copy has been taken which represents the
state at the start of the system. We must now advance through the system to get
ready for the next snapshot. The system we are dealing with is in pl_sysblock.

This pass also handles changes of stave and system spacing, setting up new data
blocks as necessary. It also handles changes of page number.

While we are doing this, we can compute the tiecount values to go in the tie
control blocks. We cannot do this when reading in, because we don't know the
stem direction at the time the tie block is created. The count value is the
number of ties that take the opposite stemflag to the note.

We also have to do a lot of work if we encounter a beamed-over bar line at the
end of a system.

Arguments:  none
Returns:    nothing
*/

static void
setcont(void)
{
BOOL hadssnext = FALSE;
BOOL hadsshere = FALSE;

for (curstave = 0; curstave <= curmovt->laststave; curstave++)
  {
  stavestr *ss;
  BOOL no_ulay, no_olay;

  if (mac_notbit(curmovt->select_staves, curstave)) continue;
  ss = curmovt->stavetable[curstave];

  bar_cont = wk_cont + curstave;
  no_ulay = !pendulay();
  no_olay = !pendolay();

  for (curbarnumber = pl_sysblock->barstart;
       curbarnumber <= pl_sysblock->barend;
       curbarnumber++)
    {
    int chordcount;
    int32_t moff, beammoff, lastAlevel, lastBlevel;
    BOOL hadulay, hadolay, upflag;
    b_notestr *beamfirst;
    bstr *p;

    moff = 0;
    beammoff = 0;
    lastAlevel = 0;       /* Above level */
    lastBlevel = 0;       /* Below level */
    chordcount = 0;
    hadulay = FALSE;
    hadolay = FALSE;
    upflag = FALSE;
    beamfirst = NULL;
    beam_forceslope = INT32_MAX;

    for (p = (bstr *)(ss->barindex[curbarnumber]);
         p != NULL; p = p->next) switch(p->type)
      {
      headstr *h, *hh, *lh;
      b_notestr *tp;
      b_tiestr *tie;

      /* Actions that are common to this scan and to the output scan are
      held in a separate function. Items that are not relevant are skipped,
      though there is a check for an illegal item type. */

      default:
      misc_commoncont(p);
      break;

      case b_footnote:
      hh = h = &(((b_footnotestr *)p)->h);
      lh = NULL;
      justifyheading(h);
      while (hh != NULL)
        {
        lh = hh;
        pl_newfootnotedepth += hh->space;
        hh = hh->next;
        }
      if (pl_newfootnotes == NULL) pl_newfootnotes = h; else
        {
        pl_lastnewfootnote->next = h;
        h->spaceabove = curmovt->footnotesep;
        }
      pl_lastnewfootnote = lh;
      break;

      case b_slur:
      (void)slur_startslur((b_slurstr *)p);
      break;

      case b_endline:
      case b_endslur:
        {
        b_endslurstr *es = (b_endslurstr *)p;
        if (slur_endslur(es) == NULL)
          {
          char buff[8];
          if (es->value == 0) buff[0] = 0; else sprintf(buff, "\"%c\"",
            es->value);
          if (p->type == b_endline)
            error(ERR132, "line", buff, "[endline]");
          else
            error(ERR132, "slur", buff, "[endslur]");
          }
        }
      break;

      case b_tie:
      tie = (b_tiestr *)p;
      bar_cont->tie = tie;
      if (tie->abovecount == 0 && tie->belowcount == 0)
        {
        if (upflag)
          tie->belowcount = (chordcount > 1)? chordcount/2 : 1;
        else
          tie->abovecount = (chordcount+1)/2;
        }

      if (tie->abovecount == 0 && tie->belowcount < chordcount)
        tie->abovecount = chordcount - tie->belowcount;

      else if (tie->belowcount == 0 && tie->abovecount < chordcount)
        tie->belowcount = chordcount - tie->abovecount;

      /* Allow for tie below on underlay level*/

      if (lastBlevel != 0 && tie->belowcount > 0)
        {
        lastBlevel -= 3500;
        if (pl_sysblock->ulevel[curstave] > lastBlevel)
          pl_sysblock->ulevel[curstave] = lastBlevel;
        }

      /* Allow for tie above on overlay level*/

      if (lastAlevel != 0 && tie->abovecount > 0)
        {
        lastAlevel += 3500;
        if (pl_sysblock->olevel[curstave] < lastAlevel)
          pl_sysblock->olevel[curstave] = lastAlevel;
        }
      break;

      /* For notes/chords we calculate the underlay and overlay pitches, and
      also set flags for a possible subsequent tie. Skip rests, of course, and
      also skip the underlay/overlay calculation for notes that neither have
      their own text nor have an extender or hyphens under them (indicated by
      the existence of uolaystr data). Keep track of the last start-of-beam
      note, in case we are in an end-of-line bar with a continued beam. Handle
      automatic stem length adjustment. */

      case b_note:
      bar_cont->tie = NULL;
      tp = (b_notestr *)p;

      if (tp->spitch != 0)
        {
        int32_t stemlength;
        uint32_t acflags = tp->acflags;
        uint32_t flags = tp->flags;
        uint16_t apitch = tp->spitch;
        uint16_t bpitch = tp->spitch;
        uint8_t aacc = tp->acc;
        uint8_t bacc = tp->acc;

        if (tp->notetype >= quaver)
          {
          if (beamfirst == NULL)
            {
            beamfirst = tp;
            beammoff = moff;
            }
          }
        else
          {
          beamfirst = NULL;
          beam_forceslope = INT32_MAX;
          }

        upflag = (flags & nf_stemup) != 0;
        if (curmovt->shortenstems != 0 && tp->length != 0)
          shorten_stem(tp, upflag);
        stemlength = tp->yextra;
        chordcount = 1;

        /* Process the remaining notes of a chord, if any. Find the highest and
        lowest pitches, and their related accidentals. */

        for (b_notestr *ntp = (b_notestr *)(tp->next);
             ntp->type == b_chord;
             ntp = (b_notestr *)(ntp->next))
          {
          chordcount++;
          flags |= ntp->flags;
          acflags |= ntp->acflags;
          if (ntp->spitch > apitch)
            {
            apitch = ntp->spitch;
            aacc = ntp->acc;
            }
          if (ntp->spitch < bpitch)
            {
            bpitch = ntp->spitch;
            bacc = ntp->acc;
            }
          if (curmovt->shortenstems && ntp->length != 0)
            shorten_stem(ntp, upflag);
          if (abs(ntp->yextra) > abs(stemlength)) stemlength = ntp->yextra;
          }

        /* Now do the underlay/overlay stuff */

        lastAlevel = (apitch - P_0L)*500 +
          ((!upflag || (flags & nf_stem) == 0)?
             (5*accuptab[aacc])/4 : (14000 + stemlength));

        lastBlevel = (bpitch - P_1L - 2)*500 -
          ((hadulay || no_ulay)? pl_ulaysize : 4000) -
            ((upflag || (flags & nf_stem) == 0)?
               accdowntab[bacc]/2 : (13000 + stemlength));

        /* Allow for dynamics */

        if ((acflags & af_accents) != 0)
          {
          int32_t dynextraA = accboundtable[
            (((acflags & af_opposite) == 0)? 0:1) + (upflag? 2:0)];
          int32_t dynextraB = accboundtable[4 +
            (((acflags & af_opposite) == 0)? 0:1) + (upflag? 2:0)];

          lastAlevel += dynextraA;
          lastBlevel += dynextraB;

          /* That's all if no relevant accent, or the accent falls inside the
          staff; otherwise make sure the level is suitably outside the staff.
          */

          if (dynextraA != 0 && (acflags & af_accoutside) != 0)
            lastAlevel += dynextraA;   /* these are bigger accents */
          if (dynextraB != 0 && (acflags & af_accoutside) != 0)
            lastBlevel += dynextraB;   /* these are bigger accents */
          }

        /* Impose {min,max}imum level and keep {high,low}est level for the line
        if appropriate */

        if (lastAlevel < 20000) lastAlevel = 20000;
        if (lastBlevel > -pl_ulaysize - 1000) lastBlevel = -pl_ulaysize - 1000;

        if (no_olay || hadolay || pendolay())
          {
          if (pl_sysblock->olevel[curstave] < lastAlevel)
            pl_sysblock->olevel[curstave] = lastAlevel;
          }

        if (no_ulay || hadulay || pendulay())
          {
          if (pl_sysblock->ulevel[curstave] > lastBlevel)
            pl_sysblock->ulevel[curstave] = lastBlevel;
          }

        /* Turn off value if don't want tie noticed */

        if ((flags & nf_stem) != 0)
          {
          if (upflag) lastAlevel = 0; else lastBlevel = 0;
          }
        }

      /* Deal with rests - kill any outstanding underlay or overlay blocks for
      extensions, but not for hyphens. */

      else
        {
        uolaystr **uu = &(bar_cont->uolay);
        uolaystr *u = *uu;
        while (u != NULL)
          {
          if (u->type == '=')
            {
            *uu = u->next;
            mem_free_cached((void **)(&main_freeuolayblocks), u);
            }
          else uu = &(u->next);
          u = *uu;
          }
        lastAlevel = lastBlevel = 0;
        if (tp->notetype < quaver) beamfirst = NULL;
        }

      /* Notes and rests dealt with */

      moff += tp->length;
      hadulay = hadolay = FALSE;
      break;

      /* Deal with beam breaks; action needed only if all beams are to be
      broken. */

      case b_beambreak:
      if (((b_beambreakstr *)p)->value == BEAMBREAK_ALL)
        {
        beamfirst = NULL;
        beam_forceslope = INT32_MAX;
        }
      break;

      /* Deal with resets */

      case b_reset:
      moff = 0;
      break;

      /* Set up or cancel a hairpin pending block. */

      case b_hairpin:
      if ((((b_hairpinstr *)p)->flags & hp_end) != 0)
        {
        if (bar_cont->hairpin != NULL)
          {
          mem_free_cached((void **)(&main_freehairpinstr), bar_cont->hairpin);
          bar_cont->hairpin = NULL;
          }
        }
      else
        {
        hairpinstr *hs = mem_get_cached((void **)(&main_freehairpinstr),
         sizeof(hairpinstr));

        hs->hairpin = (b_hairpinstr *)p;
        hs->x = 0;
        hs->maxy = -INT32_MAX;
        hs->miny =  INT32_MAX;
        bar_cont->hairpin = hs;
        }


      break;

      /* For nth time bars we need only keep one block, since continued
      cases won't be printing the numbers during pagination. */

      case b_nbar:
      if (bar_cont->nbar == NULL) misc_startnbar((b_nbarstr *)p, 0, 0);
      break;

      case b_all:
      if (bar_cont->nbar != NULL)
        {
        mem_free_cached((void **)(&main_freenbarblocks), bar_cont->nbar);
        bar_cont->nbar = NULL;
        }
      break;

      case b_clef:
      bar_cont->clef = ((b_clefstr *)p)->clef;
      break;

      case b_time:
      bar_cont->time = ((b_timestr *)p)->time;
      break;

      case b_key:
      bar_cont->key = ((b_keystr *)p)->key;
      break;

      case b_sgabove:            /* retrospective change of system gap */
        {
        sysblock *pb = curpage->sysblocks;
        if (pb != NULL)  /* Check for a previous system */
          {
          b_sgstr *pp = (b_sgstr *)p;
          int32_t v = pp->value;
          while (pb->next != NULL) pb = pb->next;  /* Find last block */
          if(pb->is_sysblock)                      /* Ignore if heading */
            {
            curpage->spaceleft += pb->systemgap;
            if (pp->relative) pb->systemgap += v;
              else pb->systemgap = v;
            curpage->spaceleft -= pb->systemgap;
            }
          }
        }
      break;

      case b_sghere:            /* immediate change of system gap */
        {
        int32_t v = ((b_sgstr *)p)->value;
        if (((b_sgstr *)p)->relative) pl_sysblock->systemgap += v;
          else pl_sysblock->systemgap = v;
        }
      break;

      case b_sgnext:            /* delayed change of system gap */
        {
        int32_t v = ((b_sgstr *)p)->value;
        if (((b_sgstr *)p)->relative) pl_sgnext += v; else pl_sgnext = v;
        }
      break;

      case b_ssabove:           /* change ensured stave spacing above */
        {
        b_ssstr *pp = (b_ssstr *)p;
        int32_t v = pp->value;

        if (pp->stave == 0)  /* Stave 0 => all staves */
          {
          int i;
          for (i = 1; i < curmovt->laststave; i++)
            {
            if (pp->relative) pl_ssehere[i] += v;
              else pl_ssehere[i] = v;
            }
          }
        else
          {
          if (pp->relative) pl_ssehere[pp->stave] += v;
            else pl_ssehere[pp->stave] = v;
          }
        }
      break;

      case b_sshere:            /* immediate change of stave spacing */
        {
        b_ssstr *pp = (b_ssstr *)p;
        int s = pp->stave;
        int32_t v = pp->value;
        int i;

        if (!hadsshere)
          {
          int32_t *new = mem_get((MAX_STAVE+1)*sizeof(int32_t));
          memcpy(new, pl_sysblock->stavespacing,
            (curmovt->laststave+1)*sizeof(int32_t));
          pl_sysblock->stavespacing = new;
          hadsshere = TRUE;
          }

        if (s == 0)  /* Stave 0 => all staves */
          {
          for (i = 1; i < curmovt->laststave; i++)
            {
            if (pp->relative)
              pl_sysblock->stavespacing[i] += v;
            else
              pl_sysblock->stavespacing[i] = v;
            }
          }
        else if (pp->relative)
          {
          pl_sysblock->stavespacing[s] += v;

          /* Find the next stave that is not suspended, and bump its ensured
          space. This makes sure that a relative [sshere] always has a
          noticeable effect. */

          for (i = s+1; i <= curmovt->laststave; i++)
            {
            if (mac_isbit(pl_sysblock->notsuspend, i))
              {
              pl_ssehere[i] += v;
              break;
              }
            }
          }
        else pl_sysblock->stavespacing[s] = v;
        }
      break;

      case b_ssnext:            /* delayed change of stave spacing */
        {
        b_ssstr *pp = (b_ssstr *)p;
        int s = pp->stave;
        int32_t v = pp->value;

        if (!hadssnext)
          {
          int32_t *new = mem_get((MAX_STAVE+1)*sizeof(int32_t));
          memcpy(new, pl_ssnext, (curmovt->laststave+1)*sizeof(int32_t));
          pl_ssnext = new;
          hadssnext = TRUE;
          }

        if (s == 0)  /* => all staves */
          {
          int i;
          for (i = 1; i < curmovt->laststave; i++)
            {
            if (pp->relative) pl_ssnext[i] += v;
              else pl_ssnext[i] = v;
            }
          }

        else if (pp->relative) pl_ssnext[s] += v;
          else pl_ssnext[s] = v;
        }
      break;

      /* Changes to vertical justification are put into system-specific
      variables, as it isn't known at the time to which page they will apply.
      Changes to horizontal justification are put directly into the justifyLR
      variable, as they apply to this system. If the just_add bit is not set,
      subtraction is implied. */

      case b_justify:
        {
        b_justifystr *pp = (b_justifystr *)p;
        BOOL add = (pp->value & just_add) != 0;

        if ((pp->value & just_horiz) != 0)
          pl_justifyLR = add?
            (pl_justifyLR | pp->value) : (pl_justifyLR & ~pp->value);

        if ((pp->value & just_vert) != 0)
          {
          int oldjustify = (pl_sys_justify == -1)? pl_justify : pl_sys_justify;
          pl_sys_justify = add?
            (oldjustify | pp->value) : (oldjustify & ~pp->value);
          }
        }
      break;

      case b_page:
        {
        b_pagestr *pg = (b_pagestr *)p;
        uint32_t value = pg->value;
        if (pg->relative) value += curpage->number;
        if (value < curpage->number) error(ERR133, value, curpage->number);
        else
          curpage->number = value;
        }
      break;

      case b_pagebotmargin:
      pl_sys_botmargin = ((b_pagebotsstr *)p)->value;
      break;

      case b_pagetopmargin:
      pl_sys_topmargin = ((b_pagetopsstr *)p)->value;
      break;

      /* The only text we are interested in here is underlay or overlay; set up
      or remove continuation control blocks. */

      case b_text:
        {
        BOOL overlay;
        uint32_t c;
        uolaystr *u, **uu;
        b_textstr *t = (b_textstr *)p;

        if ((t->flags & text_ul) == 0 ||
            (t->laylen == 1 && PCHAR(t->string[0]) == '=')) break;

        /* We have something relevant */

        overlay = (t->flags & text_above) != 0;
        c = PCHAR(t->string[t->laylen]);
        uu = &bar_cont->uolay;
        u = *uu;

        /* On hitting any underlay or overlay, clear the field and flag so that
        only relevant notes are counted. Flag the type for the next note. */

        if (!overlay)
          {
          hadulay = TRUE;
          if (no_ulay)
            {
            pl_sysblock->ulevel[curstave] = 0;
            no_ulay = FALSE;
            }
          }
        else
          {
          hadolay = TRUE;
          if (no_olay)
            {
            pl_sysblock->olevel[curstave] = 0;
            no_olay = FALSE;
            }
          }

        /* Find existing control block for this level */

        while (u != NULL && (overlay != u->above || u->level != t->laylevel))
          {
          uu = &u->next;
          u = *uu;
          }

        /* If control block needed, either carry on with this one or
        get a new one. */

        if (c == '=' || c == '-')
          {
          if (u == NULL)
            {
            u = mem_get_cached((void **)(&main_freeuolayblocks),
              sizeof(uolaystr));
            u->next = NULL;
            u->x = u->y = 0;
            u->level = t->laylevel;
            u->above = overlay;
            *uu = u;
            }
          u->type = c;
          u->htype = t->htype;
          }

        /* Else free an existing one */

        else if (u != NULL)
          {
          *uu = u->next;
          mem_free_cached((void **)(&main_freeuolayblocks), u);
          }
        }
      break;

      /* Handle changes of underlay level */

      case b_ulevel:
      pl_ulevel[curstave] = ((b_uolevelstr *)p)->value;
      break;

      case b_ulhere:
      pl_ulhere[curstave] = ((b_uolherestr *)p)->value;
      break;

      /* Handle changes of overlay level */

      case b_olevel:
      pl_olevel[curstave] = ((b_uolevelstr *)p)->value;
      break;

      case b_olhere:
      pl_olhere[curstave] = ((b_uolherestr *)p)->value;
      break;

      /* Suspend sets flag for start of next system */

      case b_suspend:
      mac_clrbit(accepteddata->notsuspend, curstave);
      break;

      /* We must cope with resume following suspend in the
      same system. */

      case b_resume:
      mac_setbit(accepteddata->notsuspend, curstave);
      break;

      case b_barline:
      break;

      /* If we have a beam that may be carried over the end of the last bar in
      the system, we have to fudge up various values that will be set when the
      beam is drawn, in order to run setupbeam() so that we can find the beam's
      slope. This is preserved in the cont structure for use at the start of
      the next system. */

      case b_overbeam:
      if (curbarnumber == pl_sysblock->barend && beamfirst != NULL)
        {
        barposstr *bp = curmovt->posvector + curbarnumber;
        out_postable = out_posptr = bp->vector;
        out_poslast = out_postable + bp->count - 1;
        out_stavemagn = curmovt->stavesizes[curstave];
        out_pitchmagn = out_stavemagn/2;
        out_gracenotes = FALSE;
        out_gracefudge = 0;
        out_sysblock = pl_sysblock;
        beam_offsetadjust = 0;
        n_upflag = (beamfirst->flags & nf_stemup) != 0;
        n_upfactor = n_upflag? (+1):(-1);
        (void)out_setupbeam(beamfirst, beammoff, TRUE, FALSE);
        }
      break;
      }  /* End switch */
    }    /* End bar loop */
  }      /* End stave loop */


/* At the end of a system we must check on the {und,ov}erlay continuation
control blocks. For each stave that has such a block (or blocks) we must look
at the next bar. If it does not exist, is emtpy, or starts with a rest, we must
kill the continuation block(s) for extender lines. Don't do this for hyphen
blocks - another syllable is always expected and there are odd cases when these
do go over rests, etc.

In the same loop we can deal with {und,ov}erlay levels. */

for (curstave = 0; curstave <= curmovt->laststave; curstave++)
  {
  BOOL remove;
  /* Handle changes to {und,ov}erlay levels */

  if (pl_ulevel[curstave] != FIXED_UNSET)
    pl_sysblock->ulevel[curstave] = pl_ulevel[curstave];
  pl_sysblock->ulevel[curstave] += pl_ulhere[curstave];

  if (pl_olevel[curstave] != FIXED_UNSET)
    pl_sysblock->olevel[curstave] = pl_olevel[curstave];
  pl_sysblock->olevel[curstave] += pl_olhere[curstave];

  /* Deal with {under,over}lay continuations */

  bar_cont = wk_cont + curstave;
  if (mac_notbit(curmovt->select_staves, curstave) || bar_cont->uolay == NULL)
    continue;

  remove = TRUE;
  curbarnumber = pl_sysblock->barend + 1;  /* Next bar */

  if (curbarnumber < curmovt->barcount)    /* Not end of movement */
    {
    bstr *p;
    for (p = (bstr *)(((curmovt->stavetable)[curstave])->barindex[curbarnumber]);
         p != NULL;
         p = p->next)
      {
      if (p->type == b_note)
        {
        if (((b_notestr *)p)->spitch != 0) remove = FALSE;
        break;
        }
      }
    }

  /* No notes or rests found or first is a rest. Remove the underlay/overlay
  blocks for extender lines. They are put on the free chain for re-use. */

  if (remove)
    {
    uolaystr **uu = &(bar_cont->uolay);
    uolaystr *u = *uu;
    while (u != NULL)
      {
      if (u->type == '=')
        {
        *uu = u->next;
        mem_free_cached((void **)(&main_freeuolayblocks), u);
        }
      else uu = &(u->next);
      u = *uu;
      }
    }
  }
}



/*************************************************
*      Construct position table for one bar      *
*************************************************/

/* This function constructs a list of pairs containing a musical offset (from
the start of the bar), and a horizontal offset (from the first note position).

We first check to see if we are at a place where there are many bars rest for
all the relevant staves. If so, special action is taken later.

We go through each stave in turn, inserting between entries where necessary and
adjusting for minimum note widths. A space item in the input can cause
non-standard spacing.

Suspended staves are not excluded -- as they should only contain whole bar
rests, they won't affect the spacing. If "resume" is encountered, we set the
"notsuspend" bit.

There are several passes, to ensure that the notes on any one stave are not too
close together when accidentals and dots are present. There is yet another pass
to adjust the spacing for any embedded clefs, key signatures, etc.

The yield of the procedure is the horizontal width of the bar (excluding the
bar line). If a key or time signature has been read, then
pl_xxwidth is set to the width for printing just that much of the bar, for
use at the end of a line.

Argument:   TRUE if mis-matched bar lengths in different staves give an error
            (set FALSE when a bar is re-processed for a large stretch)
Returns:    the width of the bar or more than linelength for [newline/page]
*/

static int32_t
makepostable(BOOL lengthwarn)
{
int32_t MaxKeyWidth = 0;         /* Widest final key signature */
int32_t MaxTimeWidth = 0;        /* Widest final time signature */
int32_t largestmagn = 0;         /* Largest stave magnification */
int32_t endbarmoff = INT32_MAX;  /* Musical offset at end bar */
uint8_t forcenewline = b_start;  /* Not b_newline or b_newpage => unset */

int i;
workposstr *left;
posstr *outptr;

BOOL doublebar = FALSE;
BOOL Oldlastendwide = pl_lastendwide;      /* previous bar's value */
BOOL Oldlastenddouble = pl_lastenddouble;  /* this for double bar */
BOOL restbar = TRUE;
BOOL firstrestbar = TRUE;

uint64_t ulaymap = 0;                      /* map underlay staves */

/* Initialize shared variables */

pl_lastendwide = FALSE;         /* flag wide bar line (e.g. repeat) */
pl_lastenddouble = FALSE;       /* this for double barline (not wide) */
pl_manyrest = 0;                /* not many bars rest */
pl_xxwidth = 0;                 /* key and/or time change width */
pl_warnkey = FALSE;             /* no warning key read */
pl_warntime = FALSE;            /* no warning time read */

/* Find and initialize the barposstr for this bar. */

pl_bp = curmovt->posvector + pl_barnumber;
pl_bp->vector = NULL;             /* vector of positions */
pl_bp->count = 0;                 /* no entries in vector */
pl_bp->multi = 1;                 /* not multibar rest */
pl_bp->posxRL = -posx_RLleft;     /* default order for time/key/repeat */
pl_bp->barnoforce = bnf_auto;     /* no forced or suppressed bar number */
pl_bp->barnoX = 0;                /* no bar number movement */
pl_bp->barnoY = 0;

/* Set up an initial position table entry for the barline, currently the final
entry. */

pl_posptr = pl_postable;
pl_posptr->stemup = 0;
pl_posptr->stemdown = 0;
pl_posptr->moff = INT32_MAX;
pl_posptr->xoff = 0;
pl_posptr->space = 0;
pl_posptr->auxid = 0;

/* First we check for whole-bar rests in all the relevant staves, and count the
number of successive such bars if any are found. Key and and time changes and
newline/page and a number of other non-printing items are allowed in the first
rest bar. So is printed text. A clef change is allowed at the start of the
first bar (to allow multiple rests at the start of a piece); otherwise a clef
causes the bar to be treated as the last rest bar. */

for (int barnumber = pl_barnumber; barnumber < curmovt->barcount; barnumber++)
  {
  BOOL rrepeatORclefORdbar = FALSE;

  /* Scan all the relevant staves */

  for (curstave = 0; curstave <= curmovt->laststave; curstave++)
    {
    bstr *p;
    stavestr *ss;
    BOOL hadnote;

    if (mac_notbit(curmovt->select_staves, curstave)) continue;

    hadnote = FALSE;
    ss = curmovt->stavetable[curstave];

    for (p = (bstr *)(ss->barindex[barnumber]);
         p != NULL && restbar;
         p = p->next)
      {
      switch(p->type)
        {
        case b_start:
        break;

        case b_note:
        if (((b_notestr *)p)->spitch != 0 ||
           (((b_notestr *)p)->flags & nf_centre) == 0 ||
           (((b_notestr *)p)->flags & nf_nopack) != 0)
             restbar = FALSE;
        hadnote = TRUE;
        break;

        case b_comma:    case b_ornament:
        case b_caesura:  case b_nbar:
        case b_slur:     case b_endslur:
        case b_dotbar:   case b_hairpin:
        case b_reset:    case b_tick:
        restbar = FALSE;
        break;

        case b_barline:
        if (((b_barlinestr *)p)->bartype == barline_double)
          rrepeatORclefORdbar = TRUE;
        break;

        case b_clef:
        if (firstrestbar)
          {
          if (hadnote) restbar = FALSE;
          }
        else
          {
          if (!hadnote) restbar = FALSE;
            else rrepeatORclefORdbar = TRUE;
          }
        break;

        case b_rrepeat:
        rrepeatORclefORdbar = TRUE;
        break;

        default:
        if (!firstrestbar) restbar = FALSE;
        break;
        }
      }
    }

  /* All the staves have been scanned, or a non-rest was found.*/

  if (!restbar) break;
  pl_manyrest++;
  firstrestbar = FALSE;
  if (rrepeatORclefORdbar) break;   /* allow rrepeat/clef/double bar in last bar */
  }

/* Now set about constructing the position table. We scan the staves several
times in order to do this. The first scan establishes the entries for the
notes; horizontal offsets are set for the first staff, and any notes on
subsequent staves that are past the last notes on the staves above. Other notes
are interpolated pro rata at this stage. During this pass we also deal with
non-note items that don't affect the spacing, but must be noted for other parts
of the code. We also record any [space] settings for implemention right at the
end, and keep track of the largest stave magnification.

If we are handling a sequence of rest bars, we must process the last bar as
well as the first, in case there are clefs at the end of the last bar. It isn't
straightforward to write this as any kind of a loop, so we resort to the
dreaded GOTO for simplicity. */

curbarnumber = pl_barnumber;

REPEATFIRSTSCAN:

for (curstave = 0; curstave <= curmovt->laststave; curstave++)
  {
  BOOL auxitem, extraspace_set, wholebarrest;
  int32_t extraspace, moff, maxmoff, moffdelta;
  bstr *p;
  stavestr *ss;
  workposstr *pp;

  /* Do nothing if the stave isn't selected. */

  if (mac_notbit(curmovt->select_staves, curstave)) continue;

  /* If the bar has no data, ignore it, but we need to check the magnification,
  as otherwise we may end up with a zero largest magnification if all staves
  are empty. Note: ignore the magnification for stave 0. */

  if (curstave > 0 && curmovt->stavesizes[curstave] > largestmagn)
    largestmagn = curmovt->stavesizes[curstave];
  ss = curmovt->stavetable[curstave];
  if (mac_emptybar(ss->barindex[curbarnumber])) continue;

  /* Initialize */

  auxitem = FALSE;          /* last item was aux */
  moff = 0;                 /* musical offset */
  maxmoff = 0;              /* for resets */
  extraspace = 0;           /* value of [space] */
  extraspace_set = FALSE;   /* TRUE if [space] encountered */
  wholebarrest = TRUE;      /* FALSE if non-whole-bar-rest read */
  pp = pl_postable;         /* position in pl_postable */

  /* Scan the bar */

  for (p = (bstr *)(ss->barindex[curbarnumber]); p != NULL; p = p->next)
    {
    b_notestr *note;
    snamestr *sname;
    uint64_t upflags, downflags;

    switch(p->type)
      {
      /* Deal with items that are notes, ignoring grace notes in this pass */

      case b_note:
      note = (b_notestr *)p;
      if (note->length == 0)
        {
        auxitem = TRUE;   /* Grace notes are aux items */
        break;
        }

      /* Not a grace note */

      upflags = downflags = 0;
      if (note->spitch != 0 || (note->flags & nf_centre) == 0)
        wholebarrest = FALSE;  /* Found a non-whole-bar-rest item */

      /* Set stem direction flags for fine spacing adjustment, but only
      if there really is an actual stem. */

      if (note->spitch != 0 && note->notetype >= minim)
        {
        if ((note->flags & nf_stemup) != 0) mac_setbit(upflags, curstave);
          else mac_setbit(downflags, curstave);
        }

      /* Scan up position table to this musical offset; until one stave's data
      has been read, the barline has a musical offset of "infinity". */

      while (pp < pl_posptr && pp->moff < moff) pp++;

      /* If we have matched at the barline, we are on a stave with a bar that
      is longer than any on any previous staves. We reset the barline moff to
      "infinity". */

      if (pp->moff <= moff && pp == pl_posptr) pp->moff = INT32_MAX;

      /* If we are at a previously-existing entry, do nothing. Otherwise, move
      up the existing entries in the table and and insert a new entry. Note
      that that the moving up leaves the correct xoff value in the "new" entry,
      and it is the field in the *next* entry that must be updated. */

      if (pp->moff != moff)
        {
        for (workposstr *q = pl_posptr; q >= pp; q--) q[1] = q[0];

        pp->moff = moff;
        pp->space = 0;
        pp->auxid = 0;

        /* If we are at the end of the bar, and the bar length is unset, set
        the horizontal offset to be appropriate to this note type. (Note that
        pl_posptr is temporarily pointing one before the barline entry here.)
        */

        if (pp == pl_posptr && (pl_posptr + 1)->moff == INT32_MAX)
          {
          (pl_posptr + 1)->xoff = pos_notewidth(note->length);
          }

        /* If we are not at the end of the bar, or if the bar length has
        already been set, set the horizontal offset pro rata pro tem, and
        adjust the next value to compensate for what has been taken off. */

        else
          {
          workposstr *prev = pp - 1;
          workposstr *next = pp + 1;
          pp->xoff = mac_muldiv(next->xoff, moff - prev->moff,
            next->moff - prev->moff);
          next->xoff -= pp->xoff;
          }

        /* Move pointer to include one more entry. */

        pl_posptr++;
        }

      /* Or in the stem flags and handle any extra space value. */

      pp->stemup |= upflags;
      pp->stemdown |= downflags;

      if (extraspace_set)
        {
        if (extraspace >= 0)
          {
          if (extraspace > pp->space) pp->space = extraspace;
          }
        else if (extraspace < pp->space) pp->space = extraspace;
        extraspace = 0;
        extraspace_set = FALSE;
        }

      /* Adjust the musical offset for the next note, and set the flag
      saying last item was not an aux item. */

      moff += note->length;
      auxitem = FALSE;
      break;

      /* Deal with non-note items. We handle those that are noted for external
      action in this pass, and also with [space]. Those that have auxiliary
      positions must be noted, in order to cause a bar length check to happen.
      */

      case b_barline:
      if (((b_barlinestr *)p)->bartype == barline_double)
        doublebar = pl_lastenddouble = TRUE;

      break;

      /* It is convenient to handle the bar number forcing item here, when
      we have the relevant block available. */

      case b_barnum:
        {
        b_barnumstr *bn = (b_barnumstr *)p;
        if (!bn->flag) pl_bp->barnoforce = bnf_no; else
          {
          pl_bp->barnoforce = bnf_yes;
          pl_bp->barnoX = bn->x;
          pl_bp->barnoY = bn->y;
          }
        }
      break;

      case b_space:
        {
        b_spacestr *bs = (b_spacestr *)p;
        extraspace += bs->relative?
          (bs->x * curmovt->stavesizes[curstave])/1000 : bs->x;
        }
      extraspace_set = TRUE;
      break;

      case b_ns:
        {
        b_nsstr *bn = (b_nsstr *)p;
        for (i = 0; i < NOTETYPE_COUNT; i++)
          nextdata->note_spacing[i] += bn->ns[i];
        }
      break;

      case b_nsm:
        {
        int32_t v = ((b_nsmstr *)p)->value;
        for (i = 0; i < NOTETYPE_COUNT; i++)
          nextdata->note_spacing[i] =
            mac_muldiv(nextdata->note_spacing[i], v, 1000);
        }
      break;

      case b_ens:
      memcpy(nextdata->note_spacing, curmovt->note_spacing,
        NOTETYPE_COUNT*sizeof(int32_t));
      break;

      case b_newline:
      case b_newpage:
      if (!pl_startlinebar) forcenewline = p->type;
      break;

      case b_resume:
      if (mac_notbit(accepteddata->notsuspend, curstave))
        mac_setbit(nextdata->notsuspend, curstave);
      break;

      /* If this fragment is longer than any previous fragment, update the
      maximum fragment length. If it is longer than a previous barlength,
      update the bar length. */

      case b_reset:
      if (moff > maxmoff)
        {
        maxmoff = moff;
        if (moff > pl_posptr->moff) pl_posptr->moff = moff;
        }
      moff = 0;
      pp = pl_postable;
      break;

      /* Changes of stave name affect the available width for the system,
      and so must be handled here. However, we needn't (mustn't) process
      them when restretching to get even widths. */

      case b_name:             /* change of stave name */
      if (pl_stretchn != 1 || pl_stretchd != 1) break;
      i = ((b_namestr *)p)->value;
      sname = ss->stave_name;
      while (--i > 0 && sname != NULL) sname = sname->next;

      /* Get fresh memory if unchanged in this bar */

      if (accepteddata->stavenames == nextdata->stavenames)
        {
        nextdata->stavenames = mem_get((curmovt->laststave+1)*sizeof(snamestr *));
        memcpy(nextdata->stavenames, accepteddata->stavenames,
          (curmovt->laststave+1)*sizeof(snamestr *));
        }
      nextdata->stavenames[curstave] = sname;
      break;

      case b_text:             /* remember underlay staves */
      if ((((b_textstr *)p)->flags & text_ul) != 0)
        mac_setbit(ulaymap, curstave);
      break;

      case b_clef: case b_time:  case b_key:
      case b_tick: case b_comma: case b_dotbar:
      case b_caesura:
      if (moff > 0) auxitem = TRUE;
      break;

      default:
      if (p->type >= b_baditem) error(ERR128, p->type);  /* Hard */
      break;
      }
    }   /* End of scan of bar on one stave */

  /* If maxmoff is greater than zero at the end of a bar, it means there has
  been a [reset]. The following notes/rests may not have filled the bar, and
  may not have coincided with note positions before the [reset]. Therefore, we
  might need to insert one more position for where we ended up. Then set the
  current position to the maximum encountered. */

  if (moff < maxmoff)
    {
    while (pp < pl_posptr && pp->moff < moff) pp++;
    if (pp->moff != moff)
      {
      workposstr *prev = pp - 1;
      workposstr *next = pp + 1;

      for (workposstr *q = pl_posptr; q >= pp; q--) q[1] = q[0];

      pp->moff = moff;
      pp->space = 0;
      pp->auxid = 0;

      pp->xoff = mac_muldiv(next->xoff, moff - prev->moff,
        next->moff - prev->moff);
      next->xoff -= pp->xoff;

      /* Move end pointer to include one more entry */

      pl_posptr++;
      }

    moff = maxmoff;
    }

  /* Handle rounding problems caused by tuplets. If the current length
  differs from a multiple of the shortest note by only a small amount,
  round it. This can happen when note lengths are divided by non-factors
  for tuplets. */

  moffdelta = moff % len_shortest;
  if (moffdelta <= TUPLET_ROUND) moff -= moffdelta;
    else if (len_shortest - moffdelta <= TUPLET_ROUND)
      moff += len_shortest - moffdelta;

  /* Deal with a [space] value. If it is positive, use if greater than any
  previous setting; if it is negative, use if less. */

  if (extraspace_set)
    {
    if (extraspace >= 0)
      {
      if (extraspace > pl_posptr->space) pl_posptr->space = extraspace;
      }
    else
      if (extraspace < pl_posptr->space) pl_posptr->space = extraspace;
    }

  /* If this bar was not a whole bar rest, or if it ended with something
  that is positioned relative to the bar line, check that it has the same
  length as those above it. For the first stave, endbarmoff will equal
  INT32_MAX. If the lengths differ, keep the largest. Generate a warning
  the first time we measure the bar (when lengthwarn will be TRUE), if the
  length is less than the length of rest bars above, or not equal to note
  bars above. */

  if (!wholebarrest || auxitem)
    {
    if (endbarmoff == INT32_MAX) endbarmoff = 0; else
      {
      if (moff != endbarmoff && lengthwarn)
        error(ERR129, sfn(moff), sfn(endbarmoff));
      }

    if (moff > endbarmoff) endbarmoff = moff;
    pl_posptr->moff = endbarmoff;
    }

  /* For a whole bar rest, we set the moff in the final entry if it is not
  set (i.e. if this is the first stave), so that subsequent staves space
  correctly. However, whole bar rests are not checked against other bars for
  length. This makes it easy to handle odd cases by using R!, though it does
  miss the occasional warning that might have been nice. */

  else if (pl_posptr->moff == INT32_MAX) pl_posptr->moff = moff;
  }  /* End of per-stave loop */

/* The previous loop has to be repeated if pl_manyrest is greater than one,
in order to process the final bar of a repeat sequence. This just isn't easy to
code as a standard loop. We also need to set empty positioning for the
intermediate bars, just in case they are inspected for startbracketbar spacing.
*/

if (pl_manyrest >= 2 && curbarnumber == pl_barnumber)
  {
  for (i = 1; i < (int)pl_manyrest - 1; i++)
    {
    barposstr *bp = curmovt->posvector + curbarnumber + i;
    bp->vector = NULL;
    bp->count = 0;
    }
  curbarnumber += pl_manyrest - 1;
  goto REPEATFIRSTSCAN;
  }

/* If the bar contained no notes in all staves, the final musical offset and
spacing will not have been set. We need to set them in order to cope  with
other items, e.g. text or caesurae, at the end of the bar. */

if (pl_posptr->moff == INT32_MAX)
  {
  pl_posptr->moff = 0;
  pl_posptr->xoff = nextdata->note_spacing[semibreve];
  }

/* Debugging: print out the basic position table */

if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
  {
  eprintf("-------------------------\n");
  eprintf("BAR %d (%s) BASIC POSITIONS:\n", pl_barnumber,
    sfb(curmovt->barvector[pl_barnumber]));
  for (workposstr *t = pl_postable; t <= pl_posptr; t++)
    eprintf("%8d %6d\n", t->moff, t->xoff);
  }


/* We have now constructed the basic position table for the bar. However, some
gaps may be unacceptably narrow. We now do a series of repeat scans of the
notes, gradually adjusting the spacing. */


/* The first scan adjusts for note length only, applying minimum distances that
will ensure that notes do not overprint. This pass is concerned with space to
the *right* of each note only -- this includes inverted notes when the stem is
up. Space to the *left* is dealt with in subsequent passes.

If this is a multiple rest bar we don't need to go through the whole
rigmarole. The length of such bars is fixed. */

if (pl_manyrest >= 2)
  {
  int ii = (MFLAG(mf_codemultirests) && pl_manyrest < 9)? pl_manyrest : 0;
  pl_posptr->xoff = longrest_barwidths[ii]*largestmagn;
  pl_bp->multi = pl_manyrest;
  }

else for (curstave = 0; curstave <= curmovt->laststave; curstave++)
  {
  bstr *p;
  workposstr *prev;
  BOOL beambreak2;
  int32_t length, moff;
  uint32_t flags;

  if (mac_notbit(curmovt->select_staves, curstave)) continue;

  p = (bstr *)((curmovt->stavetable[curstave])->barindex[pl_barnumber]);
  if (p == NULL) continue;

  flags = 0;
  length = 0;            /* length of previous note */
  prev = pl_postable;    /* previous pl_postable entry */
  beambreak2 = FALSE;    /* secondary beambreak detected */
  moff = 0;              /* musical offset */

  /* Loop for all items in the bar. We must do the same as bar end for a
  [reset] which is at the bar end! */

  for (; p != NULL; p = p->next)
    {
    int32_t nextlength = 0;
    usint type = p->type;

    /* When we reach a non-grace note, or the barline, or a [reset] at the end
    of the bar, and there is a previous note, process the space that follows.
    Then remember a note's data, or process [reset]. Checks start at the second
    position - the variable "length" contains the length of the previous note,
    and the flags are also those of the previous note at this point. */

    if (type == b_barline ||
       (type == b_note && (nextlength = ((b_notestr *)p)->length) != 0) ||
       (type == b_reset && moff == pl_posptr->moff))
      {
      if (moff != 0)  /* Skip this at bar start */
        {
        workposstr *t = prev;
        int n = 0;
        int32_t extra;
        int32_t minwidth;
        int32_t width = 0;
        int32_t wantedwidth = pos_notewidth(length);

        /* Set an appropriate minimum distance from the previous note. At the
        barline, the minimum distance depends on the type of barline. If it's
        not double or an explicit ending barline, the value depends on the
        "unfinished" flag. */

        if (type == b_barline)
          {
          if (((b_barlinestr *)p)->bartype == barline_double) minwidth = 10000;
          else if (((b_barlinestr *)p)->bartype == barline_ending)
            minwidth = 13000;
          else minwidth =
            (pl_barnumber + 1 != curmovt->barcount || MFLAG(mf_unfinished))?
              7400: 11000;
          }
        else minwidth = 7250;

        /* Adjust the minimum width and wanted width if necessary. */

        minwidth = pos_typewidth(minwidth, length, flags);
        if (wantedwidth < minwidth) wantedwidth = minwidth;

        /* Insert a small amount of space after a secondary beam break, to
        avoid an optical illusion. */

        if (beambreak2)
          {
          wantedwidth += mac_muldiv(1300, curmovt->stavesizes[curstave], 1000);
          beambreak2 = FALSE;
          }

        /* Scan up position table to this musical offset, accumulating
        horizontal widths. */

        while (t < pl_posptr && t->moff < moff)
          {
          n++;
          t++;
          width += t->xoff;
          }

        /* If the width is insufficient, distribute the additional space
        amongst all the positions between the previous note on this stave and
        this note. Currently just divide it evenly, but we may need to improve
        on that one day. The wanted width is multiplied by the layout stretch
        factor - this is unity for the first measuring, but greater when
        re-laying-out for wide stretches. */

        extra = mac_muldiv(wantedwidth, pl_stretchn, pl_stretchd) - width;
        if (extra > 0)
          {
          int32_t x = extra/n;
          workposstr *pp = prev + 1;
          while (pp <= t)
            {
            (pp++)->xoff += x;
            extra -= x;
            if (extra < x) x = extra;
            }
          }

        /* Save previous pl_postable pointer */

        prev = t;
        }

      /* If this was an end-of-bar [reset], zero the musical offset and reset
      the previous pointer. No need to reset length & flags, as the first note
      doesn't use them. */

      if (type == b_reset)
        {
        moff = 0;
        prev = pl_postable;
        }

      /* For a (non-grace) note, save the length and flags from this note, and
      adjust the musical offset. */

      else if (type == b_note)
        {
        length = nextlength;
        flags = ((b_notestr *)p)->flags;
        moff += length;
        }
      }

    /* Deal with other relevant items */

    else switch (type)
      {
      case b_reset:  /* This reset is not at end-of-bar */
      moff = 0;
      prev = pl_postable;
      break;

      /* Remember a secondary beam break */

      case b_beambreak:
      if (((b_beambreakstr *)p)->value != BEAMBREAK_ALL) beambreak2 = TRUE;
      break;

      /* For a chord, collect the invert and dotting flags for subsequent
      processing. */

      case b_chord:        /* 2nd and subsequent notes */
      flags |= ((b_notestr *)p)->flags & (nf_dotted | nf_invert);
      break;
      }
    }   /* End of item on stave loop */
  }     /* End of per-stave loop */


/* Debugging: print out the note-spaced position table */

if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
  {
  eprintf("-------------------------\n");
  eprintf("BAR %d (%s) NOTE-SPACED POSITIONS:\n", pl_barnumber,
    sfb(curmovt->barvector[pl_barnumber]));
  for (workposstr *t = pl_postable; t <= pl_posptr; t++)
    eprintf("%8d %6d\n", t->moff, t->xoff);
  }


/* Now we do a scan for adjacent up and down stems. We can make an adjustment
with confidence only if all staves require it; otherwise it looks silly. Other
cases may need manual adjustment. This scan can be done on the pl_postable
only, using the flag bits already set up. Take care not to shorten distance
below the absolute minimum! */

for (left = pl_postable; left < pl_posptr - 1; left++)
  {
  workposstr *right = left + 1;

  if (left->stemup == 0 && right->stemdown == 0 &&
      left->stemdown != 0 && left->stemdown == right->stemup)
    {
    if (right->xoff >= 11000) right->xoff -= 1000;
    }

  else if (left->stemdown == 0 && right->stemup == 0 &&
           left->stemup != 0 && left->stemup == right->stemdown)
    right->xoff += 1000;
  }

/* Debugging: print out the new spacings */

if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
  {
  eprintf("-------------------------\n");
  eprintf("BAR %d (%s) NOTE-SPACED/STEMMED POSITIONS:\n", pl_barnumber,
    sfb(curmovt->barvector[pl_barnumber]));
  for (workposstr *t = pl_postable; t <= pl_posptr; t++)
    eprintf("%8d %6d\n", t->moff, t->xoff);
  }

/* Now we do a pass to insert space for things to the left of notes, like
accidentals, clefs, caesuras, etc. We have to scan through to each note before
handling them, as they come in a fixed (well, almost fixed) order. Use a word
of flag bits to remember those that have a width associated with them, and also
if anything at all has been encountered. If the bar starts with an lrepeat on
the barline, a flag gets set so that we can add space afterwards. */

pl_barstartrepeat = FALSE;

/* If we are handling a sequence of rest bars, we must process the last bar as
well as the first, in case there are clefs at the end of the last bar. It isn't
straightforward to write this as any kind of a loop, so we resort to the
dreaded GOTO for simplicity. */

curbarnumber = pl_barnumber;

REPEATSPACESCAN:

for (curstave = 0; curstave <= curmovt->laststave; curstave++)
  {
  bstr *p;
  workposstr *previous;
  BOOL arp_read, spread_read;
  int32_t moff, prevlength, ensured;
  uint32_t xflags, prevflags;
  int gracevector[posx_maxgrace + 1];
  int timevector[posx_maxtime + 1];
  int keyvector[posx_maxkey + 1];

  if (mac_notbit(curmovt->select_staves, curstave)) continue;

  p = (bstr *)((curmovt->stavetable[curstave])->barindex[curbarnumber]);
  if (p == NULL) continue;

  previous = NULL;
  arp_read = spread_read = FALSE;
  moff = 0;
  xflags = 0;                     /* flags for encountered items */
  prevlength = -1;                /* length of previous note */
  prevflags = 0;                  /* flags on previous note/chord */
  ensured = 0;
  gracevector[0] = 0;             /* count of gracenotes */
  timevector[0] = 0;              /* count of time signatures */
  keyvector[0] = 0;               /* count of key signatures */

  for (; p != NULL; p = p->next)
    {
    b_notestr *note;
    uint32_t length;

    switch(p->type)
      {
      case b_note:
      note = (b_notestr *)p;
      length = note->length;

      /* Count gracenotes and note something has been encountered */

      if (length == 0)
        {
        fontsizestr *fontsizes = curmovt->fontsizes;
        int gracecount = gracevector[0] + 1;
        if (gracecount > posx_maxgrace) gracecount = posx_maxgrace;
        gracevector[gracecount] = mac_muldiv(note->accleft,
          fontsizes->fontsize_grace.size, fontsizes->fontsize_music.size);
        gracevector[0] = gracecount;
        xflags |= xf_grace;
        }

      /* A real note -- first collect data for accidentals, then if
      anything precedes the note, call a routine to do most of the work.
      Always update the moff and save the note item for use next time. */

      else
        {
        uint32_t thisflags = 0;
        int32_t accleft = 0;

        /* Collect the maximum accidental width for a chord, and also check
        for inverted notes. Update the p pointer to point to the last note
        of the chord, to save scanning it again. */

        do
          {
          int32_t a = ((thisflags & (nf_invert | nf_stemup)) == nf_invert)?
            4500 : 0;
          if ((note->flags & nf_accinvis) == 0 && note->accleft > a)
            a = note->accleft;
          if (accleft < a) accleft = a;
          thisflags |= note->flags;
          p = (bstr *)note;
          note = (b_notestr *)note->next;
          }
        while (note->type == b_chord);

        /* Having got the accidental width, we need to add a teeny bit
        more space on the left. */

        if (accleft > 0) accleft += 600;

        /* Breves get their left bars printed to the left of the actual note
        position. We can treat this as a little bit of extra accidental space.
        The distance is in fact 2.3 points, but because things to the left get
        at least 11 points (as opposed to 7 points for notes only) we just need
        to ensure that something is inserted if there are no other accidentals.
        At the start of a bar, accidentals are shifted left, so in that case,
        leave a bit more. */

        if (length >= len_breve && accleft == 0)
          accleft = (moff == 0)? 3000 : 250;

        /* Extra space is needed for arpeggio or spread marks. This too can
        be treated as extra accidental space. */

        if (arp_read) { accleft += 6000; arp_read = FALSE; }
        if (spread_read) { accleft += 6000; spread_read = FALSE; }

        /* If accidental space is needed, or if there are other things to the
        left of the note, we call a separate function to do the work. This is
        also called at end of bar for the last space. */

        if (xflags != 0 || accleft != 0)
          {
          /* Arrange to keep the widest final key/time for warning bars */
          if (timevector[0] > 0)
            {
            if (timevector[timevector[0]] > MaxTimeWidth)
              MaxTimeWidth = timevector[timevector[0]];
            }
          if (keyvector[0] > 0)
            {
            if (keyvector[keyvector[0]] > MaxKeyWidth)
              MaxKeyWidth = keyvector[keyvector[0]];
            }

          /* Now do the insertion work */

          previous = pos_insertextras(moff, xflags, accleft, keyvector,
            timevector, gracevector, previous, prevlength, prevflags);

          /* Reset all the flags for the next note */

          xflags = 0;
          timevector[0] = 0;
          keyvector[0] = 0;
          gracevector[0] = 0;
          }

        /* If there are no extras on this note, just get previous
        up-to-date. */

        else
          {
          if (previous == NULL) previous = pl_postable;
          while (previous->moff < moff) previous++;
          }

        /* Handle any ensured value for this note (which previous is now
        pointing at) */

        if (ensured > 0)
          {
          int32_t between = 0;
          if (prevlength > 0)
            {
            workposstr *last = previous - 1;
            while (last->moff > moff - prevlength)
              { between += last->xoff; last--; }
            }

          if (previous->xoff + between < ensured)
            { previous->xoff = ensured - between; ensured = 0; }
          }

        /* Remember previous note's length and its flags */

        prevlength = length;
        prevflags = thisflags;
        moff += length;
        }
      break;

      case b_ensure:
      ensured = (curmovt->stavesizes[curstave] * ((b_ensurestr *)p)->value)/1000;
      break;

      /* Deal with non-note items. Clefs, keys, and times at the starts
      of lines will be marked for suppression. */

      case b_reset:
      moff = 0;
      previous = NULL;
      prevlength = -1;
      prevflags = 0;
      break;

      case b_lrepeat:
      xflags |= xf_lrepeat;

      /* If this repeat follows a key or time signature not at the start of
      a bar, move its position so that it prints after them, i.e. in the
      same order as in the input. */

      if ((xflags & xf_keytime) != 0) pl_bp->posxRL = -posx_RLright;
      break;

      case b_rrepeat: xflags |= xf_rrepeat; break;
      case b_comma:   xflags |= xf_comma;   break;
      case b_tick:    xflags |= xf_tick;    break;
      case b_caesura: xflags |= xf_caesura; break;
      case b_dotbar:  xflags |= xf_dotbar;  break;

      /* When not suppressed, update the working copy for use with key
      signatures, for both actual and assumed clefs. Clefs are the one thing
      that are allowed at the end of a multiple repeat bar, so for non-assumed
      clefs we must fudge the spacing. */

      case b_clef:
        {
        b_clefstr *c = (b_clefstr *)p;
        if (!c->suppress)
          {
          pl_sysclef[curstave] = c->clef;
          if (!c->assume)
            {
            xflags |= xf_clef;
            if (curbarnumber != pl_barnumber && moff != 0)
              pl_posptr->xoff += 15*curmovt->stavesizes[curstave];
            }
          }
        }
      break;

      case b_key:
        {
        b_keystr *k = (b_keystr *)p;
        if (!k->suppress && !k->assume)
          {
          int keycount = keyvector[0] + 1;
          if (keycount > posx_maxkey) keycount = posx_maxkey;
          xflags |= xf_keytime;
          keyvector[keycount] = (pl_startlinebar? 0 : 4000) +
            misc_keywidth(k->key, pl_sysclef[curstave]);
          keyvector[0] = keycount;
          if (mac_isbit(accepteddata->notsuspend, curstave))
            pl_warnkey |= k->warn;
          }
        }
      break;

      case b_time:
        {
        b_timestr *t = (b_timestr *)p;
        if (!t->suppress && !t->assume)
          {
          int timecount = timevector[0] + 1;
          if (timecount > posx_maxtime) timecount = posx_maxtime;
          timevector[timecount] = misc_timewidth(t->time) + 5000;
          timevector[0] = timecount;
          xflags |= xf_keytime;
          if (MFLAG(mf_showtime) &&
            mac_isbit(accepteddata->notsuspend, curstave))
              pl_warntime |= t->warn;
          }
        }
      break;

      case b_ornament:
        {
        b_ornamentstr *o = (b_ornamentstr *)p;
        if (o->ornament == or_arp ||
            o->ornament == or_arpu ||
            o->ornament == or_arpd) arp_read = TRUE;
          else if (o->ornament == or_spread) spread_read = TRUE;
        }
      break;
      }
    }    /* End bar scanning loop */

  /* Process for auxiliaries at the end of the bar */

  if (xflags != 0) pos_insertextras(moff, xflags, 0, keyvector, timevector,
    gracevector, previous, prevlength, prevflags);

  /* Handle [ensure] at end of bar */

  if (ensured > 0)
    {
    int32_t between = 0;
    if (prevlength > 0)
      {
      workposstr *last = pl_posptr - 1;
      while (last->moff > moff - prevlength)
        { between += last->xoff; last--; }
      }
    if (pl_posptr->xoff + between < ensured)
      pl_posptr->xoff = ensured - between;
    }
  }      /* End of per-stave loop */

/* The previous loop has to be repeated iff pl_manyrest is greater than one,
in order to process the final bar of a repeat sequence. This just isn't easy to
code as a standard loop. */

if (pl_manyrest >= 2 && curbarnumber == pl_barnumber)
  {
  curbarnumber += pl_manyrest - 1;
  goto REPEATSPACESCAN;
  }

/* Add a bit of space if the bar is not the first on a line, and starts with a
left repeat. */

if (pl_barstartrepeat)
  {
  pl_postable->xoff += 6500;       /* extra space at start bar */
  pl_xxwidth -= 6500;              /* but not for xxwidth */
  }

/* Debugging: print out the postable yet again */

if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
  {
  eprintf("-------------------------\n");
  eprintf("BAR %d (%s) ALL-IN POSITIONS:\n", pl_barnumber,
    sfb(curmovt->barvector[pl_barnumber]));
  for (workposstr *t = pl_postable; t <= pl_posptr; t++)
    eprintf("%8d %6d\n", t->moff, t->xoff);
  }

/* If enabled, we now do a scan to check that any underlaid text is not going
to overprint. Assume all is well at the start and end of a bar -- we have to,
since we don't do inter-bar spacing. Underlay and overlay are handled
separately, but for multiple verses the widest syllable is taken. */

if (MFLAG(mf_spreadunderlay))
  {
  BOOL spreadsome = FALSE;

  for (curstave = 1; curstave <= curmovt->laststave; curstave++)
    {
    bstr *p;
    workposstr *previousO, *previousU;
    BOOL hadulay, hadolay;
    int32_t nextleftU, nextleftO;
    int32_t nextrightU, nextrightO;
    int32_t lastrightU, lastrightO;
    int32_t moff;

    if (mac_notbit(curmovt->select_staves, curstave)) continue;
    p = (bstr *)((curmovt->stavetable[curstave])->barindex[pl_barnumber]);
    if (p == NULL) continue;

    previousO = previousU = pl_postable;
    hadulay = hadolay = FALSE;
    nextleftU = nextleftO = 0;
    nextrightU = nextrightO = 0;
    lastrightU = lastrightO = 0;
    moff = 0;

    for (; p != NULL; p = p->next)
      {
      if (p->type == b_reset)
        {
        previousO = previousU = pl_postable;
        hadulay = hadolay = FALSE;
        nextleftU = nextleftO = 0;
        nextrightU = nextrightO = 0;
        lastrightU = lastrightO = 0;
        moff = 0;
        }

      /* Deal with items that are notes. The hadulay and hadolay flags are set
      by preceding text items. There is no need to look at subsequent notes of
      a chord. */

      else if (p->type == b_note)
        {
        b_notestr *pp = (b_notestr *)p;

        if (pp->length == 0) continue;  /* Ignore grace notes */

        /* We have to process underlay and overlay entirely separately. */

        if (hadulay)
          {
          workposstr *this = previousU;
          while (this->moff < moff) this++;

          if (moff > 0)  /* Do nothing at bar start */
            {
            int32_t avail = 0;
            workposstr *t = previousU + 1;
            while (t <= this) avail += (t++)->xoff;
            avail -= lastrightU - nextleftU;
            if (avail < 0)
              {
              workposstr *tt = previousU + 1;
              while (tt->moff < moff + posx_max) tt++;
              tt->xoff -= avail;
              spreadsome = TRUE;
              }
            }

          lastrightU = nextrightU;
          nextleftU = nextrightU = 0;
          hadulay = FALSE;
          previousU = this;
          }

        /* Similar code for overlay */

        if (hadolay)
          {
          workposstr *this = previousO;
          while (this->moff < moff) this++;
          if (moff > 0)  /* Do nothing at bar start */
            {
            int32_t avail = 0;
            workposstr *t = previousO + 1;
            while (t <= this) avail += (t++)->xoff;
            avail -= lastrightO - nextleftO;
            if (avail < 0)
              {
              workposstr *tt = previousO + 1;
              while (tt->moff < moff + posx_max) tt++;
              tt->xoff -= avail;
              spreadsome = TRUE;
              }
            }

          lastrightO = nextrightO;
          nextleftO = nextrightO = 0;
          hadolay = FALSE;
          previousO = this;
          }

        moff += pp->length;
        }

      /* Deal with text items - only interested in {under,over}lay, and then
      only in syllables that aren't just "=". */

      else if (p->type == b_text)
        {
        b_textstr *t = (b_textstr *)p;
        fontinststr *fdata = curmovt->fontsizes->fontsize_text + t->size;
        uint32_t ss[256];
        uint32_t *cc, *pp, *qq;
        uint32_t ch;
        int32_t leftx, rightx;

        if ((t->flags & text_ul) == 0 ||
            (t->laylen == 1 && PCHAR(t->string[0]) == '=')) continue;

        leftx = t->x;

        /* Copy this bit of lyric, stopping if we reach '^', and converting #
        into space. */

        pp = t->string;
        qq = ss;

        for (i = 0; i < t->laylen && PCHAR(*pp) != '^'; i++)
          *qq++ = (PCHAR(*pp) == '#')? (PFTOP(*pp++) | ' ') : *pp++;
        *qq = 0;

        /* If we have not hit '^', or if there is only one circumflex, the
        string to centre is in ss. */

        cc = ss;
        if (i < t->laylen)
          {
          int k;

          for (k = i + 1; k < t->laylen; k++)
            if (PCHAR(t->string[k]) == '^') break;

          /* If there is a second circumflex, the string to centre is the one
          between the two circumflexes, but first we have to move the starting
          point left by the width of the initial string. */

          if (k < t->laylen)
            {
            int32_t ssw = string_width(ss, fdata, NULL);
            leftx -= mac_muldiv(ssw, curmovt->stavesizes[curstave], 1000);
            cc = qq;
            for (++i, ++pp; i < t->laylen && PCHAR(*pp) != '^'; i++)
              *qq++ = (PCHAR(*pp) == '#')? PFTOP(*pp++) | ' ' : *pp++;
            *qq = 0;
            }
          }

        /* cc now points to the string that might be centred, with the whole
        string so far in ss. Scanning the input string has stopped either at a
        circumflex or on reaching the syllable length. The means that the next
        character (pointed at by pp) must be one of the following:

          '^'  first or second circumflex
          '-'  end of syllable, not end of word
            0  end of word syllable at end of string
          '='  end of word syllable, continued

        If the underlay style is 0 (default) we always centre syllables.
        Otherwise, we do it if there was a circumflex

        Do the centring if the underlay style is 0 (default) or if explicitly
        requested by a circumflex or if this syllable applies to only one note,
        that is, there is no following '=' (possibly after '-'). The test for
        the latter also covers the circumflex case. */

        ch = PCHAR(*pp);  /* Next character */
        if (curmovt->underlaystyle == 0 ||
             (ch != '=' && (ch != '-' || PCHAR(pp[1]) != '=')))
          {
          int32_t w = string_width(cc, fdata, NULL);
          w = mac_muldiv(w, curmovt->stavesizes[curstave], 1000);

          if (w != 0) leftx += 3*curmovt->stavesizes[curstave] - w/2;

          /* After '^', add on the remainder of the syllable */

          if (ch == '^')
            {
            for (++i, ++pp; i < t->laylen; i++)
              *qq++ = (PCHAR(*pp) == '#')? PFTOP(*pp++) | ' ' : *pp++;
            *qq = 0;
            }
          }

        /* We now have the complete syllable, with circumflexes removed and #
        converted to space, in ss, with qq at the end. Cut off trailing spaces,
        but not if they were originally # characters. */

        pp = t->string + t->laylen;
        while (qq > ss && PCHAR(qq[-1]) == ' ' && PCHAR(pp[-1]) != '#')
          {
          qq--;
          pp--;
          }
        *qq = 0;

        /* A quarter of the fontsize is the space to the next syllable */

        rightx = leftx +
          mac_muldiv(fdata->size/4 + string_width(ss, fdata, NULL),
            curmovt->stavesizes[curstave], 1000);

        /* Correct for leading spaces, but not if they were originally
        # characters. */

        if (PCHAR(*ss) == ' ')
          {
          pp = t->string;
          qq = ss;
          while (PCHAR(*pp) != '#' && PCHAR(*qq) == ' ')
            {
            pp++;
            qq++;
            }
          *qq = 0;
          leftx += string_width(ss, fdata, NULL);
          }

        /* Keep maximum for verses, separately for overlay and underlay */

        if ((t->flags & text_above) != 0)
          {
          if (nextleftO > leftx) nextleftO = leftx;
          if (nextrightO < rightx) nextrightO = rightx;
          hadolay = TRUE;
          }
        else
          {
          if (nextleftU > leftx) nextleftU = leftx;
          if (nextrightU < rightx) nextrightU = rightx;
          hadulay = TRUE;
          }
        }    /* End handling text item */
      }      /* End loop through bar's data */
    }        /* End of per-stave loop */

  /* Show debugging info only if there has actually been some spreading. */

  if (spreadsome && (main_tracepos == INT32_MAX ||
                     main_tracepos == pl_barnumber))
    {
    eprintf("-------------------------\n");
    eprintf("BAR %d (%s) UNDERLAY SPREAD POSITIONS:\n", pl_barnumber,
      sfb(curmovt->barvector[pl_barnumber]));
    for (workposstr *t = pl_postable; t <= pl_posptr; t++)
      eprintf("%8d %6d\n", t->moff, t->xoff);
    }
  }

/* ------ End of passes through the data. Do some final adjustments. ------ */

curstave = -1;   /* For error messages - no specific stave */

/* If the bar ends with a double bar line or it's the last bar and not
unfinished, allow space for it. */

if (doublebar) pl_posptr->xoff += 1600;
  else if (pl_barnumber + 1 >= curmovt->barcount && !MFLAG(mf_unfinished))
    pl_posptr->xoff += 2000;

/* If the bar starts with an accidental position, we can reduce the initial
starting position to be nearer the bar line. */

if (!pl_startlinebar && pl_postable->auxid == posx_acc &&
     pl_barlinewidth > 3000)
  {
  int32_t notepos = pl_postable[0].xoff + pl_postable[1].xoff;
  if (notepos > pl_barlinewidth - 3000) notepos = pl_barlinewidth - 3000;
  pl_postable->xoff -= notepos;
  }

/* If the bar starts with a clef, we can reduce the initial starting position
to be nearer the bar line. This does not happen in conventional music, where a
change of clef is usually at the end of a bar, but is possible after an incipit
and other special cases. We also add space after the clef if the next thing is
a note. Finally, include the startline spacing parameters for the clef and any
following signatures. */

if (pl_postable->auxid == posx_clef && !pl_startlinebar)
  {
  workposstr *t = pl_postable;
  if (pl_barlinewidth > 2000)
    {
    int32_t adjust = pl_barlinewidth - 2000;
    pl_postable->xoff -= adjust;
    if (pl_postable[1].auxid == 0) pl_postable[1].xoff += adjust;
    }

  pl_postable->xoff += curmovt->startspace[0];  /* Add clef space */

  while (++t <= pl_posptr && posx_keyfirst <= t->auxid && t->auxid <= posx_timelast)
    {
    if (t->auxid <= posx_keylast) t->xoff += curmovt->startspace[1]; /* Key */
      else t->xoff += curmovt->startspace[2] - 4*largestmagn;        /* Time */
    }

  /***  This would make the spacing as at line start, but stretching makes it
  look bad. If ever the stretching is changed so as not to move the first note
  when it follows other things, instate this, and the same below.

  if (t <= pl_posptr) t->xoff += 3*largestmagn + curmovt->startspace[3];
  ***/
  }

/* If the bar starts with key and/or time signatures, we can reduce the initial
starting position to be nearer the bar line (provided the barlinewidth is large
enough), and reduce any gaps between them. However, we must increase the
position if the previous bar ended with a wide barline, or if a double barline
is going to be generated for a key signature.

The keyspace and timespace values are inserted before the first signature,
depending on the type.

We must also set the special width used for printing warning bars at the ends
of lines. At the start of a line, we make the reduction for any signature,
since the presence of an entry indicates a second signature. */

else if (pl_postable->auxid == posx_keyfirst ||
         pl_postable->auxid == posx_timefirst)
  {
  workposstr *t = pl_postable + (pl_startlinebar? 0:1);

  pl_postable->xoff += (pl_postable->auxid == posx_keyfirst)?
    curmovt->midkeyspacing : curmovt->midtimespacing;

  /* Move back start for wide enough barline spacing */

  if (pl_barlinewidth > 3000) pl_postable->xoff += 3000 - pl_barlinewidth;

  /* Move forward start if wide bar line */

  if (Oldlastendwide ||
      (pl_postable->auxid == posx_keyfirst && MFLAG(mf_keydoublebar) &&
      !Oldlastenddouble))
    pl_postable->xoff += 2000;

  /* Handle multiple signatures and compute the special width. */

  pl_xxwidth += pl_postable->xoff;

  for (; t <= pl_posptr &&
         t->auxid >= posx_keyfirst &&
         t->auxid <= posx_timelast; t++)
    {
    if (t->auxid <= posx_keylast)
      {
      t->xoff -= 3*largestmagn;
      if (pl_warnkey) pl_xxwidth += t->xoff;
      }
    else
      {
      t->xoff += curmovt->startspace[2] - 4*largestmagn;
      if (pl_warntime) pl_xxwidth += t->xoff;
      }
    }

  /* Add in space for the final item. MaxTimeWidth has extra space added to it
  which seems to be too much in the case when a key signature could have been
  present, but was suppressed by [nowarn]. In that case (only), we reduce the
  value. This is a fudge because I did't want to mess with the rest of the code
  when adding the independent [nowarn] facility. */

  if (!pl_warnkey && MaxKeyWidth > 0) MaxTimeWidth -= 8000;

  pl_xxwidth += (pl_warnkey && pl_warntime)?
    ((MaxKeyWidth > MaxTimeWidth)? MaxKeyWidth:MaxTimeWidth) :
    (pl_warnkey? MaxKeyWidth : MaxTimeWidth);

  /* Add notespace to midline bars */

  /***  This would make the spacing as at line start, but stretching makes it
  look bad. If ever the stretching is changed so as not to move the first note
  when it follows other things, instate this.

  if (!pl_startlinebar && t <= pl_posptr)
    t->xoff += 3*largestmagn + curmovt->startspace[3];
  ****/

  /* If the keys+times are followed by a repeat, bring it nearer too, and if
  the thing following that is an accidental, it can come nearer. Also if a time
  (but not key) signature is followed by an accidental, close the gap slightly.
  */

  if (t <= pl_posptr)
    {
    if (t->auxid == posx_RLright)
      {
      (t++)->xoff -= 2000;
      if (t <= pl_posptr && t->auxid == posx_acc) t->xoff -= 2000;
      }
    else if (posx_timefirst <= (t-1)->auxid && (t-1)->auxid <= posx_timelast &&
      t->auxid == posx_acc) t->xoff -= 2000;
    }
  }

/* We now have to check up on grace notes preceding notes with accidentals. If
the grace notes are on staves that do not have accidentals, we do not need to
leave more space between the grace notes and the accidentals. This copes with
several common cases, but it does not do the complete job. */

for (left = pl_postable; left < pl_posptr; left++)
  {
  workposstr *right = left + 1;
  if (right->auxid == posx_acc && left->auxid >= posx_gracefirst &&
      left->auxid <= posx_gracelast &&
      (left->auxstaves & right->auxstaves) == 0)
    {
    right->xoff -= (right+1)->xoff;
    if (right->xoff < 0) right->xoff = 0;
    }
  }

/* So far we have been working with offsets between the notes, but the final
result must have offsets from the start of the bar. At the same time we can
incorporate the values of any [space] directives. */

pl_postable->xoff += pl_postable->space;
for (left = pl_postable + 1; left <= pl_posptr; left++)
  left->xoff += left->space + (left-1)->xoff;

/* Debugging output */

if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
  {
  eprintf("-------------------------\n");
  eprintf("BAR %d (%s) FINAL POSITIONS:\n", pl_barnumber,
    sfb(curmovt->barvector[pl_barnumber]));
  eprintf("%8d %6d %6d\n", pl_postable->moff, 0, pl_postable->xoff);
  for (workposstr *t = pl_postable + 1; t <= pl_posptr; t++)
    eprintf("%8d %6d %6d\n", t->moff, t->xoff - (t-1)->xoff, t->xoff);
  if (forcenewline != b_start) eprintf("!! Newline forced !!\n");
  }

/* Now copy the retained data - (moff, xoff) pairs - into a vector which is
attached to the bar's data structure. If the bar has to be respaced for
stretching or squashing, the vector will already exist. */

left = pl_postable;
pl_bp->count = pl_posptr - pl_postable + 1;
if (pl_bp->vector == NULL)
  pl_bp->vector = mem_get(pl_bp->count * sizeof(posstr));
outptr = pl_bp->vector;
while (left <= pl_posptr)
  {
  outptr->moff = left->moff;
  (outptr++)->xoff = (left++)->xoff;
  }

/* Normally, return the width of the bar, up to the bar line, but if a new line
or page is being forced, return a number that will force a newline if this is
not the first bar in a system. Note: do NOT return INT32_MAX because the value
is added to the current position to see if the bar fits on the line - that
would cause integer overflow. */

if (forcenewline == b_start) return pl_posptr->xoff; else
  {
  if (forcenewline == b_newpage) pl_newpagewanted = TRUE;
  return 2 * curmovt->linelength;
  }
}



/*************************************************
*          Pagination function                   *
*************************************************/

/* This function is called when the music has been successfully read in to
memory.

Arguments:  none
Returns:    nothing
*/

void
paginate(void)
{
int i;
int layoutptr;
int layoutstack[MAX_LAYOUT_STACK];
int layoutstackptr = 0;
int lengthwarn = 0;
int lastbarcountbump = 0;  /* Set to avoid compiler warning */
usint movtnumber = 1;
usint page_state = page_state_newmovt;
headstr *lastfootnote = NULL;

int32_t adjustkeyposition;
int32_t adjusttimeposition;
int32_t nextbarwidth;
int32_t save_note_spacing[NOTETYPE_COUNT];
int32_t timewidth;
int32_t xposition;

BOOL firstsystem;
BOOL movt_pending = FALSE;
BOOL page_done = FALSE;

TRACE("\npaginate() start\n");

/* Get memory for the working position table. */

pl_postable = mem_get_independent(MAX_POSTABLESIZE * sizeof(workposstr));

/* Set up page and line lengths in magnified units. Once we have the line
length, we can split and justify heading and footing lines. */

main_pagelength = (main_pagelength * 1000)/main_magnification;
for (i = 0; i < (int)movement_count; i++)
  {
  curmovt = movements[i];
  curmovt->linelength = (curmovt->linelength * 1000)/main_magnification;
  justifyheading(curmovt->heading);
  justifyheading(curmovt->footing);
  justifyheading(curmovt->lastfooting);
  }

/* Other initialization */

curmovt = movements[0];  /* Sic: first movement */

/* Set up various blocks of memory */

accepteddata->stavenames = mem_get((MAX_STAVE+1) * sizeof(snamestr *));

pl_ulevel = mem_get((MAX_STAVE+1)*sizeof(int32_t));
pl_ulhere = mem_get((MAX_STAVE+1)*sizeof(int32_t));
pl_olevel = mem_get((MAX_STAVE+1)*sizeof(int32_t));
pl_olhere = mem_get((MAX_STAVE+1)*sizeof(int32_t));

pl_ssehere = mem_get((MAX_STAVE+1)*sizeof(int32_t));

/* Set up the first page block and associated variables. */

curpage = main_pageanchor = mem_get(sizeof(pagestr));
*curpage = init_curpage;
curpage->number = main_lastpagenumber = page_firstnumber;
curpage->spaceleft = main_pagelength;

pl_sysprevptr = &(curpage->sysblocks);
pl_countsystems = 0;
pl_lastsystem = NULL;

pl_pagefooting = pl_pagefootnotes = NULL;
pl_pagefootnotedepth = 0;

/* Set the spreading parameters for the first page */

pl_botmargin = curmovt->bottommargin;
pl_justify = curmovt->justify;
pl_topmargin = curmovt->topmargin;

/* Loop that does the job; page_state controls which action is taken. */

while (!page_done) switch(page_state)
  {
  uint8_t justbits;
  int32_t barlinewidth, stretchn, stretchd, sysdepth, sysfootdepth;
  int32_t lastulevel;


  /****************************************************************************/
  /****************************************************************************/

  /* Deal with the start of a movement. We deal with the heading lines and then
  set up for paginating the rest of the movement. */

  case page_state_newmovt:
  active_transpose = curmovt->transpose;
  firstsystem = TRUE;

  /* The equivalent of this code also exists in pmw_read_header, in connection
  with the barlinespace directive. Keep in step. */

  if (curmovt->barlinespace == FIXED_UNSET)
    {
    pl_barlinewidth = (curmovt->note_spacing)[minim]/2 - 5000;
    if (pl_barlinewidth < 3000) pl_barlinewidth = 3000;
    curmovt->barlinespace = pl_barlinewidth;
    }
  else pl_barlinewidth = curmovt->barlinespace;

  pl_allstavebits = 1Lu << curmovt->laststave;  /* Top stave's bit */
  pl_allstavebits |= pl_allstavebits - 2;       /* + all below, except 0 */

  pl_stavemap = curmovt->select_staves;
  pl_ssenext = curmovt->stave_ensure;
  pl_ssnext   = curmovt->stave_spacing;
  pl_sgnext = curmovt->systemgap;
  pl_ulaysize = (curmovt->fontsizes->fontsize_text)[ff_offset_ulay].size;
  pl_olaysize = (curmovt->fontsizes->fontsize_text)[ff_offset_olay].size;

  /* Deal with heading texts if we know the movement is to go on this page. For
  movements other than the first, if we are at the top of a page, do the page
  heading unless it has been turned off explicitly. Also, set up a footing for
  this page. */

  if (!movt_pending)
    {
    if (movtnumber > 1 && curmovt->pageheading != NULL &&
         !MFLAG(mf_nopageheading) && curpage->spaceleft == main_pagelength)
      do_pageheading(curmovt->pageheading);
    if (curmovt->heading != NULL) do_pageheading(curmovt->heading);
    if (curmovt->footing != NULL) pl_pagefooting = curmovt->footing;
    }

  /* Create vector of per-bar data structures. For movements with many bars
  this can be quite large, so get it as an independent block. */

  curmovt->posvector = mem_get_independent(curmovt->barcount * sizeof(barposstr));

  /* Now set up to process the bars. Cut back the working count of staves to
  those that have been selected. If no staves are present in the movement,
  curmovt->laststave is already set to -1. */

  if (curmovt->laststave >= 0)
    {
    for (int8_t x = curmovt->laststave; x >= 0; x--)
      {
      if (mac_isbit(curmovt->select_staves, x))
        {
        curmovt->laststave = x;
        break;
        }
      }
    }

  /* Initialize values in the accepted data structure */

  accepteddata->notsuspend = curmovt->select_staves & ~curmovt->suspend_staves;
  memcpy(accepteddata->note_spacing, curmovt->note_spacing,
    NOTETYPE_COUNT*sizeof(int));

  accepteddata->stavenames = mem_get((curmovt->laststave+1)*sizeof(snamestr *));
  for (i = 1; i <= curmovt->laststave; i++)
    accepteddata->stavenames[i] = ((curmovt->stavetable)[i])->stave_name;

  /* Initialize vectors for {und,ov}erlay level handling, and set defaults in
  the continuation data vector. */

  for (i = 0; i <= curmovt->laststave; i++)
    {
    pl_ulevel[i] = pl_olevel[i] = FIXED_UNSET;
    wk_cont[i] = init_cont;
    wk_cont[i].time = curmovt->time;
    wk_cont[i].key = transpose_key(curmovt->key);
    }

  /* Disable all time signatures if startnotime was specified; otherwise enable
  for all staves except stave 0. */

  pl_showtimes = MFLAG(mf_startnotime)? 0 : (~0 ^ 1);

  /* Set up for fixed layout if required. The layout data consists mainly of
  pairs, the first of which identifies the second. A "newpage" item, however,
  has no following data. The items are as follows:

  lv_repeatcount, value   Repeat next group that many times
  lv_barcount,    value   Next system to have this many bars
  lv_repeatptr,   value   End repeat that started at value offset
  lv_newpage              Force a new page (no value)

  Repetition is handled by using the local layoutstack to keep count of the
  various, possibly nested, repeats. The final item in a layout list is always
  a repeat pointer to offset zero, so we put a large repeat count at the start
  of the stack. Then copy any initial repeat in the layout. */

  if (curmovt->layout == NULL) layoutptr = -1; else
    {
    layoutstack[0] = 10000;
    layoutstackptr = 1;
    layoutptr = 0;
    while (curmovt->layout[layoutptr++] == lv_repeatcount)
      layoutstack[layoutstackptr++] = curmovt->layout[layoutptr++];
    }

  /* The left and right justification bits can be set immediately, so that they
  apply to all systems in the new movement. The top and bottom bits can't be
  changed yet, because we don't know if this movement will start on the current
  page. */

  pl_justifyLR = curmovt->justify;

  /* Initialize the bar number and deal with the case of no bars of music in
  the movement. The three pl_ variables are otherwise set when we know that the
  first system of the new movement fits on the page. This code copes with a
  single-movement file. */

  if ((pl_barnumber = 0) >= curmovt->barcount)
    {
    pl_botmargin = curmovt->bottommargin;
    pl_justify = curmovt->justify;
    pl_topmargin = curmovt->topmargin;
    page_state = page_state_donemovt;
    }

  /* If there are some bars, we are now ready to start a new system. */

  else page_state = page_state_newsystem;
  break;



  /****************************************************************************/
  /****************************************************************************/

  /* Deal with the start of a new system. There will always be at least one bar
  left when control gets here. */

  case page_state_newsystem:
  curbarnumber = pl_barnumber;
  timewidth = 0;
  pl_newpagewanted = FALSE;
  pl_stretchn = pl_stretchd = 1;  /* no stretch */
  memcpy(pl_ssehere, pl_ssenext, (curmovt->laststave+1)*sizeof(int32_t));

  /* Get a new system block and initialize some of the fields. The remainder
  get set as the system is processed. */

  pl_sysblock = mem_get(sizeof(sysblock));
  pl_sysblock->next = NULL;
  pl_sysblock->movt = curmovt;
  pl_sysblock->is_sysblock = TRUE;
  pl_sysblock->flags = 0;
  pl_sysblock->stavenames = accepteddata->stavenames;
  pl_sysblock->stavespacing = pl_ssnext;
  pl_sysblock->ulevel = mem_get((curmovt->laststave+1) * sizeof(uint32_t));
  pl_sysblock->olevel = mem_get((curmovt->laststave+1) * sizeof(uint32_t));
  pl_sysblock->systemgap = pl_sgnext;
  pl_sysblock->notsuspend = accepteddata->notsuspend;
  pl_sysblock->showtimes = 0;
  pl_sysblock->barstart = pl_barnumber;
  pl_sysblock->barend = pl_barnumber;

  pl_sysblock->cont = mem_get((curmovt->laststave+1)*sizeof(contstr));
  misc_copycontstr(pl_sysblock->cont, wk_cont, curmovt->laststave, TRUE);

  /* Working clefs while measuring. The current clef is needed only for
  measuring special keys, which may depend on the clef. We can't use the
  wk_cont versions because they are not updated until after we know which bars
  are to be included in this system. */

  for (i = 1; i <= curmovt->laststave; i++)
    pl_sysclef[i] = pl_sysblock->cont[i].clef;

  /* Finished with any overbeam structures (this loses memory that could
  potentially be re-used, but it isn't huge, and is probably also rare).
  Also set default underlay and overlay values. */

  for (i = 0; i <= curmovt->laststave; i++)
    {
    wk_cont[i].overbeam = NULL;
    pl_ulhere[i] = pl_olhere[i] = 0;
    pl_sysblock->ulevel[i] = -(pl_ulaysize + 1000);
    pl_sysblock->olevel[i] = 20000;
    }

  /* Update the current clef and key/time signatures if necessary */

  setsignatures();

  /* Initialize those fields of the current data structure that are reset for
  each system */

  accepteddata->endkey = FALSE;
  accepteddata->endtime = FALSE;
  accepteddata->endbar = pl_barnumber - 1;
  accepteddata->count = 0;

  /* Save the initial notespacing so that it can be restored for re-spacing
  bars when there's a big stretch factor. */

  memcpy(save_note_spacing, accepteddata->note_spacing,
    NOTETYPE_COUNT * sizeof(int32_t));

  /* If all existing selected staves are suspended (can happen during part
  extraction with the use of [newline] or with S! bars) unsuspend the lowest
  numbered selected stave. If any staves get resumed in the system, this gets
  undone again later. */

  if (curmovt->laststave > 0)  /* Movement has some selected staves */
    {
    if ((pl_sysblock->notsuspend & curmovt->select_staves & pl_allstavebits) == 0)
      {
      uint64_t bit = 2;
      for (int k = 1; k <= curmovt->laststave; k++)
        {
        if ((curmovt->select_staves & bit) != 0) break;
        bit <<= 1;
        }
      pl_sysblock->notsuspend |= bit;
      }
    }

  /* Find the starting position of the stave */

  accepteddata->startxposition = startwidth(accepteddata, pl_stavemap,
    pl_sysblock->notsuspend);

  /* Compute position for key signatures. We make them all line up vertically.
  For the moment, the value generated is relative to startxposition. */

  pl_sysblock->keyxposition = 0;
  for (i = 1; i <= curmovt->laststave; i++)
    {
    stavestr *ss = curmovt->stavetable[i];
    if ((pl_stavemap & pl_sysblock->notsuspend & (1 << i)) != 0 &&
        (!ss->omitempty || ss->barindex[pl_barnumber] != NULL))
      {
      int32_t xpos = (curmovt->clefwidths[(pl_sysblock->cont[i]).clef] *
        curmovt->stavesizes[i]) + curmovt->startspace[0];
      if (xpos > pl_sysblock->keyxposition) pl_sysblock->keyxposition = xpos;
      }
    }

  /* Compute the relative position of the time signature relative to the key
  signature by finding the widest time key signature. */

  pl_sysblock->timexposition = 0;
  for (i = 1; i <= curmovt->laststave; i++)
    {
    if ((pl_stavemap & pl_sysblock->notsuspend & (1 << i)) != 0)
      {
      uint8_t key = (pl_sysblock->cont[i]).key;
      int32_t xpos = (misc_keywidth(key, pl_sysblock->cont[i].clef) *
        curmovt->stavesizes[i])/1000;
      if (xpos > pl_sysblock->timexposition) pl_sysblock->timexposition = xpos;
      }
    }

  /* If at least one key signature is not empty, insert extra space before the
  key position. */

  if (pl_sysblock->timexposition != 0)
    pl_sysblock->keyxposition += curmovt->startspace[1];

  /* Make the time signature position relative to startxposition. */

  pl_sysblock->timexposition += pl_sysblock->keyxposition;

  /* When there are no clefs or key signatures, put a little bit of space
  before the time signature or first note. */

  if (pl_sysblock->timexposition == 0) pl_sysblock->timexposition += 2000;

  /* Find the widest time signature that is to be shown. */

  for (i = 1; i <= curmovt->laststave; i++)
    {
    if ((pl_stavemap & pl_sysblock->notsuspend & pl_showtimes & (1 << i)) != 0)
      {
      int32_t tw = (misc_timewidth((pl_sysblock->cont[i]).time) *
        curmovt->stavesizes[i])/1000;
      if (tw > timewidth) timewidth = tw;
      }
    }

  /* If any time signature is to be shown, add in the configured extra space
  beforehand. */

  if (timewidth != 0) pl_sysblock->timexposition += curmovt->startspace[2];

  /* Preserve the show-time-signatures setting for this system, and reset to
  none for the next one. */

  pl_sysblock->showtimes = pl_showtimes;
  pl_showtimes = 0;

  /* Set the first note position. */

  pl_sysblock->firstnoteposition = pl_sysblock->timexposition + timewidth +
    PAGE_LEFTBARSPACE + curmovt->startspace[3];

  /* The xposition is an absolute position, used during calculations. */

  accepteddata->xposition =
    accepteddata->startxposition + pl_sysblock->firstnoteposition;

  /* Initialize start of line flags */

  pl_startlinebar = TRUE;        /* becomes FALSE after one acceptance */
  pl_lastendwide = FALSE;        /* at start of line */
  pl_lastenddouble = FALSE;      /* ditto */

  /* Enter the mid-system state. */

  page_state = page_state_insystem;
  break;




  /****************************************************************************/
  /****************************************************************************/

  /* In the middle of a system - measure the next bar and see if it will fit.
  It it doesn't, and it starts with a key or time signature and there is not
  even enough room for that, we have to back off from accepting the previous
  bar. When the system is full, change state again. */

  case page_state_insystem:

  /* Make copy of current status for makepostable() to update */

  *nextdata = *accepteddata;

  /* Measure the bar -- this also sets various flags such as pl_warnkey and
  also sets pl_xxwidth if there is a key and/or time signature. The lengthwarn
  variable is normally zero (or less), but is set to 1 after the end of a
  system so that re-measuring the bar, for the next system, doesn't give a
  length warning again. If we back up two bars, in order to fit in a key/time
  signature, lengthwarn is set to 2. */

  nextbarwidth = makepostable(lengthwarn-- < 1);

  /* Compute position if bar were accepted */

  xposition = accepteddata->xposition + nextbarwidth;

  /* If a stave has been resumed in this bar, or if a stave name has changed,
  it may be necessary to change the width of the stave name space (i.e.
  nextdata->startxposition). This may make the bar unacceptable.

  We must also check the clef of the resumed stave, if it has a bar to print at
  the start of the system, because a wider clef will alter the position of the
  key signature.

  Likewise the key signature of the resumed stave, because a wider key
  signature will alter the position of the time signature. */

  adjustkeyposition = 0;
  adjusttimeposition = 0;

  if (nextdata->notsuspend != accepteddata->notsuspend ||
      nextdata->stavenames != accepteddata->stavenames)
    {
    int32_t newkeyxposition = 0;
    int32_t newtimexposition = 0;

    /* Deal with stave names */

    int32_t newstartx = startwidth(nextdata, pl_stavemap, nextdata->notsuspend);
    nextdata->startxposition = newstartx;
    xposition += newstartx - accepteddata->startxposition;

    /* Deal with change of key signature position */

    for (i = 1; i <= curmovt->laststave; i++)
      {
      stavestr *ss = curmovt->stavetable[i];
      if (mac_isbit2(pl_stavemap, nextdata->notsuspend, i) &&
          (!ss->omitempty || ss->barindex[pl_sysblock->barstart] != NULL))
        {
        int clef = (pl_sysblock->cont[i]).clef;
        int key = (pl_sysblock->cont[i]).key;
        int xp = curmovt->clefwidths[clef] * curmovt->stavesizes[i] +
          curmovt->startspace[0];
        if (xp > newkeyxposition) newkeyxposition = xp;
        xp = newkeyxposition +
          (curmovt->stavesizes[i] * misc_keywidth(key, clef))/1000;
        if (xp > newtimexposition) newtimexposition = xp;
        }
      }

    if (newkeyxposition > pl_sysblock->keyxposition)
      {
      adjustkeyposition = newkeyxposition - pl_sysblock->keyxposition;
      xposition += adjustkeyposition;
      }

    if (newtimexposition > pl_sysblock->timexposition + adjustkeyposition)
      {
      adjusttimeposition = newtimexposition - pl_sysblock->timexposition -
        adjustkeyposition;
      xposition += adjusttimeposition;
      }
    }

  /* Default overrun is "infinity" */

  pl_sysblock->overrun = INT32_MAX;

  /* If this is not the first bar on the line, see if it will fit. We always
  accept one bar - it gets squashed (with a warning). If a fixed layout has
  been specified, accept bars until we have one more than required. This test
  is for an unacceptable bar. */

  if ((layoutptr < 0 && accepteddata->count > 0 &&
       xposition > curmovt->linelength) ||
      (layoutptr >= 0 && accepteddata->count >= curmovt->layout[layoutptr]))
    {
    int32_t overrun = xposition - curmovt->linelength;
    lengthwarn = 1;       /* Don't warn when we reprocess the bar */

    /* See if cautionary signature(s) are needed. */

    if (!pl_startlinebar && (pl_warnkey || pl_warntime))
      {
      xposition = accepteddata->xposition + pl_xxwidth;

      /* If there is not even enough space for the end-of-line signature(s),
      back up to the previous bar, unless there isn't one to back up to, in
      which case give an overlong line warning. */

      if (layoutptr < 0 && xposition > curmovt->linelength)
        {
        if (accepteddata->count > 1)
          {
          pagedatastr *temp = prevdata;
          prevdata = accepteddata;
          accepteddata = temp;
          pl_barnumber -= lastbarcountbump;
          overrun = xposition - curmovt->linelength;
          lengthwarn++;   /* Two bars not to warn for */
          }
        else
          {
          error(ERR130);
          overrun = INT32_MAX;
          }
        }

      /* Room for key/time -- set flag(s) and xposition */

      else
        {
        accepteddata->xposition = xposition + pl_barlinewidth;
        accepteddata->endkey = pl_warnkey;
        accepteddata->endtime = pl_warntime;
        }
      }

    /* Set up the overrun value; round to 0.5 points */

    pl_sysblock->overrun = ((overrun + 499)/500) * 500;

    /* In all cases we've finished the system */

    page_state = page_state_donesystem;
    }

  /* There is room on the line for this bar, or it is the first bar on the
  line, so accept it. Give a warning for an overflowing single bar, which will
  be squashed to fit. */

  else
    {
    pagedatastr *temp = prevdata;
    prevdata = accepteddata;
    accepteddata = nextdata;
    nextdata = temp;

    if (accepteddata->count == 0 && xposition > curmovt->linelength)
      {
      error(ERR131, sff(nextbarwidth), sff(xposition - nextbarwidth),
        sff(curmovt->linelength));
      }

    /* If this bar starts with a key or time change, then bump the previous x
    position to account for it, as that will be what happens if we have to back
    off. */

    if (pl_xxwidth > 0 && !pl_startlinebar)
      {
      prevdata->xposition += pl_xxwidth + pl_barlinewidth;
      prevdata->endkey = pl_warnkey;
      prevdata->endtime = pl_warntime;
      }
    else
      {
      prevdata->endkey = FALSE;
      prevdata->endtime = FALSE;
      }

    /* Allow for key & time signature positioning adjustment */

    if (adjustkeyposition || adjusttimeposition)
      {
      pl_sysblock->keyxposition += adjustkeyposition;
      pl_sysblock->timexposition += adjustkeyposition + adjusttimeposition;
      pl_sysblock->firstnoteposition += adjustkeyposition + adjusttimeposition;
      }

    /* Update xposition, endbar number, and counts. The value of pl_manyrest is
    0 for a non-rest bar, 1 for a single rest bar, and 2 or more for a sequence
    of rest bars. */

    accepteddata->xposition = xposition + pl_barlinewidth;
    lastbarcountbump = (pl_manyrest >= 2)? pl_manyrest : 1;

    accepteddata->endbar += lastbarcountbump;
    pl_barnumber += lastbarcountbump;
    curbarnumber = pl_barnumber;
    accepteddata->count++;                  /* Count printed bars */
    pl_startlinebar = FALSE;                /* No longer first in system */

    /* The end of the movement perforce ends the system. */

    if (pl_barnumber >= curmovt->barcount) page_state = page_state_donesystem;
    }

  break;



  /****************************************************************************/
  /****************************************************************************/

  /* Completed a system - tidy the data structures and see if it fits onto the
  current page. The positions at the start of the line can be made absolute now
  that the starting xposition is known. We also perform the stretching
  operation on the position tables in the bars at this point. */

  case page_state_donesystem:
  pl_sysblock->stavenames = accepteddata->stavenames;
  pl_sysblock->barend = accepteddata->endbar;

  /* If there are no unsuspended staves (that is, all staves are suspended) in
  the accepted data, leave the sysblock alone, as it will have had one stave
  forced into it. Otherwise, overwrite with the accepted value, thereby turning
  off the fudged stave if there was one. */

  if ((accepteddata->notsuspend & curmovt->select_staves & pl_allstavebits) != 0)
    pl_sysblock->notsuspend = accepteddata->notsuspend;

  /* Fix various initial positions */

  pl_sysblock->startxposition = accepteddata->startxposition;
  pl_sysblock->joinxposition = accepteddata->startxposition;
  pl_sysblock->keyxposition += accepteddata->startxposition;
  pl_sysblock->timexposition += accepteddata->startxposition;
  pl_sysblock->firstnoteposition += accepteddata->startxposition;

  if (accepteddata->endkey) pl_sysblock->flags |= sysblock_warnkey;
  if (accepteddata->endtime) pl_sysblock->flags |= sysblock_warntime;

  /* Advance the continuation data to the end of the system, ready for the
  next one. This scan also handles changes of stave and system spacing, and
  local justification. Because we do not yet know if this system is going to
  fit on the page, any vertical justification changes that it makes are
  placed in pl_sys_xxx variables. We initialize them to negative numbers to
  detect changes.

  There is an unfortunate chicken-and-egg situation here. We need to set the
  barlinewidth, as it is used when computing beam slopes for beams that cross
  barlines. It should really be set to the stretched value, but we can't
  compute the stretching factor until we've done the barcont stuff, in order
  to know if we have to justify or not. We cheat by setting it to the
  unstretched value and hoping that is near enough... */

  pl_sysblock->barlinewidth = barlinewidth = pl_barlinewidth;
  pl_sys_topmargin = pl_sys_botmargin = pl_sys_justify = -1;

  /* The setcont() function also collects footnotes and system notes.
  Initialize the variables before calling it. */

  pl_newfootnotes = NULL;
  pl_newfootnotedepth = 0;
  setcont();

  /* Set up the xposition of the end of the line and the justify bits, and
  compute the spreading parameters. We spread if the line is wider than a
  proportion of the linewidth as set by the stretchthresh variables, or if
  it's too long (when the "spreading" is actually squashing). Note that
  left/right justification bits are taken from this system's flags, if there
  were any. */

  xposition = accepteddata->xposition - barlinewidth;
  justbits = pl_justifyLR & just_horiz;

  /* Left + right justification */

  if ((justbits == just_horiz &&
    xposition - pl_sysblock->startxposition >
      (STRETCHTHRESHNUM *
        (curmovt->linelength - pl_sysblock->startxposition)) /
          STRETCHTHRESHDEN) ||
            xposition > curmovt->linelength)
    {
    int32_t save_xxwidth = pl_xxwidth;
    int32_t xxadjust = pl_sysblock->firstnoteposition +
      ((accepteddata->endkey || accepteddata->endtime)?
        pl_xxwidth + barlinewidth : 0);

    stretchn = curmovt->linelength - xxadjust;
    stretchd = xposition - xxadjust;

    pl_sysblock->xjustify = 0;
    pl_sysblock->flags |= sysblock_stretch;

    /* If the stretching factor is large enough, throw away the position
    tables and re-format all the bars using the known stretching factor. They
    should not get any wider. Then compute revised stretching factors. Repeat
    if necessary, up to 4 times. Note that we have to keep re-stretching the
    barlinewidth.

    From release 4.22 we also do this when the stretching is actually
    squashing by a large enough amount, which can happen when the layout
    directive forces more bars onto a line than would normally fit. This is
    necessary when there is underlay, where words might crash when a bar is
    squashed. (While testing this, it turns out that the squashing version
    also sometimes kicks in after a stretching time round the loop, which
    sometimes overdoes things, it seems. It does no harm.) */

    pl_stretchn = stretchn;
    pl_stretchd = stretchn;   /* sic - see below - it changes cumulatively */

    i = 0;
    while (i++ < 4 &&
           (
           (mac_muldiv(stretchd, 1000, stretchn) > STRETCHRESPACETHRESH) ||
           (mac_muldiv(stretchn, 1000, stretchd) > STRETCHRESPACETHRESH)
           ))
      {
      int j;

      pl_stretchd = mac_muldiv(pl_stretchd, stretchd, stretchn);
      xposition = pl_sysblock->firstnoteposition;
      pl_startlinebar = TRUE;        /* becomes FALSE after one acceptance */
      pl_lastendwide = FALSE;        /* at start of line */
      pl_lastenddouble = FALSE;      /* ditto */

      barlinewidth = mac_muldiv(barlinewidth, stretchn, stretchd);
      memcpy(nextdata->note_spacing, save_note_spacing,
        NOTETYPE_COUNT * sizeof(int32_t));

      /* Reset clefs at system start */

      for (j = 1; j <= curmovt->laststave; j++)
        pl_sysclef[j] = pl_sysblock->cont[j].clef;

      for (pl_barnumber = pl_sysblock->barstart;
           pl_barnumber <= pl_sysblock->barend;
           pl_barnumber++)
        {
        curbarnumber = pl_barnumber;
        xposition += makepostable(FALSE) + barlinewidth;
        pl_startlinebar = FALSE;
        if (pl_manyrest >= 2) pl_barnumber += pl_manyrest - 1;
        }

      xposition -= barlinewidth;
      if (accepteddata->endkey || accepteddata->endtime)
        xposition += save_xxwidth + curmovt->barlinespace;  /* Unstretched barlinewidth */
      stretchd = xposition - xxadjust;

      if (main_tracepos == INT32_MAX ||
          (pl_sysblock->barstart <= main_tracepos &&
           pl_sysblock->barend >= main_tracepos))
        {
        eprintf("-------------------------\n");
        eprintf("REDO BARS %d-%d old=%d new=%d cycles=%d\n",
          pl_sysblock->barstart, pl_sysblock->barend,
            mac_muldiv(pl_stretchn, 1000, pl_stretchd),
              mac_muldiv(stretchn, 1000, stretchd), i);
        }
      }  /* End up to 4 times loop */
    }    /* End left+right justification */

  /* Deal with right only or no justification */

  else
    {
    stretchn = stretchd = 1;
    if ((justbits & just_left) == 0)
      {
      int32_t xjustify = curmovt->linelength - xposition;
      if (justbits == 0) xjustify /= 2;
      pl_sysblock->xjustify = xjustify;
      }
    else pl_sysblock->xjustify = 0;
    }

  /* The barline width for the system is the final stretched value. */

  pl_sysblock->barlinewidth = mac_muldiv(barlinewidth, stretchn, stretchd);

  /* Now apply the stretching operation to the bars in the system. Key and
  time signatures and left repeats at the start of a bar are not stretched.
  Grace notes are kept at the same distance from their successors. */

  for (i = pl_sysblock->barstart; i <= pl_sysblock->barend; i++)
    {
    barposstr *bp = curmovt->posvector + i;
    posstr *p = bp->vector;
    int count = bp->count;

    /* If this is the first bar of a multi-rest, make a correction to the
    value of i to skip the others. */

    i += bp->multi - 1;

    /* Skip over any clefs, key signatures or time signatures at the start of
    the bar. For big stretches, it is in fact not enough to do this, as the
    stretched barlinewidth can make their positioning look silly. We move
    them to the left in this case.

    This is probably less relevant now that we re-lay-out lines to get the
    stretching factor down. */

    /***** PRO TEM remove fix to retain previous state pending revised
    stretching. In this state, clefs are not tested here (they don't normally
    occur at line starts). We need a revised stretching algorithm to keep the
    first note fixed even after clefs, keys, and times. ****/

    if ((p->moff <= posx_timelast && p->moff >= posx_keyfirst))  /*** || p->moff == posx_clef) ***/
      {
      int n = 9;
      while ((count > 0 && p->moff >= posx_keyfirst && p->moff <= posx_timelast) ||
             p->moff == posx_clef)
        {
        p->xoff -= ((pl_sysblock->barlinewidth - pl_barlinewidth)*n)/10;
        if (n > 2) n -= 2;
        p++;
        count--;
        }
      }

    /* Else skip over any grace notes and accidentals, and also the first
    note, which we do not want to move. But if there is nothing in the bar,
    don't skip over the first (= last) item. */

    else
      {
      while (count > 0 && p->moff < 0) { p++; count--; }
      if (count > 1) { p++; count--; }
      }

    /* Now stretch the remaining items, dealing specially with grace notes,
    which are identified by finding the next full note and checking the
    offset. Also deal specially with clefs. */

    while (count-- > 0)
      {
      int old;
      posstr *pp = p;

      while (count > 0 && (pp+1)->moff - pp->moff <= -posx_max)
        {
        pp++;
        count--;
        }

      old = pp->xoff;
      pp->xoff = mac_muldiv(pp->xoff, stretchn, stretchd);

      while (p < pp)
        {
        int32_t d = -(pp->moff - p->moff);
        int32_t rightmost = p->xoff + pp->xoff - old;
        int32_t leftmost = mac_muldiv(p->xoff, stretchn, stretchd);

        /* Clef positions are stretched just a bit if they are the last thing
        in the bar. Otherwise, the position used is halfway between an
        unstretched and stretched position. */

        if (d == posx_clef)
          p->xoff = (count == 0)? rightmost - (rightmost - leftmost)/5 :
            (rightmost+leftmost)/2;

        /* Grace notes are never stretched at all; other things are stretched
        a bit, but not the full amount. */

        else if (d >= posx_gracefirst && d <= posx_gracelast)
          p->xoff = rightmost;
        else p->xoff = (rightmost + leftmost)/2;
        p++;
        }

      p++;
      }

    if (main_tracepos == INT32_MAX || main_tracepos == pl_barnumber)
      {
      p = bp->vector;
      count = bp->count;
      eprintf("-------------------------\n");
      eprintf("BAR %d (%s) STRETCHED POSITIONS:\n", i, sfb(curmovt->barvector[i]));
      while (count-- > 0)
        {
        eprintf("%8d %6d\n", p->moff, p->xoff);
        p++;
        }
      }
    }   /* End of loop to stretch bars */

  /* If this was the first system of a movement, reset the stave name structure
  to use the second name, or NULL if there isn't one. Also, if an indent is set
  for the brackets and braces, adjust the position of the joining signs. */

  if (firstsystem)
    {
    int j;
    firstsystem = FALSE;

    accepteddata->stavenames = mem_get((MAX_STAVE + 1) * sizeof(snamestr *));
    for (j = 1; j <= curmovt->laststave; j++)
      {
      snamestr *sn = ((curmovt->stavetable)[j])->stave_name;
      accepteddata->stavenames[j] = (sn != NULL)? sn->next : NULL;
      }

    if (curmovt->startbracketbar > 0 &&
        curmovt->startbracketbar <= pl_sysblock->barend)
      {
      int32_t blw = 0;
      pl_sysblock->joinxposition = pl_sysblock->firstnoteposition;

      for (i = pl_sysblock->barstart; i < curmovt->startbracketbar; i++)
        {
        barposstr *bp = curmovt->posvector + i;
        if (bp->count > 0)
          pl_sysblock->joinxposition += (bp->vector)[bp->count-1].xoff + blw;
        blw = pl_sysblock->barlinewidth;
        }
      }
    }

  /* Check that the stavespacing vector conforms to the ensure values, and if
  not, make a new one that does. At the same time, compute the total depth of
  the system. If an unsuspended stave has a zero stave spacing, make sure
  that the following stave is not suspended. */

  sysdepth = lastulevel = 0;

  for (i = 1; i <= curmovt->laststave; i++)
    {
    if (mac_isbit2(pl_stavemap, pl_sysblock->notsuspend, i))
      {
      int j = i;
      int next = i+1;

      while (j < curmovt->laststave && pl_sysblock->stavespacing[j++] == 0)
        mac_setbit(pl_sysblock->notsuspend, j);

      while (next <= curmovt->laststave &&
        mac_notbit2(pl_stavemap, pl_sysblock->notsuspend, next))
          next++;

      if (next <= curmovt->laststave)
        {
        if (pl_sysblock->stavespacing[i] < pl_ssehere[next])
          {
          if (pl_sysblock->stavespacing == pl_ssnext)
            {
            pl_sysblock->stavespacing = mem_get((MAX_STAVE+1)*sizeof(int32_t));
            memcpy(pl_sysblock->stavespacing, pl_ssnext,
              (curmovt->laststave + 1) * sizeof(int32_t));
            }
          pl_sysblock->stavespacing[i] = pl_ssehere[next];
          }
        sysdepth += pl_sysblock->stavespacing[i];
        }

      lastulevel = pl_sysblock->ulevel[i];
      }
    }
  pl_sysblock->systemdepth = sysdepth;

  /* Compute a testing depth consisting of the system depth plus the total
  depth of any footnotes, and space below the current system. And space between
  the current footnotes and any new ones. */

  sysfootdepth = sysdepth + pl_pagefootnotedepth + pl_newfootnotedepth;
  if (pl_pagefootnotedepth + pl_newfootnotedepth > 0)
    {
    sysfootdepth += -lastulevel;
    if (pl_pagefootnotedepth > 0 && pl_newfootnotedepth > 0)
      sysfootdepth += curmovt->footnotesep;
    }

  /* If this system is deeper than the page depth, we can't handle it. After
  the error, it will cause a new page to be started, but it will never be
  printed. */

  if (sysfootdepth > main_pagelength)
    {
    int32_t overflow = sysfootdepth - main_pagelength;
    error(ERR134, sfb(curmovt->barvector[pl_sysblock->barstart]),
      movtnumber, sff(overflow), (overflow == 1000)? "" : "s");
    }

  /* If we have a new movement pending, find the depth of the headings and
  see if the headings plus this system will fit on the current page. If the
  system depth is zero, we have a single-stave system, in which case we
  insist on there being room for another one as well. */

  if (movt_pending)
    {
    headstr *h = curmovt->heading;
    int32_t depth = (h == NULL)? 0 : 17000;

    while (h != NULL)
      {
      depth += h->space;
      h = h->next;
      }

    depth += (sysfootdepth == 0)? pl_sysblock->systemgap : sysfootdepth;

    /* If no room, terminate the page and start a new one. We must arrange
    that footings are printed from the *previous* movement, but take the
    option for lastfooting and pageheading from the *current* movement.
    Note that movtnumber starts at 1, but the vector starts at 0. */

    if (curpage->spaceleft < depth)
      {
      BOOL uselastfooting = MFLAG(mf_uselastfooting);   /* From curmovt */
      curmovt = movements[movtnumber - 2];
      do_endpage(uselastfooting);
      curmovt = movements[movtnumber - 1];
      do_newpage(curmovt->heading,
        MFLAG(mf_nopageheading)? NULL : curmovt->pageheading);
      }

    /* There is room: output the new heading on this page, and set the
    justification parameters from the new movement. (The horizontal ones will
    have been set already, but the vertical ones can't be changed until the
    page is known.) We also change the bottom margin, but leave the top
    margin until the next page. */

    else
      {
      pl_justify = curmovt->justify;
      pl_botmargin = curmovt->bottommargin;
      if (curmovt->heading != NULL)
        {
        do_pageheading(curmovt->heading);
        curpage->spaceleft -= 17000;
        }
      }

    /* Set up a new footing, if present. Note that if there isn't one, and we
    didn't start a new page, and there is one still set up from the previous
    movement already (in pagefooting), then it will still get printed at the
    bottom of this page. */

    if (curmovt->footing != NULL) pl_pagefooting = curmovt->footing;

    /* Cancel pending flag */

    movt_pending = FALSE;
    }

  /* If this system does not fit on the page, start a new one. */

  if (curpage->spaceleft < sysfootdepth)
    {
    curpage->overrun = sysfootdepth - curpage->spaceleft;
    do_endpage(FALSE);
    do_newpage(NULL, curmovt->pageheading);
    }

  /* Connect the system to the chain and keep count of the number of
  vertically spreadable systems on the page. */

  *pl_sysprevptr = pl_sysblock;
  pl_sysprevptr = &(pl_sysblock->next);
  pl_countsystems++;

  /* If there were any footnotes, connect them to the page's footnote list
  for inclusion at the end. Save the current spacing value for use if the
  page does actually end here. */

  if (pl_newfootnotes != NULL)
    {
    if (pl_pagefootnotes == NULL) pl_pagefootnotes = pl_newfootnotes; else
      {
      lastfootnote->next = pl_newfootnotes;
      pl_newfootnotes->spaceabove = curmovt->footnotesep;
      pl_pagefootnotedepth += curmovt->footnotesep;
      }
    lastfootnote = pl_lastnewfootnote;
    pl_pagefootnotedepth += pl_newfootnotedepth;
    }
  pl_footnotespacing = -lastulevel;

  /* Update the space left on the page; just take off the space for the music
  (the system), not the footnotes. They will be considered again with the next
  system. */

  curpage->spaceleft -= sysdepth + pl_sysblock->systemgap;
  pl_lastsystem = pl_sysblock;

  /* Update the vertical justification parameters if they changed in this
  system. */

  if (pl_sys_justify != -1)   pl_justify = pl_sys_justify;
  if (pl_sys_topmargin != -1) pl_topmargin = pl_sys_topmargin;
  if (pl_sys_botmargin != -1) pl_botmargin = pl_sys_botmargin;

  /* If we have an explicit layout, deal with advancing the pointer and
  checking for a forced new page. */

  if (layoutptr >= 0)
    {
    layoutptr++;
    for (;;)
      {
      while (curmovt->layout[layoutptr] == lv_newpage)
        {
        layoutptr++;
        if (pl_barnumber < curmovt->barcount) pl_newpagewanted = TRUE;
        }

      if (curmovt->layout[layoutptr] == lv_repeatptr)
        {
        if ((layoutstack[layoutstackptr-1] -= 1) > 0)
          layoutptr = curmovt->layout[layoutptr+1];
        else
          {
          layoutstackptr--;
          layoutptr += 2;
          }
        }
      else
        {
        while (curmovt->layout[layoutptr++] == lv_repeatcount)
          layoutstack[layoutstackptr++] = curmovt->layout[layoutptr++];
        break;
        }
      }
    }

  /* If a new page was forced after this system, set it up. This can only
  happen via [newpage] if there are more bars; hence it can't also be a
  movement end. Via explicit layout, it is also only set if there are more
  bars. */

  if (pl_newpagewanted)
    {
    do_endpage(FALSE);
    do_newpage(NULL, curmovt->pageheading);
    }

  /* Change state, either to process the next system, or to handle the end of
  the movement. */

  page_state = (pl_barnumber >= curmovt->barcount)?
    page_state_donemovt : page_state_newsystem;
  break;




  /****************************************************************************/
  /****************************************************************************/

  /* Completed a movement. Deal with the end of the whole piece or with
  starting a subsequent movement. */

  case page_state_donemovt:
  misc_tidycontstr(wk_cont, curmovt->laststave);

  if (movtnumber++ >= movement_count)
    {
    do_endpage(TRUE);
    page_done = TRUE;
    }

  /* There is another movement to follow. If it contains no staves, we must
  deal with the headings here. */

  else
    {
    movtstr *nextmovt = movements[movtnumber - 1];
    uint32_t movt_type = nextmovt->flags & mf_typeflags;

    /* Deal with the case of no staves in the movement; we must decide now
    whether or not it fits on the page if none of newpage, thispage, or
    thisline is specified. Only one of these flags is ever set. */

    if (nextmovt->barcount < 1 && movt_type == 0)
      {
      headstr *h = nextmovt->heading;
      int32_t depth = 0;
      while (h != NULL)
        {
        depth += h->space;
        h = h->next;
        }
      movt_type = (curpage->spaceleft < depth)? mf_newpage : mf_thispage;
      }

    /* Handle new page; set page_heading NULL to prevent any heading output,
    which will be done by the start-of-movt code. */

    if (movt_type == mf_newpage)
      {
      do_endpage((nextmovt->flags & mf_uselastfooting) != 0);
      curmovt = nextmovt;
      do_newpage(NULL, NULL);
      }

    /* If newpage is not set, we can't decide whether to start a new page until
    after the next system has been read. We just set a flag for the work to be
    done then. For the very special case of "thisline", we remove and vertical
    advance from the last system. Another system of the same depth will then
    always fit. We must also reduce the count of spreadable systems, since this
    one should not get additional space added to it! */

    else
      {
      curmovt = nextmovt;
      if (movt_type == mf_thisline && pl_lastsystem != NULL)
        {
        curpage->spaceleft += pl_lastsystem->systemdepth +
          pl_lastsystem->systemgap;
        pl_lastsystem->flags |= sysblock_noadvance;
        pl_countsystems--;
        }
      if (movt_type != mf_thispage) movt_pending = TRUE;
      }

    /* Change state */

    page_state = page_state_newmovt;
    }
  break;
  }

TRACE("paginate() end\n\n");
}

/* End of paginate.c */
