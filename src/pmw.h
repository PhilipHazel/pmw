/*************************************************
*              Main header for PMW               *
*************************************************/

/* Copyright Philip Hazel 2025 */

/* PMW rewrite project started: December 2020 */
/* This file created: December 2020 */
/* This file last modified: October 2025 */

/* This file is included by all the other sources except rdargs.c. */

#define PMW_VERSION "5.33-DEV"
#define PMW_DATE    "05-April-2025"
#define COPYRIGHT   "Copyright (c) Philip Hazel 2025"

/* Standard C headers */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PMW configuration file */

#include "config.h"

/* Optional B2PF support */

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
#include <b2pf.h>
#define B2PF_OUTSTACKSIZE           2000  /* Max B2PF output */
#endif

/* Program parameters */

#define ACCSPACE_COUNT                 8  /* Size of accidental space table */
#define BARACCS_LEN                  216  /* Size of bar accidentals table */
#define BARINDEX_CHUNKSIZE           100  /* Start and increase by this */
#define BEAMBREAK_ALL                255  /* Conventional value */
#define BEAMSTACKSIZE                 50  /* Size of pending beam stack */
#define DEFAULT_ERROR_MAXIMUM         30  /* Give up after this many */
#define DRAW_CHUNKSIZE               256  /* Chunk size for draw routines */
#define DRAWQUEUE_CHUNKSIZE           10  /* For queueing draws before notes */
#define DRAWQUEUE_SIZELIMIT          100  /* Max entries */
#define DRAW_STACKSIZE               100  /* Size of draw stack */
#define FONTLIST_CHUNKSIZE            10  /* Start and increase by this */
#define FONTWIDTHS_SIZE              512  /* 2 encoded fonts */
#define LOWCHARLIMIT                 384  /* See below */
#define MAIN_READBUFFER_CHUNKSIZE    256  /* Start and increase by this */
#define MAIN_READBUFFER_SIZELIMIT  10240  /* To stop mad runaway */
#define MAX_BEAMNOTES                100  /* Hopefully overkill */
#define MAX_CHORDSIZE                 16  /* 16-note chords should be enough */
#define MAX_COMMANDARGS              100  /* Max including .pmwrc args */
#define MAX_DRAW_VARIABLE             19  /* Max number of a draw variable */
#define MAX_KEYACCS                    8  /* Max key signature accidentals */
#define MAX_INCLUDE                   10  /* Max include depth */
#define MAX_LAYOUT                   200  /* Max layout items */
#define MAX_LAYOUT_STACK              20  /* Max nesting depth */
#define MAX_MACROARGS                 20  /* Max number of macro arguments */
#define MAX_MACRODEFAULT             256  /* Max length of macro default argument */
#define MAX_MACRODEPTH                10  /* Depth of macro expansion nesting */
#define MAX_POSTABLESIZE             400  /* Max number of entries */
#define MAX_PLETNEST                   4  /* Max depth of plet nesting */
#define MAX_REPEATSTYLE                4  /* Max repeat style */
#define MAX_REPCOUNT                1000  /* Just to catch crazies */
#define MAX_STAVE                     63  /* Highest stave number */
#define MAX_UNICODE          0x0010ffffu  /* Largest Unicode code point */
#define MAX_UTRANSLATE              1000  /* Max Unicode translations for a font */
#define MAX_XKEYS                     10  /* Max custom keys (must be <= 85) */
#define MIDI_MAXCHANNEL               16  /* Max MIDI channels */
#define MIDI_MAXTEMPOCHANGE           50  /* Max MIDI tempo changes */
#define MIDI_START_CHUNKSIZE          50  /* Start and increase by this */
#define MIN_STEMLENGTH_ADJUST      -8000  /* Minimum stem length adjustment */
#define MEMORY_CHUNKSIZE            8192  /* For never-released memory */
#define MEMORY_MAXBLOCK (MEMORY_CHUNKSIZE - sizeof(char *))
#define MOVTVECTOR_CHUNKSIZE          10  /* For vector of movements */
#define PAGE_LEFTBARSPACE           8000  /* Space at left of lefthand bar */
#define READ_FOOTINGSIZE            8000  /* Default size ) for headings */
#define READ_PAGEHEADINGSIZE       10000  /* Default size ) there is a list */
#define STEMCENTADJUST              2545  /* Adjustment for central stems */
#define STEMSTACKSIZE                 50  /* Size of pending stem stack */
#define STRETCHTHRESHDEN               2  /* Currently fixed stretching */
#define STRETCHTHRESHNUM               1  /*   threshold of 1/2 */
#define STRETCHRESPACETHRESH        1075  /* Threshold for respacing */
#define STRINGBUFFER_CHUNKSIZE       256  /* Start and increase by this */
#define STRINGBUFFER_SIZELIMIT     20000  /* Max string length */
#define TEXTQUEUE_CHUNKSIZE           10  /* For queueing texts before notes */
#define TEXTQUEUE_SIZELIMIT          100  /* Max entries */
#define UNICODE_PRIVATE           0xe000  /* Start of Unicode "private use" characters */
#define UUSIZE                       100  /* For list of unsupported/invalid Unicode codes */
#define WORDBUFFER_SIZE               64

/* Characters whose code point is less than LOWCHARLIMIT can be used directly
in the "doubled-up" font encoding for standardly-encoded fonts. Some other
Unicode values are translated when a string is read into code points just above
this limit, which is no longer Unicode encoding. FONTWIDTHS_SIZE must be large
enough to accommodate these extra characters. */

/* Major program states */

enum { STATE_INIT, STATE_READ, STATE_PAGINATE, STATE_WRITE, STATE_ENDING };

/* Bits in the main_testing variable. For PS output, any non-zero value runs in
testing mode. Only for PDF are there separate actions. */

#define mtest_version     0x01u   /* Suppress version and date info */
#define mtest_barids      0x02u   /* Insert bar and page idents (PDF) */
#define mtest_omitfont    0x04u   /* Omit font programs (PDF) */
#define mtest_forcered    0x08u   /* For red output (PDF) */

/* A maximum is set for transposition - 5 octaves should be ample - and a
conventional value for "no transposition". */

#define MAX_TRANSPOSE  120
#define NO_TRANSPOSE  1000

/* Names for C major, A minor, and the special key 'N', which behaves like C
major (no key signature) but does not transpose. There are 42 normal keys (7
letters times 2 modes times 3 accidentals) whose values are 0-41. There are
also up to MAX_XKEYS custom-defined keys, starting at 43. */

#define key_C   2
#define key_Am  21
#define key_N   42  /* The answer to life, the universe, and everything. */
#define key_X   43  /* The first custom key. */

/* This bit is added to a key signature in some contexts to indicate "reset
with naturals". */

#define key_reset  0x80u

/* The total number of possible key signatures. */

#define KEYS_COUNT (key_X + MAX_XKEYS)

/* Special values in key signature definitions */

#define ks_bad  0xf0u   /* Unsupported key signature */
#define ks_end  0xffu   /* End of accidental list */

/* Accidental types. The "half-full-double" triplets must be kept together, in
that order. The overall ordering has the three taller accidentals (natural,
half sharp, and sharp) first. There is a dependence on this in the positioning
of accidentals on chords. These values are also used to index into certain
tables. */

#define ac_no   0u      /* None */
#define ac_nt   1u      /* Natural */
#define ac_hs   2u      /* Half sharp */
#define ac_sh   3u      /* Sharp */
#define ac_ds   4u      /* Double sharp */
#define ac_hf   5u      /* Half flat */
#define ac_fl   6u      /* Flat */
#define ac_df   7u      /* Double flat */

/* Note pitch values, measured in quarter tones. */

#define OCTAVE  24
#define MIDDLE_C   (4*OCTAVE)   /* Allow 4 octaves below middle C */

/* If MaxExtraFont is increased, check that font_IdStrings contains sufficient
identifying strings, and that ps_IdStrings is of the right size too. If
MaxFontSizes is changed, the initializing table init_fontsizes must be made to
be the correct size. */

#define MaxExtraFont   12   /* user-settable typefaces */
#define UserFontSizes  20   /* user-settable text sizes for /s */
#define FixedFontSizes 10   /* non-settable text sizes for /S */
#define AllFontSizes   (UserFontSizes+FixedFontSizes)

/* Offsets for special text font sizes and matrices, which follow in the same
vector as text sizes specified by the user and the fixed sizes. */

#define ff_offset_ts     (AllFontSizes)
#define ff_offset_ulay   (ff_offset_ts+1)
#define ff_offset_olay   (ff_offset_ulay+1)
#define ff_offset_fbass  (ff_offset_olay+1)
#define ff_offset_init   (ff_offset_fbass+1)
#define TEXTSIZES_SIZE   (ff_offset_init+1)

/* Flags that appear in fontstr blocks. */

#define ff_stdencoding  0x00000001u    /* std encoding */
#define ff_fixedpitch   0x00000002u    /* fixed pitch */
#define ff_include      0x00000004u    /* to be included in output */
#define ff_used         0x00000008u    /* font is used (for PDF output) */
#define ff_usedlower    0x00000010u    /* lower half used (for std enc) */
#define ff_usedupper    0x00000020u    /* upper half used (for std enc) */

/* Identifiers for each type of font. Any changes in this list must be kept in
step with the list of font ids which is kept in Font_IdStrings. Font ids must
be less than 0x80 because they are put into the top byte of 32-bit characters,
with 0x80 reserved to indicate a smaller than normal size, either small caps or
the \mu\ music font at 0.9 size. */

enum { font_rm, font_it, font_bf, font_bi, font_sy, font_mf, font_xx,
       font_tablen = font_xx + MaxExtraFont };

#define font_small   0x80u
#define font_unknown 0x7fu

/* These define the characters that are substituted for unavailable characters,
in standardly encoded and non-standardly encoded fonts, respectively. The
latter is always taken from the Music font. */

#define UNKNOWN_CHAR_S   0x00a4    /* Currency symbol */
#define UNKNOWN_CHAR_N   0x0024    /* Triangle in Music */

/* Some specific Unicode characters */

#define QUOTE_LEFT       0x2018
#define QUOTE_RIGHT      0x2019
#define CHAR_FI          0xfb01    /* fi ligature */

/* Some special character values used in character strings. They are outside
the Unicode range, but still within 24-bits. */

#define ss_page          (MAX_UNICODE + 1)  /* Insert page number */
#define ss_pageodd       (MAX_UNICODE + 2)  /* Ditto, if odd */
#define ss_pageeven      (MAX_UNICODE + 3)  /* Ditto, if even */
#define ss_skipodd       (MAX_UNICODE + 4)  /* Skip if page number odd */
#define ss_skipeven      (MAX_UNICODE + 5)  /* Skip if page number even */
#define ss_repeatnumber  (MAX_UNICODE + 6)  /* Insert bar repeat number */
#define ss_repeatnumber2 (MAX_UNICODE + 7)  /* Insert bar repeat number from 2 */
#define ss_verticalbar   (MAX_UNICODE + 8)  /* Unescaped vertical bar */
#define ss_asciiquote    (MAX_UNICODE + 9)  /* ASCII quote character */
#define ss_asciigrave    (MAX_UNICODE + 10) /* ASCII grave accent */
#define ss_escapedhyphen (MAX_UNICODE + 11) /* Escaped hyphen (for underlay) */
#define ss_escapedequals (MAX_UNICODE + 12) /* Escaped equals (ditto) */
#define ss_escapedsharp  (MAX_UNICODE + 13) /* Escaped sharp (ditto) */
#define ss_top           (MAX_UNICODE + 14) /* One after last used */

/* Save some typing for unsigned ints and unsigned chars */

typedef unsigned int  usint;
typedef unsigned char uschar;

/* Boolean stuff*/

typedef usint BOOL;
typedef uint8_t CBOOL;
#define FALSE 0
#define TRUE  1

/* Unsigned char handling */

#define CS   (char *)
#define CCS  (const char *)
#define CSS  (char **)
#define US   (unsigned char *)
#define CUS  (const unsigned char *)

#define Uatoi(s)             atoi(CS(s))
#define Ufgets(b,n,f)        US fgets(CS(b),n,f)
#define Ufopen(s,t)          fopen(CCS(s),CCS(t))
#define Ufputs(b,f)          fputs(CS(b),f)
#define Ustrcat(s,t)         strcat(CS(s),CS(t))
#define Ustrchr(s,n)         US strchr(CS(s),n)
#define Ustrcmp(s,t)         strcmp(CCS(s),CCS(t))
#define Ustrcpy(s,t)         strcpy(CS(s),CCS(t))
#define Ustrlen(s)           (size_t)strlen(CCS(s))
#define Ustrncmp(s,t,n)      strncmp(CCS(s),CCS(t),n)
#define Ustrncmpic(s,t,n)    strncmpic(CUS(s),CUS(t),n)
#define Ustrncpy(s,t,n)      strncpy(CS(s),CS(t),n)
#define Ustrrchr(s,n)        US strrchr(CCS(s),n)
#define Ustrstr(s, t)        US strstr(CS(s),CS(t))
#define Ustrtoul(s,t,b)      strtoul(CS(s),CSS(t),b)

/* Miscellaneous definitions. */

#define ENDFILE            (-1)
#define FIXED_UNSET        ((int32_t)(~0u))
#define KEY_UNSET          0xffffffffu
#define SIZE_UNSET         ((size_t)(~0uL))

/* Functional and shorthand macros */

#define sfb string_format_barnumber
#define sfd string_format_double
#define SFD string_format_multiple_double
#define sff string_format_fixed
#define SFF string_format_multiple_fixed
#define sfn string_format_notelength
#define sfp string_format_pitch

#define isdigitorsign(x) (x=='-'||x=='+'||isdigit(x))

#define PCHAR(x)  ((x) & 0x00ffffffu)
#define PFTOP(x)  ((x) & 0xff000000u)
#define PFONT(x)  (((x) & 0xff000000u)>>24)
#define PBFONT(x) (PFONT(x) & ~font_small)

#define MFLAG(x) ((curmovt->flags & x) != 0)

#define read_nextsigc() read_nextc(); read_sigc()
#define read_sigc() while (isspace(read_c)) read_nextc()
#define read_sigcc() { if (read_c == ',') read_nextc(); read_sigc(); }
#define read_sigcNL() while (read_c != '\n' && isspace(read_c)) read_nextc()

#define mac_muldiv(a,b,c) \
  ((int)((((double)((int)(a)))*((double)((int)(b))))/((double)((int)(c)))))

#define mac_nextnote(a,b) \
  for (a = (b_notestr *)(b->next); \
       a != NULL && a->type != b_note; \
       a = (b_notestr *)(a->next)) {}

#define mac_couplepitch(p, f) \
  if ((f & (nf_coupleU | nf_coupleD)) != 0) \
    p += (((f & nf_coupleU) != 0)? (out_upgap - 48):(48 - out_downgap))

#define mac_setstackedstems(flag) \
  while (brs.stemstackptr > 0) \
    { (read_stemstack[--brs.stemstackptr])->flags |= flag; \
      if (((read_stemstack[brs.stemstackptr])->flags & nf_chord) != 0) \
        read_sortchord(read_stemstack[brs.stemstackptr], flag); }

#define mac_setstemflag(noteptr, flag) \
  { \
  noteptr->flags |= flag; \
  if ((noteptr->flags & nf_chord) != 0) read_sortchord(noteptr, flag); \
  }

#define mac_setbit(a,b)    a |= 1Lu<<(b)
#define mac_clrbit(a,b)    a &= ~(1Lu<<(b))
#define mac_isbit(a,b)     ((a & (1Lu<<(b))) != 0)
#define mac_isbit2(a,c,b)  ((a & c & (1Lu<<(b))) != 0)
#define mac_notbit(a,b)    ((a & (1Lu<<(b))) == 0)
#define mac_notbit2(a,c,b) ((a & c & (1Lu<<(b))) == 0)

#define mac_emptybar(a) (a->next->type == b_barline)

#define poutx(x)  ((x) + print_xmargin)
#define pouty(y)  (pout_ymax - (y))

/* Tracing and debugging. D_any is set for all -d instances at the start of
debug decoding. */

#define D_all          0xffffffffu
#define D_any          0x00000001u
#define D_bar          0x00000002u  /* ) Synonymous */
#define D_barR         0x00000002u  /* )            */
#define D_barP         0x00000004u
#define D_barO         0x00000008u
#define D_barX         0x00000010u
#define D_font         0x00000020u
#define D_header_all   0x00000040u
#define D_header_glob  0x00000080u
#define D_macro        0x00000100u
#define D_memory       0x00000200u
#define D_memorydetail 0x00000400u
#define D_movtflags    0x00000800u
#define D_preprocess   0x00001000u
#define D_sortchord    0x00002000u
#define D_stringwidth  0x00004000u
#define D_trace        0x00008000u
#define D_xmlanalyze   0x00010000u
#define D_xmlgroups    0x00020000u
#define D_xmlread      0x00040000u
#define D_xmlstaves    0x00080000u

#define DEBUG(x)       if ((debug_selector & (x)) != 0)
#define TRACE(...)     if ((debug_selector & D_trace) != 0) \
                         (void)fprintf(stderr, __VA_ARGS__)
#define eprintf(...)   (void)fprintf(stderr, __VA_ARGS__)

/* Error numbers, named for ease of finding. */

enum error_number {
  ERR0,  ERR1,  ERR2,  ERR3,  ERR4,  ERR5,  ERR6,  ERR7,  ERR8,  ERR9,
  ERR10, ERR11, ERR12, ERR13, ERR14, ERR15, ERR16, ERR17, ERR18, ERR19,
  ERR20, ERR21, ERR22, ERR23, ERR24, ERR25, ERR26, ERR27, ERR28, ERR29,
  ERR30, ERR31, ERR32, ERR33, ERR34, ERR35, ERR36, ERR37, ERR38, ERR39,
  ERR40, ERR41, ERR42, ERR43, ERR44, ERR45, ERR46, ERR47, ERR48, ERR49,
  ERR50, ERR51, ERR52, ERR53, ERR54, ERR55, ERR56, ERR57, ERR58, ERR59,
  ERR60, ERR61, ERR62, ERR63, ERR64, ERR65, ERR66, ERR67, ERR68, ERR69,
  ERR70, ERR71, ERR72, ERR73, ERR74, ERR75, ERR76, ERR77, ERR78, ERR79,
  ERR80, ERR81, ERR82, ERR83, ERR84, ERR85, ERR86, ERR87, ERR88, ERR89,
  ERR90, ERR91, ERR92, ERR93, ERR94, ERR95, ERR96, ERR97, ERR98, ERR99,
  ERR100,ERR101,ERR102,ERR103,ERR104,ERR105,ERR106,ERR107,ERR108,ERR109,
  ERR110,ERR111,ERR112,ERR113,ERR114,ERR115,ERR116,ERR117,ERR118,ERR119,
  ERR120,ERR121,ERR122,ERR123,ERR124,ERR125,ERR126,ERR127,ERR128,ERR129,
  ERR130,ERR131,ERR132,ERR133,ERR134,ERR135,ERR136,ERR137,ERR138,ERR139,
  ERR140,ERR141,ERR142,ERR143,ERR144,ERR145,ERR146,ERR147,ERR148,ERR149,
  ERR150,ERR151,ERR152,ERR153,ERR154,ERR155,ERR156,ERR157,ERR158,ERR159,
  ERR160,ERR161,ERR162,ERR163,ERR164,ERR165,ERR166,ERR167,ERR168,ERR169,
  ERR170,ERR171,ERR172,ERR173,ERR174,ERR175,ERR176,ERR177,ERR178,ERR179,
  ERR180,ERR181,ERR182,ERR183,ERR184,ERR185,ERR186,ERR187,ERR188,ERR189,
  ERR190,ERR191,ERR192,ERR193,ERR194,ERR195,ERR196
};

/* Types of input file */

enum filetype { FT_AUTO, FT_PMW, FT_ABC, FT_MXML };

/* Clef identifiers. Keep in step with the clef_names list in tables, and
various data tables in out.c and xmlout.c. */

enum {
  clef_alto,
  clef_baritone,
  clef_bass,
  clef_cbaritone,
  clef_contrabass,
  clef_deepbass,
  clef_hclef,
  clef_mezzo,
  clef_none,
  clef_soprabass,
  clef_soprano,
  clef_tenor,
  clef_treble,
  clef_trebledescant,
  clef_trebletenor,
  clef_trebletenorB,
  CLEF_COUNT };

/* Stem swap options */

enum { stemswap_default, stemswap_up, stemswap_down, stemswap_left,
       stemswap_right };

/* Types for data in a layout vector */

enum { lv_barcount, lv_repeatcount, lv_repeatptr, lv_newpage };

/* Conventional and default time signatures */

#define time_common   0x000000ffu
#define time_cut      0x000000feu
#define time_default  0x00010404u

/* Flags for text strings */

#define text_above      0x00000001u  /* Place above the stave */
#define text_absolute   0x00000002u  /* Positioning is absolute, not relative */
#define text_atulevel   0x00000004u  /* Normal text at underlay level */
#define text_baralign   0x00000008u  /* Align text at starting barline */
#define text_barcentre  0x00000010u  /* Centre the text in the bar */
#define text_boxed      0x00000020u  /* Enclose in a box */
#define text_boxrounded 0x00000040u  /* ... with rounded corners */
#define text_centre     0x00000080u  /* Centred  */
#define text_endalign   0x00000100u  /* End aligned */
#define text_fb         0x00000200u  /* Figured bass */
#define text_followon   0x00000400u  /* Follow-on to previous */
#define text_middle     0x00000800u  /* Put halfway betwen staves */
#define text_rehearse   0x00001000u  /* Rehearsal mark */
#define text_ringed     0x00002000u  /* Enclose in a ring */
#define text_timealign  0x00004000u  /* Align with time signature */
#define text_ul         0x00008000u  /* Underlay; with text_above = overlay */

/* Flags for stave name texts */

#define snf_vcentre     0x01u
#define snf_hcentre     0x02u
#define snf_rightjust   0x04u
#define snf_vertical    0x08u

/* Specific sheet sizes */

enum sheet { sheet_unknown, sheet_A5, sheet_A4, sheet_A3, sheet_B5,
  sheet_letter };

/* Print configuration options */

enum { pc_normal, pc_a4sideways, pc_a4ona3, pc_a5ona4, pc_EPS };

/* Bits for remembering which headings/footings have been read, for the purpose
of throwing away old ones at movement starts. These are also used for selecting
default sizes when reading them in. */

#define rh_footing       0x001u
#define rh_footnote      0x002u
#define rh_heading       0x004u
#define rh_lastfooting   0x008u
#define rh_pagefooting   0x010u
#define rh_pageheading   0x020u

/* Types of value in draw programs, stacks, and variables; used in the dtype
field in the structure above for items on the stack and for variables. */

#define dd_any      1      /* Used in the checklist for any type allowed */
#define dd_number   2
#define dd_text     3
#define dd_code     4
#define dd_varname  5

/* Justification options */

#define just_none       0u
#define just_top     0x01u
#define just_bottom  0x02u
#define just_left    0x04u
#define just_right   0x08u
#define just_horiz   (just_left|just_right)
#define just_vert    (just_top|just_bottom)
#define just_all     (just_horiz|just_vert)
#define just_add     0x80u  /* For justification changes */

/* Type values that identify each structure in a bar's data. */

enum {
  b_start,    /* This one is put in the barhdr structure */

  b_accentmove, b_all, b_barline, b_barnum, b_beamacc, b_beambreak, b_beammove,
  b_beamrit, b_beamslope, b_bowing, b_breakbarline, b_caesura, b_chord, b_clef,
  b_comma, b_dotbar, b_dotright, b_draw, b_endline, b_endplet, b_endslur,
  b_ens, b_ensure, b_footnote, b_hairpin, b_justify, b_key, b_linegap,
  b_lrepeat, b_midichange, b_move, b_name, b_nbar, b_newline, b_newpage,
  b_note, b_noteheads, b_notes, b_ns, b_nsm, b_olevel, b_olhere, b_ornament,
  b_overbeam, b_page, b_pagebotmargin, b_pagetopmargin, b_plet, b_reset,
  b_resume, b_rrepeat, b_sgabove, b_sghere, b_sgnext, b_slur, b_slurgap,
  b_space, b_ssabove, b_sshere, b_ssnext, b_suspend, b_text, b_tick, b_tie,
  b_time, b_tremolo, b_tripsw, b_ulevel, b_ulhere, b_unbreakbarline,
  b_zerocopy,

  b_baditem   /* For detecting bad values */
};

/* It is helpful to have names for the vertical offsets of the various lines
and spaces on the stave. The bottom line is numbered one. These pitches are the
appropriate vertical offsets from the bottom line to print a note or an
accidental on the relevant line or space. */

#define L_0L       -6000
#define L_0S       -4000
#define L_1L       -2000
#define L_1S           0
#define L_2L        2000
#define L_2S        4000
#define L_3L        6000
#define L_3S        8000
#define L_4L       10000
#define L_4S       12000
#define L_5L       14000
#define L_5S       16000
#define L_6L       18000

/* Similarly, define names for positions on the stave in stave pitch units. The
bottom line of a 5-line stave has a value of 256 and lines are 2 "tones" (8
quarter tones) apart. This is exactly double the distance in points. In the
original PMW this value was 2 semitones; it is assumed to be a power of 2. When
used as a bit mask on a stave pitch, the bit is set for notes on spaces and
unset for notes on lines. When computing the positions of things other than
notes, e.g. slurs, values other than multiples of P_T may be used. */

#define P_T      4             /* One "tone" */
#define P_M      (2*P_T-1)     /* Mask to check for even multiple of P_T */
#define P_0L     (P_1L-2*P_T)
#define P_0S     (P_1L-1*P_T)
#define P_1L     256           /* Base line of a 5-line stave */
#define P_1S     (P_1L+1*P_T)
#define P_2L     (P_1L+2*P_T)
#define P_2S     (P_1L+3*P_T)
#define P_3L     (P_1L+4*P_T)
#define P_3S     (P_1L+5*P_T)
#define P_4L     (P_1L+6*P_T)
#define P_4S     (P_1L+7*P_T)
#define P_5L     (P_1L+8*P_T)
#define P_5S     (P_1L+9*P_T)
#define P_6L     (P_1L+10*P_T)
#define P_6S     (P_1L+11*P_T)
#define P_7L     (P_1L+12*P_T)


/* Note types. These must be in descending order of length so that adding one
halves the note length. We also need a value for unset masquerade that cannot
be a note type. These are unsigned 8-bit values. */

enum { breve, semibreve, minim, crotchet, quaver, squaver, dsquaver, hdsquaver,
       NOTETYPE_COUNT, MASQ_UNSET };

/* Lengths for notes. Even if longer notes are added, there should be plenty of
space in 32-bits. */

#define len_breve      (128*4*5*7*9*11*13)   /* This is 0x015fea00 */
#define len_semibreve  (len_breve/2)
#define len_minim      (len_breve/4)
#define len_crotchet   (len_breve/8)
#define len_quaver     (len_breve/16)
#define len_squaver    (len_breve/32)
#define len_dsquaver   (len_breve/64)
#define len_hdsquaver  (len_breve/128)
#define len_shortest   len_hdsquaver

#define TUPLET_ROUND   10  /* Rounding tolerance for tuplets */

/* Plet flags */

#define plet_a     0x01u  /* above */
#define plet_b     0x02u  /* below */
#define plet_lx    0x04u  /* invert left end jog */
#define plet_rx    0x08u  /* invert right end jog */
#define plet_x     0x10u  /* suppress triplet mark altogether */
#define plet_bn    0x20u  /* force no bracket */
#define plet_by    0x40u  /* force bracket */
#define plet_abs   0x80u  /* adjust values are absolute positions */

/* Slur flags - a 16-bit field */

#define sflag_w       0x0001u   /* wiggly */
#define sflag_b       0x0002u   /* below */
#define sflag_l       0x0004u   /* line */
#define sflag_h       0x0008u   /* horizontal */
#define sflag_ol      0x0010u   /* line open on left */
#define sflag_or      0x0020u   /* line open on right */
#define sflag_i       0x0040u   /* intermittent (dashed) */
#define sflag_e       0x0080u   /* editorial */
#define sflag_x       0x0100u   /* crossing */
#define sflag_abs     0x0200u   /* absolutely vertically positioned */
#define sflag_lay     0x0400u   /* at {und,ov}erlay level */
#define sflag_idot    0x0800u   /* intermittent (dotted) */
#define sflag_cx      0x1000u   /* interpret end 'c' options as in MusicXML */

/* Hairpin flags */

#define hp_below     0x0001u
#define hp_middle    0x0002u
#define hp_halfway   0x0004u
#define hp_abs       0x0008u
#define hp_bar       0x0010u
#define hp_end       0x4000u
#define hp_cresc     0x8000u

/* Control flags on a note. */

#define nf_accinvis    0x00000001u  /* Don't show this accidental */
#define nf_accleft     0x00000002u  /* Explicit accidental move was given */
#define nf_accrbra     0x00000004u  /* Round bracket for accidental */
#define nf_accsbra     0x00000008u  /* Square bracket for accidental */
#define nf_appogg      0x00000010u  /* Print slash through stem */
#define nf_centre      0x00000020u  /* Centre this (rest or note) in bar */
#define nf_chord       0x00000040u  /* This note is part of a chord */
#define nf_coupleD     0x00000080u  /* Down coupled note */
#define nf_coupleU     0x00000100u  /* Up coupled note (to control ledgers) */
#define nf_cuedotalign 0x00000200u  /* Align cue dots with normal dots */
#define nf_cuesize     0x00000400u  /* Note must print at cue size */
#define nf_dot         0x00000800u  /* One augmentation dot */
#define nf_dot2        0x00001000u  /* Two augmentation dots */
#define nf_dotright    0x00002000u  /* Dot/plus moved right because of invert */
#define nf_fuq         0x00004000u  /* This note is a free upstemmed quaver */
#define nf_headbra     0x00008000u  /* Round bracket around notehead */
#define nf_hidden      0x00010000u  /* Invisible note */
#define nf_highdot     0x00020000u  /* Move space dot up to next space */
#define nf_invert      0x00040000u  /* Print this note on other side of stem */
#define nf_lowdot      0x00080000u  /* Print dots/plus below line */
#define nf_nopack      0x00100000u  /* Do not pack this rest bar */
#define nf_noplay      0x00200000u  /* Do not play - note tied */
#define nf_plus        0x00400000u  /* Augmentation plus */
#define nf_restrep     0x00800000u  /* Rest displayed as repetition sign */
#define nf_shortened   0x01000000u  /* This note's stem has been automatically shortened */
#define nf_stem        0x02000000u  /* This note has a stem */
#define nf_stemup      0x04000000u  /* Stem direction */
#define nf_tripletize  0x08000000u  /* Check note for tripletizing */
#define nf_stemcent    0x10000000u  /* Place stem central to note */
#define nf_wastied     0x20000000u  /* Tie was output (for XML output) */

#define nf_couple      (nf_coupleU+nf_coupleD)
#define nf_dotted      (nf_dot+nf_dot2+nf_plus)

/* These flags are for a copy of the dot & plus settings that are needed for 
correctly handling masquerading in MusicXML output. They are set in an 8-bit 
field. */

#define od_dot          0x01u
#define od_dot2         0x02u
#define od_plus         0x04u

/* Flags for the tie item (currently 8-bits) */

#define tief_default    0x01u
#define tief_slur       0x02u
#define tief_gliss      0x04u
#define tief_editorial  0x08u
#define tief_dashed     0x10u
#define tief_dotted     0x20u
#define tief_savedash   0x40u

/* Notehead type options. If these are changed, the "headchars" table in
setnote.c must also be changed. */

enum { nh_normal,           /* conventional noteheads */
       nh_cross,            /* X noteheads */
       nh_harmonic,         /* diamond-shaped */
       nh_none,             /* no noteheads */
       nh_direct,           /* fancy 'w' */
       nh_circular,         /* special circular shape */
       nh_number            /* number of special noteheads values */
};

/* The mask is for the bottom bits of a note's noteheadstyle field, which
contain one of the values above, leaving the rest for flags. */

#define nh_mask 0x07u
#define nhf_smallhead   0x80u         /* Small notehead */

/* Flags for accents on a note. Ornaments are held separately, as individual
numbers less than 256, but both accent and ornament encodings are listed in a
single table. This table relies on accent bit values being greater than 255; in
other words, don't use the bottom eight bits here without reworking things.
The accents must be in the order of user-visible accent numbers, so that the
the accent number can be calculated from the flag bit by left shifting. */

#define af_opposite    0x80000000u    /* Print accents on opposite side */
#define af_staccato    0x40000000u    /* 1 Staccato */
#define af_bar         0x20000000u    /* 2 Bar accent */
#define af_gt          0x10000000u    /* 3 Greater Than (>) */
#define af_wedge       0x08000000u    /* 4 Vertical wedge */
#define af_tp          0x04000000u    /* 5 Teepee (large circumflex) */
#define af_down        0x02000000u    /* 6 String down bow */
#define af_up          0x01000000u    /* 7 String up bow */
#define af_ring        0x00800000u    /* 8 Ring */
#define af_vline       0x00400000u    /* 9 Short vertical line ("start bar") */
#define af_staccatiss  0x00200000u    /* 10 Staccatissimo (teardrop) */

/* We also need symbolic definitions of the accent numbers, the last value
being the size of the out_accentmove structure. */

enum { accent_none, accent_staccato, accent_bar, accent_gt, accent_wedge,
       accent_tp, accent_down, accent_up, accent_ring, accent_vline,
       accent_staccatiss, ACCENT_COUNT };

#define af_accents    (af_staccato | af_bar | af_gt | af_wedge | \
                       af_tp | af_down | af_up | af_ring | af_vline | \
                       af_staccatiss)

#define af_accinside  (af_staccato | af_staccatiss | af_bar | af_ring)

#define af_accoutside (af_gt | af_wedge | af_tp | af_down | af_up | af_vline)

/* Ornament types. These are for rarer things. Accidentals above/below notes
are handled as ornaments. These values must all be less than 256, and if ever
there are more than 32 real ornaments, recoding in read_note() will be
necessary (flag bits are used to handle duplication). 

NOTE: Any extension or re-arrangement of this list must be matched by 
corresponding adjustments to the tables in setnote.c whose names begin with 
"ornament_". */

enum {
  or_unset,       /* No ornament */

  /* In XML output, these come under <ornament> under <notations>. */

  or_tr,          /* Trill */
  or_trsh,        /* Trill + sharp */
  or_trfl,        /* Trill + flat */
  or_trnat,       /* Trill + natural */
  or_trem1,       /* One tremolo line */
  or_trem2,       /* Two tremolo lines */
  or_trem3,       /* Three tremolo lines */
  or_mord,        /* Mordent */
  or_dmord,       /* Double mordent */
  or_imord,       /* Inverted mordent */
  or_dimord,      /* Double inverted mordent */
  or_turn,        /* Turn */
  or_iturn,       /* Inverted turn (vertical line) */
  or_rturn,       /* Reversed turn (flipped about vertical axix) */
  or_irturn,      /* Inverted reversed turn */  
  or_spread,      /* Spread */

  /* These come directly under <notations> in XML output. Fermata must be
  first. */

  or_ferm,        /* Fermata */
  or_arp,         /* Arpeggio */
  or_arpu,        /* Arpeggio + up arrow */
  or_arpd,        /* Arpeggio + down arrow */

  /* These triples must be in the standard accidental ordering, and must be
  last in this enumeration. Each triple defines an above/below accidental as an
  "ornament", either without adornment, or in round or square brackets. */

  or_nat,    or_natrb,    or_natsb,    /* Natural */
  or_hsharp, or_hsharprb, or_hsharpsb, /* Half sharp */
  or_sharp,  or_sharprb,  or_sharpsb,  /* Sharp */
  or_dsharp, or_dsharprb, or_dsharpsb, /* Double sharp */
  or_hflat,  or_hflatrb,  or_hflatsb,  /* Half flat */
  or_flat,   or_flatrb,   or_flatsb,   /* Flat */
  or_dflat,  or_dflatrb,  or_dflatsb,  /* Double flat */

  or_accbelow    /* The above are repeated from here, but don't need */
                 /* individual names. */
};

/* Ornament bracketing and other flags */

#define orn_rbra    0x01u
#define orn_rket    0x02u
#define orn_sbra    0x04u
#define orn_sket    0x08u
#define orn_invis   0x10u

/* Boolean bit flags in movement structures */

#define mf_beamendrests      0x00000001u
#define mf_breverests        0x00000002u
#define mf_check             0x00000004u
#define mf_checkdoublebars   0x00000008u
#define mf_codemultirests    0x00000010u
#define mf_copiedfontsizes   0x00000020u
#define mf_fullbarend        0x00000040u
#define mf_keydoublebar      0x00000080u
#define mf_keywarn           0x00000100u
#define mf_midistart         0x00000200u
#define mf_newpage           0x00000400u
#define mf_nopageheading     0x00000800u
#define mf_rehearsallsleft   0x00001000u
#define mf_repeatwings       0x00002000u
#define mf_showtime          0x00004000u
#define mf_showtimebase      0x00008000u
#define mf_spreadunderlay    0x00010000u
#define mf_startjoin         0x00020000u
#define mf_startnotime       0x00040000u
#define mf_thisline          0x00080000u
#define mf_thispage          0x00100000u
#define mf_tiesoverwarnings  0x00200000u
#define mf_timewarn          0x00400000u
#define mf_underlayextenders 0x00800000u
#define mf_unfinished        0x01000000u
#define mf_uselastfooting    0x02000000u

/* These flags are set at the start of the first movement. */

#define mf_startflags (mf_check|mf_checkdoublebars|mf_copiedfontsizes|\
  mf_fullbarend|mf_keydoublebar|mf_keywarn|mf_showtime|mf_showtimebase|\
  mf_timewarn|mf_spreadunderlay|mf_underlayextenders)

/* These flags specify the type of new movement, and are set by options on the
[newmovement] directive. */

#define mf_typeflags (mf_newpage|mf_thisline|mf_thispage)

/* These flags are unset at the start of a new movement, before setting any
flags from the [newmovement] directive. */

#define mf_unsetflags (mf_typeflags|mf_copiedfontsizes|mf_nopageheading| \
  mf_midistart|mf_startjoin|mf_startnotime|mf_unfinished|mf_uselastfooting)

/* These flags are set at the start of a new movement. */

#define mf_resetflags (mf_showtime)

/* Flags for enabling/disabling MIDI tremolo action. */

#define mtf_repeat    0x00000001u
#define mtf_trill     0x00000002u
#define mtf_both      (mtf_repeat|mtf_trill)

/* Backward offsets for non-note items in a bar. */

#define posx_acc         (-1)    /* accidental */
#define posx_gracelast   (-2)    /* grace notes use 2 to 15, in reverse order */
#define posx_gracefirst  (-15)
#define posx_RLright     (-16)   /* left repeat when printed on the right */
#define posx_timelast    (-17)   /* time signatures use 17 to 20, in reverse */
#define posx_timefirst   (-20)
#define posx_keylast     (-21)   /* key signatures use 21 to 24, in reverse */
#define posx_keyfirst    (-24)
#define posx_RLleft      (-25)   /* left repeat when printed on the left */
#define posx_RR          (-26)
#define posx_dotbar      (-27)
#define posx_clef        (-28)
#define posx_tick        (-29)
#define posx_comma       (-30)
#define posx_caesura     (-31)
#define posx_max         (-31)

#define posx_maxgrace    (posx_gracelast - posx_gracefirst + 1)
#define posx_maxtime     (posx_timelast - posx_timefirst + 1)
#define posx_maxkey      (posx_keylast - posx_keyfirst + 1)

/* Barnumber forcing settings */

enum { bnf_auto, bnf_yes, bnf_no };

/* Cap and join types for lines. */

#define caj_butt           0x00000000u  /* Neither round nor square */
#define caj_round          0x00000001u
#define caj_square         0x00000002u

#define caj_mitre_join     0x00000000u  /* Neither round nor bevel */
#define caj_round_join     0x00000004u
#define caj_bevel_join     0x00000008u

/* Path types */

enum { path_end, path_move, path_curve, path_line };

/* System joining types */

enum { join_barline, join_brace, join_bracket, join_thinbracket };

/* Types of barline */

enum { barline_normal, barline_double, barline_ending, barline_invisible };

/* Bar line types are the relevant characters in the music font */

#define bar_single '@'
#define bar_double 'A'
#define bar_thick  'B'
#define bar_dotted '['

/* Types for repeat marks are indices into a table */

#define rep_right   0
#define rep_left   15
#define rep_dright 30
#define rep_dleft  45

/* Characters in a virtual musical font. These are used to index the table
out_mftable_ps, which is defined in tables.c and must be kept in step. The
order is arbitrary. */

enum {
  mc_trebleclef,       mc_trebleTclef,    mc_trebleDclef,
  mc_bassclef,         mc_Cbassclef,      mc_Sbassclef,
  mc_cbaritoneclef,    mc_tenorclef,      mc_altoclef,
  mc_mezzoclef,        mc_sopranoclef,    mc_natural,
  mc_sharp,            mc_dsharp,         mc_flat,
  mc_dflat,            mc_rnatural,       mc_rsharp,
  mc_rdsharp,          mc_rflat,          mc_rdflat,
  mc_snatural,         mc_ssharp,         mc_sdsharp,
  mc_sflat,            mc_sdflat,         mc_common,
  mc_cut,              mc_longrest,       mc_trebleTBclef,
  mc_hclef,            mc_baritoneclef,   mc_deepbassclef,
  mc_oldbassclef,      mc_oldCbassclef,   mc_oldSbassclef,
  mc_oldcbaritoneclef, mc_oldtenorclef,   mc_oldaltoclef,
  mc_oldmezzoclef,     mc_oldsopranoclef, mc_oldbaritoneclef,
  mc_olddeepbassclef,  mc_hsharp1,        mc_hrsharp1,
  mc_hssharp1,         mc_hsharp2,        mc_hrsharp2,
  mc_hssharp2,         mc_hflat1,         mc_hrflat1,
  mc_hsflat1,          mc_hflat2,         mc_hrflat2,
  mc_hsflat2
};

/* Error type codes */

#define ec_warning   0
#define ec_minor     1   /* Soft error - can continue and produce output */
#define ec_major     2   /* Soft error - can continue, but no output */
#define ec_failed    3   /* Hard error - cannot continue */

/* Include definitions of structures, global variables, and functions. */

#include "structs.h"
#include "globals.h"
#include "functions.h"

/* Optional MusicXML support */

#if SUPPORT_XML
#include "xml.h"

/* MusicXML output options */

#define mx_numberlyrics  0x00000001u

#define MX(x) ((main_xmloptions & (x)) != 0)

#endif

/* End of pmw.h */
