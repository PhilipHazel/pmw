/*************************************************
*        PMW native stave reading functions      *
*************************************************/

/* Copyright Philip Hazel 2025 */
/* This file created: December 2020 */
/* This file last modified: August 2025 */

#include "pmw.h"


/* Values for baraccs for the different accidentals. */

static int8_t ba_values[] = { 0, 0, 1, 2, 4, -1, -2, -4 };

/* Pitch offsets for different line/space values in the key signature table. */
                            /*  D  E   F   G   A   B  C  D  E  F   G   A */
static uint8_t ba_offsets[] = { 4, 8, 10, 14, 18, 22, 0, 4, 8, 10, 14, 18 };



/*************************************************
*                 Static variables               *
*************************************************/

static barstr **overbeam_nextbar = NULL;



/*************************************************
*           Initialize accidentals table         *
*************************************************/

/* The baraccs table is used to hold the number of quartertones in the current
accidental for a given "white note" pitch, to enable an absolute pitch to be
calculated. Values are between -4 (double flat) and +4 (double sharp). When
transposition is happening, the baraccs table is used for the input values, and
baraccs_tp for the output values. This function is called at the start of each
bar, and also after [key] and [reset], to initialized the accidentals to the
current key signature.

Arguments:
  ba         the table that is to be set (baraccs or baraccs_tp)
  key        the key signature

Returns:     nothing
*/

void
read_init_baraccs(int8_t *ba, uint32_t key)
{
uint8_t *kp;

/* Clear out the table to no accidentals, then process the current key
signature. Each byte in a key signature has an accidental in the top 4 bits,
and a position on a treble clef stave in the bottom 4, with 0 being below the
bottom line, 1 the bottom line, etc. We do not need to know the actual current
clef because the baraccs table is for absolute pitches.  */

memset(ba, 0, BARACCS_LEN * sizeof(int8_t));
for (kp = keysigtable[key]; *kp != ks_end; kp++)
  {
  usint po;
  int8_t ao = ba_values[*kp >> 4];
  for (po = ba_offsets[*kp & 0x0f]; po < BARACCS_LEN; po += 24) ba[po] = ao;
  }
}



/*************************************************
*       Handle things in square brackets         *
*************************************************/

/* Called when an initial '[' is read. Continues processing until ']' is
reached. Returns TRUE after [endstave] or EOF. */

static BOOL
handle_square_brackets(uint32_t *repeatcount)
{
for (;;)
  {
  read_sigc();

  /* Handle a stave directive */

  if (isalpha(read_c))
    {
    read_nextword();
    if (read_do_stavedirective())   /* Check for ] after [endstave */
      {
      read_sigc();
      if (read_c != ']')
        {
        error(ERR8, "]");
        if (read_c != ENDFILE) read_i--;  /* Back up to re-read */
        }
      return TRUE;
      }
    }

  /* Deal with repeated bars and nth-time bar markings. */

  else if (isdigit(read_c))
    {
    b_nbarstr *p;
    uschar *pt;
    int n = read_usint();

    if (n == 0)
      {
      error(ERR8, "Number greater than zero");
      continue;
      }

    read_sigc();
    pt = main_readbuffer + read_i - 1;

    /* Handle a repeated bar count. */

    if (isalpha(pt[2]) || (
        Ustrncmp(pt, "st", 2) != 0 &&
        Ustrncmp(pt, "nd", 2) != 0 &&
        Ustrncmp(pt, "rd", 2) != 0 &&
        Ustrncmp(pt, "th", 2) != 0))
      {
      *repeatcount = n;
      mem_record_next_item(&(brs.repeatstart));
      continue;
      }

    /* Handle an nth-time bar marking */

    p = mem_get_item(sizeof(b_nbarstr), b_nbar);
    p->x = p->y = 0;
    p->n = n;
    p->s = NULL;
    p->ssize = 0;   /**** Not currently in use ****/

    read_i++;
    read_nextsigc();

    while (read_c == '/')
      {
      int xsign = 1;
      int ysign = 1;

      read_nextc();

      switch(read_c)
        {
        case 'd':
        ysign = -1;
        /* Fall through */
        case 'u':
        read_nextc();
        read_expect_integer(&n, TRUE, FALSE);
        p->y += n * ysign;
        break;

        case 'l':
        xsign = -1;
        /* Fall through */
        case 'r':
        read_nextc();
        read_expect_integer(&n, TRUE, FALSE);
        p->x += n * xsign;
        break;

        /* One day we might want individual sizes for strings, but for
        the moment this is not implemented. */

        /*****************
        case 's':
        next_ch();
        read_expect_integer(&n, FALSE, FALSE);
        p->ssize = n;
        break;
        *****************/

        case '\"':
        p->s = string_read(curmovt->fonttype_repeatbar, TRUE);
        break;

        default:
        error(ERR8, "\"u\", \"d\", \"l\", \"r\", or quoted string");
        break;
        }
      }
    }

  /* Next character is not a letter or digit. */

  else switch(read_c)
    {
    case ENDFILE:
    return TRUE;

    /* End of the bracket coding. */

    case ']':
    read_nextsigc();
    return (read_c == ENDFILE);

    /* A string inside [] is a rehearsal mark. */

    case '"':
    string_stavestring(TRUE);
    break;

    /* Any number of default accent or ornament codings can be given. The
    accents are cumulative, but there can only be one default ornament. */

    case '\\':
    srs.accentflags = 0;
    srs.ornament = or_unset;
    read_nextc();
    for (;;)
      {
      uint32_t a;
      read_sigc();
      if (read_c == '\\')
        {
        read_nextc();
        break;
        }   /* End accent/ornament default */
      a = read_accororn(('\\'<<8) | ']');   /* Skip to \ or ] on error */
      if (a == or_unset)
        {
        if (read_c == '\\') read_nextc();
        break;
        }
      if (a < 256) srs.ornament = a; else srs.accentflags |= a;
      }
    if ((srs.accentflags & (af_staccato|af_staccatiss)) ==
      (af_staccato|af_staccatiss)) error(ERR91);
    break;

    /* Unrecognized item in [] - skip to ] */

    default:
    error_skip(ERR8, ']', "staff directive");
    return FALSE;
    }
  }

/* Control never reaches here. */
}



/*************************************************
*                 Read one bar                   *
*************************************************/

/* This is entered with the next significant character already read.

Arguments:
  barstrptr    where to return the new head of bar structure
  repeatcount  where to return the repeat count

Returns:       TRUE if reached the end of the stave
*/

static BOOL
read_bar(barstr **barstrptr, uint32_t *repeatcount)
{
BOOL done = FALSE;
BOOL endstave = FALSE;
BOOL startnotesoff = !srs.noteson;
barstr **set_overbeam_nextbar = NULL;
barstr *bar;

/* Cancel any residual [cue] from the default note flags. */

srs.noteflags &= ~(nf_cuesize|nf_cuedotalign);

/* Initialize the per-bar shared ephemeral data. Some fields are taken from the
current stave or movement. */

brs = init_breadstr;
brs.checktripletize = (srs.noteflags & nf_tripletize) != 0;

if ((brs.checklength = MFLAG(mf_check)) && srs.lastwasdouble)
  {
  brs.checklength = MFLAG(mf_checkdoublebars);
  srs.lastwasdouble = FALSE;
  }

/* Initialize the current accidental state */

read_init_baraccs(read_baraccs, srs.key);
read_init_baraccs(read_baraccs_tp, srs.key_tp);

/* Initialize a new head of bar structure, passing it back to the caller, and
making it available while reading the bar. */

*barstrptr = bar = brs.bar = mem_get(sizeof(barstr));
bar->next = bar->prev = NULL;
bar->type = b_start;
bar->repeatnumber = 0;

/* The head of bar structure starts with the appropriate fields to be the part
of the chain of bar items. Initialize the lastitem field so subsequent ones are
automatically chained on. */

read_lastitem = (bstr *)bar;

/* If the previous bar was flagged for overbeaming, we need to fill in a
pointer to this next bar unless we immediately hit [endstave] or EOL. So we
just make a note here so that we can reset overbeam_nextbar (it may get set
again if this bar needs itself to be flagged for overbeaming). */

if (overbeam_nextbar != NULL)
  {
  set_overbeam_nextbar = overbeam_nextbar;
  overbeam_nextbar = NULL;
  }

/* This is the main loop that reads items until the end of the bar. */

while (!done)
  {
  b_barlinestr *bs;

  read_sigc();

  /* Notes must begin with # $ % or certain letters, or ( for the start of a
  chord (which is handled separately in the switch below). Some special
  characters are used for other things. */

  switch (read_c)
    {
    default:
    if (Ustrchr("#$%abcdefgpqrstx", tolower(read_c)) != NULL)
      {
      read_note();
      }
    else
      {
      error(ERR89, read_c);  /* Unexpected character */
      read_nextc();
      }
    break;

    case ENDFILE:
    done = endstave = TRUE;
    break;

    case '[':
    read_nextc();
    done = endstave = handle_square_brackets(repeatcount);
    break;

    /* Various kinds of bar line. */

    case '|':
    bs = mem_get_item(sizeof(b_barlinestr), b_barline);
    bs->bartype = barline_normal;
    bs->barstyle = srs.barlinestyle;

    read_nextc();
    if (read_c == '|')           /* || is a double barline */
      {
      read_nextc();
      if (read_c == '|')
        {
        bs->bartype = barline_ending;  /* ||| is an "dnding" barline */
        read_nextc();
        }
      else
        {
        bs->bartype = barline_double;
        if (!MFLAG(mf_checkdoublebars)) brs.checklength = FALSE;
        srs.lastwasdouble = TRUE;
        }
      }

    else if (read_c == '?')      /* |? is an invisible barline */
      {
      read_nextc();
      bs->bartype = barline_invisible;
      }

    else if (isdigit(read_c))    /* Specific bar line style */
      {
      bs->barstyle = read_usint();
      }

    if (read_c == '=')           /* Continue beam over bar line */
      {
      b_overbeamstr *ob = mem_get_insert_item(sizeof(b_overbeamstr),
        b_overbeam, (bstr *)bs);
      ob->nextbar = NULL;
      overbeam_nextbar = &(ob->nextbar);
      read_nextc();
      }

    /* Cancel the ensured space for a tied chord with seconds at the end of a
    bar. */

    if (brs.lasttieensure != NULL) brs.lasttieensure->value = 0;

    /* End of bar, but not end of stave. */

    done = TRUE;
    break;

    case ';':
    case ',':
    error(ERR88, read_c);   /* Unexpected beam break character */
    read_nextsigc();
    break;

    /* A slash must be followed by another, for a caesura. */

    case '/':
    read_nextc();
    if (read_c != '/') error(ERR8, "'/' (to make // for caesura)"); else
      {
      (void)mem_get_item(sizeof(bstr), b_caesura);
      read_nextc();
      }
    break;

    /* Quotes introduce text */

    case '\"':
    string_stavestring(FALSE);
    break;

    /* A colon can be a dotted bar or the end of a repeat. */

    case ':':
    read_nextc();
    if (read_c == ')')
      {
      (void)mem_get_item(sizeof(bstr), b_rrepeat);
      read_nextc();
      if (brs.barlength == 0) error(ERR87, "end", "start");  /* Warning */
      }
    else (void)mem_get_item(sizeof(bstr), b_dotbar);
    break;

    /* An opening bracket can be the start of a repeat, or the start of a
    chord. We peek at the next character to make sure. */

    case '(':
    if (main_readbuffer[read_i] == ':')
      {
      (void)mem_get_item(sizeof(bstr), b_lrepeat);
      read_i++;
      read_nextsigc();
      if (read_c == '|') error(ERR87, "start", "end");  /* Warning */
      }
    else read_note();
    break;

    /* Curly braces are used for plets. */

    case '{':
    if (brs.pletlen != 0)
      {
      error(ERR90);
      read_nextc();
      }
    else
      {
      b_pletstr *p;
      usint  flags = 0;
      int32_t  adjustx = 0;
      int32_t  adjustyleft = srs.plety;
      int32_t  adjustyright = adjustyleft;

      /* The length of each note in the plet will be multiplied by
      brs.pletnum, and divided by brs.pletlen. */

      brs.pletnum = 0;   /* Indicates not explicitly set */
      brs.pletlen = 3;   /* Default to triplet */

      /* Read an explicit specification */

      read_nextsigc();
      if (isdigit(read_c))
        {
        brs.pletlen = read_usint();  /* First number */

        /* A slash followed by a digit indicates that the first number is
        really the number of notes we are dividing, and the real value of
        brs.pletlen follows. */

        if (read_c == '/' && isdigit(main_readbuffer[read_i]))
          {
          brs.pletnum = brs.pletlen;
          read_nextc();
          brs.pletlen = read_usint();
          }
        }

      /* If a numerator was not provided and we are dividing into a power of
      two, assume we are dividing 3 into this number; otherwise assume 2. */

      if (brs.pletnum == 0) brs.pletnum =
        ((brs.pletlen & (-brs.pletlen)) == brs.pletlen)? 3 : 2;

      /* Set a bit to remember which plets exist in this stave. This is for the
      benefit of MusicXML output's code that sets its "divisions" value. */

      srs.tuplet_bits |= 1 << (brs.pletlen - 1);

      /* If the number of irregular notes is more than twice the number of
      regular notes, we double the numerator, because the irregulars must be
      the next note size down; similarly quadruple the numerator for even more
      notes. */

      if (brs.pletlen >= 4 * brs.pletnum) brs.pletnum *= 4;
        else if (brs.pletlen >= 2 * brs.pletnum) brs.pletnum *= 2;

      /* Now deal with the options */

      while (read_c == '/')
        {
        int sign = -1;
        int8_t pflag;
        int32_t mvalue;
        int32_t *adjustyptr;

        read_nextc();
        switch(read_c)
          {
          case 'a':             /* Above */
          sign = -sign;
          /* Fall through */
          case 'b':             /* Below */
          flags &= ~(plet_a | plet_b | plet_abs | plet_bn);
          flags |= plet_by | ((read_c == 'a')? plet_a : plet_b);
          adjustyleft = adjustyright = 0;
          read_nextsigc();
          if (isdigit(read_c))
            {
            flags |= plet_abs;
            adjustyleft = adjustyright = sign * read_fixed();
            }
          break;

          case 'n':             /* Force no bracket */
          flags = (flags & ~plet_by) | plet_bn;
          read_nextc();
          break;

          case 'r':             /* /r, /rx, /ru, or /rd */
          sign = -sign;
          adjustyptr = &adjustyright;
          pflag = plet_rx;
          /* Fall through */
          case 'l':             /* /l, /lx, /lu, or /ld */
          if (sign < 0)
            {
            adjustyptr = &adjustyleft;
            pflag = plet_lx;
            }
          read_nextc();
          if (isdigit(read_c)) adjustx += sign * read_fixed(); else
            {
            sign = -1;
            switch(read_c)
              {
              case 'x':
              flags |= pflag;
              read_nextc();
              break;

              case 'u':
              sign = -sign;
              /* Fall through */
              case 'd':
              *adjustyptr += sign * read_movevalue();
              break;

              default:
              error(ERR8,
                "/l<n>, /r<n>, /lx, /rx, /lu<n>, /ld<n>, /ru<n> or /rd<n>");
              break;
              }
            }
          break;

          case 'u':             /* Move plet up */
          sign = -sign;
          /* Fall through */
          case 'd':             /* Move plet down */
          mvalue = sign * read_movevalue();
          adjustyleft += mvalue;
          adjustyright += mvalue;
          break;

          case 'x':             /* No plet mark */
          flags |= plet_x;
          read_nextc();
          break;

          default:
          error(ERR8, "a, b, n, x, u, d, l or r");
          read_nextc();
          break;
          }
        read_sigc();
        }

      /* If neither /a nor /b has been given, use the default flags */

      if ((flags & (plet_a | plet_b)) == 0) flags |= srs.pletflags;

      /* Now generate the data block */

      p = mem_get_item(sizeof(b_pletstr), b_plet);
      p->pletnum = brs.pletnum;
      p->pletlen = brs.pletlen;
      p->flags = flags;
      p->x = adjustx;
      p->yleft = adjustyleft;
      p->yright = adjustyright;
      }

    /* Prevent reset (but resets are forbidden inside plets anyway). */

    brs.resetOK = FALSE;
    break;

    /* Ends of plets are often dealt with at the end of a note, but can
    occasionally arise here if another directive intervenes. */

    case '}':
    if (brs.pletlen == 0) error(ERR90); else
      {
      brs.pletlen = 0;
      (void)mem_get_item(sizeof(bstr), b_endplet);
      }
    read_nextc();
    break;

    /* Angle brackets code for hairpins. An opening hairpin automatically
    closes an open hairpin of the opposite type. */

    case '>':
    case '<':
      {
      b_hairpinstr *p;
      BOOL ending = (read_c == '<' && srs.hairpinbegun < 0) ||
                    (read_c == '>' && srs.hairpinbegun > 0);
      int32_t slu = 0;
      int32_t sru = 0;

      /* If we are starting a new hairpin and there is already an open one,
      generate a default ending. A new hairpin also starts at a default y
      level. */

      if (!ending && srs.hairpinbegun != 0)
        {
        b_hairpinstr *q = mem_get_item(sizeof(b_hairpinstr), b_hairpin);
        q->flags = hp_end;
        q->x = q->y = q->offset = q->su = q->halfway = q->width = 0;
        srs.hairpinbegun = 0;
        }

      /* Handle a coded ending or a new start. */

      p = mem_get_item(sizeof(b_hairpinstr), b_hairpin);
      p->flags = srs.hairpinflags;
      if (read_c == '<') p->flags |= hp_cresc;
      p->width = srs.hairpinwidth;
      p->x = p->y = 0;
      p->y = ending? 0 : srs.hairpiny;
      p->halfway = 0;
      p->offset = 0;

      /* Now handle the options */

      read_nextc();
      while (read_c == '/')
        {
        int sign = +1;

        /* b is anomalous - /bar is always allowed, but /b only on
        beginning hairpins. */

        if (Ustrncmp(main_readbuffer + read_i, "bar", 3) == 0)
          {
          p->flags |= hp_bar;
          read_i += 3;
          read_nextc();
          continue;
          }

        read_nextc();
        switch(read_c)
          {
          case 'u': p->y += read_movevalue(); break;
          case 'd': p->y -= read_movevalue(); break;

          case 'l':
          sign = -1;
          /* Fall through */
          case 'r':
          if (main_readbuffer[read_i] == 'c')
            {
            read_i++;
            p->offset += sign * read_movevalue();
            }
          else p->x += sign * read_movevalue();
          break;

          case 'h':
          read_nextc();
          p->halfway = (isdigit(read_c))? read_fixed() : 500;
          p->flags |= hp_halfway;
          break;

          case 's':
          read_nextc();
          if (read_c == 'l' || read_c == 'r')
            {
            int32_t amount;
            int32_t *a = (read_c == 'l')? &slu : &sru;
            read_nextc();
            if (read_c == 'u') sign = +1;
              else if (read_c == 'd') sign = -1;
                else goto SLERROR;

            read_nextc();
            if (!read_expect_integer(&amount, TRUE, FALSE)) break;
            *a += sign * amount;
            }
          else
            {
            SLERROR:
            error(ERR8, "/slu, /sld, /sru or /srd");
            }
          break;

          /* The remaining options are allowed only on starting hairpins. */

          default:
          if (ending)
            error(ERR8, "/u, /d, /l, /r, /slu, /sru or /h");
          else switch(read_c)
            {
            case 'b':
            sign = -1;
            /* Fall through */
            case 'a':
            p->flags &= ~(hp_below | hp_middle | hp_abs);
            if (read_c == 'b') p->flags |= hp_below;
            p->y = 0;
            read_nextc();
            if (isdigit(read_c))
              {
              p->flags |= hp_abs;
              p->y = sign * read_fixed();
              }
            break;

            case 'm':
            p->flags |= hp_below | hp_middle;
            read_nextc();
            break;

            case 'w':
            read_nextc();
            read_expect_integer(&p->width, TRUE, FALSE);
            break;

            default:
            error(ERR8, "/u, /d, /l, /r, /a, /b, /m, /w, /slu, /sru or /h");
            break;
            }
          break;
          }
        }

      /* Ending hairpin */

      if (srs.hairpinbegun != 0)
        {
        p->flags |= hp_end;
        p->su = srs.hairpinsru;
        srs.hairpinbegun = 0;
        }

      /* Starting hairpin */

      else
        {
        srs.hairpinbegun = ((p->flags & hp_cresc) == 0)? +1 : -1;
        p->su = slu;
        srs.hairpinsru = sru;
        brs.resetOK = FALSE;
        }
      }
    break;
    }
  }   /* End of loop through the bar. */

/* Grumble if unclosed plet */

if (brs.pletlen != 0) error(ERR161);

/* If EOF or [endstave] was encountered before anything else, we are done. */

if (bar->next == NULL) return endstave;

/* Set the pointer to this bar in the previous bar's overbeam structure if
there is one. */

if (set_overbeam_nextbar != NULL) *set_overbeam_nextbar = bar;

/* Remember if this was an empty bar */

srs.lastwasempty = bar->next->type == b_barline;

/* If notes were switched off at the start of the bar, insert another control
for the benefit of the playing code, because it assumes notes on at the start
of each bar. */

if (startnotesoff) ((b_notesstr *)
  mem_get_insert_item(sizeof(b_notesstr), b_notes, bar->next))->value = FALSE;

/* Update the maximum length of the bar ([reset] might have set a larger or
smaller value) and check this value if enabled. If the bar contains tuplets of
prime numbers greater than 13, there may be rounding errors because the length
of a breve is not a multiple of, for example, 17. This can also happen with
tuplets of other kinds that involve hemidemisemiquavers. In these cases we
allow a small discrepancy. */

if (brs.barlength > brs.maxbarlength) brs.maxbarlength = brs.barlength;
if (brs.checklength && brs.maxbarlength != 0)
  {
  int32_t extra = brs.maxbarlength - srs.required_barlength;
  if (abs(extra) > TUPLET_ROUND)
    error(ERR101, (extra > 0)? "long":"short", sfn(abs(extra)));
  }

/* Give an error for a tie that runs into an empty bar. */

if (brs.maxbarlength == 0 && srs.lastwastied) error(ERR118, "empty bar");

/* Deal with incomplete beams and unchosen stems */

if (srs.beaming) read_setbeamstems();
mac_setstackedstems(srs.laststemup? nf_stemup : 0);

/* If tripletizing was set at any point in this bar, scan it for dotted-
quaver groups and adjust the note lengths. Also handle dotted crotchet
followed by two semiquavers. */

if (brs.checktripletize)
  {
  b_notestr *first = (b_notestr *)(bar->next);

  if (first->type != b_note) first = misc_nextnote(first);

  while (first != NULL)
    {
    b_notestr *third;
    b_notestr *next = misc_nextnote(first);

    if (next == NULL) break;

    /* Both notes must be flagged for tripletizing */

    if ((first->flags & next->flags & nf_tripletize) == 0)
      {
      first = next;  /* Not this pair */
      continue;
      }

    third = misc_nextnote(next);

    /* Dotted quaver + semiquaver */

    if (first->length == (len_quaver * 3)/2 &&
        next->length == len_squaver)
      {
      first->length = (len_crotchet * 2)/3;
      next->length = len_crotchet/3;
      first = third;
      continue;
      }

    /* Dotted crotchet + quaver */

    if (first->length == (len_crotchet * 3)/2 &&
        next->length == len_quaver)
      {
      first->length = (len_minim * 2)/3;
      next->length = len_minim/3;
      first = third;
      continue;
      }

    if (third == NULL) break;

    /* Dotted crotched + 2 semiquavers */

    if ((third->flags & nf_tripletize) != 0 &&
        first->length == (len_crotchet * 3)/2 &&
        next->length == len_squaver &&
        third->length == len_squaver)
      {
      first->length = (len_crotchet * 4)/3;
      next->length = len_crotchet/3;
      third->length = len_crotchet/3;
      first = misc_nextnote(third);
      continue;
      }

    /* No tripletizing this pair */

    first = next;
    }
  }

return endstave;
}



/*************************************************
*                Read one stave                  *
*************************************************/

/* We get here having encountered "[stave" at an appropriate place, ready to
read the next character. */

void
pmw_read_stave(void)
{
BOOL endstave = FALSE;
int32_t stave;
uint32_t lastnextbaroffset = 0;
uint32_t nextbaroffset = 0;

read_nextc();
if (!read_expect_integer(&stave, FALSE, FALSE))
  error(ERR9, "stave number");  /* Hard */

/* Initialize variables that are used while reading a stave. */

st = read_init_stave(stave, TRUE);

/* Handle number of stave lines */

if (read_c == '/')
  {
  int32_t n;
  read_nextc();
  if (!read_expect_integer(&n, FALSE, FALSE))
    error(ERR9, "number of stave lines");  /* Hard */
  if (n > 6) error(ERR8, "number in the range 0-6");
    else curmovt->stavetable[srs.stavenumber]->stavelines = n;
  }

/* Handle "omitempty" */

read_sigc();
if (Ustrncmpic(main_readbuffer + read_i - 1, "omitempty", 9) == 0)
  {
  read_i += 8;
  read_nextsigc();
  curmovt->stavetable[srs.stavenumber]->omitempty = TRUE;
  }

/* If a string or "draw" follows, obey the implied "[name]" directive. */

if (read_c == '\"' ||
    (Ustrncmpic(main_readbuffer + read_i - 1, "draw ", 5) == 0))
  read_stavename();

/* We are still inside [stave <n> .... so pretend we are at '[' and arrange to
re-read the current character. */

read_c = '[';
read_i--;

/* This is the main stave-reading loop. The yield of read_bar() is the number
of bars to skip. It is negative at the end of the stave. */

while (!endstave)
  {
  barstr *bar;
  uint32_t barrepeat = 1;

  endstave = read_bar(&bar, &barrepeat);
  nextbaroffset += brs.skip;

  /* If we have reached [endstave] or EOF and the bar is empty,
  and there is no repeat or skip pending, discard it. Otherwise, add a regular
  bar line. */

  if (endstave)
    {
    b_barlinestr *bs;
    if (bar->next == NULL && barrepeat == 1 && brs.skip == 0) break;
    bs = mem_get_item(sizeof(b_barlinestr), b_barline);
    bs->bartype = barline_normal;
    bs->barstyle = srs.barlinestyle;
    }

  /* Ensure bar indexes are large enough */

  read_ensure_bar_indexes(nextbaroffset + barrepeat - 1);

  /* If we have skipped some bars, fill in the bar index vector with pointers
  to an empty bar. We have to make a new empty bar each time in case the
  barline style has changed. */

  if (lastnextbaroffset < nextbaroffset)
    {
    barstr *bs = mem_get(sizeof(barstr));
    b_barlinestr *bl = mem_get(sizeof(b_barlinestr));

    bs->next = (bstr *)bl;
    bs->prev = NULL;
    bs->type = b_start;
    bs->repeatnumber = 0;

    bl->next = NULL;
    bl->prev = (bstr *)bs;
    bl->type = b_barline;
    bl->bartype = barline_normal;
    bl->barstyle = srs.barlinestyle;

    while (lastnextbaroffset < nextbaroffset)
      st->barindex[lastnextbaroffset++] = bs;
    }

  /* Set the pointer in the index, and note any [nocount] bars. Then duplicate
  the information for repeated bars. */

  if (barrepeat > 1) bar->repeatnumber = 1;

  for (uint32_t i = 1;;)
    {
    barstr *newbar;
    if (brs.nocount) curmovt->barvector[nextbaroffset] = -1;
    st->barindex[nextbaroffset++] = bar;
    if (i++ == barrepeat) break;

    newbar = mem_get(sizeof(barstr));
    newbar->next = brs.repeatstart;
    newbar->prev = NULL;
    newbar->type = b_start;
    newbar->repeatnumber = i;
    bar = newbar;
    }

  lastnextbaroffset = nextbaroffset;
  }

/* Reached the end of the stave */

st->longest_note = srs.longest_note;
st->shortest_note = srs.shortest_note;
st->tuplet_bits = srs.tuplet_bits;

st->barcount = nextbaroffset;
if (st->barcount > curmovt->barcount) curmovt->barcount = st->barcount;

if (srs.pitchcount != 0)
  {
  st->toppitch = srs.maxpitch;
  st->botpitch = srs.minpitch;
  st->totalpitch = srs.pitchtotal;
  st->notecount = srs.pitchcount;
  }

if (srs.pendulay != NULL || srs.pendolay != NULL) error(ERR164, stave); /* Warn */
}

/* End of pmw_read_stave.c */
