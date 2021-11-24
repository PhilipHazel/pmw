/*************************************************
*       PMW prototypes for global functions      *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: December 2020 */
/* This file last modified: September 2021 */

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
extern void         font_loadtables(uint32_t);
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

extern void         pmw_read(void);
extern void         pmw_read_header(void);
extern void         pmw_read_stave(void);

extern void         preprocess_line(void);

extern void         ps_abspath(int32_t *, int32_t *, int *, int32_t);
extern void         ps_barline(int32_t, int32_t, int32_t, int, int32_t);
extern void         ps_beam(int32_t, int32_t, int, int);
extern void         ps_brace(int32_t, int32_t, int32_t, int32_t);
extern void         ps_bracket(int32_t, int32_t, int32_t, int32_t);
extern void         ps_go(void);
extern void         ps_grestore(void);
extern void         ps_gsave(void);
extern void         ps_line(int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t);
extern void         ps_lines(int32_t *, int32_t *, int, int32_t);
extern void         ps_muschar(int32_t, int32_t, uint32_t, int32_t);
extern void         ps_musstring(uschar *, int32_t, int32_t, int32_t);
extern void         ps_path(int32_t *, int32_t *, int *, int32_t);
extern void         ps_relmusstring(uschar *, int32_t, int32_t, int32_t);
extern void         ps_rotate(double);
extern void         ps_setdash(int32_t, int32_t, uint32_t);
extern void         ps_setgray(int32_t);
extern void         ps_slur(int32_t, int32_t, int32_t, int32_t, uint32_t,
                      int32_t);
extern void         ps_startbar(int, int);
extern void         ps_stave(int32_t, int32_t, int32_t, int);
extern void         ps_string(uint32_t *, fontinststr *, int32_t *, int32_t *,
                      BOOL);
extern void         ps_translate(int32_t, int32_t);

extern uint32_t     read_accororn(uint32_t);
extern uint32_t     read_barnumber(void);
extern uint32_t     read_compute_barlength(uint32_t);
extern BOOL         read_do_stavedirective(void);
extern void         read_draw(tree_node **, drawitem **, uint32_t);
extern void         read_draw_definition(void);
extern drawtextstr *read_draw_text(void);
extern BOOL         read_expect_integer(int32_t *, BOOL, BOOL);
extern void         read_extend_buffers(void);
extern void         read_file(enum filetype);
extern uint32_t     read_fixed(void);
extern void         read_fontsize(fontinststr *, BOOL);
extern int          read_getmidinumber(uschar *, uschar *, uschar *);
extern void         read_headfootingtext(headstr *, uint32_t, uint32_t);
extern void         read_init_baraccs(int8_t *, uint32_t);
extern void         read_init_movement(movtstr *, uint32_t);
extern stavestr    *read_init_stave(int32_t);
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
extern uint32_t     read_time(void);
extern uint32_t     read_usint(void);

extern void         slur_drawslur(slurstr *, int32_t, int, BOOL);
extern slurstr     *slur_endslur(b_endslurstr *);
extern slurstr     *slur_startslur(b_slurstr *);

extern void         string_extend_buffer(void);
extern char        *string_format_barnumber(uint32_t);
extern char        *string_format_fixed(int32_t);
extern char        *string_format_key(uint32_t);
extern char        *string_format_multiple_fixed(const char *, ...);
extern char        *string_format_notelength(int32_t);
extern char        *string_format_pitch(uint16_t, BOOL);
extern BOOL         string_pmweq(uint32_t *, uint32_t *);
extern uint32_t    *string_pmw(uschar *, int);
extern uint32_t    *string_read(uint32_t);
extern BOOL         string_read_plain(void);
extern void         string_relativize(void);
extern void         string_stavestring(BOOL);
extern int32_t      string_width(uint32_t *, fontinststr *, int32_t *);
extern int          strncmpic(const char*, const char *, int);

extern uint32_t     transpose_key(uint32_t);
extern uint16_t     transpose_note(uint16_t, uint16_t *, uint8_t *, uint8_t,
                      BOOL, BOOL, BOOL, int);

extern BOOL         tree_insert(tree_node **, tree_node *);
extern tree_node   *tree_search(tree_node *, uschar *);

/* End of functions.h */
