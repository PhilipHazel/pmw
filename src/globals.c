/*************************************************
*       PMW global variable instantiations       *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: Decenber 2025 */

#include "pmw.h"

int32_t      active_transpose = NO_TRANSPOSE;
int32_t      active_transpose_letter = 0;
BOOL         active_transpose_letter_is_auto = FALSE;
BOOL         active_transposedaccforce = FALSE;

uint8_t      barlinestyles[MAX_STAVE+1];
contstr     *bar_cont;
BOOL         bar_use_draw = FALSE;

int          beam_accrit;
BOOL         beam_continued;
int          beam_count;
b_notestr   *beam_first;
int32_t      beam_firstmoff;
uint16_t     beam_firstpitch;
int32_t      beam_firstX;
int32_t      beam_firstY;
int32_t      beam_forceslope = INT32_MAX;
b_notestr   *beam_last;
int32_t      beam_lastmoff;
int32_t      beam_offset;
int32_t      beam_offsetadjust;
BOOL         beam_overbeam = FALSE;
int          beam_seq;
int32_t      beam_slope;
BOOL         beam_splitOK;
int8_t       beam_stemadjusts[MAX_BEAMNOTES];
BOOL         beam_upflag;
int32_t      beam_Xcorrection;

breadstr     brs;

int32_t      curbarnumber;
movtstr     *curmovt = NULL;
pagestr     *curpage = NULL;
int32_t      curstave;

int          dbd_bar = -1;
int          dbd_movement = -1;
int          dbd_stave = -1;
uint32_t     debug_selector = 0;

int32_t      draw_gap = 0;
int32_t      draw_lgx = 0;
int32_t      draw_lgy = 0;
int32_t      draw_ox;
int32_t      draw_oy;
int32_t      draw_thickness = 500;
tree_node   *draw_tree = NULL;
tree_node   *draw_variable_tree = NULL;

usint        error_maximum = DEFAULT_ERROR_MAXIMUM;
const char  *error_inoption = NULL;

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
b2pf_context **font_b2pf_contexts = NULL;
uint32_t    *font_b2pf_options = NULL;
BOOL         font_call_b2pf = FALSE;
#endif

int32_t      font_cosr = 1000;
uint32_t     font_count = 0;
uschar      *font_data_default = US FONTMETRICS ":" FONTDIR;
uschar      *font_data_extra = NULL;
fontstr     *font_list = NULL;
size_t       font_list_size = 0;
uschar      *font_music_default = US FONTDIR;
uschar      *font_music_extra = NULL;
int32_t      font_sinr = 0;
uint32_t     font_table[font_tablen];
int32_t      font_transform[6];

BOOL         macro_expanding = FALSE;
size_t       macro_in = 0;
tree_node   *macro_tree = NULL;

uschar      *main_argbuffer[MAX_MACRODEPTH] = { NULL };
size_t       main_argbuffer_size[MAX_MACRODEPTH];
BOOL         main_error_136 = FALSE;
uschar      *main_filename = NULL;
uschar      *main_format = NULL;
BOOL         main_format_tested = FALSE;
gapstr      *main_freegapblocks = NULL;
hairpinstr  *main_freehairpinstr = NULL;
nbarstr     *main_freenbarblocks = NULL;
obeamstr    *main_freeobeamstr = NULL;
overdrawstr *main_freeoverdrawstr = NULL;
slurstr     *main_freeslurblocks = NULL;
uolaystr    *main_freeuolayblocks = NULL;
contstr     *main_freezerocontblocks = NULL;
htypestr    *main_htypes = NULL;
BOOL         main_kerning = TRUE;
keytransstr *main_keytranspose = NULL;
BOOL         main_landscape = FALSE;
uint32_t     main_lastpagenumber = 0;
uint32_t     main_magnification = 1000;
int32_t      main_maxstave = -1;
int32_t      main_maxvertjustify = 60000;
BOOL         main_midifornotesoff = FALSE;
b_ornamentstr *main_nextnoteornament = NULL;
pagestr     *main_pageanchor;
int32_t      main_pagelength = 720000;
pkeystr     *main_printkey = NULL;
ptimestr    *main_printtime = NULL;
uschar      *main_readbuffer = NULL;
uschar      *main_readbuffer_raw = NULL;
uschar      *main_readbuffer_previous = NULL;
size_t       main_readbuffer_size = MAIN_READBUFFER_CHUNKSIZE;
size_t       main_readbuffer_threshold = MAIN_READBUFFER_CHUNKSIZE - 2;
size_t       main_readlength = 0;
BOOL         main_righttoleft = FALSE;
uint64_t     main_selectedstaves = ~0uL;
uint32_t     main_sheetdepth = 842000;
uint32_t     main_sheetsize = sheet_A4;
uint32_t     main_sheetwidth = 595000;
BOOL         main_showid = TRUE;
int          main_state = STATE_INIT;
BOOL         main_suppress_output = FALSE;
int          main_testing = 0;
int32_t      main_tracepos = INT32_MAX - 1;
int32_t      main_transpose = NO_TRANSPOSE;
BOOL         main_transposedaccforce = TRUE;
trkeystr    *main_transposedkeys = NULL;
int32_t      main_truepagelength = 0;
BOOL         main_verify = FALSE;
uint32_t     main_xmloptions = 0;

uint32_t     midi_endbar = UINT32_MAX;
uschar      *midi_filename = NULL;
int          midi_movement = 1;
uschar      *midi_perc = US MIDIPERC;
uschar      *midi_percnames = NULL;
BOOL         midi_repeats = TRUE;
uint32_t     midi_startbar = UINT32_MAX;
uschar      *midi_voicenames = NULL;
uschar      *midi_voices = US MIDIVOICES;

usint        movement_count = 0;
movtstr    **movements = NULL;
size_t       movements_size = 0;

uint8_t      n_acc;
uint32_t     n_acflags;
int32_t      n_accleft;
BOOL         n_beamed;
uint32_t     n_chordacflags;
int          n_chordcount;
b_notestr   *n_chordfirst;
uint32_t     n_chordflags;
int32_t      n_cueadjust;
uint32_t     n_dots;
int32_t      n_dotxadjust;
uint8_t      n_firstacc;
uint32_t     n_flags;
int32_t      n_fontsize;
int          n_gracecount;
int32_t      n_gracemoff;
BOOL         n_invertleft;
BOOL         n_invertright;
uint8_t      n_lastacc;
b_notestr   *n_lastnote;
uint32_t     n_length;
int32_t      n_longrestmid;
uint8_t      n_masq;
int32_t      n_maxaccleft;
uint16_t     n_maxpitch;
uint16_t     n_minpitch;
b_tiestr    *n_nexttie;
BOOL         n_nhtied;
uint8_t      n_noteheadstyle;
int          n_notetype;
b_ornamentstr *n_ornament;
int32_t      n_pcorrection;
uint16_t     n_pitch;
b_tiestr    *n_prevtie;
int32_t      n_restlevel;
BOOL         n_smallhead;
int32_t      n_stemlength;
int          n_upfactor;
BOOL         n_upflag;
int32_t      n_x;

b_accentmovestr *out_accentmove[ACCENT_COUNT];
int32_t      out_barlinemagn;
int32_t      out_barlinex;
int32_t      out_barx;
int32_t      out_bbox[4];
BOOL         out_beaming;
int32_t      out_botstave;
int32_t      out_dashgaplength;
int32_t      out_dashlength;
int32_t      out_downgap;
b_drawstr  **out_drawqueue = NULL;
size_t       out_drawqueue_ptr = 0;
size_t       out_drawqueue_size = 0;
int          out_drawstackptr;
FILE        *out_file;
uschar      *out_filename = NULL;
int32_t      out_gracefudge;
BOOL         out_gracenotes;
BOOL         out_hairpinhalf;
int          out_keycount;
int32_t      out_lastbarlinex;
BOOL         out_lastbarwide;
int32_t      out_lastmoff;
BOOL         out_lastnotebeamed;
int32_t      out_lastnotex;
int          out_laststave;
CBOOL        out_laststemup[MAX_STAVE+1];
BOOL         out_lineendflag;
int          out_manyrest;
int32_t      out_moff;
overdrawstr *out_overdraw = NULL;
BOOL         out_passedreset;
int32_t      out_pitchmagn;
b_pletstr   *out_plet;
int          out_pletden;
int          out_pletnum;
int          out_plet_highest;
int          out_plet_highest_head;
int          out_plet_lowest;
int32_t      out_plet_x;
posstr      *out_poslast;
posstr      *out_posptr;
posstr      *out_postable;
BOOL         out_repeatonbarline;
int32_t      out_slurclx = 0;
int32_t      out_slurcly = 0;
int32_t      out_slurcrx = 0;
int32_t      out_slurcry = 0;
BOOL         out_slurstarted = FALSE;
BOOL         out_startlinebar;
int32_t      out_stavebottom;
uint8_t      out_stavelines;
int32_t      out_stavemagn;
int32_t      out_stavetop;
int32_t      out_string_endx;
int32_t      out_string_endy;
sysblock    *out_sysblock;
int          out_textnextabove;
int          out_textnextbelow;
b_textstr  **out_textqueue = NULL;
size_t       out_textqueue_ptr = 0;
size_t       out_textqueue_size = 0;
int          out_timecount;
int32_t      out_topstave;
b_tremolostr *out_tremolo;
BOOL         out_tremupflag;
int32_t      out_tremx;
int32_t      out_tremy;
int32_t      out_upgap;
int32_t      out_Xadjustment;
int32_t      out_Yadjustment;
int32_t      out_ybarend;
int32_t      out_ybarenddeep;
int32_t      out_yposition;
int32_t      out_ystave;

#if SUPPORT_XML
uschar      *outxml_filename = NULL;
int          outxml_movement = 1;
#endif

uint32_t     page_firstnumber = 1;
uint32_t     page_increment = 1;

BOOL         PDF = PDF_DEFAULT;
BOOL         PDFforced = FALSE;
BOOL         PSforced = FALSE;
BOOL         EPSforced = FALSE;

uint32_t     pletstack[MAX_PLETNEST - 1];
uint32_t     pletstackcount;

BOOL         pout_changecolour = FALSE;
int32_t      pout_curcolour[3] = {0, 0, 0};
stavelist   *pout_curlist;
uint32_t     pout_curnumber;
fontinststr  pout_mfdata = { NULL, 0, 0 };   /* For temporary use */

/* Characters in the music font for stave fragments with different numbers of
lines, both 10 points long and 100 points long. */

uint8_t      pout_stavechar1[] =  { 0, 'D', 169, 170, 171, 'C', 172 };
uint8_t      pout_stavechar10[] = { 0, 'G', 247, 248, 249, 'F', 250 };

int32_t      pout_wantcolour[3] = {0, 0, 0};
int32_t      pout_ymax;

movtstr     *premovt = NULL;

int          print_copies = 1;
BOOL         print_duplex = FALSE;
int32_t      print_gutter = 0;
int32_t      print_image_xadjust = 0;
int32_t      print_image_yadjust = 0;
uint8_t      print_imposition = pc_normal;
BOOL         print_incPMWfont = FALSE;
uint32_t     print_lastpagenumber = 0;
int32_t      print_magnification = 1000;
BOOL         print_manualfeed = FALSE;
uint8_t      print_pagefeed = pc_normal;
stavelist   *print_pagelist = NULL;
uint8_t      print_pageorigin;
BOOL         print_pamphlet = FALSE;
BOOL         print_reverse = FALSE;
int32_t      print_sheetwidth;
BOOL         print_side1 = TRUE;
BOOL         print_side2 = TRUE;
BOOL         print_tumble = FALSE;
int32_t      print_xmargin = 0;

const uschar *ps_header = CUS PSHEADER;

BOOL         pmw_reading_stave = FALSE;

uint32_t     read_absnotespacing[NOTETYPE_COUNT] =
               { 30000,30000,22000,16000,12000,10000,10000,10000 };
int8_t      *read_baraccs;
int8_t      *read_baraccs_tp;
b_notestr  **read_beamstack;

/* It is important that read_c be a signed integer rather than unsigned,
because otherwise the EOF value (0xFFFFFFFF) is not treated as -1 and in some
environments functions like isspace() crash. */

int32_t      read_c = 0;
FILE        *read_filehandle = NULL;
uschar      *read_filename = NULL;
filestackstr read_filestack[MAX_INCLUDE];
usint        read_filestackptr = 0;
uint32_t     read_headmap = 0;
size_t       read_i = 0;
uint32_t     read_invalid_unicode[UUSIZE];
bstr        *read_lastitem = NULL;
usint        read_linenumber = 0;
usint        read_nextheadsize = 0;
usint        read_okdepth = 0;
usint        read_skipdepth = 0;
b_notestr  **read_stemstack;
uschar      *read_stringbuffer = NULL;
size_t       read_stringbuffer_size = 0;
tiedata      read_tiedata[MAX_CHORDSIZE];
int32_t      read_uinvnext = 0;
BOOL         read_uinvoverflow = FALSE;
uint32_t     read_unsupported_unicode[UUSIZE];
int32_t      read_uunext = 0;
BOOL         read_uuoverflow = FALSE;
uschar       read_wordbuffer[WORDBUFFER_SIZE];

sreadstr     srs;
stavestr    *st = NULL;
int          stave_use_draw = 0;
BOOL         stave_use_widechars = TRUE;
uschar      *stdmacs_dir = US STDMACS;
int          string_double_precision = 2;

BOOL         unclosed_slurline = FALSE;

contstr     *wk_cont;


/* -------- Switched function pointers -------- */

void         (*ofi_abspath)(int32_t *, int32_t *, int *, int32_t);
void         (*ofi_barline)(int32_t, int32_t, int32_t, int, int32_t);
void         (*ofi_beam)(int32_t, int32_t, int, int);
void         (*ofi_brace)(int32_t, int32_t, int32_t, int32_t);
void         (*ofi_bracket)(int32_t, int32_t, int32_t, int32_t);
void         (*ofi_getcolour)(int32_t *);
void         (*ofi_grestore)(void);
void         (*ofi_gsave)(void);
void         (*ofi_line)(int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t);
void         (*ofi_lines)(int32_t *, int32_t *, int, int32_t);
void         (*ofi_muschar)(int32_t, int32_t, uint32_t, int32_t);
void         (*ofi_musstring)(uschar *, int32_t, int32_t, int32_t);
void         (*ofi_path)(int32_t *, int32_t *, int *, int32_t);
void         (*ofi_rotate)(double);
void         (*ofi_setcapandjoin)(uint32_t);
void         (*ofi_setcolour)(int32_t *);
void         (*ofi_setdash)(int32_t, int32_t);
void         (*ofi_setgray)(int32_t);
void         (*ofi_slur)(int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t);
void         (*ofi_startbar)(int, int);
void         (*ofi_stave)(int32_t, int32_t, int32_t, int);
void         (*ofi_string)(uint32_t *, fontinststr *, int32_t *, int32_t *,
               BOOL);
void         (*ofi_translate)(int32_t, int32_t);

/* End of globals.c */
