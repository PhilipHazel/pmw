/*************************************************
*          PMW preprocess one input line         *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: December 2020 */
/* This file last modified: January 2026 */

/* This file contains code for handling pre-processing directives. */

#include "pmw.h"



/*************************************************
*        Deal with pre-processing directive      *
*************************************************/

/* We enter with read_c containing the first letter of the directive's name, so
we are guaranteed to read a word. We can use normal item reading routines, but
must take care not to pass the end of the line, because a pre-processing
directive cannot straddle a line boundary. */

void
preprocess_line(void)
{
BOOL was_include = FALSE;

read_nextword();
read_sigcNL();

DEBUG(D_preprocess) (void)fprintf(stderr, "preprocess *%s\n", read_wordbuffer);

/* "if", "else", and "fi" must always be processed, whether or not we are
currently skipping. */

if (Ustrcmp(read_wordbuffer, "if") == 0)
  {
  BOOL OK;
  if (read_skipdepth > 0)
    {
    read_skipdepth++;
    read_i = main_readlength;
    read_c = '\n';
    DEBUG(D_preprocess) (void)fprintf(stderr, "already skipping\n");
    return;
    }

  /* Loop for multiple "or" conditions */

  for (;;)
    {
    OK = TRUE;
    read_wordbuffer[0] = 0;
    if (read_c != '\n') read_nextword();

    if (Ustrcmp(read_wordbuffer, "not") == 0)
      {
      OK = !OK;
      read_sigcNL();
      read_wordbuffer[0] = 0;
      if (read_c != '\n') read_nextword();
      }

    if (read_wordbuffer[0] == 0)
      {
      error_skip(ERR8, '\n', "word");
      break;
      }

    if (Ustrcmp(read_wordbuffer, "score") == 0)
      {
      if (curmovt->select_staves != ~0uL) OK = !OK;
      }
    else if (Ustrcmp(read_wordbuffer, "part") == 0)
      {
      if (curmovt->select_staves == ~0uL) OK = !OK;
      }

    else if (Ustrcmp(read_wordbuffer, "staff") == 0 ||
             Ustrcmp(read_wordbuffer, "stave") == 0 ||
             Ustrcmp(read_wordbuffer, "staves") == 0)
      {
      int rc;
      uint64_t list;
      uschar *endptr;

      read_sigcNL();
      if (!isdigit(read_c))
        {
        error_skip(ERR8, '\n', "number");
        break;
        }
      rc = read_stavelist(main_readbuffer + read_i - 1, &endptr, &list, NULL);

      if (rc != 0)
        {
        error_skip(rc, '\n');
        }
      else
        {
        read_i = endptr - main_readbuffer;
        read_nextc();
        }

      list |= 1uL;        /* Stave zero is always selected */
      if (curmovt->select_staves != list) OK = !OK;
      }

    /* Deal with definition test */

    else if (Ustrcmp(read_wordbuffer, "undef") == 0)
      {
      int i = 0;
      read_sigcNL();

      /* We cannot use read_nextword() because macro names may start with a
      digit and are case-sensitive. */

      if (isalnum(read_c))
        {
        do
          {
          if (i >= WORDBUFFER_SIZE - 1)
            error(ERR7, "macro name", WORDBUFFER_SIZE - 1);  /* Hard */
          read_wordbuffer[i++] = read_c;
          read_nextc();
          }
        while (isalnum(read_c));
        }
      read_wordbuffer[i] = 0;

      if (read_wordbuffer[0] == 0) error_skip(ERR8, '\n', "macro name"); else
        {
        if (tree_search(macro_tree, read_wordbuffer) != NULL) OK = !OK;
        }
      }

    /* Test output format */

    else if (Ustrcmp(read_wordbuffer, "pdf") == 0)
      {
      if (!PDF) OK = !OK;
      }

    /* Test if any format is set */

    else if (Ustrcmp(read_wordbuffer, "format") == 0)
      {
      if (main_format == NULL) OK = !OK;
      }

    /* Test transposition */

    else if (Ustrcmp(read_wordbuffer, "transpose") == 0)
      {
      read_sigcNL();
      if (isdigit(read_c) || read_c == '-' || read_c == '+')
        {
        int x;
        (void)read_expect_integer(&x, FALSE, TRUE);
        if (active_transpose != 2 * x) OK = !OK;
        }
      else if (active_transpose == NO_TRANSPOSE) OK = !OK;
      }

    /* Not recognized; take as format word */

    else
      {
      if (main_format == NULL || Ustrcmp(read_wordbuffer, main_format) != 0)
        OK = !OK;
      else main_format_tested = TRUE;
      }

    /* See if the next thing is "or"; if not and if not newline, error.
    Otherwise, if it's "or" and OK == FALSE, let the loop continue. */

    read_sigcNL();
    if (read_c == '\n') break;
    read_nextword();
    if (Ustrcmp(read_wordbuffer, "or") != 0)
      {
      error_skip(ERR8, '\n', "\"or\"");
      break;
      }

    if (OK) break;
    read_sigcNL();
    }

  /* Decision taken; act appropriately */

  if (OK) read_okdepth++; else read_skipdepth++;
  DEBUG(D_preprocess) (void)fprintf(stderr, "skip=%d ok=%d\n", read_skipdepth,
    read_okdepth);
  return;
  }


/* Deal with "else" */

if (Ustrcmp(read_wordbuffer, "else") == 0)
  {
  if (read_skipdepth <= 1)
    {
    if (read_skipdepth == 1)
      {
      read_skipdepth--;
      read_okdepth++;
      }
    else if (read_okdepth > 0)
      {
      read_skipdepth++;
      read_okdepth--;
      }
    else error_skip(ERR11, '\n', "\"*else\"");
    }
  DEBUG(D_preprocess) (void)fprintf(stderr, "skip=%d ok=%d\n", read_skipdepth,
    read_okdepth);
  return;
  }


/* Deal with "fi" */

if (Ustrcmp(read_wordbuffer, "fi") == 0)
  {
  if (read_skipdepth > 0) read_skipdepth--; else
    if (read_okdepth > 0) read_okdepth--;
      else error_skip(ERR11, '\n', "\"*fi\"");
  DEBUG(D_preprocess) (void)fprintf(stderr, "skip=%d ok=%d\n", read_skipdepth,
    read_okdepth);
  return;
  }


/* The other preprocessing directives must be ignored when skipping. */

if (read_skipdepth >  0)
  {
  DEBUG(D_preprocess) (void)fprintf(stderr, "skipping\n");
  return;
  }


/* Deal with defining macros. We can't use read_nextword() because it converts
to lower case and also insists on an alphabetic first character. */

if (Ustrcmp(read_wordbuffer, "define") == 0)
  {
  usint i = 0;
  usint argcount = 0;
  uschar *rep;
  uschar *args[MAX_MACROARGS];
  uschar argbuffer[MAX_MACRODEFAULT + 1];
  tree_node *p;
  macrostr *mm;
  size_t replen;

  read_sigcNL();
  if (isalnum(read_c))
    {
    do
      {
      if (i >= WORDBUFFER_SIZE - 1)
        error(ERR7, "macro name", WORDBUFFER_SIZE - 1);  /* Hard */
      read_wordbuffer[i++] = read_c;
      read_nextc();
      }
    while (isalnum(read_c));
    }
  read_wordbuffer[i] = 0;

  if (read_wordbuffer[0] == 0)
    {
    error_skip(ERR8, '\n', "macro name");
    return;
    }

  p = mem_get(sizeof(tree_node));
  p->name = mem_get(Ustrlen(read_wordbuffer) + 1);
  Ustrcpy(p->name, read_wordbuffer);
  p->data = NULL;  /* Default no arguments */

  /* Handle macro optional default arguments. */

  if (read_c == '(')
    {
    do
      {
      int bracount = 0;
      usint s = 0;
      BOOL inquotes = FALSE;

      if (argcount >= MAX_MACROARGS) error(ERR14, MAX_MACROARGS);  /* Hard */

      /* Read one argument; there is a length limit. */

      read_nextc();
      while (read_c != 'n' && ((read_c != ',' && read_c != ')') ||
              bracount > 0 || inquotes))
        {
        if (s >= MAX_MACRODEFAULT)
          error(ERR7, "macro default argument", MAX_MACRODEFAULT);  /* Hard */

        if (read_c == '&')  /* & is a literal escaper */
          {
          read_nextc();
          }
        else  /* Keep track of nested parens and quotes */
          {
          if (read_c == '\"') inquotes = !inquotes;
          if (!inquotes)
            {
            if (read_c == '(') bracount++;
              else if (read_c == ')') bracount--;
            }
          }

        argbuffer[s++] = read_c;
        read_nextc();
        }

      /* If the argument is not empty, remember it, else record it as NULL. */

      if (s > 0)
        {
        uschar *ss = mem_get(s + 1);
        memcpy(ss, argbuffer, s);
        ss[s] = 0;
        args[argcount++] = ss;
        }
      else args[argcount++] = NULL;
      }
    while (read_c == ',');  /* End of "do" loop */

    if (read_c == ')') read_nextc();
      else if (read_c == '\n') error(ERR15);   /* Missing ')' */
    }

  /* Now set up the replacement text and the arguments. The replacement is the
  rest of the line, excluding the final newline. */

  read_sigcNL();
  replen = main_readlength - read_i;  /* Don't include the newline */
  rep = mem_get(replen + 1);
  memcpy(rep, main_readbuffer + read_i - 1, replen); /* Include current char */
  rep[replen] = 0;

  /* The macrostr structure ends with a vector of size 1, but we need to get
  memory for however many arguments there actually are. Note: we must not use
  (argcount-1) because argcount is unsigned and may be zero. */

  p->data = mm =
    mem_get(sizeof(macrostr) - sizeof(uschar *) + argcount*sizeof(uschar *));
  mm->argcount = argcount;
  mm->text = rep;
  for (i = 0; i < argcount; i++) mm->args[i] = args[i];

  DEBUG(D_macro)
    {
    (void)fprintf(stderr, "defined macro \"%s\" argcount=%d\n", p->name, argcount);
    (void)fprintf(stderr, "  replacement: >%s<\n", rep);
    for (i = 0; i < argcount; i++)
      (void)fprintf(stderr, "  %d %s\n", i+1, mm->args[i]);
    }

  if (!tree_insert(&macro_tree, p)) error(ERR16, read_wordbuffer);
  read_i = main_readlength;
  read_c = '\n';
  }


/* Deal with included files */

else if (Ustrcmp(read_wordbuffer, "include") == 0)
  {
  FILE *f;
  uschar buffer[256];

  if (read_filestackptr >= MAX_INCLUDE) error(ERR33, MAX_INCLUDE);  /* Hard */
  if (read_c == '\n' || !string_read_plain())
    {
    error(ERR8, "File name in quotes");
    return;
    }

  /* Remember a short-enough unqualified name for standard macros. */

  if (Ustrchr(read_stringbuffer, '/') == NULL &&
      Ustrlen(read_stringbuffer) < 32)
    {
    read_wordbuffer[0] = '/';
    Ustrcpy(read_wordbuffer + 1, read_stringbuffer);
    }
  else read_wordbuffer[0] = 0;   /* Not potential standard macro file */

  /* First, relativize the name and look for an existing file; if not found and
  the original string was unqualified, try for a standard macros file. */

  string_relativize();
  f = Ufopen(read_stringbuffer, "r");
  if (f == NULL)
    {
    if (read_wordbuffer[0] != 0)
      {
      Ustrcpy(read_stringbuffer, stdmacs_dir);
      Ustrcat(read_stringbuffer, read_wordbuffer);
      f = Ufopen(read_stringbuffer, "r");
      }
    if (f == NULL) error(ERR23, read_stringbuffer, strerror(errno));   /* Hard */
    }

  /* Stack the current variables. */

  read_filestack[read_filestackptr].file = read_filehandle;
  read_filestack[read_filestackptr].filename = read_filename;
  read_filestack[read_filestackptr].linenumber = read_linenumber;
  read_filestack[read_filestackptr++].okdepth = read_okdepth;

  /* Replace current variables for reading a PMW file. */

  read_filehandle = f;
  read_filename = mem_copystring(read_stringbuffer);
  read_linenumber = 0;
  read_okdepth = 0;

  /* Check the first line of the file to test for MusicXML. */

  if (fgets(CS buffer, sizeof(buffer), f) != NULL)
    {
    uschar *p = buffer;
    if (Ustrncmp(main_readbuffer, "\xef\xbb\xbf", 3) == 0) p += 3;
    if (Ustrncmp(p, "<?xml version=", 14) == 0)
      {
      TRACE("*Include MusicXML file detected\n");
#if !SUPPORT_XML
      error(ERR3, "MusicXML");    /* Hard */
#else
      /* Process a MusicXML file as long as we are not in the middle of a PMW
      stave, then restore the variables. Give an error if we are in the middle
      of a PMW stave. Otherwise, we are done. */

      if (!pmw_reading_stave) xml_read();
      read_filename = read_filestack[--read_filestackptr].filename;
      read_filehandle = read_filestack[read_filestackptr].file;
      read_linenumber = read_filestack[read_filestackptr].linenumber;
      read_okdepth = read_filestack[read_filestackptr].okdepth;
      if (pmw_reading_stave) error(ERR4, "MusicXML");  /* Hard */
      return;
#endif
      }

    /* Not a MusicXML file; rewind for PMW processing. */

    else rewind(f);
    }

  was_include = TRUE;
  }


/* Deal with comment */

else if (Ustrcmp(read_wordbuffer, "comment") == 0)
  {
  (void)fprintf(stderr, "%s", main_readbuffer + read_i - 1);
  read_i = main_readlength;
  read_c = '\n';
  }


/* Else unknown preprocessing directive */

else error_skip(ERR12, '\n', read_wordbuffer);


/* Test for extraneous characters, then, if it was *include, adjust the current
file that is being read. We leave this till now so that ERR13 below shows the
old file's status. */

read_sigcNL();
if (read_c != '\n') error_skip(ERR13, '\n', "extraneous characters ignored");

if (was_include)
  {
  read_filename = mem_copystring(read_stringbuffer);
  read_linenumber = 0;
  TRACE("including file %s\n", read_filename);
  }
}

/* End of preprocess.c */
