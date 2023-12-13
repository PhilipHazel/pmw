/*************************************************
*            PMW debugging functions             *
*************************************************/

/* Copyright Philip Hazel 2022 */
/* This file created: December 2020 */
/* This file last modified: December 2023 */

#include "pmw.h"

static const char *acnames[] = { "", "%", "#-", "#", "##", "$-", "$", "$$" };

static const char *absnamess = "C\0..C#-\0C#\0.D$-\0"
                               "D\0..D#-\0D#\0.E$-\0"
                               "E\0..E#-\0"
                               "F\0..F#-\0F#\0.G$-\0"
                               "G\0..G#-\0G#\0.A$-\0"
                               "A\0..A#-\0A#\0.B$-\0"
                               "B\0..B#-";

static const char *absnamesf = "C\0..C#-\0D$\0.D$-\0"
                               "D\0..D#-\0E$\0.E$-\0"
                               "E\0..F$-\0"
                               "F\0..F#-\0G$\0.G$-\0"
                               "G\0..G#-\0A$\0.A$-\0"
                               "A\0..A#-\0B$\0.B$-\0"
                               "B\0..C$-";

static const char *noteflags[] =
  { "++", "+", "", "", "-", "=", "=-", "==", "==-", "===" };

static const char *fonttype_names[] = {
  "roman", "italic", "bold", "bold italic", "symbol", "music" };

static const char *stemswaptype[] = {
  "default", "up", "down", "left", "right" };

#define fp  (void)fprintf
#define sfk string_format_key



/*************************************************
*          Output a set of slashed flags         *
*************************************************/

static void
debug_flags(uint32_t flags, const char *s)
{
while (*s != 0 && flags != 0)
  {
  const char *ss = s;
  while (*s != 0 && *s != ',') s++;
  if ((flags & 1) != 0) eprintf("/%.*s", (int)(s - ss), ss);
  if (*s == ',') s++;
  flags >>= 1;
  }
if (flags != 0) eprintf("/LEFTOVER=0x%x", flags);
}



/*************************************************
*          Output font size information          *
*************************************************/

/* Note that string_format_fixed() aka sff() allows up to 5 simultaneous
outputs to exist. The reconstituted stretch and shear often end up as x.y99
when the original had just one decimal place, so we round them up. */

static void
debug_fontsize(const char *s, fontinststr *fdata)
{
eprintf("    %s %s", s, sff(fdata->size));
if (fdata->matrix != NULL)
  {
  int32_t stretch = mac_muldiv(fdata->matrix[0], 1000, 65536);
  int32_t shear = (int32_t)(
    atan((double)(fdata->matrix[2])/65536.0)/atan(1.0)*45000);
  if ((stretch + 1) % 100 == 0) stretch++;
  if ((shear + 1) % 100 == 0) shear++;
  eprintf("/%s/%s", sff(stretch), sff(shear));
  eprintf("  %s  %s", sff(fdata->matrix[0]), sff(fdata->matrix[2]));
  }
eprintf("\n");
}



/*************************************************
*           Output font type information         *
*************************************************/

static void
debug_fonttype(const char *s, uint8_t type)
{
if (type < font_xx)
  eprintf("  %s = %s\n", s, fonttype_names[type]);
else
  eprintf("  %s = extra %d\n", s, type - font_xx + 1);
}



/*************************************************
*            Output a PMW string                 *
*************************************************/

/* This is global so that it can be called from debugging code in other
modules. */

void
debug_string(uint32_t *p)
{
uint32_t sfont = font_unknown;

eprintf("\"");
for (uint32_t c = *p; c != 0; c = *(++p))
  {
  uint32_t font = PFONT(c);
  c = PCHAR(c);

  if (font != sfont)
    {
    sfont = font;
    if (font >= font_small)
      {
      font -= font_small;
      if (font == font_mf) eprintf("\\mu\\");
        else eprintf("\\%s\\\\sc\\", font_IdStrings[font]);
      }
    else eprintf("\\%s\\", font_IdStrings[font]);
    }

  if (c <= MAX_UNICODE)
    {
    if (c == '|' || c == '"') eprintf("\\%c", c);
    else if (c >= ' ' && c <= 126) eprintf("%c", c);
    else eprintf("\\x%x\\", c);
    }

  else switch(c)
    {
    case ss_page: eprintf("\\p\\"); break;
    case ss_pageeven: eprintf("\\pe\\"); break;
    case ss_pageodd:  eprintf("\\po\\"); break;
    case ss_skipeven: eprintf("\\se\\"); break;
    case ss_skipodd:  eprintf("\\so\\"); break;
    case ss_repeatnumber: eprintf("\\r\\"); break;
    case ss_repeatnumber2: eprintf("\\r2\\"); break;
    case ss_verticalbar: eprintf("|"); break;
    case ss_escapedhyphen: eprintf("\\-"); break;
    case ss_escapedequals: eprintf("\\="); break;
    case ss_escapedsharp: eprintf("\\#"); break;

    default:
    eprintf("\\UNKNOWN %06x\\", c);
    break;
    }
  }

eprintf("\"");
}



/*************************************************
*            Output a draw call                  *
*************************************************/

static void
debug_draw(tree_node *drawing, drawitem *drawargs, const char *s)
{
eprintf("%sdraw", s);
if (drawargs != NULL)
  {
  int i;
  drawitem *d = drawargs;
  for (i = 1; i <= d->d.val; i++)
    {
    drawitem *dd = d + i;
    if (dd->dtype == dd_number)
      {
      eprintf(" %s", sff(dd->d.val));
      }
    else
      {
      drawtextstr *dt = dd->d.ptr;
      eprintf(" ");
      debug_string(dt->text);
      if (dt->rotate != 0) eprintf("/rot%s", sff(dt->rotate));
      if (dt->xdelta != 0) eprintf("/r%s", sff(dt->xdelta));
      if (dt->ydelta != 0) eprintf("/u%s", sff(dt->ydelta));
      if (dt->size != 0) eprintf("/s%d", dt->size);
      if (dt->flags != 0) eprintf("/flags=0x%x", dt->flags);
      }
    }
  }
eprintf(" %s", drawing->name);
}



/*************************************************
*           Output heading/footing               *
*************************************************/

static void
debug_heading(const char *name, headstr *hd)
{
if (hd == NULL) return;

for (; hd != NULL; hd = hd->next)
  {
  if (hd->drawing != NULL)
    {
    eprintf("  %s", name);
    debug_draw(hd->drawing, hd->drawargs, "  ");
    eprintf(" %s", sff(hd->space));
    }

  else
    {
    const char *sep = "";
    eprintf("  %s = %s", name, sff(hd->fdata.size));
    if (hd->fdata.matrix != NULL)
      {
      int32_t stretch = mac_muldiv(hd->fdata.matrix[0], 1000, 65536);
      int32_t shear = (int32_t)(
        atan((double)(hd->fdata.matrix[2])/65536.0)/atan(1.0)*45000);
      eprintf("/%s/%s", sff(stretch), sff(shear));
      }
    eprintf(" ");
    for (int i = 0; i < 3; i++)
      {
      if (hd->string[i] != NULL)
        {
        eprintf("%s", sep);
        debug_string(hd->string[i]);
        sep = " | ";
        }
      }
    eprintf(" %s", sff(hd->space));

    if (hd->fdata.spacestretch != 0) eprintf(" spacestretch=%s",
      sff(hd->fdata.spacestretch));
    if (hd->spaceabove != 0) eprintf(" above=%s", sff(hd->spaceabove));
    }

  eprintf("\n");
  }
}



/*************************************************
*           Output a moving dimension            *
*************************************************/

static void
debug_move(int32_t x, const char *p, const char *m)
{
if (x != 0) eprintf("/%s%s", (x >= 0)? p : m, sff(abs(x)));
}



/*************************************************
*              Output text flags                 *
*************************************************/

typedef struct {
  uint32_t bit;
  const char *name;
} textflag_name;

static textflag_name textflags[] = {
  { text_above,     "above" },
  { text_absolute,  "absolute" },
  { text_atulevel,  "atulevel" },
  { text_baralign,  "baralign" },
  { text_barcentre, "barcentre" },
  { text_boxed,     "boxed" },
  { text_boxrounded,"boxrounded" },
  { text_centre,    "centre" },
  { text_endalign,  "endalign" },
  { text_fb,        "fb" },
  { text_followon,  "followon" },
  { text_middle,    "middle" },
  { text_rehearse,  "rehearse" },
  { text_ringed,    "ringed" },
  { text_timealign, "timealign" },
  { text_ul,        "ul" }
};

static void
debug_textflags(uint32_t flags)
{
usint i;
if (flags == 0) eprintf(" none");
else for (i = 0; i < sizeof(textflags)/sizeof(textflag_name); i++)
  {
  textflag_name *n = textflags + i;
  if ((flags & n->bit) != 0) eprintf(" %s", n->name);
  }
}



/*************************************************
*            Output a stave string               *
*************************************************/

static void
debug_stavestring(b_textstr *t)
{
eprintf("  ");
debug_string(t->string);

debug_move(t->x, "r", "l");
debug_move(t->offset, "rc", "lc");
debug_move(t->y, "u", "d");
if (t->rotate != 0) eprintf("/rot%s", sff(t->rotate));
if (t->halfway != 0) eprintf("/h%s", sff(t->halfway));
if (t->size > 0) eprintf("/s%d", t->size + 1);
if (t->htype != 0) eprintf(" htype=%d", t->htype);
if (t->laylevel != 0) eprintf(" laylevel=%d", t->laylevel);
if (t->laylen != 0) eprintf(" laylen=%d", t->laylen);

eprintf(" flags =");
debug_textflags(t->flags);
eprintf("\n");
}



/*************************************************
*              Output stave bitmap               *
*************************************************/

static void
debug_stavemap(const char *s, uint64_t bits)
{
eprintf("  %s =", s);
if (bits == 0) eprintf(" none");
else if (bits == ~0ul) eprintf(" all");
else
  {
  usint start, first, last;
  for (start = 0;; start = last + 1)
    {
    if (!misc_get_range(bits, start, &first, &last)) break;
    eprintf(" %d", first);
    if (last != first) eprintf("-%d", last);
    }
  }
eprintf("\n");
}


/*************************************************
*              Output stave list                 *
*************************************************/

static void
debug_stavelist(const char *s, stavelist *sl)
{
eprintf("  %s =", s);
if (sl == NULL) eprintf(" none");
else do
  {
  eprintf(" %d", sl->first);
  if (sl->last != sl->first) eprintf("-%d", sl->last);
  sl = sl->next;
  }
while (sl != NULL);
eprintf("\n");
}


/*************************************************
*             Output time signature              *
*************************************************/

static void
debug_time(const char *s, uint32_t ts, const char *t)
{
if (s != NULL) eprintf("  %s = ", s);
eprintf("%d * ", ts >> 16);
ts &= 0x0000ffffu;
if (ts == time_common) eprintf("C");
else if (ts == time_cut) eprintf("A");
else eprintf("%d/%d", (ts >> 8) & 0xff, ts & 0xff);
eprintf("%s", t);
}



/*************************************************
*          Output layout information             *
*************************************************/

static void
debug_layout(uint16_t *p)
{
const char *sp = " ";
eprintf("  layout =");
if (p == NULL) { eprintf(" none\n"); return; }

for (;;)
  {
  switch (*p++)
    {
    case lv_barcount:
    eprintf("%s%d", sp, *p++);
    sp = ", ";
    break;

    case lv_repeatcount:
    eprintf(" %d(", *p++);
    sp = "";
    break;

    case lv_repeatptr:
    if (*p++ == 0)
      {
      eprintf("\n");
      return;
      }
    eprintf(")");
    sp = " ";
    break;

    case lv_newpage:
    eprintf(";");
    sp = " ";
    break;

    default:
    error(ERR47);  /* Hard */
    break;
    }
  }
}



/*************************************************
*            Output header information           *
*************************************************/

void
debug_header(void)
{
usint i;
sheetstr *sh;
pkeystr *pk;
ptimestr *pt;
keytransstr *kt;
trkeystr *tk;

eprintf("\nGLOBAL VALUES\n");

for (i = 0; i < font_count; i++)
  eprintf("  font %d: %s\n", i, font_list[i].name);

for (i = 0; i < font_xx; i++)
  eprintf("    %s = %d\n", font_IdStrings[i], font_table[i]);

for (i = font_xx; i < font_tablen; i++)
  if (font_table[i] != 0)
    eprintf("    %s = %d\n", font_IdStrings[i], font_table[i]);

for (i = 0; i < MAX_XKEYS; i++)
  {
  uint8_t *kp = &(keysigtable[key_X + i][0]);
  if (*kp == ks_end) continue;
  eprintf("  custom key X%d =", i+1);
  while (*kp != ks_end)
    {
    uint8_t k = *kp++;
    eprintf(" %s%d", acnames[k>>4], (k & 0x0f) - 1);
    }
  eprintf("\n");
  }

eprintf("  kerning = %s\n", main_kerning? "true" : "false");

for (kt = main_keytranspose; kt != NULL; kt = kt->next)
  {
  eprintf("  keytranspose (quarter tones) %s", sfk(kt->oldkey));
  for (i = 0; i < 24; i++)
    {
    if (kt->newkeys[i] != KEY_UNSET)
      {
      int j = i;
      eprintf(" %d=%s/%d", j, sfk(kt->newkeys[i]), kt->letterchanges[i]);
      }
    }
  eprintf("\n");
  }

eprintf("  landscape = %s\n", main_landscape? "true" : "false");
eprintf("  magnification = %s\n", sff(main_magnification));
eprintf("  maxvertjustify = %s\n", sff(main_maxvertjustify));
eprintf("  midifornotesoff = %s\n", main_midifornotesoff? "true" : "false");

eprintf("  note spacing =");
for (i = 0; i < NOTETYPE_COUNT; i++)
  eprintf(" %s", sff(read_absnotespacing[i]));
eprintf("\n");

eprintf("  page = %d %d\n", page_firstnumber, page_increment);
eprintf("  pagelength = %s\n", sff(main_pagelength));
eprintf("  righttoleft = %s\n", main_righttoleft? "true" : "false");
eprintf("  sheetdepth = %s\n", sff(main_sheetdepth));

for (i = 0; i < sheets_count; i++)
  {
  sh = sheets_list + i;
  if (main_sheetsize == sh->sheetsize) break;
  }
if (i < sheets_count) eprintf("  sheetsize = %s\n", sh->name);
  else eprintf("  sheetsize = unknown\n");

eprintf("  sheetwidth = %s\n", sff(main_sheetwidth));
eprintf("  transposedacc = %s\n", main_transposedaccforce? "force" : "noforce");

for (tk = main_transposedkeys; tk != NULL; tk = tk->next)
  {
  eprintf("  transposed key %s use ", sfk(tk->oldkey));
  eprintf("%s\n", sfk(tk->newkey));
  }

DEBUG(D_header_glob) return;  /* Just want globals */

/* Show header information for each movement */

for (i = 0; i < movement_count; i++)
  {
  usint j, k, spptr, spmax;
  int32_t spmost;
  uint32_t splist[2*(MAX_STAVE+1)];
  int8_t lvlist[2*(MAX_STAVE+1)];
  int8_t lvmost;
  uint8_t lvmax;
  BOOL found;
  const char *w;
  fontsizestr *f;
  zerocopystr *z;
  movtstr *m = movements[i];

  eprintf("\nMOVEMENT %d\n", m->number);

  found = FALSE;
  k = 0;
  eprintf("  accadjusts =");
  for (j = 0; j < NOTETYPE_COUNT; j++)
    {
    if (m->accadjusts[j] != 0)
      {
      while (k++ < j) eprintf(" 0.0");
      eprintf(" %s", sff(m->accadjusts[j]));
      k = j + 1;
      found = TRUE;
      }
    }
  if (!found) eprintf(" none");
  eprintf("\n");

  eprintf("  accspacing =");
  for (j = 0; j < ACCSPACE_COUNT; j++) eprintf(" %s", sff(m->accspacing[j]));
  eprintf("\n");

  eprintf("  barlinesize = %s\n", sff(m->barlinesize));
  eprintf("  barlinespace = ");
  if (m->barlinespace == FIXED_UNSET) eprintf("default\n");
    else eprintf("%s\n", sff(m->barlinespace));

  eprintf("  barlinestyle = %d\n", m->barlinestyle);
  eprintf("  barnumberlevel = %s\n", sff(m->barnumber_level));

  eprintf("  barnumbers =");
  if (m->barnumber_interval == 0) eprintf(" none"); else
    {
    debug_textflags(m->barnumber_textflags);
    if (m->barnumber_interval < 0) eprintf(" line");
      else eprintf(" %d", m->barnumber_interval);
    }

  eprintf("\n");

  eprintf("  baroffset = %d\n", m->baroffset);
  eprintf("  beamflaglength = %s\n", sff(m->beamflaglength));
  eprintf("  beamthickness = %s\n", sff(m->beamthickness));
  eprintf("  bottommargin = %s\n", sff(m->bottommargin));
  debug_stavelist("bracelist", m->bracelist);
  eprintf("  bracestyle = %d\n", m->bracestyle);
  debug_stavelist("bracketlist", m->bracketlist);
  debug_stavemap("breakbarlines", m->breakbarlines);
  eprintf("  breveledgerextra = %s\n", sff(m->breveledgerextra));
  eprintf("  caesurastyle = %d\n", m->caesurastyle);
  eprintf("  clefstyle = %d\n", m->clefstyle);

  /* Clef widths are a whole number of points, no fractions. */
  eprintf("  clefwidths =");
  for (j = 0; j < CLEF_COUNT; j++) eprintf(" %d", m->clefwidths[j]);
  eprintf("\n");

  eprintf("  copyzero =");
  for (z = m->zerocopy; z != NULL; z = z->next)
    eprintf(" %d/%s", z->stavenumber, sff(z->adjust));
  eprintf("\n");


  eprintf("  dotspacefactor = %s\n", sff(m->dotspacefactor));
  eprintf("  endlinesluradjust = %s\n", sff(m->endlinesluradjust));
  eprintf("  endlineslurstyle = %d\n", m->endlineslurstyle);
  eprintf("  endlinetieadjust = %s\n", sff(m->endlinetieadjust));
  eprintf("  endlinetiestyle = %d\n", m->endlinetiestyle);
  eprintf("  extenderlevel = %s\n", sff(m->extenderlevel));
  eprintf("  flags = 0x%08x\n", m->flags);

  f = m->fontsizes;
  if (i > 0 && f == movements[i-1]->fontsizes)
    eprintf("  fontsizes unchanged\n");
  else
    {
    eprintf("  fontsizes\n");
    debug_fontsize("barnumber", &(f->fontsize_barnumber));
    debug_fontsize("cue", &(f->fontsize_cue));
    debug_fontsize("cuegrace", &(f->fontsize_cuegrace));
    debug_fontsize("fbass", &(f->fontsize_text[ff_offset_fbass]));
    debug_fontsize("footnotes", &(f->fontsize_footnote));
    debug_fontsize("grace", &(f->fontsize_grace));
    debug_fontsize("init", &(f->fontsize_text[ff_offset_init]));
    debug_fontsize("midclefs", &(f->fontsize_midclefs));
    debug_fontsize("music", &(f->fontsize_music));
    debug_fontsize("olay", &(f->fontsize_text[ff_offset_olay]));
    debug_fontsize("rehearse", &(f->fontsize_rehearse));
    debug_fontsize("repno", &(f->fontsize_repno));
    debug_fontsize("restct", &(f->fontsize_restct));
    debug_fontsize("time", &(f->fontsize_text[ff_offset_ts]));
    debug_fontsize("trill", &(f->fontsize_trill));
    debug_fontsize("triplet", &(f->fontsize_triplet));
    debug_fontsize("ulay", &(f->fontsize_text[ff_offset_ulay]));
    debug_fontsize("vertacc", &(f->fontsize_vertacc));

    for (j = 0; j < AllFontSizes; j++)
      {
      char buffer[24];
      (void)sprintf(buffer, "text %d", j+1);
      debug_fontsize(buffer, &(f->fontsize_text[j]));
      }
    }

  debug_fonttype("fonttype_barnumber", m->fonttype_barnumber);
  debug_fonttype("fonttype_longrest", m->fonttype_longrest);
  debug_fonttype("fonttype_rehearse", m->fonttype_rehearse);
  debug_fonttype("fonttype_repeatbar", m->fonttype_repeatbar);
  debug_fonttype("fonttype_time", m->fonttype_time);
  debug_fonttype("fonttype_triplet", m->fonttype_triplet);

  debug_heading("footing", m->footing);
  eprintf("  footnotesep = %s\n", sff(m->footnotesep));
  eprintf("  gracespacing = %s %s\n", sff(m->gracespacing[0]),
    sff(m->gracespacing[1]));
  eprintf("  gracestyle = %d\n", m->gracestyle);
  eprintf("  hairpinlinewidth = %s\n", sff(m->hairpinlinewidth));
  eprintf("  hairpinwidth = %s\n", sff(m->hairpinwidth));
  eprintf("  halfflatstyle = %d\n", m->halfflatstyle);
  eprintf("  halfsharpstyle = %d\n", m->halfsharpstyle);

  debug_heading("heading", m->heading);
  eprintf("  hyphenstring = ");
  debug_string(m->hyphenstring);
  eprintf("\n");
  eprintf("  hyphenthreshold = %s\n", sff(m->hyphenthreshold));

  j = 1;
  for (htypestr *h = main_htypes; h != NULL; h = h->next, j++)
    {
    eprintf("  hyphentype %d ", j);
    debug_string(h->string1);
    eprintf("/s%d", h->size1 + 1);
    if (h->string3 != NULL)
      {
      eprintf(" END=");
      debug_string(h->string3);
      debug_move(h->adjust, "u", "d");
      }
    if (h->string2 != NULL)
      {
      eprintf(" NL=");
      debug_string(h->string2);
      eprintf("/s%d", h->size2 + 1);
      }
    eprintf("\n");
    }

  debug_stavelist("joinlist", m->joinlist);
  debug_stavelist("joindottedlist", m->joindottedlist);

  eprintf("  justify =");
  if (m->justify == just_all) eprintf(" all"); else
    {
    if ((m->justify & just_top) != 0) eprintf(" top");
    if ((m->justify & just_bottom) != 0) eprintf(" bottom");
    if ((m->justify & just_left) != 0) eprintf(" left");
    if ((m->justify & just_right) != 0) eprintf(" right");
    }
  eprintf("\n");

  eprintf("  key = %s\n", sfk(m->key));
  debug_heading("lastfooting", m->lastfooting);
  debug_layout(m->layout);
  eprintf("  ledgerstyle = %d\n", m->ledgerstyle);
  eprintf("  leftmargin = %s\n", (m->leftmargin < 0)? "unset" : sff(m->leftmargin));
  eprintf("  linelength = %s\n", sff(m->linelength));
  eprintf("  maxbeamslope = %s %s\n", sff(m->maxbeamslope[0]),
    sff(m->maxbeamslope[1]));

  /* Show only those MIDI channels that were set for this movement. */

  for (j = 1, k = 0; j <= MIDI_MAXCHANNEL; j++, k++)
    {
    usint s, t;
    if (mac_notbit(m->midichanset, j)) continue;
    eprintf("  midichannel %d ", j);
    if (m->midivoice[k] >= 128) eprintf("\"\"");
      else eprintf("\"#%d\"", m->midivoice[k] + 1);
    eprintf("/%d", m->midichannelvolume[k]);
    t = 0;  /* Not set for any stave */
    for (s = 1; s <= MAX_STAVE; s++)
      {
      if (m->midichannel[s] != j) continue;
      for (t = s + 1; t <= MAX_STAVE; t++)
        {
        if (m->midichannel[t] != j) break;
        }
      eprintf(" %d", s);
      if (--t > s) eprintf("-%d", t);
      s = t;
      }
    if (t > 0 && m->midinote[t] != 0) eprintf(" \"#%d\"", m->midinote[t]);
    eprintf("\n");
    }

  eprintf("  midistart =");
  if (m->midistart == NULL) eprintf(" unset"); else
    {
    for (j = 1; j <= m->midistart[0]; j++) eprintf(" %d", m->midistart[j]);
    }
  eprintf("\n");

  eprintf("  miditempo = %d", m->miditempo);
  if (m->miditempochanges != NULL)
    {
    uint32_t *p = m->miditempochanges;
    while (*p != UINT32_MAX)
      {
      eprintf(" %s/%d", sfb(p[0]), p[1]);
      p += 2;
      }
    }
  eprintf("\n");

  w = "  miditranspose =";
  for (j = 0; j <= MAX_STAVE; j++)
    {
    if ((m->miditranspose)[j] != 0)
      {
      eprintf("%s %d/%d", w, j, m->miditranspose[j]);
      w = "";
      }
    }
  if (w[0] == 0) eprintf("\n");

  w = "  midivolume =";
  for (j = 0; j <= MAX_STAVE; j++)
    {
    if ((m->midistavevolume)[j] != 15)
      {
      eprintf("%s %d/%d", w, j, m->midistavevolume[j]);
      w = "";
      }
    }
  if (w[0] == 0) eprintf("\n");

  eprintf("  midkeyspacing = %s\n", sff(m->midkeyspacing));
  eprintf("  midtimespacing = %s\n", sff(m->midtimespacing));
  eprintf("  note scaling = %d/%d\n", m->notenum, m->noteden);

  eprintf("  note spacing =");
  for (j = 0; j < NOTETYPE_COUNT; j++)
    eprintf(" %s", sff(m->note_spacing[j]));
  eprintf("\n");

  eprintf("  overlaydepth = %s\n", sff(m->overlaydepth));

  debug_heading("pagefooting", m->pagefooting);
  debug_heading("pageheading", m->pageheading);

  for (pk = main_printkey; pk != NULL; pk = pk->next)
    {
    if (i + 1 >= pk->movt_number)
      {
      eprintf("  printkey = %s %s ", sfk(pk->key), clef_names[pk->clef]);
      debug_string(pk->string);
      if (pk->cstring != NULL)
        {
        eprintf(" ");
        debug_string(pk->cstring);
        }
      eprintf("\n");
      }
    }

  for (pt = main_printtime; pt != NULL; pt = pt->next)
    {
    if (i + 1 >= pt->movt_number)
      {
      debug_time("printtime", pt->time, " ");
      debug_string(pt->top);
      eprintf("/s%d ", pt->sizetop+1);
      debug_string(pt->bot);
      eprintf("/s%d\n", pt->sizebot+1);
      }
    }

  eprintf("  rehearsalstyle = %s\n",
    ((m->rehearsalstyle & text_boxed) != 0)? "boxed" :
    ((m->rehearsalstyle & text_ringed) != 0)? "ringed" : "plain");
  eprintf("  repeatstyle = %d\n", m->repeatstyle);
  eprintf("  smallcapsize = %s\n", sff(m->smallcapsize));
  debug_stavemap("select_staves", m->select_staves);
  eprintf("  shortenstems = %s\n", sff(m->shortenstems));
  eprintf("  startbracketbar = %d\n", m->startbracketbar);
  eprintf("  startlinespacing =");
  for (j = 0; j < 4; j++) eprintf(" %s", sff(m->startspace[j]));
  eprintf("\n");

  found = FALSE;
  eprintf("  stavesizes (varied) =");
  for (j = 0; j <= MAX_STAVE; j++)
    {
    if (m->stavesizes[j] != 1000)
      {
      eprintf(" %d/%s", j, sff(m->stavesizes[j]));
      found = TRUE;
      }
    }
  if (!found) eprintf(" none");
  eprintf("\n");

  /* Scan the stave spacing list and find which spacing is the most common.
  This can be output first, as for the stavespacing directive, followed by the
  exceptions. */

  eprintf("  stavespacing =");
  spptr = 0;
  spmost = 0;
  spmax = 0;

  for (j = 1; j <= MAX_STAVE; j++)
    {
    uint32_t space = m->stave_spacing[j];
    for (k = 0; k < spptr; k += 2)
      {
      if (splist[k] == space)
        {
        splist[k+1] += 1;
        break;
        }
      }
    if (k >= spptr)
      {
      splist[k] = space;
      splist[k+1] = 1;
      spptr += 2;
      }
    if (splist[k+1] > spmax)
      {
      spmax = splist[k+1];
      spmost = splist[k];
      }
    }

  eprintf(" %s", sff(spmost));
  for (j = 1; j <= MAX_STAVE; j++)
    {
    if (m->stave_spacing[j] == spmost) continue;
    eprintf(" %d/", j);
    if (m->stave_ensure[j] > 0)
      eprintf("%s/", sff(m->stave_ensure[j]));
    eprintf("%s", sff(m->stave_spacing[j]));
    }
  eprintf("\n");

  eprintf("  stemlengths =");
  for (j = 2; j < NOTETYPE_COUNT; j++)
    eprintf(" %s", sff(m->stemadjusts[j]));
  eprintf("\n");

  eprintf("  stemswap = %s\n", stemswaptype[m->stemswaptype]);

  /* Scan the stemswaplevel list and find which is the most common. This can be
  output first, as for the directive, followed by the exceptions. We can't make
  this a common function with stavespacing because (a) the data types are
  different and (b) stavespacing has the ensure addition. */

  eprintf("  stemswaplevel =");
  spptr = 0;
  lvmost = 0;
  lvmax = 0;

  for (j = 1; j <= MAX_STAVE; j++)
    {
    int8_t level = m->stemswaplevel[j];
    for (k = 0; k < spptr; k += 2)
      {
      if (lvlist[k] == level)
        {
        lvlist[k+1] += 1;
        break;
        }
      }
    if (k >= spptr)
      {
      lvlist[k] = level;
      lvlist[k+1] = 1;
      spptr += 2;
      }
    if (lvlist[k+1] > lvmax)
      {
      lvmax = lvlist[k+1];
      lvmost = lvlist[k];
      }
    }

  eprintf(" %d", lvmost);
  for (j = 1; j <= MAX_STAVE; j++)
    {
    if (m->stemswaplevel[j] == lvmost) continue;
    eprintf(" %d/%d", j, m->stemswaplevel[j]);
    }
  eprintf("\n");

  debug_stavemap("suspend_staves", m->suspend_staves);

  eprintf("  systemgap = %s\n", sff(m->systemgap));
  eprintf("  systemsepangle = %s\n", sff(m->systemsepangle));
  eprintf("  systemseplength = %s\n", sff(m->systemseplength));
  eprintf("  systemsepwidth = %s\n", sff(m->systemsepwidth));
  eprintf("  systemsepposx = %s\n", sff(m->systemsepposx));
  eprintf("  systemsepposy = %s\n", sff(m->systemsepposy));

  debug_stavelist("thinbracketlist", m->thinbracketlist);
  eprintf("  topmargin = %s\n", sff(m->topmargin));
  eprintf("  transpose = ");
  if (m->transpose == NO_TRANSPOSE) eprintf("none\n");
    else eprintf("%d (quarter tones)\n", m->transpose);

  eprintf("  trillstring = ");
  debug_string(m->trillstring);
  eprintf("\n");

  eprintf("  tripletlinewidth = %s\n", sff(m->tripletlinewidth));
  debug_time("time", m->time, "\n");
  debug_time("time_unscaled", m->time_unscaled, "\n");
  eprintf("  underlaydepth = %s\n", sff(m->underlaydepth));
  eprintf("  underlaystyle = %d\n", m->underlaystyle);
  }
}



/*************************************************
*          Output a stave name structure         *
*************************************************/

static void
debug_stave_name(snamestr *sn, BOOL isextra)
{
snamestr *sn2;
if (sn->text != NULL)
  {
  eprintf("%s", isextra? "/" : " ");
  debug_string(sn->text);
  if ((sn->flags & snf_hcentre) != 0) eprintf("/c");
  if ((sn->flags & snf_rightjust) != 0) eprintf("/e");
  if ((sn->flags & snf_vcentre) != 0) eprintf("/m");
  if ((sn->flags & snf_vertical) != 0) eprintf("/v");
  eprintf("/s%d", sn->size);
  if (sn->adjustx != 0) debug_move(sn->adjustx, "r", "l");
  if (sn->adjusty != 0) debug_move(sn->adjusty, "u", "d");
  if (sn->linecount != 1) eprintf(" (COUNT=%d)", sn->linecount);

  for (sn2 = sn->extra; sn2 != NULL; sn2 = sn2->next)
    debug_stave_name(sn2, TRUE);
  }

if (sn->drawing != NULL) debug_draw(sn->drawing, sn->drawargs, "  ");
}



/*************************************************
*          Output one bar's information          *
*************************************************/

static void
debug_one_bar(movtstr *m, usint n, barstr *bar)
{
int i;
int stl;
bstr *b;
b_accentmovestr *am;
b_barlinestr *bs;
b_barnumstr *bn;
b_clefstr *clef;
b_drawstr *dr;
b_hairpinstr *hp;
b_justifystr *jf;
b_keystr *key;
b_midichangestr *mcs;
b_movestr *mv;
b_nbarstr *nb;
b_notestr *nt;
b_nsstr *ns;
b_ornamentstr *orn;
b_pletstr *pl;
b_sgstr *syg;
b_slurgapstr *sg;
b_slurstr *sl;
b_slurmodstr *sm;
b_spacestr *sp;
b_ssstr *ss;
b_tiestr *ti;
b_timestr *time;
b_tremolostr *tr;

eprintf("BAR %d (%s)", n, sfb(m->barvector[n]));
if (bar->repeatnumber != 0) eprintf(" [%d]", bar->repeatnumber);
eprintf("\n");
for (b = (bstr *)(bar->next); b != NULL; b = b->next)
  {
  switch (b->type)
    {
    case b_all:
    eprintf("  [all]\n");
    break;

    case b_barline:
    bs = (b_barlinestr *)b;
    eprintf("  barline type=%d style=%d\n", bs->bartype, bs->barstyle);
    break;

    case b_barnum:
    bn = (b_barnumstr *)b;
    eprintf("  [barnumber");
    if (!bn->flag) eprintf(" off"); else
      {
      debug_move(bn->x, "r", "l");
      debug_move(bn->y, "u", "d");
      }
    eprintf("]\n");
    break;

    case b_beamacc:
    eprintf("  [beamacc %d]\n", ((b_beamaccritstr *)b)->value);
    break;

    case b_beambreak:
    eprintf("  break beam %d\n", ((b_bytevaluestr *)b)->value);
    break;

    case b_beammove:
    eprintf("  [beammove %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_beamrit:
    eprintf("  [beamrit %d]\n", ((b_beamaccritstr *)b)->value);
    break;

    case b_beamslope:
    eprintf("  [beamslope %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_breakbarline:
    eprintf("  [breakbarline]\n");
    break;

    case b_bowing:
    eprintf("  [bowing %s]\n", ((b_bowingstr *)b)->value? "above" : "below");
    break;

    case b_caesura:
    eprintf("  //\n");
    break;

    case b_clef:
    clef = (b_clefstr *)b;
    eprintf("  %s%s%s\n", (clef->assume)? "assume " : "",
      clef_names[clef->clef], (clef->suppress)? " (suppress)" : "");
    break;

    case b_comma:
    eprintf("  [comma]\n");
    break;

    case b_dotbar:
    eprintf("  :\n");
    break;

    case b_dotright:
    eprintf("  dotright %s\n", sff(((b_dotrightstr *)b)->value));
    break;

    case b_draw:
    dr = (b_drawstr *)b;
    debug_draw(dr->drawing, dr->drawargs, (dr->overflag)? "  [over" : "  [");
    eprintf("]\n");
    break;

    case b_accentmove:
    am = (b_accentmovestr *)b;
    eprintf("  accentmove %d x=%s y=%s flags=0x%02x\n", am->accent, sff(am->x),
      sff(am->y), am->bflags);
    break;

    case b_endline:
    eprintf("  [endline");
    if (((b_bytevaluestr *)b)->value != 0)
      eprintf("/=%c", ((b_bytevaluestr *)b)->value);
    eprintf("]\n");
    break;

    case b_endslur:
    eprintf("  [endslur");
    if (((b_bytevaluestr *)b)->value != 0)
      eprintf("/=%c", ((b_bytevaluestr *)b)->value);
    eprintf("]\n");
    break;

    case b_endplet:
    eprintf("  }\n");
    break;

    case b_ens:
    eprintf("  [ns]\n");
    break;

    case b_ensure:
    eprintf("  [ensure %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_footnote:
    debug_heading("footnote", (&((b_footnotestr *)b)->h));
    break;

    case b_hairpin:
    hp = (b_hairpinstr *)b;
    eprintf("  %c", ((hp->flags & hp_cresc) == 0)? '>' : '<');
    debug_move(hp->x, "r", "l");
    debug_move(hp->y, "u", "d");
    if (hp->halfway != 0) eprintf("/h%s", sff(hp->halfway));
    debug_move(hp->offset, "rc", "lc");
    if (hp->width != 0) eprintf("/w%s", sff(hp->width));
    if (hp->su != 0) eprintf(" SU=%s", sff(hp->su));
    eprintf(" flags=0x%04x\n", hp->flags);
    break;

    case b_justify:
    jf = (b_justifystr *)b;
    eprintf("  [justify %c", ((jf->value & just_add) == 0)? '-' : '+');
    if ((jf->value & just_top) != 0) eprintf("top");
    if ((jf->value & just_bottom) != 0) eprintf("bottom");
    if ((jf->value & just_left) != 0) eprintf("left");
    if ((jf->value & just_right) != 0) eprintf("right");
    eprintf("]\n");
    break;

    case b_key:
    key = (b_keystr *)b;
    eprintf("  [%skey %s", (key->assume)? "assume " : "", sfk(key->key));
    if (!key->warn) eprintf(" nowarn");
    eprintf("]\n");
    break;

    case b_lrepeat:
    eprintf("  (:\n");
    break;

    case b_midichange:
    mcs = (b_midichangestr *)b;
    eprintf("  midichange channel=%d voice=%d volume=%d note=%d transpose=%d\n",
      mcs->channel, mcs->voice, mcs->volume, mcs->note, mcs->transpose);
    break;

    case b_move:
    mv = (b_movestr *)b;
    eprintf("  [%smove %s", (mv->relative)? "r" : "", sff(mv->x));
    if (mv->y != 0) eprintf(",%s", sff(mv->y));
    eprintf("]\n");
    break;

    case b_name:
    eprintf("  name %d\n", ((b_namestr *)b)->value);
    break;

    case b_nbar:
    nb = (b_nbarstr *)b;
    eprintf("  nbar %d", nb->n);
    if (nb->x != 0) eprintf(" x=%s", sff(nb->x));
    if (nb->y != 0) eprintf(" y=%s", sff(nb->y));
    if (nb->s != NULL) debug_string(nb->s);
    eprintf("\n");
    break;

    case b_newline:
    eprintf("  [newline]\n");
    break;

    case b_newpage:
    eprintf("  [newpage]\n");
    break;

    case b_note:
    if ((((b_notestr *)b)->flags & nf_chord) != 0) eprintf("  (\n");
    /* Fall through */
    case b_chord:
    nt = (b_notestr *)b;
    eprintf("  %s", acnames[nt->acc]);
    if (nt->acc != ac_no && nt->accleft != (int32_t)(m->accspacing[nt->acc]))
      eprintf("<%s", sff(nt->accleft));
    eprintf("%c%s", (nt->notetype < crotchet)? toupper(nt->char_orig) :
      nt->char_orig, noteflags[nt->notetype]);
    if ((nt->flags & nf_dot2) != 0) eprintf("..");
      else if ((nt->flags & nf_dot) != 0) eprintf(".");
    if (nt->acc != ac_no && nt->acc_orig != nt->acc)
      eprintf(" orig=%s", acnames[nt->acc_orig]);
    if (nt->masq != MASQ_UNSET) eprintf(" masq=%d", nt->masq);
    if (nt->acflags != 0) eprintf(" acflags=0x%08x", nt->acflags);
    eprintf(" flags=0x%08x", nt->flags);
    if (nt->noteheadstyle != 0) eprintf(" nhstyle=0x%02x", nt->noteheadstyle);
    eprintf(" length=%d", nt->length);
    if (nt->yextra != 0) eprintf(" yextra=%s", sff(nt->yextra));

    /* Skip this for rests */

    if (nt->spitch != 0)
      {
      stl = ((int)(nt->spitch) - P_0L)/4;
      eprintf(" P_%d%c", stl/2, ((stl & 1) != 0)? 'S':'L');
      eprintf(" %d%s", nt->abspitch/OCTAVE - 3,
        ((nt->acc >= ac_hf)? absnamesf:absnamess) + 4*(nt->abspitch % OCTAVE));
      }

    eprintf("\n");
    if (b->type == b_chord && b->next != NULL && b->next->type != b_chord)
      eprintf("  )\n");
    break;

    case b_notes:
    eprintf("  [notes %s]\n", (((b_notesstr *)b)->value)? "on" : "off");
    break;

    case b_ns:
    ns = (b_nsstr *)b;
    eprintf("  [ns");
    for (i = 0; i < NOTETYPE_COUNT; i++) eprintf(" %s", sff(ns->ns[i]));
    eprintf("]\n");
    break;

    case b_nsm:
    eprintf("  [ns *%s]\n", sff(((b_nsmstr *)b)->value));
    break;

    case b_olevel:
    eprintf("  [olevel ");
    if (((b_uolevelstr *)b)->value == FIXED_UNSET) eprintf("*]\n");
    else eprintf("%s]\n", sff(((b_uolevelstr *)b)->value));
    break;

    case b_olhere:
    eprintf("  [olhere %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_ornament:
    orn = (b_ornamentstr *)b;
    eprintf("  ornament %d flags=0x%02x x=%s y=%s\n", orn->ornament,
      orn->bflags, sff(orn->x), sff(orn->y));
    break;

    case b_overbeam:
    eprintf("  overbeam\n");
    break;

    case b_page:
    eprintf("  [page %s%d]\n", ((b_pagestr *)b)->relative? "+" : "",
      ((b_pagestr *)b)->value);
    break;

    case b_pagebotmargin:
    eprintf("  [bottommargin %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_pagetopmargin:
    eprintf("  [topmargin %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_plet:
    pl = (b_pletstr *)b;
    eprintf("  {%d x=%d yl=%d yr=%d flags=0x%0x\n", pl->pletlen, pl->x,
      pl->yleft, pl->yright, pl->flags);
    break;

    case b_reset:
    if (((b_resetstr *)b)->moff == 0) eprintf("  [reset]\n");
      else eprintf("  [backup %d]\n", ((b_resetstr *)b)->moff);
    break;

    case b_resume:
    eprintf("  [resume]\n");
    break;

    case b_rrepeat:
    eprintf("  :)\n");
    break;

    case b_sgabove:
    case b_sghere:
    case b_sgnext:
    syg = (b_sgstr *)b;
    eprintf("  [sg%s ", (b->type == b_sgabove)? "above" :
                        (b->type == b_sghere)? "here" : "next");
    if (syg->relative && syg->value >= 0) eprintf("+");
    eprintf("%s]\n", sff(syg->value));
    break;

    case b_slur:
    sl = (b_slurstr *)b;
    eprintf("  [%s%s", ((sl->flags & sflag_x) != 0)? "x" : "",
      ((sl->flags & sflag_l) == 0)? "slur" : "line");
    if (sl->id != 0) eprintf("/=%c", sl->id);
    if (sl->ally != 0) debug_move(sl->ally, "u", "d");
    debug_flags(sl->flags, "w,b,l,h,ol,or,i,e,x,abs,lay,ip,cx");
    for (sm = sl->mods; sm!= NULL; sm = sm->next)
      {
      if (sm->sequence != 0) eprintf("/%d", sm->sequence);
      if (sm->lxoffset != 0) debug_move(sm->lxoffset, "lrc", "llc");
      if (sm->lx != 0) debug_move(sm->lx, "lr", "ll");
      if (sm->ly != 0) debug_move(sm->ly, "lu", "ld");
      if (sm->rxoffset != 0) debug_move(sm->rxoffset, "rrc", "rlc");
      if (sm->rx != 0) debug_move(sm->rx, "rr", "rl");
      if (sm->ry != 0) debug_move(sm->ry, "ru", "rd");
      if (sm->c != 0) debug_move(sm->c, "co", "ci");
      if (sm->clx != 0) debug_move(sm->clx, "clr", "cll");
      if (sm->cly != 0) debug_move(sm->cly, "clu", "cld");
      if (sm->crx != 0) debug_move(sm->crx, "crr", "crl");
      if (sm->cry != 0) debug_move(sm->cry, "cru", "crd");
      }
    eprintf("]\n");
    break;

    case b_slurgap:
    case b_linegap:
    sg = (b_slurgapstr *)b;
    eprintf("  [%sgap", (sg->type == b_slurgap)? "slur" : "line");
    if (sg->id != 0) eprintf("/=%c", sg->id);
    eprintf("/w%s", sff(sg->width));
    if (sg->hfraction >= 0) eprintf("/h%s", sff(sg->hfraction));
    if (sg->xadjust != 0) debug_move(sg->xadjust, "r" , "l");
    if (sg->drawing != NULL) debug_draw(sg->drawing, sg->drawargs, "/");
    if (sg->gaptext != NULL)
      {
      eprintf("/");
      debug_string(sg->gaptext);
      if (sg->textsize != 0) eprintf("/%d", sg->textsize);
      if ((sg->textflags & text_boxed) != 0) eprintf("/box");
      if ((sg->textflags & text_ringed) != 0) eprintf("/ring");
      }
    eprintf("]\n");
    break;

    case b_space:
    sp = (b_spacestr *)b;
    eprintf("  [%sspace %s]\n", (sp->relative)? "r" : "", sff(sp->x));
    break;

    case b_ssabove:
    case b_sshere:
    case b_ssnext:
    ss = (b_ssstr *)b;
    eprintf("  [ss%s ", (b->type == b_ssabove)? "above" :
                        (b->type == b_sshere)? "here" : "next");
    if (ss->relative && ss->value >= 0) eprintf("+");
    eprintf("%s]\n", sff(ss->value));
    break;

    case b_suspend:
    eprintf("  [suspend]\n");
    break;

    case b_text:
    debug_stavestring((b_textstr *)b);
    break;

    case b_tick:
    eprintf("  [tick]\n");
    break;

    case b_tie:
    ti = (b_tiestr *)b;
    eprintf("  tie flags=0x%02x above=%d below=%d\n", ti->flags,
      ti->abovecount, ti->belowcount);
    break;

    case b_time:
    time = (b_timestr *)b;
    eprintf("  [%stime ", (time->assume)? "assume " : "");
    debug_time(NULL, time->time, "");
    if (!time->warn) eprintf(" nowarn");
    eprintf("]\n");
    break;

    case b_tremolo:
    tr = (b_tremolostr *)b;
    eprintf("  [tremolo/x%d/j%d]\n", tr->count, tr->join);
    break;

    case b_tripsw:
    eprintf("  [triplets %s]\n", ((b_tripswstr *)b)->value? "on" : "off");
    break;

    case b_ulevel:
    eprintf("  [ulevel ");
    if (((b_uolevelstr *)b)->value == FIXED_UNSET) eprintf("*]\n");
    else eprintf("%s]\n", sff(((b_uolevelstr *)b)->value));
    break;

    case b_ulhere:
    eprintf("  [ulhere %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    case b_unbreakbarline:
    eprintf("  [unbreakbarline]\n");
    break;

    case b_zerocopy:
    eprintf("  [copyzero %s]\n", sff(((b_intvaluestr *)b)->value));
    break;

    default:
    eprintf("*** Unknown b_type: %d\n", b->type);
    break;
    }
  }
}



/*************************************************
*               Output bar information           *
*************************************************/

void
debug_bar(void)
{
if (dbd_bar >= 0)
  {
  movtstr *m;
  if (dbd_movement > (int)movement_count)
    error(ERR163, "movement", dbd_movement);  /* Hard */
  m = movements[dbd_movement - 1];
  st = m->stavetable[dbd_stave];
  if (dbd_stave > MAX_STAVE || st == NULL)
    error(ERR163, "stave", dbd_stave);  /* Hard */
  if (dbd_bar >= m->barcount) error(ERR163, "absolute bar", dbd_bar);  /* Hard */
  eprintf("\nMOVEMENT %d STAVE %d ", dbd_movement, dbd_stave);
  debug_one_bar(m, dbd_bar, st->barindex[dbd_bar]);
  }


else for (usint i = 0; i < movement_count; i++)
  {
  int j;
  movtstr *m = movements[i];
  eprintf("\nMOVEMENT %d\n", i+1);
  for (j = 0; j <= m->laststave; j++)
    {
    int k;
    snamestr *sn;
    st = m->stavetable[j];
    eprintf("\nSTAVE %d", j);
    if (st->omitempty) eprintf(" omitempty");
    for (sn = st->stave_name; sn != NULL; sn = sn->next)
      debug_stave_name(sn, FALSE);
    eprintf("\n");
    if (st->stavelines != 5) eprintf("  [stavelines %d]\n", st->stavelines);
    for (k = 0; k < st->barcount; k++) if (st->barindex[k]->next != NULL) break;
    if (k >= st->barcount)
      eprintf("  All bars are empty\n");
    else for (k = 0; k < st->barcount; k++)
      {
      eprintf("\n");
      debug_one_bar(m, k, st->barindex[k]);
      }
    }
  }
}



/*************************************************
*             Output memory usage                *
*************************************************/

void
debug_memory_usage(void)
{
usint i;
usint chunk_count;
size_t available, independent_total;
size_t total = 0;

eprintf("\nMEMORY USAGE\n");

eprintf("  Read buffers: 3*%zd = %zd\n", main_readbuffer_size,
  3 * main_readbuffer_size);
total += 3 * main_readbuffer_size;

for (i = 0; i < MAX_MACRODEPTH; i++)
  {
  if (main_argbuffer[i] == NULL) break;
  eprintf("  Macro arg buffer %d: %zd\n", i, main_argbuffer_size[i]);
  total += main_argbuffer_size[i];
  }
if (i == 0) eprintf("  Macro arg buffers: 0\n");

eprintf("  String buffer: %zd\n", read_stringbuffer_size);
total += read_stringbuffer_size;

eprintf("  Text queue: %zd", out_textqueue_size * sizeof(b_textstr *));
total += out_textqueue_size * sizeof(b_textstr *);

eprintf("  Font list vector: %zd\n", font_list_size * sizeof(fontstr));
total += font_list_size * sizeof(fontstr);

#if SUPPORT_XML
if (xml_layout_list_size > 0)
  {
  eprintf("  MusicXML layout size: %zd\n", xml_layout_list_size);
  total += xml_layout_list_size;
  }
#endif

eprintf("  Movements vector: %zd\n", movements_size * sizeof(movtstr *));
total += movements_size * sizeof(movtstr *);

/* Expandable items in movements */

for (i = 0; i < movement_count; i++)
  {
  int j;
  movtstr *m = movements[i];
  if ((m->flags & mf_midistart) != 0)
    {
    size_t x = m->midistart[0] + MIDI_START_CHUNKSIZE;
    x -= x % MIDI_START_CHUNKSIZE;
    eprintf("  MIDI start bytes: %zd\n", x);
    total += x;
    }

  for (j = 0; j <= m->laststave; j++)
    {
    size_t x = m->stavetable[j]->barindex_size * sizeof(barstr *);
    if (x != 0)
      {
      eprintf("  Barvector %d/%d: %zd\n", i+1, j, x);
      total += x;
      }
    }
  }

/* General non-expandable memory items */

chunk_count = mem_get_info(&available, &independent_total);
eprintf("  Fixed memory: independent blocks = %zd\n", independent_total);
total += independent_total;

eprintf("  Fixed memory: %d chunk%s = %zd (%zd still available)\n",
  chunk_count, (chunk_count != 1)? "s" : "",
  chunk_count * (size_t)MEMORY_CHUNKSIZE, available);
total += chunk_count * MEMORY_CHUNKSIZE;

eprintf("Grand total: %zd\n", total);
}

/* End of debug.c */
