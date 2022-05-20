/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last edited: User 2022 */

/* Allocate storage and initialize global XML variables, apart from those that
are tables of some kind. */

#include "pmw.h"


/*************************************************
*           General global variables             *
*************************************************/

uint8_t          xml_couple_settings[64];

int              xml_error_max              = 0;

int32_t          xml_first_system_distance  = -1;
int32_t          xml_fontsizes[UserFontSizes];
int              xml_fontsize_next          = 0;
int32_t          xml_fontsize_word_default  = -1;

xml_group_data  *xml_groups_list            = NULL;
BOOL             xml_group_symbol_set       = FALSE;

tree_node       *xml_ignored_element_tree   = NULL;

uschar          *xml_layout_list            = NULL;
size_t           xml_layout_list_size       = 0;
size_t           xml_layout_top             = 0;

xml_item        *xml_main_item_list         = NULL;
uint32_t         xml_movt_setflags          = 0;
uint32_t         xml_movt_unsetflags        = 0;

xml_part_data   *xml_parts_list             = NULL;
xml_item        *xml_partwise_item_list     = NULL;
int              xml_pmw_stave_count        = 0;

xml_item        *xml_read_addto             = NULL;
BOOL             xml_read_done              = FALSE;
uschar          *xml_read_filename          = NULL;
int              xml_read_linenumber        = 0;
BOOL             xml_right_justify_stave_names = TRUE;

BOOL             xml_set_stave_size         = FALSE;
int              xml_stave_sizes[64];

int              xml_time_signature_seen    = -1;

tree_node       *xml_unrecognized_element_tree = NULL;

BOOL             xml_warn_unrecognized      = TRUE;

/* End of xml_globals.c */
