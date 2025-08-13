/*************************************************
*                PMW fixed tables                *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: January 2021 */
/* This file last modified: August 2025 */

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
  0,                  /* stemlength */
  0,                  /* textabsolute */
  0,                  /* textflags */

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
  0,                  /* toppitch */
  9999,               /* botpitch */
  5,                  /* stavelines */
  FALSE,              /* omitempty */
  FALSE               /* halfaccs */
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

/* End of tables.c */
