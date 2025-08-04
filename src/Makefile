# Building Makefile for PMW on Unix-like systems

# The toplevel Makefile passes in configured settings

FONTMETRICS=$(DATADIR)/fontmetrics
PSHEADER=$(DATADIR)/PSheader
MIDIVOICES=$(DATADIR)/MIDIvoices
MIDIPERC=$(DATADIR)/MIDIperc
STDMACS=$(DATADIR)/macros
FONTDIR=$(DATADIR)/psfonts

COMP = $(CC) -DFONTDIR='"$(FONTDIR)"' -DFONTMETRICS='"$(FONTMETRICS)"' -DPSHEADER='"$(PSHEADER)"' -DMIDIVOICES='"$(MIDIVOICES)"' -DMIDIPERC='"$(MIDIPERC)"' -DSTDMACS='"$(STDMACS)"' $(CFLAGS) -I. -c
LINK = $(CC) $(CFLAGS)

HDRS = config.h functions.h globals.h pmw.h rdargs.h structs.h
DEPS = $(HDRS) Makefile ../Makefile
XMLDEPS = xml.h xml_functions.h xml_globals.h xml_structs.h

OBJ = debug.o draw.o error.o font.o globals.o main.o mem.o midi.o misc.o \
      out.o paginate.o pdf.o pmw_read.o pmw_read_header.o \
      pmw_read_note.o pmw_read_stave.o pmw_read_stavedirs.o pout.o \
      preprocess.o ps.o rdargs.o read.o setbar.o setbeam.o setnote.o \
      setother.o setslur.o settie.o string.o tables.o transpose.o tree.o \
      xmlout.o

ifeq ($(SUPPORT_XML),1)
  XMLOBJ = xml.o xml_analyze.o xml_debug.o xml_error.o xml_globals.o \
           xml_heading.o xml_read.o xml_staves.o xml_tables.o
else
  XMLOBJ=
endif

# The normal targets in this Makefile are not called directly, but should be
# called from the top-level Makefile. To make it easy when developing from
# within the source directory, a call with no target gets passed back up to
# the outer Makefile.

intcall:;      cd ..; $(MAKE);

# This is the main target

all:            pmw

pmw:            $(OBJ) $(XMLOBJ)
		@echo "$(CC) -o pmw"
		$(FE)$(LINK) $(OBJ) $(XMLOBJ) -o pmw -lm $(LFLAGS)
		@echo ">>> pmw binary built"

clean:;         rm -f *.o pmw

distclean:      clean
		rm -f config.h

.SUFFIXES: .o .c
.c.o:;  @echo  "$(CC) $*.c"
		$(FE)$(COMP) $*.c

debug.o:               $(DEPS) debug.c
draw.o:                $(DEPS) draw.c
error.o:               $(DEPS) error.c
font.o:                $(DEPS) font.c
globals.o:             $(DEPS) globals.c
main.o:                $(DEPS) main.c
mem.o:                 $(DEPS) mem.c
midi.o:                $(DEPS) midi.c
misc.o:                $(DEPS) misc.c
out.o:                 $(DEPS) out.c
paginate.o:            $(DEPS) paginate.c
pdf.o:                 $(DEPS) pdf.c
pmw_read.o:            $(DEPS) pmw_read.c
pmw_read_header.o:     $(DEPS) pmw_read_header.c
pmw_read_note.o:       $(DEPS) pmw_read_note.c
pmw_read_stave.o:      $(DEPS) pmw_read_stave.c
pmw_read_stavedirs.o:  $(DEPS) pmw_read_stavedirs.c
pout.o:                $(DEPS) pout.c
preprocess.o:          $(DEPS) preprocess.c
ps.o:                  $(DEPS) ps.c
rdargs.o:              $(DEPS) rdargs.c
read.o:                $(DEPS) read.c
setbar.o:              $(DEPS) setbar.c
setbeam.o:             $(DEPS) setbeam.c
setnote.o:             $(DEPS) setnote.c
setother.o:            $(DEPS) setother.c
setslur.o:             $(DEPS) setslur.c
settie.o:              $(DEPS) settie.c
string.o:              $(DEPS) string.c
tables.o:              $(DEPS) tables.c
transpose.o:           $(DEPS) transpose.c
tree.o:                $(DEPS) tree.c
xmlout.o:              $(DEPS) xmlout.c
xml.o:                 $(DEPS) $(XMLDEPS) xml.c
xml_analyze.o:         $(DEPS) $(XMLDEPS) xml_analyze.c
xml_debug.o:           $(DEPS) $(XMLDEPS) xml_debug.c
xml_error.o:           $(DEPS) $(XMLDEPS) xml_error.c
xml_globals.o:         $(DEPS) $(XMLDEPS) xml_globals.c
xml_heading.o:         $(DEPS) $(XMLDEPS) xml_heading.c
xml_read.o:            $(DEPS) $(XMLDEPS) xml_read.c
xml_staves.o:          $(DEPS) $(XMLDEPS) xml_staves.c
xml_tables.o:          $(DEPS) $(XMLDEPS) xml_tables.c

# End of building Makefile for PMW
