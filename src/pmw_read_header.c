/*************************************************
*        PMW native header reading functions     *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: February 2025 */

#include "pmw.h"


/* Options for reading 32-bit integer values */

#define int_f   1              /* fixed point */
#define int_u   2              /* unsigned */
#define int_uf  (int_u|int_f)  /* unsigned fixed point */
#define int_rs  4              /* relative if signed */


/*************************************************
*                 Local data                     *
*************************************************/

/* An empty PMW string */

static uint32_t empty_string[] = { 0 };

/* The table of header directives contains integer parameters that index into
this list when setting global values that apply to all movements. */

enum {
  glob_drawbarlines,
  glob_kerning,
  glob_incpmwfont,
  glob_magnification,
  glob_maxvertjustify,
  glob_midifornotesoff,
  glob_nowidechars,
  glob_pagelength,
  glob_righttoleft,
  glob_sheetdepth,
  glob_sheetwidth
};

static uint32_t *global_vars[] = {
  (uint32_t *)(&bar_use_draw),
  (uint32_t *)(&main_kerning),
  (uint32_t *)(&print_incPMWfont),
  (uint32_t *)(&main_magnification),
  (uint32_t *)(&main_maxvertjustify),
  (uint32_t *)(&main_midifornotesoff),
  (uint32_t *)(&stave_use_widechars),
  (uint32_t *)(&main_pagelength),
  (uint32_t *)(&main_righttoleft),
  (uint32_t *)(&main_sheetdepth),
  (uint32_t *)(&main_sheetwidth)
};

/* Data for the b2pffont directive */

#if defined SUPPORT_B2PF && SUPPORT_B2PF != 0
typedef struct b2pfopt {
  const char *name;
  uint32_t option;
} b2pfopt;

static b2pfopt b2pf_options[] = {
  { "input_backchars", B2PF_INPUT_BACKCHARS },
  { "input_backcodes", B2PF_INPUT_BACKCODES },
  { "output_backchars", B2PF_OUTPUT_BACKCHARS },
  { "output_backcodes", B2PF_OUTPUT_BACKCODES }
};

#define b2pf_options_size (sizeof(b2pf_options)/sizeof(b2pfopt))
#endif



/*************************************************
*          Local static variables                *
*************************************************/

static dirstr *dir;  /* Points to found header directive */




/*************************************************
*        Read and set font type, if present      *
*************************************************/

/* If a recognized font type ("roman", "italic", etc.) is the next thing in the
input, read it and put its index number into the current movement data
structure, at a given offset. If what follows is not a recognized font type, do
nothing.

Argument:   the offset for the result
Returns:    nothing
*/

static void
set_fonttype(size_t offset)
{
size_t save_read_i;
uint32_t save_read_c;
uint32_t x;

read_sigc();
if (!isalpha(read_c)) return;

save_read_i = read_i;
save_read_c = read_c;
if ((x = font_readtype(TRUE)) != font_unknown)
  {
  *((uint8_t *)(((uschar *)curmovt) + offset)) = (uint8_t)x;
  }
else
  {
  read_c = save_read_c;
  read_i = save_read_i;
  }
}



/*************************************************
*           Read and set font size               *
*************************************************/

/* This is for the table of font sizes in the movement structure. If the next
thing in the input is a digit, read it as a font size. If not, do nothing.
The size may be followed by a stretch and shear, if permitted. Multiple
movements share the same font size structure until a change is made, in which
case a new structure must be set up.

Arguments:
  offset      offset in fontsize structure for the fontinststr
  stretchOK   TRUE if stretch and/or shearing are permitted for the font

Returns:      nothing
*/

static void
set_fontsize(size_t offset, BOOL stretchOK)
{
read_sigc();
if (!isdigit(read_c)) return;

/* If this movement is not the first and has not got its own fontsizes
structure yet, set one up. */

if (!MFLAG(mf_copiedfontsizes))
  {
  fontsizestr *new = mem_get(sizeof(fontsizestr));
  *new = *(curmovt->fontsizes);
  curmovt->fontsizes = new;
  curmovt->flags |= mf_copiedfontsizes;
  }

read_fontsize((fontinststr *)((char *)(curmovt->fontsizes) + offset), stretchOK);
}



/*******************************************************************
*  The following functions are referenced in the read_headstring   *
*  vector of header directives. Each is called when the relevant   *
*  directive is encountered. They all have the same interface:     *
*  dir->arg1 and dir->arg2 contain up to two arguments associated  *
*  with the directive. There are no arguments to the functions     *
*  themselves, and they do not return any values.                  *
******************************************************************/


/*************************************************
*      Give warning for obsolete directive       *
*************************************************/

static void
warning(void)
{
error(ERR39, dir->name);
}


/*************************************************
*      Give warning for obsolete directive       *
*************************************************/

/* For some directives we need to read the argument in case something else
follows on the same input line. */

static void
warningint(void)
{
int32_t x;
if (read_expect_integer(&x, FALSE, FALSE)) error(ERR39, dir->name);
}



/*************************************************
*             Deal with font size                *
*************************************************/

static void
movt_fontsize(void)
{
if (isdigit(read_c)) set_fontsize(dir->arg1, dir->arg2);
  else error(ERR8, "number");
}



/*************************************************
*           Deal with font size and type         *
*************************************************/

static void
movt_font(void)
{
usint x;
set_fontsize(dir->arg2, TRUE);
if ((x = font_readtype(FALSE)) != font_unknown)
  *((uint8_t *)(((uschar *)curmovt) + dir->arg1)) = x;
}



/*************************************************
*           Deal with movement flags             *
*************************************************/

/* The function is called for directives that just set or unset a bit in the
movement flags. Some other flags are set/unset as part of a more complicated
header directive. */

static void
movt_flag(void)
{
if (dir->arg2) curmovt->flags |= dir->arg1;
  else curmovt->flags &= ~(dir->arg1);
}



/*************************************************
*           Deal with global boolean             *
*************************************************/

static void
glob_bool(void)
{
if (movement_count == 1) *((BOOL *)(global_vars[dir->arg1])) = dir->arg2;
  else error(ERR40, dir->name);
}


/*************************************************
*         Deal with global 32-bit integer        *
*************************************************/

static void
glob_int(void)
{
int32_t x;
int32_t value = 0;
int32_t flags = dir->arg2;
int32_t *address = (int32_t *)(global_vars[dir->arg1]);
if ((flags & int_rs) != 0 && (read_c == '+' || read_c == '-')) value = *address;
if (read_expect_integer(&x, (flags & int_f) != 0, (flags & int_u) == 0))
  {
  if (movement_count == 1) *address = value + x;
    else error(ERR40, dir->name);
  }
}



/*************************************************
*         Deal with movement 32-bit integer      *
*************************************************/

static void
movt_int(void)
{
int32_t x;
int32_t value = 0;
int32_t flags = dir->arg2;
int32_t *address = (int32_t *)((uschar *)curmovt + dir->arg1);
if ((flags & int_rs) != 0 && (read_c == '+' || read_c == '-')) value = *address;
if (read_expect_integer(&x, (flags & int_f) != 0, (flags & int_u) == 0))
  *address = value + x;
}



/*************************************************
*         Deal with movement 8-bit integer       *
*************************************************/

/* These are always small positive numbers, with a specified maximum. */

static void
movt_int8(void)
{
int32_t x;
if (read_expect_integer(&x, FALSE, FALSE))
  {
  if (x > dir->arg2) error(ERR38, dir->name, dir->arg2);
    else *((int8_t *)((uschar *)curmovt + dir->arg1)) = x;
  }
}



/*************************************************
*           Deal with header stave list          *
*************************************************/

/* Some lists turn into bit maps, others into chains of stavelist blocks (when
a bit map would not distinguish between "1-2,2-4" and "1-4", for example). */

static void
movt_list(void)
{
uschar *endptr;
uint64_t *mapptr;
stavelist **listptr;
enum error_number n;

if (dir->arg2)
  {
  mapptr = (uint64_t *)((uschar *)curmovt + dir->arg1);
  listptr = NULL;
  }
else
  {
  mapptr = NULL;
  listptr = (stavelist **)((uschar *)curmovt + dir->arg1);
  }

n = read_stavelist(main_readbuffer + read_i - 1, &endptr, mapptr, listptr);

if (n != 0)
  {
  error(n);
  read_i = main_readlength;
  read_c = '\n';
  }
else
  {
  read_i = endptr - main_readbuffer;
  read_nextc();
  }
}



/*************************************************
*          Deal with heading/footing text        *
*************************************************/

/* The bits in the map remember which have been read. This is so that those
that persist by default, e.g. pageheading, can be overridden when supplied in a
new movement. Some, e.g. heading, are always cleared at the start of a new
movement, so this mechanism isn't necessary for them but it does no harm. */

static void
movt_headfoot(void)
{
headstr **oldp = (headstr **)(((uschar *)curmovt) + dir->arg1);
headstr *new = mem_get(sizeof(headstr));

/* Start a new chain if this is the first occurrence this movement. */

if ((read_headmap & dir->arg2) == 0)
  {
  read_headmap |= dir->arg2;
  *oldp = NULL;
  }

/* Else find the end of the chain */

else while (*oldp != NULL) oldp = &((*oldp)->next);

/* Add the new block onto the chain and initialize. */

*oldp = new;
read_headfootingtext(new, dir->arg2, '\n');
}



/*************************************************
*            Accadjusts                          *
*************************************************/

static void
accadjusts(void)
{
int i;
int32_t *x = mem_get(NOTETYPE_COUNT*sizeof(int32_t));
for (i = 0; i < NOTETYPE_COUNT; i++)
  {
  if (read_c == ',') read_nextsigc();
  if (read_c != '-' && read_c != '+' && !isdigit(read_c)) x[i] = 0;
    else (void)read_expect_integer(x+i, TRUE, TRUE);
  read_sigc();
  }
curmovt->accadjusts = x;
}



/*************************************************
*            Accspacing                          *
*************************************************/

/* The ordering of accidentals has changed from the original PMW. In order to
preserve compatibility with the specification, the widths here are read in a
different order to the way the values are stored. If an additional value is
given, it applies to the narrow Egyptian half sharp, which otherwise is set 0.2
points narrower than a normal sharp. This is stored in the "no accidental" slot
in the table. */

static void
accspacing(void)
{
int i;
uint32_t *x = mem_get(ACCSPACE_COUNT*sizeof(uint32_t));
memcpy(x, curmovt->accspacing, ACCSPACE_COUNT*sizeof(uint32_t));
curmovt->accspacing = x;

for (i = 0; i < 5; i++)
  {
  int32_t w;
  if (read_c == ',') read_nextsigc();
  if (!read_expect_integer(&w, TRUE, FALSE)) break;
  switch(i)
    {
    case 0: x[ac_ds] = w; break;              /* double sharp */
    case 1: x[ac_hf] = x[ac_fl] = w; break;   /* half flat & flat */
    case 2: x[ac_df] = w; break;              /* double flat */
    case 3: x[ac_nt] = w; break;              /* natural */
    case 4: x[ac_hs] = x[ac_sh] = w; break;   /* half sharp and sharp */
    }
  read_sigc();
  }

if (isdigit(read_c))
  (void)read_expect_integer((int32_t *)(&(x[ac_no])), TRUE, FALSE);
else x[ac_no] = x[ac_sh] - 200;
}



/*************************************************
*       Set up font for B2PF processing          *
*************************************************/

/* The b2pffont directive is available only if explicitely selected when PMW is
built. It is permitted only in the first movement of a file (like textfont). */

static void
b2pffont(void)
{
#if !defined SUPPORT_B2PF || SUPPORT_B2PF == 0
error(ERR76);   /* Hard error: not supported */
#else

int rc;
usint fontid, ln;
uint32_t options = B2PF_UTF_32;

if (movement_count != 1) { error(ERR40); return; }

read_sigc();
if ((fontid = font_readtype(FALSE)) == font_unknown) return;
if (font_b2pf_contexts[fontid] != NULL) error(ERR77);  /* Hard */

/* Read B2PF options */

read_sigc();
while (read_c != '\"')
  {
  size_t i;
  read_nextword();
  for (i = 0; i < b2pf_options_size; i++)
    {
    if (Ustrcmp(read_wordbuffer, b2pf_options[i].name) == 0)
      {
      options |= b2pf_options[i].option;
      break;
      }
    }
  if (i >= b2pf_options_size)
    {
    error(ERR78, read_wordbuffer);
    break;
    }
  read_sigc();
  }

font_b2pf_options[fontid] = options;  /* Remember processing options */

/* Read the B2PF context name */

if (!string_read_plain())
  {
  error(ERR8, "B2PF context name (quoted)");
  return;
  }

/* Create a B2PF context for this font (there are currently no context create
options). */

rc = b2pf_context_create((const char *)read_stringbuffer,
  (const char *)font_data_extra, 0, font_b2pf_contexts + fontid,
  NULL, NULL, NULL, &ln);

/* If there are more strings in quotes, add them as extra context rules. */

if (rc == B2PF_SUCCESS)
  {
  read_sigc();
  while (read_c == '\"')
    {
    (void)string_read_plain();  /* Can't fail if leading quote seen */
    rc = b2pf_context_add_file(font_b2pf_contexts[fontid],
      (const char *)read_stringbuffer, (const char *)font_data_extra, 0, &ln);
    if (rc != B2PF_SUCCESS) break;
    read_sigc();
    }
  }

if (rc != B2PF_SUCCESS)
  {
  size_t buffused;
  char buffer[128];
  (void)b2pf_get_error_message(rc, buffer, sizeof(buffer), &buffused, 0);
  buffer[buffused] = 0;
  error(ERR79, buffer);  /* Hard */
  }

#endif  /* SUPPORT_B2PF */
}



/*************************************************
*             Bar (initial bar number)           *
*************************************************/

/* The value retained is one less than the given number, that is, it is an
offset to add to the default number. */

static void
bar(void)
{
movt_int();
curmovt->baroffset -= 1;
}



/*************************************************
*               Barlinespace                     *
*************************************************/

/* We have to set the default here in case the directive uses + or - to adjust
the value. Note that this code is replicated in paginate.c. */

static void
barlinespace(void)
{
if (read_c == '*')
  {
  curmovt->barlinespace = FIXED_UNSET;
  read_nextsigc();
  }
else
  {
  if (curmovt->barlinespace == FIXED_UNSET)
    {
    curmovt->barlinespace = (curmovt->note_spacing)[minim]/2 - 5000;
    if (curmovt->barlinespace < 3000) curmovt->barlinespace = 3000;
    }
  movt_int();
  }
}



/*************************************************
*              Barnumberlevel                    *
*************************************************/

/* A sign is mandatory */

static void
barnumberlevel(void)
{
if (read_c != '+' && read_c != '-') error(ERR8, "\"+\" or \"-\"");
  else movt_int();
}



/*************************************************
*            Barnumbers                          *
*************************************************/

static void
barnumbers(void)
{
BOOL wordread = FALSE;
curmovt->barnumber_textflags = 0;
if (isalpha(read_c))
  {
  read_nextword();
  read_sigc();
  if (Ustrcmp(read_wordbuffer, "boxed") == 0)
    curmovt->barnumber_textflags = text_boxed;
  else if (Ustrcmp(read_wordbuffer, "roundboxed") == 0)
    curmovt->barnumber_textflags = text_boxed | text_boxrounded;
  else if (Ustrcmp(read_wordbuffer, "ringed") == 0)
    curmovt->barnumber_textflags = text_ringed;
  else wordread = TRUE;
  }

if (!wordread && isalpha(read_c))
  { read_nextword(); wordread = TRUE; }

if (wordread)
  {
  if (Ustrcmp(read_wordbuffer, "line") == 0) curmovt->barnumber_interval = -1;
    else { error(ERR8, "\"line\""); return; }
  }
else
  {
  if (!read_expect_integer(&(curmovt->barnumber_interval), FALSE, FALSE))
    return;
  }

set_fontsize(offsetof(fontsizestr, fontsize_barnumber), TRUE);
set_fonttype(offsetof(movtstr, fonttype_barnumber));
}



/*************************************************
*            Breakbarlines[x]                    *
*************************************************/

static void
breakbarlines(void)
{
if (!isdigit(read_c)) curmovt->breakbarlines = ~0LU;
  else movt_list();
if (dir->name[13] == 'x') curmovt->flags |= mf_fullbarend;
  else curmovt->flags &= ~mf_fullbarend;
}



/*************************************************
*             Clef size (relative)               *
*************************************************/

/* The clef font size is relative to a 10-point font, which is why it gets
multiplied by 10. */

static void
clefsize(void)
{
movt_fontsize();
curmovt->fontsizes->fontsize_midclefs.size *= 10;
}



/************************************************
*             Clef widths                       *
************************************************/

/* There are only five different clef shapes, whose widths can be set here. The
widths are applied to all clefs of the same shape. */

/* These are the five basic clef shapes, in the order in which their widths
must be specifed in the "clefwidths" directive. */

static uint8_t clef_shapes[] = { clef_treble, clef_bass, clef_alto, clef_hclef,
  clef_none };

/* These are the basic clef shapes that apply to each actual clef. */

static uint8_t clef_shape_list[] = {
  clef_alto,    /* alto */
  clef_bass,    /* baritone */
  clef_bass,    /* bass */
  clef_alto,    /* cbaritone */
  clef_bass,    /* contrabass */
  clef_bass,    /* deepbass */
  clef_hclef,   /* hclef */
  clef_alto,    /* mezzo */
  clef_none,    /* none */
  clef_bass,    /* soprabass */
  clef_alto,    /* soprano */
  clef_alto,    /* tenor */
  clef_treble,  /* treble */
  clef_treble,  /* trebledescant */
  clef_treble,  /* trebletenor */
  clef_treble   /* trebletenorB */
};


static void
clefwidths(void)
{
usint i, j;
for (i = 0; i < (sizeof(clef_shapes)/sizeof(uint8_t)) && isdigit(read_c); i++)
  {
  int32_t width;
  (void)read_expect_integer(&width, FALSE, FALSE);
  if (read_c == ',') read_nextc();
  read_sigc();
  for (j = 0; j < CLEF_COUNT; j++)
    if (clef_shape_list[j] == clef_shapes[i]) curmovt->clefwidths[j] = width;
  }
}



/*************************************************
*                Copyzero                        *
*************************************************/

static void
copyzero(void)
{
zerocopystr **pp = &(curmovt->zerocopy);

if (!isdigit(read_c)) { error(ERR8, "stave number"); return; }

while (isdigit(read_c))
  {
  zerocopystr *p = mem_get(sizeof(zerocopystr));
  *pp = p;
  pp = &(p->next);
  p->next = NULL;
  p->stavenumber = read_usint();
  if (read_c == '/')
    {
    read_nextc();
    if (!read_expect_integer(&(p->adjust), TRUE, TRUE)) break;
    }
  else p->adjust = 0;
  p->baradjust = 0;
  read_sigc();
  if (read_c == ',') read_nextsigc();
  }
}



/*************************************************
*              Doublenotes                       *
*************************************************/

static void
doublenotes(void)
{
curmovt->notenum *= 2;
curmovt->time = read_scaletime(curmovt->time_unscaled);
}



/*************************************************
*              DrawStaveLines                    *
*************************************************/

static void
drawstavelines(void)
{
int x = 3;
if (isdigit(read_c)) x = read_usint();
if (movement_count == 1) stave_use_draw = x;
  else error(ERR40, dir->name);
}



/*************************************************
*                 EPS                            *
*************************************************/

/* This is synonym for "output eps", which existed before PDF output was 
implemented. It's small enough not to bother with trying to combine it with the 
"output" directive. */

static void
eps(void)
{
if (movement_count == 1) 
  {
  if (!PDF || !PDFforced) 
    {
    print_imposition = pc_EPS;
    PDF = FALSE; 
    PSforced = TRUE; 
    }
  else error(ERR188, "eps", "-pdf");    
  } 
else error(ERR40, dir->name);
}



/*************************************************
*               Gracespacing                     *
*************************************************/

/* Read up to two fixed point numbers, second defaulting to the first; "+" and
"-" can be used to adjust values. */

static void
gracespacing(void)
{
int32_t arg;
int32_t value = 0;
BOOL wasrelative = FALSE;

if (read_c == '+' || read_c == '-')
  {
  value = curmovt->gracespacing[0];
  wasrelative = TRUE;
  }
if (!read_expect_integer(&arg, TRUE, TRUE)) return;
curmovt->gracespacing[0] = value + arg;

read_sigc();
if (read_c == ',') read_nextsigc();
if (!isdigit(read_c) && read_c != '+' && read_c != '-')
  {
  curmovt->gracespacing[1] = wasrelative?
    curmovt->gracespacing[1] + arg :
    curmovt->gracespacing[0];
  return;
  }

value = (read_c == '+' || read_c == '-')? curmovt->gracespacing[1] : 0;
if (!read_expect_integer(&arg, TRUE, TRUE)) return;
curmovt->gracespacing[1] = value + arg;
}



/*************************************************
*               Halvenotes                       *
*************************************************/

static void
halvenotes(void)
{
if (curmovt->notenum > 1) curmovt->notenum /= 2; else curmovt->noteden *= 2;
curmovt->time = read_scaletime(curmovt->time_unscaled);
}



/*************************************************
*            Hyphenstring                        *
*************************************************/

static void
hyphenstring(void)
{
curmovt->hyphenstring = string_read(font_rm, TRUE);
}



/*************************************************
*              Justify                           *
*************************************************/

static void
justify(void)
{
uint8_t yield = just_none;
for (;;)
  {
  if (isalpha(read_c))
    {
    size_t backup_i = read_i;
    uint32_t backup_c = read_c;
    read_nextword();
    if (Ustrcmp(read_wordbuffer, "top") == 0)         yield |= just_top;
    else if (Ustrcmp(read_wordbuffer, "bottom") == 0) yield |= just_bottom;
    else if (Ustrcmp(read_wordbuffer, "left") == 0)   yield |= just_left;
    else if (Ustrcmp(read_wordbuffer, "right") == 0)  yield |= just_right;
    else if (Ustrcmp(read_wordbuffer, "all") == 0)    yield |= just_all;
    else
      {
      read_i = backup_i;
      read_c = backup_c;
      break;
      }
    }
  else break;
  read_sigc();
  }
curmovt->justify = yield;
}


/*************************************************
*                 Key                            *
*************************************************/

/* After setting the key for this movement, ensure the transpose data is
updated, in case there are transposed text strings in the subsequent heading or
footing lines. */

static void
key(void)
{
curmovt->key = read_key();
(void)transpose_key(curmovt->key);
}



/*************************************************
*                 Keytranspose                   *
*************************************************/

static void
keytranspose(void)
{
int32_t x, y;
keytransstr *k;
uint32_t oldkey = read_key();

if (oldkey == key_N)
  {
  error(ERR8, "transposable key signature");
  return;
  }

/* Find any existing setting; if found, it gets overwritten. */

for (k = main_keytranspose; k != NULL; k = k->next)
  {
  if (k->oldkey == oldkey) break;
  }

/* No existing setting, create a new one. */

if (k == NULL)
  {
  int i;
  k = mem_get(sizeof(keytransstr));
  k->oldkey = oldkey;
  k->next = main_keytranspose;
  main_keytranspose = k;
  for (i = 0; i < 24; i++)
    {
    k->newkeys[i] = KEY_UNSET;
    k->letterchanges[i] = 0;
    }
  }

/* Now read the data. Transposition is in semitones for backwards
compatibility, but one day quarter tones may be allowed, so we actually work in
quarter tones. */

for (;;)
  {
  read_sigc();

  if (!isdigitorsign(read_c)) return;
  if (!read_expect_integer(&x, FALSE, TRUE)) return;

  x *= 2;  /* Quarter tones */

  while (x > 23) x -= 24;
  while (x < 0) x += 24;
  read_sigc();

  if (read_c != '=')
    {
    error(ERR8, "'='");
    break;
    }

  read_nextc();
  k->newkeys[x] = read_key();

  if (read_c != '/')
    {
    error(ERR8, "'/'");
    break;
    }

  read_nextc();
  if (!read_expect_integer(&y, FALSE, TRUE)) break;
  if (y < -6 || y > 6)
    {
    error(ERR8, "number in the range -6 to +6");
    break;
    }

  if (abs(y) > x + 1)
    {
    error(ERR46, y, x);
    break;
    }

  k->letterchanges[x] = y;
  }

/* We get here only if there's been an error after setting up x. */

k->newkeys[x] = oldkey;
k->letterchanges[x] = 0;
}



/*************************************************
*                 Landscape                      *
*************************************************/

static void
landscape(void)
{
uint32_t temp = curmovt->linelength;
curmovt->linelength = main_pagelength;
main_pagelength = temp;
if (main_sheetsize == sheet_A5) main_pagelength -= 28000;

temp = main_sheetwidth;
main_sheetwidth = main_sheetdepth;
main_sheetdepth = temp;
main_landscape = TRUE;
if (movement_count != 1) error(ERR40, dir->name);
}



/*************************************************
*                Layout                          *
*************************************************/

static void
layout(void)
{
uint16_t temp[MAX_LAYOUT * 2];
usint stack[MAX_LAYOUT_STACK];
usint ptr = 0;
usint level = 0;

for (;;)
  {
  int32_t value;

  read_sigc();

  /* Reached end of layout list. */

  if (!isdigit(read_c))
    {
    if (ptr == 0 || level > 0) { error(ERR8, "number"); return; }

    /* Final item is always repeat back to start */

    temp[ptr++] = lv_repeatptr;
    temp[ptr++] = 0;

    /* Save in correct size piece of store */

    curmovt->layout = mem_get(ptr*sizeof(uint16_t));
    memcpy(curmovt->layout, temp, ptr*sizeof(uint16_t));
    return;
    }

  /* Value must be > 0 */

  (void)read_expect_integer(&value, FALSE, FALSE);
  if (value == 0)
    {
    error(ERR13, "zero value changed to 1");
    value = 1;
    }

  /* If a number is followed by '(' it is a repeat count */

  read_sigc();
  if (read_c == '(')
    {
    read_nextc();
    temp[ptr++] = lv_repeatcount;
    temp[ptr++] = value;
    stack[level++] = ptr;
    }

  /* Else it is a barcount value, with varying terminators. If none of the
  specials, it does nothing, and another number will be a continuation, while
  anything else is the next directive. There may be a number of these
  terminators. */

  else
    {
    temp[ptr++] = lv_barcount;
    temp[ptr++] = value;

    for (;;)
      {
      if (read_c == ',') { read_nextsigc(); break; }

      /* Close bracket is the end of a repeat. Check nesting. It can be
      followed by comma, semicolon, or another bracket. */

      else if (read_c == ')')
        {
        if (level == 0) { error(ERR8, "closing bracket not"); return; } else
          {
          temp[ptr++] = lv_repeatptr;
          temp[ptr++] = stack[--level];
          }
        read_nextsigc();
        }

      /* Semicolon generates a new page item */

      else if (read_c == ';')
        {
        temp[ptr++] = lv_newpage;
        read_nextsigc();
        }

      /* Anything else, just carry on with the big loop */

      else break;
      }
    }
  }
}



/*************************************************
*             Makekey                            *
*************************************************/

/* Read the definition of a custom key signature. A maximum of MAX_KEYACCS
accidentals are supported. */

static void
makekey(void)
{
int32_t i, n;
uint8_t *kp;

if (toupper(read_c) != 'X' ||
    (read_nextc(), !read_expect_integer(&n, FALSE, FALSE)) ||
    n == 0 || n > MAX_XKEYS)
  {
  error(ERR44, MAX_XKEYS);
  return;
  }

kp = &(keysigtable[key_X + n - 1][0]);

for (i = 0;; i++)
  {
  usint ac;

  read_sigc();
  if (strchr("#$%", read_c) == NULL) break;

  if (i >= MAX_KEYACCS)
    {
    error_skip(ERR45, '\n', MAX_KEYACCS);
    break;
    }

  if (read_c == '%')
    {
    ac = ac_nt;
    }
  else
    {
    int32_t peek_c = main_readbuffer[read_i];
    ac = (read_c == '#')? ac_sh : ac_fl;
    if (peek_c == read_c)
      {
      ac++;
      read_i++;
      }
    else if (peek_c == '-')
      {
      ac--;
      read_i++;
      }
    }

  read_nextc();
  if (!read_expect_integer(&n, FALSE, FALSE)) break;
  if (n > 9)
    {
    error(ERR8, "number between 0 and 9");
    break;
    }

  /* The "makekey" directive numbers accidental positions from 0 for the bottom
  line, so we must add 1 because the new way of handling key signatures has
  zero for the space below the bottom line. Each item in a key signature list
  has an accidental in the top four bits and a position in the bottom four. */

  kp[i] = (ac << 4) | (n + 1);
  }

/* Mark the end of the list */

kp[i] = ks_end;
}



/*************************************************
*             Maxbeamslope                       *
*************************************************/

static void
maxbeamslope(void)
{
if (!read_expect_integer(&(curmovt->maxbeamslope[0]), TRUE, FALSE)) return;
if (!read_expect_integer(&(curmovt->maxbeamslope[1]), TRUE, FALSE)) return;
}



/*************************************************
*              Midichannel                       *
*************************************************/

static void
midichannel(void)
{
int32_t channel;

/* A channel number is always expected */

if (!read_expect_integer(&channel, FALSE, FALSE)) return;
if (channel < 1 || channel > MIDI_MAXCHANNEL)
  {
  error(ERR48, "channel", channel, MIDI_MAXCHANNEL);
  channel = 1;
  }

/* Remember which channels have been set for this movement. */

mac_setbit(curmovt->midichanset, channel);

/* Deal with an optional voice setting */

if (string_read_plain())
  {
  int voicenumber;
  if (read_stringbuffer[0] == 0)
    voicenumber = 129;   /* >= 128 => don't do MIDI voice setting */
  else
    {
    if (read_stringbuffer[0] == '#') voicenumber = Uatoi(read_stringbuffer+1);
      else voicenumber = read_getmidinumber(midi_voicenames,
        read_stringbuffer, US"voice");
    if (voicenumber < 1 || voicenumber > 128)
      {
      error(ERR48, "voice", 128);
      voicenumber = 1;
      }
    }

  curmovt->midivoice[channel-1] = voicenumber - 1;

  /* There may be an optional volume setting */

  if (read_c == '/')
    {
    int32_t vol;
    read_nextc();
    if (read_expect_integer(&vol, FALSE, FALSE))
      {
      if (vol > 15) error(ERR8, "number between 0 and 15");
        else (curmovt->midichannelvolume)[channel-1] = vol;
      }
    }
  }

/* Deal with an optional stave list */

read_sigc();
if (isdigit(read_c))
  {
  int i;
  int pitch = 0;
  uint64_t map;
  uschar *endptr;
  enum error_number n;

  n = read_stavelist(main_readbuffer + read_i - 1, &endptr, &map, NULL);
  if (n != 0)
    {
    error(n);
    read_i = main_readlength;
    read_c = '\n';
    }
  else
    {
    read_i = endptr - main_readbuffer;
    read_nextc();
    }

  /* Deal with optional 'pitch' forcing */

  if (string_read_plain())
    {
    if (read_stringbuffer[0] == 0) pitch = 0;  /* => don't do MIDI pitch forcing */
      else if (read_stringbuffer[0] == '#') pitch = Uatoi(read_stringbuffer+1);
        else pitch = read_getmidinumber(midi_percnames, read_stringbuffer,
          US"percussion instrument");
    }

  /* Now update the per-stave data */

  for (i = 1; i <= MAX_STAVE; i++)
    {
    if ((map & (1lu << i)) != 0)
      {
      if (pitch != 0) curmovt->midinote[i] = pitch;
      curmovt->midichannel[i] = channel;
      }
    }
  }
}



/*************************************************
*               Midistart                        *
*************************************************/

/* The mf_midistart flag indicates that the data was obtained for this
movement. This prevents it being freed more than once when it is copied to a
subsequent movement. */

static void
midistart(void)
{
usint max = 0;
usint count = 0;
uint8_t *temp = NULL;

/* Just in case there are two occurrences */

if (MFLAG(mf_midistart))
  {
  free(curmovt->midistart);
  curmovt->midistart = NULL;
  curmovt->flags &= ~mf_midistart;
  }

for (;;)
  {
  int32_t value;

  read_sigc();
  if (!isdigit(read_c))
    {
    if (count == 0) { error(ERR8, "number"); return; }
    temp[0] = count;
    curmovt->midistart = temp;
    curmovt->flags |= mf_midistart;
    return;
    }

  /* Ensure there's enough memory */

  if (++count >= max)
    {
    max += MIDI_START_CHUNKSIZE;
    temp = realloc(temp, max*sizeof(uint8_t));
    if (temp == NULL)
      error(ERR0, "re-", "MIDI start buffer", max*sizeof(uint8_t));
    }

  (void)read_expect_integer(&value, FALSE, FALSE);
  if (value < 0 || value > 255) error(ERR8, "number in range 0-255");
  temp[count] = value;
  }
}



/*************************************************
*                  Miditempo                     *
*************************************************/

static void
miditempo(void)
{
BOOL barerror = FALSE;
int i = 0;
uint32_t lastbar = 0;
uint32_t list[2*MIDI_MAXTEMPOCHANGE];

movt_int();           /* Read a single number for the base tempo */

curmovt->miditempochanges = NULL;  /* Just in case */
read_sigc();
if (read_c == ',') read_nextsigc();

/* Now look for additional data giving tempo changes within a movement. Logical
bar numbers can have a fractional part. They are stored in two 16-bit parts. */

while (isdigit(read_c))
  {
  int32_t tempo;
  uint32_t bar = read_barnumber();

  if (read_c != '/')
    {
    error(ERR8, "/");
    break;
    }
  read_nextc();
  if (!read_expect_integer(&tempo, FALSE, FALSE)) break;

  if (bar <= lastbar && !barerror)
    {
    error(ERR51);
    barerror = TRUE;
    }
  lastbar = bar;

  if (i >= 2 * MIDI_MAXTEMPOCHANGE)
    {
    error(ERR50, MIDI_MAXTEMPOCHANGE);
    break;
    }

  list[i++] = bar;
  list[i++] = tempo;

  read_sigc();
  if (read_c == ',') read_nextsigc();
  }

if (i > 0)
  {
  curmovt->miditempochanges = mem_get((i+1) * sizeof(uint32_t));
  memcpy(curmovt->miditempochanges, list, i * sizeof(uint32_t));
  curmovt->miditempochanges[i] = UINT32_MAX;
  }
}



/*************************************************
*              Miditranspose                     *
*************************************************/

static void
miditranspose(void)
{
while (isdigit(read_c))
  {
  int32_t amount;
  uint32_t stave = read_usint();
  read_sigc();

  if (read_c != '/')
    {
    error(ERR8, "/");
    return;
    }

  read_nextc();
  if (!read_expect_integer(&amount, FALSE, TRUE)) return;
  read_sigc();
  (curmovt->miditranspose)[stave] = amount;
  if (read_c == ',') read_nextc();
  read_sigc();
  }
}



/*************************************************
*               Midivolume                       *
*************************************************/

static void
midivolume(void)
{
int i, v;

if (!read_expect_integer(&v, FALSE, FALSE)) return;  /* Default setting */

if (v > 15)
  {
  error(ERR8, "Number between 0 and 15");
  return;
  }

for (i = 1; i <= MAX_STAVE; i++) (curmovt->midistavevolume)[i] = v;

/* Now look for additional data giving individual stave volumes */

read_sigc();
if (read_c == ',') read_nextsigc();

while (isdigit(read_c))
  {
  uint32_t stave = read_usint();

  if (stave > MAX_STAVE) { error(ERR10); stave = 1; }
  if (read_c != '/')
    {
    error(ERR8, "/");
    break;
    }
  read_nextc();
  if (!read_expect_integer(&v, FALSE, FALSE)) break;

  if (v > 15)
    {
    error(ERR8, "Number between 0 and 15");
    break;
    }

  (curmovt->midistavevolume)[stave] = v;
  read_sigc();
  if (read_c == ',') read_nextsigc();
  }
}



/*************************************************
*               Notespacing                      *
*************************************************/

static void
notespacing(void)
{
int i;
int32_t f;
read_sigc();

/* Adjust movement note spacings by multiplication */

if (read_c == '*')
  {
  read_nextc();
  if (!read_expect_integer(&f, TRUE, FALSE)) return;
  if (read_c == '/')
    {
    int32_t d;
    read_nextc();
    if (!read_expect_integer(&d, TRUE, FALSE)) return;
    f = mac_muldiv(f, 1000, d);
    }
  for (i = 0; i < NOTETYPE_COUNT; i++)
    curmovt->note_spacing[i] = (f * curmovt->note_spacing[i])/1000;
  }

/* Set absolute note spacings and make current */

else
  {
  for (i = 0; i < NOTETYPE_COUNT; i++)
    {
    if (!isdigit(read_c)) return;
    if (!read_expect_integer(&f, TRUE, FALSE)) return;
    curmovt->note_spacing[i] = read_absnotespacing[i] = f;
    read_sigc();
    if (read_c == ',') read_nextsigc();
    }
  }
}



/*************************************************
*                Output                          *
*************************************************/

static void
output(void)
{
if (movement_count != 1) { error(ERR40, dir->name); return; }
read_nextword();
read_sigc();

if (Ustrcmp(read_wordbuffer, "eps") == 0)
  {
  if (!PDF || !PDFforced) 
    {
    print_imposition = pc_EPS;
    PDF = FALSE;
    EPSforced = TRUE;  
    } 
  else error(ERR188, "output eps", "PDF");    
  } 
else if (Ustrcmp(read_wordbuffer, "pdf") == 0)
  {
  if (!PSforced && !EPSforced)
    {
    PDF = PDFforced = TRUE;
    print_imposition = pc_normal;  
    }
  else error(ERR188, "output pdf", "PostScript");
  } 
else if (Ustrcmp(read_wordbuffer, "ps") == 0 || 
         Ustrcmp(read_wordbuffer, "postscript") == 0)
  {
  if (!PDFforced && !EPSforced)
    {
    PDF = FALSE;
    PSforced = TRUE;
    print_imposition = pc_normal;  
    }  
  else error(ERR188, "output ps", PDFforced? "PDF" : "eps");
  }
else error(ERR8, "\"eps\", \"pdf\", or \"ps\"");    
}




/*************************************************
*               Page                             *
*************************************************/

static void
page(void)
{
if (movement_count != 1) { error(ERR40, dir->name); return; }
if (!read_expect_integer((int32_t *)(&page_firstnumber), FALSE, FALSE)) return;
read_sigc();
page_increment = 1;
if (!isdigit(read_c)) return;
(void)read_expect_integer((int32_t *)(&page_increment), FALSE, FALSE);
}



/*************************************************
*             PMW version check                  *
*************************************************/

static void
pmwversion(void)
{
BOOL ok = FALSE;
int c = '=';
int32_t v, vv;
float f;

if (read_c == '>' || read_c == '<' || read_c == '=')
  {
  c = read_c;
  read_nextc();
  if (read_c == '=')
    {
    c = (c << 8) | read_c;
    read_nextc();
    }
  }
if (!read_expect_integer(&v, TRUE, FALSE)) return;

(void)sscanf(PMW_VERSION, "%f", &f);
vv = (int32_t)(f * 1000.0);

switch (c)
  {
  case '<':          ok = vv < v;  break;
  case ('<'<<8)|'=': ok = vv <= v; break;
  case '=':
  case ('='<<8)|'=': ok = vv == v; break;
  case ('>'<<8)|'=': ok = vv >= v; break;
  case '>':          ok = vv > v;  break;
  }

if (!ok) error(ERR53, c, string_format_fixed(v), PMW_VERSION);  /* Hard */
}



/*************************************************
*                Printkey                        *
*************************************************/

static void
printkey(void)
{
dirstr *d;
pkeystr *p = mem_get(sizeof(pkeystr));
p->next = main_printkey;
p->movt_number = movement_count;
main_printkey = p;
p->key = read_key();

read_nextword();
d = read_stave_searchdirlist(TRUE);
if (d == NULL)
  {
  error_skip(ERR8, '\n', "clef name");
  return;
  }

p->clef = d->arg1;
p->string = string_read(font_mf, TRUE);
read_sigc();
p->cstring = (read_c == '"')? string_read(font_mf, TRUE) : empty_string;
}



/*************************************************
*                Printtime                       *
*************************************************/

/* Local subroutine to deal with one PMW string possibly followed by /s or /S
and a number.

Arguments:
  s          pointer to where to put a pointer to the string
  sizeptr    pointer to where to put the font offset

Returns:     TRUE if all goes well; FALSE on error
*/

static BOOL
ptstring(uint32_t **s, uint8_t *sizeptr)
{
*s = string_read(curmovt->fonttype_time, TRUE);
if (*s == NULL) return FALSE;

if (read_c == '/')
  {
  int32_t size;
  read_nextc();
  if (read_c == 's')
    {
    read_nextc();
    if (!read_expect_integer(&size, FALSE, FALSE)) return FALSE;
    if ((size -= 1) >= UserFontSizes) return error(ERR75, UserFontSizes);
    }
  else if (read_c == 'S')
    {
    read_nextc();
    if (!read_expect_integer(&size, FALSE, FALSE)) return FALSE;
    if ((size -= 1) >= FixedFontSizes) return error(ERR75, FixedFontSizes);
    size += UserFontSizes;
    }
  else
    {
    error(ERR8, "/s or /S");
    return FALSE;
    }
  *sizeptr = size;
  }
else *sizeptr = ff_offset_ts;

return TRUE;
}


/* The actual routine */

static void
printtime(void)
{
ptimestr *p = mem_get(sizeof(ptimestr));
p->next = main_printtime;
p->movt_number = movement_count;
main_printtime = p;
if ((p->time = read_time()) == 0) return;
if (!ptstring(&(p->top), &(p->sizetop))) return;
(void)ptstring(&(p->bot), &(p->sizebot));
}



/*************************************************
*              Rehearsalmarks                    *
*************************************************/

static void
rehearsalmarks(void)
{
while (isalpha(read_c))
  {
  read_nextword();
  read_sigc();

  if (Ustrcmp(read_wordbuffer, "linestartleft") == 0)
    {
    curmovt->flags |= mf_rehearsallsleft;
    continue;
    }

  if (Ustrcmp(read_wordbuffer, "nolinestartleft") == 0)
    {
    curmovt->flags &= ~mf_rehearsallsleft;
    continue;
    }

  if (Ustrcmp(read_wordbuffer, "roundboxed") == 0)
    curmovt->rehearsalstyle = text_boxed | text_boxrounded;
  else if (Ustrcmp(read_wordbuffer, "boxed") == 0)
    curmovt->rehearsalstyle = text_boxed;
  else if (Ustrcmp(read_wordbuffer, "ringed") == 0)
    curmovt->rehearsalstyle = text_ringed;
  else if (Ustrcmp(read_wordbuffer, "plain") == 0)
    curmovt->rehearsalstyle = 0;
  else error(ERR8, "\"boxed\", \"roundboxed\", \"ringed\", or \"plain\"");

  break;
  }

set_fontsize(offsetof(fontsizestr, fontsize_rehearse), TRUE);
set_fonttype(offsetof(movtstr, fonttype_rehearse));
}



/*************************************************
*               Repeatstyle                      *
*************************************************/

/* Repeat styles run from 0 to MAX_REPEATSTYLE, but can have 10 added to
request "wings". This causes the mf_repeatwings flag to be set. */

static void
repeatstyle(void)
{
int32_t x;
if (read_expect_integer(&x, FALSE, FALSE))
  {
  if (x >= 10)
    {
    x -= 10;
    curmovt->flags |= mf_repeatwings;
    }
  if (x <= MAX_REPEATSTYLE) curmovt->repeatstyle = x;
    else error(ERR174, MAX_REPEATSTYLE, MAX_REPEATSTYLE + 10);
  }
}



/*************************************************
*               Selectstaves                     *
*************************************************/

static void
selectstaves(void)
{
movt_list();
curmovt->select_staves |= 1;  /* Stave 0 always selected */

/* Give a warning if "selectstaves" conflicts with -s. */

if (main_selectedstaves != ~0uL &&
    main_selectedstaves != curmovt->select_staves)
  error(ERR169);
}



/*************************************************
*        Sheetdepth and sheetwidth               *
*************************************************/

static void
sheetdim(void)
{
main_sheetsize = sheet_unknown;
glob_int();
}


/*************************************************
*                 Sheetsize                      *
*************************************************/

/* Sheetsize handles sheetdepth and sheetwidth via a set of named paper sizes.
It also adjusts the default line length. */

static void
sheetsize(void)
{
usint i;
sheetstr *s;

if (movement_count != 1) error(ERR40, dir->name);
read_nextword();

for (i = 0; i < sheets_count; i++)
  {
  s = sheets_list + i;
  if (Ustrcmp(read_wordbuffer, s->name) == 0) break;
  }

if (i < sheets_count)
  {
  curmovt->linelength = s->linelength;
  main_pagelength = s->pagelength;
  main_sheetdepth = s->sheetdepth;
  main_sheetwidth = s->sheetwidth;
  main_sheetsize = s->sheetsize;
  }

else error(ERR8, "\"A3\", \"A4\", \"A5\", \"B5\", or \"letter\"");
}



/*************************************************
*               Startbracketbar                  *
*************************************************/

static void
startbracketbar(void)
{
if (isalpha(read_c))
  {
  read_nextword();
  if (Ustrcmp(read_wordbuffer, "join") == 0)
    curmovt->flags |= mf_startjoin;
  else if (Ustrcmp(read_wordbuffer, "nojoin") == 0)
    curmovt->flags &= ~mf_startjoin;
  else { error(ERR8, "\"join\" or \"nojoin\""); return; }
  }

read_expect_integer(&curmovt->startbracketbar, FALSE, FALSE);
}



/*************************************************
*               Startlinespacing                 *
*************************************************/

static void
startlinespacing(void)
{
usint i;
for (i = 0; i < 4; i++)
  {
  if (isdigit(read_c) || read_c == '-' || read_c == '+')
    {
    (void)read_expect_integer(&(curmovt->startspace[i]), TRUE, TRUE);
    if (read_c == ',') read_nextc();
    read_sigc();
    }
  else curmovt->startspace[i] = 0;
  }
}



/*************************************************
*               Stavesize                        *
*************************************************/

/* Use a new memory block so that the setting applies only to this and any
subsequent movements. */

static void
stavesize(void)
{
int32_t *stavesizes = mem_get((MAX_STAVE+1)*sizeof(int32_t));
memcpy(stavesizes, curmovt->stavesizes, (MAX_STAVE+1)*sizeof(int32_t));
curmovt->stavesizes = stavesizes;

while (isdigit(read_c))
  {
  int32_t size;
  uint32_t stave = read_usint();
  if (stave > MAX_STAVE) { error(ERR10); stave = 1; }
  if (read_c != '/') { error(ERR8, "/"); return; }
  read_nextc();
  if (!read_expect_integer(&size, TRUE, FALSE)) return;
  stavesizes[stave] = size;
  read_sigc();
  if (read_c == ',') read_nextsigc();
  }
}



/*************************************************
*                Stavespacing                    *
*************************************************/

static void
stavespacing(void)
{
usint stave;
int32_t space, ensure;
uint64_t done = 0;
uint32_t save_c = read_c;
size_t save_i = read_i;
BOOL all;

if (!isdigit(read_c)) { error(ERR8, "number"); return; }

/* Read the first number, check its terminator, then prepare to re-read
depending on what it is. */

stave = read_usint();
all = read_c != '/';
read_c = save_c;
read_i = save_i;

/* If the first argument is an integer not ending in '/', read it as a
dimension that applies to all staves. */

if (all)
  {
  space = read_fixed();
  for (int i = 1; i <= MAX_STAVE; i++) curmovt->stave_spacing[i] = space;
  if (read_c == ',') read_nextc();
  read_sigc();
  }

/* Now scan for stave/dimension pairs and triples */

while (isdigit(read_c))
  {
  stave = read_usint();
  if (read_c != '/')
    {
    error(ERR8, "/");
    return;
    }

  read_nextc();
  if (!read_expect_integer(&space, TRUE, FALSE)) return;
  read_sigc();
  if (read_c == '/')
    {
    ensure = space;
    read_nextc();
    if (!read_expect_integer(&space, TRUE, FALSE)) return;
    }
  else ensure = 0;

  if (stave == 0) error(ERR55, "stave spacing"); else
    {
    if (mac_isbit(done, stave)) error(ERR54, stave, US"stavespacing");
    mac_setbit(done, stave);
    curmovt->stave_spacing[stave] = space;
    curmovt->stave_ensure[stave] = ensure;
    }

  if (read_c == ',') read_nextc();
  read_sigc();
  }
}



/************************************************
*            Stem lengths                       *
************************************************/

static void
stemlengths(void)
{
usint i;
for (i = 2;
     i < NOTETYPE_COUNT && (read_c == '-' || read_c == '+' || isdigit(read_c));
     i++)
  {
  (void)read_expect_integer(&(curmovt->stemadjusts[i]), TRUE, TRUE);
  if (read_c == ',') read_nextc();
  read_sigc();
  }
}



/*************************************************
*                Stemswap                        *
*************************************************/

static void
stemswap(void)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "up") == 0)
  curmovt->stemswaptype = stemswap_up;
else if (Ustrcmp(read_wordbuffer, "down") == 0)
  curmovt->stemswaptype = stemswap_down;
else if (Ustrcmp(read_wordbuffer, "left") == 0)
  curmovt->stemswaptype = stemswap_left;
else if (Ustrcmp(read_wordbuffer, "right") == 0)
  curmovt->stemswaptype = stemswap_right;
else error(ERR8, "\"up\", \"down\", \"left\", or \"right\"");
}



/*************************************************
*              Stemswaplevel                     *
*************************************************/

static void
stemswaplevel(void)
{
int32_t level;
uint64_t done = 0;
uint32_t save_c = read_c;
size_t save_i = read_i;

/* Read the first number, check its terminator, then prepare to re-read
depending on what it is. */

if (!read_expect_integer(&level, FALSE, TRUE)) return;

/* If the first argument does not end in '/', it is a value that applies to all
staves. */

if (read_c != '/')
  {
  int i;
  for (i = 1; i <= MAX_STAVE; i++) curmovt->stemswaplevel[i] = level;
  read_sigcc();
  }
else
  {
  read_c = save_c;
  read_i = save_i;
  }

/* Now scan for stave/adjustment pairs */

while (isdigit(read_c))
  {
  usint stave = read_usint();
  if (read_c != '/')
    {
    error(ERR8, "/");
    return;
    }

  read_nextc();
  if (!read_expect_integer(&level, FALSE, TRUE)) return;

  if (stave == 0) error(ERR55, "stem swap level"); else
    {
    if (mac_isbit(done, stave)) error(ERR54, stave, US"stemswaplevel");
    mac_setbit(done, stave);
    curmovt->stemswaplevel[stave] = level;
    }

  read_sigcc();
  }
}



/*************************************************
*              Systemseparator                   *
*************************************************/

static void
systemseparator(void)
{
if (!read_expect_integer((int32_t *)(&(curmovt->systemseplength)), TRUE, FALSE))
  return;

read_nextsigc();
if (!isdigit(read_c)) return;
curmovt->systemsepwidth = read_fixed();

for (int i = 0; i < 3; i++)
  {
  int32_t value;
  read_nextsigc();
  if (!isdigit(read_c) && read_c != '+' && read_c != '-') return;
  if (!read_expect_integer(&value, TRUE, TRUE)) return;
  if (i == 0) curmovt->systemsepangle = value;
  else if (i == 1) curmovt->systemsepposx = value;
  else curmovt->systemsepposy = value;
  }
}



/*************************************************
*                Textfont                        *
*************************************************/

/* This also handles musicfont, if arg1 is non-zero. */

static void
textfont(void)
{
uint32_t flags = 0;
uint32_t fontid, n;

/* Font selection is permitted only in the first movement */

if (movement_count != 1) { error(ERR40, dir->name); return; }

/* If a font type is given (music font), use it, else read a font word or
"extra <n>", optionally followed by "include". */

if (dir->arg1 >= 0) fontid = dir->arg1; else
  {
  if ((fontid = font_readtype(FALSE)) == font_unknown) return;
  read_sigc();
  if (read_c != '"')
    {
    read_nextword();
    if (Ustrcmp(read_wordbuffer, "include") == 0) flags = ff_include; else
      {
      error(ERR8, "\"include\" or a string");
      return;
      }
    }
  }

/* Read the font name and see if is already in the font list. If it is, set its
number in the font table, enable include if required, and return. If not, set
it up as a new font. */

if (!string_read_plain()) { error(ERR8, "string"); return; }
n = font_search(read_stringbuffer);

if (n != font_unknown)
  {
  font_table[fontid] = n;
  font_list[n].flags |= flags;
  }
else
  {
  font_addfont(read_stringbuffer, fontid, flags);
  }
}



/*************************************************
*            Textsizes                           *
*************************************************/

/* Note: set_fontsize() does nothing if the next character is not a digit. */

static void
textsizes(void)
{
int i;
for (i = 0; i < UserFontSizes; i++)
  {
  set_fontsize(offsetof(fontsizestr, fontsize_text) + i*sizeof(fontinststr), TRUE);
  if (read_c == ',') read_nextc();
  }
read_sigc();
if (isdigit(read_c)) error_skip('\n', ERR63, UserFontSizes);
}



/*************************************************
*                     Time                       *
*************************************************/

static void
timesig(void)
{
uint32_t t = read_time();            /* returns 0 after giving error */
if (t != 0)
  {
  curmovt->time_unscaled = t;
  curmovt->time = read_scaletime(t);
  }
}



/*************************************************
*                Transpose                       *
*************************************************/

static void
transpose(void)
{
int32_t temp = curmovt->transpose;
if (temp == NO_TRANSPOSE) temp = 0;
movt_int();    /* Reads into curmovt->transpose */
curmovt->transpose = 2 * curmovt->transpose + temp;  /* Quarter tones */

if (abs(curmovt->transpose) > MAX_TRANSPOSE)
  error(ERR64, (temp == 0)? "":"accumulated ", curmovt->transpose/2,
    MAX_TRANSPOSE/2);  /* Hard error */

/* Set up transposing data in case this directive is followed by headings that
contain transposed key names. The key must be transposed in order to set up the
number of letter changes. */

active_transpose = curmovt->transpose;
(void)transpose_key(curmovt->key);
}



/*************************************************
*             Transposed accidental option       *
*************************************************/

static void
transposedacc(void)
{
read_nextword();
if (Ustrcmp(read_wordbuffer, "force") == 0) main_transposedaccforce = TRUE; else
  if (Ustrcmp(read_wordbuffer, "noforce") == 0) main_transposedaccforce = FALSE;
    else error(ERR8, "\"force\" or \"noforce\"");
}



/*************************************************
*               Transposed key                   *
*************************************************/

static void
transposedkey(void)
{
uint32_t oldkey = read_key();
read_nextword();
if (Ustrcmp(read_wordbuffer, "use") != 0) error(ERR8, "\"use\""); else
  {
  trkeystr *k = mem_get(sizeof(trkeystr));
  read_sigc();
  k->oldkey = oldkey;
  k->newkey = read_key();
  k->next = main_transposedkeys;
  main_transposedkeys = k;
  }
}



/*************************************************
*             Trill string                       *
*************************************************/

static void
trillstring(void)
{
uint32_t *s;
if (isdigit(read_c)) set_fontsize(offsetof(fontsizestr,fontsize_trill), TRUE);
s = string_read(font_rm, TRUE);
if (s != NULL) curmovt->trillstring = s;
}



/*************************************************
*            Heading directive list              *
*************************************************/

/* The code for the read_draw_definition() function that reads a drawing
description is in the draw.c module. */

#define oo offsetof

static dirstr headlist[] = {
  { "accadjusts",       accadjusts,     0, 0 },
  { "accspacing",       accspacing,     0, 0 },
  { "b2pffont",         b2pffont,       0, 0 },
  { "bar",              bar,            oo(movtstr,baroffset), int_u },
  { "barcount",         warningint,     0, 0 },
  { "barlinesize",      movt_int,       oo(movtstr,barlinesize), int_uf },
  { "barlinespace",     barlinespace,   oo(movtstr,barlinespace), int_rs+int_f },
  { "barlinestyle",     movt_int8,      oo(movtstr,barlinestyle), 5 },
  { "barnumberlevel",   barnumberlevel, oo(movtstr,barnumber_level), int_rs+int_f },
  { "barnumbers",       barnumbers,     0, 0 },
  { "beamendrests",     movt_flag,      mf_beamendrests, TRUE },
  { "beamflaglength",   movt_int,       oo(movtstr,beamflaglength), int_uf },
  { "beamthickness",    movt_int,       oo(movtstr,beamthickness), int_uf },
  { "bottommargin",     movt_int,       oo(movtstr,bottommargin), int_uf },
  { "brace",            movt_list,      oo(movtstr,bracelist), FALSE },
  { "bracestyle",       movt_int8,      oo(movtstr,bracestyle), 1 },
  { "bracket",          movt_list,      oo(movtstr,bracketlist), FALSE },
  { "breakbarlines",    breakbarlines,  oo(movtstr,breakbarlines), TRUE },
  { "breakbarlinesx",   breakbarlines,  oo(movtstr,breakbarlines), TRUE },
  { "breveledgerextra", movt_int,       oo(movtstr,breveledgerextra), int_uf},
  { "breverests",       movt_flag,      mf_breverests, TRUE },
  { "caesurastyle",     movt_int8,      oo(movtstr,caesurastyle), 1 },
  { "check",            movt_flag,      mf_check, TRUE },
  { "checkdoublebars",  movt_flag,      mf_checkdoublebars, TRUE },
  { "clefsize",         clefsize,       oo(fontsizestr,fontsize_midclefs), FALSE },
  { "clefstyle",        movt_int8,      oo(movtstr,clefstyle), 3 },
  { "clefwidths",       clefwidths,     0, 0 },
  { "codemultirests",   movt_flag,      mf_codemultirests, TRUE },
  { "copyzero",         copyzero,       0, 0 },
  { "cuegracesize",     movt_fontsize,  oo(fontsizestr,fontsize_cuegrace), FALSE },
  { "cuesize",          movt_fontsize,  oo(fontsizestr,fontsize_cue), FALSE },
  { "dotspacefactor",   movt_int,       oo(movtstr,dotspacefactor), int_uf },
  { "doublenotes",      doublenotes,    0, 0 },
  { "draw",      read_draw_definition,  0, 0 },
  { "drawbarlines",     glob_bool,      glob_drawbarlines, TRUE },
  { "drawstafflines",   drawstavelines, 0, 0 },
  { "drawstavelines",   drawstavelines, 0, 0 },
  { "endlinesluradjust",movt_int,       oo(movtstr,endlinesluradjust), int_f },
  { "endlineslurstyle", movt_int8,      oo(movtstr,endlineslurstyle), 1 },
  { "endlinetieadjust", movt_int,       oo(movtstr,endlinetieadjust), int_f },
  { "endlinetiestyle",  movt_int8,      oo(movtstr,endlinetiestyle), 1 },
  { "eps",              eps,            0, 0 },
  { "extenderlevel",    movt_int,       oo(movtstr,extenderlevel), int_f },
  { "fbsize",           movt_fontsize,  oo(fontsizestr,fontsize_text)+ff_offset_fbass*sizeof(fontinststr), TRUE },
  { "footing",          movt_headfoot,  oo(movtstr,footing), rh_footing },
  { "footnotesep",      movt_int,       oo(movtstr,footnotesep), int_f },
  { "footnotesize",     movt_fontsize,  oo(fontsizestr,fontsize_footnote), TRUE },
  { "gracesize",        movt_fontsize,  oo(fontsizestr,fontsize_grace), FALSE },
  { "gracespacing",     gracespacing,   0, 0 },
  { "gracestyle",       movt_int8,      oo(movtstr,gracestyle), 1 },
  { "hairpinlinewidth", movt_int,       oo(movtstr,hairpinlinewidth), int_rs+int_f },
  { "hairpinwidth",     movt_int,       oo(movtstr,hairpinwidth), int_rs+int_f },
  { "halfflatstyle",    movt_int8,      oo(movtstr,halfflatstyle), 1 },
  { "halfsharpstyle",   movt_int8,      oo(movtstr,halfsharpstyle), 1 },
  { "halvenotes",       halvenotes,     0, 0 },
  { "heading",          movt_headfoot,  oo(movtstr,heading), rh_heading },
  { "hyphenstring",     hyphenstring,   0, 0 },
  { "hyphenthreshold",  movt_int,       oo(movtstr,hyphenthreshold), int_rs+int_f },
  { "includepmwfont",   glob_bool,      glob_incpmwfont, TRUE },
  { "incpmwfont",       glob_bool,      glob_incpmwfont, TRUE },
  { "join",             movt_list,      oo(movtstr,joinlist), FALSE },
  { "joindotted",       movt_list,      oo(movtstr,joindottedlist), FALSE },
  { "justify",          justify,        0, 0 },
  { "key",              key,            0, 0 },
  { "keydoublebar",     movt_flag,      mf_keydoublebar, TRUE },
  { "keysinglebar",     movt_flag,      mf_keydoublebar, FALSE },
  { "keytranspose",     keytranspose,   0, 0 },
  { "keywarn",          movt_flag,      mf_keywarn, TRUE },
  { "landscape",        landscape,      0, 0 },
  { "lastfooting",      movt_headfoot,  oo(movtstr,lastfooting), rh_lastfooting },
  { "layout",           layout,         0, 0 },
  { "ledgerstyle",      movt_int8,      oo(movtstr,ledgerstyle), 1 },
  { "leftmargin",       movt_int,       oo(movtstr,leftmargin), int_f },
  { "linelength",       movt_int,       oo(movtstr,linelength), int_rs+int_f },
  { "longrestfont",     movt_font,      oo(movtstr,fonttype_longrest), oo(fontsizestr, fontsize_restct) },
  { "magnification",    glob_int,       glob_magnification, int_uf },
  { "makekey",          makekey,        0, 0 },
  { "maxbeamslope",     maxbeamslope,   0, 0 },
  { "maxvertjustify",   glob_int,       glob_maxvertjustify, int_uf },
  { "midichannel",      midichannel,    0, 0 },
  { "midifornotesoff",  glob_bool,      glob_midifornotesoff, TRUE },
  { "midistart",        midistart,      0, 0 },
  { "miditempo",        miditempo,      oo(movtstr,miditempo), int_u },
  { "miditranspose",    miditranspose,  0, 0 },
  { "midivolume",       midivolume,     0, 0 },
  { "midkeyspacing",    movt_int,       oo(movtstr,midkeyspacing), int_f },
  { "midtimespacing",   movt_int,       oo(movtstr,midtimespacing), int_f },
  { "musicfont",        textfont,       font_mf, 0 },
  { "nobeamendrests",   movt_flag,      mf_beamendrests, FALSE },
  { "nocheck",          movt_flag,      mf_check, FALSE },
  { "nocheckdoublebars",movt_flag,      mf_checkdoublebars, FALSE },
  { "nocodemultirests", movt_flag,      mf_codemultirests, FALSE },
  { "nokerning",        glob_bool,      glob_kerning, FALSE },
  { "nokeywarn",        movt_flag,      mf_keywarn, FALSE },
  { "nosluroverwarnings", movt_flag,    mf_tiesoverwarnings, FALSE },
  { "nospreadunderlay", movt_flag,      mf_spreadunderlay, FALSE },
  { "notespacing",      notespacing,    0, 0 },
  { "notime",           movt_flag,      mf_showtime, FALSE },
  { "notimebase",       movt_flag,      mf_showtimebase, FALSE },
  { "notimewarn",       movt_flag,      mf_timewarn, FALSE },
  { "nounderlayextenders",movt_flag,    mf_underlayextenders, FALSE },
  { "nowidechars",      glob_bool,      glob_nowidechars, FALSE },
  { "oldbeambreak",     warning,        0, 0 },
  { "oldrestlevel",     warning,        0, 0 },
  { "oldstemlength",    warning,        0, 0 },
  { "oldstretchrule",   warning,        0, 0 },
  { "output",           output,         0, 0 },
  { "overlaydepth",     movt_int,       oo(movtstr,overlaydepth), int_f },
  { "overlaysize",      movt_fontsize,  oo(fontsizestr,fontsize_text)+ff_offset_olay*sizeof(fontinststr), TRUE },
  { "page",             page,           0, 0 },
  { "pagefooting",      movt_headfoot,  oo(movtstr,pagefooting), rh_pagefooting },
  { "pageheading",      movt_headfoot,  oo(movtstr,pageheading), rh_pageheading },
  { "pagelength",       glob_int,       glob_pagelength, int_rs+int_f },
  { "pmwversion",       pmwversion,     0, 0 },
  { "printkey",         printkey,       0, 0 },
  { "printtime",        printtime,      0, 0 },
  { "rehearsalmarks",   rehearsalmarks, 0, 0 },
  { "repeatbarfont",    movt_font,      oo(movtstr,fonttype_repeatbar), oo(fontsizestr, fontsize_repno) },
  { "repeatstyle",      repeatstyle,    0, 0 },
  { "righttoleft",      glob_bool,      glob_righttoleft, TRUE },
  { "selectstaff",      selectstaves,   oo(movtstr,select_staves), TRUE },
  { "selectstave",      selectstaves,   oo(movtstr,select_staves), TRUE },
  { "selectstaves",     selectstaves,   oo(movtstr,select_staves), TRUE },
  { "sheetdepth",       sheetdim,       glob_sheetdepth, int_uf },
  { "sheetsize",        sheetsize,      0, 0 },
  { "sheetwidth",       sheetdim,       glob_sheetwidth, int_uf },
  { "shortenstems",     movt_int,       oo(movtstr,shortenstems), int_uf },
  { "sluroverwarnings", movt_flag,      mf_tiesoverwarnings, TRUE },
  { "smallcapsize",     movt_int,       oo(movtstr,smallcapsize), int_uf },
  { "staffsize",        stavesize,      0, 0 },
  { "staffsizes",       stavesize,      0, 0 },
  { "staffspacing",     stavespacing,   0, 0 },
  { "startbracketbar",  startbracketbar, 0, 0 },
  { "startlinespacing", startlinespacing, 0, 0 },
  { "startnotime",      movt_flag,      mf_startnotime, TRUE },
  { "stavesize",        stavesize,      0, 0 },
  { "stavesizes",       stavesize,      0, 0 },
  { "stavespacing",     stavespacing,   0, 0 },
  { "stemlengths",      stemlengths,    0, 0 },
  { "stemswap",         stemswap,       0, 0 },
  { "stemswaplevel",    stemswaplevel,  0, 0 },
  { "stretchrule",      warningint,     0, 0 },
  { "suspend",          movt_list,      oo(movtstr,suspend_staves), TRUE },
  { "systemgap",        movt_int,       oo(movtstr,systemgap), int_uf },
  { "systemseparator",  systemseparator,0, 0 },
  { "textfont",         textfont,       -1, 0 },
  { "textsize",         textsizes,      0, 0 },
  { "textsizes",        textsizes,      0, 0 },
  { "thinbracket",      movt_list,      oo(movtstr,thinbracketlist), FALSE },
  { "time",             timesig,        0, 0 },
  { "timebase",         movt_flag,      mf_showtimebase, TRUE },
  { "timefont",         movt_font,      oo(movtstr,fonttype_time),oo(fontsizestr,fontsize_text)+ff_offset_ts*sizeof(fontinststr) },
  { "timewarn",         movt_flag,      mf_timewarn, TRUE },
  { "topmargin",        movt_int,       oo(movtstr,topmargin), int_uf },
  { "transpose",        transpose,      oo(movtstr,transpose), 0 },
  { "transposedacc",    transposedacc,  0, 0 },
  { "transposedkey",    transposedkey,  0, 0 },
  { "trillstring",      trillstring,    0, 0 },
  { "tripletfont",      movt_font,      oo(movtstr,fonttype_triplet),oo(fontsizestr,fontsize_triplet) },
  { "tripletlinewidth", movt_int,       oo(movtstr,tripletlinewidth), int_uf },
  { "underlaydepth",    movt_int,       oo(movtstr,underlaydepth), int_f },
  { "underlayextenders",movt_flag,      mf_underlayextenders, TRUE },
  { "underlaysize",     movt_fontsize,  oo(fontsizestr,fontsize_text)+ff_offset_ulay*sizeof(fontinststr), TRUE },
  { "underlaystyle",    movt_int8,      oo(movtstr,underlaystyle), 1 },
  { "unfinished",       movt_flag,      mf_unfinished, TRUE },
  { "vertaccsize",      movt_fontsize,  oo(fontsizestr,fontsize_vertacc), FALSE }
};

static int headsize = sizeof(headlist)/sizeof(dirstr);



/*************************************************
*            Read PMW heading section            *
*************************************************/

/* This function is entered with the first line of the movement in the input
buffer, and the first significant character already in read_c. It exits on
reading the first unquoted [ in the input, or the end of the file. */

void
pmw_read_header(void)
{
TRACE("Read header directives: movement %d\n", movement_count);

for (;;)
  {
  dirstr *first, *last;

  read_nextword();
  if (read_wordbuffer[0] == 0)
    {
    if (read_c == '[' || read_c == ENDFILE) break;
    error_skip(ERR8, '\n', "header directive");
    continue;
    }

  /* Look up the word in the list of heading directives and if found, call the
  appropriate function. The directives whose names start with "play" are
  obsolete synonyms for those that start "midi" and are no longer documented.
  Give a warning, but continue to process them. */

  first = headlist;
  last  = first + headsize;

  while (last > first)
    {
    int c;
    dir = first + (last - first)/2;
    c = Ustrcmp(read_wordbuffer, dir->name);
    if (c == 0)
      {
      read_sigc();
      (dir->proc)();
      break;
      }
    if (c > 0) first = dir + 1; else last = dir;
    }

  if (last <= first) error_skip(ERR24, '\n', read_wordbuffer);
  }
}

/* End of pmw_read_header.c */
