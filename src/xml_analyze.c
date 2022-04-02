/*************************************************
*            MusicXML input for PMW              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */
/* File last modified: April 2022 */

/* Analyse XML after it has been read into memory. */

#include "pmw.h"


/*************************************************
*              Analyze entry point               *
*************************************************/

BOOL
xml_analyze(void)
{
int pmw_stave = 1;
xml_item *mi, *part_list;
xml_part_data **pdlink = &xml_parts_list;
BOOL yield = TRUE;

/* Skip over initial housekeeping items (whose names start with '#') at the
start before seeking a partwise item list. */

for (mi = xml_main_item_list; mi != NULL && mi->name[0] == '#'; mi = mi->next)
  {
  continue;
  }
if (mi != NULL) xml_partwise_item_list = xml_find_item(mi, US"score-partwise");
if (xml_partwise_item_list == NULL) xml_error(ERR16);   /* Hard */

/* Process parts list */

part_list = xml_find_item(xml_partwise_item_list, US"part-list");
if (part_list == NULL) xml_error(ERR17, "<part-list>");  /* Hard */

for (mi = part_list->next; mi != part_list->partner; mi = mi->partner->next)
  {
  int layout_next = 0;
  int system_start_measure = 1;
  BOOL layout_new = (xml_layout_list == NULL);

  /* Handle information about a group of parts. Group numbers are re-usable,
  being just used to handle nested groups. Therefore, build the chain with the
  most recent first. However, we want to process it in reverse order (PMW stave
  order) so maintain a double chain. */

  if (Ustrcmp(mi->name, US"part-group") == 0)
    {
    int number = xml_get_attr_number(mi, US"number", 1, 10, 1, TRUE);

    if (ISATTR(mi, "type", "start", TRUE, "start"))
      {
      xml_group_data *new = mem_get(sizeof(xml_group_data));
      new->next = xml_groups_list;
      new->prev = NULL;
      if (xml_groups_list != NULL) xml_groups_list->prev = new;
      xml_groups_list = new;
      new->number = number;
      new->first_pstave = pmw_stave;
      new->last_pstave = -1;  /* Unset */
      new->group_barline = xml_get_string(mi, US"group-barline", NULL,
        FALSE);
      new->group_symbol = xml_get_string(mi, US"group-symbol", NULL,
        FALSE);
      xml_group_symbol_set = new->group_symbol != NULL;

      /* Handle a group name */

      new->name = xml_find_item(mi, US"group-name");
      new->name_display = xml_find_item(mi, US"group-name-display");
      new->abbreviation = xml_find_item(mi, US"group-abbreviation");
      new->abbreviation_display = xml_find_item(mi, US"group-abbreviation-display");
      }

    else
      {
      xml_group_data *group;
      for (group = xml_groups_list; group != NULL; group = group->next)
        {
        if (group->number == number) break;
        }
      if (group == NULL || group->last_pstave >= 0)
        xml_Eerror(mi, ERR36, number);
          else group->last_pstave = pmw_stave - 1;
      }
    }  /* End of handling <part-group> */

  /* Handle information about a part */

  else if (Ustrcmp(mi->name, US"score-part") == 0)
    {
    xml_attrstr *p;
    xml_item *measure, *part;
    xml_part_data *new = mem_get(sizeof(xml_part_data));

    *pdlink = new;
    pdlink = &(new->next);
    new->next = NULL;

    new->stave_count = 1;
    new->stave_first = pmw_stave;
    new->has_lyrics = FALSE;
    new->noprint_before = 0;

    /* Handle a part name */

    new->name = xml_find_item(mi, US"part-name");
    new->name_display = xml_find_item(mi, US"part-name-display");
    new->abbreviation = xml_find_item(mi, US"part-abbreviation");
    new->abbreviation_display = xml_find_item(mi, US"part-abbreviation-display");

    /* Record the part id */

    p = xml_find_attr(mi, US"id");
    if (p == NULL)
      {
      xml_Eerror(mi, ERR19, "id");
      new->id = NULL;
      new->part = NULL;
      }
    else
      {
      BOOL noprint = FALSE;

      new->id = p->value;

      part = xml_find_part(xml_partwise_item_list, new->id);
      if (part == NULL) xml_Eerror(part_list, ERR20, new->id);    /* Hard */
      new->part = part;
      new->measure_count = 0;

      /* The only way of finding out how many staves this part uses is to scan
      all its measures. At the same time, we do some additional analysis:

      1. Scan each measure's notes for chords, because the PMW needs to know
      their start and end and the scan is better done here than later.

      2. Note whether there are any lyrics.

      3. Check for print-object="no" settings in "staff-details", which we use
      to insert a "pmw-suspend" item.

      4. Handle relative staff sizing.

      5. If there are "print new-system" and/or "print new-page" items,
      construct or check a previously constructed layout list.

      6. For the first system in the first part, extract the top system
      distance, for use when processing headings. */

      for (measure = xml_find_item(part, US"measure");
           measure != NULL;
           measure = xml_find_next(part, measure))
        {
        xml_item *note, *attributes;
        xml_item *print = xml_find_item(measure, US"print");

        new->measure_count++;

        if (print != NULL)
          {
          BOOL new_page = ISATTR(print, "new-page", "no", FALSE, "yes");
          BOOL new_system = ISATTR(print, "new-system", "no", FALSE, "yes");
          int prevsyscount = new->measure_count - system_start_measure;

          if ((new_system || new_page) && prevsyscount > 0)
            {
            system_start_measure = new->measure_count;

            if (layout_new)  /* The first part to do layout */
              {
              if (xml_layout_top >= xml_layout_list_size)
                {
                xml_layout_list_size += LAYOUTLISTMIN;
                xml_layout_list = realloc(xml_layout_list, xml_layout_list_size);
                if (xml_layout_list == NULL)  /* Hard error */
                  error(ERR0, "re-", "XML layout list", xml_layout_list_size);
                }
              xml_layout_list[xml_layout_top++] = prevsyscount;
              if (new_page) xml_layout_list[xml_layout_top++] = 0xffu;
              }

            else  /* Check that it matches previous parts */
              {
              if (xml_layout_list[layout_next++] != prevsyscount ||
                   (new_page && xml_layout_list[layout_next++] != 0xffu))
                xml_Eerror(print, ERR51);
              }
            }

          /* Take note of <top-system-distance> if this is the first bar; it is
          used to put optional space after the headings. We can't do much about
          this element if it appears elsewhere. Note that this need not occur
          in the first part (and doesn't in some samples when the top stave
          starts off suppressed). */

          if (new->measure_count == 1)
            {
            xml_item *system_layout = xml_find_item(print, US"system-layout");
            if (system_layout != NULL)
              xml_first_system_distance = xml_get_number(system_layout,
                US"top-system-distance", 0, 1000, -1, FALSE);
            }
          }

        /* It seems that <attributes> may appear more than once; it does so in
        one of the samples. */

        for (attributes = xml_find_item(measure, US"attributes");
             attributes != NULL;
             attributes = xml_find_next(measure, attributes))
          {
          xml_item *staff_details;
          int stave_count = xml_get_number(attributes, US"staves", 1, 10, 1,
            FALSE);
          if (stave_count > PARTSTAFFMAX) xml_Eerror(attributes, ERR29, stave_count);
          if (stave_count > new->stave_count) new->stave_count = stave_count;
          if (xml_find_item(attributes, US"time") != NULL &&
            xml_time_signature_seen < 0) xml_time_signature_seen = new->measure_count;

          /* It seems that <staff-details> may appear more than once; it does
          so in one of the samples. */

          for (staff_details = xml_find_item(attributes, US"staff-details");
               staff_details != NULL;
               staff_details = xml_find_next(attributes, staff_details))
            {
            uschar *number_string = xml_get_attr_string(staff_details,
              US"number", NULL, FALSE);
            uschar *print_object = xml_get_attr_string(staff_details,
              US"print-object", NULL, FALSE);
            int staff_size = xml_get_number(staff_details, US"staff-size",
              10, 1000, -1, FALSE);

            /* If there is no "print-object" setting, assume the same as was
            previously set. */

            if (print_object != NULL)
              {
              noprint = Ustrcmp(print_object, "no") == 0;
              if (noprint)
                {
                xml_item *prev_measure = xml_find_prev(part, measure);

                if (prev_measure != NULL)
                  {
                  xml_item *pmw_suspend = xml_new_item(US"pmw-suspend");
                  xml_insert_item(pmw_suspend, prev_measure->partner);

                  if (number_string != NULL)
                    {
                    xml_attrstr *newattr = mem_get(sizeof(xml_attrstr) +
                      Ustrlen(number_string));
                    Ustrcpy(newattr->name, "number");
                    Ustrcpy(newattr->value, number_string);
                    newattr->next = NULL;
                    pmw_suspend->p.attr = newattr;
                    }
                  }
                }
              }

            if (staff_size > 0)
              {
              int a, b;

              staff_size *= 10;
              xml_set_stave_size = TRUE;

              if (number_string != NULL)
                {
                a = pmw_stave + xml_string_to_number(number_string, FALSE);
                b = a;
                }
              else
                {
                a = pmw_stave;
                b = a + stave_count - 1;
                }

              for (; a <= b; a++)
                {
                if (xml_stave_sizes[a] < 0)
                  xml_stave_sizes[a] = staff_size;
                else if (xml_stave_sizes[a] != staff_size)
                  xml_Eerror(staff_details, ERR47);
                }
              }
            }
          }

        /* Scan the notes of a measure. If any lyrics are found, set the flag
        in the part, and also arrange to turn off the full barline at the end
        of systems. */

        for (note = xml_find_item(measure, US"note");
             note != NULL;
             note = xml_find_next(measure, note))
          {
          if (!new->has_lyrics && xml_find_item(note, US"lyric") != NULL)
            {
            new->has_lyrics = TRUE;
            xml_movt_unsetflags = mf_fullbarend;
            }

          /* When we hit one that has <chord>, we identify the start of the
          chord, which is the previous note. Then find the end, identify that,
          and skip on to the end. */

          if (xml_find_item(note, US"chord") != NULL)
            {
            int staff = xml_get_number(note, US"staff", 0, 12, -1, FALSE);
            xml_item *pnote = xml_find_prev(measure, note);

            /* Insert "pmw-chord-first" after the first note, then scan for the
            last note and give it "pmw-chord-last". If the notes are on
            different staves, arrange for PMW coupling and adjust the staff
            numbers to be all the same as the first note. */

            if (pnote == NULL) xml_Eerror(note, ERR27); else
              {
              int pstaff = xml_get_number(pnote, US"staff", 0, 12, -1, FALSE);
              BOOL setcouple = (staff != pstaff);

              xml_insert_item(xml_new_item(US"pmw-chord-first"), pnote->next);
              for(;;)
                {
                xml_item *next = xml_find_next(measure, note);
                if (next == NULL || xml_find_item(next, US"chord") == NULL)
                  break;
                if (xml_get_number(next, US"staff", 0, 12, -1, FALSE) != pstaff)
                  setcouple = TRUE;
                note = next;
                }
              xml_insert_item(xml_new_item(US"pmw-chord-last"), note->next);

              if (setcouple)
                {
                if (new->stave_count > 2) xml_Eerror(note, ERR58); else
                  {
                  xml_item *next;
                  if (pstaff == 1) xml_couple_settings[pmw_stave] = COUPLE_DOWN;
                    else xml_couple_settings[pmw_stave+1] = COUPLE_UP;

                  for (next = xml_find_next(measure, pnote);
                       next != note; next = xml_find_next(measure, next))
                    xml_set_number(next, US"staff", pstaff);
                  xml_set_number(note, US"staff", pstaff);
                  }
                }
              }
            }
          }  /* End of note scan */

        /* Keep track of the first measure that is marked as printing when the
        part starts with non-printing measures. */

        if (noprint && new->measure_count - 1 == new->noprint_before)
          new->noprint_before++;

        }    /* End of measure scan */

      /* If system/page layout is set, add or check the final count. */

      if (xml_layout_list != NULL)
        {
        int prevsyscount = new->measure_count - system_start_measure + 1;
        if (layout_new)  /* The first part to do layout */
          {
          if (xml_layout_top >= xml_layout_list_size)
            {
            xml_layout_list_size += LAYOUTLISTMIN;
            xml_layout_list = realloc(xml_layout_list, xml_layout_list_size);
            if (xml_layout_list == NULL)  /* Hard error */
              error(ERR0, "re-", "XML layout list", xml_layout_list_size);
            }
          xml_layout_list[xml_layout_top++] = prevsyscount;
          }

        else  /* Check that it matches previous parts */
          {
          if (xml_layout_list[layout_next++] != prevsyscount)
            xml_Eerror(mi, ERR51);
          }
        }
      }

    /* If the part uses more than one stave, insert a brace group for it if
    there isn't an unclosed group starting at this stave. */

    if (new->stave_count > 1)
      {
      xml_group_data *group;

      for (group = xml_groups_list; group != NULL; group = group->next)
        {
        if (group->first_pstave == new->stave_first &&
            group->last_pstave < 0)
          break;
        }

      if (group == NULL)
        {
        group = mem_get(sizeof(xml_group_data));
        group->next = xml_groups_list;
        group->prev = NULL;
        if (xml_groups_list != NULL) xml_groups_list->prev = group;
        xml_groups_list = group;
        group->number = 99;
        group->name = group->name_display = group->abbreviation =
          group->abbreviation_display = NULL;
        group->first_pstave = pmw_stave;
        group->last_pstave = pmw_stave + new->stave_count - 1;
        group->group_barline = US"yes";
        group->group_symbol = US"brace";
        xml_group_symbol_set = TRUE;
        }
      }

    /* Keep track of PMW stave number */

    pmw_stave += new->stave_count;

    }  /* End of handling <score-part> */
  }  /* End of scan of items under <part-list> */

if (pmw_stave > 64) xml_error(ERR21, pmw_stave - 1);
xml_pmw_stave_count = pmw_stave - 1;

DEBUG(D_xmlgroups|D_xmlanalyze)
  {
  xml_part_data *p;
  xml_group_data *g;

  eprintf("Found parts:\n");
  for (p = xml_parts_list; p != NULL; p = p->next)
    {
    eprintf("  %d/%d [%s]", p->stave_count, p->stave_first, p->id);
    if (p->name == NULL) eprintf(" <No name>\n");
      else eprintf(" %s\n", p->name->name);
    }

  eprintf("Found groups:\n");
  for (g = xml_groups_list; g != NULL; g = g->next)
    {
    eprintf("  %d %d-%d %s %s\n", g->number, g->first_pstave,
      g->last_pstave, g->group_symbol, g->group_barline);
    }

  DEBUG(D_xmlanalyze)
    xml_debug_print_item_list(xml_main_item_list, "after analyzing");
  }

return yield;
}

/* End of xml_analyze.c */
