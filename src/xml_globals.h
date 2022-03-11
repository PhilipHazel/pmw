/*************************************************
*             MusicXML input for PMW             *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last edited: February 2022 */

/* Header file for all the XML global variables */


/*************************************************
*           General global variables             *
*************************************************/

extern uint8_t          xml_couple_settings[];

extern BOOL             xml_debug_need_nl;
// extern uint32_t      debug_selector;

// extern entity_block  entity_list[];
// extern int           entity_list_count;
extern int              xml_error_max;

extern int32_t          xml_first_system_distance;
extern int32_t          xml_fontsizes[];
extern int              xml_fontsize_next;
extern int32_t          xml_fontsize_word_default;

extern xml_group_data  *xml_groups_list;
extern BOOL             xml_group_symbol_set;

// extern tree_node    *id_tree;
extern tree_node       *xml_ignored_element_tree;

extern uschar          *xml_layout_list;
extern size_t           xml_layout_list_size;
extern size_t           xml_layout_top;

extern xml_item        *xml_main_item_list;
extern uint32_t         xml_movt_setflags;
extern uint32_t         xml_movt_unsetflags;

// extern int           magnification;
// extern int           memory_hwm;
// extern int           memory_used;

// extern BOOL          nocheck;

// extern FILE         *outfile;

extern xml_part_data   *xml_parts_list;
extern xml_item        *xml_partwise_item_list;
extern int              xml_pmw_stave_count;

extern xml_item        *xml_read_addto;
extern BOOL             xml_read_done;
extern uschar          *xml_read_filename;
extern int              xml_read_linenumber;
extern BOOL             xml_right_justify_stave_names;

extern BOOL             xml_set_stave_size;
extern int              xml_stave_sizes[];
extern xml_elliststr    xml_supported_elements[];
extern int              xml_supported_elements_count;
// extern BOOL          suppress_version;

extern int              xml_time_signature_seen;

extern tree_node       *xml_unrecognized_element_tree;

extern BOOL             xml_warn_unrecognized;

/* End of xml_globals.h */
