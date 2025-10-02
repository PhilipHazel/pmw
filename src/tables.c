/*************************************************
*                PMW fixed tables                *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: January 2021 */
/* This file last modified: October 2025 */

#include "pmw.h"

/* ------------------ Tables for UTF-8 support ------------------ */

/* These are the breakpoints for different numbers of bytes in a UTF-8
character. */

const usint utf8_table1[] =
  { 0x7fu, 0x7ffu, 0xffffu, 0x1fffffu, 0x3ffffffu, 0x7fffffffu };

/* These are the indicator bits and the mask for the data bits to set in the
first byte of a character, indexed by the number of additional bytes. */

const int utf8_table2[] = { 0,    0xc0, 0xe0, 0xf0, 0xf8, 0xfc};
const int utf8_table3[] = { 0xff, 0x1f, 0x0f, 0x07, 0x03, 0x01};

/* Table of the number of extra characters, indexed by the first character
masked with 0x3f. The highest number for a valid UTF-8 character is in fact
0x3d. */

const uschar utf8_table4[] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5 };

/* ------------------------------------------------------------- */

/* Font identification strings. This table must be lengthened if more than 12
extra faces are allowed. */

uschar *font_IdStrings[] = {
  US"rm",  US"it",  US"bf",  US"bi",  US"sy",  US"mf",
  US"xx1", US"xx2", US"xx3", US"xx4", US"xx5", US"xx6", US"xx7", US"xx8",
  US"xx9", US"xx10", US"xx11", US"xx12" };

/* Default sizes for headings */

uint32_t read_headingsizes[] = { 17000, 12000, 10000, 8000, 0 };

/* Clef names */

const char *clef_names[] = {
  "alto", "baritone", "bass", "cbaritone", "contrabass", "deepbass",
  "hclef", "mezzo", "noclef", "soprabass", "soprano", "tenor", "treble",
  "trebledescant", "trebletenor", "trebletenorb" };

/* Note letter quartertone pitch offsets into an octave starting at C, and
adjustments for each accidental. */

/*                             A   B  C  D  E   F   G    */
uint8_t read_basicpitch[] = { 18, 22, 0, 4, 8, 10, 14 };

/*                       none % h#  #  *  h$   $  $$  */
int8_t read_accpitch[] = { 0, 0, 1, 2, 4, -1, -2, -4 };

/* Table of key signatures. The extra ones can be defined using the "makekey"
directive; they are initialized as C major. This table lists where the
accidentals go for the treble clef. For other clefs, the positions are suitably
modified. Each line in the table represents possible accidentals for the key.
The top 4 bits of each byte specify an accidental, e.g. ac_sh for sharp. The
bottom 4 bits are a number in the range 0-10 specifying where to place the
accidental: 0 is below the bottom line of the stave, 1 is the first line, etc.
Macros are used to save typing and make it easier to read. Each signature ends
with END, and unsupported key signatures start with BAD. Allow up to 8
accidentals per signature (standard key signatures never have more than 7). */

#define BAD ks_bad
#define END ks_end

#define S1L ((ac_sh<<4)|1)
#define S2L ((ac_sh<<4)|3)
#define S3L ((ac_sh<<4)|5)
#define S4L ((ac_sh<<4)|7)
#define S5L ((ac_sh<<4)|9)

#define S1S ((ac_sh<<4)|2)
#define S2S ((ac_sh<<4)|4)
#define S3S ((ac_sh<<4)|6)
#define S4S ((ac_sh<<4)|8)
#define S5S ((ac_sh<<4)|10)

#define F1L ((ac_fl<<4)|1)
#define F2L ((ac_fl<<4)|3)
#define F3L ((ac_fl<<4)|5)
#define F4L ((ac_fl<<4)|7)
#define F5L ((ac_fl<<4)|9)

#define F1S ((ac_fl<<4)|2)
#define F2S ((ac_fl<<4)|4)
#define F3S ((ac_fl<<4)|6)
#define F4S ((ac_fl<<4)|8)
#define F5S ((ac_fl<<4)|10)

uint8_t keysigtable[KEYS_COUNT][MAX_KEYACCS+1] = {
  { S5L, S3S, S5S, END,   0,   0,   0,   0,   0 },  /* A major */
  { S5L, S3S, S5S, S4L, S2S, END,   0,   0,   0 },  /* B major */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* C major */
  { S5L, S3S, END,   0,   0,   0,   0,   0,   0 },  /* D major */
  { S5L, S3S, S5S, S4L, END,   0,   0,   0,   0 },  /* E major */
  { F3L, END,   0,   0,   0,   0,   0,   0,   0 },  /* F major */
  { S5L, END,   0,   0,   0,   0,   0,   0,   0 },  /* G major */

  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* A# major */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* B# major */
  { S5L, S3S, S5S, S4L, S2S, S4S, S3L, END,   0 },  /* C# major */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* D# major */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* E# major */
  { S5L, S3S, S5S, S4L, S2S, S4S, END,   0,   0 },  /* F# major */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* G# major */

  { F3L, F4S, F2S, F4L, END,   0,   0,   0,   0 },  /* A$ major */
  { F3L, F4S, END,   0,   0,   0,   0,   0,   0 },  /* B$ major */
  { F3L, F4S, F2S, F4L, F2L, F3S, F1S, END,   0 },  /* C$ major */
  { F3L, F4S, F2S, F4L, F2L, END,   0,   0,   0 },  /* D$ major */
  { F3L, F4S, F2S, END,   0,   0,   0,   0,   0 },  /* E$ major */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* F$ major */
  { F3L, F4S, F2S, F4L, F2L, F3S, END,   0,   0 },  /* G$ major */

  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* A minor */
  { S5L, S3S, END,   0,   0,   0,   0,   0,   0 },  /* B minor */
  { F3L, F4S, F2S, END,   0,   0,   0,   0,   0 },  /* C minor */
  { F3L, END,   0,   0,   0,   0,   0,   0,   0 },  /* D minor */
  { S5L, END,   0,   0,   0,   0,   0,   0,   0 },  /* E minor */
  { F3L, F4S, F2S, F4L, END,   0,   0,   0,   0 },  /* F minor */
  { F3L, F4S, END,   0,   0,   0,   0,   0,   0 },  /* G minor */

  { S5L, S3S, S5S, S4L, S2S, S4S, S3L, END,   0 },  /* A# minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* B# minor */
  { S5L, S3S, S5S, S4L, END,   0,   0,   0,   0 },  /* C# minor */
  { S5L, S3S, S5S, S4L, S2S, S4S, END,   0,   0 },  /* D# minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* E# minor */
  { S5L, S3S, S5S, END,   0,   0,   0,   0,   0 },  /* F# minor */
  { S5L, S3S, S5S, S4L, S2S, END,   0,   0,   0 },  /* G# minor */

  { F3L, F4S, F2S, F4L, F2L, F3S, F1S, END,   0 },  /* A$ minor */
  { F3L, F4S, F2S, F4L, F2L, END,   0,   0,   0 },  /* B$ minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* C$ minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* D$ minor */
  { F3L, F4S, F2S, F4L, F2L, F3S, END,   0,   0 },  /* E$ minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* F$ minor */
  { BAD,   0,   0,   0,   0,   0,   0,   0,   0 },  /* G$ minor */

  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* N */

  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 1 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 2 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 3 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 4 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 5 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 6 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 7 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 8 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 },  /* xkey 9 */
  { END,   0,   0,   0,   0,   0,   0,   0,   0 }   /* xkey 10 */
};


/* Table of per-clef key signature positioning. For each clef there are three
values: a number to subtract from the treble clef position, then limits for
sharps/naturals and flats below which the accidental must be raised by an
octave. */

uint8_t keyclefadjusts[] = {
  1, 2, 1,   /* Alto */
  4, 1, 2,   /* Baritone */
  2, 1, 0,   /* Bass */
  4, 1, 1,   /* Cbaritone */
  2, 1, 1,   /* Contrabass */
  0, 1, 1,   /* Deepbass */
  0, 1, 1,   /* Hclef */
  3, 3, 1,   /* Mezzo */
  0, 1, 1,   /* No clef */
  2, 1, 1,   /* Soprabass */
  5, 1, 1,   /* Soprano */
  6, 3, 3,   /* Tenor */
  0, 1, 1,   /* Treble */
  0, 1, 1,   /* Trebledescant */
  0, 1, 1,   /* Trebletenor */
  0, 1, 1    /* TrebletenorB */
};


/* These tables are used to convert from an accidental-less note in absolute
units to a stave-relative position. Only "white" notes are ever used to index
into the first table, hence the columns of zeros for the quartertones and
semitones. Each column represents one of PMW's supported octaves, 4 on either
side of the middle C octave. The units are quartertones for consistency, but
are not true "pitches" because they are defining positions on the stave, so
there is equal spacing between the note names. */

uint16_t pitch_stave[] = {
/* C           D           E       F           G           A           B */
   8, 0,0,0,  12, 0,0,0,  16, 0,  20, 0,0,0,  24, 0,0,0,  28, 0,0,0,  32, 0,
  36, 0,0,0,  40, 0,0,0,  44, 0,  48, 0,0,0,  52, 0,0,0,  56, 0,0,0,  60, 0,
  64, 0,0,0,  68, 0,0,0,  72, 0,  76, 0,0,0,  80, 0,0,0,  84, 0,0,0,  88, 0,
  92, 0,0,0,  96, 0,0,0, 100, 0, 104, 0,0,0, 108, 0,0,0, 112, 0,0,0, 116, 0,
 120, 0,0,0, 124, 0,0,0, 128, 0, 132, 0,0,0, 136, 0,0,0, 140, 0,0,0, 144, 0,
 148, 0,0,0, 152, 0,0,0, 156, 0, 160, 0,0,0, 164, 0,0,0, 168, 0,0,0, 172, 0,
 176, 0,0,0, 180, 0,0,0, 184, 0, 188, 0,0,0, 192, 0,0,0, 196, 0,0,0, 200, 0,
 204, 0,0,0, 208, 0,0,0, 212, 0, 216, 0,0,0, 220, 0,0,0, 224, 0,0,0, 228, 0,
 232, 0,0,0, 236, 0,0,0, 240, 0, 244, 0,0,0, 248, 0,0,0, 252, 0,0,0, 256, 0 };

/* This table is used to convert a pitch obtained fom the above table into a
position relative to a stave, where 256 is the bottom line on the stave. */

uint16_t pitch_clef[] =

/*   A    Ba   B   cBa   CB   DB   H    M    N    SB   S    Te   Tr  TrD  TrT  TrTB */
  { 152, 168, 176, 168, 166, 184, 128, 144, 128, 166, 136, 160, 128, 128, 128, 128 };


/* These tables give the extra "accidental left" amounts for accidentals in
round and square brackets. */
                              /* no    nt    hs    sh    ds    hf    fl    df */
uint32_t rbra_left[] = { 5800, 5800, 6800, 6800, 5800, 5300, 5300, 5300 };
uint32_t sbra_left[] = { 5800, 6800, 6800, 6800, 6800, 6300, 6300, 6300 };


/* Default font sizes at normal magnification; null pointers to stretch/shear
matrices and zero space stretching. This data is pointed at from the global
default movement structure below. */

static fontsizestr init_fontsizes = {
  { NULL, 10000, 0 },    /* music */
  { NULL,  7000, 0 },    /* grace */
  { NULL,  7000, 0 },    /* cue */
  { NULL,  5000, 0 },    /* cuegrace */
  { NULL, 10000, 0 },    /* midclefs */
  { NULL, 10000, 0 },    /* barnumber */
  { NULL,  9000, 0 },    /* footnote */
  { NULL, 12000, 0 },    /* rehearse */
  { NULL, 10000, 0 },    /* triplet */
  { NULL, 10000, 0 },    /* repno */
  { NULL, 10000, 0 },    /* restct */
  { NULL, 10000, 0 },    /* trill */
  { NULL, 10000, 0 },    /* vertacc */

  /* It doesn't seem possible in ANSI C to parameterize this initialization
  using UserFontSizes and FixedFontSizes, so we must keep this in step when
  either of those are changed. */

  { { NULL, 10000, 0 },  /* user font size 1 */
    { NULL, 10000, 0 },  /* user font size 2 */
    { NULL, 10000, 0 },  /* user font size 3 */
    { NULL, 10000, 0 },  /* user font size 4 */
    { NULL, 10000, 0 },  /* user font size 5 */
    { NULL, 10000, 0 },  /* user font size 6 */
    { NULL, 10000, 0 },  /* user font size 7 */
    { NULL, 10000, 0 },  /* user font size 8 */
    { NULL, 10000, 0 },  /* user font size 9 */
    { NULL, 10000, 0 },  /* user font size 10 */
    { NULL, 10000, 0 },  /* user font size 11 */
    { NULL, 10000, 0 },  /* user font size 12 */
    { NULL, 10000, 0 },  /* user font size 13 */
    { NULL, 10000, 0 },  /* user font size 14 */
    { NULL, 10000, 0 },  /* user font size 15 */
    { NULL, 10000, 0 },  /* user font size 16 */
    { NULL, 10000, 0 },  /* user font size 17 */
    { NULL, 10000, 0 },  /* user font size 18 */
    { NULL, 10000, 0 },  /* user font size 19 */
    { NULL, 10000, 0 },  /* user font size 20 */

    { NULL, 12000, 0 },  /* fixed font size 1 */
    { NULL, 11000, 0 },  /* fixed font size 2 */
    { NULL, 10000, 0 },  /* fixed font size 3 */
    { NULL, 9000, 0 },   /* fixed font size 4 */
    { NULL, 8000, 0 },   /* fixed font size 5 */
    { NULL, 7000, 0 },   /* fixed font size 6 */
    { NULL, 10000, 0 },  /* fixed font size 7 */
    { NULL, 10000, 0 },  /* fixed font size 8 */
    { NULL, 10000, 0 },  /* fixed font size 9 */
    { NULL, 10000, 0 },  /* fixed font size 10 */

    { NULL, 11800, 0 },  /* tsfont */
    { NULL, 10000, 0 },  /* ulay */
    { NULL, 10000, 0 },  /* olay */
    { NULL, 10000, 0 },  /* fbass */
    { NULL, 10000, 0 }   /* init */
  }
};


/* Default relative stave sizes */

static int32_t init_stavesizes[] =
  {
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
  1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000 };


/* Tables for accent and ornament decoding. The values of the or_xx variables
are all less than 256, whereas the flag bits defined by the af_xx variables are
all greater. The ordering of strings that start with the same character is
important! */

accent accent_chars[] = {
  { US"..",  af_staccatiss, 2 },
  { US".",   af_staccato, 1 },
  { US"a10", af_staccatiss, 3 },
  { US"a1",  af_staccato, 2 },
  { US"-",   af_bar, 1 },
  { US"a2",  af_bar, 2},
  { US">",   af_gt, 1 },
  { US"a3",  af_gt, 2 },
  { US"v",   af_wedge, 1 },
  { US"a4",  af_wedge, 2 },
  { US"V",   af_tp, 1 },
  { US"a5",  af_tp, 2 },
  { US"d",   af_down, 1 },
  { US"a6",  af_down, 2 },
  { US"u",   af_up, 1 },
  { US"a7",  af_up, 2 },
  { US"o",   af_ring, 1 },
  { US"a8",  af_ring, 2 },
  { US"'",   af_vline, 1 },
  { US"a9",  af_vline, 2 },
  { US"!",   af_opposite, 1 },
  { US"///", or_trem3, 3 },
  { US"//",  or_trem2, 2 },
  { US"/",   or_trem1, 1 },
  { US"~~|", or_dmord, 3 },
  { US"~~",  or_dimord, 2 },
  { US"~|",  or_mord, 2 },
  { US"~",   or_imord, 1 },
  { US"aru", or_arpu, 3 },
  { US"ard", or_arpd, 3 },
  { US"ar",  or_arp, 2 },
  { US"sp",  or_spread, 2 },
  { US"tr#", or_trsh, 3 },
  { US"tr$", or_trfl, 3 },
  { US"tr%", or_trnat, 3 },
  { US"tr",  or_tr, 2 },
  { US"t|",  or_iturn, 2 },
  { US"t",   or_turn, 1 },
  { US"rt|", or_irturn, 3},
  { US"rt",  or_rturn, 2},
  { US"f",   or_ferm, 1 },
  { NULL,    0, 0 }
};

/* Accidental adjustments are initialized to zero by the rules of Standard C,
so we don't have to specify a list of fixed length. */

static int32_t init_accadjusts[NOTETYPE_COUNT];

/* Accspacing contains the width of each accidental character. The first entry
in the table (corresponding to "no accidental") is used for the narrow Egyptian
half sharp. */

static uint32_t init_accspacing[] =
  /* E#-   %     #-    #     ##    $-    $     $$  */
  { 4800, 4250, 5000, 5000, 5250, 4500, 4500, 8000 };

/* Default trill string is character 136 in the music font. */

static uint32_t init_trillstring[] = { (font_mf << 24)|136, 0 };

/* Default accent move structure (no move, no bracketing flags; the accent
number is not relevant. */

b_accentmovestr no_accent_move = { NULL, NULL, b_accentmove, 0, 0, 0, 0 };

/* Starting stave zero copy block */

static zerocopystr init_zerocopy = { NULL, NULL, 1, 0, 0, 0 };

/* Values for sreadstr that are reset at the start of reading each stave. Some
are overridden by values from the movement. */

sreadstr init_sreadstr = {
  /* Fields that are overridden by data from the movement */

  /* 32_bit fields */
  0,                  /* hairpinwidth */
  0,                  /* key */
  0,                  /* key_tp */
  0,                  /* required_barlength */
  0,                  /* stavenumber */

  /* 8-bit fields */
  0,                  /* barlinestyle */
  FALSE,              /* suspended */

  /* ---- Fields that are not overridden  ---- */

  NULL,               /* beamfirstnote */
  NULL,               /* firstnoteptr */
  NULL,               /* lastnoteptr */
  NULL,               /* lastnote_ornament */
  NULL,               /* pendolay */
  NULL,               /* pendulay */

  /* 32-bit fields */
  0,                  /* accentflags */
  0,                  /* clef_octave */
  0,                  /* hairpinsru */
  0,                  /* hairpiny */
  0,                  /* laststemforce */
  0,                  /* longest_note*/
  0,                  /* matchden */
  0,                  /* matchnum (0 => unset) */
  1,                  /* noteden */
  nf_stem,            /* noteflags (not "noteheads only") */
  1,                  /* notenum */
  0,                  /* octave */
  0,                  /* pitchcount */
  0,                  /* pitchtotal */
  0,                  /* plety */
  0,                  /* rlevel */
  0xffffffffu,        /* shortest_note */
  0,                  /* stemlength */
  0,                  /* textabsolute */
  0,                  /* textflags */
  0,                  /* tuplet_bits */

  /* 16-bit fields */
  0,                  /* beamcount */
  clef_treble,        /* clef */
  0,                  /* couplestate */
  font_rm,            /* fbfont */
  ff_offset_fbass,    /* fbsize */
  0,                  /* hairpinbegun */
  hp_below,           /* hairpinflags */
  0,                  /* lasttiepitch */
  0,                  /* maxaway */
  0,                  /* maxpitch */
  UINT16_MAX,         /* minpitch */
  font_rm,            /* olfont */
  ff_offset_olay,     /* olsize */
  0,                  /* printpitch */
  0,                  /* slurcount */
  font_it,            /* textfont */
  0,                  /* textsize - 0 is user size 1 */
  font_rm,            /* ulfont */
  ff_offset_ulay,     /* ulsize */

  /* 8-bit fields */
  3,                  /* accrit */
  0,                  /* beamstemforce */
  0,                  /* chordcount */
  nh_normal,          /* noteheadstyle */
  0,                  /* ornament */
  0,                  /* pletflags */
  0,                  /* stemsdirection: 0 => auto */
  0,                  /* tiesplacement: 0 => auto */
  FALSE,              /* beaming */
  TRUE,               /* noteson */
  TRUE,               /* laststemup */
  FALSE,              /* lastwasdouble */
  FALSE,              /* lastwasempty */
  FALSE,              /* lastwastied */
  FALSE               /* string_followOK */
};

/* Values for breadstr that are reset at the start of reading each bar. Some
fields are overridden from the current movement or the current stave. */

breadstr init_breadstr = {
  NULL,               /* bar */
  NULL,               /* lasttieensure */
  NULL,               /* repeatstart */

  /* 32-bit fields */
  0,                  /* barlength */
  0,                  /* maxbarlength */
  0,                  /* pletlen */
  0,                  /* pletnum */
  0,                  /* skip */
  0,                  /* smove */

  /* 16-bit fields */
  0,                  /* beamstackptr */
  0,                  /* stemstackptr */

  /* 8-bit fields */
  0,                  /* lastgracestem */
  TRUE,               /* checklength */
  FALSE,              /* checktripletize */
  TRUE,               /* firstnoteinbar */
  FALSE,              /* firstnontied */
  FALSE,              /* firstnontiedset */
  FALSE,              /* nocount */
  FALSE,              /* resetOK */
  FALSE               /* smove_isrelative */
};

/* Initial values for a new stave name structure */

snamestr init_snamestr = {
  NULL,               /* next */
  NULL,               /* extra */
  NULL,               /* text */
  NULL,               /* drawing */
  NULL,               /* drawargs */
  0,                  /* adjustx */
  0,                  /* adjusty */
  0,                  /* flags */
  ff_offset_init,     /* size */
  0                   /* linecount */
};

/* Initial values for a new stave */

stavestr init_stavestr = {
  NULL,               /* stave_name */
  NULL,               /* barindex */
  0,                  /* barindex_size */
  0,                  /* barcount */
  0,                  /* totalpitch; */
  0,                  /* notecount */
  0,                  /* longest_note */
  0xffffffffu,        /* shortest_note */
  0,                  /* tuplet_bits */
  0,                  /* toppitch */
  9999,               /* botpitch */
  5,                  /* stavelines */
  FALSE,              /* omitempty */
  FALSE,              /* halfaccs */
  FALSE               /* hadlayequals */
};


/* Default stavelist item covering all staves */

static stavelist default_stavelist = { NULL, NULL, 1, MAX_STAVE };

/* Default hyphen string for underlay. This is global because it is needed to
compare its width with that of a custom hyphen string. */

uint32_t default_hyphen[] = { (font_rm << 24) | '-', 0 };


/* Default values that start off the first movement. */

movtstr default_movtstr = {

  /* Pointer fields */
  init_accadjusts,    /* accadjusts */
  init_accspacing,    /* accspacing */
  NULL,               /* barvector */
  NULL,               /* bracelist */
  &default_stavelist, /* bracketlist */
  &init_fontsizes,    /* fontsizes */
  NULL,               /* footing */
  NULL,               /* heading */
  default_hyphen,     /* hyphenstring */
  &default_stavelist, /* joinlist */
  NULL,               /* joindottedlist */
  NULL,               /* lastfooting */
  NULL,               /* layout - reset at movement start */
  NULL,               /* midistart */
  NULL,               /* miditempochanges - reset at movement start */
  NULL,               /* pagefooting */
  NULL,               /* pageheading */
  NULL,               /* posvector */
  init_stavesizes,    /* stavesizes */
  {NULL},             /* stavetable */
  NULL,               /* thinbracketlist */
  init_trillstring,   /* trillstring */
  &init_zerocopy,     /* zerocopy */

  /* 64-bit fields */
  0,                  /* breakbarlines */
  ~0,                 /* select_staves */
  0,                  /* suspend_staves */

  /* size_t fields */

  0,                  /* barvector_size */

  /* 32-bit fields */
  0,                  /* barcount */
  0,                  /* barlinesize */
  FIXED_UNSET,        /* barlinespace */
  0,                  /* barnocount */
  0,                  /* barnumber_interval */
  0,                  /* barnumber_level */
  0,                  /* barnumber_textflags */
  0,                  /* baroffset (start bar number) */
  5000,               /* beamflaglength*/
  1800,               /* beamthickness */
  0,                  /* bottommargin */
  2300,               /* breveledgerextra */
    {                 /* clefwidths - whole number of points */
    15,  /* Alto */
    16,  /* Baritone */
    16,  /* Bass */
    15,  /* Cbaritone */
    16,  /* Contrabass */
    16,  /* Deepbase */
    15,  /* Hclef */
    15,  /* Mezzo */
    0,      /* No clef */
    16,  /* Soprabass */
    15,  /* Soprano */
    15,  /* Tenor */
    13,  /* Treble */
    13,  /* Trebledescant */
    13,  /* Trebletenor */
    13   /* TrebletenorB */
    },
  1200,               /* dotspacefactor */
  0,                  /* endlinesluradjust */
  0,                  /* endlinetieadjust */
  0,                  /* extenderlevel */
  mf_startflags,      /* flags */
  { 6000, 6000 },     /* gracespacing */
  4000,               /* footnotesep */
  200,                /* hairpinlinewidth */
  7000,               /* hairpinwidth */
  50000,              /* hyphenthreshold */
  key_C,              /* key - reset at movement start */
  -1,                 /* leftmargin */
  480000,             /* linelength */
  { 310, 330 },       /* maxbeamslope */
  0,                  /* midichanset - reset at movement start */
  120,                /* miditempo */
  mtf_both,           /* miditremolo */
  0,                  /* midkeyspacing */
  0,                  /* midtimespacing */
  1,                  /* noteden - reset at movement start */
  1,                  /* notenum - reset at movement start */
  { 0 },              /* note_spacing - reset at movement start */
  0,                  /* number - set at movement start */
  11000,              /* overlaydepth */
  text_boxed,         /* rehearsalstyle */
  0,                  /* shortenstems */
  700,                /* smallcapsize */
  0,                  /* startbracketbar */
  { 0, 0, 0, 0 },     /* startspace */

  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* stave_ensure */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },

  {     0,44000,44000,44000,44000,44000,44000,44000,  /* stave_spacing */
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000,
    44000,44000,44000,44000,44000,44000,44000,44000 },

  { 0, 0, 0, 0, 0, 0, 2000, 4000 },  /* stemadjusts */

  44000,              /* systemgap */
  20000,              /* systemsepangle */
  0,                  /* systemseplength */
  2000,               /* systemsepwidth */
  0,                  /* systemsepposx */
  0,                  /* systemsepposy */
  time_default,       /* time default = 1*4/4 */
  time_default,       /* time_unscaled */
  10000,              /* topmargin */
  0,                  /* transpose */
  300,                /* tripletlinewidth */
  11000,              /* underlaydepth */

  /* 8-bit fields */
  0,                  /* barlinestyle */
  0,                  /* bracestyle */
  0,                  /* caesurastyle */
  0,                  /* clefstyle */
  0,                  /* endlineslurstyle */
  0,                  /* endlinetiestyle */
  font_rm,            /* fonttype_barnumber */
  font_bf,            /* fonttype_longrest */
  font_rm,            /* fonttype_rehearse */
  font_rm,            /* fonttype_repeatbar */
  font_bf,            /* fonttype_time */
  font_rm,            /* fonttype_triplet */
  0,                  /* gracestyle */
  0,                  /* halfsharpstyle */
  0,                  /* halfflatstyle */
  just_all,           /* justify */
  -1,                 /* laststave */
  -1,                 /* lastreadstave */
  0,                  /* ledgerstyle */

  { 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   /* midichannel (per stave) */
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },

  { 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },  /* midichannelvolume */

  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   /* midinote (0 => no forced pitch) */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },

  { 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,  /* midistavevolume */
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },

  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   /* miditranspose */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },

  { 128,128,128,128,128,128,128,128,   /* midivoice (per MIDI channel) */
    128,128,128,128,128,128,128,128 },

  0,                  /* repeatstyle */
  { 0 },              /* stemswaplevel - all initialized to zero */
  stemswap_default,   /* stemswaptype */
  0                   /* underlaystyle */

};

/* Top and bottom lines of staves, indexed by numbeer of lines in the stave. */

int stave_tops[] =    { 8000, 8000, 12000, 16000, 12000, 16000, 20000 };
int stave_bottoms[] = { 8000, 8000,  4000,     0,     0,     0,     0 };

/* Data for named paper sizes */

sheetstr sheets_list[] = {
  { "a3",     730000, 1060000, 1190000, 842000, sheet_A3 },
  { "a4",     480000,  720000,  842000, 595000, sheet_A4 },
  { "a5",     366000,  480000,  595000, 421000, sheet_A5 },
  { "b5",     420000,  590000,  709000, 499000, sheet_B5 },
  { "letter", 500000,  670000,  792000, 612000, sheet_letter }
};

usint sheets_count = (usint)(sizeof(sheets_list)/sizeof(sheetstr));

/* The out_mfstr table below converts from an idealized (virtual) "music font"
into actual characters and positions in the real music font. The table contains
pointers to mfstr structures, each of which contains an offset and a up to 4
music font characters packed into an unsigned int (i.e. a short string), also a
chain pointer to another mfstr for further characters at a different offset if
necessary. */

/* C clefs */

static mfstr alto          = { NULL, 0,     0, '#' };
static mfstr cbaritone     = { NULL, 0,  8000, '#' };
static mfstr mezzo         = { NULL, 0, -4000, '#' };
static mfstr soprano       = { NULL, 0, -8000, '#' };
static mfstr tenor         = { NULL, 0,  4000, '#' };

/* Old-style C clefs */

static mfstr oldalto       = { NULL, 0,     0, 175 };
static mfstr oldcbaritone  = { NULL, 0,  8000, 175 };
static mfstr oldmezzo      = { NULL, 0, -4000, 175 };
static mfstr oldsoprano    = { NULL, 0, -8000, 175 };
static mfstr oldtenor      = { NULL, 0,  4000, 175 };

/* Bass clefs, modern and old-style */

static mfstr bass          = { NULL, 0,     0, '\"' };
static mfstr oldbass       = { NULL, 0,     0, 174  };
static mfstr baritone      = { NULL, 0, -4000, '\"' };
static mfstr oldbaritone   = { NULL, 0, -4000, 174  };
static mfstr deepbass      = { NULL, 0,  4000, '\"' };
static mfstr olddeepbass   = { NULL, 0,  4000, 174  };

static mfstr cbp1          = { NULL, 2500, -6000, ']'  };
static mfstr contrabass    = { &cbp1,   0,     0, '\"' };
static mfstr oldcontrabass = { &cbp1,   0,     0, 174  };

static mfstr sbp1          = { NULL, 3500, 16500, ']'  };
static mfstr soprabass     = { &sbp1,   0,     0, '\"' };
static mfstr oldsoprabass  = { &sbp1,   0,     0, 174  };

/* Treble clefs */

static mfstr treble        = { NULL, 0, 0, '!' };

static mfstr ttp1          = { NULL, 4000, -12000, ']' };
static mfstr trebletenor   = { &ttp1,   0,      0, '!' };

static mfstr ttp4          = { NULL,  9300, -12000, 158 };
static mfstr ttp3          = { &ttp4, 1000, -12000, 157 };
static mfstr ttp2          = { &ttp3, 4300, -12000, ']' };
static mfstr trebletenorB  = { &ttp2,    0,      0, '!' };

static mfstr tdp1          = { NULL, 6000, 23000, ']' };
static mfstr trebledescant = { &tdp1,   0,     0, '!' };

/* Accidentals */

static mfstr natural       = { NULL, 0, 0, '(' };
static mfstr sharp         = { NULL, 0, 0, '%' };
static mfstr flat          = { NULL, 0, 0, '\'' };
static mfstr doublesharp   = { NULL, 0, 0, '&' };

static mfstr df1           = { NULL, 3900,  0, '\'' };
static mfstr doubleflat    = { &df1, 0,     0, '\'' };

/* Round-bracketed accidentals */

static mfstr rnatural      = { NULL,  0,  0, (142<<16)+('('<<8)+141 };
static mfstr rsharp        = { NULL,  0,  0, (142<<16)+('%'<<8)+141 };
static mfstr rdsharp       = { NULL,  0,  0, (142<<16)+('&'<<8)+141 };

static mfstr rf2           = { NULL,  8000,2000,  142 };
static mfstr rf1           = { &rf2,  3500,   0, '\'' };
static mfstr rflat         = { &rf1,     0,2000,  141 };

static mfstr rdf3          = { NULL, 11900,2000,  142 };
static mfstr rdf2          = { &rdf3, 7400,   0, '\'' };
static mfstr rdf1          = { &rdf2, 3500,   0, '\'' };
static mfstr rdflat        = { &rdf1,    0,2000,  141 };

/* Square-bracket accidentals */

static mfstr snatural      = { NULL,  0,  0, (140<<16)+('('<<8)+139 };
static mfstr ssharp        = { NULL,  0,  0, (140<<16)+('%'<<8)+139 };
static mfstr sdsharp       = { NULL,  0,  0, (140<<16)+('&'<<8)+139 };

static mfstr sf2           = { NULL,  8000,2000,  140 };
static mfstr sf1           = { &sf2,  3500,   0, '\'' };
static mfstr sflat         = { &sf1,     0,2000,  139 };

static mfstr sdf3          = { NULL, 11900,2000,  140 };
static mfstr sdf2          = { &sdf3, 7400,   0, '\'' };
static mfstr sdf1          = { &sdf2, 3500,   0, '\'' };
static mfstr sdflat        = { &sdf1,    0,2000,  139 };

/* Half accidentals */

static mfstr halfsharp1    = { NULL, 0, 0, 189 };
static mfstr halfsharp2    = { NULL, 0, 0, 190 };
static mfstr halfflat1     = { NULL, 0, 0, 191 };
static mfstr halfflat2     = { NULL, 0, 0, 192 };

/* Round-bracketed half accidentals */

static mfstr hrsharp1      = { NULL,  0,  0, (142<<16)+(189<<8)+141 };
static mfstr hrsharp2      = { NULL,  0,  0, (142<<16)+(190<<8)+141 };

static mfstr hrf12         = { NULL,   8000, 2000,  142 };
static mfstr hrf11         = { &hrf12, 3500,    0,  191 };
static mfstr hrflat1       = { &hrf11,    0, 2000,  141 };

static mfstr hrf22         = { NULL,   8000, 2000,  142 };
static mfstr hrf21         = { &hrf22, 3500,    0,  192 };
static mfstr hrflat2       = { &hrf21,    0, 2000,  141 };

/* Square-bracketed half-accidentals */

static mfstr hssharp1      = { NULL,  0,  0, (140<<16)+(189<<8)+139 };
static mfstr hssharp2      = { NULL,  0,  0, (140<<16)+(190<<8)+139 };

static mfstr hsf12         = { NULL,   8000, 2000, 140 };
static mfstr hsf11         = { &hsf12, 3500,    0, 191 };
static mfstr hsflat1       = { &hsf11,    0, 2000, 139 };

static mfstr hsf22         = { NULL,   8000, 2000, 140 };
static mfstr hsf21         = { &hsf22, 3500,    0, 192 };
static mfstr hsflat2       = { &hsf21,    0, 2000, 139 };

/* Miscllaneous */

static mfstr common        = { NULL, 0, 0, '^' };
static mfstr cut           = { NULL, 0, 0, '_' };
static mfstr hclef         = { NULL, 0, 0, 173 };
static mfstr longrest      = { NULL, 0, 0, '0' };


/* This table is indexed by "virtual music font" character. These are defined
as mc_xxx enums and must be kept in step with this table. The order is
arbitrary. */

mfstr *out_mftable[] =
  {
  &treble,       &trebletenor,  &trebledescant, &bass,
  &contrabass,   &soprabass,    &cbaritone,     &tenor,
  &alto,         &mezzo,        &soprano,       &natural,
  &sharp,        &doublesharp,  &flat,          &doubleflat,
  &rnatural,     &rsharp,       &rdsharp,       &rflat,
  &rdflat,       &snatural,     &ssharp,        &sdsharp,
  &sflat,        &sdflat,       &common,        &cut,
  &longrest,     &trebletenorB, &hclef,         &baritone,
  &deepbass,     &oldbass,      &oldcontrabass, &oldsoprabass,
  &oldcbaritone, &oldtenor,     &oldalto,       &oldmezzo,
  &oldsoprano,   &oldbaritone,  &olddeepbass,   &halfsharp1,
  &hrsharp1,     &hssharp1,     &halfsharp2,    &hrsharp2,
  &hssharp2,     &halfflat1,    &hrflat1,       &hsflat1,
  &halfflat2,    &hrflat2,      &hsflat2
  };

/* This table translates character names from PostScript fonts that use Adobe's
standard encoding into Unicode. In addition, for characters whose Unicode
values are greater than LOWCHARLIMIT, it includes the offset above LOWCHARLIMIT
that we use for printing these characters (essentially a private encoding). The
current highest offset is 43. If this is raised, check the unihigh[] variable
in xmlout.c. The value of FONTWIDTHS_SIZE must be large enough to accommodate
all these characters. */

an2uencod an2ulist[] = {
  { US"A",               0x0041, -1 },
  { US"AE",              0x00c6, -1 },
  { US"Aacute",          0x00c1, -1 },
  { US"Abreve",          0x0102, -1 },
  { US"Acircumflex",     0x00c2, -1 },
  { US"Adieresis",       0x00c4, -1 },
  { US"Agrave",          0x00c0, -1 },
  { US"Amacron",         0x0100, -1 },
  { US"Aogonek",         0x0104, -1 },
  { US"Aring",           0x00c5, -1 },
  { US"Atilde",          0x00c3, -1 },
  { US"B",               0x0042, -1 },
  { US"C",               0x0043, -1 },
  { US"Cacute",          0x0106, -1 },
  { US"Ccaron",          0x010c, -1 },
  { US"Ccedilla",        0x00c7, -1 },
  { US"Ccircumflex",     0x0108, -1 },
  { US"Cdotaccent",      0x010a, -1 },
  { US"D",               0x0044, -1 },
  { US"Dcaron",          0x010e, -1 },
  { US"Dcroat",          0x0110, -1 },
  { US"Delta",           0x0394, +0 },
  { US"E",               0x0045, -1 },
  { US"Eacute",          0x00c9, -1 },
  { US"Ebreve",          0x0114, -1 },
  { US"Ecaron",          0x011a, -1 },
  { US"Ecircumflex",     0x00ca, -1 },
  { US"Edieresis",       0x00cb, -1 },
  { US"Edotaccent",      0x0116, -1 },
  { US"Egrave",          0x00c8, -1 },
  { US"Emacron",         0x0112, -1 },
  { US"Eng",             0x014a, -1 },
  { US"Eogonek",         0x0118, -1 },
  { US"Eth",             0x00d0, -1 },
  { US"Euro",            0x20ac, +1 },
  { US"F",               0x0046, -1 },
  { US"G",               0x0047, -1 },
  { US"Gbreve",          0x011e, -1 },
  { US"Gcircumflex",     0x011c, -1 },
  { US"Gcommaaccent",    0x0122, -1 },
  { US"Gdotaccent",      0x0120, -1 },
  { US"H",               0x0048, -1 },
  { US"Hbar",            0x0126, -1 },
  { US"Hcircumflex",     0x0124, -1 },
  { US"I",               0x0049, -1 },
  { US"IJ",              0x0132, -1 },
  { US"Iacute",          0x00cd, -1 },
  { US"Ibreve",          0x012c, -1 },
  { US"Icircumflex",     0x00ce, -1 },
  { US"Idieresis",       0x00cf, -1 },
  { US"Idotaccent",      0x0130, -1 },
  { US"Igrave",          0x00cc, -1 },
  { US"Imacron",         0x012a, -1 },
  { US"Iogonek",         0x012e, -1 },
  { US"Itilde",          0x0128, -1 },
  { US"J",               0x004a, -1 },
  { US"Jcircumflex",     0x0134, -1 },
  { US"K",               0x004b, -1 },
  { US"Kcommaaccent",    0x0136, -1 },
  { US"L",               0x004c, -1 },
  { US"Lacute",          0x0139, -1 },
  { US"Lcaron",          0x013d, -1 },
  { US"Lcommaaccent",    0x013b, -1 },
  { US"Ldot",            0x013f, -1 },
  { US"Lslash",          0x0141, -1 },
  { US"M",               0x004d, -1 },
  { US"N",               0x004e, -1 },
  { US"NBspace",         0x00a0, -1 },  /* Added Nov-2019 */
  { US"Nacute",          0x0143, -1 },
  { US"Ncaron",          0x0147, -1 },
  { US"Ncommaaccent",    0x0145, -1 },
  { US"Ntilde",          0x00d1, -1 },
  { US"O",               0x004f, -1 },
  { US"OE",              0x0152, -1 },
  { US"Oacute",          0x00d3, -1 },
  { US"Obreve",          0x014e, -1 },
  { US"Ocircumflex",     0x00d4, -1 },
  { US"Odieresis",       0x00d6, -1 },
  { US"Ograve",          0x00d2, -1 },
  { US"Ohungarumlaut",   0x0150, -1 },
  { US"Omacron",         0x014c, -1 },
  { US"Oslash",          0x00d8, -1 },
  { US"Otilde",          0x00d5, -1 },
  { US"P",               0x0050, -1 },
  { US"Q",               0x0051, -1 },
  { US"R",               0x0052, -1 },
  { US"Racute",          0x0154, -1 },
  { US"Rcaron",          0x0158, -1 },
  { US"Rcommaaccent",    0x0156, -1 },
  { US"S",               0x0053, -1 },
  { US"Sacute",          0x015a, -1 },
  { US"Scaron",          0x0160, -1 },
  { US"Scedilla",        0x015e, -1 },
  { US"Scircumflex",     0x015c, -1 },
  { US"Scommaaccent",    0x0218, +2 },
  { US"T",               0x0054, -1 },
  { US"Tbar",            0x0166, -1 },
  { US"Tcaron",          0x0164, -1 },
  { US"Tcedilla",        0x0162, -1 },
  { US"Tcommaaccent",    0x021a, +3 },
  { US"Thorn",           0x00de, -1 },
  { US"U",               0x0055, -1 },
  { US"Uacute",          0x00da, -1 },
  { US"Ubreve",          0x016c, -1 },
  { US"Ucircumflex",     0x00db, -1 },
  { US"Udieresis",       0x00dc, -1 },
  { US"Ugrave",          0x00d9, -1 },
  { US"Uhungarumlaut",   0x0170, -1 },
  { US"Umacron",         0x016a, -1 },
  { US"Uogonek",         0x0172, -1 },
  { US"Uring",           0x016e, -1 },
  { US"Utilde",          0x0168, -1 },
  { US"V",               0x0056, -1 },
  { US"W",               0x0057, -1 },
  { US"Wcircumflex",     0x0174, -1 },
  { US"X",               0x0058, -1 },
  { US"Y",               0x0059, -1 },
  { US"Yacute",          0x00dd, -1 },
  { US"Ycircumflex",     0x0176, -1 },
  { US"Ydieresis",       0x0178, -1 },
  { US"Z",               0x005a, -1 },
  { US"Zacute",          0x0179, -1 },
  { US"Zcaron",          0x017d, -1 },
  { US"Zdotaccent",      0x017b, -1 },
  { US"a",               0x0061, -1 },
  { US"aacute",          0x00e1, -1 },
  { US"abreve",          0x0103, -1 },
  { US"acircumflex",     0x00e2, -1 },
  { US"acute",           0x00b4, -1 },
  { US"adieresis",       0x00e4, -1 },
  { US"ae",              0x00e6, -1 },
  { US"agrave",          0x00e0, -1 },
  { US"amacron",         0x0101, -1 },
  { US"ampersand",       0x0026, -1 },
  { US"aogonek",         0x0105, -1 },
  { US"aring",           0x00e5, -1 },
  { US"asciicircum",     0x005e, -1 },
  { US"asciitilde",      0x007e, -1 },
  { US"asterisk",        0x002a, -1 },
  { US"at",              0x0040, -1 },
  { US"atilde",          0x00e3, -1 },
  { US"b",               0x0062, -1 },
  { US"backslash",       0x005c, -1 },
  { US"bar",             0x007c, -1 },
  { US"braceleft",       0x007b, -1 },
  { US"braceright",      0x007d, -1 },
  { US"bracketleft",     0x005b, -1 },
  { US"bracketright",    0x005d, -1 },
  { US"breve",           0x0306, +4 },
  { US"brokenbar",       0x00a6, -1 },
  { US"bullet",          0x00b7, -1 },
  { US"c",               0x0063, -1 },
  { US"cacute",          0x0107, -1 },
  { US"caron",           0x030c, +5 },
  { US"ccaron",          0x010d, -1 },
  { US"ccedilla",        0x00e7, -1 },
  { US"ccircumflex",     0x0109, -1 },
  { US"cdotaccent",      0x010b, -1 },
  { US"cedilla",         0x00b8, -1 },
  { US"cent",            0x00a2, -1 },
  { US"circumflex",      0x0302, +6 },
  { US"colon",           0x003a, -1 },
  { US"comma",           0x002c, -1 },
  { US"commaaccent",     0x0326, +7 },
  { US"copyright",       0x00a9, -1 },
  { US"currency",        0x00a4, -1 },
  { US"d",               0x0064, -1 },
  { US"dagger",          0x2020, +8 },
  { US"daggerdbl",       0x2021, +9 },
  { US"dcaron",          0x010f, -1 },
  { US"dcroat",          0x0111, -1 },
  { US"degree",          0x00b0, -1 },
  { US"dieresis",        0x00a8, -1 },
  { US"divide",          0x00f7, -1 },
  { US"dollar",          0x0024, -1 },
  { US"dotaccent",       0x0307, 10 },
  { US"dotlessi",        0x0131, -1 },
  { US"e",               0x0065, -1 },
  { US"eacute",          0x00e9, -1 },
  { US"ebreve",          0x0115, -1 },
  { US"ecaron",          0x011b, -1 },
  { US"ecircumflex",     0x00ea, -1 },
  { US"edieresis",       0x00eb, -1 },
  { US"edotaccent",      0x0117, -1 },
  { US"egrave",          0x00e8, -1 },
  { US"eight",           0x0038, -1 },
  { US"ellipsis",        0x2026, 11 },
  { US"emacron",         0x0113, -1 },
  { US"emdash",          0x2014, 12 },
  { US"endash",          0x2013, 13 },
  { US"eng",             0x014b, -1 },
  { US"eogonek",         0x0119, -1 },
  { US"equal",           0x003d, -1 },
  { US"eth",             0x00f0, -1 },
  { US"exclam",          0x0021, -1 },
  { US"exclamdown",      0x00a1, -1 },
  { US"f",               0x0066, -1 },
  { US"fi",              0xfb01, 14 },
  { US"five",            0x0035, -1 },
  { US"fl",              0xfb02, 15 },
  { US"florin",          0x0192, 16 },
  { US"four",            0x0034, -1 },
  { US"fraction",        0x2044, 17 },
  { US"g",               0x0067, -1 },
  { US"gbreve",          0x011f, -1 },
  { US"gcircumflex",     0x011d, -1 },
  { US"gcommaaccent",    0x0123, -1 },
  { US"gdotaccent",      0x0121, -1 },
  { US"germandbls",      0x00df, -1 },
  { US"grave",           0x0060, -1 },
  { US"greater",         0x003e, -1 },
  { US"greaterequal",    0x2265, 18 },
  { US"guillemotleft",   0x00ab, -1 },
  { US"guillemotright",  0x00bb, -1 },
  { US"guilsinglleft",   0x2039, 19 },
  { US"guilsinglright",  0x203a, 20 },
  { US"h",               0x0068, -1 },
  { US"hbar",            0x0127, -1 },
  { US"hcircumflex",     0x0125, -1 },
  { US"hungarumlaut",    0x030b, 21 },
  /* 002d is "hyphen-minus"; Unicode also has separate codes for hyphen and
  for minus. We use the latter below. */
  { US"hyphen",          0x002d, -1 },
  { US"i",               0x0069, -1 },
  { US"iacute",          0x00ed, -1 },
  { US"ibreve",          0x012d, -1 },
  { US"icircumflex",     0x00ee, -1 },
  { US"idieresis",       0x00ef, -1 },
  { US"igrave",          0x00ec, -1 },
  { US"ij",              0x0133, -1 },
  { US"imacron",         0x012b, -1 },
  { US"infinity",        0x221e, 43 },
  { US"iogonek",         0x012f, -1 },
  { US"itilde",          0x0129, -1 },
  { US"j",               0x006a, -1 },
  { US"jcircumflex",     0x0135, -1 },
  { US"k",               0x006b, -1 },
  { US"kcommaaccent",    0x0137, -1 },
  { US"kgreenlandic",    0x0138, -1 },
  { US"l",               0x006c, -1 },
  { US"lacute",          0x013a, -1 },
  { US"lcaron",          0x013e, -1 },
  { US"lcommaaccent",    0x013c, -1 },
  { US"ldot",            0x0140, -1 },
  { US"less",            0x003c, -1 },
  { US"lessequal",       0x2264, 22 },
  { US"logicalnot",      0x00ac, -1 },
  { US"longs",           0x017f, -1 },
  { US"lozenge",         0x25ca, 23 },
  { US"lslash",          0x0142, -1 },
  { US"m",               0x006d, -1 },
  { US"macron",          0x00af, -1 },
  { US"minus",           0x2212, 24 },
  { US"mu",              0x00b5, -1 },
  { US"multiply",        0x00d7, -1 },
  { US"n",               0x006e, -1 },
  { US"nacute",          0x0144, -1 },
  { US"napostrophe",     0x0149, -1 },
  { US"ncaron",          0x0148, -1 },
  { US"ncommaaccent",    0x0146, -1 },
  { US"nine",            0x0039, -1 },
  { US"notequal",        0x2260, 25 },
  { US"ntilde",          0x00f1, -1 },
  { US"numbersign",      0x0023, -1 },
  { US"o",               0x006f, -1 },
  { US"oacute",          0x00f3, -1 },
  { US"obreve",          0x014f, -1 },
  { US"ocircumflex",     0x00f4, -1 },
  { US"odieresis",       0x00f6, -1 },
  { US"oe",              0x0153, -1 },
  { US"ogonek",          0x0328, 26 },
  { US"ograve",          0x00f2, -1 },
  { US"ohungarumlaut",   0x0151, -1 },
  { US"omacron",         0x014d, -1 },
  { US"one",             0x0031, -1 },
  { US"onehalf",         0x00bd, -1 },
  { US"onequarter",      0x00bc, -1 },
  { US"onesuperior",     0x00b9, -1 },
  { US"ordfeminine",     0x00aa, -1 },
  { US"ordmasculine",    0x00ba, -1 },
  { US"oslash",          0x00f8, -1 },
  { US"otilde",          0x00f5, -1 },
  { US"p",               0x0070, -1 },
  { US"paragraph",       0x00b6, -1 },
  { US"parenleft",       0x0028, -1 },
  { US"parenright",      0x0029, -1 },
  { US"partialdiff",     0x2202, 27 },
  { US"percent",         0x0025, -1 },
  { US"period",          0x002e, -1 },
  { US"periodcentered",  0x2027, 28 },
  { US"perthousand",     0x2031, 29 },
  { US"plus",            0x002b, -1 },
  { US"plusminus",       0x00b1, -1 },
  { US"q",               0x0071, -1 },
  { US"question",        0x003f, -1 },
  { US"questiondown",    0x00bf, -1 },
  { US"quotedbl",        0x0022, -1 },
  { US"quotedblbase",    0x201e, 30 },
  { US"quotedblleft",    0x201c, 31 },
  { US"quotedblright",   0x201d, 32 },
  { US"quoteleft",       0x2018, 33 },
  { US"quoteright",      0x2019, 34 },
  { US"quotesinglbase",  0x201a, 35 },
  { US"quotesingle",     0x0027, -1 },
  { US"r",               0x0072, -1 },
  { US"racute",          0x0155, -1 },
  { US"radical",         0x221a, 36 },
  { US"rcaron",          0x0159, -1 },
  { US"rcommaaccent",    0x0157, -1 },
  { US"registered",      0x00ae, -1 },
  { US"ring",            0x030a, 37 },
  { US"s",               0x0073, -1 },
  { US"sacute",          0x015b, -1 },
  { US"scaron",          0x0161, -1 },
  { US"scedilla",        0x015f, -1 },
  { US"scircumflex",     0x015d, -1 },
  { US"scommaaccent",    0x0219, 38 },
  { US"section",         0x00a7, -1 },
  { US"semicolon",       0x003b, -1 },
  { US"seven",           0x0037, -1 },
  { US"six",             0x0036, -1 },
  { US"slash",           0x002f, -1 },
  { US"space",           0x0020, -1 },
  { US"sterling",        0x00a3, -1 },
  { US"summation",       0x2211, 39 },
  { US"t",               0x0074, -1 },
  { US"tbar",            0x0167, -1 },
  { US"tcaron",          0x0165, -1 },
  { US"tcedilla",        0x0163, -1 },
  { US"tcommaaccent",    0x021b, 40 },
  { US"thorn",           0x00fe, -1 },
  { US"three",           0x0033, -1 },
  { US"threequarters",   0x00be, -1 },
  { US"threesuperior",   0x00b3, -1 },
  { US"tilde",           0x0303, 41 },
  { US"trademark",       0x2122, 42 },
  { US"two",             0x0032, -1 },
  { US"twosuperior",     0x00b2, -1 },
  { US"u",               0x0075, -1 },
  { US"uacute",          0x00fa, -1 },
  { US"ubreve",          0x016d, -1 },
  { US"ucircumflex",     0x00fb, -1 },
  { US"udieresis",       0x00fc, -1 },
  { US"ugrave",          0x00f9, -1 },
  { US"uhungarumlaut",   0x0171, -1 },
  { US"umacron",         0x016b, -1 },
  { US"underscore",      0x005f, -1 },
  { US"uogonek",         0x0173, -1 },
  { US"uring",           0x016f, -1 },
  { US"utilde",          0x0169, -1 },
  { US"v",               0x0076, -1 },
  { US"w",               0x0077, -1 },
  { US"wcircumflex",     0x0175, -1 },
  { US"x",               0x0078, -1 },
  { US"y",               0x0079, -1 },
  { US"yacute",          0x00fd, -1 },
  { US"ycircumflex",     0x0177, -1 },
  { US"ydieresis",       0x00ff, -1 },
  { US"yen",             0x00a5, -1 },
  { US"z",               0x007a, -1 },
  { US"zacute",          0x017a, -1 },
  { US"zcaron",          0x017e, -1 },
  { US"zdotaccent",      0x017c, -1 },
  { US"zero",            0x0030, -1 }
};

size_t an2ucount = sizeof(an2ulist)/sizeof(an2uencod);

/* End of tables.c */
