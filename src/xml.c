/*************************************************
*            MusicXML for PMW input              *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */

/* This module contains functions used while creating and processing a chain of
XML items. */


#include "pmw.h"


#ifdef NEVER
static uschar utf8[8];   /* Returned with UTF-8 string */
#endif


/*************************************************
*    Convert string to a number with fraction    *
*************************************************/

/* The argument is checked for consisting only of decimal digits, optionally
followed by a dot and a decimal fraction. The yield is in thousandths, that is,
milli-units.

Argument:  the string
Returns:   the number, or -1 on error
*/

static int
string_to_mils(uschar *s)
{
int yield = 0;
while (isdigit(*s)) yield = yield * 10 + *s++ - '0';
yield *= 1000;

if (*s == '.')
  {
  int m = 100;

  while (isdigit(*(++s)))
    {
    if (m > 0)
      {
      yield += (*s - '0') * m;
      m /= 10;
      }
    }
  }

return (*s == 0)? yield : -1;
}


/*************************************************
*           Convert string to a number           *
*************************************************/

/* The argument is checked for consisting only of decimal digits, except that
if ignore_fraction is true, a trailing fraction is ignored.

Argument:
  s                 the string
  ignore_fraction   TRUE to ignore fractional part

Returns:   the number, or -1 on error
*/

int
xml_string_to_number(uschar *s, BOOL ignore_fraction)
{
int yield = 0;
while (isdigit(*s)) yield = yield * 10 + *s++ - '0';
if (ignore_fraction && *s == '.') while (isdigit(*(++s))) {}
return (*s == 0)? yield : -1;
}



/*************************************************
*     Convert string to number with checking     *
*************************************************/

/* As above, but convert the result into a number, ignoring any fractional
part.

Arguments:
  s          string
  min        minimum legal value
  max        maximum legal value
  bad        what to return on error
  wasbad     error indicator

Returns:     legal value or bad
*/

int
xml_string_check_number(uschar *s, int min, int max, int bad, BOOL *wasbad)
{
if (s != NULL)
  {
  int m, yield;
  if (*s == '-')
    {
    m = -1;
    s++;
    }
  else m = +1;
  yield = xml_string_to_number(s, TRUE);
  if (yield >= 0)
    {
    yield *= m;
    if (yield >= min && yield <= max)
      {
      *wasbad = FALSE;
      return yield;
      }
    }
  }
*wasbad = TRUE;
return bad;
}


/*************************************************
*      Add attribute value entry to tree         *
*************************************************/

/* This is used for the unrecognized and ignored trees.

Arguments:

Returns:

*/

void
xml_add_attrval_to_tree(tree_node **tree, xml_item *i, xml_attrstr *a)
{
tree_node *p;
uschar buff[256];
(void)sprintf((char *)buff, "+%s=%s:%s", a->name, a->value, i->name);
p = tree_search(*tree, buff);
if (p == NULL)
  {
  p = mem_get(sizeof(tree_node) + Ustrlen(buff));
  p->name = mem_copystring(buff); 
  (void)tree_insert(tree, p);
  }
}



#ifdef NEVER
/*************************************************
*       Convert character value to UTF-8         *
*************************************************/

/* This function takes an integer value in the range 0 - 0x7fffffff
and encodes it as a UTF-8 character in 1 to 6 bytes.

Arguments:
  cvalue     the character value
  buffer     pointer to buffer for result - at least 6 bytes long

Returns:     number of characters placed in the buffer
*/

int
misc_ord2utf8(int cvalue, uschar *buffer)
{
register int i, j;
for (i = 0; i < 6; i++) if (cvalue <= utf8_table1[i]) break;
buffer += i;
for (j = i; j > 0; j--)
 {
 *buffer-- = 0x80 | (cvalue & 0x3f);
 cvalue >>= 6;
 }
*buffer = utf8_table2[i] | cvalue;
return i + 1;
}



/*************************************************
*            Format a fixed-point number         *
*************************************************/

/*  Fixed point numbers use 3 decimal places.

Arguments:  the fixed point number
Returns:    pointer to the formatted buffer
*/

uschar *
format_fixed(int n)
{
int count = 0;
div_t qr = div(abs(n), 1000);
if (n < 0) format_buffer[count++] = '-';
count += sprintf(CS format_buffer + count, "%d", qr.quot);
if (qr.rem != 0)
  {
  format_buffer[count++] = '.';
  if (qr.rem < 10)  format_buffer[count++] = '0';
  if (qr.rem < 100) format_buffer[count++] = '0';
  count += sprintf(CS format_buffer + count, "%d", qr.rem);
  while (format_buffer[count - 1] == '0') count--;
  format_buffer[count] = 0;
  }
return format_buffer;
}





/*************************************************
*         Find the value of a named entity       *
*************************************************/

/*

Arguments:
  attname      the entity name
  vptr         where to put the value ("" if not found)

Returns:       TRUE if found
*/

BOOL
entity_find_byname(uschar *attname, uschar **vptr)
{
entity_block *bot, *mid, *top;

bot = entity_list;
top = entity_list + entity_list_count;

while (top > bot)
  {
  int c;
  mid = bot + (top - bot)/2;
  c = Ustrcmp(mid->name, attname);
  if (c == 0) break;
  if (c < 0) bot = mid + 1; else top = mid;
  }

if (top > bot)
  {
  if (Ustrncmp(mid->value, "&#x", 3) == 0)
    {
    char *t2;
    unsigned int longvalue = strtoul(CS(mid->value + 3), &t2, 16);
    utf8[misc_ord2utf8(longvalue, utf8)] = 0;
    *vptr = utf8;
    }
  else *vptr = mid->value;
  return TRUE;
  }

return FALSE;
}
#endif


/*************************************************
*          Create a new item with defaults       *
*************************************************/

/*
Arguments:  item name
Returns:    pointer to the item
*/

xml_item *
xml_new_item(uschar *name)
{
xml_item *yield = mem_get(sizeof(xml_item));
yield->next = NULL;
yield->prev = NULL;
yield->partner = yield;
yield->linenumber = 0;
yield->flags = 0;
yield->p.attr = NULL;
Ustrcpy(yield->name, name);
return yield;
}



/*************************************************
*          Insert an item into the chain         *
*************************************************/

/* This function inserts a single item before another. It is assumed that
there is always a previous item.

Arguments:
  new          the item
  old          the item to insert before

Returns:       nothing
*/

void
xml_insert_item(xml_item *new, xml_item *old)
{
new->prev = old->prev;
new->next = old;
old->prev->next = new;
old->prev = new;
}


/*************************************************
*     Find next item of same type in a chain     *
*************************************************/

/*
Arguments:
  i         start of enclosing item
  p         current item of this type

Returns:    next item or NULL
*/

xml_item *
xml_find_next(xml_item *i, xml_item *p)
{
xml_item *end = i->partner;
if (end == i) end = NULL;
i = p->partner->next;
while (i != end)
  {
  if (Ustrcmp(p->name, i->name) == 0) return i;
  i = i->next;
  }
return NULL;
}



/*************************************************
*   Find previous item of same type in a chain   *
*************************************************/

/*
Arguments:
  i         start of enclosing item
  p         current item of this type

Returns:    next item or NULL
*/

xml_item *
xml_find_prev(xml_item *i, xml_item *p)
{
xml_item *x;
if (p->prev == i) return NULL;
x = p->prev->partner;
while (x != i)
  {
  if (Ustrcmp(p->name, x->name) == 0) return x;
  x = x->prev->partner;
  }
return NULL;
}



/*************************************************
*           Find an item in a chain              *
*************************************************/

/* The search stops at the partner of the initial item.

Arguments:
  i        where to start
  name     what to look for

Returns:   found item or NULL
*/

xml_item *
xml_find_item(xml_item *i, uschar *name)
{
xml_item *end;
end = i->partner;
for (; i != end; i = i->next)
  {
  if (Ustrcmp(i->name, name) == 0) return i;
  }
return NULL;
}



/*************************************************
*           Find a attribute for an item         *
*************************************************/

/* This function scans a list of attributes, looking for one by name.

Arguments:
  i           points to the item
  name        the name of the attribute

Returns:       pointer to the attribute, or NULL
*/

xml_attrstr *
xml_find_attr(xml_item *i, uschar *name)
{
xml_attrstr *p;
for (p = i->p.attr; p != NULL; p = p->next)
  {
  if (Ustrcmp(p->name, name) == 0) break;
  }
return p;
}



/*************************************************
*          Find the start of a part              *
*************************************************/

/*
Arguments:
  i         where to start looking
  name      name of part

Returns:    found part start or NULL
*/

xml_item *
xml_find_part(xml_item *i, uschar *name)
{
xml_attrstr *p;
while (i != NULL)
  {
  i = xml_find_item(i, US"part");
  if (i == NULL) return NULL;
  p = xml_find_attr(i, US"id");
  if (p != NULL && Ustrcmp(p->value, name) == 0) return i;
  i = i->partner->next;
  }
return NULL;
}



/*************************************************
*         Get string value for given item        *
*************************************************/

uschar *
xml_get_this_string(xml_item *i)
{
i = xml_find_item(i, US"#TEXT");
return (i == NULL)? US"" : i->p.txtblk->string;
}



#ifdef NEVER
/*************************************************
*         Get number value for given item        *
*************************************************/

/* A fractional part is ignored by string_check_number().

Arguments:

Returns:

*/

int
xml_get_this_number(item *i, int min, int max, int bad, BOOL moan)
{
uschar *s;
BOOL wasbad;
int yield;
i = xml_find_item(i, US"#TEXT");
if (i == NULL) return bad;
s = i->p.txtblk->string;
yield = misc_string_check_number(s, min, max, bad, &wasbad);
if (wasbad && moan) Eerror(i, ERR23, s);
return yield;
}
#endif



/*************************************************
*         Get string value for named item        *
*************************************************/

/* This function searches for an item of the given name contained within an
outer item. Then it searches for text data within the inner item.

Arguments:
  i          the outer item
  name       inner item name
  bad        value to return if not found
  moan       TRUE to give error

Returns:     string, or NULL
*/

uschar *
xml_get_string(xml_item *i, uschar *name, uschar *bad, BOOL moan)
{
xml_item *yield = xml_find_item(i, name);
if (yield == NULL)
  {
  if (moan) xml_Eerror(i, ERR24, name);
  return bad;
  }
return xml_get_this_string(yield);
}


/*************************************************
*       Get number value for given item          *
*************************************************/

/* Call xml_get_string(), then convert the result into a number. A fractional
part is ignored by misc_string_check_number().

Arguments:
  i          the outer item
  name       inner item name
  min        minimum legal value
  max        maximum legal value
  bad        what to return on error
  moan       TRUE to give error message

Returns:     legal value or bad
*/

int
xml_get_number(xml_item *i, uschar *name, int min, int max, int bad, BOOL moan)
{
int yield;
BOOL wasbad;
uschar *s = xml_get_string(i, name, NULL, moan);
if (s == NULL) return bad;
yield = xml_string_check_number(s, min, max, bad, &wasbad);
if (wasbad) xml_Eerror(i, ERR23, s);
return yield;
}



/*************************************************
*          Set a new value for a number          *
*************************************************/

/* Call xml_get_string(), then replace the string with the string value of the 
given number. This is used to change staff numbers for coupled staves, so we 
know that the number is either 1 or 2. FIXME: perhaps this needs better 
checking and an error if name not found.

Arguments:
  i          the outer item
  name       inner item name
  n          the new number
  
Returns:     nothing
*/

void
xml_set_number(xml_item *i, uschar *name, int n)
{
uschar *s = xml_get_string(i, name, NULL, TRUE);
if (s != NULL) *s = n + '0';
} 


/*************************************************
*       Get mils value for given item            *
*************************************************/

/* Call xml_get_string(), then convert the result into a number with optional
fractional part.

Arguments:
  i          the outer item
  name       inner item name
  min        minimum legal value
  max        maximum legal value
  bad        what to return on error
  moan       TRUE to give error message

Returns:     legal value or bad
*/

int
xml_get_mils(xml_item *i, uschar *name, int min, int max, int bad, BOOL moan)
{
int yield;
uschar *s = xml_get_string(i, name, NULL, moan);
if (s == NULL) return bad;
yield = string_to_mils(s);
if (yield >= 0 && yield >= min && yield <= max) return yield;
if (moan) xml_Eerror(i, ERR23, s);
return bad;
}


/*************************************************
*      Get string value for given attribute      *
*************************************************/

/*
Arguments:
  i          the item
  name       name of attribute
  bad        what to return on error
  moan       TRUE to give error message

Returns:     legal value or bad
*/

uschar *
xml_get_attr_string(xml_item *i, uschar *name, uschar *bad, BOOL moan)
{
xml_attrstr *p = xml_find_attr(i, name);
if (p != NULL) return p->value;
if (moan) xml_Eerror(i, ERR32, name);
return bad;
}



/*************************************************
*      Get number value for given attribute      *
*************************************************/

/* A fractional part is ignored by misc_string_check_number().

Arguments:
  i          the item
  name       name of attribute
  min        minimum legal value
  max        maximum legal value
  bad        what to return on error
  moan       TRUE to give error message

Returns:     legal value or bad
*/

int
xml_get_attr_number(xml_item *i, uschar *name, int min, int max, int bad,
  BOOL moan)
{
int yield;
BOOL wasbad = TRUE;
uschar *s = xml_get_attr_string(i, name, NULL, moan);
if (s == NULL) return bad;
yield = xml_string_check_number(s, min, max, bad, &wasbad);
if (wasbad) xml_Eerror(i, ERR23, s);
return yield;
}



/*************************************************
*     Get mils value for given attribute         *
*************************************************/

/* As above, but convert the result into a number with optional fractional
part.

Arguments:
  i          the outer item
  name       the name of the attribute
  min        minimum legal value
  max        maximum legal value
  bad        what to return on error
  moan       TRUE to give error message

Returns:     legal value or bad
*/

int32_t
xml_get_attr_mils(xml_item *i, uschar *name, int min, int max, int bad, 
  BOOL moan)
{
int32_t yield;
uschar *s = xml_get_attr_string(i, name, NULL, moan);
if (s == NULL) return bad;
yield = string_to_mils(s);
if (yield >= 0 && yield >= min && yield <= max) return yield;
if (moan) xml_Eerror(i, ERR23, s);
return bad;
}


/* End of xml.c */
