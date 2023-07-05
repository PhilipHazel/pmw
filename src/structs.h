/*************************************************
*           PMW structure definitions            *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: December 2020 */
/* This file last modified: July 2023 */

/* These structures must be defined before the stave data items. */

/* Structure for each node in a tree */

typedef struct tree_node {
  struct  tree_node *left;       /* pointer to left child */
  struct  tree_node *right;      /* pointer to right child */
  uschar *name;                  /* node name */
  void   *data;                  /* for pointer values */
  int32_t value;                 /* for numeric values */
  uint8_t balance;               /* balancing factor */
} tree_node;


/* Structure for items in encoded draw function vectors, arguments, variables,
and stack. A union is needed for the value because some are integers and some
are pointers.  */

typedef struct {
  usint dtype;             /* data type, eg dd_number */
  union {
    void *ptr;
    int32_t val;
  } d;
} drawitem;


/* Structure containing size and other information for a font instance, for use
when outputting a string. The trig function values are cached from the rotation
in the matrix to save repeated computation. */

typedef struct fontinststr {
  int32_t *matrix;          /* Transformation matrix or NULL */
  int32_t size;             /* Point size */
  int32_t spacestretch;     /* Extra width to add to spaces */
} fontinststr;


/* One heading/footing/footnote. There are two different cases: a text string
or a drawing. They use different subsets of the fields in this structure. The
drawing field is set NULL when it's a text string. */

typedef struct headstr {
  struct headstr *next;
  int32_t space;            /* Space to follow */
  /* --- Text string --- */
  uint32_t *string[3];      /* Left/middle/right PMW strings */
  fontinststr fdata;        /* Font instance data (size, matrix, etc.) */
  int32_t spaceabove;       /* Space above line of text (used for footnotes) */
  /* --- Drawing --- */
  tree_node *drawing;       /* Pointer to the drawing */
  drawitem *drawargs;       /* Drawing arguments */
} headstr;



/* ----------------------------------------------------------------------- */
/*                     Structures for items in stave data                  */
/* ----------------------------------------------------------------------- */

/* The first three fields are the same in each of these structures so that they
can be chained together in a generic form. */

#define BSTRHEAD \
  struct bstr   *next; \
  struct bstr   *prev; \
  uint8_t        type

/* This is the "generic" structure. Note that this ends with an 8-byte field
after the two pointers. To optimize the sizes of those that follow, up to 3
8-bit fields, or an 8.bit and a 16-bit field, (if required) are placed first,
before any subsequent pointers and longer fields.  */

typedef struct bstr {
  BSTRHEAD;
} bstr;

/* The structure at the start of each bar in each stave. The type gets set to
b_start. */

typedef struct barstr {
  BSTRHEAD;
  uint16_t repeatnumber;
} barstr;

/* Standard struct for items with one 8-bit arg */

typedef struct {
  BSTRHEAD;
  uint8_t value;
} b_bytevaluestr;

/* Standard struct for items with a single dimension arg */

typedef struct {
  BSTRHEAD;
  int32_t value;
} b_intvaluestr;

/* The remainder of the bar structures are musical items that have some data.
Thoe that have no data do not have their own structures; they just use a
generic bstr. Of the rest, some are the same as others, but by giving each its
own name it is easier to alter them when necessary. */

typedef struct {
  BSTRHEAD;
  uint8_t accent;
  uint8_t bflags;
  int32_t x;
  int32_t y;
} b_accentmovestr;

typedef struct {
  BSTRHEAD;
  uint8_t bartype;
  uint8_t barstyle;
} b_barlinestr;

typedef struct {
  BSTRHEAD;
  CBOOL flag;
  int32_t x;
  int32_t y;
} b_barnumstr;

typedef b_bytevaluestr b_beamaccritstr;
typedef b_bytevaluestr b_beambreakstr;
typedef b_intvaluestr  b_beammovestr;
typedef b_intvaluestr  b_beamslopestr;
typedef b_bytevaluestr b_bowingstr;

typedef struct {
  BSTRHEAD;
  uint8_t clef;
  CBOOL assume;
  CBOOL suppress;
} b_clefstr;

typedef b_intvaluestr  b_dotrightstr;

typedef struct {
  BSTRHEAD;
  CBOOL overflag;
  tree_node *drawing;
  drawitem *drawargs;
} b_drawstr;

typedef b_bytevaluestr b_endslurstr;
typedef b_intvaluestr  b_ensurestr;

typedef struct {
  BSTRHEAD;
  headstr h;
} b_footnotestr;

typedef struct {
  BSTRHEAD;
 uint16_t flags;
  int32_t x;
  int32_t y;
  int32_t halfway;   /* "Halfway" fraction */
  int32_t offset;    /* "Offset" fraction */
  int32_t su;        /* Split end up/down */
  int32_t width;     /* Width of open end */
} b_hairpinstr;

typedef b_bytevaluestr b_justifystr;

typedef struct {
  BSTRHEAD;
  CBOOL assume;
  CBOOL suppress;
  CBOOL warn;
  uint32_t key;
} b_keystr;

typedef struct {
  BSTRHEAD;
  CBOOL relative;
  int32_t x;
  int32_t y;
} b_movestr;

typedef b_bytevaluestr b_namestr;

typedef struct {
  BSTRHEAD;
  uint8_t   n;
  uint8_t   ssize;
  uint32_t *s;
   int32_t  x;
   int32_t  y;
} b_nbarstr;

typedef b_bytevaluestr b_notesstr;     /* sic (notes on/off) */

/* Single notes and notes in a chord use the same structure; they are
distinguished by the type field */

typedef struct {
  BSTRHEAD;
  uint8_t  notetype;
  uint8_t  masq;
  uint8_t  acc;
   int32_t accleft;
  uint32_t acflags;
  uint32_t flags;
  uint32_t length;
   int32_t yextra;
  uint16_t abspitch;
  uint16_t spitch;
  uint8_t  acc_orig;
  uint8_t  char_orig;
} b_notestr;

typedef b_bytevaluestr b_noteheadsstr;
typedef b_intvaluestr  b_nsmstr;

typedef struct {
  BSTRHEAD;
  int32_t ns[NOTETYPE_COUNT];
} b_nsstr;

typedef struct {
  BSTRHEAD;
  uint8_t ornament;
  uint8_t bflags;
  int32_t x;
  int32_t y;
} b_ornamentstr;

typedef b_intvaluestr  b_pagebotsstr;
typedef b_intvaluestr  b_pagetopsstr;

typedef struct b_midichangestr {
  BSTRHEAD;
  int16_t transpose;
  uint8_t channel;
  uint8_t note;
  uint8_t voice;
  uint8_t volume;
} b_midichangestr;

typedef struct {
  BSTRHEAD;
  barstr *nextbar;
} b_overbeamstr;

typedef struct {
  BSTRHEAD;
  CBOOL   relative;
  int32_t value;
} b_pagestr;

typedef struct {
  BSTRHEAD;
  uint8_t pletlen;
  uint8_t flags;
  int32_t x;
  int32_t yleft;
  int32_t yright;
} b_pletstr;

typedef struct {
  BSTRHEAD;
  int32_t moff;
} b_resetstr;    

typedef struct {
  BSTRHEAD;
  CBOOL relative;
  int32_t value;
} b_sgstr;

/* Structure for holding a set of slur modifications, which might repeat
for different parts of a split slur. */

typedef struct b_slurmodstr {
  struct b_slurmodstr *next;
  uint16_t sequence;
   int32_t lxoffset;
   int32_t lx;
   int32_t ly;
   int32_t rxoffset;
   int32_t rx;
   int32_t ry;
   int32_t c;
   int32_t clx;
   int32_t cly;
   int32_t crx;
   int32_t cry;
} b_slurmodstr;

/* The basic slur structure, also used for lines. */

typedef struct {
  BSTRHEAD;
  uint8_t  id;
  uint16_t flags;
  b_slurmodstr *mods;
  int32_t  ally;       /* Adjustment applying to all sections */
} b_slurstr;

/* Slur gap structure, also used for lines. */

typedef struct {
  BSTRHEAD;
  uint8_t     id;
  tree_node  *drawing;
  drawitem   *drawargs;
  uint32_t   *gaptext;
   int32_t    hfraction;
   int32_t    xadjust;
   int32_t    width;
   int32_t    textx;
   int32_t    texty;
  uint16_t    textflags;
  uint16_t    textsize;
} b_slurgapstr;

typedef struct {
  BSTRHEAD;
  CBOOL relative;
  int32_t x;
} b_spacestr;

typedef struct {
  BSTRHEAD;
  CBOOL   relative;
  uint8_t stave;
  int32_t value;
} b_ssstr;

typedef struct {
  BSTRHEAD;
  uint16_t  flags;
  uint32_t *string;
   int32_t  x;
   int32_t  y;
   int32_t  rotate;
   int32_t  halfway;   /* "Halfway" fraction */
   int32_t  offset;    /* "Offset fraction */
  uint8_t   size;
  uint8_t   htype;
  uint8_t   laylevel;  /* Underlay or overlay level */
  uint8_t   laylen;    /* Underlay or overlay syllable length */
} b_textstr;

typedef struct {
  BSTRHEAD;
  uint8_t flags;
  uint8_t abovecount;
  uint8_t belowcount;
  b_notestr *noteprev;
} b_tiestr;

typedef struct {
  BSTRHEAD;
  CBOOL assume;
  CBOOL suppress;
  CBOOL warn;
  uint32_t time;
} b_timestr;

typedef struct {
  BSTRHEAD;
  uint8_t count;
  uint8_t join;
} b_tremolostr;

typedef b_bytevaluestr b_tripswstr;
typedef b_intvaluestr  b_uolherestr;
typedef b_intvaluestr  b_uolevelstr;
typedef b_intvaluestr  b_zerocopystr;




/* ----------------------------------------------------------------------- */

/* Structure for tables of directives */

typedef struct dirstr {
  const char *name;
  void (*proc)(void);
  int  arg1;
  int  arg2;
} dirstr;

/* Structure for remembering stacked files during include */

typedef struct filestackstr {
  FILE *file;
  uschar *filename;
  int   linenumber;
  int   okdepth;
} filestackstr;

/* Items in a kerning table, pointed to from a fontstr */

typedef struct kerntablestr {
  uint32_t pair;
   int32_t kwidth;
} kerntablestr;


/* Items in Unicode translation table, pointed to from a fontstr */

typedef struct utrtablestr {
  uint32_t unicode;
  uint32_t pscode;
} utrtablestr;


/* Font data for a specific typeface */

typedef struct fontstr {
  uschar *name;             /* Font name */
  int32_t *widths;          /* Pointer to basic width table */
  int32_t *r2ladjusts;      /* Pointer to right-to-left adjusts */
  int32_t *heights;         /* Pointer to height table, if any */
  utrtablestr *utr;         /* Unicode translation for nonstd font */
  uschar **encoding;        /* Optional encoding for non-stdenc fonts */
  tree_node *high_tree;     /* Tree for data for high val stdenc chars */
  kerntablestr *kerns;      /* Pointer to kerning table */
  int32_t kerncount;        /* Size of kern table */
  int32_t utrcount;         /* Size of utr table */
  uint32_t flags;           /* Various bit flags */
  uint32_t invalid;         /* What to use for unsupported character */
} fontstr;


/* Structure for font instance data, which contains sizes at standard
magnification, along with matrices for font stretching and shearing. Sizes for
certain kinds of text (underlay, overlay, figured bass, stave init) follow the
user-defined sizes in fontsize_text[], and are accessed via ff_offset_ulay,
etc. */

typedef struct fontsizestr {
  fontinststr fontsize_music;
  fontinststr fontsize_grace;
  fontinststr fontsize_cue;
  fontinststr fontsize_cuegrace;
  fontinststr fontsize_midclefs;
  fontinststr fontsize_barnumber;
  fontinststr fontsize_footnote;
  fontinststr fontsize_rehearse;
  fontinststr fontsize_triplet;
  fontinststr fontsize_repno;                 /* repeat bar number */
  fontinststr fontsize_restct;                /* long rest count */
  fontinststr fontsize_trill;
  fontinststr fontsize_vertacc;               /* vertical accidentals */
  fontinststr fontsize_text[TEXTSIZES_SIZE];  /* vector of text sizes */
} fontsizestr;


/* Structure for describing a text item used in drawing functions */

typedef struct {
  uint32_t *text;
   int32_t  rotate;
   int32_t  xdelta;
   int32_t  ydelta;
  uint16_t  flags;
  uint16_t  size;
} drawtextstr;


/* Layout of data structure used for saving drawing operations until the
end of a stave - to ensure they are over everything else. Use a union
to handle the two different types. */

struct overdrawstr_graphic {
  int32_t linewidth;
  int32_t gray;
  int32_t ystave;
  int32_t *x;
  int32_t *y;
  int     *c;
};

struct overdrawstr_text {
  uint32_t *text;
  fontinststr fdata;
  uint32_t boxring;
  int32_t xx;
  int32_t yy;
  int32_t matrix[6];
};

typedef struct overdrawstr {
  struct overdrawstr *next;
  BOOL texttype;
  union {
    struct overdrawstr_graphic g;
    struct overdrawstr_text t;
  } d;
} overdrawstr;


/* Structure for list of accent/ornament codings */

typedef struct accent {
  uschar  *string;
  uint32_t flag;
  uint8_t  len;
} accent;


/* One KeyTranspose item */

typedef struct keytransstr {
  struct keytransstr *next;
  uint32_t oldkey;
  uint32_t newkeys[24];
   int32_t letterchanges[24];
} keytransstr;


/* Structure for handling macros. We can't unfortunately have a variable length
vector at the end, as C doesn't support such things. */

typedef struct macrostr {
  int  argcount;       /* number of default arguments */
  uschar *text;        /* replacement text */
  uschar *args[1];     /* vector of pointers */
} macrostr;


/* One item in a chain of stave selections. Must be defined before movtstr. */

typedef struct stavelist {
  struct stavelist *next;
  struct stavelist *prev;
  uint32_t first;
  uint32_t last;
} stavelist;


/* Structure for keeping data about a copy of stave 0 on another stave. Must be
defined before movtstr. */

struct contstr;                /* Defined further down */

typedef struct zerocopystr {
  struct zerocopystr *next;
  struct contstr *cont;       /* Holds cont ptr for this copy */
  int32_t  stavenumber;       /* Stave to overprint */
  int32_t  adjust;            /* Overall adjustment for this stave */
  int32_t  baradjust;         /* Adjustment for this bar */
  int32_t  level;             /* Used to hold actual level for printing */
} zerocopystr;


/* Structure for remembering data about tie accidentals, which consists of
small positive or negative integers. */

typedef struct {
  uint16_t pitch;
  int8_t  acc;
  int8_t  acc_tp;
} tiedata;


/* Structure for holding data that is used while reading a stave, but not
retained afterwards. Putting it in a structure means it can mostly be
initialized at the start of a stave by a simple copy, though a few values have
to be taken from the current movement. Keep in step with initializing values in
tables.c. */

typedef struct sreadstr {
  /* Fields that are overridden by data from the movement or otherwise. */

  /* 32_bit fields */
   int32_t   hairpinwidth;
  uint32_t   key;
  uint32_t   key_tp;
  uint32_t   required_barlength;
  uint32_t   stavenumber;

  /* 8-bit fields */
  uint8_t    barlinestyle;
  CBOOL      suspended;

  /*  ---- Fields that are not overridden ---- */

  b_notestr *beamfirstnote;
  b_notestr *firstnoteptr;
  b_notestr *lastnoteptr;
  b_ornamentstr *lastnote_ornament;
  b_textstr *pendolay;
  b_textstr *pendulay;

  /* 32-bit fields */
  uint32_t   accentflags;
   int32_t   clef_octave;
   int32_t   hairpinsru;
   int32_t   hairpiny;
  uint32_t   matchden;   /* For matching to an incompatible time signature */
  uint32_t   matchnum;   /* For matching to an incompatible time signature */
  uint32_t   noteden;    /* For doubling or halving note lengths */
  uint32_t   noteflags;
  uint32_t   notenum;    /* For doubling or halving note lengths */
   int32_t   octave;
  uint32_t   pitchcount;
  uint32_t   pitchtotal;
   int32_t   plety;
   int32_t   rlevel;
   int32_t   stemlength;
   int32_t   textabsolute;
  uint32_t   textflags;

  /* 16-bit fields */
  uint16_t   beamcount;
  uint16_t   clef;
   int16_t   couplestate;
  uint16_t   fbfont;
  uint16_t   fbsize;
   int16_t   hairpinbegun;
   int16_t   hairpinflags;
   int16_t   maxaway;
  uint16_t   maxpitch;
  uint16_t   minpitch;
  uint16_t   olfont;
  uint16_t   olsize;
  uint16_t   printpitch;
  uint16_t   slurcount;
  uint16_t   textfont;
  uint16_t   textsize;
  uint16_t   ulfont;
  uint16_t   ulsize;

  /* 8-bit fields */
  uint8_t    accrit;
   int8_t    beamstemforce;
  uint8_t    chordcount;    /* Number of notes in last note/chord */
  uint8_t    ornament;
  uint8_t    pletflags;
   int8_t    stemsdirection;
   int8_t    tiesplacement;
  uint16_t   lasttiepitch;
  CBOOL      beaming;
  CBOOL      noteson;
  CBOOL      laststemup;
  CBOOL      lastwasdouble;
  CBOOL      lastwasempty;
  CBOOL      lastwastied;
  CBOOL      string_followOK;
} sreadstr;


/* Structure for holding data that is used while reading a bar, but not
retained afterwards. Putting it in a structure means it can be initialized by a
simple copy. Keep in step with initializing values in tables.c. */

typedef struct breadstr {
  barstr    *bar;
  b_ensurestr *lasttieensure;
  bstr      *repeatstart;

  /* 32-bit fields */
  uint32_t   barlength;
  uint32_t   maxbarlength;
  uint32_t   pletlen;
  uint32_t   pletnum;
  uint32_t   pletden;
   int32_t   skip;
   int32_t   smove;

  /* 16-bit fields */
  uint16_t   beamstackptr;
  uint16_t   stemstackptr;

  /* 8-bit fields */
   int8_t    lastgracestem;
  CBOOL      checklength;
  CBOOL      checktripletize;
  CBOOL      firstnoteinbar;
  CBOOL      firstnontied;
  CBOOL      firstnontiedset;
  CBOOL      nocount;
  CBOOL      resetOK;
  CBOOL      smove_isrelative;
} breadstr;

/* Structure for a stave name text. Keep in step with initializing values in
tables.c. */

typedef struct snamestr {
  struct snamestr *next;
  struct snamestr *extra;   /* additional string(s); maybe different options */
  uint32_t  *text;
  tree_node *drawing;
  drawitem  *drawargs;
  int32_t    adjustx;
  int32_t    adjusty;
  uint8_t    flags;
  uint8_t    size;
  uint8_t    linecount;
} snamestr;

/* Structure for items in the final position table for a bar */

typedef struct {
  int32_t  moff;     /* musical offset of the position */
  int32_t  xoff;     /* horizontal offset */
} posstr;

/* Layout of one item in a bar's positioning table while it is being worked on.
The stemup & stemdown flags are used as stated only at the beginning of
processing, when non-grace notes are the only things considered. For auxiliary
items, the stemup field is re-used to track which staves have the auxiliary
item, under the name auxstaves. */

#define auxstaves stemup

typedef struct {
  uint64_t  stemup;     /* bitmap of staves where there is an actual up stem */
  uint64_t  stemdown;   /* ditto for stem down */
   int32_t  moff;
   int32_t  xoff;
   int32_t  space;      /* additional space required */
   int      auxid;      /* identification of auxiliary items (may be -ve) */
 } workposstr;

/* Structure for entries in the vector kept for a movement's bars while
paginating. */

typedef struct {
  posstr *vector;      /* points to positioning vector */
  int32_t barnoX;      /* X offset for bar number */
  int32_t barnoY;      /* Y offset for bar number */
  int16_t count;       /* number of entries in the vector */
  int16_t multi;       /* multi-bar rest count */
  uint8_t posxRL;      /* order adjustment for left repeat */
  uint8_t barnoforce;  /* default/force/unforce */
} barposstr;

/* Structure for remembering things while measuring a system. */

typedef struct {
  snamestr **stavenames;       /* stave name vector */
  uint64_t   notsuspend;       /* not suspended staves */
   int32_t   xposition;        /* total width of bars */
   int32_t   startxposition;   /* position of lhs, after stave names */
   int32_t   endbar;           /* end bar number */
  uint32_t   count;            /* count of bars */
   int32_t   note_spacing[NOTETYPE_COUNT];
  CBOOL      endkey;           /* line ends with key signature */
  CBOOL      endtime;          /* line ends with time signature */
} pagedatastr;

/* Retained data pertaining to one stave. Must be defined before movtstr. Keep
in step with initializing values in tables.c. */

typedef struct stavestr {
  snamestr *stave_name;
  barstr  **barindex;
  size_t    barindex_size;
  int32_t   barcount;
  uint32_t  totalpitch;
  uint32_t  notecount;
  uint16_t  toppitch;
  uint16_t  botpitch;
  uint8_t   stavelines;
  CBOOL     omitempty;
  CBOOL     halfaccs;
} stavestr;

/* Data pertaining to a movement. Keep in step with initializing values in
tables.c. */

typedef struct movtstr {

  /* Pointers */

   int32_t     *accadjusts;
  uint32_t     *accspacing;
  uint32_t     *barvector;
  stavelist    *bracelist;
  stavelist    *bracketlist;
  fontsizestr  *fontsizes;
  headstr      *footing;
  headstr      *heading;
  uint32_t     *hyphenstring;
  stavelist    *joinlist;
  stavelist    *joindottedlist;
  headstr      *lastfooting;
  uint16_t     *layout;
  uint8_t      *midistart;
  uint32_t     *miditempochanges;
  headstr      *pagefooting;
  headstr      *pageheading;
  barposstr    *posvector;
   int32_t     *stavesizes;
  stavestr     *stavetable[MAX_STAVE+1];
  stavelist    *thinbracketlist;
  uint32_t     *trillstring;
  zerocopystr  *zerocopy;

  /* 64-bit fields */

  uint64_t      breakbarlines;
  uint64_t      select_staves;
  uint64_t      suspend_staves;

  /* Size-t fields */

  size_t        barvector_size;

  /* 32-bit fields */

   int32_t      barcount;
   int32_t      barlinesize;
   int32_t      barlinespace;
  uint32_t      barnocount;
   int32_t      barnumber_interval;
   int32_t      barnumber_level;
  uint32_t      barnumber_textflags;
  uint32_t      baroffset;
  uint32_t      beamflaglength;
  uint32_t      beamthickness;
  uint32_t      bottommargin;
  uint32_t      breveledgerextra;
  uint32_t      clefwidths[CLEF_COUNT];
   int32_t      dotspacefactor;
   int32_t      endlinesluradjust;
   int32_t      endlinetieadjust;
   int32_t      extenderlevel;
  uint32_t      flags;
   int32_t      gracespacing[2];
   int32_t      footnotesep;
  uint32_t      hairpinlinewidth;
  uint32_t      hairpinwidth;
   int32_t      hyphenthreshold;
  uint32_t      key;
   int32_t      leftmargin;
   int32_t      linelength;
   int32_t      maxbeamslope[2];
  uint32_t      midichanset;
  uint32_t      miditempo;
   int32_t      midkeyspacing;
   int32_t      midtimespacing;
  uint32_t      noteden;
  uint32_t      notenum;
   int32_t      note_spacing[NOTETYPE_COUNT];
  uint32_t      number;
   int32_t      overlaydepth;
  uint32_t      rehearsalstyle;
   int32_t      shortenstems;
  uint32_t      smallcapsize;
   int32_t      startbracketbar;
   int32_t      startspace[4];   /* Space before clef, key, time, first note */
   int32_t      stave_ensure[MAX_STAVE+1];
   int32_t      stave_spacing[MAX_STAVE+1];
   int32_t      stemadjusts[NOTETYPE_COUNT];
  uint32_t      systemgap;
   int32_t      systemsepangle;
  uint32_t      systemseplength;
  uint32_t      systemsepwidth;
   int32_t      systemsepposx;
   int32_t      systemsepposy;
  uint32_t      time;
  uint32_t      time_unscaled;
  uint32_t      topmargin;
   int32_t      transpose;
  uint32_t      tripletlinewidth;
   int32_t      underlaydepth;

  /* 8-bit fields */

  uint8_t       barlinestyle;
  uint8_t       bracestyle;
  uint8_t       caesurastyle;
  uint8_t       clefstyle;
  uint8_t       endlineslurstyle;
  uint8_t       endlinetiestyle;
  uint8_t       fonttype_barnumber;
  uint8_t       fonttype_longrest;
  uint8_t       fonttype_rehearse;
  uint8_t       fonttype_repeatbar;
  uint8_t       fonttype_time;
  uint8_t       fonttype_triplet;
  uint8_t       gracestyle;
  uint8_t       halfflatstyle;
  uint8_t       halfsharpstyle;
  uint8_t       justify;
   int8_t       laststave;
   int8_t       lastreadstave;
  uint8_t       ledgerstyle;
  uint8_t       midichannel[MAX_STAVE+1];
  uint8_t       midichannelvolume[MIDI_MAXCHANNEL];
  uint8_t       midinote[MAX_STAVE+1];
  uint8_t       midistavevolume[MAX_STAVE+1];
   int8_t       miditranspose[MAX_STAVE+1];
  uint8_t       midivoice[MIDI_MAXCHANNEL];
  uint8_t       repeatstyle;
   int8_t       stemswaplevel[MAX_STAVE+1];
  uint8_t       stemswaptype;
  uint8_t       underlaystyle;
} movtstr;

/* Data for an active line gap */

typedef struct gapstr {
  struct gapstr *next;
  int32_t x;
  b_slurgapstr *gap;
} gapstr;

/* Data for an active slur */

typedef struct slurstr {
  struct slurstr *next;
  b_slurstr *slur;
  gapstr *gaps;
  int32_t moff;
  int32_t x;
  int32_t y;
  int32_t maxy;
  int32_t miny;
  int32_t lastx;
  int32_t lasty;
  int32_t slopeleft;
  int32_t sloperight;
  int16_t count;
  int16_t section;
} slurstr;

/* Data for an active hairpin */

typedef struct hairpinstr {
  struct hairpinstr *next;     /* For hanging on a free chain */
  b_hairpinstr *hairpin;
  int32_t x;
  int32_t maxy;
  int32_t miny;
} hairpinstr;

/* Data for an active nth time bar. There can be a chain of these when, e.g. 1
and 2 are given for the same bar. */

typedef struct nbarstr {
  struct nbarstr *next;
  b_nbarstr *nbar;
  int32_t x;
  int32_t maxy;
  int32_t miny;
} nbarstr;

/* Data for a pending underlay or overlay hyphen string or extension. */

typedef struct uolaystr {
  struct uolaystr *next;
  int32_t x;                   /* start position */
  int32_t y;                   /* vertical level */
  uint8_t type;                /* '=' or '-' */
  uint8_t level;               /* underlay or overlay level */
  uint8_t htype;               /* hyphen type index */
  CBOOL   above;               /* TRUE for overlay */
} uolaystr;

/* Data saved when a beam extends over a bar line. Longestnote is used when the
split is over a line end. */

typedef struct obeamstr {
  struct obeamstr *next;       /* for caching free blocks */
  int32_t  firstX;
  int32_t  firstY;
  int32_t  slope;
  int32_t  count;
  int32_t  Xcorrection;
  int32_t  longestnote;
  CBOOL    splitOK;
  CBOOL    upflag;
}obeamstr;

/* Layout of one stave's continuation data held in a vector for use
at the start of each system, and while setting the stave. Multiple copies of
stave 0 require temporary blocks of this type. These use the caching mechanism
which will assume a pointer field at the start of the block. */

typedef struct constr {
  slurstr    *slurs;           /* chain of pending slurs */
  hairpinstr *hairpin;         /* pending hairpin */
  nbarstr    *nbar;            /* chain of pending nth time bars */
  obeamstr   *overbeam;        /* data for beam over bar line */
  b_tiestr   *tie;             /* pending tie */
  uolaystr   *uolay;           /* pending {und,ov}erlay hyphens or extension */
  int32_t     tiex;            /* start position for tie */
  uint32_t    time;            /* time signature */
  uint8_t     noteheadstyle;   /* sic */
  uint8_t     flags;           /* see below */
  uint8_t     clef;            /* current clef */
  uint8_t     key;             /* current key signature */
} contstr;

#define cf_bowingabove   1
#define cf_notes         2     /* on/off switch */
#define cf_triplets      4     /* on/off switch */
#define cf_noteheads     8     /* on/off switch for noteheads only */
#define cf_rdrepeat     16     /* last bar ended with double repeat */

#define cf_default (cf_bowingabove|cf_notes|cf_triplets)

/* Layout of data in a system block structure. Up to and including is_sysblock
it must have the same layout as a headblock, because they are held in the same
chain. */

typedef struct sysblock {
  struct sysblock *next;         /* next system or headblock */
  movtstr   *movt;               /* -> relevant movement */
  CBOOL      is_sysblock;        /* indicates sysblock or headblock */
  /********** The following are specific to sysblock **********/
  uint8_t    flags;              /* see below */
  snamestr **stavenames;         /* -> pointers to name structures */
  contstr   *cont;               /* -> vector of contstrs */
  int       *stavespacing;       /* -> vector of stave spacings */
  int       *ulevel;             /* -> vector of underlay levels */
  int       *olevel;             /* -> vector of overlay levels */
  uint64_t   notsuspend;         /* staves not suspended in this system */
  uint64_t   showtimes;          /* staves that have starting time signatures */
  uint16_t   barstart;           /* starting bar number */
  uint16_t   barend;             /* ending bar number */
  int32_t    systemgap;          /* system gap value */
  /* Values computed as the system is processed */
  int32_t    barlinewidth;       /* stretched for this system */
  int32_t    firstnoteposition;  /* sic */
  int32_t    joinxposition;      /* position for joining signs */
  int32_t    keyxposition;       /* position for key signature */
  int32_t    overrun;            /* overrun */
  int32_t    startxposition;     /* position after staff name text */
  int32_t    systemdepth;        /* total of stavespacings */
  int32_t    timexposition;      /* position for time signature */
  int32_t    xjustify;           /* justify amount for whole system */
} sysblock;

/* Flag bits */

#define sysblock_warnkey   1    /* print additional key signature warning bar */
#define sysblock_warntime  2    /* ditto, time signature */
#define sysblock_warn      3    /* mask for either one of the above */
#define sysblock_stretch   4    /* flag set when stretched */
#define sysblock_noadvance 8    /* don't advance vertically */


/* Layout of data in a head block structure. Up to and including is_sysblock
it must have the same layout as a sysblock, because they are held in the same
chain. We avoid using a union because the notation is so long and clumsy. */

typedef struct {
  sysblock *next;
  movtstr  *movt;
  CBOOL     is_sysblock;    /* indicates sysblock or headblock */
  /********** The following are specific to headblock **********/
  CBOOL     pageheading;    /* indicates this is a page heading */
  headstr  *headings;
} headblock;


/* Head of page block */

typedef struct pagestr {
  struct pagestr  *next;
  sysblock  *sysblocks;     /* -> chain of head+sysblocks */
  headblock *footing;       /* -> footing blocks */
   int32_t   topspace;      /* space to insert at top of page */
   int32_t   spaceleft;     /* space left (used for justifying) */
   int32_t   overrun;       /* space required to fit another system on */
  uint32_t   number;        /* page number */
} pagestr;


/* Custom built "hyphen type" for underlay/overlay. */

typedef struct htypestr {
  struct htypestr *next;
  uint32_t *string1;        /* main repeat string */
  uint32_t *string2;        /* line start string */
  uint32_t *string3;        /* final string */
   int32_t adjust;          /* vertical adjust */
  uint16_t size1;           /* size for main and final string */
  uint16_t size2;           /* size for line start string */
} htypestr;

/* One printkey item */

typedef struct pkeystr {
  struct pkeystr *next;
  uint32_t movt_number;     /* movement in which defined */
  uint32_t key;
  uint32_t clef;
  uint32_t *string;
  uint32_t *cstring;
} pkeystr;

/* One printtime item */

typedef struct ptimestr {
  struct ptimestr *next;
  uint32_t  movt_number;      /* movement in which defined */
  uint32_t  time;
  uint32_t *top;
  uint32_t *bot;
  uint8_t   sizetop;
  uint8_t   sizebot;
} ptimestr;

/* Structure for paper size information */

typedef struct sheetstr{
  const char *name;
  uint32_t    linelength;
  uint32_t    pagelength;
  uint32_t    sheetdepth;
  uint32_t    sheetwidth;
  uint32_t    sheetsize;
} sheetstr;

/* One transposed key masquerade item */

typedef struct trkeystr {
  struct trkeystr *next;
  uint32_t oldkey;
  uint32_t newkey;
} trkeystr;

/* Layout of entry in music font printing structure */

typedef struct mfstr {
  struct mfstr *next;
  int32_t x;
  int32_t y;
 uint32_t ch;                 /* Holds up to 4 8-bit code points */
} mfstr;


/* End of structs.h */
