/*************************************************
*             PMW drawing functions              *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: February 2021 */
/* This file last modified: December 2023 */

#include "pmw.h"


/* Types of entry in draw items; note that the table of required data on the
stack (stack_rqd) in must be kept in step with this. Also remember to add any
new operators into the check_ptr() function. */

enum {
  dr_accleft, dr_add, dr_and,
  dr_barnumber, dr_bra,
  dr_copy, dr_cos, dr_currentcolor, dr_currentdash, dr_currentgray,
    dr_currentlinewidth, dr_currentpoint, dr_curveto, dr_cvs,
  dr_def, dr_div, dr_draw, dr_dup,
  dr_end, dr_eq, dr_exch, dr_exit,
  dr_false, dr_fill, dr_fillretain, dr_fontsize,
  dr_gaptype, dr_gapx, dr_gapy, dr_ge, dr_gt,
  dr_headbottom, dr_headleft, dr_headright, dr_headtop,
  dr_if, dr_ifelse,
  dr_jump,
  dr_ket,
  dr_le, dr_leftbarx, dr_linebottom, dr_linelength, dr_lineto, dr_linetop,
    dr_loop, dr_lt,
  dr_magnification, dr_moveto, dr_mul,
  dr_ne, dr_neg, dr_not, dr_number,
  dr_or, dr_originx, dr_originy,
  dr_pagelength, dr_pagenumber, dr_pop, dr_pstack,
  dr_rcurveto, dr_repeat, dr_rlineto, dr_rmoveto, dr_roll,
  dr_setcolor, dr_setdash, dr_setgray, dr_setlinewidth, dr_show, dr_sin,
    dr_sqrt, dr_stavesize, dr_stavespace, dr_stavestart, dr_stembottom,
    dr_stemtop, dr_stringwidth, dr_stroke, dr_sub, dr_systemdepth,
  dr_text, dr_topleft, dr_translate, dr_true,
  dr_varname, dr_varref,
  dr_xor
};


/* This table contains the number and type of entries required to be on the
stack for each operator. Up to 8 values are catered for (though the maximum
used is 6). Each entry uses one nibble, starting from the least significant end
of the word. The values are the dd_* values defined in pmwhdr, namely:

  dd_any      1   any type of data
  dd_number   2   number
  dd_text     3   string
  dd_code     4   code fragment
  dd_varname  5   variable name
*/

static uint32_t stack_rqd[] = {
  0u,                 /* accleft */
  0x00000022u,        /* add */
  0x00000022u,        /* and */
  0u,                 /* barnumber */
  0u,                 /* bra */
  0x00000001u,        /* copy */
  0x00000002u,        /* cos */
  0u,                 /* currentcolor */
  0u,                 /* currentdash */
  0u,                 /* currentgray */
  0u,                 /* currentlinewidth */
  0u,                 /* currentpoint */
  0x00222222u,        /* curveto */
  0x00000023u,        /* cvs */
  0x00000051u,        /* def */
  0x00000022u,        /* div */
  0u,                 /* draw */
  0x00000001u,        /* dup */
  0u,                 /* end */
  0x00000022u,        /* eq */
  0x00000011u,        /* exch */
  0u,                 /* exit */
  0u,                 /* false */
  0u,                 /* fill */
  0u,                 /* fillretain */
  0x00000003u,        /* fontsize */
  0u,                 /* gaptype */
  0u,                 /* gapx */
  0u,                 /* gapy */
  0x00000022u,        /* ge */
  0x00000022u,        /* gt */
  0u,                 /* headbottom */
  0u,                 /* headleft */
  0u,                 /* headright */
  0u,                 /* headtop */
  0x00000024u,        /* if */
  0x00000244u,        /* ifelse */
  0u,                 /* jump */
  0u,                 /* ket */
  0x00000022u,        /* le */
  0u,                 /* leftbarx */
  0u,                 /* linebottom */
  0u,                 /* linelength */
  0x00000022u,        /* lineto */
  0u,                 /* linetop */
  0x00000004u,        /* loop */
  0x00000022u,        /* lt */
  0u,                 /* magnification */
  0x00000022u,        /* moveto */
  0x00000022u,        /* mul */
  0x00000022u,        /* ne */
  0x00000002u,        /* neg */
  0x00000002u,        /* not */
  0u,                 /* number */
  0x00000022u,        /* or */
  0u,                 /* originx */
  0u,                 /* originy */
  0u,                 /* pagelength */
  0u,                 /* pagenumber */
  0x00000001u,        /* pop */
  0u,                 /* pstack: no checking done */
  0x00222222u,        /* rcurveto */
  0x00000024u,        /* repeat */
  0x00000022u,        /* rlineto */
  0x00000022u,        /* rmoveto */
  0x00000022u,        /* roll */
  0x00000222u,        /* setcolor */
  0x00000022u,        /* setdash */
  0x00000002u,        /* setgray */
  0x00000002u,        /* setlinewidth */
  0x00000003u,        /* show */
  0x00000002u,        /* sin */
  0x00000002u,        /* sqrt */
  0u,                 /* stavesize */
  0u,                 /* stavespace */
  0u,                 /* stavestart */
  0u,                 /* stembottom */
  0u,                 /* stemtop */
  0x00000003u,        /* stringwidth */
  0u,                 /* stroke */
  0x00000022u,        /* sub */
  0u,                 /* systemdepth */
  0u,                 /* text */
  0u,                 /* topleft */
  0x00000022u,        /* translate */
  0u,                 /* true */
  0u,                 /* varname */
  0u,                 /* varref */
  0x00000022u         /* xor */
};



/*************************************************
*                Variables                       *
*************************************************/

static BOOL currentpoint;
static int32_t colour[3];
static int32_t dash[2];
static int level;
static int xp, yp, cp;
static usint next_variable = 0;
static drawitem draw_variables[MAX_DRAW_VARIABLE + 1];
static drawitem draw_stack[DRAW_STACKSIZE];



/*************************************************
*          Table of operators/variables etc      *
*************************************************/

typedef struct {
  const char *name;
  int  value;
} draw_op;

/* This table is used for compiling operators, and also for finding a name to
print in an error message. The names that don't correspond to operator names
are enclosed in <> so they don't affect the first usage. */

static draw_op draw_operators[] = {
  { "<number>",     dr_number },
  { "<text>",       dr_text },
  { "<name>",       dr_varname },
  { "<reference>",  dr_varref },
  { "accleft",      dr_accleft },
  { "add",          dr_add },
  { "and",          dr_and },
  { "barnumber",    dr_barnumber },
  { "copy",         dr_copy },
  { "cos",          dr_cos },
  { "currentcolor", dr_currentcolor },
  { "currentcolour",dr_currentcolor },
  { "currentdash",  dr_currentdash },
  { "currentgray",  dr_currentgray },
  { "currentgrey",  dr_currentgray },
  { "currentlinewidth", dr_currentlinewidth },
  { "currentpoint", dr_currentpoint },
  { "curveto",      dr_curveto },
  { "cvs",          dr_cvs },
  { "def",          dr_def },
  { "div",          dr_div },
  { "dup",          dr_dup },
  { "eq",           dr_eq },
  { "exch",         dr_exch },
  { "exit",         dr_exit },
  { "false",        dr_false },
  { "fill",         dr_fill },
  { "fillretain",   dr_fillretain },
  { "fontsize",     dr_fontsize },
  { "gaptype",      dr_gaptype },
  { "gapx",         dr_gapx },
  { "gapy",         dr_gapy },
  { "ge",           dr_ge },
  { "gt",           dr_gt },
  { "headbottom",   dr_headbottom },
  { "headleft",     dr_headleft },
  { "headright",    dr_headright },
  { "headtop",      dr_headtop },
  { "if",           dr_if },
  { "ifelse",       dr_ifelse},
  { "le",           dr_le },
  { "leftbarx",     dr_leftbarx },
  { "linebottom",   dr_linebottom },
  { "linegapx",     dr_gapx },
  { "linegapy",     dr_gapy },
  { "linelength",   dr_linelength },
  { "lineto",       dr_lineto },
  { "linetop",      dr_linetop },
  { "loop",         dr_loop },
  { "lt",           dr_lt },
  { "magnification",dr_magnification},
  { "moveto",       dr_moveto },
  { "mul",          dr_mul },
  { "ne",           dr_ne },
  { "neg",          dr_neg },
  { "not",          dr_not },
  { "or",           dr_or },
  { "originx",      dr_originx },
  { "originy",      dr_originy },
  { "pagelength",   dr_pagelength },
  { "pagenumber",   dr_pagenumber },
  { "pop",          dr_pop },
  { "pstack",       dr_pstack },
  { "rcurveto",     dr_rcurveto },
  { "repeat",       dr_repeat },
  { "rlineto",      dr_rlineto },
  { "rmoveto",      dr_rmoveto },
  { "roll",         dr_roll },
  { "setcolor",     dr_setcolor },
  { "setcolour",    dr_setcolor },
  { "setdash",      dr_setdash },
  { "setgray",      dr_setgray },
  { "setgrey",      dr_setgray },
  { "setlinewidth", dr_setlinewidth },
  { "show",         dr_show },
  { "sin",          dr_sin },
  { "sqrt",         dr_sqrt },
  { "stavesize",    dr_stavesize },
  { "stavespace",   dr_stavespace },
  { "stavestart",   dr_stavestart },
  { "stembottom",   dr_stembottom },
  { "stemtop",      dr_stemtop },
  { "stringwidth",  dr_stringwidth },
  { "stroke",       dr_stroke },
  { "sub",          dr_sub },
  { "systemdepth",  dr_systemdepth },
  { "topleft",      dr_topleft },
  { "translate",    dr_translate },
  { "true",         dr_true },
  { "xor",          dr_xor }
};

static int draw_operator_count = sizeof(draw_operators)/sizeof(draw_op);



/*************************************************
*         Print out the stack contents           *
*************************************************/

/* This function is called by "pstack" and is also used after some errors.

Arguments:
  s1         introductory string
  s2         terminating string

Returns:     nothing
*/

static void
do_pstack(const char *s1, const char *s2)
{
eprintf("%s", s1);
for (int i = 0; i < out_drawstackptr; i++)
  {
  drawtextstr *d;
  switch (draw_stack[i].dtype)
    {
    case dd_text:
    d = draw_stack[i].d.ptr;

    debug_string(d->text);
    if (d->xdelta != 0) eprintf("/%c%s", (d->xdelta > 0)? 'r':'l',
      sff(d->xdelta));
    if (d->ydelta != 0) eprintf("/%c%s", (d->ydelta > 0)? 'u':'d',
      sff(d->ydelta));

    if (d->size != 0) eprintf("/s%d", d->size + 1);
    if (d->rotate != 0) eprintf("/rot%s", sff(d->rotate));
    if ((d->flags & text_boxrounded) != 0) eprintf("/rbox");
      else if ((d->flags & text_boxed) != 0) eprintf("/box");
    if ((d->flags & text_centre) != 0) eprintf("/c");
    if ((d->flags & text_endalign) != 0) eprintf("/e");
    if ((d->flags & text_ringed) != 0) eprintf("/ring");
    eprintf(" ");
    break;

    case dd_code:
    eprintf("<code-block> ");
    break;

    default:
    eprintf("%s ", sff(draw_stack[i].d.val));
    break;
    }
  }
eprintf("\n%s", s2);
}



/*************************************************
*        Read a draw text string                 *
*************************************************/

/* This is also called when reading arguments for draw calls; hence its global
accessibility. It reads the string and parameters into a block of memory, and
yields its address.

Arguments:  none
Returns:    pointer to a textstr
*/

drawtextstr *
read_draw_text(void)
{
drawtextstr *textptr = mem_get(sizeof(drawtextstr));

if ((textptr->text = string_read(font_rm, TRUE)) == NULL) return NULL;

textptr->rotate = 0;
textptr->xdelta = 0;
textptr->ydelta = 0;
textptr->flags = 0;
textptr->size = 0;

while (read_c == '/')
  {
  int32_t delta, size;
  read_nextc();

  switch (read_c)
    {
    case 'b':
    if (Ustrncmp(main_readbuffer + read_i, "ox", 2) == 0)
      {
      read_i += 2;
      textptr->flags |= text_boxed;
      }
    else error(ERR8, "/box, /c, /e, /ring, /rot, or /s");
    read_nextc();
    break;

    case 'c':
    textptr->flags &= ~text_endalign;
    textptr->flags |= text_centre;
    read_nextc();
    break;

    case 'd':
    read_nextc();
    if (read_expect_integer(&delta, TRUE, TRUE)) textptr->ydelta -= delta;
    break;

    case 'e':
    textptr->flags &= ~text_centre;
    textptr->flags |= text_endalign;
    read_nextc();
    break;

    case 'l':
    read_nextc();
    if (read_expect_integer(&delta, TRUE, TRUE)) textptr->xdelta -= delta;
    break;

    case 'n':
    read_nextc();
    if (read_c == 'c') textptr->flags &= ~text_centre;
    else if (read_c == 'e') textptr->flags &= ~text_endalign;
    else error(ERR8, "/nc or /ne");
    read_nextc();
    break;

    case 's':
    read_nextc();
    if (read_expect_integer(&size, FALSE, FALSE))
      {
      if (--size < 0 || size >= UserFontSizes)
        { error(ERR75, UserFontSizes); size = 0; }
      textptr->size = size;
      }
    break;

    case 'S':
    read_nextc();
    if (read_expect_integer(&size, FALSE, FALSE))
      {
      if (--size < 0 || size >= FixedFontSizes)
        { error(ERR75, FixedFontSizes); size = 0; }
      textptr->size = UserFontSizes + size;
      }
    break;

    case 'r':
    if (Ustrncmp(main_readbuffer + read_i, "ot", 2) == 0)
      {
      int rotate;
      read_i += 2;
      read_nextc();
      if (read_expect_integer(&rotate, TRUE, TRUE)) textptr->rotate = rotate;
      break;
      }
    else if (Ustrncmp(main_readbuffer + read_i, "box", 3) == 0)
      {
      read_i += 3;
      read_nextc();
      textptr->flags |= text_boxed | text_boxrounded;
      break;
      }
    else if (Ustrncmp(main_readbuffer + read_i, "ing", 3) == 0)
      {
      read_i += 3;
      read_nextc();
      textptr->flags |= text_ringed;
      break;
      }
    else
      {
      read_nextc();
      if (read_expect_integer(&delta, TRUE, TRUE)) textptr->xdelta += delta;
      }
    break;

    case 'u':
    read_nextc();
    if (read_expect_integer(&delta, TRUE, TRUE)) textptr->ydelta += delta;
    break;

    default:
    error(ERR8, "/box, /c, /d, /e, /l, /r, /ring, /rot, /s, or /u");
    break;
    }
  }

return textptr;
}



/*************************************************
*                  Read a Draw function          *
*************************************************/

/* The function sets up a structure representing the function, and adds it to
the tree of draw functions.

Arguments: none
Returns:   nothing
*/

void
read_draw_definition(void)
{
tree_node *drawnode = mem_get(sizeof(tree_node));
drawitem *ptr;
size_t left;
int bracount = 0;

read_nextword();
if (read_wordbuffer[0] == 0) { error(ERR8, "name"); return; }

drawnode->name = mem_copystring(read_wordbuffer);
drawnode->data = mem_get(DRAW_CHUNKSIZE);

ptr = (drawitem *)drawnode->data;
left = DRAW_CHUNKSIZE/sizeof(drawitem);

/* Loop to read the contents of the draw function */

for (;;)
  {
  int type = -1;
  int32_t value = 0;
  void *pointer = NULL;

  read_sigc();

  /* Deal with numerical values; data put into "value" */

  if (isdigitorsign(read_c))
    {
    type = dr_number;
    (void)read_expect_integer(&value, TRUE, TRUE);
    }

  /* Deal with strings; data put into "pointer" */

  else if (read_c == '\"')
    {
    type = dr_text;
    pointer = read_draw_text();
    }

  /* Deal with brackets; no data */

  else if (read_c == '{')
    {
    read_nextc();
    bracount++;
    type = dr_bra;
    }
  else if (read_c == '}')
    {
    read_nextc();
    type = dr_ket;
    if (--bracount < 0) error(ERR81, drawnode->name);
    }

  /* Deal with variable names; data put into "value" is the variable index */

  else if (read_c == '/')
    {
    tree_node *node;
    type = dr_varname;
    read_nextc();

    if (!isalpha(read_c)) error(ERR8, "variable name"); else
      {
      read_nextword();
      node = tree_search(draw_variable_tree, read_wordbuffer);
      if (node == NULL)
        {
        if (next_variable > MAX_DRAW_VARIABLE)
          {
          error(ERR82, MAX_DRAW_VARIABLE + 1);
          next_variable--;
          }
        value = next_variable++;
        node = mem_get(sizeof(tree_node));
        node->name = mem_copystring(read_wordbuffer);
        node->value = value;
        tree_insert(&draw_variable_tree, node);
        }
      else value = node->value;
      }
    }

  /* Else it must be a command word */

  else if (isalpha(read_c))
    {
    read_nextword();
    if (Ustrcmp(read_wordbuffer, "enddraw") == 0)
      {
      ptr->d.val = dr_end;
      break;
      }

    /* Deal with "subroutine" call; value put into "pointer" */

    if (Ustrcmp(read_wordbuffer, "draw") == 0)
      {
      tree_node *node;
      type = dr_draw;
      read_nextword();
      node = tree_search(draw_tree, read_wordbuffer);
      if (node == NULL) error(ERR70, read_wordbuffer);
        else pointer = node;
      }

    /* Deal with normal operators and variables */

    else
      {
      draw_op *first = draw_operators;
      draw_op *last = first + draw_operator_count;

      /* Search for a standard variable or operator; if found, the type is set
      but there is no data. */

      while (last > first)
        {
        int c;
        draw_op *middle = first + (last - first)/2;
        c = Ustrcmp(middle->name, read_wordbuffer);
        if (c == 0) { type = middle->value; break; }
        if (c > 0) last = middle; else first = middle + 1;
        }
      }

    /* If haven't matched a standard variable or operator, try for a user
    variable. If found, the variable number is put into "value". */

    if (type < 0)
      {
      tree_node *node = tree_search(draw_variable_tree, read_wordbuffer);
      if (node != NULL)
        {
        type = dr_varref;
        value = node->value;
        }
      }

    /* Grumble if unrecognized word */

    if (type < 0) error(ERR83, read_wordbuffer);
    }

  /* Grumble if unrecognized input; error 10 skips to end of line */

  else
    {
    error(ERR8, "number, string, name, or curly bracket");
    continue;
    }

  /* We now have the data for a new item. Extend to new block if necessary */

  if (left < 4)
    {
    (ptr++)->d.val = dr_jump;
    ptr->d.ptr = mem_get(DRAW_CHUNKSIZE);
    ptr = (drawitem *)(ptr->d.ptr);
    left = DRAW_CHUNKSIZE/sizeof(drawitem);
    }

  /* Add this item to the "program". Numbers, variable names, and variable
  references have a numerical argument; strings and draw subroutine calls have
  an address argument. */

  (ptr++)->d.val = type;
  left--;
  if (type == dr_number || type == dr_varname || type == dr_varref)
    {
    (ptr++)->d.val = value;
    left--;
    }
  else if (type == dr_text || type == dr_draw)
    {
    (ptr++)->d.ptr = pointer;
    left--;
    }
  }

/* Insert into tree; give error if duplicate */

if (!tree_insert(&draw_tree, drawnode)) error(ERR16, drawnode->name);
}



/*************************************************
*     Generate an error while drawing            *
*************************************************/

/* This function always aborts after outputting error information.

Arguments:
  n          the error number
  s          a text string to pass to the error function
  t          the current drawing function node (for the name)

Returns:     nothing; all drawing errors are hard
*/

static void
draw_error(int n, const char *s, tree_node *t)
{
const char *inhf = (curstave < 0)? " in a heading or footing" : "";
error(n, s, t->name, inhf);
do_pstack("** Draw stack contents when error detected:\n", "\n");
exit(EXIT_FAILURE);
}



/*************************************************
*      Scale for a normal or grace/cue note      *
*************************************************/

/* This function is called for dimensions that are relative to the current
note. For normal notes, it just returns its argument. Otherwise, it scales
appropriately for cue notes and grace notes.

Argument:  the dimension to be scaled
Returns:   the scaled dimension
*/

static int
cuegrace_scale(int32_t value)
{
/* Not grace note */
if (n_length != 0)
  {
  return ((n_flags & nf_cuesize) == 0)? value :
    mac_muldiv(value, curmovt->fontsizes->fontsize_cue.size, 10000);
  }

/* Grace note */
else
  {
  int32_t size = ((n_flags & nf_cuesize) != 0)?
    curmovt->fontsizes->fontsize_cuegrace.size :
    curmovt->fontsizes->fontsize_grace.size;
  return mac_muldiv(value, size, 10000);
  }
}


/*************************************************
*   Set up an overdraw saved block for graphic   *
*************************************************/

/* This function saves the parameters of a graphic so that it can be drawn at
the end of the stave (overdrawn). Because each graphic is of variable length,
we can't easily use a caching scheme to re-use the data. As the amount of
memory for the path is relatively small, it doesn't matter if we use it just
once. The actual overdrawstr *is* remembered for re-use.

Arguments:
  thickness      the line thickness
  x              vector of x coordinates
  y              vector of y coordinates
  c              vector of control verbs (move, draw, etc)

Returns:         nothing
*/

static void
setup_overdraw(int32_t thickness, int32_t *x, int32_t *y, int *c)
{
overdrawstr *last = out_overdraw;
overdrawstr *new = mem_get_cached((void **)(&main_freeoverdrawstr),
  sizeof(overdrawstr));

if (last == NULL) out_overdraw = new; else
  {
  while (last->next != NULL) last = last->next;
  last->next = new;
  }

new->next = NULL;
new->texttype = FALSE;
memcpy(new->d.g.colour, colour, 3 * sizeof(int32_t));
new->d.g.dash[0] = dash[0];
new->d.g.dash[1] = dash[1];
new->d.g.linewidth = thickness;
new->d.g.ystave = out_ystave;

new->d.g.x = mem_get(xp*sizeof(int32_t));
memcpy(new->d.g.x, x, xp*sizeof(int32_t));

new->d.g.y = mem_get(yp*sizeof(int32_t));
memcpy(new->d.g.y, y, yp*sizeof(int32_t));

new->d.g.c = mem_get(cp*sizeof(int));
memcpy(new->d.g.c, c, cp*sizeof(int));
}



/*************************************************
*       Check validity of program pointer        *
*************************************************/

/* This function checks the validity of the argument of a conditional or
looping command, which is a sequence of draw commands, ending at the current
command pointer.

Arguments:
  p          the pointer to the conditional or looping command
  t          the node of the draw subfunction argument start

Returns:     nothing (hard error on failure)
*/

static void
check_ptr(drawitem *p, tree_node *t)
{
char buff[40];
drawitem *pp = (drawitem *)t->data;
while (pp != p && pp->d.val != dr_end)
  {
  switch ((pp++)->d.val)
    {
    case dr_jump:
    pp = (drawitem *)(pp->d.ptr);
    break;

    case dr_draw:        /* These have data in the stream, so skip one */
    case dr_number:
    case dr_text:
    case dr_varname:
    case dr_varref:
    pp++;
    break;

    case dr_accleft:     /* These have no data */
    case dr_add:
    case dr_and:
    case dr_barnumber:
    case dr_bra:
    case dr_copy:
    case dr_cos:
    case dr_currentcolor:
    case dr_currentdash:
    case dr_currentgray:
    case dr_currentlinewidth:
    case dr_currentpoint:
    case dr_curveto:
    case dr_cvs:
    case dr_def:
    case dr_div:
    case dr_dup:
    case dr_end:
    case dr_eq:
    case dr_exch:
    case dr_exit:
    case dr_false:
    case dr_fill:
    case dr_fillretain:
    case dr_fontsize:
    case dr_gaptype:
    case dr_gapx:
    case dr_gapy:
    case dr_ge:
    case dr_gt:
    case dr_headbottom:
    case dr_headleft:
    case dr_headright:
    case dr_headtop:
    case dr_if:
    case dr_ifelse:
    case dr_ket:
    case dr_le:
    case dr_leftbarx:
    case dr_linebottom:
    case dr_linelength:
    case dr_lineto:
    case dr_linetop:
    case dr_loop:
    case dr_lt:
    case dr_magnification:
    case dr_moveto:
    case dr_mul:
    case dr_ne:
    case dr_neg:
    case dr_not:
    case dr_or:
    case dr_originx:
    case dr_originy:
    case dr_pagelength:
    case dr_pagenumber:
    case dr_pop:
    case dr_pstack:
    case dr_rcurveto:
    case dr_repeat:
    case dr_rlineto:
    case dr_rmoveto:
    case dr_roll:
    case dr_setcolor:
    case dr_setdash:
    case dr_setgray:
    case dr_setlinewidth:
    case dr_show:
    case dr_sin:
    case dr_sqrt:
    case dr_stavesize:
    case dr_stavespace:
    case dr_stavestart:
    case dr_stembottom:
    case dr_stemtop:
    case dr_stringwidth:
    case dr_stroke:
    case dr_sub:
    case dr_systemdepth:
    case dr_topleft:
    case dr_translate:
    case dr_true:
    case dr_xor:
    break;

    default:
    sprintf(buff, " (bad value %d) ", pp[-1].d.val);
    draw_error(ERR152, buff, t);
    }
  }

if (p != pp) draw_error(ERR152, " (ended too soon) ", t);
}


/*************************************************
*   Interpret a draw function - recursive call   *
*************************************************/

/* This recursive function is called from the external interface. All errors
are fatal.

Arguments:
  t           the node for the function
  p           vector of draw commands; if NULL, take from node pointer
  x           vector for storing x path coordinates
  y           vector for storing y path coordinates
  c           vector for storing path drawing commands
  overflag    true if output is to be saved till after the stave is done

Returns:      FALSE on obeying "exit"; TRUE otherwise
*/

static BOOL
sub_draw(tree_node *t, drawitem *p, int32_t *x, int32_t *y, int *c, BOOL overflag)
{
if (p == NULL) p = (drawitem *)t->data;

while (p->d.val != dr_end)
  {
  int errornumber = -1;
  if (out_drawstackptr > DRAW_STACKSIZE - 4)
    errornumber = ERR148;
  else
    {
    int cc = 0;
    uint32_t xx = stack_rqd[p->d.val];
    while (xx != 0)
      {
      cc++;
      if (out_drawstackptr < cc)
        {
        errornumber = ERR149;    /* Insufficient items on stack */
        break;
        }
      if ((xx & 0x0f) != dd_any &&
          (xx & 0x0f) != draw_stack[out_drawstackptr - cc].dtype)
        {
        errornumber = ERR150;     /* Wrong type of item on stack */
        break;
        }
      xx >>= 4;
      }
    }

  if (errornumber >= 0)
    {
    const char *s = "???";
    for (int i = 0; i < draw_operator_count; i++)
      if (draw_operators[i].value == p->d.val)
        { s = draw_operators[i].name; break; }
    draw_error(errornumber, s, t);
    }

  switch ((p++)->d.val)
    {
    case dr_jump:
    p = (drawitem *)(p->d.ptr);
    break;

    case dr_bra:
      {
      int count = 1;
      draw_stack[out_drawstackptr].dtype = dd_code;
      draw_stack[out_drawstackptr++].d.ptr = p;
      while (p->d.val != dr_end && (p->d.val != dr_ket || --count > 0))
        {
        int type = (p++)->d.val;
        if (type == dr_jump) p = (drawitem *)(p->d.ptr);
        else if (type == dr_bra) count++;
        else if (type == dr_draw || type == dr_varref || type == dr_number ||
          type == dr_text || type == dr_varname) p++;
        }
      if ((p++)->d.val != dr_ket) draw_error(ERR151, " ", t);
      }
    break;

    case dr_ket:
    return TRUE;

    case dr_if:
      {
      drawitem *pp = draw_stack[--out_drawstackptr].d.ptr;
      check_ptr(pp, t);
      if (draw_stack[--out_drawstackptr].d.val != 0 &&
        !sub_draw(t, pp, x, y, c, overflag)) return FALSE;
      }
    break;

    case dr_ifelse:
      {
      drawitem *pp2 = draw_stack[--out_drawstackptr].d.ptr;
      drawitem *pp1 = draw_stack[--out_drawstackptr].d.ptr;
      check_ptr(pp1, t);
      check_ptr(pp2, t);
      if (!sub_draw(t, (draw_stack[--out_drawstackptr].d.val != 0)? pp1 : pp2,
            x, y, c, overflag))
        return FALSE;
      }
    break;

    case dr_number:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = (p++)->d.val;
    break;

    case dr_text:
    draw_stack[out_drawstackptr].dtype = dd_text;
    draw_stack[out_drawstackptr++].d.ptr = (p++)->d.ptr;
    break;

    case dr_varname:
    draw_stack[out_drawstackptr].dtype = dd_varname;
    draw_stack[out_drawstackptr++].d.val = (p++)->d.val;
    break;

    case dr_varref:
    draw_stack[out_drawstackptr++] = draw_variables[(p++)->d.val];
    break;

    case dr_accleft:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = n_maxaccleft;
    break;

    case dr_add:
    draw_stack[out_drawstackptr-2].d.val +=
      draw_stack[out_drawstackptr-1].d.val;
    out_drawstackptr--;
    break;

    case dr_and:
      {
      int a1 = draw_stack[--out_drawstackptr].d.val / 1000;
      int a2 = draw_stack[--out_drawstackptr].d.val / 1000;
      draw_stack[out_drawstackptr++].d.val = (a1 & a2)*1000;
      }
    break;

    /* In a heading or footing, the current barnumber is negative. */

    case dr_barnumber:
      {
      int a = 0, b = 0;
      if (curbarnumber >= 0)
        {
        char *bn = sfb(curmovt->barvector[curbarnumber]);
        if (strchr(bn, '.') == NULL) sscanf(bn, "%d", &a); else
          {
          sscanf(bn, "%d.%d", &a, &b);
          if (b < 10) b *= 100; else if (b < 100) b *= 10;
          }
        }
      draw_stack[out_drawstackptr].dtype = dd_number;
      draw_stack[out_drawstackptr++].d.val = a * 1000 + b;
      }
    break;

    case dr_copy:
      {
      int count = draw_stack[--out_drawstackptr].d.val / 1000;
      if (out_drawstackptr < count) draw_error(ERR149, "copy", t);
      memcpy(draw_stack + out_drawstackptr,
        draw_stack + out_drawstackptr - count, count * sizeof(drawitem));
      out_drawstackptr += count;
      }
    break;

    case dr_currentcolor:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = colour[0];
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = colour[1];
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = colour[2];
    break;

    case dr_currentdash:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = dash[0];
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = dash[1];
    break;

    /* The formula for converting a non-grey colour to grey is taken from the
    NTSC video standard that is used to convert colour television to black and
    white, as described in the PostScript documentation. The formula is:
    gray = 0.3*red + 0.59*green + 0.11*blue. */

    case dr_currentgray:
    draw_stack[out_drawstackptr].dtype = dd_number;
    if (colour[0] == colour[1] && colour[1] == colour[2])
      draw_stack[out_drawstackptr++].d.val = colour[0];
    else
      draw_stack[out_drawstackptr++].d.val =
        (30 * colour[0] + 59 * colour[1] + 11 * colour[2])/100;
    break;

    case dr_currentlinewidth:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_thickness;
    break;

    case dr_currentpoint:
    if (cp <= 0) draw_error(ERR153, "currentpoint", t);
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = x[xp-1] - draw_ox;
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = y[yp-1] - draw_oy;
    break;

    case dr_curveto:
      {
      if (!currentpoint) draw_error(ERR153, "curveto", t);
      for (int i = 6; i >= 2; i -= 2)
        {
        x[xp++] = draw_ox + draw_stack[out_drawstackptr - i].d.val;
        y[yp++] = draw_oy + draw_stack[out_drawstackptr - i + 1].d.val;
        }
      out_drawstackptr -= 6;
      c[cp++] = path_curve;
      }
    break;

    /* Waste a bit of memory for the PMW string that is created. Take the font
    from the first character in the placeholder string, if any. */

    case dr_cvs:
      {
      drawtextstr *d = (drawtextstr *)(draw_stack[--out_drawstackptr].d.ptr);
      int f = (d->text[0] != 0)? PFONT(d->text[0]) : font_rm;
      char *s = sff(draw_stack[--out_drawstackptr].d.val);
      size_t len = (strlen(s) + 1) * sizeof(uint32_t);
      uint32_t *sp = string_pmw(US s, f);

      d->text = mem_get(len);
      memcpy(d->text, sp, len);
      draw_stack[out_drawstackptr].dtype = dd_text;
      draw_stack[out_drawstackptr++].d.ptr = d;
      }
    break;

    case dr_def:
      {
      int32_t n = draw_stack[out_drawstackptr-2].d.val;
      if (n < 0 || n > MAX_DRAW_VARIABLE) draw_error(ERR154, "", t);
      draw_variables[n] = draw_stack[out_drawstackptr-1];
      }
    out_drawstackptr -= 2;
    break;

    case dr_div:
    if (draw_stack[out_drawstackptr-1].d.val == 0)
      draw_error(ERR155, "division by zero", t);
    draw_stack[out_drawstackptr-2].d.val =
      mac_muldiv(draw_stack[out_drawstackptr-2].d.val, 1000,
        draw_stack[out_drawstackptr-1].d.val);
    out_drawstackptr--;
    break;

    case dr_draw:
    if (++level > 20) draw_error(ERR147, " ", t);
    (void)sub_draw((tree_node *)((p++)->d.ptr), NULL, x, y, c, overflag);
    level--;
    break;

    case dr_dup:
    draw_stack[out_drawstackptr] = draw_stack[out_drawstackptr-1];
    out_drawstackptr++;
    break;

    case dr_eq:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val ==
        draw_stack[out_drawstackptr-1].d.val)? 1000 : 0;
    out_drawstackptr--;
    break;

    case dr_exch:
      {
      drawitem temp = draw_stack[out_drawstackptr-1];
      draw_stack[out_drawstackptr-1] = draw_stack[out_drawstackptr-2];
      draw_stack[out_drawstackptr-2] = temp;
      }
    break;

    case dr_exit:
    return FALSE;

    case dr_fill:
    if (!currentpoint) draw_error(ERR153, "fill", t);
    c[cp++] = path_end;
    if (overflag) setup_overdraw(-1, x, y, c); else ps_path(x, y, c, -1);
    cp = xp = yp = 0;
    currentpoint = FALSE;
    break;

    case dr_fillretain:
    if (!currentpoint) draw_error(ERR153, "fillretain", t);
    c[cp++] = path_end;
    if (overflag) setup_overdraw(-1, x, y, c); else ps_path(x, y, c, -1);
    break;

    case dr_fontsize:
      {
      drawtextstr *d = (drawtextstr *)(draw_stack[--out_drawstackptr].d.ptr);
      draw_stack[out_drawstackptr].dtype = dd_number;
      draw_stack[out_drawstackptr++].d.val =
        (curmovt->fontsizes->fontsize_text[d->size]).size;
      }
    break;

    case dr_false:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = 0;
    break;

    case dr_gaptype:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_gap;
    break;

    case dr_gapx:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_lgx;
    break;

    case dr_gapy:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_lgy;
    break;

    case dr_ge:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val >=
        draw_stack[out_drawstackptr-1].d.val)? 1000 : 0;
    out_drawstackptr--;
    break;

    case dr_gt:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val >
        draw_stack[out_drawstackptr-1].d.val)?
          1000 : 0;
    out_drawstackptr--;
    break;

    case dr_headbottom:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      (n_minpitch - 256)*out_pitchmagn - cuegrace_scale(2*out_stavemagn);
    break;

    case dr_headleft:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = n_invertleft?
      cuegrace_scale(6*out_stavemagn) : 0;
    break;

    case dr_headright:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      cuegrace_scale((n_invertright? 12 : 6)*out_stavemagn);
    break;

    case dr_headtop:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      (n_maxpitch - 256)*out_pitchmagn + cuegrace_scale(2*out_stavemagn);
    break;

    case dr_le:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val <=
        draw_stack[out_drawstackptr-1].d.val)?
          1000 : 0;
    out_drawstackptr--;
    break;

    case dr_leftbarx:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr].d.val = out_lastbarlinex - draw_ox;
    if (out_startlinebar) draw_stack[out_drawstackptr].d.val -= 6000;
    out_drawstackptr++;
    break;

    case dr_linebottom:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      ((n_minpitch & 4) != 0)? 2*out_stavemagn : 0;
    break;

    case dr_linelength:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = curmovt->linelength;
    break;

    case dr_lineto:
    if (!currentpoint) draw_error(ERR72, "lineto", t);
    y[yp++] = draw_oy + draw_stack[--out_drawstackptr].d.val;
    x[xp++] = draw_ox + draw_stack[--out_drawstackptr].d.val;
    c[cp++] = path_line;
    break;

    case dr_linetop:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      ((n_maxpitch & 4) != 0)? 2*out_stavemagn : 0;
    break;

    case dr_loop:
      {
      int count = 1000;
      drawitem *pp = draw_stack[--out_drawstackptr].d.ptr;
      check_ptr(pp, t);
      while (count-- > 0 && sub_draw(t, pp, x, y, c, overflag));
      }
    break;

    case dr_lt:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val <
        draw_stack[out_drawstackptr-1].d.val)?
          1000 : 0;
    out_drawstackptr--;
    break;

    case dr_magnification:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = main_magnification;
    break;

    case dr_moveto:
    y[yp++] = draw_oy + draw_stack[--out_drawstackptr].d.val;
    x[xp++] = draw_ox + draw_stack[--out_drawstackptr].d.val;
    c[cp++] = path_move;
    currentpoint = TRUE;
    break;

    case dr_mul:
    draw_stack[out_drawstackptr-2].d.val =
      mac_muldiv(draw_stack[out_drawstackptr-1].d.val,
        draw_stack[out_drawstackptr-2].d.val, 1000);
    out_drawstackptr--;
    break;

    case dr_ne:
    draw_stack[out_drawstackptr-2].d.val =
      (draw_stack[out_drawstackptr-2].d.val !=
        draw_stack[out_drawstackptr-1].d.val)?
          1000 : 0;
    out_drawstackptr--;
    break;

    case dr_neg:
    draw_stack[out_drawstackptr-1].d.val =
      -draw_stack[out_drawstackptr-1].d.val;
    break;

    case dr_not:
    draw_stack[out_drawstackptr-1].d.val =
      (~(draw_stack[out_drawstackptr-1].d.val/1000))*1000;
    break;

    case dr_or:
      {
      int a1 = draw_stack[--out_drawstackptr].d.val / 1000;
      int a2 = draw_stack[--out_drawstackptr].d.val / 1000;
      draw_stack[out_drawstackptr++].d.val = (a1 | a2)*1000;
      }
    break;

    case dr_originx:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_ox;
    break;

    case dr_originy:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = draw_oy;
    break;

    case dr_pagelength:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = main_pagelength;
    break;

    case dr_pagenumber:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = curpage->number * 1000;
    break;

    case dr_topleft:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = - draw_ox;
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =   out_ystave - draw_oy;
    break;

    case dr_pop:
    out_drawstackptr--;
    break;

    case dr_pstack:
    do_pstack("Draw stack contents (pstack command):\n", "");
    break;

    case dr_rcurveto:
      {
      if (!currentpoint) draw_error(ERR153, "rcurveto", t);
      for (int i = 0; i <= 2; i++)
        {
        x[xp+i] = x[xp-1] + draw_stack[out_drawstackptr + 2*i - 6].d.val;
        y[yp+i] = y[yp-1] + draw_stack[out_drawstackptr + 2*i - 5].d.val;
        }
      xp += 3;
      yp += 3;
      out_drawstackptr -= 6;
      c[cp++] = path_curve;
      }
    break;

    case dr_repeat:
      {
      drawitem *pp = draw_stack[--out_drawstackptr].d.ptr;
      int count = draw_stack[--out_drawstackptr].d.val / 1000;
      check_ptr(pp, t);
      while (count-- > 0 && sub_draw(t, pp, x, y, c, overflag));
      }
    break;

    case dr_rlineto:
    if (!currentpoint) draw_error(ERR153, "rlineto", t);
    y[yp] = y[yp-1] + draw_stack[--out_drawstackptr].d.val;
    yp++;
    x[xp] = x[xp-1] + draw_stack[--out_drawstackptr].d.val;
    xp++;
    c[cp++] = path_line;
    break;

    case dr_rmoveto:
    if (!currentpoint) draw_error(ERR153, "rmoveto", t);
    y[yp] = y[yp-1] + draw_stack[--out_drawstackptr].d.val;
    yp++;
    x[xp] = x[xp-1] + draw_stack[--out_drawstackptr].d.val;
    xp++;
    c[cp++] = path_move;
    break;

    case dr_roll:
      {
      int i, j;
      int amount = (draw_stack[--out_drawstackptr].d.val)/1000;
      int count = (draw_stack[--out_drawstackptr].d.val)/1000;

      if (out_drawstackptr < count) draw_error(ERR149, "roll", t);

      if (amount > 0) for (i = 0; i < amount; i++)
        {
        drawitem temp = draw_stack[out_drawstackptr - 1];
        for (j = 1; j < count; j++)
          draw_stack[out_drawstackptr-j] = draw_stack[out_drawstackptr-j-1];
        draw_stack[out_drawstackptr - count] = temp;
        }

      else for (i = 0; i < -amount; i++)
        {
        drawitem temp = draw_stack[out_drawstackptr - count];
        for (j = count; j > 1; j--)
          draw_stack[out_drawstackptr-j] = draw_stack[out_drawstackptr-j+1];
        draw_stack[out_drawstackptr - 1] = temp;
        }
      }
    break;

    case dr_setcolor:
    colour[2] = draw_stack[--out_drawstackptr].d.val;
    colour[1] = draw_stack[--out_drawstackptr].d.val;
    colour[0] = draw_stack[--out_drawstackptr].d.val;
    ps_setcolour(colour);
    break;

    case dr_setdash:
    dash[1] = draw_stack[--out_drawstackptr].d.val;
    dash[0] = draw_stack[--out_drawstackptr].d.val;
    ps_setdash(dash[0], dash[1]);
    break;

    case dr_setgray:
    colour[0] = draw_stack[--out_drawstackptr].d.val;
    colour[1] = colour[2] = colour[0];
    ps_setcolour(colour);
    break;

    case dr_setlinewidth:
    draw_thickness = mac_muldiv(draw_stack[--out_drawstackptr].d.val,
      out_stavemagn, 1000);
    break;

    case dr_sin:
    draw_stack[out_drawstackptr-1].d.val =
      (int)(sin((double)(draw_stack[out_drawstackptr-1].d.val)*3.14159/180000.0)
        * 1000.0);
    break;

    case dr_cos:
    draw_stack[out_drawstackptr-1].d.val =
      (int)(cos((double)(draw_stack[out_drawstackptr-1].d.val)*3.14159/180000.0)
        * 1000.0);
    break;

    case dr_sqrt:
    if (draw_stack[out_drawstackptr-1].d.val < 0)
      draw_error(ERR155, "negative argument for square root", t);
    else draw_stack[out_drawstackptr-1].d.val =
      (int)(sqrt((double)(draw_stack[out_drawstackptr-1].d.val)/1000.0)*1000.0);
    break;

    case dr_stringwidth:
    case dr_show:
      {
      int32_t width, height;
      drawtextstr *d = draw_stack[--out_drawstackptr].d.ptr;
      fontinststr local_fdata = curmovt->fontsizes->fontsize_text[d->size];
      fontinststr *fdata;

      local_fdata.size = mac_muldiv(local_fdata.size, out_stavemagn, 1000);
      fdata = (d->rotate == 0)? &local_fdata :
        font_rotate(&local_fdata, d->rotate);
      width = string_width(d->text, fdata, &height);

      /* If stringwidth, just return values */

      if (p[-1].d.val == dr_stringwidth)
        {
        draw_stack[out_drawstackptr].dtype = dd_number;
        draw_stack[out_drawstackptr++].d.val = width;
        draw_stack[out_drawstackptr].dtype = dd_number;
        draw_stack[out_drawstackptr++].d.val = height;
        }

      /* Else carry on to do the showing */

      else
        {
        int32_t xx = x[xp-1] + d->xdelta;
        int32_t yy = y[yp-1] + d->ydelta;
        uint32_t flags = d->flags;

        if (!currentpoint) draw_error(ERR153, "show", t);

        if ((flags & text_centre) != 0)
          {
          xx -= width/2;
          yy -= height/2;
          }
        else if ((flags & text_endalign) != 0)
          {
          xx -= width;
          yy -= height;
          }
        else
          {
          y[yp++] = yy + height;
          x[xp++] = xx + width;
          c[cp++] = path_move;
          }

        if (overflag)
          {
          overdrawstr *last = out_overdraw;
          overdrawstr *new = mem_get_cached((void **)(&main_freeoverdrawstr),
            sizeof(overdrawstr));

          if (last == NULL) out_overdraw = new; else
            {
            while (last->next != NULL) last = last->next;
            last->next = new;
            }
          new->next = NULL;
          new->texttype = TRUE;
          new->d.t.text = d->text;
          new->d.t.flags = flags;
          memcpy(new->d.t.colour, colour, 3 * sizeof(int32_t));
          new->d.t.xx = xx;
          new->d.t.yy = out_ystave - yy;
          new->d.t.fdata = *fdata;
          if (fdata->matrix != NULL)
            {
            memcpy(new->d.t.matrix, fdata->matrix, 6*sizeof(int32_t));
            new->d.t.fdata.matrix = new->d.t.matrix;
            }
          }

        else out_string(d->text, fdata, xx, out_ystave - yy, flags);
        }
      }
    break;

    case dr_stavesize:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = out_stavemagn;
    break;

    case dr_stavestart:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = (curstave < 0)? 0 :
      (out_sysblock->startxposition + out_sysblock->xjustify - draw_ox);
    break;

    case dr_stavespace:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val =
      out_sysblock->stavespacing[curstave];
    break;

    case dr_stembottom:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr].d.val = (n_minpitch - 260)*500;
    if ((n_flags & (nf_stem | nf_stemup)) == nf_stem)
      draw_stack[out_drawstackptr].d.val -= cuegrace_scale(12000 + n_stemlength);
    draw_stack[out_drawstackptr].d.val =
      mac_muldiv(draw_stack[out_drawstackptr].d.val, out_stavemagn, 1000);
    out_drawstackptr++;
    break;

    case dr_stemtop:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr].d.val = (n_maxpitch - 252)*500;
    if ((n_flags & (nf_stem | nf_stemup)) == (nf_stem | nf_stemup))
      draw_stack[out_drawstackptr].d.val += cuegrace_scale(12000+ n_stemlength);
    draw_stack[out_drawstackptr].d.val =
      mac_muldiv(draw_stack[out_drawstackptr].d.val, out_stavemagn, 1000);
    out_drawstackptr++;
    break;

    /*** Code is common with dr_show above
    case dr_stringwidth:
    ***/

    case dr_stroke:
    if (!currentpoint) draw_error(ERR153, "stroke", t);
    c[cp++] = path_end;
    if (overflag) setup_overdraw(draw_thickness, x, y, c);
      else ps_path(x, y, c, draw_thickness);
    cp = xp = yp = 0;
    currentpoint = FALSE;
    break;

    case dr_systemdepth:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = out_sysblock->systemdepth;
    break;

    case dr_sub:
    draw_stack[out_drawstackptr-2].d.val -= draw_stack[out_drawstackptr-1].d.val;
    out_drawstackptr--;
    break;

    case dr_translate:
    draw_oy += draw_stack[--out_drawstackptr].d.val;
    draw_ox += draw_stack[--out_drawstackptr].d.val;
    break;

    case dr_true:
    draw_stack[out_drawstackptr].dtype = dd_number;
    draw_stack[out_drawstackptr++].d.val = 1000;
    break;

    case dr_xor:
      {
      int32_t a1 = draw_stack[--out_drawstackptr].d.val / 1000;
      int32_t a2 = draw_stack[--out_drawstackptr].d.val / 1000;
      draw_stack[out_drawstackptr++].d.val = (a1 ^ a2)*1000;
      }
    break;
    }
  }

return TRUE;
}



/*************************************************
*    Prime stack and interpret a draw function   *
*************************************************/

/* This is the external interface to the drawing action.

Arguments:
  t          the node of the drawing function
  args       vector of arguments
  overflag   TRUE if the output is to be saved till after the stave is done

Returns:     nothing; all errors are hard
*/

void
out_dodraw(tree_node *t, drawitem *args, BOOL overflag)
{
int32_t x[100];
int32_t y[100];
int c[100];
int32_t save_colour[3];

if (args != NULL)
  for (int i = 1; i <= args[0].d.val; i++)
    draw_stack[out_drawstackptr++] = args[i];

xp = yp = cp = level = colour[0] = colour[1] = colour[2] =
  dash[0] = dash[1] = 0;
currentpoint = FALSE;
ps_getcolour(save_colour);
ps_setgray(0);
ps_setdash(0,0);
ps_setcapandjoin(0);
draw_thickness = 500;
(void)sub_draw(t, NULL, x, y, c, overflag);
ps_setcolour(save_colour);
ps_setcapandjoin(0);
}


/* End of draw.c */
