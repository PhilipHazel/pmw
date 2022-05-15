/*************************************************
*            PMW transposing functions           *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: February 2021 */
/* This file last modified: May 2022 */

#include "pmw.h"


/* These tables specify whether a requested transposed accidental can actually
be used. The first five specify which note pitches can be expressed using the
given accidental. The sixth table selects between the first five tables
according to the requested transposed accidental. At present, half accidentals
can never be used. */

static uschar sharpable[] = {
/*  C    C#-    C#    D$-     D     D#-    D#    E$-     E */
  TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE,
/* E#-    F     F#-    F#    G$-     G     G#-    G#    A$- */
  FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE,
/*  A     A#-    A#    B$-    B      B#- */
  FALSE, FALSE, TRUE, FALSE, FALSE, FALSE };

static uschar flatable[] = {
/*  C    C#-     C#    D$-     D     D#-    D#    E$-    E */
  FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE,
/*  E#-     F      F#-    F#    G$-     G      G#-    G#    A$- */
   FALSE, FALSE,  FALSE, TRUE, FALSE, FALSE,  FALSE, TRUE, FALSE,
/*  A      A#-    A#     B$-    B      B#- */
   FALSE, FALSE, TRUE,  FALSE, TRUE, FALSE };

static uschar dsharpable[] = {
/*  C     C#-    C#     D$-    D     D#-    D#     E$-    E */
  FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE,
/*  E#-    F     F#-    F#     G$-    G     G#-    G#     A$- */
  FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE,
/*  A    A#-     A#    B$-    B     B#- */
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE };

static uschar dflatable[] = {
/*  C    C#-     C#    D$-     D     D#-    D#    E$-    E */
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE,
/* E#-     F     F#-    F#    G$-     G     G#-   G#     A$- */
  FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE,
/*  A     A#-    A#     B$-    B      B#- */
  TRUE, FALSE, FALSE, FALSE, FALSE, FALSE };

static uschar naturalable[] = {
/*  C    C#-     C#    D$-    D     D#-    D#     E$-    E */
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE,
/* E#-     F     F#-    F#    G$-    G     G#-    G#     A$- */
  FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE,
/*  A     A#-    A#     B$-    B     B#- */
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE };

static uschar *able[] = {
  NULL, naturalable, NULL, sharpable, dsharpable, NULL, flatable, dflatable };

/* These tables are used when transposing a note according to the number of
letter changes in the transposed key signature. */

static uint8_t tp_forward_offset[] = {
  2, 0, 4, 0, 5, 7, 0, 9, 0, 11, 0, 0 };

static uint8_t tp_forward_pitch[] = {
  2, 0, 2, 0, 1, 2, 0, 2, 0, 2, 0, 1 };

static uint8_t tp_reverse_offset[] = {
  11, 0, 0, 0, 2, 4, 0, 5, 0, 7, 0, 9 };

static uint8_t tp_reverse_pitch[] = {
  1, 0, 2, 0, 2, 1, 0, 2, 0, 2, 0, 2 };

/* This table gives the accidental to use for a given offset from a note. The
values are offset by 4. The "three-quarters" values should never be used. */

static uint8_t tp_newacc[] = {
  ac_df, 0, ac_fl, ac_hf, ac_nt, ac_hs, ac_sh, 0, ac_ds };

/* Table of key transpositions. Each entry in the table gives the new key for
an upward transposition of one semitone. The zero entries for non-existant keys
should never be accessed. */

static uschar tp_keytable[] = {
/* A   B   C   D   E   F   G */
  15,  2, 17, 18,  5, 20, 14,   /*  0 -  6 natural */
  0,   0,  3,  0,  0,  6,  0,   /*  7 - 13 sharp */
  0,   1,  2,  3,  4,  5,  6,   /* 14 - 20 flat */
  36, 23, 30, 39, 26, 33, 34,   /* 21 - 27 minor */
  22,  0, 24, 25,  0, 27, 21,   /* 28 - 34 sharp minor */
  21, 22, 23, 24, 25, 25, 26 }; /* 35 - 41 flat minor */

/* Table of enharmonic keys; the first of each pair is a key that is never
automatically selected (i.e. is not in the above table); the second is the
equivalent. */

static uschar enh_keytable[] = {
  16,  1,   /* C$  = B% */
   9, 17,   /* C#  = D$ */
  12, 20,   /* F#  = G$ */
  35, 34,   /* A$m = G#m */
  31, 39,   /* D#m = E$m */
  28, 36,   /* A#m = B$m */
  255 };    /* Marks end */



/*************************************************
*           Transpose a note                     *
*************************************************/

/* This function is called when reading a note, and also when processing a
string that contains note (chord) names. The amount by which to transpose is
set in active_transpose.

Arguments:
  abspitch               the absolute pitch
  pitch                  the normal pitch (updated)
  acc                    the accidental value (updated)
  transposeacc           if not 0, accidental required
  transposedaccforce     retain accidental, even if implied by new key
  acc_onenote            TRUE if accidental is printed above/below, and
                           hence applies only to a single note
  texttranspose          TRUE if transposing a note name in text
  tiedcount              < 0 if note is not tied, else note number in a tie

Returns:                 the transposed absolute pitch
                         the transposed pitch in *pitch
                         the transposed accidental in *acc
*/

int16_t
transpose_note(int16_t abspitch, int16_t *pitch, uint8_t *acc,
  uint8_t transposeacc, BOOL transposedaccforce, BOOL acc_onenote,
  BOOL texttranspose, int tiedcount)
{
int newpitch;
usint newacc;

/* First, transpose the absolute pitch */

abspitch += active_transpose;

/* If a particular accidental is requested, and the new note is suitable, use
it. */

if (transposeacc != ac_no && able[transposeacc][abspitch%24])
  {
  newacc = transposeacc;
  newpitch = abspitch - read_accpitch[newacc];
  }

/* Otherwise we must change the note letter by the same amount as the note
letter of the key signature has changed. We know that the value of pitch is
always that of a "white" note. Therefore, we can work with offsets in the range
0-11 instead of 0-23, which makes for smaller tables. */

else
  {
  int i = active_transpose_letter;
  int offset, note_offset;

  newpitch = *pitch;
  offset = (newpitch%24)/2;

  if (i >= 0)
    {
    while (i-- > 0)
      {
      newpitch += 2 * tp_forward_pitch[offset];
      offset = tp_forward_offset[offset];
      }
    }
  else
    {
    while (i++ < 0)
      {
      newpitch -= 2 * tp_reverse_pitch[offset];
      offset = tp_reverse_offset[offset];
      }
    }

  /* Allow for >= octave transposition. */

  while (newpitch <= abspitch - 24) newpitch += 24;
  while (newpitch >= abspitch + 24) newpitch -= 24;

  /* Now set offset to the difference between the true pitch and the pitch of
  the written note without an accidental. For transpositions around an octave
  we may have to correct for wraparound effects. */

  offset = abspitch - newpitch;

  if (offset >= 20)
    {
    offset -= 24;
    newpitch += 24;
    }
  else if (offset <= -20)
    {
    offset += 24;
    newpitch -= 24;
    }

  /* This is the offset within the octave. */

  note_offset = newpitch % 24;

  /* Transposition of half accidentals may result in three-quarter accidentals.
  As there are no such beasts, we must convert to the adjacent note with a half
  accidental.

  There are also some rare cases when the offset ends up as 6 or -6 (i.e. 3
  semitones):

  (1) Double accidentals are involved. For example, C-double-flat cannot be
  transposed by +1 when the letter change derived from the key signature is
  also +1 because B cannot be represented as some accidental applied to D.

  (2) A note is specified using an unexpected accidental. For example, if G$ is
  used in the key of E major, transposed up by 2 semitones to G-flat major. The
  note should end up as A$, but the algorithm above wants it to be offset from
  B because the key letter has changed by 2.

  Most probably these situations will never arise in practice, but just in case
  be prepared to adjust things. */

  if (offset == -3 || offset == -6)  /* Downward accidental */
    {
    if (note_offset == 0 || note_offset == 10)  /* C or F */
      {
      newpitch -= P_T/2;
      offset += 2;
      }
    else
      {
      newpitch -= P_T;
      offset += 4;
      }
    }

  else if (offset == 3 || offset == 6)  /* Upward accidental */
    {
    if (note_offset == 8 || note_offset == 22)  /* E or B */
      {
      newpitch += P_T/2;
      offset -= 2;
      }
    else
      {
      newpitch += P_T;
      offset -= 4;
      }
    }

  /* This double-check caught some bugs in the past for auto-selected letter
  changes. The maximum accidental offset is 4 quarter tones. */

  if (offset > 4 || offset < (-4))
    {
    if (active_transpose_letter_is_auto)
      error(ERR71, *pitch, *acc, abspitch, newpitch);  /* Hard */
    else
      {
      error(ERR72, active_transpose_letter, active_transpose);  /* Hard */
      }
    }

  /* The value of offset is now in the range -4 to +4 (number of quarter tones
  from the plain note), though +3 and -3 should be unused. */

  newacc = tp_newacc[offset+4];
  }

/* We now have the new pitch and its accidental. If we are transposing an
actual note, as opposed to a note name in some text, there is extra work to do
on the accidental. */

if (!texttranspose)
  {
  int newaccpitch;
  int impliedacc;
  BOOL keyNomit = FALSE;

  /* When the key is N (C major but never transposed) there are special rules
  for adjusting accidentals, as the default note transposing rules assume the
  result will have a key signature. Double sharps and flats are retained only
  if they were in the input; C-flat, B-sharp, E-sharp, and F-flat are converted
  to enharmonic notes unless the input was also a similar "white-note" sharp or
  flat. */

  if (srs.key == key_N)
    {
    int note_offset = newpitch%24;
    int old_offset = (*pitch)%24;
    BOOL isEorB = note_offset == 8 || note_offset == 22;
    BOOL isCorF = note_offset == 0 || note_offset == 10;
    BOOL old_isEorB = old_offset == 8 || old_offset == 22;
    BOOL old_isCorF = old_offset == 0 || old_offset == 10;

    switch (newacc)
      {
      case ac_ds:
      if (*acc != ac_ds)
        {
        if (isEorB)
          {
          newpitch += P_T/2;
          newacc = ac_sh;
          }
        else
          {
          newpitch += P_T;
          newacc = ac_nt;
          }
        }
      break;

      case ac_df:
      if (*acc != ac_df)
        {
        if (isCorF)
          {
          newpitch -= P_T/2;
          newacc = ac_fl;
          }
        else
          {
          newpitch -= P_T;
          newacc = ac_nt;
          }
        }
      break;

      case ac_sh:
      if (isEorB && !old_isEorB)
        {
        newpitch += P_T/2;
        newacc = ac_nt;
        }
      break;

      case ac_fl:
      if (isCorF && !old_isCorF)
        {
        newpitch -= P_T/2;
        newacc = ac_nt;
        }
      break;
      }

    /* In key N, we normally want to omit all redundant accidentals except for
    a natural at the first non-tied note of a bar. */

    if (*acc != ac_nt || !brs.firstnontied) keyNomit = TRUE;
    }

  /* Handle implied accidental. If this accidental is already implied, and
  there was no accidental in the input or if the force option is unset, or if
  special key N conditions apply, cancel the accidental. The case of a tied
  note is special and is handled by passing in the accidental to check against.
  */

  newaccpitch = read_accpitch[newacc];
  impliedacc = (tiedcount < 0)? read_baraccs_tp[newpitch] :
    read_tiedata[tiedcount].acc_tp;

  if (impliedacc == newaccpitch &&
       (*acc == ac_no || !transposedaccforce || keyNomit))
    newacc = ac_no;

  /* Otherwise, remember the accidental for next time in this bar, unless
  acc_onenote is set, which means that the accidental applies only to this note
  (printed above/below) and does not apply to later notes in the bar. */

  else if (!acc_onenote) read_baraccs_tp[newpitch] = newaccpitch;
  }

/* Return the transposed pitch+accidental and absolute pitch */

*pitch = newpitch;
*acc = newacc;
return abspitch;
}


/*************************************************
*            Transpose key signature             *
*************************************************/

/* As well as returning the transposed key, we also set up the variable
active_transpose_letter to contain the number of letter changes that are
required for any transposition. This has to be done even when the key signature
is N, which means "key signature does not transpose". However, in this case the
new key is discarded. This function is called even when there is no
transposition (in which case it does nothing). This is not the same as a
transposition of zero.

Earlier versions of PMW worked in semitones. We now work in quarter tones, but
standard key signatures can only be transposed by semitones. However, the
amount of transposition, set in active_transpose, is in quarter tones.

Argument: key signature
Returns:  new key signature
*/

uint32_t
transpose_key(uint32_t key)
{
int j;
uint32_t letterkey, newkey, usekey;
trkeystr *k;
keytransstr *kt;

if (active_transpose == NO_TRANSPOSE) return key;

/* Get within octave transposition */

for (j = active_transpose; j < 0; j += 24) {}
while (j > 23) j -= 24;

/* Check for custom transpose instruction */

for (kt = main_keytranspose; kt != NULL; kt = kt->next)
  {
  if (kt->oldkey == key) break;
  }

if (kt != NULL && kt->newkeys[j] != KEY_UNSET)
  {
  int x = kt->letterchanges[j];
  if (active_transpose > 0) x = abs(x);
    else if (active_transpose < 0) x = -abs(x);
  active_transpose_letter = x;
  active_transpose_letter_is_auto = FALSE;
  return kt->newkeys[j];
  }

/* Cannot transpose custom key without instruction */

if (key >= key_X)
  {
  error(ERR73, key - key_X + 1, active_transpose);
  return key;
  }

/* Cannot transpose by quarter tone without instruction */

if ((j & 1) != 0)
  {
  error(ERR74);
  return key;
  }

j /= 2;  /* Convert to semitones */

/* Transpose a standard key */

newkey = usekey = (key == key_N)? key_C : key;

for (int i = 0; i < j; i++) newkey = tp_keytable[newkey];
letterkey = newkey;

/* See if there's been a transposed key request for the new key. */

k = main_transposedkeys;
while (k != NULL)
  {
  if (k->oldkey == newkey) { newkey = k->newkey; break; }
  k = k->next;
  }

/* If the new key has changed to an enharmonic key, use the forced key to
compute the number of letter changes; otherwise use the default new key. This
copes with the two different uses of "transposedkey": (a) to use an enharmonic
key and (b) to print music with a key signature different to the tonality. */

if (letterkey != newkey)
  {
  uschar *p = enh_keytable;
  while (*p < 255)
    {
    if (letterkey == p[1] && newkey == p[0])
      {
      letterkey = newkey;
      break;
      }
    p += 2;
    }
  }

active_transpose_letter = (letterkey%7) - (usekey%7);
active_transpose_letter_is_auto = TRUE;

if (active_transpose > 0 && active_transpose_letter < 0)
  active_transpose_letter += 7;

if (active_transpose < 0 && active_transpose_letter > 0)
  active_transpose_letter -= 7;

return (key == key_N)? key : newkey;
}

/* End of transpose_c */
