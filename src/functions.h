/*************************************************
*       PMW prototypes for global functions      *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: April 2025 */

/* -------- General functions -------- */

extern void         debug_bar(void);
extern void         debug_header(void);
extern void         debug_memory_usage(void);
extern void         debug_string(uint32_t *);

extern BOOL         error(enum error_number, ...);
extern BOOL         error_skip(enum error_number, uint32_t, ...);

extern void         font_addfont(uschar *, uint32_t, uint32_t);
extern int32_t      font_charwidth(uint32_t, uint32_t, uint32_t, int32_t,
                      int32_t *);
extern FILE        *font_finddata(uschar *, const char *, uschar *, uschar *,
                      uschar *, BOOL);
extern uint32_t     font_readtype(BOOL);
extern fontinststr *font_rotate(fontinststr *, int32_t);
extern uint32_t     font_search(uschar *);
extern uint32_t     font_utranslate(uint32_t, fontstr *);

extern void         mem_connect_item(bstr *);
extern uschar      *mem_copystring(uschar *);
extern void        *mem_duplicate_item(void *, size_t);
extern void         mem_free(void);
extern void         mem_free_cached(void **, void *);
extern void        *mem_get(size_t);
extern void        *mem_get_cached(void **, size_t);
extern void        *mem_get_independent(size_t);
extern usint        mem_get_info(size_t *, size_t *);
extern void        *mem_get_insert_item(size_t, usint, bstr *);
extern void        *mem_get_item(size_t, usint);
extern void         mem_record_next_item(bstr **);
extern void         mem_register(void *, size_t);

extern void         midi_write(void);

extern void         misc_commoncont(bstr *);
extern void         misc_copycontstr(contstr *, contstr *, int, BOOL);
extern void         misc_freenbar(void);
extern BOOL         misc_get_range(uint64_t, usint, usint *, usint *);
extern int32_t      misc_keywidth(uint32_t, uint16_t);
extern b_notestr   *misc_nextnote(void *);
extern int          misc_ord2utf8(uint32_t, uschar *);
extern void         misc_psprintf(uint32_t *, int, const char *, ...);
extern void         misc_startnbar(b_nbarstr *, int32_t, int32_t);
extern void         misc_tidycontstr(contstr *, int);
extern int32_t      misc_timewidth(uint32_t);
extern int32_t      misc_ybound(BOOL, b_tiestr *, BOOL, BOOL);

extern void         out_ascstring(uschar *, int, int32_t, int32_t, int32_t);
extern void         out_dodraw(tree_node *, drawitem *, BOOL);
extern void         out_drawhairpin(b_hairpinstr *, int32_t);
extern int32_t      out_drawnbar(BOOL, int32_t);
extern void         out_extension(int32_t, int32_t, int32_t, fontinststr *);
extern posstr      *out_findTentry(int32_t);
extern int32_t      out_findAoffset(int32_t);
extern int32_t      out_findGoffset(int32_t, int32_t);
extern int32_t      out_findXoffset(int32_t);
extern void         out_glissando(int32_t, uint8_t);
extern void         out_hyphens(int32_t, int32_t, int32_t, fontinststr *, BOOL);
extern void         out_page(void);
extern void         out_repeatstring(int32_t, int32_t, int32_t, BOOL, BOOL, int);
extern int          out_setbar(int);
extern void         out_setchordtie(b_notestr *, int, int32_t, BOOL, uint8_t);
extern bstr        *out_setnote(b_notestr *);
extern void         out_setnotetie(int32_t, BOOL, uint8_t);
extern BOOL         out_setother(bstr *);
extern BOOL         out_setupbeam(b_notestr *, int32_t, BOOL, BOOL);
extern void         out_slur(int32_t, int32_t, int32_t, int32_t, uint32_t,
                      int32_t, int32_t, int32_t);
extern void         out_string(uint32_t *, fontinststr *, int32_t, int32_t,
                      uint32_t);
extern void         out_text(b_textstr *, BOOL);
extern void         out_writeclef(int32_t, int32_t, int, int32_t, BOOL);
extern void         out_writekey(int32_t, int32_t, uint32_t, uint32_t);
extern void         out_writerepeat(int32_t, int, int32_t);
extern void         out_writetime(int32_t, int32_t, usint);

extern void         paginate(void);

extern void         pout_beam(int32_t *, int32_t *, int32_t *, int32_t *,
                      int32_t *, int, int);
extern void         pout_getcolour(int32_t *);
extern int32_t      pout_getswidth(uint32_t *, usint, fontstr *, int32_t *,
                      int32_t *);
extern void         pout_muschar(int32_t, int32_t, uint32_t, int32_t,
                      void(*)(uint32_t *, usint, fontinststr *, int32_t,
                      int32_t));
extern BOOL         pout_get_pages(pagestr **, pagestr **);
extern void         pout_set_ymax_etc(int32_t *, int32_t *);
extern void         pout_setcolour(int32_t *);
extern void         pout_setgray(int32_t);
extern void         pout_setup_pagelist(BOOL);
extern void         pout_string(uint32_t *, fontinststr *, int32_t *,
                      int32_t *, BOOL, void(*)(uint32_t *, usint, fontinststr *,
                      int32_t, int32_t));
extern void         pmw_read(void);
extern void         pmw_read_header(void);
extern void         pmw_read_stave(void);

extern void         preprocess_line(void);

extern uint32_t     read_accororn(uint32_t);
extern uint32_t     read_barnumber(void);
extern uint32_t     read_compute_barlength(uint32_t);
extern BOOL         read_do_stavedirective(void);
extern void         read_draw(tree_node **, drawitem **, uint32_t);
extern void         read_draw_definition(void);
extern drawtextstr *read_draw_text(void);
extern void         read_ensure_bar_indexes(size_t);
extern BOOL         read_expect_integer(int32_t *, BOOL, BOOL);
extern void         read_extend_buffers(void);
extern void         read_file(enum filetype);
extern uint32_t     read_fixed(void);
extern void         read_fontsize(fontinststr *, BOOL);
extern int          read_getmidinumber(uschar *, uschar *, uschar *);
extern void         read_headfootingtext(headstr *, uint32_t, uint32_t);
extern void         read_init_baraccs(int8_t *, uint32_t);
extern void         read_init_movement(movtstr *, uint32_t, uint32_t);
extern stavestr    *read_init_stave(int32_t, BOOL);
extern uint32_t     read_key(void);
extern int32_t      read_movevalue(void);
extern void         read_nextc(void);
extern void         read_nextword(void);
extern void         read_note(void);
extern BOOL         read_physical_line(size_t);
extern uint32_t     read_scaletime(uint32_t);
extern void         read_setbeamstems(void);
extern void         read_sortchord(b_notestr *, uint32_t);
extern int          read_stavelist(uschar *, uschar **, uint64_t *,
                      stavelist **);
extern void         read_stavename(void);
extern uint16_t     read_stavepitch(void);
extern dirstr      *read_stave_searchdirlist(BOOL);
extern void         read_tidy_staves(BOOL);
extern uint32_t     read_time(void);
extern uint32_t     read_usint(void);

extern void         slur_drawslur(slurstr *, int32_t, int, BOOL);
extern slurstr     *slur_endslur(b_endslurstr *);
extern slurstr     *slur_startslur(b_slurstr *);

extern uint32_t    *string_check(uint32_t *, const char *, BOOL);
extern int          string_check_utf8(uschar *);
extern void         string_extend_buffer(void);
extern char        *string_format_barnumber(uint32_t);
extern char        *string_format_double(double);
extern char        *string_format_fixed(int32_t);
extern char        *string_format_key(uint32_t);
extern char        *string_format_multiple_fixed(const char *, ...);
extern char        *string_format_multiple_double(const char *, ...);
extern char        *string_format_notelength(int32_t);
extern char        *string_format_pitch(uint16_t, BOOL);
extern BOOL         string_pmweq(uint32_t *, uint32_t *);
extern uint32_t    *string_pmw(uschar *, int);
extern uint32_t    *string_read(uint32_t, BOOL);
extern BOOL         string_read_plain(void);
extern void         string_relativize(void);
extern void         string_stavestring(BOOL);
extern int32_t      string_width(uint32_t *, fontinststr *, int32_t *);
extern int          strncmpic(const unsigned char*, const unsigned char *, int);

extern uint32_t     transpose_key(uint32_t);
extern int16_t      transpose_note(int16_t, int16_t *, uint8_t *, uint8_t,
                      BOOL, BOOL, BOOL, BOOL, int);

extern BOOL         tree_insert(tree_node **, tree_node *);
extern tree_node   *tree_search(tree_node *, uschar *);

#if SUPPORT_XML
extern void         xml_read(void);
#endif


/* -------- PostScript output functions -------- */

extern void         ps_abspath(int32_t *, int32_t *, int *, int32_t);
extern void         ps_barline(int32_t, int32_t, int32_t, int, int32_t);
extern void         ps_beam(int32_t, int32_t, int, int);
extern void         ps_brace(int32_t, int32_t, int32_t, int32_t);
extern void         ps_bracket(int32_t, int32_t, int32_t, int32_t);
extern void         ps_getcolour(int32_t *);
extern void         ps_go(void);
extern void         ps_grestore(void);
extern void         ps_gsave(void);
extern void         ps_line(int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t);
extern void         ps_lines(int32_t *, int32_t *, int, int32_t);
extern void         ps_muschar(int32_t, int32_t, uint32_t, int32_t);
extern void         ps_musstring(uschar *, int32_t, int32_t, int32_t);
extern void         ps_path(int32_t *, int32_t *, int *, int32_t);
extern void         ps_rotate(double);
extern void         ps_setcapandjoin(uint32_t);
extern void         ps_setcolour(int32_t *);
extern void         ps_setdash(int32_t, int32_t);
extern void         ps_setgray(int32_t);
extern void         ps_slur(int32_t, int32_t, int32_t, int32_t, uint32_t,
                      int32_t);
extern void         ps_startbar(int, int);
extern void         ps_stave(int32_t, int32_t, int32_t, int);
extern void         ps_string(uint32_t *, fontinststr *, int32_t *, int32_t *,
                      BOOL);
extern void         ps_translate(int32_t, int32_t);


/* -------- PDF output functions -------- */

extern void         pdf_abspath(int32_t *, int32_t *, int *, int32_t);
extern void         pdf_barline(int32_t, int32_t, int32_t, int, int32_t);
extern void         pdf_beam(int32_t, int32_t, int, int);
extern void         pdf_brace(int32_t, int32_t, int32_t, int32_t);
extern void         pdf_bracket(int32_t, int32_t, int32_t, int32_t);
extern void         pdf_free_data(void);
extern void         pdf_getcolour(int32_t *);
extern void         pdf_go(void);
extern void         pdf_grestore(void);
extern void         pdf_gsave(void);
extern void         pdf_line(int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t);
extern void         pdf_lines(int32_t *, int32_t *, int, int32_t);
extern void         pdf_muschar(int32_t, int32_t, uint32_t, int32_t);
extern void         pdf_musstring(uschar *, int32_t, int32_t, int32_t);
extern void         pdf_path(int32_t *, int32_t *, int *, int32_t);
extern void         pdf_rotate(double);
extern void         pdf_setcapandjoin(uint32_t);
extern void         pdf_setcolour(int32_t *);
extern void         pdf_setdash(int32_t, int32_t);
extern void         pdf_setgray(int32_t);
extern void         pdf_slur(int32_t, int32_t, int32_t, int32_t, uint32_t,
                      int32_t);
extern void         pdf_startbar(int, int);
extern void         pdf_stave(int32_t, int32_t, int32_t, int);
extern void         pdf_string(uint32_t *, fontinststr *, int32_t *, int32_t *,
                      BOOL);
extern void         pdf_translate(int32_t, int32_t);


/* -------- Indirections for switching between PostScript and PDF -------- */

extern void         (*ofi_abspath)(int32_t *, int32_t *, int *, int32_t);
extern void         (*ofi_barline)(int32_t, int32_t, int32_t, int, int32_t);
extern void         (*ofi_beam)(int32_t, int32_t, int, int);
extern void         (*ofi_brace)(int32_t, int32_t, int32_t, int32_t);
extern void         (*ofi_bracket)(int32_t, int32_t, int32_t, int32_t);
extern void         (*ofi_getcolour)(int32_t *);
extern void         (*ofi_grestore)(void);
extern void         (*ofi_gsave)(void);
extern void         (*ofi_line)(int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t);
extern void         (*ofi_lines)(int32_t *, int32_t *, int, int32_t);
extern void         (*ofi_muschar)(int32_t, int32_t, uint32_t, int32_t);
extern void         (*ofi_musstring)(uschar *, int32_t, int32_t, int32_t);
extern void         (*ofi_path)(int32_t *, int32_t *, int *, int32_t);
extern void         (*ofi_rotate)(double);
extern void         (*ofi_setcapandjoin)(uint32_t);
extern void         (*ofi_setcolour)(int32_t *);
extern void         (*ofi_setdash)(int32_t, int32_t);
extern void         (*ofi_setgray)(int32_t);
extern void         (*ofi_slur)(int32_t, int32_t, int32_t, int32_t, uint32_t,
                      int32_t);
extern void         (*ofi_startbar)(int, int);
extern void         (*ofi_stave)(int32_t, int32_t, int32_t, int);
extern void         (*ofi_string)(uint32_t *, fontinststr *, int32_t *,
                       int32_t *, BOOL);
extern void         (*ofi_translate)(int32_t, int32_t);

/* End of functions.h */
