/*************************************************
*         PMW global variable definitions        *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: December 2020 */
/* This file last modified: May 2022 */


/*************************************************
*               Tables in tables.c               *
*************************************************/

extern accent        accent_chars[];

extern const char   *clef_names[];

extern uint32_t      default_hyphen[];
extern movtstr       default_movtstr;

extern uschar *      font_IdStrings[];

extern breadstr      init_breadstr;
extern snamestr      init_snamestr;
extern sreadstr      init_sreadstr;
extern stavestr      init_stavestr;

extern uint8_t       keyclefadjusts[];
extern uint8_t       keysigtable[][MAX_KEYACCS+1];

extern b_accentmovestr no_accent_move;

extern mfstr        *out_mftable[];    /* Music font table */

extern uint16_t      pitch_clef[];
extern uint16_t      pitch_stave[];

extern uint32_t      rbra_left[];
extern int8_t        read_accpitch[];
extern uint8_t       read_basicpitch[];
extern uint32_t      read_headingsizes[];

extern uint32_t      sbra_left[];
extern usint         sheets_count;
extern sheetstr      sheets_list[];

extern int32_t       stave_bottoms[];
extern int32_t       stave_tops[];

extern const usint   utf8_table1[];    /* Various tables for UTF-8 processing */
extern const int     utf8_table2[];
extern const int     utf8_table3[];
extern const uschar  utf8_table4[];


/*************************************************
*           Variables in globals.c               *
*************************************************/

extern int32_t      active_transpose;
extern int32_t      active_transpose_letter;
extern BOOL         active_transpose_letter_is_auto;
extern BOOL         active_transposedaccforce;

extern uint8_t      barlinestyles[];
extern contstr     *bar_cont;
extern BOOL         bar_use_draw;

extern int          beam_accrit;
extern BOOL         beam_continued;
extern int          beam_count;
extern b_notestr   *beam_first;
extern int32_t      beam_firstmoff;
extern uint16_t     beam_firstpitch;
extern int32_t      beam_firstX;
extern int32_t      beam_firstY;
extern int32_t      beam_forceslope;
extern b_notestr   *beam_last;
extern int32_t      beam_lastmoff;
extern int32_t      beam_offset;
extern int32_t      beam_offsetadjust;
extern BOOL         beam_overbeam;
extern int          beam_seq;
extern int32_t      beam_slope;
extern BOOL         beam_splitOK;
extern int8_t       beam_stemadjusts[];
extern BOOL         beam_upflag;
extern int32_t      beam_Xcorrection;

extern breadstr     brs;

extern int32_t      curbarnumber;
extern movtstr     *curmovt;
extern pagestr     *curpage;
extern int32_t      curstave;

extern int          dbd_bar;
extern int          dbd_movement;
extern int          dbd_stave;
extern uint32_t     debug_selector;

extern int32_t      draw_gap;
extern int32_t      draw_lgx;
extern int32_t      draw_lgy;
extern int32_t      draw_ox;
extern int32_t      draw_oy;
extern int32_t      draw_thickness;
extern tree_node   *draw_tree;
extern tree_node   *draw_variable_tree;

extern const char  *error_inoption;
extern usint        error_maximum;

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
extern b2pf_context **font_b2pf_contexts;
extern uint32_t    *font_b2pf_options;
#endif
extern int32_t      font_cosr;                 /* Cosine for rotations */
extern uint32_t     font_count;                /* Number of typefaces */
extern uschar      *font_data_default;         /* For AFM files, etc */
extern uschar      *font_data_extra;           /* -F argument */
extern fontstr     *font_list;                 /* List of typefaces */
extern size_t       font_list_size;
extern uschar      *font_music_default;
extern uschar      *font_music_extra;
extern int32_t      font_sinr;                 /* Sine for rotations */
extern uint32_t     font_table[];              /* Fonts by type, e.g. font_rm */

extern BOOL         macro_expanding;
extern size_t       macro_in;
extern tree_node   *macro_tree;

extern uschar      *main_argbuffer[];
extern size_t       main_argbuffer_size[];
extern BOOL         main_error_136;
extern uschar      *main_filename;
extern uschar      *main_format;
extern BOOL         main_format_tested;
extern gapstr      *main_freegapblocks;
extern hairpinstr  *main_freehairpinstr;
extern nbarstr     *main_freenbarblocks;
extern obeamstr    *main_freeobeamstr;
extern overdrawstr *main_freeoverdrawstr;
extern slurstr     *main_freeslurblocks;
extern uolaystr    *main_freeuolayblocks;
extern contstr     *main_freezerocontblocks;
extern htypestr    *main_htypes;
extern BOOL         main_kerning;
extern keytransstr *main_keytranspose;
extern BOOL         main_landscape;
extern uint32_t     main_lastpagenumber;
extern uint32_t     main_magnification;
extern int32_t      main_maxstave;
extern int32_t      main_maxvertjustify;
extern BOOL         main_midifornotesoff;
extern b_ornamentstr *main_nextnoteornament;
extern pagestr     *main_pageanchor;
extern int32_t      main_pagelength;
extern pkeystr     *main_printkey;
extern ptimestr    *main_printtime;
extern uschar      *main_readbuffer;
extern uschar      *main_readbuffer_raw;
extern uschar      *main_readbuffer_previous;
extern size_t       main_readbuffer_size;
extern size_t       main_readbuffer_threshold;
extern size_t       main_readlength;
extern BOOL         main_righttoleft;
extern uint64_t     main_selectedstaves;
extern uint32_t     main_sheetdepth;
extern uint32_t     main_sheetsize;
extern uint32_t     main_sheetwidth;
extern BOOL         main_showid;
extern int          main_state;
extern BOOL         main_suppress_output;
extern BOOL         main_testing;
extern int32_t      main_tracepos;
extern int32_t      main_transpose;
extern BOOL         main_transposedaccforce;
extern trkeystr    *main_transposedkeys;
extern int32_t      main_truepagelength;
extern BOOL         main_verify;

extern uint32_t     midi_endbar;
extern uschar      *midi_filename;
extern int          midi_movement;
extern uschar      *midi_perc;
extern uschar      *midi_percnames;
extern BOOL         midi_repeats;
extern uint32_t     midi_startbar;
extern uschar      *midi_voicenames;
extern uschar      *midi_voices;

extern usint        movement_count;
extern movtstr    **movements;
extern size_t       movements_size;

extern uint8_t      n_acc;
extern uint32_t     n_acflags;
extern int32_t      n_accleft;
extern BOOL         n_beamed;
extern uint32_t     n_chordacflags;
extern int          n_chordcount;
extern b_notestr   *n_chordfirst;
extern uint32_t     n_chordflags;
extern int32_t      n_cueadjust;
extern int32_t      n_dotxadjust;
extern uint8_t      n_firstacc;
extern uint32_t     n_flags;
extern int32_t      n_fontsize;
extern int          n_gracecount;
extern int32_t      n_gracemoff;
extern BOOL         n_invertleft;
extern BOOL         n_invertright;
extern uint8_t      n_lastacc;
extern b_notestr   *n_lastnote;
extern uint32_t     n_length;
extern int32_t      n_longrestmid;
extern uint8_t      n_masq;
extern int32_t      n_maxaccleft;
extern uint16_t     n_maxpitch;
extern uint16_t     n_minpitch;
extern b_tiestr    *n_nexttie;
extern BOOL         n_nhtied;
extern int          n_notetype;
extern b_ornamentstr *n_ornament;
extern int32_t      n_pcorrection;
extern uint16_t     n_pitch;
extern b_tiestr    *n_prevtie;
extern int32_t      n_restlevel;
extern int32_t      n_stemlength;
extern int          n_upfactor;
extern BOOL         n_upflag;
extern int32_t      n_x;

extern b_accentmovestr *out_accentmove[];
extern int32_t      out_barlinemagn;
extern int32_t      out_barlinex;
extern int32_t      out_barx;
extern int32_t      out_bbox[];
extern BOOL         out_beaming;
extern int32_t      out_botstave;
extern int32_t      out_downgap;
extern int32_t      out_dashgaplength;
extern int32_t      out_dashlength;
extern b_drawstr  **out_drawqueue ;
extern size_t       out_drawqueue_ptr;
extern size_t       out_drawqueue_size;
extern int          out_drawstackptr;
extern uschar      *out_filename;
extern int32_t      out_gracefudge;
extern BOOL         out_gracenotes;
extern BOOL         out_hairpinhalf;
extern int          out_keycount;
extern int32_t      out_lastbarlinex;
extern BOOL         out_lastbarwide;
extern int32_t      out_lastmoff;
extern BOOL         out_lastnotebeamed;
extern int32_t      out_lastnotex;
extern int          out_laststave;
extern CBOOL        out_laststemup[];
extern BOOL         out_lineendflag;
extern int          out_manyrest;
extern int32_t      out_moff;
extern overdrawstr *out_overdraw;
extern BOOL         out_passedreset;
extern int32_t      out_pitchmagn;
extern b_pletstr   *out_plet;
extern int          out_pletden;
extern int          out_pletnum;
extern int32_t      out_plet_highest;
extern int          out_plet_highest_head;
extern int32_t      out_plet_lowest;
extern int32_t      out_plet_x;
extern posstr      *out_poslast;
extern posstr      *out_posptr;
extern posstr      *out_postable;
extern BOOL         out_repeatonbarline;
extern BOOL         out_startlinebar;
extern int32_t      out_slurclx;
extern int32_t      out_slurcly;
extern int32_t      out_slurcrx;
extern int32_t      out_slurcry;
extern BOOL         out_slurstarted;
extern int32_t      out_stavebottom;
extern int32_t      out_stavemagn;
extern uint8_t      out_stavelines;
extern int32_t      out_stavetop;
extern int32_t      out_string_endx;
extern int32_t      out_string_endy;
extern sysblock    *out_sysblock;
extern int          out_textnextabove;
extern int          out_textnextbelow;
extern b_textstr  **out_textqueue;
extern size_t       out_textqueue_ptr;
extern size_t       out_textqueue_size;
extern int          out_timecount;
extern int32_t      out_topstave;
extern b_tremolostr *out_tremolo;
extern BOOL         out_tremupflag;
extern int32_t      out_tremx;
extern int32_t      out_tremy;
extern int32_t      out_upgap;
extern int32_t      out_Xadjustment;
extern int32_t      out_Yadjustment;
extern int32_t      out_ybarend;
extern int32_t      out_ybarenddeep;
extern int32_t      out_yposition;
extern int32_t      out_ystave;

extern uint32_t     page_firstnumber;
extern uint32_t     page_increment;

extern movtstr     *premovt;

extern int          print_copies;
extern BOOL         print_duplex;
extern int32_t      print_gutter;
extern int32_t      print_image_xadjust;
extern int32_t      print_image_yadjust;
extern uint8_t      print_imposition;
extern BOOL         print_incPMWfont;
extern uint32_t     print_lastpagenumber;
extern int32_t      print_magnification;
extern BOOL         print_manualfeed;
extern uint8_t      print_pagefeed;
extern stavelist   *print_pagelist;
extern uint8_t      print_pageorigin;
extern BOOL         print_pamphlet;
extern BOOL         print_reverse;
extern int32_t      print_sheetwidth;
extern BOOL         print_side1;
extern BOOL         print_side2;
extern BOOL         print_tumble;
extern int32_t      print_xmargin;

extern FILE        *ps_file;
extern const uschar *ps_header;

extern BOOL         pmw_reading_stave;

extern uint32_t     read_absnotespacing[];
extern int8_t      *read_baraccs;
extern int8_t      *read_baraccs_tp;
extern b_notestr  **read_beamstack;
extern uint32_t     read_c;
extern FILE        *read_filehandle;
extern uschar      *read_filename;
extern filestackstr read_filestack[];
extern usint        read_filestackptr;
extern uint32_t     read_headmap;
extern size_t       read_i;
extern uint32_t     read_invalid_unicode[];
extern bstr        *read_lastitem;
extern usint        read_linenumber;
extern usint        read_nextheadsize;
extern usint        read_okdepth;
extern usint        read_skipdepth;
extern b_notestr  **read_stemstack;
extern uschar      *read_stringbuffer;
extern size_t       read_stringbuffer_size;
extern tiedata      read_tiedata[];
extern int32_t      read_uinvnext;
extern BOOL         read_uinvoverflow;
extern uint32_t     read_unsupported_unicode[];
extern int32_t      read_uunext;
extern BOOL         read_uuoverflow;
extern uschar       read_wordbuffer[];

extern sreadstr     srs;
extern stavestr    *st;
extern int          stave_use_draw;
extern BOOL         stave_use_widechars;

extern contstr     *wk_cont;

/* End of globals.h */
