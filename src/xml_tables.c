/*************************************************
*             MusicXML input for PMW             *
*************************************************/

/* Copyright (c) Philip Hazel, 2022 */

/* This module contains fixed XML data in the form of various tables */


#include "pmw.h"



/*************************************************
*  Tables of recognized elements and attributes  *
*************************************************/

/* The nocheck_attrs list is used for elements that are recognized, but
completely ignored. */

uschar *nocheck_attrs[]         = { US"*",          NULL };

uschar *accidental_attrs[]      = { US"parentheses", NULL };
uschar *accidental_mark_attrs[] = { US"placement",  US"**",         US"default-x", US"default-y", NULL };
uschar *arpeggiate_attrs[]      = { US"**",         US"default-x", US"number", NULL };
uschar *articulation_attrs[]    = { US"placement",  US"type", US"**", US"default-x", US"default-y", NULL };
uschar *barline_attrs[]         = { US"location",   NULL };
uschar *beam_attrs[]            = { US"number",     NULL };
uschar *bracket_attrs[]         = { US"number",     US"type", US"line-end", US"line-type", US"default-y", US"relative-x", US"**", US"default-x", NULL };
uschar *clef_attrs[]            = { US"number",     NULL };
uschar *creator_attrs[]         = { US"type",       NULL };
uschar *credit_attrs[]          = { US"page",       NULL };
uschar *credit_words_attrs[]    = { US"font-size",  US"font-weight", US"justify", US"halign", US"valign", US"default-y", US"**", US"xml:space", US"xml:lang", US"default-x", US"font-family", US"letter-spacing", NULL };
uschar *direction_attrs[]       = { US"directive",  US"placement",  NULL };
uschar *display_text_attrs[]    = { US"**",         US"xml:space", NULL };
uschar *dot_attrs[]             = { US"**",         US"relative-x", NULL };
uschar *down_bow_attrs[]        = { US"**",         US"default-x", US"default-y", US"placement", NULL };
uschar *dynamics_attrs[]        = { US"default-y",  US"halign", US"relative-x", US"**", US"default-x", NULL };
uschar *ending_attrs[]          = { US"number",     US"type", US"**", US"default-y", US"end-length", NULL };
uschar *extend_attrs[]          = { US"**",         US"type",  NULL };
uschar *fermata_attrs[]         = { US"type",       US"**", US"default-x", US"default-y", NULL };
uschar *grace_attrs[]           = { US"slash",      US"**", US"steal-time-following", NULL };
uschar *group_symbol_attrs[]    = { US"**",         US"default-x", NULL };
uschar *key_attrs[]             = { US"print-object", NULL };
uschar *lyric_attrs[]           = { US"justify",    US"relative-x", US"**",  US"default-y", US"number", NULL };
uschar *lyric_font_attrs[]      = { US"font-size",  US"**", US"font-family", NULL };
uschar *measure_attrs[]         = { US"implicit",   US"number",     US"width", NULL };
uschar *metronome_attrs[]       = { US"font-size",  US"parentheses", US"**", US"font-family", US"default-x", US"relative-y", NULL };
uschar *notehead_attrs[]        = { US"**",         US"filled", NULL };
uschar *note_attrs[]            = { US"default-x",  US"print-object", US"print-spacing", US"**", US"default-y", NULL };
uschar *offset_attrs[]          = { US"**",         US"sound", NULL };
uschar *page_margins_attrs[]    = { US"type",       NULL };
uschar *part_attrs[]            = { US"id",         NULL };
uschar *part_group_attrs[]      = { US"number",     US"type", NULL };
uschar *part_name_attrs[]       = { US"print-object", NULL };
uschar *pedal_attrs[]           = { US"line",       US"type", US"halign", US"default-y", US"relative-x", NULL };
uschar *print_attrs[]           = { US"new-system", US"**", US"new-page", US"page-number", NULL };
uschar *rehearsal_attrs[]       = { US"enclosure",  US"font-size", US"font-weight", US"default-x", US"**", US"default-y", NULL };
uschar *repeat_attrs[]          = { US"direction",  US"**", US"winged" };
uschar *rest_attrs[]            = { US"measure",    NULL };
uschar *score_part_attrs[]      = { US"id",         NULL };
uschar *score_partwise_attrs[]  = { US"version",    NULL };
uschar *slur_attrs[]            = { US"number",     US"placement", US"type", US"line-type", US"**", US"default-x", US"default-y", US"bezier-x", US"bezier-y", US"bezier-x2", US"bezier-y2", NULL };
uschar *staff_details_attrs[]   = { US"print-object", US"print-spacing", US"number", NULL };
uschar *staff_layout_attrs[]    = { US"number",     NULL };
uschar *stem_attrs[]            = { US"**",         US"default-y",  NULL };
uschar *tied_attrs[]            = { US"type",       US"orientation", NULL };
uschar *time_attrs[]            = { US"symbol",     NULL };
uschar *tremolo_attrs[]         = { US"type",       US"**", US"default-x", US"default-y", NULL };
uschar *trill_mark_attrs[]      = { US"**",         US"default-x", US"default-y", US"placement", NULL };
uschar *tuplet_attrs[]          = { US"bracket",    US"placement", US"type", US"show-number", US"**", US"number", NULL };
uschar *turn_attrs[]            = { US"placement",  US"slash", US"**", US"default-x", US"default-y", NULL };
uschar *type_attrs[]            = { US"size",       NULL };
uschar *up_bow_attrs[]          = { US"**",         US"default-x", US"default-y", US"placement", NULL };
uschar *wedge_attrs[]           = { US"type",       US"spread", US"default-y", US"relative-x", NULL };
uschar *words_attrs[]           = { US"font-size",  US"font-style", US"font-weight", US"enclosure", US"default-y", US"relative-x", US"relative-y", US"halign", US"**", US"default-x", US"xml:lang", NULL };
uschar *word_font_attrs[]       = { US"font-size",  US"**", US"font-family", NULL };


xml_elliststr xml_supported_elements[] = {
  { US"accent",                 articulation_attrs },
  { US"accidental",             accidental_attrs },
  { US"accidental-mark",        accidental_mark_attrs },
  { US"accidental-text",        NULL },
  { US"actual-notes",           NULL },
  { US"alter",                  nocheck_attrs },
  { US"appearance",             nocheck_attrs },
  { US"arpeggiate",             arpeggiate_attrs },
  { US"articulations",          NULL },
  { US"attributes",             NULL },
  { US"backup",                 NULL },
  { US"bar-style",              nocheck_attrs },
  { US"barline",                barline_attrs },
  { US"beam",                   beam_attrs },
  { US"beat-type",              NULL },
  { US"beat-unit",              NULL },
  { US"beats",                  NULL },
  { US"bottom-margin",          NULL },
  { US"bracket",                bracket_attrs },
  { US"breath-mark",            NULL },
  { US"caesura",                NULL },
  { US"chord",                  NULL },
  { US"chromatic",              nocheck_attrs },
  { US"clef",                   clef_attrs },
  { US"clef-octave-change",     NULL },
  { US"creator",                creator_attrs },
  { US"credit",                 credit_attrs },
  { US"credit-type",            NULL },
  { US"credit-words",           credit_words_attrs },
  { US"cue",                    NULL },
  { US"defaults",               NULL },
  { US"delayed-turn",           turn_attrs },
  { US"detached-legato",        articulation_attrs },
  { US"diatonic",               nocheck_attrs },
  { US"direction",              direction_attrs },
  { US"direction-type",         NULL },
  { US"display-octave",         NULL },
  { US"display-step",           NULL },
  { US"display-text",           display_text_attrs },
  { US"distance",               nocheck_attrs },
  { US"divisions",              NULL },
  { US"doit",                   nocheck_attrs },
  { US"dot",                    dot_attrs },
  { US"down-bow",               down_bow_attrs },
  { US"duration",               nocheck_attrs },
  { US"dynamics",               dynamics_attrs },
  { US"encoding",               NULL },
  { US"encoding-date",          NULL },
  { US"end-line",               nocheck_attrs },
  { US"end-paragraph",          nocheck_attrs },
  { US"ending",                 ending_attrs },
  { US"ensemble",               nocheck_attrs },
  { US"extend",                 extend_attrs },
  { US"f",                      NULL },
  { US"falloff",                nocheck_attrs },
  { US"fermata",                fermata_attrs },
  { US"ff",                     NULL },
  { US"fifths",                 NULL },
  { US"forward",                NULL },
  { US"fp",                     NULL },
  { US"glyph",                  nocheck_attrs },
  { US"grace",                  grace_attrs },
  { US"group-abbreviation",     NULL },
  { US"group-abbreviation-display", NULL },
  { US"group-barline",          NULL },
  { US"group-name",             NULL },
  { US"group-name-display",     NULL },
  { US"group-symbol",           group_symbol_attrs },
  { US"identification",         NULL },
  { US"instrument",             nocheck_attrs },
  { US"instrument-name",        nocheck_attrs },
  { US"instrument-sound",       nocheck_attrs },
  { US"instruments",            nocheck_attrs },
  { US"key",                    key_attrs },
  { US"key-accidental",         NULL },
  { US"key-alter",              nocheck_attrs },
  { US"key-step",               NULL },
  { US"left-divider",           nocheck_attrs },
  { US"left-margin",            NULL },
  { US"line",                   NULL },
  { US"line-width",             nocheck_attrs },
  { US"lyric",                  lyric_attrs },
  { US"lyric-font",             lyric_font_attrs },
  { US"lyric-language",         nocheck_attrs },
  { US"measure",                measure_attrs },
  { US"measure-numbering",      NULL },
  { US"metronome",              metronome_attrs },
  { US"mf",                     NULL },
  { US"midi-channel",           nocheck_attrs },
  { US"midi-device",            nocheck_attrs },
  { US"midi-instrument",        nocheck_attrs },
  { US"midi-program",           nocheck_attrs },
  { US"midi-unpitched",         nocheck_attrs },
  { US"millimeters",            NULL },
  { US"mode",                   NULL },
  { US"movement-number",        nocheck_attrs },
  { US"movement-title",         NULL },
  { US"mp",                     NULL },
  { US"music-font",             nocheck_attrs },
  { US"normal-notes",           NULL },
  { US"normal-type",            nocheck_attrs },
  { US"notations",              NULL },
  { US"note",                   note_attrs },
  { US"note-size",              nocheck_attrs },
  { US"notehead",               notehead_attrs },
  { US"octave",                 NULL },
  { US"octave-change",          nocheck_attrs },
  { US"offset",                 offset_attrs },
  { US"ornaments",              NULL },
  { US"other-articulation",     nocheck_attrs },
  { US"other-direction",        nocheck_attrs },
  { US"p",                      NULL },
  { US"page-height",            NULL },
  { US"page-layout",            NULL },
  { US"page-margins",           page_margins_attrs },
  { US"page-width",             NULL },
  { US"pan",                    nocheck_attrs },
  { US"part",                   part_attrs },
  { US"part-abbreviation",      NULL },
  { US"part-abbreviation-display", NULL },
  { US"part-group",             part_group_attrs },
  { US"part-list",              NULL },
  { US"part-name",              part_name_attrs },
  { US"part-name-display",      NULL },
  { US"pedal",                  pedal_attrs },
  { US"per-minute",             NULL },
  { US"pitch",                  NULL },
  { US"plop",                   nocheck_attrs },
  { US"pp",                     NULL },
  { US"print",                  print_attrs },
  { US"rehearsal",              rehearsal_attrs },
  { US"repeat",                 repeat_attrs },
  { US"rest",                   rest_attrs },
  { US"right-divider",          nocheck_attrs },
  { US"right-margin",           NULL },
  { US"rights",                 NULL },
  { US"scaling",                NULL },
  { US"scoop",                  nocheck_attrs },
  { US"score-instrument",       nocheck_attrs },
  { US"score-part",             score_part_attrs },
  { US"score-partwise",         score_partwise_attrs },
  { US"sf",                     NULL },
  { US"sign",                   NULL },
  { US"slur",                   slur_attrs },
  { US"software",               nocheck_attrs },
  { US"solo",                   nocheck_attrs },
  { US"sound",                  nocheck_attrs },
  { US"spiccato",               articulation_attrs },
  { US"staccatissimo",          articulation_attrs },
  { US"staccato",               articulation_attrs },
  { US"staff",                  NULL },
  { US"staff-details",          staff_details_attrs },
  { US"staff-distance",         NULL },
  { US"staff-layout",           staff_layout_attrs },
  { US"staff-size",             NULL },
  { US"staves",                 NULL },
  { US"stem",                   stem_attrs },
  { US"step",                   NULL },
  { US"stress",                 nocheck_attrs },
  { US"strong-accent",          articulation_attrs },
  { US"supports",               nocheck_attrs },
  { US"syllabic",               NULL },
  { US"system-distance",        NULL },
  { US"system-dividers",        NULL },
  { US"system-layout",          NULL },
  { US"system-margins",         NULL },
  { US"technical",              NULL },
  { US"tenths",                 NULL },
  { US"tenuto",                 articulation_attrs },
  { US"text",                   NULL },
  { US"tie",                    nocheck_attrs },
  { US"tied",                   tied_attrs },
  { US"time",                   time_attrs },
  { US"time-modification",      NULL },
  { US"top-margin",             NULL },
  { US"top-system-distance",    NULL },
  { US"transpose",              nocheck_attrs },
  { US"tremolo",                tremolo_attrs },
  { US"trill-mark",             trill_mark_attrs },
  { US"tuplet",                 tuplet_attrs },
  { US"turn",                   turn_attrs },
  { US"type",                   type_attrs },
  { US"unpitched",              NULL },
  { US"unstress",               nocheck_attrs },
  { US"up-bow",                 up_bow_attrs },
  { US"voice",                  nocheck_attrs },
  { US"volume",                 nocheck_attrs },
  { US"wedge",                  wedge_attrs },
  { US"word-font",              word_font_attrs },
  { US"words",                  words_attrs },
  { US"work",                   NULL },
  { US"work-number",            NULL },
  { US"work-title",             NULL },
};

int xml_supported_elements_count = 
  sizeof(xml_supported_elements)/sizeof(xml_elliststr);



/*************************************************
*         Tables of named XML entities           *
*************************************************/

/* This table must be in collating sequence of entity name, because it is
searched by binary chop. If a replacement starts with "&#x" this in turn is
interpreted as a numerical entity. Other replacements are not re-scanned.

*** This comment is from the original SDoP code. Haven't sorted this out fully
for pmw yet. ***

The entities that are commented out are some that are listed in the DocBook
specification, but which do not correspond to characters in the PostScript
fonts. Some of those that are not commented out (e.g. Cdot) are recognized by
SDoP because their codes are less than 017F, but as they don't have glyphs in
the PostScript fonts, they are printed as the currency symbol. */

xml_entity_block xml_entity_list[] = {
  { US"AElig",             US"&#x00c6" },
  { US"Aacute",            US"&#x00c1" },
  { US"Abreve",            US"&#x0102" },
  { US"Acirc",             US"&#x00c2" },
  { US"Agrave",            US"&#x00c0" },
  { US"Amacr",             US"&#x0100" },
  { US"Aogon",             US"&#x0104" },
  { US"Aring",             US"&#x00c5" },
  { US"Atilde",            US"&#x00c3" },
  { US"Auml",              US"&#x00c4" },
  { US"Cacute",            US"&#x0106" },
  { US"Ccaron",            US"&#x010c" },
  { US"Ccedil",            US"&#x00c7" },
  { US"Ccirc",             US"&#x0108" },
  { US"Cdot",              US"&#x010a" },
  { US"Dagger",            US"&#x2021" },
  { US"Dcaron",            US"&#x010e" },
  { US"Dstrok",            US"&#x0110" },
  { US"ENG",               US"&#x014a" },
  { US"ETH",               US"&#x00d0" },
  { US"Eacute",            US"&#x00c9" },
  { US"Ecaron",            US"&#x011a" },
  { US"Ecirc",             US"&#x00ca" },
  { US"Edot",              US"&#x0116" },
  { US"Egrave",            US"&#x00c8" },
  { US"Emacr",             US"&#x0112" },
  { US"Eogon",             US"&#x0118" },
  { US"Euml",              US"&#x00cb" },
  { US"Euro",              US"&#x20ac" },
  { US"Gbreve",            US"&#x011e" },
  { US"Gcedil",            US"&#x0122" },
  { US"Gcirc",             US"&#x011c" },
  { US"Gdot",              US"&#x0120" },
  { US"Hcirc",             US"&#x0124" },
  { US"Hstrok",            US"&#x0126" },
  { US"IJlig",             US"&#x0132" },
  { US"Iacute",            US"&#x00cd" },
  { US"Icirc",             US"&#x00ce" },
  { US"Idot",              US"&#x0130" },
  { US"Igrave",            US"&#x00cc" },
  { US"Imacr",             US"&#x012a" },
  { US"Iogon",             US"&#x012e" },
  { US"Itilde",            US"&#x0128" },
  { US"Iuml",              US"&#x00cf" },
  { US"Jcirc",             US"&#x0134" },
  { US"Kcedil",            US"&#x0136" },
  { US"Lacute",            US"&#x0139" },
  { US"Lcaron",            US"&#x013d" },
  { US"Lcedil",            US"&#x013b" },
  { US"Lmidot",            US"&#x013f" },
  { US"Lstrok",            US"&#x0141" },
  { US"Nacute",            US"&#x0143" },
  { US"Ncaron",            US"&#x0147" },
  { US"Ncedil",            US"&#x0145" },
  { US"Ntilde",            US"&#x00d1" },
  { US"OElig",             US"&#x0152" },
  { US"Oacute",            US"&#x00d3" },
  { US"Ocirc",             US"&#x00d4" },
  { US"Odblac",            US"&#x0150" },
  { US"Ograve",            US"&#x00d2" },
  { US"Omacr",             US"&#x014c" },
  { US"Oslash",            US"&#x00d8" },
  { US"Otilde",            US"&#x00d5" },
  { US"Ouml",              US"&#x00d6" },
  { US"Racute",            US"&#x0154" },
  { US"Rcaron",            US"&#x0158" },
  { US"Rcedil",            US"&#x0156" },
  { US"Sacute",            US"&#x015a" },
  { US"Scaron",            US"&#x0160" },
  { US"Scedil",            US"&#x015e" },
  { US"Scirc",             US"&#x015c" },
  { US"THORN",             US"&#x00de" },
  { US"Tcaron",            US"&#x0164" },
  { US"Tcedil",            US"&#x0162" },
  { US"Tstrok",            US"&#x0166" },
  { US"Uacute",            US"&#x00da" },
  { US"Ubreve",            US"&#x016c" },
  { US"Ucirc",             US"&#x00db" },
  { US"Udblac",            US"&#x0170" },
  { US"Ugrave",            US"&#x00d9" },
  { US"Umacr",             US"&#x016a" },
  { US"Uogon",             US"&#x0172" },
  { US"Uring",             US"&#x016e" },
  { US"Utilde",            US"&#x0168" },
  { US"Uuml",              US"&#x00dc" },
  { US"Wcirc",             US"&#x0174" },
  { US"Yacute",            US"&#x00dd" },
  { US"Ycirc",             US"&#x0176" },
  { US"Yuml",              US"&#x0178" },
  { US"Zacute",            US"&#x0179" },
  { US"Zcaron",            US"&#x017d" },
  { US"Zdot",              US"&#x017b" },
  { US"aacute",            US"&#x00e1" },
  { US"abreve",            US"&#x0103" },
  { US"acirc",             US"&#x00e2" },
  { US"aelig",             US"&#x00e6" },
  { US"agrave",            US"&#x00e0" },
  { US"amacr",             US"&#x0101" },
  { US"amp",               US"&" },
  { US"aogon",             US"&#x0105" },
  { US"apos",              US"'" },
  { US"aring",             US"&#x00e5" },
  { US"atilde",            US"&#x00e3" },
  { US"auml",              US"&#x00e4" },
/*  { US"blank",             US"&#x2423" }, */
/*  { US"blk12",             US"&#x2592" }, */
/*  { US"blk14",             US"&#x2591" }, */
/*  { US"blk34",             US"&#x2593" }, */
/*  { US"block",             US"&#x2588" }, */
  { US"brvbar",            US"&#x00a6" },
/*  { US"bull",              US"&#x2022" }, */
  { US"cacute",            US"&#x0107" },
/*  { US"caret",             US"&#x2041" }, */
  { US"ccaron",            US"&#x010d" },
  { US"ccedil",            US"&#x00e7" },
  { US"ccirc",             US"&#x0109" },
  { US"cdot",              US"&#x010b" },
  { US"cent",              US"&#x00a2" },
  { US"check",             US"&#x2713" },
/*  { US"cir",               US"&#x25cb" }, */
  { US"clubs",             US"&#x2663" },
  { US"copy",              US"&#x00a9" },
/*  { US"copysr",            US"&#x2117" }, */
  { US"cross",             US"&#x2717" },
  { US"curren",            US"&#x00a4" },
  { US"dagger",            US"&#x2020" },
  { US"darr",              US"&#x2193" },
/*  { US"dash",              US"&#x2010" }, */
  { US"dcaron",            US"&#x010f" },
  { US"deg",               US"&#x00b0" },
  { US"diams",             US"&#x2666" },
  { US"divide",            US"&#x00f7" },
/*  { US"dlcrop",            US"&#x230d" }, */
/*  { US"drcrop",            US"&#x230c" }, */
  { US"dstrok",            US"&#x0111" },
/*  { US"dtri",              US"&#x25bf" }, */
/*  { US"dtrif",             US"&#x25be" }, */
  { US"eacute",            US"&#x00e9" },
  { US"ecaron",            US"&#x011b" },
  { US"ecirc",             US"&#x00ea" },
  { US"edot",              US"&#x0117" },
  { US"egrave",            US"&#x00e8" },
  { US"emacr",             US"&#x0113" },
  { US"eng",               US"&#x014b" },
  { US"eogon",             US"&#x0119" },
  { US"eth",               US"&#x00f0" },
  { US"euml",              US"&#x00eb" },
/*  { US"female",            US"&#x2640" }, */
/*  { US"ffilig",            US"&#xfb03" }, */
/*  { US"fflig",             US"&#xfb00" }, */
/*  { US"ffllig",            US"&#xfb04" }, */
  { US"filig",             US"&#xfb01" },
/*  { US"flat",              US"&#x266d" }, */
  { US"fllig",             US"&#xfb02" },
  { US"footcenter",        US"&footcentre;" },
  { US"footcentre",        US"&footcentre;" },
  { US"footleft",          US"&footleft;" },
  { US"footright",         US"&footright;" },
  { US"frac12",            US"&#x00bd" },
/*  { US"frac13",            US"&#x2153" }, */
  { US"frac14",            US"&#x00bc" },
/*  { US"frac15",            US"&#x2155" }, */
/*  { US"frac16",            US"&#x2159" }, */
/*  { US"frac18",            US"&#x215b" }, */
/*  { US"frac23",            US"&#x2154" }, */
/*  { US"frac25",            US"&#x2156" }, */
  { US"frac34",            US"&#x00be" },
/*  { US"frac35",            US"&#x2157" }, */
/*  { US"frac38",            US"&#x215c" }, */
/*  { US"frac45",            US"&#x2158" }, */
/*  { US"frac56",            US"&#x215a" }, */
/*  { US"frac58",            US"&#x215d" }, */
/*  { US"frac78",            US"&#x215e" }, */
/*  { US"gacute",            US"&#x01f5" }, */
  { US"gbreve",            US"&#x011f" },
  { US"gcirc",             US"&#x011d" },
  { US"gdot",              US"&#x0121" },
  { US"gt",                US">" },
  { US"half",              US"&#x00bd" },
  { US"hcirc",             US"&#x0125" },
  { US"headcenter",        US"&headcentre;" },
  { US"headcentre",        US"&headcentre;" },
  { US"headleft",          US"&headleft;" },
  { US"headright",         US"&headright;" },
  { US"hearts",            US"&#x2665" },
  { US"hellip",            US"&#x2026" },
/*  { US"horbar",            US"&#x2015" }, */
  { US"hstrok",            US"&#x0127" },
/*  { US"hybull",            US"&#x2043" }, */
  { US"iacute",            US"&#x00ed" },
  { US"icirc",             US"&#x00ee" },
  { US"iexcl",             US"&#x00a1" },
  { US"igrave",            US"&#x00ec" },
  { US"ijlig",             US"&#x0133" },
  { US"imacr",             US"&#x012b" },
/*  { US"incare",            US"&#x2105" }, */
  { US"inodot",            US"&#x0131" },
  { US"iogon",             US"&#x012f" },
  { US"iquest",            US"&#x00bf" },
  { US"itilde",            US"&#x0129" },
  { US"iuml",              US"&#x00ef" },
  { US"jcirc",             US"&#x0135" },
  { US"kcedil",            US"&#x0137" },
  { US"kgreen",            US"&#x0138" },
  { US"lacute",            US"&#x013a" },
  { US"laquo",             US"&#x00ab" },
  { US"larr",              US"&#x2190" },
  { US"lcaron",            US"&#x013e" },
  { US"lcedil",            US"&#x013c" },
  { US"ldquo",             US"&#x201c" },
  { US"ldquor",            US"&#x201e" },
/*  { US"lhblk",             US"&#x2584" }, */
  { US"lmidot",            US"&#x0140" },
  { US"loz",               US"&#x25ca" },
  { US"lsquo",             US"&#x2018" },
  { US"lsquor",            US"&#x201a" },
  { US"lstrok",            US"&#x0142" },
  { US"lt",                US"<" },
/*  { US"ltri",              US"&#x25c3" }, */
/*  { US"ltrif",             US"&#x25c2" }, */
/*  { US"male",              US"&#x2642" }, */
  { US"malt",              US"&#x2720" },
/*  { US"marker",            US"&#x25ae" }, */
  { US"mdash",             US"&#x2014" },
  { US"micro",             US"&#x00b5" },
  { US"middot",            US"&#x00b7" },
  { US"mldr",              US"&#x2026" },
  { US"nacute",            US"&#x0144" },
  { US"napos",             US"&#x0149" },
/*  { US"natur",             US"&#x266e" }, */
  { US"nbsp",              US"&#x00a0" },
  { US"ncaron",            US"&#x0148" },
  { US"ncedil",            US"&#x0146" },
  { US"ndash",             US"&#x2013" },
/*  { US"nldr",              US"&#x2025" }, */
  { US"not",               US"&#x00ac" },
  { US"ntilde",            US"&#x00f1" },
  { US"oacute",            US"&#x00f3" },
  { US"ocirc",             US"&#x00f4" },
  { US"odblac",            US"&#x0151" },
  { US"oelig",             US"&#x0153" },
  { US"ograve",            US"&#x00f2" },
/*  { US"ohm",               US"&#x2126" }, */
  { US"omacr",             US"&#x014d" },
  { US"ordf",              US"&#x00aa" },
  { US"ordm",              US"&#x00ba" },
  { US"oslash",            US"&#x00f8" },
  { US"otilde",            US"&#x00f5" },
  { US"ouml",              US"&#x00f6" },
  { US"para",              US"&#x00b6" },
  { US"phone",             US"&#x260e" },
  { US"plusmn",            US"&#x00b1" },
  { US"pound",             US"&#x00a3" },
  { US"quot",              US"\"" },
  { US"racute",            US"&#x0155" },
  { US"raquo",             US"&#x00bb" },
  { US"rarr",              US"&#x2192" },
  { US"rcaron",            US"&#x0159" },
  { US"rcedil",            US"&#x0157" },
  { US"rdquo",             US"&#x201d" },
  { US"rdquor",            US"&#x201d" },
/*  { US"rect",              US"&#x25ad" }, */
  { US"reg",               US"&#x00ae" },
  { US"rsquo",             US"&#x2019" },
  { US"rsquor",            US"&#x2019" },
/*  { US"rtri",              US"&#x25b9" }, */
/*  { US"rtrif",             US"&#x25b8" }, */
/*  { US"rx",                US"&#x211e" }, */
  { US"sacute",            US"&#x015b" },
  { US"scaron",            US"&#x0161" },
  { US"scedil",            US"&#x015f" },
  { US"scirc",             US"&#x015d" },
  { US"sect",              US"&#x00a7" },
  { US"sext",              US"&#x2736" },
/*  { US"sharp",             US"&#x266f" }, */
  { US"shy",               US"&#x00ad" },
  { US"spades",            US"&#x2660" },
/*  { US"squ",               US"&#x25a1" }, */
/*  { US"squf",              US"&#x25aa" }, */
  { US"sup1",              US"&#x00b9" },
  { US"sup2",              US"&#x00b2" },
  { US"sup3",              US"&#x00b3" },
  { US"szlig",             US"&#x00df" },
/*  { US"target",            US"&#x2316" }, */
  { US"tcaron",            US"&#x0165" },
  { US"tcedil",            US"&#x0163" },
/*  { US"telrec",            US"&#x2315" }, */
  { US"thorn",             US"&#x00fe" },
  { US"times",             US"&#x00d7" },
  { US"trade",             US"&#x2122" },
  { US"tstrok",            US"&#x0167" },
  { US"uacute",            US"&#x00fa" },
  { US"uarr",              US"&#x2191" },
  { US"ubreve",            US"&#x016d" },
  { US"ucirc",             US"&#x00fb" },
  { US"udblac",            US"&#x0171" },
  { US"ugrave",            US"&#x00f9" },
/*  { US"uhblk",             US"&#x2580" }, */
/*  { US"ulcrop",            US"&#x230f" }, */
  { US"umacr",             US"&#x016b" },
  { US"uogon",             US"&#x0173" },
/*  { US"urcrop",            US"&#x230e" }, */
  { US"uring",             US"&#x016f" },
  { US"utilde",            US"&#x0169" },
/*  { US"utri",              US"&#x25b5" }, */
/*  { US"utrif",             US"&#x25b4" }, */
  { US"uuml",              US"&#x00fc" },
/*  { US"vellip",            US"&#x22ee" }, */
  { US"wcirc",             US"&#x0175" },
  { US"yacute",            US"&#x00fd" },
  { US"ycirc",             US"&#x0177" },
  { US"yen",               US"&#x00a5" },
  { US"yuml",              US"&#x00ff" },
  { US"zacute",            US"&#x017a" },
  { US"zcaron",            US"&#x017e" },
  { US"zdot",              US"&#x017c" }
};

int xml_entity_list_count = sizeof(xml_entity_list)/sizeof(xml_entity_block);

/* End of xml_tables.c */
