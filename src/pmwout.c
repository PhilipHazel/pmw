/*************************************************
*            PMW source output generation        *
*************************************************/

/* Copyright Philip Hazel 2026 */
/* This file created: April 2026 */
/* This file last modified: April 2026 */

#include "pmw.h"

static FILE      *pmw_file;


/*************************************************
*                Write MusicXML file             *
*************************************************/

/* This is the main external entry to this set of functions. The data is all in
memory and global variables. Writing a PMW source file is triggered by the use of
the -pmw command line option, which sets outpmw_filename non-NULL.

Arguments:  none
Returns:    nothing
*/

void
outpmw_write(void)
{

TRACE("outpmw_write()\n");

pmw_file = Ufopen(outpmw_filename, "w");
if (pmw_file == NULL) error(ERR23, outpmw_filename, strerror(errno));  /* Hard */


if (fclose(pmw_file) != 0) error(ERR200, "PMW file", strerror(errno));
}

/* End of pmwout.c */
