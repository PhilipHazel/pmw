# Makefile for PMW specification. It uses:
#
# xfpt   to convert the base document to XML
# aspic  to generate EPS files for the figures
# pmw    to generate EPS files for the music examples
# sdop   to generate PostScript from the base document + figures + examples
# gs     to generate a PDF from the PostScript output
#
# The fancy gs command ensures that all the fonts are embedded in the PDF.
# Otherwise some of the rarer characters may not be present in fonts used by
# acroread or other PDF displayers.
#
# Use the version of PMW compiled in this tree.
#
# The other commands must be installed in the PATH. However, we have to
# ensure that SDoP sees the metrics for the PMW fonts. Just pointing it
# at the PMW directory is dangerous because it also uses a file called
# PSheader. Instead, we make temporary copies for it to use.
#
# It seems that gs doesn't pick up the PMW-Alpha font from GS_FONTPATH as
# it does for the PMW-Music font (perhaps because it's a Type 3?) so we
# include it explicitly when making the PDF.

PMW=../src/pmw -F ../fontmetrics -H ../PSheader

all:                spec.pdf

spec.pdf:           spec.ps
		    gs -q -dNOPAUSE -dBATCH -dPDFSETTINGS=/prepress -sDEVICE=pdfwrite -sOutputFile=spec.pdf ../psfonts/PMW-Alpha spec.ps

postscript:;        mkdir postscript

spec.ps:            Makefile \
                    postscript \
                    spec.xml \
                    fontmetrics/PMW-Alpha.afm \
                    fontmetrics/PMW-Music.afm \
                    postscript/fig01 \
                    postscript/fig02 \
                    postscript/fig03 \
                    postscript/fig04 \
                    postscript/music01 \
                    postscript/music02 \
                    postscript/music03 \
                    postscript/music04 \
                    postscript/music05 \
                    postscript/music06 \
                    postscript/music07 \
                    postscript/music08 \
                    postscript/music09 \
                    postscript/music10 \
                    postscript/music11 \
                    postscript/music12 \
                    postscript/music13 \
                    postscript/music14 \
                    postscript/music15 \
                    postscript/music16 \
                    postscript/music17 \
                    postscript/music18 \
                    postscript/music19 \
                    postscript/music20 \
                    postscript/music21 \
                    postscript/music22 \
                    postscript/music23 \
                    postscript/music24 \
                    postscript/music25 \
                    postscript/music26 \
                    postscript/music27 \
                    postscript/music28 \
                    postscript/music29 \
                    postscript/music30 \
                    postscript/music31 \
                    postscript/music32 \
                    postscript/music33 \
                    postscript/music34 \
                    postscript/music35 \
                    postscript/music36 \
                    postscript/music37 \
                    postscript/music38 \
                    postscript/music39 \
                    postscript/music40 \
                    postscript/music41 \
                    postscript/music42 \
                    postscript/music43 \
                    postscript/music44 \
                    postscript/music45 \
                    postscript/music46 \
                    postscript/music47 \
                    postscript/music48 \
                    postscript/music49 \
                    postscript/music50 \
                    postscript/music51 \
                    postscript/music52 \
                    postscript/music53 \
                    postscript/music54 \
                    postscript/music55 \
                    postscript/music56 \
                    postscript/music57 \
                    postscript/music58 \
                    postscript/music59 \
                    postscript/music60 \
                    postscript/music61 \
                    postscript/music62 \
                    postscript/music63 \
                    postscript/music64 \
                    postscript/music65
		    sdop -S . spec.xml

spec.xml:           spec.xfpt
		    xfpt spec.xfpt

fontmetrics/PMW-Alpha.afm: ../fontmetrics/PMW-Alpha.afm
		    mkdir -p fontmetrics
		    cp ../fontmetrics/PMW-Alpha.afm fontmetrics

fontmetrics/PMW-Music.afm: ../fontmetrics/PMW-Music.afm
		    mkdir -p fontmetrics
		    cp ../fontmetrics/PMW-Music.afm fontmetrics

postscript/fig01:   figures/fig01.aspic
		    aspic figures/fig01.aspic postscript/fig01

postscript/fig02:   figures/fig02.aspic
		    aspic figures/fig02.aspic postscript/fig02

postscript/fig03:   figures/fig03.aspic
		    aspic figures/fig03.aspic postscript/fig03

postscript/fig04:   figures/fig04.aspic
		    aspic figures/fig04.aspic postscript/fig04


postscript/music01: examples/ex01
		    ${PMW} -eps -nowidechars -o postscript/music01 examples/ex01

postscript/music02: examples/ex02
		    ${PMW} -eps -nowidechars -o postscript/music02 examples/ex02

postscript/music03: examples/ex03
		    ${PMW} -eps -nowidechars -o postscript/music03 examples/ex03

postscript/music04: examples/ex04
		    ${PMW} -eps -nowidechars -o postscript/music04 examples/ex04

postscript/music05: examples/ex05
		    ${PMW} -eps -nowidechars -o postscript/music05 examples/ex05

postscript/music06: examples/ex06
		    ${PMW} -eps -nowidechars -o postscript/music06 examples/ex06

postscript/music07: examples/ex07
		    ${PMW} -eps -nowidechars -o postscript/music07 examples/ex07

postscript/music08: examples/ex08
		    ${PMW} -eps -nowidechars -o postscript/music08 examples/ex08

postscript/music09: examples/ex09
		    ${PMW} -eps -nowidechars -o postscript/music09 examples/ex09

postscript/music10: examples/ex10
		    ${PMW} -eps -nowidechars -o postscript/music10 examples/ex10

postscript/music11: examples/ex11
		    ${PMW} -eps -nowidechars -o postscript/music11 examples/ex11

postscript/music12: examples/ex12
		    ${PMW} -eps -nowidechars -o postscript/music12 examples/ex12

postscript/music13: examples/ex13
		    ${PMW} -eps -nowidechars -o postscript/music13 examples/ex13

postscript/music14: examples/ex14
		    ${PMW} -eps -nowidechars -o postscript/music14 examples/ex14

postscript/music15: examples/ex15
		    ${PMW} -eps -nowidechars -o postscript/music15 examples/ex15

postscript/music16: examples/ex16
		    ${PMW} -eps -nowidechars -o postscript/music16 examples/ex16

postscript/music17: examples/ex17
		    ${PMW} -eps -nowidechars -o postscript/music17 examples/ex17

postscript/music18: examples/ex18
		    ${PMW} -eps -nowidechars -o postscript/music18 examples/ex18

postscript/music19: examples/ex19
		    ${PMW} -eps -nowidechars -o postscript/music19 examples/ex19

postscript/music20: examples/ex20
		    ${PMW} -eps -nowidechars -o postscript/music20 examples/ex20

postscript/music21: examples/ex21
		    ${PMW} -eps -nowidechars -o postscript/music21 examples/ex21

postscript/music22: examples/ex22
		    ${PMW} -eps -nowidechars -o postscript/music22 examples/ex22

postscript/music23: examples/ex23
		    ${PMW} -eps -nowidechars -o postscript/music23 examples/ex23

postscript/music24: examples/ex24
		    ${PMW} -eps -nowidechars -o postscript/music24 examples/ex24

postscript/music25: examples/ex25
		    ${PMW} -eps -nowidechars -o postscript/music25 examples/ex25

postscript/music26: examples/ex26
		    ${PMW} -eps -nowidechars -o postscript/music26 examples/ex26

postscript/music27: examples/ex27
		    ${PMW} -eps -nowidechars -o postscript/music27 examples/ex27

postscript/music28: examples/ex28
		    ${PMW} -eps -nowidechars -o postscript/music28 examples/ex28

postscript/music29: examples/ex29
		    ${PMW} -eps -nowidechars -o postscript/music29 examples/ex29

postscript/music30: examples/ex30
		    ${PMW} -eps -nowidechars -o postscript/music30 examples/ex30

postscript/music31: examples/ex31
		    ${PMW} -eps -nowidechars -o postscript/music31 examples/ex31

postscript/music32: examples/ex32
		    ${PMW} -eps -nowidechars -o postscript/music32 examples/ex32

postscript/music33: examples/ex33
		    ${PMW} -eps -nowidechars -o postscript/music33 examples/ex33

postscript/music34: examples/ex34
		    ${PMW} -eps -nowidechars -o postscript/music34 examples/ex34

postscript/music35: examples/ex35
		    ${PMW} -eps -nowidechars -o postscript/music35 examples/ex35

postscript/music36: examples/ex36
		    ${PMW} -eps -nowidechars -o postscript/music36 examples/ex36

postscript/music37: examples/ex37
		    ${PMW} -eps -nowidechars -o postscript/music37 examples/ex37

postscript/music38: examples/ex38
		    ${PMW} -eps -nowidechars -o postscript/music38 examples/ex38

postscript/music39: examples/ex39
		    ${PMW} -eps -nowidechars -o postscript/music39 examples/ex39

postscript/music40: examples/ex40
		    ${PMW} -eps -nowidechars -o postscript/music40 examples/ex40

postscript/music41: examples/ex41
		    ${PMW} -eps -nowidechars -o postscript/music41 examples/ex41

postscript/music42: examples/ex42
		    ${PMW} -eps -nowidechars -o postscript/music42 examples/ex42

postscript/music43: examples/ex43
		    ${PMW} -eps -nowidechars -o postscript/music43 examples/ex43

postscript/music44: examples/ex44
		    ${PMW} -eps -nowidechars -o postscript/music44 examples/ex44

postscript/music45: examples/ex45
		    ${PMW} -eps -nowidechars -o postscript/music45 examples/ex45

postscript/music46: examples/ex46
		    ${PMW} -eps -nowidechars -o postscript/music46 examples/ex46

postscript/music47: examples/ex47
		    ${PMW} -eps -nowidechars -o postscript/music47 examples/ex47

postscript/music48: examples/ex48
		    ${PMW} -eps -nowidechars -o postscript/music48 examples/ex48

postscript/music49: examples/ex49
		    ${PMW} -eps -nowidechars -o postscript/music49 examples/ex49

postscript/music50: examples/ex50
		    ${PMW} -eps -nowidechars -o postscript/music50 examples/ex50

postscript/music51: examples/ex51
		    ${PMW} -eps -nowidechars -o postscript/music51 examples/ex51

postscript/music52: examples/ex52
		    ${PMW} -eps -nowidechars -o postscript/music52 examples/ex52

postscript/music53: examples/ex53
		    ${PMW} -eps -nowidechars -o postscript/music53 examples/ex53

postscript/music54: examples/ex54
		    ${PMW} -eps -nowidechars -o postscript/music54 examples/ex54

postscript/music55: examples/ex55
		    ${PMW} -eps -nowidechars -o postscript/music55 examples/ex55

postscript/music56: examples/ex56
		    ${PMW} -eps -nowidechars -o postscript/music56 examples/ex56

postscript/music57: examples/ex57
		    ${PMW} -eps -nowidechars -o postscript/music57 examples/ex57

postscript/music58: examples/ex58
		    ${PMW} -eps -nowidechars -o postscript/music58 examples/ex58

postscript/music59: examples/ex59
		    ${PMW} -eps -nowidechars -o postscript/music59 examples/ex59

postscript/music60: examples/ex60
		    ${PMW} -eps -nowidechars -o postscript/music60 examples/ex60

postscript/music61: examples/ex61
		    ${PMW} -eps -nowidechars -o postscript/music61 examples/ex61

postscript/music62: examples/ex62
		    ${PMW} -eps -nowidechars -o postscript/music62 examples/ex62

postscript/music63: examples/ex63
		    ${PMW} -eps -nowidechars -o postscript/music63 examples/ex63

postscript/music64: examples/ex64
		    ${PMW} -eps -nowidechars -o postscript/music64 examples/ex64

postscript/music65: examples/ex65
		    ${PMW} -eps -nowidechars -o postscript/music65 examples/ex65

clean:;             /bin/rm -rf *.ps *.pdf *.xml postscript fontmetrics

# End
