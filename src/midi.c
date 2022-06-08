/*************************************************
*            PMW MIDI output generation          *
*************************************************/

/* Copyright Philip Hazel 2021 */
/* This file created: August 2021 */
/* This file last modified: May 2022 */

#include "pmw.h"


/* This file contains code for writing a MIDI file. */

typedef struct midi_event {
  int32_t time;
  int16_t seq;
  uint8_t data[8];
} midi_event;

enum { HR_NONE, HR_REPEATED, HR_PLAYON };


/*************************************************
*             Local variables                    *
*************************************************/

static midi_event *events = NULL;
static midi_event *next_event;

static int32_t   file_count = 0;
static int32_t   last_written_time;
static uint32_t  midi_bar;
static int32_t   midi_bar_moff;
static uint8_t   midi_channel[MAX_STAVE+1];
static uint8_t   midi_channel_volume[MIDI_MAXCHANNEL];
static FILE     *midi_file;
static movtstr  *midi_movt;
static int       midi_nextbar;
static int32_t   midi_nextbar_moff;
static uint8_t   midi_note[MAX_STAVE+1];
static BOOL      midi_onebar_only = FALSE;
static uint64_t  midi_staves = ~0uL;
static uint32_t  midi_tempo;
static int8_t    midi_transpose[MAX_STAVE+1];  /* NB signed */
static int       midi_volume = 127;
static int16_t   next_event_seq;
static uint32_t  repeat_bar;
static int32_t   repeat_bar_moff;
static int       repeat_count;
static uint32_t  repeat_endbar;
static uint32_t  running_status;
static uint8_t   stavetie[MAX_STAVE+1];
static uint8_t   stavevolume[MAX_STAVE+1];


/*************************************************
*      Comparison function for sorting events    *
*************************************************/

/* This function is passed to qsort(). Similar events at the same time should
preserve their order. To do this, we give each event a sequence number that is
compared if the times are equal. This function should never return zero in
practice.

Arguments:
  a          pointer to an event structure
  b          pointer to an event structure

Returns:     -1, 0, or +1
*/

static int
cf(const void *a, const void *b)
{
const midi_event *ma = (const midi_event *)a;
const midi_event *mb = (const midi_event *)b;
if (ma->time < mb->time) return -1;
if (ma->time > mb->time) return +1;
if (ma->seq < mb->seq) return -1;
if (ma->seq > mb->seq) return +1;
return 0;
}


/*************************************************
*          Find length of bar                    *
*************************************************/

/* Scan the staves selected for playing until one with some notes in it is
found. If there are none, return zero. If the bar contains only a centred rest,
carry on looking for another stave in case this bar is a nocheck whole-bar
rest, which might be of different length to the remaining staves' bars.

Arguments:   none; the required movement/bar are in midi_movt and midi_bar
Returns:     length of the bar, or zero
*/

static
int32_t find_barlength(void)
{
int32_t yield = 0;

for (int stave = 1; stave <= midi_movt->laststave; stave++)
  {
  BOOL notjustrest;
  int32_t length, moff;
  int gracecount;

  if (mac_notbit(midi_staves, stave)) continue;

  notjustrest = FALSE;
  length = 0;
  gracecount = 0;
  moff = 0;

  for (bstr *p = (bstr *)((midi_movt->stavetable[stave])->barindex[midi_bar]);
       p != NULL; p = p->next)
    {
    if (p->type == b_reset)
      {
      if (moff > length) length = moff;
      moff = 0;
      }
    else if (p->type == b_note)
      {
      b_notestr *note = (b_notestr *)p;
      moff += note->length;
      if (note->length == 0) gracecount++; else gracecount = 0;
      if (note->spitch != 0 || (note->flags & nf_centre) == 0)
        notjustrest = TRUE;
      }
    }

  /* At bar end check for longest length in case there were resets */

  if (moff > length) length = moff;

  /* If there were grace notes at the end of the bar, increase its
  length by 1/10 second for each one. */

  length += (gracecount*len_crotchet*midi_tempo)/(60*10);

  /* If we have found a bar with notes in it other than a whole bar
  rest, we are done. Otherwise carry on, but leave length so far in
  yield in case there are no staves with notes. */

  if (length > yield) yield = length;
  if (yield > 0 && notjustrest) break;
  }

return yield;
}



/*************************************************
*              Write 32-bit number               *
*************************************************/

/* Write the most significant byte first.

Argument:  the number
Returns:   nothing
*/

static void
write32(uint32_t n)
{
fputc((n>>24)&255, midi_file);
fputc((n>>16)&255, midi_file);
fputc((n>>8)&255, midi_file);
fputc(n&255, midi_file);
file_count += 4;
}


/*************************************************
*              Write 16-bit number               *
*************************************************/

/* Write the most significant byte first.

Argument:  the number
Returns:   nothing
*/

static void
write16(int n)
{
fputc((n>>8)&255, midi_file);
fputc(n&255, midi_file);
file_count += 2;
}


/*************************************************
*             Write variable length number       *
*************************************************/

/* The number is chopped up into 7-bit chunks, and then written with the most
significant chunk first. All but the last chunk have the top bit set. This
copes with numbers up to 28-bits long. That's all that MIDI needs.

Argument:  the number
Returns:   nothing
*/

static void
writevar(int n)
{
if (n < 0x80)
  {
  fputc(n, midi_file);
  file_count++;
  }

else if (n < 0x4000)
  {
  fputc(((n>>7)&127)|0x80, midi_file);
  fputc(n&127, midi_file);
  file_count += 2;
  }

else if (n < 0x200000)
  {
  fputc(((n>>14)&127)|0x80, midi_file);
  fputc(((n>>7)&127)|0x80, midi_file);
  fputc(n&127, midi_file);
  file_count += 3;
  }

else
  {
  fputc(((n>>21)&127)|0x80, midi_file);
  fputc(((n>>14)&127)|0x80, midi_file);
  fputc(((n>>7)&127)|0x80, midi_file);
  fputc(n&127, midi_file);
  file_count += 4;
  }
}


/*************************************************
*             Write one byte                     *
*************************************************/

static void
writebyte(int n)
{
fputc(n & 255, midi_file);
file_count++;
}



/*************************************************
*              Write one bar                     *
*************************************************/

/* The bar number is in midi_bar.

Argument:   TRUE if this is the final bar to be written
Returns:    nothing
*/

static void
writebar(BOOL is_lastbar)
{
BOOL oknbar = TRUE;
int hadrepeat = HR_NONE;
int32_t maxmoff = 0;
int stave;
uint32_t *ptc = midi_movt->miditempochanges;
int32_t this_barlength = find_barlength();
midi_event *eptr, *neptr;

TRACE("writebar %d\n", midi_bar);

/* Search the list of tempo changes for bars preceding or equal to this one and
obtain the tempo for this bar. If it's different to the currently set tempo,
output a change. */

if (ptc != NULL && midi_bar >= *ptc)
  {
  while (midi_bar >= *ptc) ptc += 2;
  if (ptc[-1] != midi_tempo)
    {
    uint32_t mpc;
    midi_tempo = ptc[-1];
    mpc = 60000000/midi_tempo;     /* Microseconds per crotchet */
    next_event->time = 0;
    next_event->seq = next_event_seq++;
    next_event->data[0] = 6u;
    next_event->data[1] = 0xffu;
    next_event->data[2] = 0x51u;
    next_event->data[3] = 0x03u;
    next_event->data[4] = (uint8_t)((mpc >> 16) & 0xffu);
    next_event->data[5] = (uint8_t)((mpc >> 8) & 0xffu);
    next_event->data[6] = (uint8_t)(mpc & 0xffu);
    next_event++;
    }
  }

/* Now scan the staves. When [notes off] appears in the input, a control is
placed at the start of each bar into which it continues, so we do not have to
keep track between bars. */

for (stave = 1; stave <= midi_movt->laststave; stave++)
  {
  BOOL noteson;   /* See above comment */
  int32_t moff;
  int midi_stave_status, midi_stave_pitch, midi_stave_velocity;
  int miditranspose, adjustlength, tremolo;

  if (mac_notbit(midi_staves, stave)) continue;

  moff = 0;
  miditranspose = midi_transpose[stave];
  adjustlength = 0;
  tremolo = -1;
  noteson = TRUE;

  /* Set up midi parameters */

  midi_stave_status = 0x90 + midi_channel[stave] - 1;
  midi_stave_pitch = midi_note[stave];
  midi_stave_velocity = ((midi_volume * stavevolume[stave] *
    midi_channel_volume[midi_channel[stave]-1])/225);

  /* Scan the bar */

  for (bstr *p = (bstr *)((midi_movt->stavetable[stave])->barindex[midi_bar]);
       p != NULL; p = p->next)
    {
    switch(p->type)
      {
      case b_reset:
      moff = 0;
      break;

      /* If a previous stave saw a repeat, hadrepeat is set to indicate
      what has been done. */

      case b_rrepeat:
      if (midi_repeats && !midi_onebar_only)
        {
        switch (hadrepeat)
          {
          case HR_PLAYON:
          break;

          case HR_REPEATED:
          goto NEXT_STAVE;

          default:
          case HR_NONE:
          if (repeat_count == 1)
            {
            hadrepeat = HR_REPEATED;
            midi_nextbar = repeat_bar;
            midi_nextbar_moff = repeat_bar_moff;
            repeat_endbar = midi_bar;
            repeat_count++;
            goto NEXT_STAVE;   /* Skip rest of bar */
            }
          else
            {
            hadrepeat = HR_PLAYON;
            if (midi_bar == repeat_endbar) repeat_count = 1;
            }
          break;
          }
        }
      break;

      case b_lrepeat:
      repeat_bar = midi_bar;
      repeat_bar_moff = moff;
      break;

      case b_nbar:
      if (moff == 0 && !midi_onebar_only && oknbar)
        {
        b_nbarstr *b = (b_nbarstr *)p;
        if (b->n == 1 && repeat_count > 1)
          {
          int second = 0;

          /* Search for a second time bar */

          for (int i = midi_bar + 1; i < midi_movt->barcount; i++)
            {
            for (bstr *pp = (bstr *)((midi_movt->stavetable[stave])->barindex[i]);
                 pp != NULL; pp = pp->next)
              {
              if (pp->type == b_nbar)
                {
                second = i;
                break;
                }
              }
            }

          if (second > 0)
            {
            midi_bar = second;
            midi_bar_moff = 0;
            midi_nextbar = midi_bar + 1;
            midi_nextbar_moff = 0;
            repeat_bar = midi_bar;
            repeat_bar_moff = 0;
            repeat_count = 1;
            writebar(is_lastbar);
            }
          return;
          }
        else oknbar = FALSE;
        }
      break;

      case b_notes:
      noteson = ((b_notesstr *)p)->value;
      break;

      case b_midichange:
        {
        b_midichangestr *change = (b_midichangestr *)p;

        miditranspose += change->transpose;
        midi_transpose[stave] = miditranspose;

        /* If the relative volume parameter occurs with a change of
        channel, it is a channel volume change. Otherwise it is a
        stave volume change. */

        if (change->volume < 128 && change->channel == 128)
          {
          stavevolume[stave] = change->volume;
          midi_stave_velocity = ((midi_volume * stavevolume[stave] *
            midi_channel_volume[midi_channel[stave]-1])/225);
          }

        /* Other changes */

        if (change->channel < 128)
          {
          midi_channel[stave] = change->channel;
          midi_stave_status = 0x90 + midi_channel[stave] - 1;
          if (change->volume < 128)
            midi_channel_volume[change->channel - 1] = change->volume;
          midi_stave_velocity = ((midi_volume * stavevolume[stave] *
            midi_channel_volume[midi_channel[stave]-1])/225);
          }

        if (change->note < 128)
          midi_stave_pitch = midi_note[stave] = change->note;

        /* A voice change must be scheduled to occur in the correct
        sequence with the notes. */

        if (change->voice < 128)
          {
          next_event->time = moff;
          next_event->seq = next_event_seq++;
          next_event->data[0] = 2;
          next_event->data[1] = 0xC0 +  midi_channel[stave] - 1;
          next_event->data[2] = change->voice;
          next_event++;
          }
        }
      break;

      case b_ornament:
        {
        b_ornamentstr *orn = (b_ornamentstr *)p;
        if (orn->ornament == or_trem1 ||
            orn->ornament == or_trem2)
          tremolo = orn->ornament;
        }
      break;

      case b_note:
        {
        b_notestr *note = (b_notestr *)p;
        BOOL thisnotetied = FALSE;
        int length = note->length;
        int nstart = 0;
        int scrub = 1;
        int tiebarcount = 1;
        int pitchcount = 0;
        int pitchlist[20];
        int pitchlen[20];
        int pitchstart[20];

        oknbar = FALSE;

        if (length == 0)
          {
          length = (len_crotchet*midi_tempo)/(60*10); /* 1/10 sec */
          adjustlength += length;
          }
        else
          {
          length -= adjustlength;
          adjustlength = 0;
          }

        /* nf_noplay is set when a note has already been played, because of
        a previous tie, which might have been in a previous bar. */

        if ((noteson || main_midifornotesoff) && moff >= midi_bar_moff &&
            note->spitch != 0 && (note->flags & nf_noplay) == 0)
          {
          /* Get a list of pitches in a chord, and leave the general
          pointer p at the final note. */

          do
            {
            pitchlist[pitchcount] = note->abspitch;
            pitchlen[pitchcount] = length;
            pitchstart[pitchcount++] = nstart;
            p = (bstr *)note;
            note = (b_notestr *)note->next;
            }
          while (note->type == b_chord);

          /* Advance to start of following note */

          nstart += length;

          /* If the note is followed by a tie, find the next note or chord on
          the stave. If any of its notes have the same pitch as any of those in
          the list, extend their playing times. If there are any new notes, add
          them to the list, with a later starting time. We have to do this
          because all the notes we are accumulating will be output at the end
          of this bar. Set the noplay flag in the next notes, to stop them
          playing again later. Continue for multiple ties. */

          while (note->type == b_tie)
            {
            int nlength;
            note = misc_nextnote(note);
            if (note == NULL &&
                midi_bar + tiebarcount <= (uint32_t)(midi_movt->barcount))
              {
              note = (b_notestr *)((midi_movt->stavetable)[stave])->
                barindex[midi_bar + tiebarcount++];
              if (note != NULL && note->type != b_note)
                note = misc_nextnote(note);
              }
            if (note == NULL) break;

            nlength = note->length;
            do
              {
              int i;
              for (i = 0; i < pitchcount; i++)
                {
                if (pitchlist[i] == note->abspitch)
                  {
                  pitchlen[i] += note->length;
                  thisnotetied = TRUE;
                  note->flags |= nf_noplay;
                  break;
                  }
                }
              if (i >= pitchcount)
                {
                pitchlist[pitchcount] = note->abspitch;
                pitchlen[pitchcount] = nlength;
                note->flags |= nf_noplay;
                pitchstart[pitchcount++] = nstart;
                }

              note = (b_notestr *)note->next;
              }
            while (note->type == b_chord);
            nstart += nlength;
            }

          /* Handle some common scrubbing */

          if (tremolo > 0 && !thisnotetied)
            {
            int ttype = (tremolo == or_trem1)? 1 : 2;
            switch (length)
              {
              case len_crotchet:       scrub = 2*ttype; break;
              case (len_crotchet*3)/2: scrub = 3*ttype; break;
              case len_minim:          scrub = 4*ttype; break;
              case (len_minim*3)/2:    scrub = 6*ttype; break;
              }
            }

          /* The value of "scrub" is 1 for ordinary, non-tremolo notes. */

          for (int scrubcount = 0; scrubcount < scrub; scrubcount++)
            {
            int pc = pitchcount;

            /* For each required pitch, set up the events to make a sound.
            The lengths may be different because of tied/non-tied notes in
            chords, but these can only happen when not scrubbing.

            Note: PMW operates in quartertones, with middle C at 96, whereas
            MIDI operates in semitones with middle C at 60. Thus, to get a MIDI
            pitch from PMW we divide by two and add 12. The MIDI transposition
            value, however, is in semitones. */

            while (--pc >= 0)
              {
              int pitch = (midi_stave_pitch != 0)? midi_stave_pitch :
                pitchlist[pc]/2 + 12 + miditranspose;
              int start = moff - midi_bar_moff + pitchstart[pc] +
                scrubcount * (pitchlen[pc]/scrub);

              if (pitch < 0 || pitch > 127)
                {
                char buff[24];
                sprintf(buff, "%s", sfb(midi_movt->barvector[midi_bar]));
                error(ERR172, pitch, buff, stave);
                }

              /* We have to schedule a note on and a note off event. Use
              note on with zero velocity for note off, because that means
              running status can be used. */

              else
                {
                next_event->time = start;
                next_event->seq = next_event_seq++;
                next_event->data[0] = 3;
                next_event->data[1] = midi_stave_status;
                next_event->data[2] = pitch;
                next_event->data[3] = midi_stave_velocity;
                next_event++;

                next_event->time = start + (pitchlen[pc]/scrub);
                next_event->seq = next_event_seq++;
                next_event->data[0] = 3;
                next_event->data[1] = midi_stave_status;
                next_event->data[2] = pitch;
                next_event->data[3] = 0;
                next_event++;
                }
              }
            }
          }

        stavetie[stave] = thisnotetied;
        moff += length;
        }

      tremolo = -1;
      break;
      }  /* End switch on bar item */
    }    /* End of bar scan */

  NEXT_STAVE:
  if (moff > maxmoff) maxmoff = moff;
  }

/* Sort and output the items we've created, along with any events left over
from the previous bar (ending tied notes). We relativize the times, and make
use of running status. Stop when we hit either the end, or an event that is
past the end of the bar, unless this is the last bar being played. */

qsort(events, next_event - events, sizeof(midi_event), cf);

for (eptr = events; eptr < next_event; eptr++)
  {
  if (!is_lastbar && eptr->time > this_barlength) break;

  writevar(mac_muldiv(eptr->time - last_written_time, 24, len_crotchet));
  last_written_time = eptr->time;

  if ((eptr->data[1] & 0xf0) == 0x90)
    {
    if (eptr->data[1] != running_status)
      {
      writebyte(eptr->data[1]);
      running_status = eptr->data[1];
      }
    writebyte(eptr->data[2]);
    writebyte(eptr->data[3]);
    }
  else
    {
    int i;
    running_status = 0;
    for (i = 1; i <= eptr->data[0]; i++) writebyte(eptr->data[i]);
    }
  }

/* If we haven't written all the items (some notes are tied over the barline),
shift down the remaining events, and re-relativize them. */

neptr = events;
next_event_seq = 0;
for (; eptr < next_event; eptr++, neptr++)
  {
  *neptr = *eptr;
  neptr->time -= this_barlength;
  next_event_seq = neptr->seq + 1;
  }
next_event = neptr;

/* Set time for start of next bar */

last_written_time -= (maxmoff - midi_bar_moff);
}



/*************************************************
*     Convert logical bar number to absolute     *
*************************************************/

/* There is no fast index for this, but it's probably not a huge cost to search
the absolute-to-logical index.

Argument:  a logical bar number (16-bit number, 16-bit fraction)
Returns:   an absolute bar number or UINT32_MAX if not found
*/

static uint32_t
absbar(uint32_t lb)
{
for (int i = 0; i < midi_movt->barcount; i++)
  if (midi_movt->barvector[i] == lb) return i;
return UINT32_MAX;
}



/*************************************************
*                  Write MIDI file               *
*************************************************/

/* This is the only external entry to this set of functions. The data is all in
memory and global variables. Writing a MIDI file is triggered by the use of the
-midi command line option, which sets midi_filename non-NULL.

Arguments:  none
Returns:    nothing
*/

void
midi_write(void)
{
int32_t mpc;

TRACE("midi_write() movement %d\n", midi_movement);

midi_movt = movements[midi_movement - 1];
if (midi_movt->barcount < 1)
  {
  error(ERR160, midi_movt->number);
  return;
  }
midi_tempo = midi_movt->miditempo;   /* Default tempo */
mpc = 60000000/midi_tempo;           /* Microseconds per crotchet */

/* Convert any tempo changes from logical bar numbers as specified in a header
directive into absolute bar numbers. */

if (midi_movt->miditempochanges != NULL)
  for (uint32_t *p = midi_movt->miditempochanges; *p != UINT32_MAX; p += 2)
    {
    uint32_t a = absbar(*p);
    if (a == UINT32_MAX)
      {
      error(ERR159, "tempo change", sfb(*p), midi_movt->number);
      return;
      }
    *p = a;
    }

/* If the starting bar number is unset, set it to the first absolute bar in
this movement. Otherwise, convert the logical bar number to an absolute bar. */

if (midi_startbar == UINT32_MAX) midi_startbar = 0; else
  {
  uint32_t a = absbar(midi_startbar);
  if (a == UINT32_MAX)
    {
    error(ERR159, "start", sfb(midi_startbar), midi_movt->number);
    return;
    }
  midi_startbar = a;
  }

/* Similarly for the ending bar number. */

if (midi_endbar == UINT32_MAX) midi_endbar = midi_movt->barcount - 1; else
  {
  uint32_t a = absbar(midi_endbar);
  if (a == UINT32_MAX)
    {
    error(ERR159, "end", sfb(midi_endbar), midi_movt->number);
    return;
    }
  midi_endbar = a;
  }

midi_onebar_only = (midi_startbar == midi_endbar);

/* Stave selection is the movement's stave selection. Currently there's no way
of changing this. */

midi_staves = midi_movt->select_staves;

/* Initialize the tie information */

for (int stave = 1; stave <= midi_movt->laststave; stave++)
  stavetie[stave] = FALSE;

/* Miscellaneous stuff */

last_written_time = 0;
running_status = 0;

/* Get store in which to hold a bar's events before sorting. For the
first bar, it is empty at the start. */

events = mem_get_independent(sizeof(midi_event) * 1000);
next_event = events;
next_event_seq = 0;

/* Set up the initial per-stave vectors */

memcpy(midi_channel, midi_movt->midichannel, sizeof(midi_channel));
memcpy(midi_channel_volume, midi_movt->midistavevolume, sizeof(midi_channel_volume));
memcpy(midi_note, midi_movt->midinote, sizeof(midi_note));

/* Open the output file */

midi_file = Ufopen(midi_filename, "w");
if (midi_file == NULL) error(ERR23, midi_filename, strerror(errno));  /* Hard */

/* Write header chunk */

fprintf(midi_file, "MThd");
write32(6);                     /* length */
write16(0);                     /* format */
write16(1);                     /* number of tracks */
write16(24);                    /* ticks per crotchet (MIDI standard) */

/* Now write the track, leaving space for the length */

fprintf(midi_file, "MTrk");
write32(0);
file_count = 0;                 /* For computing the length */

/* Output any user-supplied initialization. The user's data is a plain MIDI
stream, without any time deltas. Ensure that each event is set to occur at the
beginning (time zero). */

if (midi_movt->midistart != NULL)
  {
  for (int i = 1; i <= midi_movt->midistart[0]; i++)
    {
    if ((midi_movt->midistart[i] & 0x80) != 0) writebyte(0);
    writebyte(midi_movt->midistart[i]);
    }
  }

/* Default tempo - can change for specific bars */

writebyte(0);
writebyte(0xff);
writebyte(0x51);
writebyte(0x03);

writebyte(mpc >> 16);
writebyte(mpc >> 8);
writebyte(mpc);

/* Assign MIDI voices to MIDI channels if required. */

for (int i = 1; i <= MIDI_MAXCHANNEL; i++)
  {
  if (midi_movt->midivoice[i-1] < 128)
    {
    writebyte(0);               /* delta time */
    writebyte(0xC0 + i - 1);
    writebyte(midi_movt->midivoice[i-1]);
    }
  }

/* Initialize the per-stave relative volume & transpose vectors */

memcpy(stavevolume, midi_movt->midistavevolume, sizeof(stavevolume));
memcpy(midi_transpose, midi_movt->miditranspose, sizeof(midi_transpose));

/* If not starting at the beginning, we must scan through the stave data
for all preceding bars, in order to pick up any in-line MIDI changes. */

for (midi_bar = 0; midi_bar < midi_startbar; midi_bar++)
  {
  for (int stave = 1; stave <= midi_movt->laststave; stave++)
    {
    if (mac_notbit(midi_staves, stave)) continue;

    for (bstr *p = (bstr *)((midi_movt->stavetable[stave])->barindex[midi_bar]);
         p != NULL; p = p->next)
      {
      b_midichangestr *change;

      if (p->type != b_midichange) continue;
      change = (b_midichangestr *)p;
      midi_transpose[stave] += change->transpose;

      /* If the relative volume parameter occurs with a change of
      channel, it is a channel volume change. Otherwise it is a
      stave volume change. */

      if (change->volume < 128 && change->channel == 128)
        stavevolume[stave] = change->volume;

      /* Other changes */

      if (change->channel < 128)
        {
        midi_channel[stave] = change->channel;
        if (change->volume < 128)
          midi_channel_volume[change->channel - 1] = change->volume;
        }

      if (change->note < 128) midi_note[stave] = change->note;

      if (change->voice < 128)
        {
        writebyte(0);   /* delta time */
        writebyte(0xC0 + midi_channel[stave] - 1);
        writebyte(change->voice);
        }
      }
    }
  }

/* Now write the bars */

repeat_bar = midi_bar;
repeat_bar_moff = 0;
repeat_endbar = -1;
repeat_count = 1;

for (midi_bar = midi_startbar; midi_bar <= midi_endbar;)
  {
  midi_nextbar = midi_bar + 1;
  midi_nextbar_moff = 0;
  writebar(midi_bar == midi_endbar);
  midi_bar = midi_nextbar;
  midi_bar_moff = midi_nextbar_moff;
  }

/* Mark the end of the track, and fill in its length before closing the file */

writebyte(0);
writebyte(0xff);
writebyte(0x2f);
writebyte(0);

fseek(midi_file, 18, SEEK_SET);
write32(file_count);

fclose(midi_file);
}

/* End of midi.c */
