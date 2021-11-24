/*************************************************
*     Command Line Decoding Function (rdargs)    *
*************************************************/

/* Header for rdargs() function. */

/* This file last edited: January 2021 */

typedef struct {
  int presence;
  int number;
  const char *text;
} arg_result;

enum { arg_present_not, arg_present_unkeyed, arg_present_keyed };

extern int rdargs(int, char **, const char *, arg_result *, int size);

/* End of rdargs.h */
