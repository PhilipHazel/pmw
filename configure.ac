dnl Process this file with autoconf to produce a configure script.

dnl This is required at the start; the name is the name of a file
dnl it should be seeing, to verify it is in the same directory.

AC_INIT
AC_CONFIG_SRCDIR([src/pmw.h])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(memmove strerror)

dnl Handle --disable-pmwrc
NO_PMWRC=0
AC_ARG_ENABLE(pmwrc,
  AS_HELP_STRING([--disable-pmwrc], [disable support for .pmwrc file]),
  , enable_pmwrc=unset)

if test "x$enable_pmwrc" = "xunset"; then enable_pmwrc=yes; fi
if test "x$enable_pmwrc" != "xyes"; then NO_PMWRC=1; fi

dnl Handle --enable-b2pf
SUPPORT_B2PF=0
AC_ARG_ENABLE(b2pf,
  AS_HELP_STRING([--enable-b2pf], [enable support for B2PF processing]),
  , enable_b2pf=unset)

if test "x$enable_b2pf" = "xunset"; then enable_b2pf=no; fi
if test "x$enable_b2pf" = "xyes"; then
  AC_CHECK_LIB([b2pf], [b2pf_context_create],
    , AC_MSG_ERROR(B2PF library not found))
  SUPPORT_B2PF=1
  LFLAGS=-lb2pf
fi

dnl Handle --enable-musicxml
SUPPORT_XML=0
AC_ARG_ENABLE(musicxml,
  AS_HELP_STRING([--enable-musicxml], [enable support for MusicXML input]),
  , enable_musicxml=unset)

if test "x$enable_musicxml" = "xunset"; then enable_musicxml=no; fi
if test "x$enable_musicxml" = "xyes"; then SUPPORT_XML=1; fi

dnl Handle --enable-pdf-default
PDF_DEFAULT=0
AC_ARG_ENABLE(pdf-default,
  AS_HELP_STRING([--enable-pdf-default], [make default output PDF]),
  , enable_pdf_default=unset) 
  
if test "x$enable_pdf_default" = "xunset"; then pdf_default=PostScript; fi
if test "x$enable_pdf_default" = "xyes"; then PDF_DEFAULT=1; pdf_default=PDF; fi
 

dnl Export these values in addition to the defaults
AC_SUBST(LFLAGS)
AC_SUBST(NO_PMWRC)
AC_SUBST(PDF_DEFAULT)
AC_SUBST(SUPPORT_B2PF)
AC_SUBST(SUPPORT_XML)

dnl Write these files
AC_CONFIG_FILES(
  Makefile
  src/config.h
)

dnl This must be last
AC_OUTPUT

dnl Output configuration settings
cat <<EOF

PMW configuration summary:

    C compiler .......................: ${CC}
    C compiler flags .................: ${CFLAGS}
    Linker flags .....................: ${LFLAGS}
    Install pmw command in ...........: ${prefix}/bin
    Install shared data in ...........: ${prefix}/share/pmw
    Install man page in ..............: ${prefix}/man
    Default output format ............: ${pdf_default} 
    Support .pmwrc file ..............: ${enable_pmwrc}
    Support B2PF text processing .....: ${enable_b2pf}
    Support for MusicXML input .......: ${enable_musicxml}

EOF

dnl End of configure.ac
