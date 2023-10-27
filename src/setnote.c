/*************************************************
*        PMW code for setting one note/chord     *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: June 2021 */
/* This file last modified: October 2023 */

#include "pmw.h"




/*************************************************
*        Tables used only in this module         *
*************************************************/

/* Data for long rest bars; [0] is for general symbol, [2-8] for special
symbols. */

static int32_t longrest_widths[] =
  { 30000, 0, 4000, 11500, 4000, 11500, 10700, 18200, 10700 };

static uschar common_notes[] = {
  49, 50, 52, 54, 56, 58,       /* stems down */
  49, 50, 51, 53, 55, 57};      /* stems up */

static uschar *reststrings[] = {
  US"*", US"+", US",", US"-", US".", US"z.w{{y.",
  US"zzx.w{{y.w{{y.", US"zzzx.w{{y.w{{y.w{{y." };

static uschar *multireststrings[] = {  /* Start for 2, last is for 8 */
  US"*", US"*z+", US"*{{w*", US"*{{w*xz+", US"*{{w*xz*",
  US"*{{w*xz*z+", US"*{{w*xz*{{w*" };

static uschar *tailstrings[] = {
  US"",  US"",       US"",            US"",
  US"H", US"<xv<v|", US"<xv<xv<v|v|", US"<xv<xv<xv<v|v|v|",
  US"",  US"",       US"",            US"",
  US"E", US";v|;xv", US";v|;v|;xvxv", US";v|;v|;v|;xvxvxv" };

/* This table must be kept instep with the definitions of notehead types
(nh_xx) in pmw.h. Each line specifies the noteheads for each type of note,
starting with breve. */

static uschar headchars[] = {
  '1', '2', 'M', 'L', 'L', 'L', 'L', 'L',    /* normal */
  'n', 'n', 'n', 'n', 'n', 'n', 'n', 'n',    /* cross */
  'm', 'm', 'm', 'l', 'l', 'l', 'l', 'l',    /* harmonic */
   0,   0,   0,   0,   0,   0,   0,   0,     /* none */
  178, 178, 178, 178, 178, 178, 178, 178,    /* direct */
  201, 201, 201, 200, 200, 200, 200, 200     /* circular */
};

/* Table giving musical character identifiers for accidentals, plain,
round-bracketed, and square-bracketed. There are two versions for the two
half-accidental styles. */

static int acctable[] = {
  0, mc_natural,  mc_hsharp1,  mc_sharp,  mc_dsharp,  mc_hflat1,  mc_flat,  mc_dflat,
  0, mc_rnatural, mc_hrsharp1, mc_rsharp, mc_rdsharp, mc_hrflat1, mc_rflat, mc_rdflat,
  0, mc_snatural, mc_hssharp1, mc_ssharp, mc_sdsharp, mc_hsflat1, mc_sflat, mc_sdflat,

  0, mc_natural,  mc_hsharp2,  mc_sharp,  mc_dsharp,  mc_hflat2,  mc_flat,  mc_dflat,
  0, mc_rnatural, mc_hrsharp2, mc_rsharp, mc_rdsharp, mc_hrflat2, mc_rflat, mc_rdflat,
  0, mc_snatural, mc_hssharp2, mc_ssharp, mc_sdsharp, mc_hsflat2, mc_sflat, mc_sdflat,
};

/* Dot position adjustments for rests */

static int restdotadjusts[] = { -20, 0, 0, -25, -20, -10, -10, 0 };

/* This is the order in which accents that go outside the stave are handled.
The bowing marks come last so that they are always furthest from the note. The
list is terminated by accent_none. */

static uint8_t accentorder[] = { accent_gt, accent_wedge, accent_tp,
  accent_vline, accent_down, accent_up, accent_none };

/* Music font character strings for accents that go outside the stave, in an
order that corresponds to accentorder[] above. */

static uschar *accentabovestrings[] = { US"U", US"Y", US"W", US"\234", US"e", US"g" };
static uschar *accentbelowstrings[] = { US"U", US"Z", US"X", US"\234", US"f", US"h" };

/* Accent-specific adjustments, for accents that go outside the stave, same
order. */

static int32_t accaboveadjusts[] = { -6000, -1000, -2000, -1000, -1000, -1000 };
static int32_t accbelowadjusts[] = { -2000,  3000,     0,  2000,  1000,  1000 };

/* Further adjustments for outside-stave accents apply when certain accidentals
are present. These are indexed by accidental. */
                                     /* -   %     #-     #  ##     $-   $  $$ */
static int32_t accaccaboveadjusts[] = {0,-3000,-3000,-3000,0,-3000,-3000,-3000};
static int32_t accaccbelowadjusts[] = {0, 3000, 3000, 3000,0, 0000, 0000, 0000};

/* Adjustments for bracketed outside-stave accents. */

static int32_t accentb_adjustsx[] = { 2000,  0,  2000, 0,  2000,  2000 };
static int32_t accentb_adjustsy[] = { -4000, 0, -1500, 0, -1000, -1000 };


/* These tables are for the straightforward ornaments. Those with blank strings
are not straightforward, and have individual code. The half accidentals are
given in style 0 - there is a fudge when used to add one to the character
values. */

static const char *ornament_strings[] = {
/* none  ferm  tr  trsh trfl trnat trem1 trem2 trem3 */
    "",   "",  "",  "",  "",  "",   "",   "",   "",
/* mord  dmord  imord dimord turn iturn arp arpu arpd spread */
   "O",   "P",   "Q",  "R",   "S", "i", "",   "", "",   "",
/* nat         natrb              natsb */
   "(",        "\215(\216",       "\213(\214",
/* hsharp      hsharprb           hsharpsb    (style 0) */
   "\275",     "\215\275\216",    "\213\275\214",
/* sharp       sharprb            sharpsb */
   "%",        "\215%\216",       "\213%\214",
/* dsharp      dsharprb           dsharpsb */
   "&",        "~v\215v&~v\216",  "~v\213v&~v\214",
/* hflat       hflatrb            hflatsb   (style 0) */
   "\277",     "~\215|\277~\216", "~\213|\277~\214",
/* flat        flatrb             flatsb */
   "\'",       "~\215|\'~\216",   "~\213|\'~\214",
/* dflat       dflatrb            dflatsb */
   "\'\'",     "~\215|\'\'~\216", "~\213|\'\'~\214" };

static int ornament_xadjusts[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, -1, -1,  /* naturals */
  1, -1, -1,  /* hsharps */
  1, -1, -1,  /* sharps */
  1, -2, -2,  /* dsharps */
  2, -1, -1,  /* hflats */
  2, -1, -1,  /* flats */
  0, -3, -3   /* dflats */
};

/* Amount by which to adjust the brackets when bracketing */

static int ornament_yaadjusts[] = {
  0,
  0,         /* Fermata */
  -1000,     /* Trill */
  -4000,
  -4000,
  -4000,
  0, 0, 0,
  0, 0, -2000, -2000, -1000, -1000, 0, 0, 0, 0,
  3000, 3000, 3000,  /* naturals */
  3000, 3000, 3000,  /* hsharps */
  3000, 3000, 3000,  /* sharps */
  1000, 2000, 2000,  /* dsharps */
  1000, 1000, 1000,  /* hflats */
  1000, 1000, 1000,  /* flats */
  1000, 1000, 1000   /* dflats */
};

static int ornament_ybadjusts[] = {
  0,
  0,         /* Fermata */
  -1000,     /* Trill */
  2000,
  2000,
  2000,
  0, 0, 0,
  0, 0, 2000, 2000, 1000, 1000, 0, 0, 0, 0,
  -2000, -2000, -2000,  /* naturals */
  -2000, -2000, -2000,  /* hsharps */
  -2000, -2000, -2000,  /* sharps */
  0,     -3000, -3000,  /* dsharps */
  -3000, -4000, -4000,  /* hflats */
  -3000, -4000, -4000,  /* flats */
  -3000, -4000, -4000   /* dflats */
};

/* These tables need only go up to or_iturn, as accidentals are handled
specially and already have a bracketting facility, and arpeggios and spread
chords can't be bracketed. */

static int ornament_xbrackadjustsL[] = {
  0,
  4000,    /* Fermata */
  3000,    /* Trill */
  3000,    /* Trill with sharp */
  3000,    /* Trill with flat */
  3000,    /* Trill with natural */
  0, 0, 0, /* Tremolos - never bracketed */
  2500,    /* Mordent */
  2500,    /* Double mordent */
  2500,    /* Inverted mordent */
  2500,    /* Double inverted mordent */
  2500,    /* Turn */
  2500     /* Inverted turn */
};

static int ornament_xbrackadjustsR[] = {
  0,
  4000,    /* Fermata */
  3000,    /* Trill */
  3000,    /* Trill with sharp */
  3000,    /* Trill with flat */
  3000,    /* Trill with natural */
  0, 0, 0, /* Tremolos - never bracketed */
  2600,    /* Mordent */
  5500,    /* Double mordent */
  2600,    /* Inverted mordent */
  5500,    /* Double inverted mordent */
  3500,    /* Turn */
  3500     /* Inverted turn */
};



/*************************************************
*              Static variables                  *
*************************************************/

/* For saving beaming state while setting grace notes. */

static int     save_beam_count;
static int32_t save_beam_firstX;
static int32_t save_beam_firstY;
static int     save_beam_seq;
static int32_t save_beam_slope;
static BOOL    save_beam_splitOK;
static BOOL    save_beam_upflag;
static int32_t save_beam_Xcorrection;
static int32_t save_orig_stemlength;
static BOOL    save_out_beaming;



/*************************************************
*   Print a possibly bracketed accent/ornament   *
*************************************************/

/* The bracket characters have width, but accent and ornament characters do
not.

Arguments:
  str       the string for the accent/ornament
  fontsize  the font size
  x         x-position for accent/ornament
  y         y-position for accent/ornament
  flags     bracket flags
  yadjust   general y adjustment if bracketed
  byadjust  specific y adjustment for brackets
  bxadjustL additional x left adjustment for brackets
  bxadjustR additional x right adjustment for brackets

Returns:    nothing
*/

static void
show_accentornament(uschar *str, int32_t fontsize, int32_t x, int y,
  uint8_t flags, int32_t yadjust, int32_t byadjust, int32_t bxadjustL,
  int32_t bxadjustR)
{
int32_t yb;

if ((flags & (orn_rbra|orn_rket|orn_sbra|orn_sket)) != 0) y += yadjust;
yb = y + byadjust;

if ((flags & orn_rbra) != 0)
  ps_musstring(US"\215", fontsize, x - (35*out_stavemagn)/100 - bxadjustL, yb);
else if ((flags & orn_sbra) != 0)
  ps_musstring(US"\213", fontsize, x - (35*out_stavemagn)/100 - bxadjustL, yb);

ps_musstring(str, fontsize, x, y);

/* We have to convert the string to 32-bit in order to get its width. */

if ((flags & (orn_rket|orn_sket)) != 0)
  {
  int32_t swidth;
  fontinststr fdata;
  fdata.size = fontsize;
  fdata.matrix = NULL;
  fdata.spacestretch = 0;
  swidth = string_width(string_pmw(str, font_mf), &fdata, NULL);
  if (swidth == 0) swidth = (58*fontsize)/100;
  ps_musstring(((flags & orn_rket) != 0)? US"\216" : US"\214",
    fontsize, x + mac_muldiv(swidth, out_stavemagn, 1000) + bxadjustR, yb);
  }
}



/*************************************************
*         Actually print one note                *
*************************************************/

/* The data about the note is in the n_* global variables. This function just
prints the note head and stems. Accidentals, dots, accents, etc are done
elsewhere.

Argument:   the x coordinate for the note
Returns:    TRUE if one or more ledger lines were used
*/

static BOOL
show_note(int x)
{
uschar buff[100];
uschar *p;

int32_t fontsize = (n_fontsize*out_stavemagn)/1000;
int32_t y = out_ystave - (n_pitch - P_1S)*out_pitchmagn - n_pcorrection;

int top = P_6L;
int bot = P_0L;

BOOL ledgered = FALSE;
BOOL positioned = FALSE;
BOOL inverted = (n_flags & nf_invert) != 0;
BOOL stemcent = (n_flags & nf_stemcent) != 0;

TRACE("show_note() start\n");

/* Set up for coupled notes */

if ((n_flags & nf_coupleU) != 0)
  {
  top += out_upgap;
  bot += out_upgap;
  }
else if ((n_flags & nf_coupleD) != 0)
  {
  top -= out_downgap;
  bot -= out_downgap;
  }

/* First deal with ledger lines if required for 5- or 6-line staves. We can
optimize into a single music-font string if the size is standard. */

if (out_stavelines >= 5 && n_noteheadstyle != nh_none)
  {
  int ledgergap;
  int toporbot;
  int movechar;
  int ledgerchar = (curmovt->ledgerstyle == 0)? '=' : 184;

  if (n_pitch <= bot)
    {
    ledgergap = +8;
    toporbot = bot;
    movechar = 'w';
    }
  else if (n_pitch >= top)
    {
    ledgergap = -8;
    toporbot = top;
    movechar = 'x';
    }
  else ledgergap = 0;

  if (ledgergap != 0)
    {
    int32_t breve_right = 0;
    int32_t xx = x;
    int32_t yy;

    if (n_notetype == breve)
      {
      xx -= ((((curmovt->breveledgerextra - 2300)*out_stavemagn)/1000) *
        n_fontsize)/10000;
      breve_right = mac_muldiv(2*curmovt->breveledgerextra, fontsize, 10000);
      }

    ledgered = positioned = TRUE;
    yy = out_ystave - (toporbot - P_1S)*out_pitchmagn - n_pcorrection;

    /* Use a single music string for a standard-sized note, not inverted. */

    if (n_fontsize == 10000 && !inverted)
      {
      int32_t yyy = yy;
      p = buff;
      while ((ledgergap > 0 && yyy <= y) ||
             (ledgergap < 0 && yyy >= y))
        {
        *p++ = ledgerchar;
        *p++ = movechar;
        yyy += ledgergap * out_pitchmagn;
        }

      *(--p) = 0;  /* removes redundant last move */
      ps_musstring(buff, fontsize, xx, yy);
      if (n_notetype == breve)
        ps_musstring(buff, fontsize, xx + breve_right, yy);
      }

    /* Otherwise we have to position each ledger separately. */

    else
      {
      int yyy = yy;
      p = buff;
      *p++ = ledgerchar;

      if (inverted)
        {
        p += sprintf(CS p, n_upflag? "}" : "yy{");
        *p++ = ledgerchar;
        }
      *p = 0;

      while ((ledgergap > 0 && yy <= y) ||
             (ledgergap < 0 && yy >= y))
        {
        ps_musstring((yy == y && !n_upflag && inverted)? buff+1 : buff,
          fontsize, xx, yy);
        yy += ledgergap * out_pitchmagn;
        }

      /* For a breve, extensions are needed. */

      if (n_notetype == breve)
        {
        yy = yyy;
        xx += breve_right;
        while ((ledgergap > 0 && yy <= y) ||
               (ledgergap < 0 && yy >= y))
          {
          ps_musstring((yy == y && !n_upflag && inverted)? buff+1 : buff,
            fontsize, xx, yy);
          yy += ledgergap * out_pitchmagn;
          }
        }
      }
    }
  }

/* Ledger lines are done. Now output the note itself. Optimize the common case
where there is a complete character available in the music font. */

p = buff;  /* Pointer for generating music font string. */

if (n_notetype < dsquaver && n_stemlength == 0 &&
    n_noteheadstyle == nh_normal && !n_smallhead && !stemcent &&
    (n_flags & (nf_invert|nf_stem)) == nf_stem)
  {
  if ((n_flags & nf_appogg) != 0) *p++ = n_upflag? 129 : 130;
  *p++ = common_notes[n_notetype + n_upflag*6];
  *p = 0;
  ps_musstring(buff, fontsize, x, y);
  return ledgered;  /* TRUE if there were ledger lines */
  }

/* Deal with rarer cases, where the notes have to be constructed from various
bits. We impose a minimum stemlength at this point. */

if (n_stemlength < MIN_STEMLENGTH_ADJUST) n_stemlength = MIN_STEMLENGTH_ADJUST;

/* Generate a stem and tails if required. */

if ((n_flags & nf_stem) != 0)
  {
  int32_t font10 = fontsize;   /* 10pt at font scale */
  int32_t font2  = font10/5;   /* 2pt at font scale */
  int32_t font1  = font2/2;    /* 1pt at font scale */
  int32_t yy;

  positioned = TRUE;
  if ((n_flags & nf_appogg) != 0) *p++ = n_upflag? 129 : 130;

  /* The variable yy is equal to the note's y coordinate adjusted for any
  change in stemlength. A string of music characters is constructed to be
  output at this y coordinate, starting with any tails, followed by stem and
  up/down movements. Afterwards, there may be a final stem character to be
  output at the base y coordinate. */

  yy = y + ((n_upflag? -1 : +1) * n_stemlength * out_stavemagn)/1000;
  p += sprintf(CS p, "%s",
    tailstrings[n_notetype + (n_upflag? NOTETYPE_COUNT : 0)]);


  /* ===================================================================== */
  /* Handle experimental centred stems separately so as not to pollute the
  normal stem handling. */

  if (stemcent)
    {
    int32_t centx = x + (n_upflag? -1:+1) * mac_muldiv(
      ((n_noteheadstyle == nh_circular)? -900 : 0) + STEMCENTADJUST,
      out_stavemagn, 1000);

    if (n_upflag)
      {
      if (yy <= y)    /* Stem is standard or lengthened */
        {
        int32_t z = yy;
        while (z <= y)
          {
          p += sprintf(CS p, "Jww|");
          z += font10;
          }
        p -= 3;
        *p = 0;
        ps_musstring(buff, fontsize, centx, yy);
        p = buff;
        if (z < y + font10)
          {
          if (n_noteheadstyle == nh_harmonic) p += sprintf(CS p, "~q");
          *p++ = 'J';
          }
        }

      else            /* Stem is shortened */
        {
        int32_t z = yy - font10 - font2;
        p += sprintf(CS p, "xxx");
        if (n_noteheadstyle == nh_harmonic) z += font2;
        while (z <= y)
          {
          p += sprintf(CS p, "q|");
          z += font2;
          }
        *(--p) = 0;
        ps_musstring(buff, fontsize, centx, yy);
        p = buff;
        }
      }

    else  /* Stem down */
      {
      if (yy >= y)    /* Stem is standard or lengthened */
        {
        int32_t z = yy;
        while (z >= y)
          {
          p += sprintf(CS p, "Kxx~");
          z -= font10;
          }
        p -= 3;
        *p = 0;
        ps_musstring(buff, fontsize, centx, yy);
        p = buff;
        if (z > y - font10)
          {
          if (n_noteheadstyle == nh_harmonic) p += sprintf(CS p, "|r");
          *p++ = 'K';
          }
        }

      else            /* Stem is shortened */
        {
        int32_t z = yy + font10 + font2;
        if (n_noteheadstyle == nh_harmonic) z -= font2;
        p += sprintf(CS p, "www");
        while (z >= y)
          {
          p += sprintf(CS p, "r~v");
          z -= font1;
          }
        *(--p) = 0;
        ps_musstring(buff, fontsize, centx, yy);
        p = buff;
        }
      }

    /* Output any additional bits at the centralized x coordinate and the base
    y coordinate. */

    if (p != buff)
      {
      *p = 0;
      ps_musstring(buff, fontsize, centx, y);
      p = buff;
      }
    }

  /* End of handling centred stems -- deal with traditional stems. */
  /* ===================================================================== */


  /* Notes with stems up */

  else if (n_upflag)
    {
    if (yy <= y)    /* Stem is standard or lengthened */
      {
      int stemch = (n_noteheadstyle == nh_cross)? 'o' : 'J';
      int32_t z = yy;
      while (z <= y)
        {
        p += sprintf(CS p, "%cww|", stemch);
        z += font10;
        }
      p -= 3;
      *p = 0;
      ps_musstring(buff, fontsize, x, yy);
      p = buff;
      if (z < y + font10) *p++ = stemch;
      if (n_noteheadstyle == nh_harmonic) *p++ = 'q';
      }

    else            /* stem is shortened */
      {
      int32_t z = yy - font10 - font2;
      p += sprintf(CS p, "xxx");
      while (z <= y)
        {
        p += sprintf(CS p, "q|");
        z += font2;
        }
      *(--p) = 0;
      ps_musstring(buff, fontsize, x, yy);
      p = buff;
      if (z > y) *p++ = 'q';
      }
    }

  /* Notes with stems down */

  else
    {
    if (yy >= y)    /* Stem is standard or lengthened */
      {
      int stemch = (n_noteheadstyle == nh_cross)? 'p' : 'K';
      int32_t z = yy;
      while (z >= y)
        {
        p += sprintf(CS p, "%cxx~", stemch);
        z -= font10;
        }
      p -= 3;
      *p = 0;
      ps_musstring(buff, fontsize, x, yy);
      p = buff;
      if (z > y - font10) *p++ = stemch;
      if (n_noteheadstyle == nh_harmonic) *p++ = 'r';
      }

    else            /* stem is shortened */
      {
      int32_t z = yy + font10 + font2;
      p += sprintf(CS p, "www");
      while (z >= y)
        {
        p += sprintf(CS p, "r~v");
        z -= font1;
        }
      *(--p) = 0;
      ps_musstring(buff, fontsize, x, yy);
      p = buff;
      if (z < y) *p++ = 'r';
      }
    }
  }   /* End of handling the stem and tails */

/* Now add the note head */

if (n_noteheadstyle != nh_none)
  {
  if (inverted)
    {
    if (n_upflag)
      {
      if (n_notetype == breve)
        p += sprintf(CS p, "}}}}{{{{z");
      else *p++ = 125;
      }
    else
      {
      if (n_notetype == breve)
        p += sprintf(CS p, "{yyyyyyyyyyyy}");
      else
        {
        *p++ = 123;
        *p++ = 121;
        *p++ = 121;
        }
      }
    }

  /* The special cases of a small note head or a circular note head are dealt
  with below; just omit the note head at this point. */

  if (!n_smallhead && n_noteheadstyle != nh_circular)
    {
    *p++ = headchars[n_notetype + NOTETYPE_COUNT*n_noteheadstyle];

    /* When printing right-to-left, we put some redundant spacing *after*
    inverted noteheads. This is just a fudge to fool the x-coordinate adjusting
    code into doing (approximately) the right thing. */

    if (main_righttoleft && inverted)
      p += sprintf(CS p, n_upflag? "{{{" : "zzzz");
    }
  }

/* Output the music font string. */

*p = 0;
ps_musstring(buff, fontsize, x, y);

/* In the special cases of a small or circular note head, the note head was
skipped above. The printing position should be in the correct place for a full
size note head if a stem or ledger lines were output above. Arrange to output
the notehead at the cue size, with a relative position adjusted to allow for
the head size. */

if (n_smallhead)
  {
  int32_t cue_fontsize = (curmovt->fontsizes)->fontsize_cue.size;
  int32_t sm_fontsize = (cue_fontsize * out_stavemagn)/1000;

  p = buff;
  *p++ = headchars[n_notetype + NOTETYPE_COUNT*n_noteheadstyle];
  *p = 0;

  if (positioned && (n_flags & nf_stem) != 0)
    {
    x = 0;
    if ((n_upflag && (n_flags & nf_invert) == 0) ||
        (!n_upflag && (n_flags & nf_invert) != 0))
      x += 6000 - 3 * (cue_fontsize / 5);
    y = 2000 - cue_fontsize / 5;
    ps_relmusstring(buff, sm_fontsize, x, y);
    }

  else
    {
    x += (((n_notetype == breve)? 19:11)*out_stavemagn)/10;
    y -= (5*out_stavemagn)/10;
    ps_musstring(buff, sm_fontsize, x, y);
    }
  }

else if (n_noteheadstyle == nh_circular)
  {
  p = buff;
  *p++ = headchars[n_notetype + NOTETYPE_COUNT*n_noteheadstyle];
  *p = 0;

  x += mac_muldiv((n_upflag? 300 : -1200), out_stavemagn, 1000);

  ps_musstring(buff, fontsize, x, y);





  }


/* Return TRUE if there were ledger lines. */

TRACE("show_note() end\n");
return ledgered;
}



/*************************************************
*         Actually print one rest                *
*************************************************/

/* This function just prints the actual rest (possibly with ledger lines). Dots
are handled elsewhere.

Arguments:
  x           x coordinate for the rest
  notetype    note type or -1 for a multi-bar rest sign

Returns:      nothing
*/

static void
show_rest(int32_t x, int notetype)
{
int32_t fontsize = (n_fontsize*out_stavemagn)/1000;
int32_t yoffset = n_restlevel;
int32_t y;

/* Rests longer than a crotchet have to have ledger lines when they are printed
off the stave. Also move a semibreve rest down on a 1-line stave and up on a
3-line stave. We must also adjust the position of breve and semibreve rests for
cue sizes. */

if (notetype <= minim)
  {
  int32_t loffset = 0;  /* Ledger offset */
  yoffset += 8000;

  switch (notetype)
    {
    case -1:     /* long rest sign */
    yoffset -= 2000;
    break;

    case breve:
    yoffset += n_pcorrection;
    /* Fall through */

    case minim:
    if (yoffset > 16000 || yoffset < 0) loffset = -2000;
    break;

    case semibreve:
    if (out_stavelines == 1) yoffset -= 4000;
      else if (out_stavelines == 3) yoffset += 4000;
    yoffset += 2*n_pcorrection;
    if (yoffset > 12000 || yoffset < -4000) loffset = 2000;
    break;
    }

  if (loffset != 0)
    ps_musstring(US"=", fontsize, x - (10*out_stavemagn)/10, out_ystave -
      ((yoffset + loffset)*out_stavemagn)/1000);
  }

else yoffset += 4000 + n_pcorrection;

y = out_ystave - (yoffset*out_stavemagn)/1000;

/* Output an individual rest, taking care of the special cases that are used
for conventional repeat signs. */

if (notetype >= 0)
  {
  uschar *s;
  if ((n_flags & nf_restrep) == 0) s = reststrings[notetype];
  else if (notetype == crotchet) s = US"\217";
  else
    {
    s = US"\220\217";
    y += 4*out_stavemagn;
    }
  ps_musstring(s, fontsize, x, y);
  }

/* Output a conventional multibar rest sign if the option is set and the number
of bars is suitable.  */

else if (MFLAG(mf_codemultirests) && out_manyrest < 9)
  {
  ps_musstring(multireststrings[out_manyrest - 2], fontsize, x, y - 2000);
  }

/* Output a |----| long rest sign. If the bar is unusually long or unusually
short, draw the long rest symbol; note that ps_line() works in conventional
coordinates relative to the base line of the stave. The variable n_longrestmid
contains the mid-point of the long rest. */

else
  {
  BOOL wide = (out_barlinex - n_longrestmid) > 40*out_stavemagn;

  if (wide || x - out_barx < 6000)
    {
    posstr *p;
    int32_t xl, xr;
    int32_t vthick = (3*out_stavemagn)/10;
    int32_t hthick = 3*out_stavemagn;
    int32_t adjust = 0;
    int32_t min = wide? 15000 : 12000;

    /* Calculate an adjustment for anything at the end of the bar, e.g. a clef
    change. */

    for (p = out_posptr + 1; p->moff <= 0; p++); /* Skip preceding */
    for (; p < out_poslast; p++) adjust += p[1].xoff - p->xoff;

    /* Place the right hand end with respect to the the barline, nearer for a
    narrow bar, then place the left hand end symmetrically. */

    if (adjust < min) adjust = min + adjust/3; else adjust += 5000;

    xr = out_barlinex - mac_muldiv(adjust, out_stavemagn, 1000);
    xl = 2*n_longrestmid - xr;

    ps_line(xl, 8*out_stavemagn, xr, 8*out_stavemagn, hthick, 0);
    ps_line(xl, 4*out_stavemagn, xl, 12*out_stavemagn, vthick, 0);
    ps_line(xr, 4*out_stavemagn, xr, 12*out_stavemagn, vthick, 0);
    }

  /* Use the long rest character */

  else ps_muschar(x, y, mc_longrest, fontsize);
  }
}



/*************************************************
*    Generate one note/rest + dots & accents     *
*************************************************/

/* The relevant data about the note/rest is all in the n_* global variables.

Arguments:  nothing
Returns:    nothing
*/

static void
shownote(void)
{
int32_t acc_level;
int32_t xn = n_x + n_cueadjust;
int32_t yyabove, yybelow;
int32_t fontsize = (n_fontsize*out_stavemagn)/1000;  /* Scaled font size */
BOOL ledgered = FALSE;
BOOL acc_upflag;

TRACE("out_shownote() start\n");

/* If the note is invisible, we skip printing and just show accents, etc.
below. This test is for notes on and this note not hidden. */

if ((bar_cont->flags & cf_notes) != 0 && (n_flags & nf_hidden) == 0)
  {
  int32_t x_acc = 0;
  int32_t x_dot = 0;

  /* If printing a breve, move left to align with semibreve position */

  if (n_pitch != 0 && n_notetype == breve) xn -= (23*out_stavemagn)/10;

  /* First, any accidental is set, prior to the given position */

  if (n_acc != ac_no && (n_flags & nf_accinvis) == 0)
    {
    int offset = n_acc;
    if ((n_flags & nf_accrbra) != 0) offset += 8;
      else if ((n_flags & nf_accsbra) != 0) offset += 16;
    if (n_acc == ac_hs && curmovt->halfsharpstyle != 0) offset += 24;
    if (n_acc == ac_hf && curmovt->halfflatstyle != 0) offset += 24;
    x_acc = xn - mac_muldiv(n_accleft, n_fontsize, 10000);
    ps_muschar(x_acc,
      out_ystave - (n_pitch - P_1S)*out_pitchmagn - n_pcorrection,
        acctable[offset], fontsize);
     }

  /* Now we can output the note or rest. */

  if (n_pitch == 0)
    {
    int notetype;
    if (out_manyrest == 1) notetype = n_notetype; else
      {
      notetype = -1;
      n_flags &= ~(nf_dot+nf_plus);   /* Kill dots for many bar rest */
      }
    show_rest(xn, notetype);
    }

  else ledgered = show_note(xn);

  /* Deal with horizontal dots/plus - fudge for quavers and breves */

  if ((n_flags & (nf_dot + nf_plus)) != 0)
    {
    BOOL dotplus = (n_flags & nf_plus) != 0;
    int32_t dotpos = 84;
    int32_t dotlevel;

    if (n_pitch == 0)
      {
      dotlevel = mac_muldiv(L_3S + n_restlevel, out_stavemagn, 1000);
      dotpos += restdotadjusts[n_notetype];
      }

    else
      {
      int dotpitch = n_pitch | 4;  /* force into space above */

      if ((n_flags & nf_lowdot) != 0 && (n_pitch & 4) == 0) dotpitch -= 8;
      if ((n_flags & nf_highdot) != 0 && (n_pitch & 4) != 0) dotpitch += 8;

      dotlevel = (dotpitch - P_1S)*out_pitchmagn;
      if (n_notetype == breve) dotpos += 50;
      }

    if ((n_flags & nf_dotright) != 0) dotpos += 60;
      else if (n_upflag && n_notetype >= quaver && n_pitch != 0) dotpos += 16;

    dotpos = (dotpos*out_stavemagn)/10 + n_dotxadjust;

    /* For cue notes there are two choices: either to scale the position
    according to the cue size, or to align the dots with full-sized notes that
    may be above or below (alignment by centre of dot). */

    if ((n_flags & nf_cuesize) != 0)
      {
      if ((n_flags & nf_cuedotalign) != 0)
        dotpos += mac_muldiv(640 - mac_muldiv(640, n_fontsize, 10000),
          out_stavemagn, 1000) - n_cueadjust;
      else
        dotpos = mac_muldiv(dotpos, n_fontsize, 10000);
      }

    /* Output the dot(s). The '+' character is 135 (0x87) in the music font. */

    x_dot = xn + dotpos;
    ps_musstring(dotplus? US"\x87" : US"?", fontsize, x_dot,
      out_ystave - dotlevel - n_pcorrection);

    if ((n_flags & nf_dot2) != 0)
      {
      x_dot += (35*out_stavemagn)/10;
      ps_musstring(US"?", fontsize, x_dot, out_ystave - dotlevel - n_pcorrection);
      }
    if (dotplus) x_dot += 4*out_stavemagn;  /* Extra for ) */
    }

  /* Deal with bracketed notehead */

  if ((n_flags & nf_headbra) != 0)
    {
    int32_t adjustbra = 0;
    int32_t adjustket = 0;
    int32_t bfontsize = (6*fontsize)/10;
    int32_t yb = out_ystave - (n_pitch - P_1S)*out_pitchmagn -
      (9*out_stavemagn/10);

    uschar *bra = main_righttoleft? US"\216" : US"\215";
    uschar *ket = main_righttoleft? US"\215" : US"\216";

    if (x_acc == 0) x_acc = xn + out_stavemagn;
    if (x_dot == 0) x_dot = xn + 5*out_stavemagn;

    if (n_notetype == breve) adjustket += 5*out_stavemagn;

    if (n_smallhead)
      {
      if ((n_flags & nf_stem) != 0)
        {
        if (n_upflag) adjustbra += 2*out_stavemagn;
          else adjustket -= 2*out_stavemagn;
        }
      else
        {
        adjustbra += 2*out_stavemagn;
        adjustket -= 2*out_stavemagn;
        }

      if (ledgered)
        {
        adjustbra -= (15*out_stavemagn)/10;
        adjustket += out_stavemagn;
        }
      }

    ps_musstring(bra, bfontsize, x_acc - (35*out_stavemagn)/10 + adjustbra, yb);
    ps_musstring(ket, bfontsize, x_dot + (40*out_stavemagn)/10 + adjustket, yb);
    }
  }

/* If there are no dynamics and no ornaments, there's nothing more to do */

if ((n_acflags & af_accents) == 0 && n_ornament == NULL) return;

/* Now set up a level and up flag for expression marks - normally these are
from the standard note values, but they are different if the accents are
flagged for printing on the same side of the note as the stem. For chords it is
arranged that the accents come with the appropriate note head. */

acc_level = (n_pitch - P_1S)*out_pitchmagn;

if ((n_acflags & af_opposite) == 0)
  {
  acc_upflag = n_upflag;
  }
else
  {
  acc_upflag = !n_upflag;
  if ((n_flags & nf_stem) != 0)
    acc_level +=
      n_upfactor * mac_muldiv((12000 + n_stemlength), out_stavemagn, 1000);
  }

/* Staccato, staccatissimo, ring, & bar go inside the staff. Except for
staccato and staccatissimo, they are allowed together - the staccat{issim}o is
nearest to the note. We don't need to compensate for ties as the ties
themselves are moved in this case. */

if ((n_acflags & af_accinside) != 0)
  {
  int32_t adjust = 4*out_stavemagn;
  int32_t p = acc_level;

  if (acc_upflag)
    {
    adjust = -adjust;
    p -= 8*out_stavemagn;

    /* Accent at notehead; ensure not on line; not for 0 or 1-line staves or
    for staccatissimo. */

    if (out_stavelines >= 2 && (n_acflags & af_staccatiss) == 0)
      {
      if (acc_upflag == n_upflag)
        {
        if (((n_pitch & 4) == 0) && n_pitch != P_1L &&
          (!ledgered || n_pitch >
            (P_5L - ((n_flags & nf_coupleD)? out_downgap : 0))))
              p -= 2*out_stavemagn;
        }

      /* Accent at stem end; ensure not on line; rather assumes stemlength
      adjustments will be in whole points... */

      else
        {
        int32_t pp = p/out_stavemagn;
        if (pp >= (-6) && pp <= 10 && (pp & 2) != 0)
          p -= 2*out_stavemagn;
        }
      }
    }

  else  /* !acc_upflag */
    {
    /* Accent at notehead; ensure not on line; not for 0 or 1-line staves or
    for staccatissimo. */

    if (out_stavelines >= 2 && (n_acflags & af_staccatiss) == 0)
      {
      if (acc_upflag == n_upflag)
        {
        if (((n_pitch & 4) == 0) && n_pitch != P_5L &&
          (!ledgered || n_pitch <
            (P_1L + ((n_flags & nf_coupleU)? out_upgap : 0))))
              p += 2*out_stavemagn;
        }

      /* Accent at stem end; ensure not on line; rather assumes stemlength
      adjustments will be in whole points... */

      else
        {
        int32_t pp = p/out_stavemagn;
        if (pp >= (-6) && pp <= 10 && (pp & 2) != 0)
          p += 2*out_stavemagn;
        }
      }
    }

  if (out_beaming && (acc_upflag != n_upflag)) p += n_upfactor * 1000;

  if ((n_acflags & af_staccato) != 0)
    {
    b_accentmovestr *am = out_accentmove[accent_staccato];
    show_accentornament(US">", fontsize, xn + am->x,
      out_ystave - p - mac_muldiv(am->y, out_stavemagn, 1000), am->bflags,
      acc_upflag? 4*out_stavemagn:(-4)*out_stavemagn, -4*out_stavemagn, 0, 0);
    p += adjust;
    out_accentmove[accent_staccato] = &no_accent_move;
    }

  /* Leave p the same value as for staccato, with a small adjustment, but we
  need some further adjustment to cope with the position of the characters in
  the font. */

  else if ((n_acflags & af_staccatiss) != 0)
    {
    b_accentmovestr *am = out_accentmove[accent_staccatiss];
    int32_t pp = (acc_upflag? 25:55)*out_stavemagn/10;
    if (acc_upflag != n_upflag) pp += (acc_upflag? 1:-1)*out_stavemagn;

    show_accentornament(acc_upflag? US"\303":US"\302", fontsize,
      xn + am->x, out_ystave - p - pp - mac_muldiv(am->y, out_stavemagn, 1000),
      am->bflags, acc_upflag? 3*out_stavemagn:(-3)*out_stavemagn, 0, 0, 0);
    p += adjust + (acc_upflag? -1:1)*out_stavemagn;
    out_accentmove[accent_staccatiss] = &no_accent_move;
    }

  /* The ring character prints 4 points lower than the other two, and a bit
  further away from the notehead when clear of stave or ledger lines. */

  if ((n_acflags & af_ring) != 0)
    {
    b_accentmovestr *am = out_accentmove[accent_ring];
    int32_t yy = 0;
    if ((n_flags & nf_couple) == 0)
      {
      if (acc_upflag)
        {
        if (n_upflag && n_pitch <= P_1S) yy = out_stavemagn;
        }
      else
        {
        if (!n_upflag && n_pitch >= P_4S) yy = -out_stavemagn;
        }
      }
    show_accentornament(US"\206", fontsize, xn + am->x,
      out_ystave - p - 4*out_stavemagn + yy -
      mac_muldiv(am->y, out_stavemagn, 1000),
      am->bflags, acc_upflag? 4*out_stavemagn:(-4)*out_stavemagn, 0, 0, 0);
    p += adjust + yy;
    out_accentmove[accent_ring] = &no_accent_move;
    }

  if ((n_acflags & af_bar) != 0)
    {
    b_accentmovestr *am = out_accentmove[accent_bar];
    show_accentornament(US"T", fontsize, xn + am->x,
      out_ystave - p - mac_muldiv(am->y, out_stavemagn, 1000), am->bflags,
      acc_upflag? 4*out_stavemagn:-4*out_stavemagn, -4*out_stavemagn,
      2*out_stavemagn, 2*out_stavemagn);
    out_accentmove[accent_bar] = &no_accent_move;
    }
  }

/* Set up y values for things that print above or below the stave; there is a
different set for the accents and the ornaments, but we compute the basic
values here for both of them. Set the stemlength to the value for the first
note of a chord, which will be the bit that sticks out. */

n_stemlength = save_orig_stemlength;

yybelow = misc_ybound(TRUE, n_nexttie, TRUE, FALSE);
yyabove = misc_ybound(FALSE, n_nexttie, TRUE, FALSE);

/* We can common up the code for accents and bowing marks that go outside the
stave into a loop. The order is controlled by a list in which the bowing marks
come last (so they are furthest from the note). */

if ((n_acflags & af_accoutside) != 0)
  {
  int32_t yextra = 0;

  for (int i = 0; accentorder[i] != accent_none; i++)
    {
    BOOL upflag = acc_upflag;
    uschar *s;
    int32_t y;
    int32_t xadjust;
    int32_t ayybelow;
    int32_t ayyabove;
    int accentnumber = accentorder[i];
    b_accentmovestr *am;

    /* Check for the accent's bit being set. */

    if ((n_acflags & (0x80000000u >> accentnumber)) == 0) continue;

    /* The > accent gets a bit of x adjustment. */

    xadjust = (accentnumber == accent_gt)? out_stavemagn : 0;

    /* For bowing marks (which come last), above or below is controlled by an
    independent setting. Paradoxically, a TRUE upflag implies accents below. If
    this is different to other accents, reset any y adjustment that might have
    happened for a previous accent. */

    if (accentnumber == accent_down || accentnumber == accent_up)
      {
      BOOL newupflag = (bar_cont->flags & cf_bowingabove) == 0;
      if (newupflag != upflag) yextra = 0;
      upflag = newupflag;
      }

    /* Further adjustments when accents are not on the stem side and there are
    no "inside" accents. Effectively we cancel some of the space put in for
    accidental signs. */

    ayybelow = yybelow;
    ayyabove = yyabove;

    if (upflag == n_upflag && (n_acflags & af_accinside) == 0)
      {
      ayybelow += accaccbelowadjusts[n_acc];
      ayyabove += accaccaboveadjusts[n_acc];
      }

    /* Set the accent string and the y position, with accent-specific
    adjustment, ensuring that it is clear of the stave. */

    if (upflag)
      {
      ayybelow = ((ayybelow <= out_stavebottom)? ayybelow : out_stavebottom)
        - 8000;
      y = ayybelow + accbelowadjusts[i] - yextra;
      s = accentbelowstrings[i];
      }
    else
      {
      ayyabove = ((ayyabove > out_stavetop)? ayyabove : out_stavetop) + 3000;
      y = ayyabove + accaboveadjusts[i] + yextra;
      s = accentabovestrings[i];
      }

    /* Now output the accent */

    am = out_accentmove[accentnumber];
    show_accentornament(s, fontsize, n_x + xadjust + am->x,
      out_ystave - mac_muldiv(y + am->y, out_stavemagn, 1000),
      am->bflags,
      0,
      mac_muldiv(accentb_adjustsy[i], out_stavemagn, 1000),
      mac_muldiv(accentb_adjustsx[i], out_stavemagn, 1000),
      mac_muldiv(accentb_adjustsx[i], out_stavemagn, 1000));

    /* Put any subsequent one further out; clear movement for this one. */

    yextra += 4000;
    out_accentmove[accentnumber] = &no_accent_move;
    }
  }

/* Deal with ornaments. It is rare to have more than one, so don't bother about
the recomputation that then happens. */

if (n_ornament == NULL) return;  /* There are no ornaments */

yybelow = ((yybelow <= out_stavebottom)? yybelow : out_stavebottom) - 8000;
yyabove = ((yyabove > out_stavetop)? yyabove : out_stavetop) + 3000;

for (; n_ornament->type == b_ornament;
     n_ornament = (b_ornamentstr *)(n_ornament->next))
  {
  uschar s[100];
  uschar *p = s;
  BOOL below;
  int ornament = n_ornament->ornament;
  int32_t size = fontsize;
  int32_t x = n_ornament->x + n_x;
  int32_t y = n_ornament->y;

  /* Above/below accidentals are special, and may be disabled when associated
  with copied notes or pitches in the same bar. */

  if (ornament >= or_nat)
    {
    if ((n_ornament->bflags & orn_invis) != 0) continue;
    size = (curmovt->fontsizes->fontsize_vertacc.size * out_stavemagn)/1000;
    if (ornament >= or_accbelow)
      {
      below = TRUE;
      y += yybelow + 8000 - (8*size)/10;
      ornament -= or_accbelow - or_nat;
      }
    else
      {
      below = FALSE;
      y += yyabove;
      }
    }

  /* Adjust y for other ornaments */

  else
    {
    below = (n_acflags & af_opposite) != 0;
    y += below? yybelow : yyabove;
    }

  /* Particular code for each ornament */

  switch (ornament)
    {
    case or_trsh:
    *p++ = '%';
    goto TR;

    case or_trfl:
    *p++ = '\'';
    goto TR;

    case or_trnat:
    *p++ = '(';
    /* Fall through */

    case or_tr:
    TR:
      {
      fontinststr *fdata = &curmovt->fontsizes->fontsize_trill;
      int32_t asize = (6 * fdata->size)/10;
      int32_t tsize = fdata->size;

      out_string(curmovt->trillstring, fdata, x,
        out_ystave - mac_muldiv(y, out_stavemagn, 1000), 0);

      /* For a bracketed trill, use the show_accentornament() function with
      an empty string. */

      if (n_ornament->bflags != 0)
        show_accentornament(US"", tsize, x,
          out_ystave - mac_muldiv(y, out_stavemagn, 1000),
          n_ornament->bflags,
          0,
          below? ornament_ybadjusts[ornament] :
                 ornament_yaadjusts[ornament],
          ornament_xbrackadjustsL[ornament],
          ornament_xbrackadjustsR[ornament]);

      size = asize;
      if (below) y -= asize;
        else if (n_ornament->ornament == or_trfl) y += (8*tsize)/10;
          else y += tsize;
      x += 2*out_stavemagn;
      }
    break;

    case or_ferm:
    if (n_pitch == 0) x -= out_stavemagn;
    *p++ = below? '/' : ')';
    break;

    case or_arp:
    case or_arpu:
    case or_arpd:
      {
      int h = (n_maxpitch - n_minpitch + 4)/8;
      if ((n_minpitch & 4) != 0 && (n_maxpitch & 4) != 0) h++;

      if (ornament == or_arpd)
        {
        *p++ = 165;
        h--;
        }
      do *p++ = 145; while (--h >= 0);
      if (ornament == or_arpu) p[-1] = 164;

      y = (n_minpitch - 260)/2;
      if ((y & 2) == 0) y -= 2;
        else if ((n_maxpitch & 4) != 0) y--;
      y = y*1000 + n_ornament->y;

      if (n_maxaccleft != 0) x -= n_maxaccleft;
        else if (n_invertleft) x -= (55*out_stavemagn)/10;
      }
    break;

    case or_spread:
      {
      int32_t co;
      int32_t y0 = (n_minpitch - 256)/2;
      int32_t y1 = (n_maxpitch - 256)/2;

      if (0 <= y0 && y0 <= 16 && (y0 & 3) == 0) y0--;
      if (0 <= y1 && y1 <= 16 && (y1 & 3) == 0) y1++;

      co = (4000 * (y1 - y0))/14;
      if (co > 4000) co = 4000;

      y0 = mac_muldiv(y0 * 1000 + n_ornament->y, out_stavemagn, 1000);
      y1 = mac_muldiv(y1 * 1000 + n_ornament->y, out_stavemagn, 1000);

      x -= 5000;
      if (n_maxaccleft != 0) x -= n_maxaccleft;
        else if (n_invertleft) x -= 4*out_stavemagn;

      out_slur(x, y0, x, y1, 0, co, 0, 1000);
      }
    break;

    /* Tremolos are handled with their own code. Vertical movement is
    permitted. */

    case or_trem3:
    *p++ = 146;
    /* Fall through */

    case or_trem2:
    *p++ = 146;
    /* Fall through */

    case or_trem1:
    *p++ = 146;

    y = (n_pitch - 248)/2;
    if (n_notetype >= minim) x += (n_upfactor*255*out_stavemagn)/100;

    if (n_upflag)
      {
      int32_t yy = (n_ornament->ornament == or_trem3)? 4000 : 2000;
      y += (n_ornament->ornament == or_trem1)? 4 : 2;
      y = y*1000 + n_ornament->y;
      if (out_beaming && n_stemlength >= yy) y += (n_stemlength - yy)/2;
      if (main_righttoleft) x -= 5000;
      }
    else
      {
      int32_t yy = 2000;
      switch (n_ornament->ornament)
        {
        case or_trem3: y -= 18; yy = 4000; break;
        case or_trem2: y -= 14; break;
        case or_trem1: y -= (y & 2)? 10 : 12; break;
        }
      y = y*1000 + n_ornament->y;
      if (out_beaming && n_stemlength >= yy) y -= (n_stemlength - yy)/2;
      if (main_righttoleft) x += 5000;
      }
    break;

    /* Handle those cases that have no complications, but just require
    setting a string and a position. This includes accidentals printed above
    and below, though we have a small fudge for half accidental styles. At
    present there are only two styles, and the relevant characters are adjacent
    in the font. If this ever changes, the code below will need to be more
    complicated, e.g. using an index listing the relevant characters. */

    default:
    p += sprintf(CS p, "%s", ornament_strings[ornament]);

    /* For the half accidentals, search the string for the accidental character
    and adjust it as necessary. */

    switch (ornament)
      {
      uschar ochar;
      uschar *pp;

      case or_hsharp:
      case or_hsharprb:
      case or_hsharpsb:
      ochar = ornament_strings[or_hsharp][0];
      for (pp = p - 1; pp >= s; pp--)
        if (*pp == ochar) { *pp += curmovt->halfsharpstyle; break; }
      break;

      case or_hflat:
      case or_hflatrb:
      case or_hflatsb:
      ochar = ornament_strings[or_hflat][0];
      for (pp = p - 1; pp >= s; pp--)
        if (*pp == ochar) { *pp += curmovt->halfflatstyle; break; }
      break;
      }

    /* Update the position in case there is another ornament. */

    x += ornament_xadjusts[ornament] * out_stavemagn;
    y += below? ornament_ybadjusts[ornament] : ornament_yaadjusts[ornament];
    break;
    }

  /* Output the string that has been set up (if any). The accidentals treated
  as ornaments are those printed above or below notes, and they have their own
  arrangements for bracketing, so we do not call show_accentornament for them.
  Also, bracketing is not available for tremolos, trills, arpeggios, or spread
  chords. */

  if (p > s)
    {
    int32_t ybadjust = 0;
    *p = 0;
    switch (ornament)
      {
      case or_ferm:
      ybadjust = below? -2000 : 1000;

      /* Fall through */
      case or_mord:
      case or_dmord:
      case or_imord:
      case or_dimord:
      case or_turn:
      case or_iturn:
      show_accentornament(s, size, x,
        out_ystave - mac_muldiv(y, out_stavemagn, 1000),
        n_ornament->bflags,
        0,
        ybadjust,
        ornament_xbrackadjustsL[ornament],
        ornament_xbrackadjustsR[ornament]);
      break;

      default:
      ps_musstring(s, size, x, out_ystave - mac_muldiv(y, out_stavemagn, 1000));
      break;
      }
    }
  }
}



/*************************************************
*     Main function for setting one note/chord   *
*************************************************/

/* The data for notes is put into global variables so they can easily be shared
between the various functions. Note that chords have been sorted so that the
note nearest the stem comes first. The variables n_prevtie and n_nexttie are
set to point to the incoming and outgoing ties, respectively.

Arguments:  pointer to the first note
Returns:    pointer after the last note
*/

bstr *
out_setnote(b_notestr *p)
{
int32_t accleftnum = 0;

TRACE("out_setnote() start\n");

n_acc = p->acc;
n_acflags = n_chordacflags = p->acflags;
n_accleft = n_maxaccleft = (p->accleft * out_stavemagn)/1000;
n_flags = n_chordflags = p->flags;
n_lastnote = p;
n_length = p->length;
n_masq = p->masq;
n_noteheadstyle = p->noteheadstyle & nh_mask;
n_smallhead = (p->noteheadstyle & nhf_smallhead) != 0;
n_notetype = p->notetype;
n_pitch = p->spitch;

/* n_firstacc and n_lastacc are used for computing a y-bound when positioning
ornaments. Ignore a hidden accidental. */

if ((n_flags & nf_accinvis) != 0) n_firstacc = n_lastacc = ac_no;
  else n_firstacc = n_lastacc = n_acc;

/* Set parameters for a rest. We preserve the stem up flag from the previous
note. This is useful in obscure cases such as tieing over rests. A rest also
kills any outstanding underlay or overlay blocks for extension (but not for
hyphens). */

if (n_pitch == 0)
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

  n_restlevel = p->yextra + out_Yadjustment;
  n_prevtie = n_nexttie = NULL;
  n_upflag = out_laststemup[curstave];
  TRACE("rest level=%d upflag=%d\n", n_restlevel, n_upflag);
  }

/* Set parameters for a note, and find the number of notes in a chord. This is
needed for tie direction computations, even for single notes. At the same time
we can find the maximum accidental offset for the note/chord. This is also a
convenient place for setting up the next and previous tie pointers. */

else
  {
  b_notestr *tp;

  n_chordcount = 1;
  n_stemlength = p->yextra;
  n_upflag = (n_flags & nf_stemup) != 0;
  n_upfactor = n_upflag? (+1):(-1);

  n_invertleft = (!n_upflag && (n_flags & nf_invert) != 0);
  n_invertright = (n_upflag && (n_flags & nf_invert) != 0);

  /* Don't use the couplepitch macro, as want to test the distances. */

  if ((n_flags & (nf_coupleU | nf_coupleD)) != 0)
    {
    if ((n_flags & nf_coupleU) != 0)
      {
      n_pitch += out_upgap - 48;
      if ((out_upgap & 7) != 0) main_error_136 = TRUE;
      }
    else
      {
      n_pitch += 48 - out_downgap;
      if ((out_downgap & 7) != 0) main_error_136 = TRUE;
      }
    }

  /* Set up in case this is a chord. */

  n_maxpitch = n_minpitch = n_pitch;
  n_chordfirst = p;

  /* Scan the remaining notes of a chord. */

  for (tp = (b_notestr *)p->next;
       tp->type == b_chord;
       tp = (b_notestr *)tp->next)
    {
    int16_t pitch = tp->spitch;
    int32_t accleft = (tp->accleft * out_stavemagn)/1000;

    if (accleft > n_maxaccleft) n_maxaccleft = accleft;
    if ((n_flags & nf_accinvis) == 0) n_lastacc = tp->acc;

    mac_couplepitch(pitch, tp->flags);
    if (pitch > n_maxpitch) n_maxpitch = pitch;
      else if (pitch < n_minpitch) n_minpitch = pitch;

    if (n_upflag)
      n_invertright |= (tp->flags & nf_invert) != 0;
    else
      n_invertleft |=  (tp->flags & nf_invert) != 0;

    n_chordflags |= tp->flags;
    n_chordacflags |= tp->acflags;

    if (abs(tp->yextra) > abs(n_stemlength)) n_stemlength = tp->yextra;
    n_chordcount++;
    }

  /* tp now points to whatever follows the note/chord. Set up pointers for
  incoming and outgoing ties. */

  n_prevtie = bar_cont->tie;
  bar_cont->tie = NULL;

  if (tp->type == b_tie)
    {
    n_nexttie = (b_tiestr *)tp;
    if (n_upflag) n_nhtied = n_nexttie->belowcount > 0;
      else n_nhtied = n_nexttie->abovecount > 0;
    }
  else
    {
    n_nexttie = NULL;
    n_nhtied = FALSE;
    }

  TRACE("note chordcount=%d upflag=%d\n", n_chordcount, n_upflag);
  }

/* Deal with grace notes. If this is the first in a sequence, preserve the
beaming state; after the last, restore it. */

if (n_length == 0)
  {
  posstr *pp;
  if (n_gracecount == 0)
    {
    save_beam_count        = beam_count;
    save_beam_firstX       = beam_firstX;
    save_beam_firstY       = beam_firstY;
    save_beam_seq          = beam_seq;
    save_beam_slope        = beam_slope;
    save_beam_splitOK      = beam_splitOK;
    save_beam_upflag       = beam_upflag;
    save_beam_Xcorrection  = beam_Xcorrection;
    save_out_beaming       = out_beaming;

    out_beaming = FALSE;
    out_gracenotes = TRUE;
    out_gracefudge = 0;
    }

  /* Set up the font size and the vertical & horizontal corrections */

  n_fontsize = ((n_flags & nf_cuesize) != 0)?
    curmovt->fontsizes->fontsize_cuegrace.size :
    curmovt->fontsizes->fontsize_grace.size;

  n_cueadjust = 0;
  n_pcorrection = (2*out_stavemagn*(10000 - n_fontsize))/10000;

  n_gracemoff = posx_gracefirst + n_gracecount++;
  n_x = out_findXoffset(out_moff + n_gracemoff);

  /* If there is an accidental on the next note or chord ON ANY STAVE, the
  grace note position may have been set to the left of this, if there were
  grace notes on that stave too. We must correctly position the grace note on
  THIS stave. If no staves with accidentals had grace notes, the grace note
  position will be the same as the accidental position, as no space will have
  been left for grace notes. However, if the accidentals are on chords, they
  may be far wider than grace notes. We must check the final grace note's
  position; if it is the same as the accidental's position, allow for one
  accidental's width (5 points). This is all very nasty. */

  if (n_gracecount == 1 && (pp = out_findTentry(out_moff + posx_acc)) != NULL)
    {
    int32_t acc_x = pp->xoff;
    int32_t next_x = out_findXoffset(out_moff);
    int32_t maxaccleft;
    b_notestr *b = misc_nextnote(p);

    while (b->length == 0) b = misc_nextnote(b);
    maxaccleft = b->accleft;

    for(b = (b_notestr *)(b->next);
        b->type == b_chord;
        b = (b_notestr *)(b->next))
      {
      if (b->accleft > maxaccleft) maxaccleft = b->accleft;
      }

    out_gracefudge = (next_x - acc_x) - maxaccleft;
    if ((pp-1)->xoff + out_gracefudge > next_x - curmovt->gracespacing[0])
      out_gracefudge = next_x - curmovt->gracespacing[0] - (pp-1)->xoff;
    if (out_gracefudge < 0) out_gracefudge = 0;
    }

  /* Incorporate any adjustment, and make relative to the bar. */

  out_Xadjustment += out_gracefudge;
  n_x += out_barx + out_Xadjustment;
  }

/* Deal with non-grace notes, first restoring beaming state after gracenotes
have been encountered. */

else
  {
  if (n_gracecount)
    {
    beam_count        = save_beam_count;
    beam_firstX       = save_beam_firstX;
    beam_firstY       = save_beam_firstY;
    beam_seq          = save_beam_seq;
    beam_slope        = save_beam_slope;
    beam_splitOK      = save_beam_splitOK;
    beam_upflag       = save_beam_upflag;
    beam_Xcorrection  = save_beam_Xcorrection;
    out_beaming       = save_out_beaming;

    out_gracenotes = FALSE;
    out_gracefudge = 0;
    }

  /* Sort out the font size and horizontals & vertical corrections for small
  notes. */

  if ((n_flags & nf_cuesize) != 0)
    {
    n_fontsize = curmovt->fontsizes->fontsize_cue.size;
    n_cueadjust = mac_muldiv(3500 - mac_muldiv(3500, n_fontsize, 10000),
        out_stavemagn, 1000);
    n_pcorrection = (2*out_stavemagn*(10000 - n_fontsize))/10000;
    }

  /* Full-sized note */

  else
    {
    n_fontsize = 10000;
    n_pcorrection = n_cueadjust = 0;
    }

  /* Set the horizontal position - centred rests and (semi)breves are a special
  case */

  if ((n_flags & nf_centre) != 0)
    {
    int32_t leftx = 4*out_stavemagn;

    if (out_findTentry(posx_RLright) != NULL)
      leftx += out_barx + out_findXoffset(posx_RLright);
    else if (out_findTentry(posx_timefirst) != NULL)
      leftx += out_barx + out_findXoffset(posx_timefirst);
    else if (out_findTentry(posx_keyfirst) != NULL)
      leftx += out_barx + out_findXoffset(posx_keyfirst);
    else if (out_startlinebar)
      {
      /* Move back to the key signature if no left repeat, else add width of
         repeat */
      leftx = (out_findTentry(posx_RLleft) == NULL)?
        out_barx - PAGE_LEFTBARSPACE : out_barx + 6 * out_stavemagn;
      }
    else leftx = out_lastbarlinex;

    if (out_manyrest == 1)
      {
      n_x = (leftx + out_barlinex)/2 - 3*out_stavemagn;

      /* Adjustments to centred notes to align with centred rest. */

      if (n_pitch != 0)
        {
        n_x -= (((n_notetype == breve)? 12 : (n_notetype == semibreve)?
          13 : 10) * out_stavemagn)/10;
        }

      else if (MFLAG(mf_breverests))
        {
        int t = bar_cont->time;
        int d = t & 255;
        int crotchets = 4 * (t >> 16);    /* 4 times multiplier */

        if (d != time_common && d != time_cut)
          crotchets = (crotchets * ((t >> 8) & 255)) / d;

        if (crotchets == 8 || crotchets == 12) n_notetype = breve;
        if (crotchets == 6 || crotchets == 12) n_flags |= nf_dot;
        }
      }

    else if ((n_flags & nf_hidden) == 0)  /* No number if Q-type rests */
      {
      uint32_t s[12];
      int i = (MFLAG(mf_codemultirests) && out_manyrest < 9)? out_manyrest : 0;
      fontinststr fdata = curmovt->fontsizes->fontsize_restct;

      fdata.size = (fdata.size*out_stavemagn)/1000;
      n_x = (leftx + out_barlinex -
        mac_muldiv(longrest_widths[i], out_stavemagn, 1000))/2;

      /* Remember the mid-point where the long rest count is output to make
      it easier to draw a suitable sign when the bar is wide or narrow. */

      n_longrestmid = out_Xadjustment + (leftx + out_barlinex)/2;
      misc_psprintf(s, curmovt->fonttype_longrest, "%d", out_manyrest);
      out_string(s, &fdata, n_longrestmid - (string_width(s, &fdata, NULL))/2,
        out_ystave - 18*out_stavemagn - out_Yadjustment, 0);
      }
    }

  /* The normal, non-centred case. Unless outputting a grace note, search for
  either the first gracenote position, or (if not found) the note itself. */

  else
    {
    if (curmovt->gracestyle != 0 && n_gracecount == 0)
      n_x = out_barx + out_findGoffset(out_moff + posx_gracefirst, out_moff)
        + out_Xadjustment;
    else
      n_x = out_barx + out_findXoffset(out_moff) + out_Xadjustment;
    if (n_pitch == 0) n_x += 1000;   /* rests all need adjusting */
    }

  /* We can now clear the gracenote count, and also key/time counts. */

  n_gracecount = n_gracemoff = out_keycount = out_timecount = 0;
  }

/* Compute accidental spacing adjustment factors. If the space between adjacent
notes is large, we stretch a bit. Note that out_lastnotex is set very large and
negative when the preceding object is not a note. */

if (n_maxaccleft > 0)
  {
  int32_t accgap = n_x - out_lastnotex - n_maxaccleft;
  accleftnum = n_maxaccleft;
  if (out_lastnotex > 0 && accgap > 20*out_stavemagn)
     accleftnum = (11*n_maxaccleft)/10;
  n_accleft = (n_accleft * accleftnum)/n_maxaccleft;
  }

/* If a plet is starting (out_plet_x < 0), remember its x start. */

if (out_plet_x < 0) out_plet_x = n_x;

/* Deal with beamed note. Take care with the messy code for keeping track of
beamed sequences for plets. The variable beam_seq is maintained for this
purpose:

beam_seq = 0  => not in a beamed sequence
          -1  => at start (first note) of plet
           1  => had beamed sequence in plet
           2  => had rest in beamed sequence

We reset 2 back to 1 if another beamed note is encountered. */

if (n_notetype >= quaver)
  {
  /* Deal with non-rests, either outside or inside beams */

  if (n_pitch != 0)
    {
    /* If not beaming, see if this is the start of one, and cause the beam to
    be output if it is. */

    if (!out_beaming && (n_flags & nf_stem) != 0)
      out_beaming = out_setupbeam(p, out_moff + n_gracemoff, FALSE, FALSE);

    /* If beaming, compute the correct stem length and turn the note type into
    a crotchet. Note that the units of n_stemlength are unmagnified; i.e. they
    are relative to the stave. */

    if (out_beaming)
      {
      int scadjust = ((n_flags & nf_stemcent) == 0)? 0 :
        n_upflag? -STEMCENTADJUST : +STEMCENTADJUST;
      n_stemlength = (n_upfactor*(beam_firstY + mac_muldiv(beam_slope,
        out_findXoffset(out_moff+n_gracemoff) + out_Xadjustment + scadjust +
          (75*out_stavemagn)/100 +
            (n_upflag? beam_Xcorrection : 0) -
              beam_firstX, 1000) -
                (n_pitch - P_1L)*out_pitchmagn -
                  (n_upfactor*14*n_fontsize*out_stavemagn)/10000)*1000)/
                    out_stavemagn;

      if (n_upflag != beam_upflag) n_stemlength += curmovt->beamthickness;
      n_notetype = crotchet;

      /* We can handle a beamed note turning into a minim by masquerade,
      but not anything else. */

      if (n_masq == minim) n_notetype = minim;

      /* If beam_splitOK is true, then we can have notes on the other side of
      the beam. In this case there may be additional stem length adjustments to
      make to any note. The values are whole points. */

      if (beam_splitOK) n_stemlength += beam_stemadjusts[beam_count] * 1000;
      if (beam_seq != 0) beam_seq = 1;
      }

    /* Not a beamed note */

    else beam_seq = 0;
    }

  /* Deal with rests. We need to check if rests at beam starts are permitted
  and we are not currently in a beam. See if this is the start of a beam, and
  if so, fudge up a note for the beam-handling code to start off with.
  Otherwise, check for masquerading within a beam. */

  else
    {
    if (MFLAG(mf_beamendrests) && !out_beaming)
      {
      b_notestr *next = misc_nextnote(p);

      while (next != NULL && next->spitch == 0 && next->notetype >= quaver)
        next = misc_nextnote(next);

      /* There is another beamable note */

      if (next != NULL && next->notetype >= quaver)
        {
        int32_t save_yextra = p->yextra;
        int32_t pextra = save_yextra/500;     /* Rest level in pitch units */
        int32_t nextra = next->yextra/500;
        uint32_t save_flags = p->flags;
        uint32_t save_acflags = p->acflags;

        p->spitch = next->spitch;       /* pretend it's as for the real note */
        p->yextra = next->yextra;
        p->flags = n_flags = next->flags;
        p->acflags = n_acflags = next->acflags;
        n_upflag = (n_flags & nf_stemup) != 0;
        n_upfactor = n_upflag? (+1):(-1);

        if (n_upflag)
          {
          int tt = ((n_notetype > squaver)? 268 : 260) + pextra;
          uint16_t ss = p->spitch;
          if (ss < 244) ss = 244;
          ss += nextra;
          if (ss < tt) p->yextra += (tt - ss)*500;
          }
        else
          {
          int tt = ((n_notetype > quaver)? 276 : 284) + pextra;
          int ss = p->spitch;
          if (ss > 300) ss = 300;
          ss -= nextra;
          if (ss > tt) p->yextra += (ss - tt)*500;
          }

        out_beaming = out_setupbeam(p, out_moff + n_gracemoff, FALSE, TRUE);

        p->spitch = 0;
        p->yextra = save_yextra;
        n_flags = p->flags = save_flags;
        n_acflags = p->acflags = save_acflags;
        }
      }

    /* Handle masquerading rests within a beam -- outside a beam they are
    handled with ordinary notes. Any masquerade is allowed. */

    if (out_beaming && n_masq != MASQ_UNSET) n_notetype = n_masq;
    }
  }

/* Maintain beam_seq in the cases of long notes and rests */

else if (n_notetype < quaver) beam_seq = 0;
  else if (beam_seq == -1) beam_seq = 0;
    else if (beam_seq == 1) beam_seq = 2;

/* Remember whether this note was beamed or not */

TRACE("beaming=%d beam_seq=%d\n", out_beaming, beam_seq);

n_beamed = out_beaming;

/* If not beaming, handle stem length adjustment and alter the note type if
masquerading. */

if (!out_beaming)
  {
  if (n_notetype >= minim)
    {
    int32_t xl = curmovt->stemadjusts[n_notetype];

    if ((n_ornament != NULL && n_ornament->ornament == or_trem3) ||
      (n_upflag && (n_flags & nf_invert) != 0 && n_notetype >= quaver))
        xl += 4000;

    /* The stems of unbeamed, uncoupled notes must reach the centre of the
    stave if pointing that way. */

    if ((n_flags & nf_couple) == 0)
      {
      int32_t minxl = MIN_STEMLENGTH_ADJUST;
      if (n_upflag)
        {
        if (n_pitch < P_0L - P_T) minxl = (P_0L - P_T - n_pitch)*500;
        }
      else
        {
        if (n_pitch > P_6L + P_T) minxl = (n_pitch - P_6L - P_T)*500;
        }
      if (xl < minxl) xl = minxl;
      }

    n_stemlength += xl;
    }

  /* Deal with masquerade */

  if (n_masq != MASQ_UNSET) n_notetype = n_masq;
  }

/* Compute top and bottom positions for the note/chord/rest, used in the code
below that remembers this data for positioning various marks. */

int32_t pt = misc_ybound(FALSE, n_nexttie, TRUE, TRUE);
int32_t pb = misc_ybound(TRUE, n_nexttie, TRUE, TRUE);

/* Remember horizontal and vertical information for hairpins if needed. The
horizontal data is relevant for both visible and invisible notes/rests, whereas
the vertical data must not be affected if the item is not visible. */

if (bar_cont->hairpin != NULL)
  {
  hairpinstr *h = bar_cont->hairpin;

  if (out_hairpinhalf)
    {
    b_hairpinstr *hh = h->hairpin;
    h->x += 6*out_stavemagn +
      mac_muldiv(out_barx + out_findXoffset(out_moff + n_length) - h->x -
        6*out_stavemagn, hh->halfway, 1000);
    out_hairpinhalf = FALSE;
    }

  if ((n_flags & nf_hidden) == 0)
    {
    if (pt > h->maxy) h->maxy = pt;
    if (pb < h->miny) h->miny = pb;
    }
  }

/* Remember data for other marks that need to know what notes are under or
above them. We must do this for rests as well as for notes/chords, but not for
invisible ones. */

if ((n_flags & nf_hidden) == 0)
  {
  slurstr *ss = bar_cont->slurs;

  /* Remember data for slur(s) if first note after start. We don't want any
  accidental on the note to influence the bounding pitch in this case. We must,
  however, use the combined accent flags for computing the bounding pitches.
  (Note: this code is for the true start only - restarts on a new line are
  different, as the accidental then is wanted.) */

  if (out_slurstarted)   /* This flag saves unnecessary work */
    {
    uint32_t flagsave = n_flags;
    uint32_t acflagsave = n_acflags;
    n_flags = n_chordflags;
    n_acflags = n_chordacflags;

    for (slurstr *s = ss; s != NULL; s = s->next)
      {
      if (s->count == 0 && s->x != 0)
        {
        uint16_t flags = (s->slur)->flags;
        BOOL below = (flags & sflag_b) != 0;
        s->moff = out_moff;
        s->x = n_x;

        if (!below &&                     /* Slur above */
            (flags & sflag_l) == 0 &&     /* Not a line (i.e. a slur) */
            (n_flags & nf_stem) != 0 &&   /* Note has a stem */
            n_upflag &&                   /* Stem is up */
            !main_righttoleft)            /* Not right-to-left */
          s->x += 5*out_stavemagn;

        s->lastx = s->x;
        s->y = misc_ybound(below, n_nexttie, FALSE, TRUE);

        /* If the note is beamed, and the slur is on the same side as the beam,
        we need to put in an additional bit of space for clearance. Also, if
        the slur is on the opposite side to the stem, ditto. */

        if (below)
          {
          if (n_upflag || out_beaming) s->y -= 1000;
          }
        else
          {
          if (!n_upflag || out_beaming) s->y += 1000;
          }

        /* Initialize max/min verticals */

        s->maxy = s->miny = s->lasty = s->y;
        }
      }

    out_slurstarted = FALSE;
    n_flags = flagsave;
    n_acflags = acflagsave;
    }

  /* Remember data at the start of continued slurs. If this note is not tied
  onwards, take note of an incoming tie. */

  if (out_moff == 0 && out_startlinebar && !out_passedreset)
    {
    int32_t ptt, pbb;

    if (n_nexttie == NULL && n_prevtie != NULL)
      {
      ptt = misc_ybound(FALSE, n_prevtie, TRUE, TRUE);
      pbb = misc_ybound(TRUE, n_prevtie, TRUE, TRUE);
      }
    else { ptt = pt; pbb = pb; }

    for (slurstr *s = ss; s != NULL; s = s->next)
      {
      if (s->x == 0)
        {
        BOOL below = ((s->slur)->flags & sflag_b) != 0;
        int32_t pbbb = pbb;
        int32_t pttt = ptt;

        if (below)
          {
          if (n_upflag || out_beaming) pbbb -= 1000;
          }
        else
          {
          if (!n_upflag || out_beaming) pttt += 1000;
          }

        s->lastx = n_x;
        s->y = s->lasty = s->maxy = s->miny = (below)? pbbb : pttt;
        }
      }
    }

  /* Remember data for slurs */

  while (ss != NULL)
    {
    BOOL below = ((ss->slur)->flags & sflag_b) != 0;

    /* Compute slope from start and current slope (for use at the end). Skip if
    at same x offset, which can happen after a reset. */

    if (ss->count != 0)              /* first time is different */
      {
      int32_t xx = n_x - ss->lastx;
      if (xx > 0)
        ss->sloperight = mac_muldiv((below? pb : pt) - ss->lasty, 1000, xx);

      xx = n_x - ss->x;
      if (xx > 0)
        {
        int32_t t = mac_muldiv((below? pb : pt) - ss->y, 1000, xx);
        if (below)
          {
          if (t < ss->slopeleft) ss->slopeleft = t;
          }
        else
          if (t > ss->slopeleft) ss->slopeleft = t;
        }

      /* Keep max/min y value -- note: we don't obey this for the first note as
      they have been set already, ignoring any accidental on the first note. We
      don't actually want to include the final note in here, so we actually
      operate one note behind here. */

      if (below)
        {
        if (ss->lasty < ss->miny) ss->miny = ss->lasty;
        }
      else
        {
        if (ss->lasty > ss->maxy) ss->maxy = ss->lasty;
        }

      /* Keep the last pitch and x position, for slope calculation */

      ss->lastx = n_x;
      ss->lasty = below? pb : pt;
      }

    /* Count notes under the slur and advance to next */

    ss->count++;
    ss = ss->next;
    }

  /* Remember data for plet */

  if (out_plet != NULL)
    {
    if (pt > out_plet_highest) out_plet_highest = pt;
    if (pb < out_plet_lowest) out_plet_lowest = pb;
    if (n_pitch > out_plet_highest_head) out_plet_highest_head = n_pitch;
    }

  /* Remember data for numbered repeat bars; a non-scaled value is required. */

  if (bar_cont->nbar != NULL && pt > bar_cont->nbar->maxy)
    bar_cont->nbar->maxy = pt;
  }

/* Output all saved up text that was waiting to find out about this
note/chord's pitch and accents, etc. We clear the above/below counts
afterwards, for the benefit of end-of-bar text. They are initially cleared at
the start of a bar. */

if (out_textqueue_ptr > 0)
  {
  uint32_t save_flags = n_flags;
  uint32_t save_acflags = n_acflags;
  n_flags = n_chordflags;
  n_acflags = n_chordacflags;
  for (size_t i = 0; i < out_textqueue_ptr; i++)
    out_text(out_textqueue[i], FALSE);
  out_textqueue_ptr = out_textnextabove = out_textnextbelow = 0;
  n_flags = save_flags;
  n_acflags = save_acflags;
  }

/* Output any draw items that were waiting likewise */

if (out_drawqueue_ptr > 0)
  {
  draw_ox = n_x + n_cueadjust;  /* Set origin */
  draw_oy = 0;
  for (size_t i = 0; i < out_drawqueue_ptr; i++)
    {
    b_drawstr *d = out_drawqueue[i];
    out_dodraw(d->drawing, d->drawargs, d->overflag);
    }
  out_drawqueue_ptr = 0;
  }

/* Now we can reset the adjustment; it is left till after text and draw
output. */

out_Xadjustment = 0;
out_Yadjustment = 0;

/* Set the note or rest, saving the original stemlength that will be used
afterwards for all accents, slurs etc. because n_stemlength might get altered
for a chord. */

save_orig_stemlength = n_stemlength;
shownote();
out_lastnotex = n_x;

/* Print tremolo bars if required between this note and its predecessor. Note
that the x-values required by ps_beam() are relative to the bar start. Ignore
if inside a beam, or if this note is a rest. (The first can't be a rest if we
have got this far.) We make use of the beam drawing function - hence the
setting of various beam_xxx variables. */

if (out_tremolo != NULL && n_pitch != 0 && !out_beaming)
  {
  int32_t y;
  int32_t x0 = out_tremx - out_barx + (75*out_stavemagn)/100;
  int32_t x1 = n_x - out_barx + (115*out_stavemagn)/100;

  beam_Xcorrection = (51*out_stavemagn)/10;

  if (n_notetype < minim)
    {
    beam_upflag = out_tremupflag = FALSE;
    out_tremolo->join = 0;     /* Force no join */
    x0 += beam_Xcorrection;
    }
  else beam_upflag = n_upflag;

  y = misc_ybound(!beam_upflag, NULL, FALSE, FALSE);  /* y for this note */
  beam_firstY = out_tremy;                            /* y for previous note */

  if (beam_upflag)
    {
    if (out_tremupflag)
      {
      x0 += beam_Xcorrection;
      beam_firstY -= 3*out_stavemagn;
      }
    else beam_firstY += 2*out_stavemagn;
    x1 += beam_Xcorrection;
    y -= 3*out_stavemagn;
    }

  else  /* !beam_upflag */
    {
    if (out_tremupflag)
      {
      x0 += beam_Xcorrection;
      y -= out_stavemagn;
      beam_firstY -= 4*out_stavemagn;
      }
    y += 3*out_stavemagn;
    beam_firstY += 3*out_stavemagn;
    }

  beam_firstX = x0;
  beam_slope = mac_muldiv(y - beam_firstY, 1000, x1 - x0);

  /* Now draw the beams. At the end of the joined ones, shorten the x values.
  If the right-hand note is a semibreve (no stem) and has accidentals, shorten
  even further. */

  for (int i = 0; i < out_tremolo->count; i++)
    {
    if (i == out_tremolo->join)
      {
      int32_t xx = (n_x - out_tremx)/5;
      x0 += xx;
      x1 -= xx;
      if ((n_flags & nf_stem) == 0) x1 -= n_accleft;
      }
    ps_beam(x0, x1, i, 0);
    }

  }

/* Finished with a single note or the stem end note of a chord. Tremolo has
been dealt with. Clear any ornament before outputting the rest of a chord. */

out_tremolo = NULL;
n_ornament = NULL;

/* Deal with the remaining notes. Save and re-instate the note flags for the
main note, if they are printed on the stem side, because these contain data
about accents which is relevant to slurs that end on the note. */

if (n_chordcount > 1)
  {
  int lastpitch = n_pitch;
  uint32_t n_orig_acflags = n_acflags;

  for (b_notestr *tp = (b_notestr *)(p->next);
       tp->type == b_chord;
       tp = (b_notestr *)(tp->next))
    {
    n_pitch = tp->spitch;
    n_flags = tp->flags & ~nf_appogg;   /* No slashes on inside chord notes */
    n_acflags = tp->acflags;
    n_acc = tp->acc;
    n_accleft = (tp->accleft*out_stavemagn)/1000;
    n_noteheadstyle = tp->noteheadstyle & nh_mask;
    n_smallhead = (tp->noteheadstyle & nhf_smallhead) != 0;

    mac_couplepitch(n_pitch, n_flags);
    n_stemlength = (abs(n_pitch - lastpitch)/2 - (14*n_fontsize)/10000) * 1000;

    if (n_stemlength < save_orig_stemlength)
      n_stemlength = save_orig_stemlength;

    if (n_accleft != 0) n_accleft = (n_accleft * accleftnum)/n_maxaccleft;
    n_notetype = (n_masq != MASQ_UNSET)? n_masq : tp->notetype;
    if (n_notetype >= quaver) n_notetype = crotchet;
    if ((bar_cont->flags & cf_notes) != 0) shownote();

    p = tp;               /* p must be left pointing to the last note */
    lastpitch = n_pitch;
    }

  n_stemlength = save_orig_stemlength;  /* restore for slur etc. calculations */

  /* Restore first accents if printed on stem side, else leave last ones. */

  if ((n_orig_acflags & af_opposite) != 0) n_acflags = n_orig_acflags;
  }

/* Output a tie or glissando that ends on this note or chord. Note the the
noteprev field may be pointing either to the first note of a chord or one of
the other notes (as a result of sorting). */

if (n_prevtie != NULL)
  {
  uint8_t flags = n_prevtie->flags;
  if ((flags & (tief_slur|tief_default)) != 0)
    {
    b_notestr *prev = n_prevtie->noteprev;
    BOOL prevchord = prev->type == b_chord || prev->next->type == b_chord;
    if (n_chordcount == 1 && !prevchord)  /* Neither are chords */
      out_setnotetie(n_x, FALSE, flags);
    else
      out_setchordtie(n_chordfirst, n_chordcount, n_x, FALSE, flags);
    }
  if ((flags & tief_gliss) != 0 && n_chordcount == 1) out_glissando(n_x, flags);
  }

/* Cancel the dot adjustment unless a tie follows */

if (n_nexttie == NULL) n_dotxadjust = 0;

/* Cancel masquerade and update last stem flag */

n_masq = MASQ_UNSET;
out_laststemup[curstave] = n_upflag;

/* Reset beaming flags if beam completed and also eset the beaming adjustments
so as to ignore directives that appear in the middle of beams. This is relevant
when beams may be split over a line end; adjustments at the start of the second
bar are ignored unless it is at the start of a line. */

if ((out_lastnotebeamed = out_beaming) == TRUE && --beam_count <= 0)
  {
  out_beaming = beam_continued = FALSE;
  beam_forceslope = INT32_MAX;
  beam_offsetadjust = 0;
  }

/* Advance musical offset */

out_lastmoff = out_moff;
out_moff += n_length;

TRACE("out_setnote() end\n");
return (bstr *)p;                   /* Pointer to last note of chord */
}

/* End of setnote.c */
