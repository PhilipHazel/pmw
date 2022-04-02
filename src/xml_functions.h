/*************************************************
*               MusicXML input for PMW           *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last updated: April 2022 */

/* This header defines all the global XML functions */

extern void          xml_add_attrval_to_tree(tree_node **, xml_item *, xml_attrstr *);
extern BOOL          xml_analyze(void);
extern size_t        xml_convert_utf8(uint32_t *, uschar *, uint32_t, BOOL);
extern void          xml_debug_print_item_list(xml_item *, const char *);
extern void          xml_do_heading(void);
extern void          xml_do_parts(void);
extern void          xml_Eerror(xml_item *, int, ...);
extern void          xml_error(int, ...);
extern xml_attrstr  *xml_find_attr(xml_item *, uschar *);
extern xml_item     *xml_find_item(xml_item *, uschar *);
extern xml_item     *xml_find_next(xml_item *, xml_item *);
extern xml_item     *xml_find_part(xml_item *, uschar *);
extern xml_item     *xml_find_prev(xml_item *, xml_item *);
extern int32_t       xml_get_attr_mils(xml_item *, uschar *, int, int, int, BOOL);
extern int           xml_get_attr_number(xml_item *, uschar *, int, int, int, BOOL);
extern uschar       *xml_get_attr_string(xml_item *, uschar *, uschar *, BOOL);
extern int           xml_get_mils(xml_item *, uschar *, int, int, int, BOOL);
extern int           xml_get_number(xml_item *, uschar *, int, int, int, BOOL);
extern uschar       *xml_get_string(xml_item *, uschar *, uschar *, BOOL);
extern int           xml_get_this_number(xml_item *, int, int, int, BOOL);
extern uschar       *xml_get_this_string(xml_item *);
extern void          xml_insert_item(xml_item *, xml_item *);
extern xml_item     *xml_new_item(uschar *);
extern int           xml_pmw_fontsize(int);
extern BOOL          xml_read_file(uschar *, FILE *, xml_item *);
extern void          xml_set_number(xml_item *, uschar *, int);
extern int           xml_string_check_number(uschar *, int, int, int, BOOL *);
extern int           xml_string_to_number(uschar *, BOOL);

/* End of xml_functions.h */
