/*************************************************
*         Command line argument decoder          *
*************************************************/

/* Copyright (c) Philip Hazel, 1991-2021 */

/* Written by Philip Hazel, starting November 1991. Used in several projects. */

/* This file last modified: November 2021 */



/*************************************************
*    Command Line Decoding Routines (rdargs)     *
*************************************************/

/* Command line argument decoding. These routines are not specific to PMW2.
They provide a generalized means of decoding a command line that consists of
keyword/value pairs. The style is very much that of the Tripos operating
system. The externally-visible routine is called rdargs(), and it takes the
following arguments:

(1) int argc and (2) char **argv are passed on directly from main();

(3) char *keystring is a string of keywords, possibly qualified, in the form

      keyitem[,keyitem]*

    where each keyitem is of the form

      keyname[=keyname]*[/a][/k][/n|/s][/<d>]

    Keynames separated by '=' are synonyms. The flags are as follows:

    /a    Item must always be present in the command line.
    /k    Item, if present, must be keyed.
    /n    Item expects numeric integer argument(s).
    /s    Item has no argument; it is a switch.
    /<d>  Item may have up to <d> arguments, where <d> is a single digit.
    /m    Item must have maximum number of arguments

    A default value can be given for a numerical argument by following /n
    with '=' and a number, e.g. /n=0. The default will be returned if the
    keyword is present without a numeric value following it.

    A typical string might be "from/a,to/k,debug=d/s". Non-keyed items may
    only appear in the command line if all preceding keyed items in the
    keylist have been satisfied.

    A key may be specified as a single question mark. When this is done, any
    keyword specified on the command line is accepted, and its value is the
    particular keyword. This provides a method for dealing with Unix options
    strings starting with a minus and containing an arbitrary collection of
    characters. A question mark must always be the last keyword, since anything
    matches it, and the key string is searched from left to right.

    A key may also start with a question mark followed by other characters. In
    this case, any key on the command line whose initial characters match the
    characters following the question mark is accepted, and again the value is
    the keyword.

(4) arg_results *results is a pointer to a vector of structures into which the
    results of parsing the command line are placed.

(5) int rsize is the number of elements in results. There must be at least as
    many elements in the vector as there are keyitems in the string, plus
    extras for any that expect multiple arguments, and in any event, there must
    be a minimum of two, because in the event of certain errors, the space is
    used for constructing error messages and returning pointers to them. Each
    element contains:

    (a) int presence, which, on successful return, contains one of the values

        arg_present_not      item was not present on the command line
        arg_present_keyed    item was present and was keyed
        arg_present_unkeyed  item was present and was not keyed

    (b) int number and char *text, which contains either a number,
        for a numerical or switch argument, or a pointer for a string.

    For non-existent arguments the number field is set to zero (i.e. FALSE
    for switches) and the text field is set to NULL. The presence field need
    only be inspected for numerical arguments if it is necessary to distinguish
    between zero supplied explicitly and zero defaulted.

For multiply-valued items, the maximum number of results slots is reserved. At
least one argument is always expected. For example, if the keystring is
"from/4", then slots 0 to 3 are associated with the potential four values for
the "from" argument. How many there are can be detected by inspecting the
presence values. Unkeyed multiple values cannot be split by keyed values, e.g.
the string "a b -to c d" is not a valid for a keystring "from/3,to/k". The "d"
item will be faulted.

If rdargs is successful, it returns zero. Otherwise it returns non-zero, and
the first two results elements contain pointers in their text fields to
two error message strings. If the error involves a particular keyword, the
first string contains that keyword, otherwise it is NULL. The yield value is -1
if the error is not related to a particular command line item; otherwise it
contains the index of the item at fault. */


#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "rdargs.h"

#define FALSE 0
#define TRUE  1

/* Flags for key types */

#define rdargflag_a       1*256
#define rdargflag_k       2*256
#define rdargflag_s       4*256
#define rdargflag_n       8*256
#define rdargflag_q      16*256
#define rdargflag_d      32*256
#define rdargflag_m      64*256

/* Mask for presence value of control word */

#define argflag_presence_mask   255

/* Size for keyoffset vector (maximum number of keys). */

#define KEYOFFSET_SIZE           64


/*************************************************
*          Search keystring for i-th keyword     *
*************************************************/

/* Given the index of a key (counting from zero), find the name of the key.
This routine is used only for generating the name in cases of error.

Arguments:
  number     key number
  keys       the key string
  word       a vector in which to place the name

Returns:     word
*/

static char *
findkey(int number, const char *keys, char *word)
{
int ch, i;
int j = 0;
int k = 0;
word[k++] = '-';
for (i = 0; i < number; i++) while (keys[j++] != ',');
while ((ch = keys[j++]) != 0 && ch != ',' && ch != '/' && ch != '=')
  word[k++] = ch;
word[k] = 0;
return word;
}


/*************************************************
*          Search keystring for keyword          *
*************************************************/

/* Given a keyword starting with "-", find its index in the keystring. Keys are
separated by ',' in the string, and there may be syonyms separated by '='.

Arguments:
  keys        the keystring
  s           the argument

Returns:      the key index number, or -1 if not found
*/

static int
findarg(const char *keys, char *s)
{
int matching = TRUE;
int argnum = 0;
int i = 0;
int j = 1;
int ch;

while ((ch = keys[i++]) != 0)
  {
  if (matching)
    {
    if (ch == '?')
      {
      for (;;)
        {
        if ((ch = keys[i++]) == 0 || ch == ',' || ch == '=') return argnum;
        if (ch != s[j++])
          {
          matching = FALSE;
          break;
          }
        }
      }

    else
      {
      if ((ch == '=' || ch == '/' || ch == ',') &&
          s[j] == 0) return argnum;
      if (ch != s[j++]) matching = FALSE;
      }
    }

  if (ch == ',' || ch == '=')
    {
    matching = TRUE;
    j = 1;
    if (ch == ',') argnum++;
    }
  }

if (matching && s[j] == 0) return argnum;
return -1;
}


/*************************************************
*             Set up for error return            *
*************************************************/

/* Copy the keyword name and error message into the first two return string
slots. This function is the value of many "return" statements when errors are
detected.

Arguments:
  results      pointer to the results vector
  arg          argument name
  message      error message

Returns:       1, suitable for passing back as an error return
*/

static int
arg_error(arg_result *results, char *arg, const char *message)
{
results[0].text = arg;
results[1].text = message;
return 1;
}


/*************************************************
*           Set up zero or more values           *
*************************************************/

/* Zero values are allowed only if the defaulted flag is set.

Arguments:
  argc           the main argc value
  argv           the main argv value
  a_argindex     pointer to the argument index value, updated
  a_argnum       pointer to the results index value, updated
  argflags       argument type flags plus value count
  results        the results vector
  arg            the key name (for error message)
  present_value  the value to set in the presence field of the result
                   (arg_present_keyed or arg_present_unkeyed)

Returns:         zero on success, non-zero for error
*/

static int
arg_setup_values(int argc, char **argv, int *a_argindex, int *a_argnum,
  int argflags, arg_result *results, char *arg, int present_value)
{
int argnum = *a_argnum;
int argindex = *a_argindex;
int argcount = (argflags & 0x00FF0000) >> 16;   /* max args */
if (argcount == 0) argcount = 1;                /* default is 1 */

/* Loop to deal with the arguments */

for (;;)
  {
  char *nextstring;
  results[argnum].presence = present_value;

  /* Deal with a numerical or string value -- but if the first time for
  a defaulted item, skip so as to let the optional test work. */

  if ((argflags & rdargflag_d) != 0)        /* Default value exists */
    {
    argflags &= ~rdargflag_d;
    argcount++;                             /* Go round the loop once more */
    }
  else if ((argflags & rdargflag_n) != 0)   /* Numeric value */
    {
    char *endptr;
    results[argnum++].number =
      (int)strtol(argv[argindex++], &endptr, 0);
    if (*endptr)
      return arg_error(results, arg, "requires a numerical argument"),
        argindex;
    }
  else results[argnum++].text = argv[argindex++];

  /* If there are no more arguments on the line, or if we have read the maximum
  number for this keyword, break out of the loop. */

  if (argindex >= argc || (--argcount) < 1) break;

  /* Examine the next item on the line. */

  nextstring = argv[argindex];

  /* Check optional arguments. If a numerical argument is expected and it
  begins with a digit or a minus sign, followed by a digit, accept it.
  Otherwise accept it as a string unless it begins with a minus sign. */

  if ((argflags & rdargflag_m) == 0)
    {
    if ((argflags & rdargflag_n) != 0)
      {
      if (!isdigit(nextstring[0]) &&
        (nextstring[0] != '-' || !isdigit(nextstring[1]))) break;
      }
    else if (nextstring[0] == '-') break;
    }
  }

*a_argnum = argnum;         /* Update where we are on the line */
*a_argindex = argindex;
return 0;
}


/*************************************************
*              Decode argument line              *
*************************************************/

/* This is the procedure that is visible to the outside world. See comments at
the head of the file for a specification.

Arguments:
  argc        the argc value from main()
  argv        the argv value from main()
  keystring   the defining keystring
  results     where to put the results
  rsize       the number of elements in results

Returns:      zero on success, non-zero on failure
*/

int
rdargs(int argc, char **argv, const char *keystring, arg_result *results,
  int rsize)
{
int keyoffset[KEYOFFSET_SIZE];
int argmax = 0;
int argindex = 1;
int argcount = 1;
int keynumber = 0;
int i, ch;

/* We first scan the key string and create, in the presence field, flags
indicating which kind of key it is. The flags are disjoint from the presence
flags, which occupy the bottom byte. The assumption is that there is at least
one key! */

results[0].presence = arg_present_not;
results[0].number = 0;
results[0].text = NULL;
keyoffset[0] = 0;

i = -1;
while ((ch = keystring[++i]) != 0)
  {
  if (ch == '?') results[argmax].presence |= rdargflag_q;

  else if (ch == '/') switch(keystring[++i])
    {
    case 'a': results[argmax].presence |= rdargflag_a; break;
    case 'k': results[argmax].presence |= rdargflag_k; break;
    case 'm': results[argmax].presence |= rdargflag_m; break;
    case 's': results[argmax].presence |= rdargflag_s; break;

    case 'n':
      {
      results[argmax].presence |= rdargflag_n;
      if (keystring[i+1] == '=')
        {
        int n;
        results[argmax].presence |= rdargflag_d;  /* flag default exists */
        if (sscanf(keystring+i+2, "%d%n", &(results[argmax].number), &n) == 0)
          return arg_error(results, findkey(keynumber, keystring,
            (char *)(results+2)), "is followed by an unknown option"), -1;
        i += n + 1;
        }
      break;
      }

    default:
    if (isdigit(keystring[i]))
      {
      argcount = keystring[i] - '0';
      if (argcount == 0) argcount = 1;
      results[argmax].presence |= argcount << 16;
      }
    else return arg_error(results, findkey(keynumber, keystring,
      (char *)(results+2)), "is followed by an unknown option"), -1;
    }

  else if (ch == ',')
    {
    int j;
    for (j = 1; j < argcount; j++)
      {
      results[++argmax].presence = argflag_presence_mask;
      results[argmax].number = 0;
      results[argmax].text = NULL;
      }
    results[++argmax].presence = arg_present_not;
    results[argmax].number = 0;
    results[argmax].text = NULL;
    keynumber++;
    if (keynumber >= KEYOFFSET_SIZE)
      return arg_error(results, NULL, "too many keywords in keys pattern");
    if (keynumber >= rsize)
      return arg_error(results, NULL, "more keywords than rsize value");
    keyoffset[keynumber] = argmax;
    argcount = 1;
    }
  }

/* Check that no keyword has been specified with incompatible qualifiers */

for (i = 0; i <= argmax; i++)
  {
  int j;
  int argflags = results[i].presence;
  keynumber = 0;
  for (j = 0; j < 30; j++)
     if (keyoffset[j] == i) { keynumber = j; break; }
  if ((argflags & (rdargflag_s + rdargflag_n)) == rdargflag_s + rdargflag_n)
    return arg_error(results, findkey(keynumber, keystring, (char *)(results+2)),
      "is defined both as a switch and as a key for a numerical value"), -1;
  if ((argflags & rdargflag_s) && (argflags & 0x00FF0000) > 0x00010000)
    return arg_error(results, findkey(keynumber, keystring, (char *)(results+2)),
      "is defined as a switch with multiple arguments"), -1;
  }

/* Loop checking items from the command line and assigning them to the
appropriate arguments. */

while (argindex < argc)
  {
  char *arg = argv[argindex];

  /* Key: find which and get its argument, if any */

  if (arg[0] == '-')
    {
    int argnum = findarg(keystring, arg);
    int argflags;

    /* Check for unrecognized key */

    if (argnum < 0) return arg_error(results, arg, "unknown"), argindex;

    /* Adjust argnum for previous keys with multiple values */

    argnum = keyoffset[argnum];

    /* Extract key type flags and and advance to point to the argument
    value(s). */

    argflags = results[argnum].presence;
    if ((argflags & rdargflag_q) == 0) argindex++;

    /* Check for multiple occurrences */

    if ((argflags & argflag_presence_mask) != 0)
      return arg_error(results, arg, "keyword specified twice"), argindex;

    /* If a switch, set value TRUE, otherwise check that at least one argument
    value is present if required, and then call the routine which sets up
    values, as specified. */

    if ((argflags & rdargflag_s) != 0)
      {
      results[argnum].presence = arg_present_keyed;
      results[argnum].number = TRUE;
      }
    else
      {
      int rc;
      if (argindex >= argc && (argflags & rdargflag_d) == 0)
        return arg_error(results, arg, "requires an argument value"), argindex;
      rc = arg_setup_values(argc, argv, &argindex, &argnum, argflags,
        results, arg, arg_present_keyed);
      if (rc != 0) return rc;    /* Error occurred */
      }
    }

  /* Non-key: scan flags for the first unused normal or mandatory key which
  precedes any keyed items other than switches. This means that unkeyed items
  can only appear after any preceding keyed items have been explicitly
  specified. */

  else
    {
    int rc;
    int argflags = 0;
    int argnum = -1;

    /* Scan argument list */

    for (i = 0; i <= argmax; i++)
      {
      argflags = results[i].presence;

      /* If unused keyed item, break (error), else if not key, it's usable */

      if ((argflags & argflag_presence_mask) == arg_present_not)
        {
        if ((argflags & rdargflag_k) != 0) break;
        if ((argflags & rdargflag_s) == 0) { argnum = i; break; }
        }
      }

    /* Check for usable item found */

    if (argnum < 0)
      return arg_error(results, arg, "requires a keyword"), argindex;

    /* Set presence bit and call subroutine to set up argument value(s) as
    specified. */

    rc = arg_setup_values(argc, argv, &argindex, &argnum, argflags,
      results, arg, arg_present_unkeyed);
    if (rc != 0) return rc;    /* Error detected */
    }
  }

/* End of string reached: check for missing mandatory args. Other missing args
have their values and the presence word set zero. */

for (i = 0; i <= argmax; i++)
  {
  int argflags = results[i].presence;
  int p = argflags & argflag_presence_mask;
  if (p == arg_present_not || p == argflag_presence_mask)
    {
    if ((argflags & rdargflag_a) == 0)
      {
      results[i].number = 0;
      results[i].presence = 0;
      }
    else
      {
      int j;
      keynumber = 0;
      for (j = 0; j < 30; j++)
        if (keyoffset[j] == i) { keynumber = j; break; }
      return arg_error(results, findkey(keynumber, keystring,
                       (char *)(results+2)),
        "is a mandatory keyword which is always required");
      }
    }
  }

/* Indicate successful decoding */

return 0;
}

/* End of rdargs.c */
