/*************************************************
*  PMW native stave directive reading functions  *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: February 2021 */
/* This file last modified: May 2022 */

#include "pmw.h"



/*************************************************
*          Local static variables                *
*************************************************/

static BOOL    assumeflag;   /* Set true after "assume " */
static dirstr *dir;          /* Points to found header directive */
static BOOL    nextwordread; /* TRUE if directive reads a word ahead */


/*************************************************
*            Local data tables                   *
*************************************************/

/* Clef to be assumed for pitch purposes. */

static uschar real_clef[] = {
  clef_alto,
  clef_baritone,
  clef_bass,
  clef_cbaritone,
  clef_bass,      /* contrabass */
  clef_deepbass,
  clef_treble,    /* hclef */
  clef_mezzo,
  clef_treble,    /* none */
  clef_bass,      /* soprabass */
  clef_soprano,
  clef_tenor,
  clef_treble,
  clef_treble,    /* trebledescant */
  clef_treble,    /* trebletenor */
  clef_treble     /* trebletenorB */
};

/* Octave adjustment associated with each clef. */

static int clef_octave[] = {
    0,   /* alto */
    0,   /* baritone */
    0,   /* bass */
    0,   /* cbaritone */
  -12,   /* contrabass */
    0,   /* deepbass */
    0,   /* hclef */
    0,   /* mezzo */
    0,   /* none */
   12,   /* soprabass */
    0,   /* soprano */
    0,   /* tenor */
    0,   /* treble */
   12,   /* trebledescant */
  -12,   /* trebletenor */
  -12    /* trebletenorB */
};



/*************************************************
*        Common function for dataless item       *
*************************************************/

/* These items are just a basic bstr. */

static void
p_common(void)
{
(void)mem_get_item(sizeof(bstr), dir->arg1);
}



/*************************************************
*       Common function for positive dimension   *
*************************************************/

static void
p_pvalue(void)
{
if (isdigit(read_c))
  {
  b_intvaluestr *p = mem_get_item(sizeof(b_intvaluestr), dir->arg1);
  p->value = read_fixed();
  }
else error(ERR8, "Number");
}



/*************************************************
*       Common function for signed dimension     *
*************************************************/

static void
p_svalue(void)
{
int32_t x;
b_intvaluestr *p;
if (!read_expect_integer(&x, TRUE, TRUE)) return;
p = mem_get_item(sizeof(b_intvaluestr), dir->arg1);
p->value = x;
}



/*************************************************
*      Common function for font type setting     *
*************************************************/

static void
p_fonttype(void)
{
uint16_t *p;
uint32_t f;
switch(dir->arg1)
  {
  default:  /* Should never occur */
  case 0: p = &srs.fbfont; break;
  case 1: p = &srs.textfont; break;
  case 2: p = &srs.ulfont; break;
  case 3: p = &srs.olfont; break;
  }
f = font_readtype(FALSE);
if (f != font_unknown) *p = f;
}



/*************************************************
*      Common function for font size setting     *
*************************************************/

static void
p_fontsize(void)
{
int32_t size;
uint16_t *p;
switch(dir->arg1)
  {
  default:  /* Should never occur */
  case 0: p = &srs.fbsize; break;
  case 1: p = &srs.textsize; break;
  case 2: p = &srs.ulsize; break;
  case 3: p = &srs.olsize; break;
  }
if (!read_expect_integer(&size, FALSE, FALSE)) return;
if (size == 0 || --size >= MaxFontSizes)
  {
  error(ERR75, MaxFontSizes);
  return;
  }
*p = size;
}



/*************************************************
*           Clef and octave setting              *
*************************************************/

static void
p_octave(void)
{
int32_t x;
if (!read_expect_integer(&x, FALSE, TRUE)) return;
if (x < -3 || x > +4) error(ERR168, x); else srs.octave = 24*x;
srs.lastnoteptr = NULL;
}


static void
p_clef(void)
{
b_clefstr *p = mem_get_item(sizeof(b_clefstr), b_clef);

p->clef = dir->arg1;
p->suppress = FALSE;
p->assume = assumeflag;
assumeflag = FALSE;

srs.clef = real_clef[p->clef];
srs.clef_octave = clef_octave[p->clef];
srs.lastnoteptr = NULL;

read_sigc();
if (isdigitorsign(read_c)) p_octave();
}



/*************************************************
*             Read string for [name]             *
*************************************************/

/* Returns TRUE if the string and its options ends with /" which implies that a
continuation string follows. */

static usint snflags[] = {
  snf_hcentre, snf_rightjust, snf_vcentre, snf_vertical };

static BOOL
read_name_string(snamestr *p)
{
uint32_t *ss;

/* Read the string, then check it, passing "|" as the special separator string.
This is interpreted to mean ss_verticalbar. Then count the number of lines. */

p->text = ss = string_check(string_read(font_rm, FALSE), "|", FALSE);
p->linecount = 1;
while (*ss != 0) if (PCHAR(*ss++) == ss_verticalbar) p->linecount += 1;

/* Handle options */

while (read_c == '/')
  {
  int32_t size;
  uschar *t;
  const uschar *s = US"cemv";

  read_nextc();
  if (read_c == '\"') return TRUE;

  else if ((t = Ustrchr(s, read_c)) != NULL)
    {
    p->flags |= snflags[t - s];
    read_nextc();
    }

  else switch(read_c)
    {
    case 's':
    read_nextc();
    if (!read_expect_integer(&size, FALSE, FALSE)) break;
    if (--size < 0 || size >= MaxFontSizes) error(ERR75, MaxFontSizes);
    p->size = size;
    break;

    case 'd':
    p->adjusty -= read_movevalue();
    break;

    case 'l':
    p->adjustx -= read_movevalue();
    break;

    case 'r':
    p->adjustx += read_movevalue();
    break;

    case 'u':
    p->adjusty += read_movevalue();
    break;

    default:
    return error_skip(ERR8, ']', "/d, /c, /e, /l, /m, /r, /s, /u or /v");  /* FALSE */
    }
  }

return FALSE;
}



/****************************************************************************/
/****************************************************************************/



/*************************************************
*                Assume                          *
*************************************************/

static const char *assume_list[] = {
  "alto", "baritone", "bass", "contrabass", "deepbass",
  "hclef", "key", "mezzo", "noclef", "soprabass", "soprano",
  "tenor", "time", "treble", "trebledescant", "trebletenor",
  "trebletenorb" };

static void
p_assume(void)
{
usint i;
read_nextword();
for (i = 0; i < (sizeof(assume_list)/sizeof(char *)); i++)
  {
  if (Ustrcmp(read_wordbuffer, assume_list[i]) == 0)
    {
    nextwordread = assumeflag = TRUE;
    return;
    }
  }
error_skip(ERR8, ']', "clef, key, or time setting");
}



/************************************************
*           Barlinestyle                        *
************************************************/

static void
p_barlinestyle(void)
{
int32_t x = 0;
(void)read_expect_integer(&x, FALSE, FALSE);
srs.barlinestyle = x;
}



/*************************************************
*               Barnumber                        *
*************************************************/

static void
p_barnum(void)
{
b_barnumstr *p = mem_get_item(sizeof(b_barnumstr), b_barnum);

p->x = p->y = 0;
p->flag = TRUE;
read_sigc();

if (isalpha(read_c))
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "off") == 0) p->flag = FALSE;
    else nextwordread = TRUE;
  }

else while (read_c == '/')
  {
  int sign;
  int32_t *a, b;
  read_nextc();
  switch (read_c)
    {
    case 'u': sign = +1; a = &(p->y); break;
    case 'd': sign = -1; a = &(p->y); break;
    case 'l': sign = -1; a = &(p->x); break;
    case 'r': sign = +1; a = &(p->x); break;

    default:
    error(ERR8, "/u, /d, /l, or /r");
    return;
    }
  read_nextc();
  if (!read_expect_integer(&b, TRUE, TRUE)) break;
  *a += sign * b;
  }
}



/*************************************************
*             Beamacc and Beamrit                *
*************************************************/

static void
p_beamaccrit(void)
{
b_beamaccritstr *p = mem_get_item(sizeof(b_beamaccritstr), dir->arg1);
if (isdigit(read_c))
  {
  usint n = read_usint();
  if (n != 2 && n != 3) error(ERR8, "2 or 3");
    else srs.accrit = n;
  }
p->value = srs.accrit;
}



/*************************************************
*                   Bowing                       *
*************************************************/

static void
p_bowing(void)
{
b_bowingstr *p = mem_get_item(sizeof(b_bowingstr), b_bowing);
p->value = FALSE;
read_nextword();
if (Ustrcmp(read_wordbuffer, "above") == 0) p->value = TRUE;
  else if (Ustrcmp(read_wordbuffer, "below") != 0)
    error(ERR8, "\"above\" or \"below\"");
}



/*************************************************
*                Couple                          *
*************************************************/

static void
p_couple(void)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "up") == 0) srs.couplestate = +1;
else if (Ustrcmp(read_wordbuffer, "down") == 0) srs.couplestate = -1;
else if (Ustrcmp(read_wordbuffer, "off") == 0) srs.couplestate = 0;
else error(ERR8, "\"up\", \"down\", or \"off\"");
}



/*************************************************
*                 Cue                            *
*************************************************/

static void
p_cue(void)
{
srs.noteflags = (srs.noteflags & ~nf_cuedotalign) | nf_cuesize;
if (read_c == '/')
  {
  read_nextc();
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "dotalign") == 0)
    srs.noteflags |= nf_cuedotalign; else error(ERR8, "\"dotalign\"");
  }
}



/*************************************************
*                 Dots                           *
*************************************************/

static void
p_dots(void)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "above") == 0) srs.noteflags &= ~nf_lowdot;
else if (Ustrcmp(read_wordbuffer, "below") == 0) srs.noteflags |= nf_lowdot;
else error(ERR8, "\"above\" or \"below\"");
}



/*************************************************
*              Doublenotes                       *
*************************************************/

static void
p_doublenotes(void)
{
srs.notenum *= 2;
}



/*************************************************
*               Draw & Overdraw                  *
*************************************************/

static void
p_draw(void)
{
b_drawstr *d = mem_get_item(sizeof(b_drawstr), b_draw);
d->overflag = dir->arg1;
read_draw(&(d->drawing), &(d->drawargs), ']');
}



/*************************************************
*                 Endcue                         *
*************************************************/

static void
p_endcue(void)
{
srs.noteflags &= ~(nf_cuesize|nf_cuedotalign);
}



/*************************************************
*             Endline & Endslur                  *
*************************************************/

static void
p_endline(void)
{
int id = 0;
if (read_c == '/')
  {
  read_nextc();
  if (read_c != '=') error(ERR8, "\"=\""); else
    {
    read_nextc();
    if (!isalnum(read_c)) error_skip(ERR167, ']');
    id = read_c;
    read_nextc();
    }
  }

if (srs.slurcount > 0)
  {
  b_endslurstr *p = mem_get_item(sizeof(b_endslurstr), dir->arg1);
  p->value = id;
  srs.slurcount--;
  }
else
  {
  error(ERR11, "end of slur or line - ignored");
  }
}



/*************************************************
*              Footnote                          *
*************************************************/

static void
p_footnote(void)
{
b_footnotestr *f = mem_get_item(sizeof(b_footnotestr), b_footnote);
read_headfootingtext(&(f->h), rh_footnote, ']');
}



/*************************************************
*                 Hairpins                       *
*************************************************/

static void
p_hairpins(void)
{
srs.hairpinflags = srs.hairpiny = 0;
read_nextword();
if (Ustrcmp(read_wordbuffer, "below") == 0) srs.hairpinflags = hp_below;
else if (Ustrcmp(read_wordbuffer, "middle") == 0)
  srs.hairpinflags = hp_below | hp_middle;
else if (Ustrcmp(read_wordbuffer, "above") != 0)
  {
  error(ERR8, "\"above\", \"below\", or \"middle\"");
  return;
  }

/* Default adjustment is allowed for all three positions */

read_sigc();
if (read_c == '+' || read_c == '-')
  {
  (void)read_expect_integer(&srs.hairpiny, TRUE, TRUE);
  }

/* Absolute value is allowed only for above and below */

else if ((srs.hairpinflags & hp_middle) == 0 && isdigit(read_c))
  {
  srs.hairpinflags |= hp_abs;
  srs.hairpiny = read_fixed();
  if ((srs.hairpinflags & hp_below) != 0) srs.hairpiny = -srs.hairpiny;
  }
}



/*************************************************
*               Hairpinwidth                     *
*************************************************/

static void
p_hairpinwidth(void)
{
(void)read_expect_integer(&srs.hairpinwidth, TRUE, FALSE);
}



/*************************************************
*              Halvenotes                        *
*************************************************/

static void
p_halvenotes(void)
{
if (srs.notenum > 1) srs.notenum /= 2; else srs.noteden *= 2;
}



/*************************************************
*                 Justify                        *
*************************************************/

static void
p_justify(void)
{
if (read_c != '+' && read_c != '-')
  {
  error(ERR8, "\"+\" or \"-\"");
  return;
  }

do
  {
  b_justifystr *p = mem_get_item(sizeof(b_justifystr), b_justify);
  p->value = (read_c == '+')? just_add : 0;
  read_nextc();
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "top") == 0) p->value |= just_top;
  else if (Ustrcmp(read_wordbuffer, "bottom") == 0) p->value |= just_bottom;
  else if (Ustrcmp(read_wordbuffer, "left") == 0) p->value |= just_left;
  else if (Ustrcmp(read_wordbuffer, "right") == 0) p->value |= just_right;
  else
    {
    error(ERR8, "\"top\", \"bottom\", \"left\", or \"right\"");
    return;
    }
  read_sigc();
  }
while (read_c == '+' || read_c == '-');
}



/*************************************************
*                 Key                            *
*************************************************/

static void
p_key(void)
{
BOOL warn = MFLAG(mf_keywarn);

uint32_t oldkey = srs.key_tp;
uint32_t oldwidth, newwidth;
b_keystr *p = mem_get_item(sizeof(b_keystr), b_key);

p->assume = assumeflag;
assumeflag = FALSE;
srs.key = read_key();

srs.key_tp = transpose_key(srs.key);
read_init_baraccs(read_baraccs, srs.key);
read_init_baraccs(read_baraccs_tp, srs.key_tp);

read_sigc();
if (isalpha(read_c))
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "nowarn") == 0) warn = FALSE;
    else nextwordread = TRUE;
  }

oldwidth = misc_keywidth(oldkey | key_reset, srs.clef);
newwidth = misc_keywidth(srs.key_tp, srs.clef);

/* If both the old (cancellation) width and the new key width are zero, there's
nothing we can do about warning. */

if (oldwidth == 0 && newwidth == 0) warn = FALSE;

/* If not "assume" create a cancellation key item if the new signature is empty
and the old one is not. */

if (!p->assume && newwidth == 0 && oldwidth != 0)
  {
  p->key = oldkey | key_reset;
  p->warn = warn;
  p->suppress = FALSE;
  p = mem_get_item(sizeof(b_keystr), b_key);
  p->assume = FALSE;
  }

/* Now set up the new key. */

p->key = srs.key_tp;
p->warn = warn;
p->suppress = FALSE;
}



/*************************************************
*      Midichannel, Midipitch, Midivoice,        *
*         Midivolume, Miditranspose              *
*************************************************/

/* These are all variations on the same theme. All of them create a MIDI change
item, with various different parameters. We start with a local subroutine that
they can all use.

Arguments:
  channel     channel number
  void        voice number
  note        note pitch
  volume      volumne
  transpose   transpose value

Returns:      nothing
*/

static void
makechange(int channel, int voice, int note, int volume, int transpose)
{
b_midichangestr *p = mem_get_item(sizeof(b_midichangestr), b_midichange);
p->channel = channel;
p->voice = voice;
p->note = note;
p->volume = volume;
p->transpose = transpose;
}

/*** Midichannel & Midivoice ***/

static void
p_midichanvoice(void)
{
int32_t channel = 128;  /* No change */
int voicenumber = 129;  /* No change */
int volume = 128;       /* Nochange */

if (dir->arg1)  /* Midichannel */
  {
  if (!read_expect_integer(&channel, FALSE, FALSE)) return;
  if (channel < 1 || channel > MIDI_MAXCHANNEL)
    {
    error(ERR48, "channel", channel, MIDI_MAXCHANNEL);
    return;
    }
  }

if (string_read_plain())
  {
  if (read_stringbuffer[0] == '#')
    voicenumber = Uatoi(read_stringbuffer + 1);
  else if (read_stringbuffer[0] != 0)
    voicenumber = read_getmidinumber(midi_voicenames, read_stringbuffer,
      US"voice");

  if (voicenumber < 1 || voicenumber > 128)
    {
    error(ERR48, "voice", voicenumber, 128);
    voicenumber = 1;
    }

  if (read_c == '/')
    {
    int32_t vol;
    read_nextc();
    if (read_expect_integer(&vol, FALSE, FALSE))
      {
      if (vol > 15) error(ERR8, "Number between 0 and 15"); else volume = vol;
      }
    }
  }
else if (!dir->arg1) error(ERR8, "string");  /* Mandatory for Midivoice */

makechange((int)channel, voicenumber - 1, 128, volume, 0);  /* 128 => no change */
}


/*** Midipitch ***/

static void
p_midipitch(void)
{
int note = 0;  /* No more forcing */
if (string_read_plain())
  {
  if (read_stringbuffer[0] == '#') note = Uatoi(read_stringbuffer + 1);
  else if (read_stringbuffer[0] != 0)
    note = read_getmidinumber(midi_percnames, read_stringbuffer,
      US"percussion instrument");
  makechange(128, 128, note, 128, 0);  /* 128 => no change */
  }
else error(ERR8, "string");
}


/*** Miditranspose ***/

static void
p_miditranspose(void)
{
int32_t transpose;
if (!read_expect_integer(&transpose, FALSE, TRUE)) return;
makechange(128, 128, 128, 128, transpose);
}


/*** Midivolume ***/

static void
p_midivolume(void)
{
int32_t volume;
if (!read_expect_integer(&volume, FALSE, FALSE)) return;
if (volume > 15)
  {
  error(ERR8, "Number between 0 and 15");
  return;
  }
makechange(128, 128, 128, volume, 0);
}



/*************************************************
*          Move, Rmove, Smove, & Rsmove          *
*************************************************/

static void
p_move(void)
{
b_movestr *p = mem_get_item(sizeof(b_movestr), b_move);

p->relative = dir->arg1;
if (!read_expect_integer(&(p->x), TRUE, TRUE)) return;
read_sigc();

if (read_c == ',')
  {
  read_nextc();
  if (!read_expect_integer(&(p->y), TRUE, TRUE)) return;
  }
else p->y = 0;

/* Handle Smove and Rsmove - save horizontal data to insert space after the
next note. */

if (dir->name[0] == 's' || dir->name[1] == 's')
  {
  brs.smove = p->x;
  brs.smove_isrelative = p->relative;
  }
}



/*************************************************
*                 Name                           *
*************************************************/

/* The stave magnification is used only if an explicit size is given; otherwise
the fixed size is used. This function is global because it is also called when
handling [stave <n> ... at the beginning of a stave. */

void
read_stavename(void)
{
snamestr *p, **pp;

/* Handle [name <n>] */

if (isdigit(read_c))
  {
  b_namestr *pn = mem_get_item(sizeof(b_namestr), b_name);
  pn->value = read_usint();
  return;
  }

/* Else handle any number of <string> <draw> pairs (either or both may be
present in each case) and add onto any existing chain for the stave. */

pp = &(st->stave_name);
while (*pp != NULL) pp = &((*pp)->next);
p = NULL;
read_sigc();

while(read_c == '\"' ||
      Ustrncmpic(main_readbuffer + read_i - 1, "draw ", 5) == 0)
  {
  /* If this is a string, always start a new item (snamestr). If it is a
  drawing, add onto a previous string item if there is one that doesn't already
  have a drawing. */

  if (read_c == '\"' || p == NULL || p->drawing != NULL)
    {
    p = mem_get(sizeof(snamestr));
    *p = init_snamestr;
    *pp = p;
    pp = &(p->next);
    }

  /* Read a string with possible extra strings attached. One or more additional
  strings are hung off the extra field; this allows for different options. Such
  strings must follow immediately. */

  if (read_c == '\"')
    {
    if (read_name_string(p))
      {
      snamestr **ppe = &(p->extra);
      for (;;)
        {
        snamestr *pe = *ppe = mem_get(sizeof(snamestr));
        *pe = init_snamestr;
        if (!read_name_string(pe)) break;
        ppe = &(pe->extra);
        }
      }
    }

  /* Handle a drawing */

  else
    {
    read_i += 4;
    read_c = ' ';
    read_draw(&(p->drawing), &(p->drawargs), ']');
    }

  /* Prepare for next item */

  read_sigc();
  }
}



/*************************************************
*                 Newmovement                    *
*************************************************/

/* [newmovement] is unexpected here - give a tidy error message */

static void
p_newmovement(void)
{
error(ERR102);  /* Hard */
}



/*************************************************
*                 Nocheck                        *
*************************************************/

static void
p_nocheck(void)
{
brs.checklength = FALSE;
}



/*************************************************
*                Nocount                         *
*************************************************/

static void
p_nocount(void)
{
brs.nocount = TRUE;
}



/*************************************************
*                 Noteheads                      *
*************************************************/

/* There some single-character shorthands that have a fixed notehead type. */

static void
p_noteheads(void)
{
b_noteheadsstr *p = mem_get_item(sizeof(b_noteheadsstr), b_noteheads);

if (dir->arg1 < nh_number) p->value = dir->arg1; else
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "only") == 0) p->value = nh_only;
  else if (Ustrcmp(read_wordbuffer, "direct") == 0) p->value = nh_direct;
  else if (Ustrcmp(read_wordbuffer, "normal") == 0) p->value = nh_normal;
  else if (Ustrcmp(read_wordbuffer, "harmonic") == 0) p->value = nh_harmonic;
  else if (Ustrcmp(read_wordbuffer, "cross") == 0) p->value = nh_cross;
  else if (Ustrcmp(read_wordbuffer, "none") == 0) p->value = nh_none;
  else
    {
    error(ERR8, "\"normal\", \"harmonic\", \"cross\", \"none\", \"only\", or \"direct\"");
    p->value = nh_normal;
    }
  }

if (p->value < nh_only) srs.noteflags |= nf_stem;
  else srs.noteflags &= ~nf_stem;
}



/*************************************************
*                 Notes                          *
*************************************************/

static void
p_notes(void)
{
b_notesstr *p = mem_get_item(sizeof(b_notesstr), b_notes);
read_nextword();
p->value = Ustrcmp(read_wordbuffer, "off") != 0;
if (p->value && Ustrcmp(read_wordbuffer, "on") != 0)
  error(ERR8, "\"on\" or \"off\"");
srs.noteson = p->value;
}



/*************************************************
*              Notespacing                       *
*************************************************/

/* There are three formats: multiplicative or individual additive changes, or
a reset request. */

static void
p_ns(void)
{
if (read_c == '*')               /* Multiplicative */
  {
  int32_t f;
  b_nsmstr *p;
  read_nextc();
  if (!read_expect_integer(&f, TRUE, FALSE)) return;
  if (read_c == '/')
    {
    int32_t d;
    read_nextc();
    if (!read_expect_integer(&d, TRUE, FALSE)) return;
    f = mac_muldiv(f, 1000, d);
    }
  p = mem_get_item(sizeof(b_nsmstr), b_nsm);
  p->value = f;
  }

/* Read individual change values */

else if (isdigitorsign(read_c))
  {
  int i;
  b_nsstr *p = mem_get_item(sizeof(b_nsstr), b_ns);
  for (i = 0; i < NOTETYPE_COUNT; i++) p->ns[i] = 0;
  for (i = 0; i < NOTETYPE_COUNT; i++)
    {
    read_sigc();
    if (!isdigitorsign(read_c)) break;
    if (!read_expect_integer(&(p->ns[i]), TRUE, TRUE)) break;
    if (read_c == ',') read_nextc();
    }
  if (i == 1) error(ERR104);  /* Single change only may be a typo: warn */
  }

/* Reset to original values */

else mem_get_item(sizeof(bstr), b_ens);
}



/*************************************************
*                 Octave                         *
*************************************************/

/* p_octave() appears higher up so that it can be called from p_clef() */




/*************************************************
*                   Olevel/Ulevel                *
*************************************************/

static void
p_uolevel(void)
{
b_uolevelstr *p = mem_get_item(sizeof(b_uolevelstr), dir->arg1);
if (read_c == '*')
  {
  p->value = FIXED_UNSET;
  read_nextc();
  }
else (void) read_expect_integer(&(p->value), TRUE, TRUE);
}



/*************************************************
*                Omitempty                       *
*************************************************/

/* This is obsolete; "omitempty" is now part of the "stave" directive. Give a
minor error and ignore it. */

static void
p_omitempty(void)
{
error(ERR165);
}



/*************************************************
*                  Page                          *
*************************************************/

static void
p_page(void)
{
b_pagestr *p = mem_get_item(sizeof(b_pagestr), b_page);
if (read_c == '+')
  {
  p->relative = TRUE;
  read_nextc();
  }
else p->relative = FALSE;
(void)read_expect_integer(&(p->value), FALSE, FALSE);
}



/*************************************************
*               Printpitch                       *
*************************************************/

static void
p_printpitch(void)
{
if (read_c == '*')
  {
  srs.printpitch = 0;
  read_nextc();
  }
else srs.printpitch = read_stavepitch();
}



/*************************************************
*                  Reset                         *
*************************************************/

static void
p_reset(void)
{
(void)mem_get_item(sizeof(bstr), b_reset);

/* Do things that are otherwise done at bar end */

if (srs.beaming) read_setbeamstems();
if (brs.barlength > brs.maxbarlength) brs.maxbarlength = brs.barlength;

/* Test for a valid reset */

if (!brs.resetOK)
  error((brs.barlength == 0)? ERR105 : ERR106);
    else if (brs.pletlen != 0) error(ERR107);

/* We do the action anyway, to prevent spurious over-long bar errors. */

read_init_baraccs(read_baraccs, srs.key);
brs.barlength = 0;
brs.resetOK = FALSE;
}



/*************************************************
*               Resume/Suspend                   *
*************************************************/

static void
p_ressus(void)
{
(void)mem_get_item(sizeof(bstr), (dir->arg1)? b_suspend : b_resume);
srs.suspended = dir->arg1;
}



/*************************************************
*                Rlevel                          *
*************************************************/

static void
p_rlevel(void)
{
(void)read_expect_integer(&srs.rlevel, TRUE, TRUE);
}



/*************************************************
*              Rspace & Space                    *
*************************************************/

static void
p_rspace(void)
{
b_spacestr *p = mem_get_item(sizeof(b_spacestr), b_space);
p->relative = dir->arg1;
(void)read_expect_integer(&(p->x), TRUE, TRUE);
}



/*************************************************
*            Sgabove, Sghere, Sgnext             *
*************************************************/

static void
p_sg(void)
{
b_sgstr *p = mem_get_item(sizeof(b_sgstr), dir->arg1);
p->relative = (read_c == '+' || read_c == '-');
(void)read_expect_integer(&(p->value), TRUE, TRUE);
}



/*************************************************
*                  Skip                          *
*************************************************/

static void
p_skip(void)
{
int32_t x;
if (brs.bar->next != NULL) error(ERR103);  /* Warn if not at bar start */
(void)read_expect_integer(&x, FALSE, FALSE);
brs.skip += x;
}



/*************************************************
*              Slur and line                     *
*************************************************/

/* A line is just a special kind of slur. The basic slur structure is quite
small; separate structures are used for sets of modifications. They are chained
together in reverse order. The sequence number 0 means "the unsplit slur" while
other counts are for parts of a split slur. For backwards compatiblity, we
retain the following synonyms:

  sly = 1ry
  sry = 2ly
  slc = 1c
  src = 2c

A local subroutine is used to find the relevant slurmod on the chain, or to
create a new one if it isn't found.

Arguments:
  sequence     the sequence number
  anchor       points to the anchor of the chain

Returns:       pointer to the required slurmod
*/

static b_slurmodstr *
findmods(usint sequence, b_slurmodstr **anchor)
{
b_slurmodstr *m;
for (m = *anchor; m != NULL; m = m->next)
  if (m->sequence == sequence) return m;
m = mem_get(sizeof(b_slurmodstr));
memset(m, 0, sizeof(b_slurmodstr));
m->next = *anchor;
*anchor = m;
m->sequence = sequence;
return m;
}

/* The slur/line function */

static void
p_slur(void)
{
b_slurmodstr *mod;
b_slurstr *p = mem_get_item(sizeof(b_slurstr), b_slur);

mod = p->mods = NULL;
p->flags = dir->arg1;
p->ally = 0;
p->id = 0;

srs.slurcount++;

/* Loop to read the many options. */

while (read_c == '/')
  {
  BOOL left, in;
  int32_t x;

  read_nextsigc();

  /* Some things may appear only before the first split number qualifier. */

  if (mod != NULL && mod->sequence != 0)
    {
    if (strchr("=abeshiow", read_c) != NULL) error(ERR99, read_c);
    }

  switch (read_c)
    {
    case '=':
    read_nextc();
    if (!isalnum(read_c))
      {
      error_skip(ERR167, ']');
      break;
      }
    p->id = read_c;
    read_nextc();
    break;

    case 'a':
    p->flags &= ~(sflag_b | sflag_abs | sflag_lay);
    read_nextc();
    if (read_c == 'o')
      {
      read_nextc();
      p->flags |= sflag_lay;
      }
    else if (isdigitorsign(read_c))
      {
      if (!read_expect_integer(&x, TRUE, TRUE)) return;
      p->flags |= sflag_abs;
      p->ally += x;
      }
    break;

    case 'b':
    p->flags = (p->flags & ~(sflag_abs | sflag_lay)) | sflag_b;
    read_nextc();
    if (read_c == 'u')
      {
      read_nextc();
      p->flags |= sflag_lay;
      }
    else if (isdigitorsign(read_c))
      {
      if (!read_expect_integer(&x, TRUE, TRUE)) return;
      p->flags |= sflag_abs;
      p->ally -= x;
      }
    break;

    case 'c':
    read_nextc();
    if (read_c == 'x')
      {
      read_nextc();
      p->flags |= sflag_cx;
      break;
      }

    if (mod == NULL) mod = findmods(0, &(p->mods));
    if ((in = read_c == 'i') || read_c == 'o')
      {
      read_nextc();
      if (!read_expect_integer(&x, TRUE, FALSE)) return;
      mod->c += in? -x : x;
      }

    else if ((left = read_c == 'l') || read_c == 'r')
      {
      read_nextc();
      if (read_c == 'u' || read_c == 'd' || read_c == 'l' || read_c == 'r')
        {
        int cc = read_c;
        read_nextc();
        if (!read_expect_integer(&x, TRUE, FALSE)) return;
        switch(cc)
          {
          case 'u':
          if (left) mod->cly += x; else mod->cry += x;
          break;

          case 'd':
          if (left) mod->cly -= x; else mod->cry -= x;
          break;

          case 'l':
          if (left) mod->clx -= x; else mod->crx -= x;
          break;

          case 'r':
          if (left) mod->clx += x; else mod->crx += x;
          break;
          }
        }
      else error(ERR8, "clu, cld, cll, clr, cru, crd, crl, or crr");
      }
    else error(ERR8, "ci, co, clu, cld, cll, clr, cru, crd, crl, or crr");
    break;

    case 'u':
    read_nextc();
    if (!read_expect_integer(&x, TRUE, FALSE)) return;
    if (mod == NULL || mod->sequence == 0) p->ally += x; else
      {
      mod->ly += x;
      mod->ry += x;
      }
    break;

    case 'd':
    read_nextc();
    if (!read_expect_integer(&x, TRUE, FALSE)) return;
    if (mod == NULL || mod->sequence == 0) p->ally -= x; else
      {
      mod->ly -= x;
      mod->ry -= x;
      }
    break;

    case 'e':
    p->flags |= sflag_e;
    read_nextc();
    break;

    case 'l':
    case 'r':
    if (mod == NULL) mod = findmods(0, &(p->mods));

    x = (read_c == 'l')? 0 : 1;
    read_nextc();

    switch(read_c)
      {
      case 'l':
      break;

      case 'r':
      x |= 2;
      break;

      case 'd':
      x |= 4;
      break;

      case 'u':
      x |= 6;
      break;

      default:
      x = -1;
      break;
      }

    read_nextc();
    if (read_c == 'c')
      {
      read_nextc();
      if ((x & 4) == 0) x |= 8; else x = -1;
      }

    if (x < 0)
      {
      error(ERR8, "lu, ld, ll, llc, lr, lrc, ru, rd, rl, rlc, rr, or rrc");
      }
    else
      {
      int32_t s;
      int32_t *z = NULL;  /* Stop compiler unset warning */

      switch (x)   /* 12-15 won't occur because c is only with left/right */
        {
        case 0:  /* ll */
        case 2:  /* lr */
        z = &(mod->lx);
        x -= 1;  /* -1 or +1 */
        break;

        case 1:  /* rl */
        case 3:  /* rr */
        z = &(mod->rx);
        x -= 2;  /* -1 or +1 */
        break;

        case 4:  /* ld */
        case 6:  /* lu */
        z = &(mod->ly);
        x -= 5;  /* -1 or +1 */
        break;

        case 5:  /* rd */
        case 7:  /* ru */
        z = &(mod->ry);
        x -= 6;  /* -1 or +1 */
        break;

        case 8:  /* llc */
        case 10: /* lrc */
        z = &(mod->lxoffset);
        x -= 9;  /* -1 or +1 */
        break;

        case 9:  /* rlc */
        case 11: /* rrc */
        z = &(mod->rxoffset);
        x -= 10; /* -1 or +1 */
        break;
        }

      if (!read_expect_integer(&s, TRUE, FALSE)) return;
      *z += x*s;
      }
    break;

    case 'h':
    p->flags |= sflag_h;
    read_nextc();
    break;

    case 'i':
    p->flags |= sflag_i;
    read_nextc();
    if (read_c == 'p')
      {
      p->flags |= sflag_idot;
      read_nextc();
      }
    break;

    case 'o':
    read_nextc();
    if (read_c == 'l') p->flags |= sflag_ol;
      else if (read_c == 'r') p->flags |= sflag_or;
        else error(ERR8, "ol or or");
    read_nextc();
    break;

    case 'w':
    p->flags |= sflag_w;
    read_nextc();
    break;

    default:
    if (isdigit(read_c))
      {
      usint n = read_usint();
      if (n == 0) error(8, "number greater than zero");
      mod = findmods(n, &(p->mods));
      read_sigc();
      }
    else error(ERR8, "=, a, b, w, ci, co, d, e, u, lu, ld, ru, rd, h, i, ol, or, or number");
    break;
    }

  read_sigc();
  }

/* We don't allow wiggly with line slurs */

if ((p->flags & (sflag_l|sflag_w)) == (sflag_l|sflag_w)) error(ERR100, "lines");
}



/*************************************************
*             Slurgap & Linegap                  *
*************************************************/

static void
p_slurgap(void)
{
b_slurgapstr *p = mem_get_item(sizeof(b_slurgapstr),
  (dir->arg1)? b_slurgap : b_linegap);

p->drawing = NULL;
p->drawargs = NULL;
p->gaptext = NULL;
p->hfraction = -1;
p->xadjust = 0;
p->width = -1;
p->textx = p->texty = 0;
p->textflags = p->textsize = 0;
p->id = 0;

/* Read the options */

while (read_c == '/')
  {
  int32_t x;
  read_nextsigc();
  switch (read_c)
    {
    case '=':
    read_nextc(); p->id = read_c; read_nextsigc();
    break;

    case 'd':
    if (Ustrncmp(main_readbuffer + read_i, "raw ", 4) == 0)
      {
      read_i += 3;
      read_nextc();
      read_draw(&(p->drawing), &(p->drawargs), ']');
      }
    else error(ERR8, "\"draw\"");
    break;

    case 'h':
    read_nextc();
    if (isdigit(read_c))
      {
      if (!read_expect_integer(&(p->hfraction), TRUE, FALSE)) return;
      }
    else p->hfraction = 500;
    break;

    case 'l':
    read_nextc();
    if (!read_expect_integer(&x, TRUE, FALSE)) return;
    p->xadjust -= x;
    break;

    case 'r':
    read_nextc();
    if (!read_expect_integer(&x, TRUE, FALSE)) return;
    p->xadjust += x;
    break;

    case 'w':
    read_nextc();
    if (!read_expect_integer(&(p->width), TRUE, FALSE)) return;
    break;

    case '\"':
    p->gaptext = string_read(font_rm, TRUE);
    while (read_c == '/')
      {
      read_nextc();
      switch (read_c)
        {
        case 'b':
        if (Ustrncmp(main_readbuffer + read_i, "ox", 2) == 0)
          {
          read_i += 2;
          read_nextc();
          p->textflags |= text_boxed;
          }
        else error(ERR8, "/box, /ring, or /s");
        break;

        case 'd':
        p->texty -= read_movevalue();
        break;

        case 'l':
        p->textx -= read_movevalue();
        break;

        case 's':
        read_nextc();
        read_expect_integer(&x, FALSE, FALSE);
        if (x == 0 || --x  >= MaxFontSizes)
          {
          error(ERR75, MaxFontSizes);
          x = 0;
          }
        p->textsize = x;
        break;

        case 'r':
        if (Ustrncmp(main_readbuffer + read_i, "ing", 3) == 0)
          {
          read_i += 3;
          read_nextc();
          p->textflags |= text_ringed;
          }
        else p->textx += read_movevalue();
        break;

        case 'u':
        p->texty += read_movevalue();
        break;

        default:
        error(ERR8, "/box, /ring, or /s");
        break;
        }
      }
    break;

    default:
    error(ERR8, "=, l, r, w, or a string");
    break;
    }

  read_sigc();
  }

if (srs.slurcount == 0)
  error(ERR13, (dir->arg1)? "unexpected [slurgap]" : "unexpected [linegap]");

/* Width defaults to width of text plus the fontsize, or 4 points if no text. */

if (p->width < 0)
  {
  if (p->gaptext == NULL) p->width = 4000; else
    {
    fontinststr *fdata = &(curmovt->fontsizes->fontsize_text[p->textsize]);
    p->width = mac_muldiv(string_width(p->gaptext, fdata, NULL) + fdata->size,
      curmovt->stavesizes[srs.stavenumber], 1000);
    }
  }
}



/*************************************************
*             Ssabove, Sshere, Ssnext            *
*************************************************/

static void
p_ss(void)
{
uint64_t done = 0;

for (;;)
  {
  int value, stave;
  BOOL relative = (read_c == '+' || read_c == '-');

  if (!read_expect_integer(&value, TRUE, TRUE)) return;

  if (read_c != '/') stave = srs.stavenumber; else
    {
    if (relative || value < 0 || (value%1000) != 0)
      {
      error(ERR8, "Stave number");
      return;
      }
    stave = value/1000;
    read_nextc();
    relative = (read_c == '+' || read_c == '-');
    if (!read_expect_integer(&value, TRUE, TRUE)) return;
    }

  if (stave > MAX_STAVE) error(ERR10); else
    {
    b_ssstr *p = mem_get_item(sizeof(b_ssstr), dir->arg1);
    p->relative = relative;
    p->stave = stave;
    p->value = value;
    }

  if ((done & (1 << stave)) != 0 ) error(ERR54, stave, dir->name);
  done |= (1 << stave);
  read_sigc();
  if (read_c == ',') read_nextsigc();
  if (!isdigitorsign(read_c)) break;
  }
}



/*************************************************
*               Stavelines                       *
*************************************************/

/* This directive is now deprecated */

static void
p_stavelines(void)
{
int32_t n;
if (!read_expect_integer(&n, FALSE, FALSE)) return;
if (n > 6) error(ERR8, "Number in the range 0-6");
  else st->stavelines = n;
error(ERR170);  /* Warning */
}



/*************************************************
*               Stemlength (aka Sl)              *
*************************************************/

static void
p_stemlength(void)
{
(void)read_expect_integer(&srs.stemlength, TRUE, TRUE);
}



/*************************************************
*                 Stems/Ties                     *
*************************************************/

static void
p_stems(void)
{
int8_t *p = (dir->arg1)? &srs.stemsdirection : &srs.tiesplacement;
read_nextword();
if (Ustrcmp(read_wordbuffer, "auto") == 0) *p = 0;
else if (Ustrcmp(read_wordbuffer, "up") == 0 || Ustrcmp(read_wordbuffer, "above") == 0) *p = +1;
else if (Ustrcmp(read_wordbuffer, "down") == 0 || Ustrcmp(read_wordbuffer, "below") == 0) *p = -1;
else error(ERR8, "\"auto\", \"above\", \"up\", \"below\", or \"down\"");
}



/*************************************************
*                  Text                          *
*************************************************/

static void
p_text(void)
{
int sign = 1;

read_nextword();
srs.textabsolute = 0;

if (Ustrcmp(read_wordbuffer, "underlay") == 0) srs.textflags = text_ul;
else if (Ustrcmp(read_wordbuffer, "overlay") == 0) srs.textflags = text_ul | text_above;
else if (Ustrcmp(read_wordbuffer, "fb") == 0) srs.textflags = text_fb;

else
  {
  if (Ustrcmp(read_wordbuffer, "above") == 0) srs.textflags = text_above;
  else if (Ustrcmp(read_wordbuffer, "below") == 0)
    {
    srs.textflags = 0;
    sign = -1;
    }
  else
    {
    error(ERR8, "\"underlay\", \"fb\", \"above\", or \"below\"");
    return;
    }

  /* Check for absolute setting */

  read_sigc();
  if (isdigit(read_c))
    {
    srs.textflags |= text_absolute;
    srs.textabsolute = sign*read_fixed();
    }
  }
}



/*************************************************
*                 Time                           *
*************************************************/

static void
p_time(void)
{
b_timestr *p;
uint32_t t = read_time();
uint32_t tt;

if (t == 0) return;          /* Error in time signature */
tt = t = read_scaletime(t);  /* Scale to header double/halve */

p = mem_get_item(sizeof(b_timestr), b_time);
p->time = t;
p->assume = assumeflag;
p->warn = MFLAG(mf_timewarn);
p->suppress = !MFLAG(mf_showtime);
assumeflag = FALSE;

/* If the time signature is followed by "->" we read a second signature to
which bars are to be musically stretched or compressed. This makes it possible
to have different non-compatible time signatures on different staves, for
example, 6/8 vs 2/4. */

read_sigc();
if (read_c == '-' && main_readbuffer[read_i] == '>')
  {
  uint32_t t3;
  read_i++;
  read_nextc();
  t3 = read_time();
  if (t3 != 0) tt = read_scaletime(t3);
  }

/* The required bar length is the length of the target time signature. */

srs.required_barlength = read_compute_barlength(tt);

/* If the two signatures are different, set up adjustment numerator and
denominator. So as not to waste time multiplying in the common case, indicate
"unset" with numerator == 0. */

if (t == tt) srs.matchnum = 0; else
  {
  srs.matchnum = srs.required_barlength;
  srs.matchden = read_compute_barlength(t);
  }

/* Now test for "nowarn" */

read_sigc();
if (isalpha(read_c))
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "nowarn") == 0) p->warn = FALSE;
    else nextwordread = TRUE;
  }
}



/*************************************************
*                 Transpose                      *
*************************************************/

/* A stave transpose does not of itself change the key signature. This is
a facility, not a bug! However, we must call the routine in order to set up the
letter count for transposing notes. The yield is discarded. */

static void
p_transpose(void)
{
int32_t x;
if (!read_expect_integer(&x, FALSE, TRUE)) return;
if (active_transpose == NO_TRANSPOSE) active_transpose = 0;
active_transpose += x * 2;  /* Quarter tones */
if (abs(active_transpose) > MAX_TRANSPOSE)
  error(ERR64, (active_transpose == x)? "":"accumulated ", active_transpose/2,
    MAX_TRANSPOSE/2);  /* Hard error */
(void)transpose_key(srs.key);
srs.lastnoteptr = NULL;
}



/*************************************************
*                Transposedacc                   *
*************************************************/

static void
p_transposedacc(void)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "force") == 0) active_transposedaccforce = TRUE;
  else if (Ustrcmp(read_wordbuffer, "noforce") == 0) active_transposedaccforce = FALSE;
    else error(ERR8, "\"force\" or \"noforce\"");
}



/*************************************************
*                 Tremolo                        *
*************************************************/

/* Force a beam break beforehand if we are beaming. */

static void
p_tremolo(void)
{
b_beambreakstr *bb;
b_tremolostr *p;
int32_t count = 2;
int32_t join = 0;

if (srs.beaming)
  {
  bb = mem_get_item(sizeof(b_beambreakstr), b_beambreak);
  bb->value = BEAMBREAK_ALL;
  }

while (read_c == '/')
  {
  read_nextc();
  if (read_c == 'x' || read_c == 'j')
    {
    int32_t *xp = (read_c == 'x')? &count : &join;
    read_nextc();
    if (!read_expect_integer(xp, FALSE, FALSE)) return;
    }
  else error(ERR8, "\"x\" or \"j\"");
  read_sigc();
  }

p = mem_get_item(sizeof(b_tremolostr), b_tremolo);
p->count = count;
p->join = join;
}



/*************************************************
*               Tripletize                       *
*************************************************/

static void
p_tripletize(void)
{
if (isalpha(read_c)) read_nextword();
  else Ustrcpy(read_wordbuffer, "on");
if (Ustrcmp(read_wordbuffer, "off") == 0)
  {
  srs.noteflags &= ~nf_tripletize;
  }
else
  {
  if (Ustrcmp(read_wordbuffer, "on") != 0) nextwordread = TRUE;
  srs.noteflags |= nf_tripletize;
  brs.checktripletize = TRUE;   /* Check completed bar for tripletizing */
  }
}



/*************************************************
*               Triplets                         *
*************************************************/

static void
p_triplets(void)
{
BOOL hadone = FALSE;
BOOL onflag = TRUE;
b_bytevaluestr *p = mem_get_item(sizeof(b_tripswstr), b_tripsw);

for (;;)
  {
  read_nextword();

  if (Ustrcmp(read_wordbuffer, "above") == 0 ||
      Ustrcmp(read_wordbuffer, "below") == 0)
    {
    srs.pletflags &= ~(plet_a | plet_b | plet_abs);
    srs.pletflags |= (read_wordbuffer[0] == 'a')? plet_a : plet_b;
    srs.plety = 0;
    read_sigc();
    if (read_c == '+' || read_c == '-')
      (void)read_expect_integer(&srs.plety, TRUE, TRUE);
    else if (isdigit(read_c))
      {
      srs.pletflags |= plet_abs;
      srs.plety = read_fixed();
      if ((srs.pletflags & plet_b) != 0) srs.plety = -srs.plety;
      }
    }

  else if (Ustrcmp(read_wordbuffer, "auto") == 0)
    {
    srs.pletflags &= ~(plet_a | plet_b | plet_abs | plet_bn | plet_by);
    srs.plety = 0;
    }

  else if (Ustrcmp(read_wordbuffer, "bracket") == 0 ||
           Ustrcmp(read_wordbuffer, "nobracket") == 0)
    {
    srs.pletflags &= ~(plet_by | plet_bn);
    srs.pletflags |= (read_wordbuffer[0] == 'b')? plet_by : plet_bn;
    }

  else if (Ustrcmp(read_wordbuffer, "off") == 0) onflag = FALSE;
  else if (Ustrcmp(read_wordbuffer, "on") == 0) onflag = TRUE;

  else  /* Unrecognized */
    {
    nextwordread = read_wordbuffer[0] != 0;
    break;
    }

  hadone = TRUE;
  }

if (!hadone) error(ERR8, "\"above\", \"below\", \"auto\", \"[no]bracket\", "
                         "\"on\", or \"off\"");
p->value = onflag;
}



/*************************************************
*           Table of stave directives            *
*************************************************/

static dirstr read_stavedirlist[] = {
  { "all",            p_common,        b_all, TRUE },
  { "alto",           p_clef,          clef_alto, FALSE },
  { "assume",         p_assume,        0, TRUE },
  { "baritone",       p_clef,          clef_baritone, FALSE },
  { "barlinestyle",   p_barlinestyle,  0, TRUE },
  { "barnumber",      p_barnum,        0, TRUE },
  { "bass",           p_clef,          clef_bass, FALSE },
  { "beamacc",        p_beamaccrit,    b_beamacc, FALSE },
  { "beammove",       p_svalue,        b_beammove, TRUE },
  { "beamrit",        p_beamaccrit,    b_beamrit, FALSE },
  { "beamslope",      p_svalue,        b_beamslope, TRUE },
  { "bottommargin",   p_pvalue,        b_pagebotmargin, TRUE },
  { "bowing",         p_bowing,        0, TRUE },
  { "breakbarline",   p_common,        b_breakbarline, TRUE },
  { "cbaritone",      p_clef,          clef_cbaritone, FALSE },
  { "comma",          p_common,        b_comma, FALSE },
  { "contrabass",     p_clef,          clef_contrabass, FALSE },
  { "copyzero",       p_svalue,        b_zerocopy, TRUE },
  { "couple",         p_couple,        0, TRUE },
  { "cue",            p_cue,           0, TRUE },
  { "deepbass",       p_clef,          clef_deepbass, FALSE },
  { "dots",           p_dots,          0, TRUE },
  { "doublenotes",    p_doublenotes,   0, TRUE },
  { "draw",           p_draw,          FALSE, FALSE },
  { "el",             p_endline,       b_endline, TRUE },
  { "endcue",         p_endcue,        0, TRUE },
  { "endline",        p_endline,       b_endline, TRUE },
  { "endslur",        p_endline,       b_endslur, TRUE },
  { "endstaff",       NULL,            0, TRUE },
  { "endstave",       NULL,            0, TRUE },
  { "ensure",         p_pvalue,        b_ensure, FALSE },
  { "es",             p_endline,       b_endslur, TRUE },
  { "fbfont",         p_fonttype,      0, TRUE },
  { "fbtextsize",     p_fontsize,      0, TRUE },
  { "footnote",       p_footnote,      0, TRUE },
  { "h",              p_noteheads,     nh_harmonic, TRUE },
  { "hairpins",       p_hairpins,      0, TRUE },
  { "hairpinwidth",   p_hairpinwidth,  0, TRUE },
  { "halvenotes",     p_halvenotes,    0, TRUE },
  { "hclef",          p_clef,          clef_hclef, FALSE },
  { "justify",        p_justify,       0, TRUE },
  { "key",            p_key,           0, FALSE },
  { "line",           p_slur,          sflag_l, FALSE },
  { "linegap",        p_slurgap,       FALSE, FALSE },
  { "mezzo",          p_clef,          clef_mezzo, FALSE },
  { "midichannel",    p_midichanvoice, TRUE, TRUE },
  { "midipitch",      p_midipitch,     0, TRUE },
  { "miditranspose",  p_miditranspose, 0, TRUE },
  { "midivoice",      p_midichanvoice, FALSE, TRUE },
  { "midivolume",     p_midivolume,    0, TRUE },
  { "move",           p_move,          FALSE, FALSE },
  { "name",           read_stavename,  0, TRUE },
  { "newline",        p_common,        b_newline, TRUE },
  { "newmovement",    p_newmovement,   0, TRUE },
  { "newpage",        p_common,        b_newpage, TRUE },
  { "nocheck",        p_nocheck,       0, TRUE },
  { "noclef",         p_clef,          clef_none, FALSE },
  { "nocount",        p_nocount,       0, TRUE },
  { "noteheads",      p_noteheads,     nh_number, TRUE },
  { "notes",          p_notes,         0, TRUE },
  { "notespacing",    p_ns,            0, TRUE },
  { "ns",             p_ns,            0, TRUE },
  { "o",              p_noteheads,     nh_normal, TRUE },
  { "octave",         p_octave,        0, TRUE },
  { "olevel",         p_uolevel,       b_olevel, TRUE },
  { "olhere",         p_svalue,        b_olhere, TRUE },
  { "oltextsize",     p_fontsize,      3, TRUE },
  { "omitempty",      p_omitempty,     0, TRUE },
  { "overdraw",       p_draw,          TRUE, FALSE },
  { "overlayfont",    p_fonttype,      3, TRUE },
  { "page",           p_page,          0, TRUE },
  { "printpitch",     p_printpitch,    0, TRUE },
  { "reset",          p_reset,         0, TRUE },
  { "resume",         p_ressus,        FALSE, TRUE },
  { "rlevel",         p_rlevel,        0, TRUE },
  { "rmove",          p_move,          TRUE, FALSE },
  { "rsmove",         p_move,          TRUE, FALSE },
  { "rspace",         p_rspace,        TRUE, FALSE },
  { "sgabove",        p_sg,            b_sgabove, TRUE },
  { "sghere",         p_sg,            b_sghere, TRUE },
  { "sgnext",         p_sg,            b_sgnext, TRUE },
  { "skip",           p_skip,          0, TRUE },
  { "sl",             p_stemlength,    0, TRUE },
  { "slur",           p_slur,          0, FALSE },
  { "slurgap",        p_slurgap,       TRUE, FALSE },
  { "smove",          p_move,          FALSE, FALSE },
  { "soprabass",      p_clef,          clef_soprabass, FALSE },
  { "soprano",        p_clef,          clef_soprano, FALSE },
  { "space",          p_rspace,        FALSE, FALSE },
  { "ssabove",        p_ss,            b_ssabove, TRUE },
  { "sshere",         p_ss,            b_sshere, TRUE },
  { "ssnext",         p_ss,            b_ssnext, TRUE },
  { "stafflines",     p_stavelines,    0, TRUE },
  { "stavelines",     p_stavelines,    0, TRUE },
  { "stemlength",     p_stemlength,    0, TRUE},
  { "stems",          p_stems,         TRUE, TRUE },
  { "suspend",        p_ressus,        TRUE, TRUE },
  { "tenor",          p_clef,          clef_tenor, FALSE },
  { "text",           p_text,          0, TRUE },
  { "textfont",       p_fonttype,      1, TRUE },
  { "textsize",       p_fontsize,      1, TRUE },
  { "tick",           p_common,        b_tick, FALSE },
  { "ties",           p_stems,         FALSE, TRUE },
  { "time",           p_time,          0, FALSE },
  { "topmargin",      p_pvalue,        b_pagetopmargin, TRUE },
  { "transpose",      p_transpose,     0, TRUE },
  { "transposedacc",  p_transposedacc, 0, TRUE },
  { "treble",         p_clef,          clef_treble, FALSE },
  { "trebledescant",  p_clef,          clef_trebledescant, FALSE },
  { "trebletenor",    p_clef,          clef_trebletenor, FALSE },
  { "trebletenorb",   p_clef,          clef_trebletenorB, FALSE },
  { "tremolo",        p_tremolo,       0, TRUE },
  { "tripletize",     p_tripletize,    0, TRUE },
  { "triplets",       p_triplets,      0, TRUE },
  { "ulevel",         p_uolevel,       b_ulevel, TRUE },
  { "ulhere",         p_svalue,        b_ulhere, TRUE },
  { "ultextsize",     p_fontsize,      2, TRUE },
  { "unbreakbarline", p_common,        b_unbreakbarline, TRUE },
  { "underlayfont",   p_fonttype,      2, TRUE },
  { "x",              p_noteheads,     nh_cross, TRUE },
  { "xline",          p_slur,          sflag_x+sflag_l, FALSE },
  { "xslur",          p_slur,          sflag_x, FALSE },
  { "z",              p_noteheads,     nh_none, TRUE }

};

static int read_stavedirsize = sizeof(read_stavedirlist)/sizeof(dirstr);



/*************************************************
*            Search directive list               *
*************************************************/

/* This function is used below, but is also called when processing the header
directive "printkey" in order to check a clef name. This avoids having multiple
lists for clefs. The directive name is already in read_wordbuffer. */

dirstr *
read_stave_searchdirlist(BOOL justclef)
{
dirstr *first = read_stavedirlist;
dirstr *last  = first + read_stavedirsize;

while (last > first)
  {
  int c;
  dirstr *d = first + (last-first)/2;
  c = Ustrcmp(read_wordbuffer, d->name);
  if (c == 0)
    {
    if (justclef && d->proc != p_clef) return NULL;
    return d;
    }
  if (c > 0) first = d + 1; else last = d;
  }

return NULL;
}



/*************************************************
*            Handle a stave directive            *
*************************************************/

/* The directive name is already in read_wordbuffer. The nextwordread variable
is set by a directive if it has read a word ahead that is not one of its
keywords. When this occurs, we process that word too.

Arguments:  the current head of bar structure
Returns:    TRUE when [endstave] reached
*/

BOOL
read_do_stavedirective(void)
{
do
  {
  dir = read_stave_searchdirlist(FALSE);
  if (dir == NULL)
    {
    error_skip(ERR86, ']', read_wordbuffer);
    return FALSE;
    }
  else
    {
    if (dir->proc == NULL) return TRUE;  /* [endstave] */
    nextwordread = FALSE;
    read_sigc();
    (dir->proc)();
    if (!dir->arg2) brs.resetOK = FALSE;  /* [reset] not allowed */
    }
  }
while (nextwordread);

return FALSE;
}


/* End of pmw_read_stavedirs.c */
