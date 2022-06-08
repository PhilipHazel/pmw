/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* This file last modified: April 2022 */


/* This module contains functions for processing heading and general
information at the start of PMW data generation. */


#include "pmw.h"



/*************************************************
*         Convert UTF-8 string to PMW string     *
*************************************************/

/* After conversion, the string is checked for unsupported characters etc.

Arguments:
  ss          vector for storing PMW string (must be long enough)
  s           a UTF-8 string
  f           font id
  keepnl      retain newlines, otherwise turn into spaces

Returns:      number of PMW characters
*/

size_t
xml_convert_utf8(uint32_t *ss, uschar *s, uint32_t f, BOOL keepnl)
{
uint32_t *tt = ss;

f = f << 24;

while (*s != 0)
  {
  int aa = string_check_utf8(s) - 1;
  uint32_t c = *s++;

  /* Pick up any additional UTF-8 bytes. If aa < 0 the byte is illegal UTF-8.
  Give a warning and accept it as a one-byte value. */

  if (aa > 0)
    {
    int bb = 6*aa;
    c = (c & utf8_table3[aa]) << bb;
    for (int ii = 1; ii <= aa; ii++)
      {
      bb -= 6;
      c |= (*s++ & 0x3f) << bb;
      }
    }
  else if (aa < 0) error(ERR66, c);   /* Warning */

  /* Handle named XML character. */

  if (c == '&')
    {
    uschar *sss = s;
    for (; *sss != 0 && *sss != ';'; sss++) {}
    if (*sss != 0)  /* Semicolon found */
      {
      size_t len = sss - s;
      uschar buffer[64];

      if (len > 63)
        {
        xml_error(ERR54, len+1, s, "is too long");
        }
      else
        {
        xml_entity_block *bot = xml_entity_list;
        xml_entity_block *top = xml_entity_list + xml_entity_list_count;

        Ustrncpy(buffer, s, len);
        buffer[len] = 0;

        c = MAX_UNICODE + 1;
        while (top > bot)
          {
          xml_entity_block *mid = bot + (top - bot)/2;
          int cf = Ustrcmp(mid->name, buffer);
          if (cf == 0)
            {
            c = mid->value;
            break;
            }
          if (cf < 0) bot = mid + 1; else top = mid;
          }

        if (c > MAX_UNICODE)
          {
          xml_error(ERR54, len+1, s, "not recognized");
          c = '&';
          }
        else s = sss + 1;
        }
      }
    }

  /* Add character to the string. */

  *tt++ = f | c;
  }

*tt = 0;
string_check(ss, NULL, keepnl);
return tt - ss;
}



/*************************************************
*         Set up heading or footing data         *
*************************************************/

/* This function creates a headstr structure and hangs it on an appropriate
chain in the current movement. The "space after" value is set to the font size.
When generating an empty heading to effect upwards movement, the font size
argument may be negative. In this case it is used for "space after", but a
dummy font size of 12 points is set.

Arguments:
  sl        lefthand string
  sm        middle string
  sr        righthand string
  oldp      pointer to the anchor of the headstr chain
  size      font size or zero for "get next heading size"
  f         font it

Returns:    the depth of heading/footing (may be negative)
*/

static int32_t
handle_headfoot(uschar *sl, uschar *sm, uschar *sr, headstr **oldp,
  int32_t size, uint32_t f)
{
headstr *new = mem_get(sizeof(headstr));

size_t lenl = Ustrlen(sl);
size_t lenm = Ustrlen(sm);
size_t lenr = Ustrlen(sr);

uint32_t *ssl = mem_get((lenl + lenm + lenr + 3) * sizeof(uint32_t));
uint32_t *ssm = ssl + lenl + 1;
uint32_t *ssr = ssm + lenm + 1;

(void)xml_convert_utf8(ssl, sl, f, FALSE);
(void)xml_convert_utf8(ssm, sm, f, FALSE);
(void)xml_convert_utf8(ssr, sr, f, FALSE);

while (*oldp != NULL) oldp = &((*oldp)->next);
*oldp = new;
new->next = NULL;
new->string[0] = ssl;
new->string[1] = ssm;
new->string[2] = ssr;

new->fdata.matrix = NULL;
if (size == 0)
  {
  new->fdata.size = read_headingsizes[read_nextheadsize];
  if (read_headingsizes[read_nextheadsize+1] != 0 ) read_nextheadsize++;
  new->space = new->fdata.size;
  }
else
  {
  new->space = size;
  new->fdata.size = (size < 0)? 12000 : size;
  }
new->fdata.spacestretch = 0;

new->spaceabove = 0;
new->drawing = NULL;

return new->space;
}



/*************************************************
*             Process header material            *
*************************************************/

/* This function deals with everything that is part of the "header" in PMW,
that is, not part of a particular stave.

Arguments: none
Returns:   nothing
*/

void
xml_do_heading(void)
{
int32_t heading_base = -1;
int32_t top_system_distance = -1;

int32_t page_height = -1;
int32_t page_width = -1;
int32_t left_margin = -1;
int32_t right_margin = -1;
int32_t top_margin = -1;
int32_t bottom_margin = -1;

int32_t footing_depth = 0;
int32_t heading_depth = 0;
headstr *existing_footing;

BOOL has_barline[64];
BOOL done_title = FALSE;
BOOL done_work_number = FALSE;
BOOL done_arranger = FALSE;
BOOL done_composer = FALSE;
BOOL done_lyricist = FALSE;
BOOL done_rights = FALSE;

uschar *movement_title = NULL;
uschar *work_title = NULL;
uschar *work_number = NULL;
uschar *creator_arranger = NULL;
uschar *creator_composer = NULL;
uschar *creator_lyricist = NULL;
uschar *id_rights = NULL;

xml_item *defaults = xml_find_item(xml_partwise_item_list, US"defaults");
xml_item *id = xml_find_item(xml_partwise_item_list, US"identification");
xml_item *work = xml_find_item(xml_partwise_item_list, US"work");

/* If verifying, output some source information. */

if (main_verify && id != NULL)
  {
  xml_item *encoding = xml_find_item(id, US"encoding");
  if (encoding != NULL)
    {
    uschar *software = xml_get_string(encoding, US"software", NULL, FALSE);
    uschar *encoding_date = xml_get_string(encoding, US"encoding-date", NULL,
      FALSE);
    if (software != NULL || encoding_date != NULL)
      {
      if (software != NULL) fprintf(stderr, "MusicXML input by %s\n", software);
      if (encoding_date != NULL) fprintf(stderr, "MusicXML input date: %s\n",
        encoding_date);
      fprintf(stderr, "\n");
      }
    }
  }

/* Handle a number of general settings from the "defaults" element. */

if (defaults != NULL)
  {
  int system_margin_left = 0;
  int system_margin_right = 0;
  xml_item *scaling = xml_find_item(defaults, US"scaling");
  xml_item *lyric_font = xml_find_item(defaults, US"lyric-font");
  xml_item *page_layout = xml_find_item(defaults, US"page-layout");
  xml_item *staff_layout = xml_find_item(defaults, US"staff-layout");
  xml_item *system_layout = xml_find_item(defaults, US"system-layout");
  xml_item *word_font = xml_find_item(defaults, US"word-font");

  /* Must set magnification from <scaling> early because other things depend on
  it, but take it only from the first movement and before the first stave. */

  if (scaling != NULL && curmovt->number == 1 && curmovt->laststave < 1)
    {
    int mils = xml_get_mils(scaling, US"millimeters", 1000, 500000, 6000, TRUE);
    int tenths = xml_get_mils(scaling, US"tenths", 1000, 500000, 40000, TRUE);

    /* The height of an unmagnified staff in PMW is 16 points, which is
    16*0.3528 millimeters. The MXML staff height is 40 tenths, and the scaling
    values express how many millimeters that number of tenths should scale to.
    We need to calculate a PMW magnification factor from these figures. We work
    to 3 significant decimal places.

    The value (16*0.3528)/40 = 0.141 is the unmagnified value of one tenth in
    millimeters. Call this X. Then X times "tenths" is an unmagnified value
    which is to be scaled to "mils". The magnification is therefore
    "mils"/(X*"tenths"). Add this to any existing magnification. */

    main_magnification = mac_muldiv(main_magnification,
      mac_muldiv(mils, 1000, mac_muldiv(tenths, 141, 1000)), 1000);
    }

  /* Process system layout before page layout in order to set up any system
  margins, which are needed to set the PMW linelength value when processing
  page layout. Note that if page_layout is not set, system margins are ignored.
  */

  if (system_layout != NULL)
    {
    xml_item *system_margins = xml_find_item(system_layout, US"system-margins");
    int32_t system_distance = xml_get_number(system_layout, US"system-distance", 0,
      1000, -1, FALSE);
    top_system_distance = xml_get_number(system_layout, US"top-system-distance",
      0, 1000, -1, FALSE);

    if (system_margins != NULL)
      {
      system_margin_left = xml_get_number(system_margins, US"left-margin",
        0, 1000, 0, FALSE);
      system_margin_right = xml_get_number(system_margins, US"right-margin",
        0, 1000, 0, FALSE);
      }

     /* MusicXML system distance is from bottom on one system to top of
     another; PMW measures to the bottom of the first staff on the lower
     system, so we add 16 points. */

    if (system_distance >= 0) curmovt->systemgap = system_distance*400 + 16000;
    }

  /* Handle overall page layout settings */

  if (page_layout != NULL)
    {
    int32_t even_left = -1;
    int32_t even_right = -1;
    int32_t even_top = -1;
    int32_t even_bottom = -1;

    int32_t odd_left = -1;
    int32_t odd_right = -1;
    int32_t odd_top = -1;
    int32_t odd_bottom = -1;

    xml_item *page_margins = xml_find_item(page_layout, US"page-margins");
    page_height = xml_get_number(page_layout, US"page-height", 100, 10000,
        -1, TRUE);
    page_width = xml_get_number(page_layout, US"page-width", 100, 10000,
        -1, TRUE);

    for (page_margins = xml_find_item(page_layout, US"page-margins");
         page_margins != NULL;
         page_margins = xml_find_next(page_layout, page_margins))
      {
      int32_t left, right, top, bottom;
      uschar *type = xml_get_attr_string(page_margins, US"type", US"both",
        FALSE);

      left = xml_get_number(page_margins, US"left-margin", 0, 1000, -1,
        TRUE);
      right = xml_get_number(page_margins, US"right-margin", 0, 1000,
        -1, TRUE);
      top = xml_get_number(page_margins, US"top-margin", 0, 1000, -1,
        TRUE);
      bottom = xml_get_number(page_margins, US"bottom-margin", 0, 1000,
        -1, TRUE);

      if (Ustrcmp(type, "both") == 0)
        {
        even_left = odd_left = left;
        even_right = odd_right = right;
        even_top = odd_top = top;
        even_bottom = odd_bottom = bottom;
        }

      else if (Ustrcmp(type, "even") == 0)
        {
        even_left = left;
        even_right = right;
        even_top = top;
        even_bottom = bottom;
        }

      else if (Ustrcmp(type, "odd") == 0)
        {
        odd_left = left;
        odd_right = right;
        odd_top = top;
        odd_bottom = bottom;
        }

      else xml_Eerror(page_margins, ERR43, "page margins type", type);
      }

    /* PMW does not support separate odd/even margins. If both have been set
    the same, all is well. Otherwise, warn. */

    if (odd_left != even_left || odd_right != even_right ||
        odd_top != even_top || odd_bottom != even_bottom)
      xml_Eerror(page_layout, ERR38);  /* Warning */

    left_margin = odd_left;
    right_margin = odd_right;
    top_margin = odd_top;
    bottom_margin = odd_bottom;

    /* Insist on all data being present. In PMW, pagelength and linelength
    are not scaled by the magnification factor, but MusicXML tenths are so
    scaled, so we need to scale the values here. If either of them is
    sufficiently large, select A3. */

    if (page_height < 0 || page_width  < 0 || left_margin < 0 ||
        right_margin < 0 || top_margin < 0 || bottom_margin < 0)
      xml_Eerror(page_layout, ERR37);
    else
      {
      int32_t pagelength = (page_height - top_margin - bottom_margin) * 400;
      int32_t linelength = (page_width - left_margin - system_margin_left -
        right_margin - system_margin_right) * 400;

      pagelength = mac_muldiv(pagelength, main_magnification, 1000);
      linelength = mac_muldiv(linelength, main_magnification, 1000);

      if (linelength > 600000 || pagelength > 850000) main_sheetsize = sheet_A3;
      main_pagelength = pagelength;
      curmovt->linelength = linelength;
      }
    }

  if (staff_layout != NULL)
    {
    int32_t staff_distance = xml_get_number(staff_layout, US"staff-distance",
      0, 1000, -1, FALSE);
    if (staff_distance >= 0)
      for (int i = 1; i <= MAX_STAVE; i++)
        curmovt->stave_spacing[i] = staff_distance*400 + 16000;
    }

  if (word_font != NULL)
    {
    int32_t d = xml_get_attr_mils(word_font, US"font-size", 1000, 40000, -1,
      FALSE);
    if (d > 0) xml_fontsize_word_default = xml_pmw_fontsize(d);
    }

  /* Treat the font size as absolute; unscale it here because PMW will
  scale it. Set the depth to 1 point more than the font size. */

  if (lyric_font != NULL)
    {
    int32_t d = xml_get_attr_mils(lyric_font, US"font-size", 1000, 40000, -1,
      FALSE);
    if (d > 0)
      {
      fontinststr *fdata;
      if (!MFLAG(mf_copiedfontsizes))
        {
        fontsizestr *new = mem_get(sizeof(fontsizestr));
        *new = *(curmovt->fontsizes);
        curmovt->fontsizes = new;
        curmovt->flags |= mf_copiedfontsizes;
        }
      fdata = curmovt->fontsizes->fontsize_text + ff_offset_ulay;
      fdata->size = mac_muldiv(d, 1000, main_magnification);
      fdata->matrix = NULL;
      curmovt->underlaydepth = mac_muldiv(d + 1000, 1000, main_magnification);
      }
    }
  }

/* Handle groups of parts. If <group-symbol> was present, we must ensure the
default bracketing is disabled. */

if (xml_group_symbol_set) curmovt->bracketlist = NULL;

/* Scan groups and deal with brackets and braces. */

if (xml_groups_list != NULL)
  {
  xml_group_data *g;

  for (int n = 0; n <= xml_pmw_stave_count; n++) has_barline[n] = FALSE;

  for (g = xml_groups_list; g != NULL; g = g->next)
    {
    if (g->group_symbol != NULL)
      {
      stavelist **slp = NULL;
      stavelist *sl = mem_get(sizeof(stavelist));

      sl->first = g->first_pstave;
      sl->last = g->last_pstave;

      if (Ustrcmp(g->group_symbol, "bracket") == 0)
        slp = &(curmovt->bracketlist);
      else if (Ustrcmp(g->group_symbol, "brace") == 0)
        slp = &(curmovt->bracelist);

      if (slp != NULL)
        {
        sl->next = *slp;
        sl->prev = NULL;
        if (*slp != NULL) (*slp)->prev = sl;
        *slp = sl;
        }
      }

    if (g->group_barline != NULL)
      {
      if (Ustrcmp(g->group_barline, "yes") == 0)
      for (int n = g->first_pstave; n < g->last_pstave; n++)
        has_barline[n] = TRUE;
      }
    }
  }

/* No groups => full barlines by default, but assume barlines are broken when a
part has lyrics, in which case we don't want a full bar at the end either. */

else
  {
  for (int n = 0; n <= xml_pmw_stave_count; n++) has_barline[n] = TRUE;
  for (xml_part_data *p = xml_parts_list; p != NULL; p = p->next)
    {
    if (p->has_lyrics)
      for (int n = 0; n < p->stave_count; n++)
        has_barline[p->stave_first + n] = FALSE;
    }
  }

/* Note: use < instead of <= here so we don't specify a barline break after the
final stave. */

curmovt->breakbarlines = 0;
for (int n = 1; n < xml_pmw_stave_count; n++)
  {
  if (!has_barline[n]) curmovt->breakbarlines |= 1u << n;
  }

/* Handle layout data */

if (xml_layout_list != NULL && xml_layout_top > 0)
  {
  uint16_t *pl = curmovt->layout =
    mem_get((xml_layout_top+1)*2*sizeof(uint16_t));

  for (size_t ln = 0; ln < xml_layout_top; ln++)
    {
    if (xml_layout_list[ln] == 0xffu)
      {
      *pl++ = lv_newpage;
      }
    else
      {
      *pl++ = lv_barcount;
      *pl++ = xml_layout_list[ln];
      }
    }
  *pl++ = lv_repeatptr;
  *pl = 0;
  }

/* If no time signature, disable time signatures and checking */

if (xml_time_signature_seen < 0)
  curmovt->flags &= ~(mf_check|mf_showtime);
else
  if (xml_time_signature_seen > 1) curmovt->flags |= mf_startnotime;

/* No harm in setting suspend if there are non-printing bars at the start. It
will take effect only if the whole of the stave in the first system is
non-printing. */

for (xml_part_data *p = xml_parts_list; p != NULL; p = p->next)
  {
  if (p->noprint_before > 1)
    {
    for (int n = 0; n < p->stave_count; n++)
      curmovt->suspend_staves |= 1 << (p->stave_first + n);
    }
  }

/* Miscellaneous */

curmovt->justify = just_top|just_left|just_right;   /* MXL has positioning */
curmovt->flags |= mf_unfinished;             /* MXL has explicit bar lines */
curmovt->flags &= ~mf_keydoublebar;

/* Headings and footings. Some are directly under the score-* item. Keep track
of the depth of headings (in millipoints). Any existing footings are put after
XML footings. */

existing_footing = curmovt->footing;
curmovt->footing = NULL;

movement_title = xml_get_string(xml_partwise_item_list, US"movement-title",
  NULL, FALSE);

if (work != NULL)
  {
  work_title = xml_get_string(work, US"work-title", NULL, FALSE);
  work_number = xml_get_string(work, US"work-number", NULL, FALSE);
  }

/* Pick up data from identification in case not found elsewhere. */

if (id != NULL)
  {
  xml_item *creator;
  for (creator = xml_find_item(id, US"creator"); creator != NULL;
       creator = xml_find_next(id, creator))
    {
    uschar *text = xml_get_this_string(creator);
    xml_attrstr *typeattr = xml_find_attr(creator, US"type");

    if (typeattr != NULL)
      {
      uschar *type = typeattr->value;
      if (Ustrcmp(type, "composer") == 0)
        creator_composer = text;
      else if (Ustrcmp(type, "lyricist") == 0)
        creator_lyricist = text;
      else if (Ustrcmp(type, "arranger") == 0)
        creator_arranger = text;
      else if (Ustrcmp(type, "poet") == 0 ||
               Ustrcmp(type, "translator") == 0)
        xml_add_attrval_to_tree(&xml_ignored_element_tree, creator, typeattr);
      else xml_Eerror(creator, ERR52, US"creator type", type);
      }
    }

  id_rights = xml_get_string(id, US"rights", NULL, FALSE);
  }

/* Handle the <credit> element */

for (xml_item *credit = xml_find_item(xml_partwise_item_list, US"credit");
     credit != NULL;
     credit = xml_find_next(xml_partwise_item_list, credit))
  {
  uschar *pn = xml_get_attr_string(credit, US"page", US"", FALSE);

  if (Ustrcmp(pn, "1") == 0 || Ustrcmp(pn, "2") == 0)
    {
    xml_item *cr_words;
    int32_t font_size = 17000;
    int dy = -1;
    uschar *justify = US"left";
    uschar *halign = NULL;
    uschar *valign = US"top";
    uschar *font_weight = US"unset";
    uschar *cr_type = xml_get_string(credit, US"credit-type", US"", FALSE);

    for (cr_words = xml_find_item(credit, US"credit-words"); cr_words != NULL;
         cr_words = xml_find_next(credit, cr_words))
      {
      BOOL hadnl = FALSE;
      BOOL top_align;
      BOOL use_head;
      uint32_t font;
      int halign_index = 0;
      int32_t *headfootdepth;
      uschar *ss[3];
      uschar *text = xml_get_this_string(cr_words);
      headstr **anchor;
      size_t len = Ustrlen(text);
      size_t ln;

      ss[0] = ss[1] = ss[2] = US"";

      /* For multiple credit-words, the parameters carry over from earlier ones
      but can be overridden. */

      dy = xml_get_attr_number(cr_words, US"default-y", 0, 10000, dy, FALSE);
      font_size = xml_get_attr_mils(cr_words, US"font-size", 1000, 100000,
        font_size, FALSE);
      font_weight = xml_get_attr_string(cr_words, US"font-weight", font_weight,
        FALSE);
      justify = xml_get_attr_string(cr_words, US"justify", justify, FALSE);
      halign = xml_get_attr_string(cr_words, US"halign", halign, FALSE);
      valign = xml_get_attr_string(cr_words, US"valign", valign, FALSE);
      top_align = Ustrcmp(valign, "top") == 0;
      use_head = page_height < 0 || dy < 0 || dy > page_height/2;

      /* Ignore totally empty strings, except take note of newlines. Have to
      cope with UTF-8 encodings of space characters. Pro tem just check for
      U+00A0. */

      for (ln = 0; ln < len; ln++)
        {
        if (text[ln] >= 0x80)
          {
          if (text[ln] != 0xc2 || text[ln+1] != 0xa0) break;
          ln++;
          }
        else
          {
          if (text[ln] == '\n') hadnl = TRUE;
            else if (!isspace(text[ln])) break;
          }
        }

      if (ln >= len)
        {
        if (hadnl) dy = -1;  /* No attempt at up/down movement for next. */
        continue;
        }

      /* Check font weight */

      font = font_rm;

      if (Ustrcmp(font_weight, "bold") == 0) font = font_bf;
        else if (Ustrcmp(font_weight, "italic") == 0) font = font_it;

      /* Check alignment. MXL positions headings with explicit x/y coordinates.
      We guess how to set PMW left/centre/right alignment by looking at halign
      and justify, letting the former take precedence. */

      if (halign != NULL)
        {
        if (Ustrcmp(halign, "right") == 0) halign_index = 2;
          else if (Ustrcmp(halign, "center") == 0) halign_index = 1;
        }
      else
        {
        if (Ustrcmp(justify, "right") == 0) halign_index = 2;
          else if (Ustrcmp(justify, "center") == 0) halign_index = 1;
        }

      /* Treat the font size as absolute; unscale it here because PMW will
      scale it. */

      font_size = mac_muldiv(font_size, 1000, main_magnification);

      /* Main titles only from first page. If there is no credit-type, we have
      to make some guesses. */

      if (pn[0] == '1')
        {
        if (cr_type[0] == 0)  /* No type supplied */
          {
          if (creator_composer != NULL && Ustrcmp(text, creator_composer) == 0)
            done_composer = TRUE;
          else if (creator_lyricist != NULL && Ustrcmp(text, creator_lyricist)
            == 0) done_lyricist = TRUE;
          else if (creator_arranger != NULL && Ustrcmp(text, creator_arranger)
            == 0) done_arranger = TRUE;
          else if (work_number != NULL && Ustrcmp(text, work_number) == 0)
            done_work_number = TRUE;
          else if (id_rights != NULL && Ustrcmp(text, id_rights) == 0)
            done_rights = TRUE;

          /* To avoid mis-matches when there is a suffix such as "(Page 1)" on
          one of the strings, check only up to the shortest length. */

          else
            {
            if (movement_title != NULL)
              {
              size_t x = Ustrlen(movement_title);
              if (len < x) x = len;
              if (Ustrncmp(text, movement_title, x) == 0) done_title = TRUE;
              }
            if (work_title != NULL)
              {
              size_t x = Ustrlen(work_title);
              if (len < x) x = len;
              if (Ustrncmp(text, work_title, x) == 0) done_title = TRUE;
              }
            }
          }

        else if (Ustrcmp(cr_type, "title") == 0)
          done_title = TRUE;
        else if (Ustrcmp(cr_type, "composer") == 0)
          done_composer = TRUE;
        else if (Ustrcmp(cr_type, "rights") == 0)
          done_rights = TRUE;
        else if (Ustrcmp(cr_type, "arranger") == 0)
          done_arranger = TRUE;
        else if (Ustrcmp(cr_type, "lyricist") == 0)
          done_lyricist = TRUE;

        if (use_head)
          {
          if (dy > 0)
            {
            int32_t down = 0;
            int32_t dymils = dy*400;
            if (heading_base < 0)
              {
              heading_base = dymils;
              if (top_align) heading_depth += font_size;
              }
            else
              {
              down = heading_base - heading_depth - dymils;
              if (top_align) down += font_size;
              }

            if (down != 0)
              heading_depth += handle_headfoot(US"", US"", US"",
                &(curmovt->heading), down, font_rm);
            }
          headfootdepth = &heading_depth;
          anchor = &(curmovt->heading);
          }

        /* PMW starts its footings 20 points below the bottom margin. MXL
        examples specify y positions above the bottom of the page. */

        else
          {
          if (dy > 0 && bottom_margin > 0)
            {
            int dymils = dy*400;
            int bmmils = bottom_margin*400;
            int down = bmmils - footing_depth - dymils;
            if (top_align) down += font_size;

            if (down != 0)
              footing_depth += handle_headfoot(US"", US"", US"",
                &(curmovt->footing), down, font_rm);
            }
          headfootdepth = &footing_depth;
          anchor = &(curmovt->footing);
          }

        /* Newlines in the string do mean new lines, so split up the text if
        necessary. */

        for (;;)
          {
          uschar *nl = Ustrchr(text, '\n');
          if (nl != NULL) *nl = 0;
          ss[halign_index] = text;
          *headfootdepth += handle_headfoot(ss[0], ss[1], ss[2], anchor,
            font_size, font);
          if (nl == NULL) break;
          *nl = '\n';
          text = nl + 1;
          }
        }

      /* Page number from either page 1 or 2. We have to check for the page
      number after conversion to a PMW string and replace it with the special
      "pagenumber" value. */

      if (Ustrcmp(cr_type, "page number") == 0)
        {
        headstr *p;
        anchor = use_head? &(curmovt->pageheading) : &(curmovt->pagefooting);
        ss[halign_index] = text;
        (void)handle_headfoot(ss[0], ss[1], ss[2], anchor, font_size, font);
        for (p = *anchor; p->next != NULL; p = p->next) continue;
        for (uint32_t *pp = p->string[halign_index]; *pp != 0; pp++)
          {
          if (PCHAR(*pp) == pn[0]) *pp = PFONT(*pp) | ss_page;
          }
        }
      }
    }

  else xml_Eerror(credit, ERR50);
  }

/* If not output via <credit> elements, generate titles from others. */

if (!done_title)
  {
  if (work_title != NULL)
    {
    heading_depth +=
      handle_headfoot(US"", work_title, US"", &(curmovt->heading), 0, font_rm);
    if (!done_work_number && work_number != NULL) heading_depth +=
      handle_headfoot(US"", work_number, US"", &(curmovt->heading), 0, font_rm);
    }

  if (movement_title != NULL) heading_depth +=
    handle_headfoot(US"", movement_title, US"", &(curmovt->heading), 0, font_rm);
  }

if ((!done_composer && creator_composer != NULL) ||
    (!done_lyricist && creator_lyricist != NULL))
  heading_depth += handle_headfoot(
    (creator_lyricist != NULL)? creator_lyricist : US"",
    US"",
    (creator_composer != NULL)? creator_composer : US"",
    &(curmovt->heading), 0, font_rm);

if (!done_arranger && creator_arranger != NULL)
  heading_depth +=
    handle_headfoot(US"", US"", creator_arranger, &(curmovt->heading), 0, font_rm);

if (!done_rights && id_rights != NULL)
  (void)handle_headfoot(US"", US"", id_rights, &(curmovt->footing), 8000, font_rm);

/* Restore any previously-existing footings (added externally). */

if (existing_footing != NULL)
  {
  headstr **ftptr;
  for (ftptr = &(curmovt->footing); *ftptr != NULL; ftptr = &((*ftptr)->next)) {}
  *ftptr = existing_footing;
  }

/* MusicXML's top system distance is defined as being from the top margin to
the top of the first system. See if this is different to the distance implied
by any headings. The value in first_system_distance (set in the "analyXe"
module) is a "top-system-distance" in the first bar. If greater, this overrides
the global setting. We use "different" rather than "greater" when testing
against the heading depth because some examples have positioned text at the
bottom of the page using "top" alignment (extraordinarily), thus requiring an
upward (negative) heading movement. */

if (xml_first_system_distance > top_system_distance)
  top_system_distance = xml_first_system_distance;

/* PMW automatically starts the top staff 17pts down from the end of the
headings, to allow for a stave depth. Adjust the spacing in the final heading,
or create a blank one if necessary. */

if (top_system_distance > 0)
  {
  top_system_distance *= 400;
  if (top_system_distance != heading_depth)
    {
    int32_t adjust = top_system_distance - heading_depth;
    if (curmovt->heading != NULL)
      {
      headstr *p;
      for (p = curmovt->heading; p->next != NULL; p = p->next) continue;
      p->space += adjust;
      }
    else
      {
      (void)handle_headfoot(US"", US"", US"", &(curmovt->heading), adjust,
        font_rm);
      }
    }
  }
}

/* End of xml_heading.c */
