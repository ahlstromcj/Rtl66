/*
 *  This file is part of rtl66.
 *
 *  rtl66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  rtl66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with rtl66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          track.cpp
 *
 *  This module declares a class for holding and managing MIDI data.
 *
 * \library       rtl66
 * \author        Chris Ahlstrom
 * \date          2015-10-10
 * \updates       2024-05-14
 * \license       GNU GPLv2 or above
 *
 *  This class is important when writing the MIDI and sequencer data out to a
 *  MIDI file.  The data handled here are specific to a single
 *  sequence/pattern/track.
 *
 *  Provides tags used by the midi::file class to control the reading and
 *  writing of the extra "proprietary" information stored in a Seq24 MIDI
 *  file.  Some of the information is stored with each track (and in the
 *  track-derived classes), and some is stored in the proprietary
 *  header.
 *
 *  Track (sequencer-specific) data:
 *
\verbatim
        midibus
        midichannel
        timesig
        triggers (deprecated)
        triggers_ex (deprecated)
        trig_transpose (triggers_ex plus!)
        musickey (can be in footer, as well)
        musicscale (ditto)
        backsequence (ditto)
        transpose
        seq_color (performance colors for a sequence)
        seq_edit_mode
        seq_loopcount
\endverbatim
 *
 *  Note that seq_color in Seq66 is stored per sequence, and only if
 *  not PaletteColor::NONE.  In Kepler34, all 1024 sequence colors are stored
 *  in a "proprietrary", whole-song section, whether used or not.  There are
 *  also numeric differences in many of these "feature" constants.
 *
 * Footer ("proprietary", whole-song) data:
 *
\verbatim
        midictrl
        midiclocks
        notes
        bpmtag (beats per minute)
        mutegroups
        perf_bp_mes (perfedit's beats-per-measure setting)
        perf_bw     (perfedit's beat-width setting)
        tempo_map   (seq32's tempo map)
        reserved_1 and reserved_2
        tempo_track (holds the song's particular tempo track)
        seq_edit_mode (a potential future feature from Kepler34).
\endverbatim
 *
 *  Also see the PDF file in the following project for more information about
 *  the "proprietary" data:
 *
 *      https://github.com/ahlstromcj/rtl66-doc.git
 *
 *  Note that the track data is read from the MIDI file, but not written
 *  directly to the MIDI file.  Instead, it is stored in the MIDI container as
 *  sequences are edited to used these "sequencer-specific" features.
 *  Also note that triggers has been replaced by triggers_ex as the code
 *  that marks the triggers stored with a sequence. And trig_transpose
 *  extends it even further with a byte-value for transposing a trigger.
 *
 *  As an extension, we can also grab the key, scale, and background sequence
 *  value selected in a sequence and write these values as track data, where
 *  they can be read in and applied to a specific sequence, when the seqedit
 *  object is created.  These values would not be stored in the legacy format.
 *
 *  Something like this could be done in the "user" configuration file, but
 *  then the key and scale would apply to all songs.  We don't want that.
 *
 *  We could also add snap and note-length to the per-song defaults, but
 *  the "user" configuration file seems like a better place to store these
 *  preferences.
 *
 * \note
 *      -   The value transpose value is from Stazed's seq32 project.
 *          The code to support this option is turned on permanently.
 *          There are additional values from Stazed/seq32, not yet used.
 *      -   The values below are compatible with Seq32, but they are not
 *          compatible with Kepler34.  It uses 0x24240011 and 0x24240012 for
 *          difference purposes.  See the asterisks below.
 *      -   Note the new "gap" values.  We just noticed this gap, which has
 *          existed between 0x24240009 and 0x24240010 since Seq24!
 */

#include "cfg/scales.hpp"               /* rtl66::scales enum class         */
#include "cfg/settings.hpp"             /* rtl66::rc()                      */
#include "midi/track.hpp"               /* rtl66::track ABC      */
#include "play/performer.hpp"           /* rtl66::performer master class    */
#include "play/seq.hpp"                 /* rtl66::seq                       */

namespace rtl66
{

/**
 *  Fills in the few members of this class.
 *
 * \param seq
 *      Provides a reference to the sequence/track for which this container
 *      holds MIDI data.
 */

track::track (sequence & seq) :
    m_bytes             (),
    m_sequence          (seq),                          /* seq() accessor   */
    m_position_for_get  (0)
{
    // Empty body
}

/**
 *
 *  Fills this list with an exportable track.  Following stazed, we're
 *  consolidate the tracks at the beginning of the song, replacing the actual
 *  track number with a counter that is incremented only if the track was
 *  exportable.  Note that this loop is kind of an elaboration of what goes on in
 *  the track :: fill() function for normal Seq66 file writing.
 *
 *  Exportability ensures that the sequence pointer is valid.  This function adds
 *  all triggered events.
 *
 *  For each trigger in the sequence, add events to the list below; fill
 *  one-by-one in order, creating a single long sequence.  Then set a single
 *  trigger for the big sequence: start at zero, end at last trigger end with
 *  snap.  We're going to reference (not copy) the triggers now, since the
 *  write_song() function is now locked.
 *
 *  The we adjust the sequence length to snap to the nearest measure past the
 *  end.  We fill the MIDI container with trigger "events", and then the
 *  container's bytes are written.
 *
 *  tick_end() isn't quite a trigger length, off by 1.  Subtracting tick_start()
 *  can really screw it up.
 */

bool
track::song_fill_track (int track, bool standalone)
{
    bool result = seq().is_exportable();
    if (result)
    {
        clear();
        if (standalone)
        {
            fill_seq_number(track);
            fill_seq_name(seq().name());

#if defined USE_FILL_TIME_SIG_AND_TEMPO             /* issue #141   */
            if (track == rc().tempo_track_number)
            {
                seq().events().scan_meta_events();
                fill_time_sig_and_tempo
                (
                    p, seq().events().has_time_signature(),
                    seq().events().has_tempo()
                );
            }
#endif
        }

        midi::pulse last_ts = 0;
        const auto & trigs = seq().get_triggers();
        for (auto & t : trigs)
            last_ts = song_fill_seq_event(t, last_ts);

        const trigger & ender = trigs.back();
        midi::pulse seqend = ender.tick_end();
        midi::pulse measticks = seq().measures_to_ticks();
        if (measticks > 0)
        {
            midi::pulse remainder = seqend % measticks;
            if (remainder != (measticks - 1))
                seqend += measticks - remainder - 1;
        }
        song_fill_seq_trigger(ender, seqend, last_ts);
    }
    return result;
}

void
track::put_meta
(
    midi::byte metavalue,
    int datalen,
    midi::pulse deltatime
)
{
    add_varinum(midi::ulong(deltatime));
    put(EVENT_MIDI_META);                           /* 0xFF meta marker     */
    put(metavalue);                                 /* meta marker          */
    add_varinum(midi::ulong(datalen));
}

void
track::put_seqspec (midi::ulong spec, int datalen)
{
    datalen += 4;                                   /* size of 0x242400nn   */
    put_meta(EVENT_META_SEQSPEC, datalen, 0);
    add_long(spec);                                 /* e.g. c_midibus       */
}

/**
 *  This function masks off the lower 8 bits of the long parameter, then
 *  shifts it right 7, and, if there are still set bits, it encodes it into
 *  the buffer in reverse order.  This function "replaces"
 *  sequence::add_list_var().  It is almost identical to midifile ::
 *  write_varinum().
 *
 * \param v
 *      The data value to be added to the current event in the MIDI container.
 */

void
track::add_varinum (midi::ulong v)
{
    midi::ulong buffer = v & 0x7F;                /* mask off a no-sign byte   */
    while (v >>= 7)                            /* shift right 7 bits, test  */
    {
        buffer <<= 8;                           /* move LSB bits to MSB     */
        buffer |= ((v & 0x7F) | 0x80);          /* add LSB and set bit 7    */
    }
    for (;;)
    {
        put(midi::byte(buffer) & 0xFF);           /* add the LSB              */
        if (buffer & 0x80)                      /* if bit 7 set             */
            buffer >>= 8;                       /* get next MSB             */
        else
            break;
    }
}

/**
 *  Adds a long value (a MIDI pulse/tick value) to the container.
 *
 *  What is the difference between this function and add_list_var()?
 *  This function "replaces" sequence::add_long_list().
 *  This was a <i> global </i> internal function called addLongList().
 *  Let's at least make it a private member now, and hew to the naming
 *  conventions of this class.
 *
 * \param x
 *      Provides the timestamp (pulse value) to be added to the container.
 */

void
track::add_long (midi::ulong x)
{
    put((x & 0xFF000000) >> 24);
    put((x & 0x00FF0000) >> 16);
    put((x & 0x0000FF00) >> 8);
    put((x & 0x000000FF));
}

/**
 *  Adds a short value (two bytes) to the container.
 *
 * \param x
 *      Provides the timestamp (pulse value) to be added to the container.
 */

void
track::add_short (midi::ushort x)
{
    put((x & 0x0000FF00) >> 8);
    put((x & 0x000000FF));
}

/**
 *  Adds an event to the container.  It handles regular MIDI events separately
 *  from "extended" (our term) MIDI events (SysEx and Meta events).
 *
 *  For normal MIDI events, if the sequence's MIDI channel is null_channel()
 *  == 0x80, then it is the copy of an SMF 0 sequence that the midi_splitter
 *  created.  We want to be able to save it along with the other tracks, but
 *  won't be able to read it back if all the channels are bad.  So we just use
 *  the channel from the event.
 *
 *  SysEx and Meta events are detected and passed to the new add_ex_event()
 *  function for proper dumping.
 *
 * \param e
 *      Provides the event to be added to the container.
 *
 * \param deltatime
 *      Provides the time-location of the event.
 */

void
track::add_event (const event & e, midi::pulse deltatime)
{
    if (e.is_ex_data())
    {
        add_ex_event(e, deltatime);
    }
    else
    {
        midi::byte d0 = e.data(0);
        midi::byte d1 = e.data(1);
        midi::byte channel = seq().seq_midi_channel();
        midi::byte st = e.get_status();
        add_varinum(midi::ulong(deltatime));        /* encode delta_time    */
        if (seq().free_channel() || is_null_channel(channel))
            put(st | e.channel());                  /* channel from event   */
        else
            put(st | channel);                      /* the sequence channel */

        if (e.has_channel())
        {
            switch (event::mask_status(st))                     /* 0xF0 */
            {
            case EVENT_NOTE_OFF:                                /* 0x80 */
            case EVENT_NOTE_ON:                                 /* 0x90 */
            case EVENT_AFTERTOUCH:                              /* 0xA0 */
            case EVENT_CONTROL_CHANGE:                          /* 0xB0 */
            case EVENT_PITCH_WHEEL:                             /* 0xE0 */
                put(d0);
                put(d1);
                break;

            case EVENT_PROGRAM_CHANGE:                          /* 0xC0 */
            case EVENT_CHANNEL_PRESSURE:                        /* 0xD0 */
                put(d0);
                break;

            default:
                break;
            }
        }
    }
}

/**
 *  Adds the bytes of a SysEx or Meta MIDI event.
 *
 * \param e
 *      Provides the MIDI event to add.  The caller must ensure that this is
 *      either SysEx or Meta event, using the event::is_ex_data() function.
 *
 * \param deltatime
 *      Provides the time of the event, which is encoded into the event.
 */

void
track::add_ex_event (const event & e, midi::pulse deltatime)
{
    add_varinum(midi::ulong(deltatime));        /* encode delta_time        */
    put(e.get_status());                        /* indicates SysEx/Meta     */
    if (e.is_meta())
        put(e.channel());                       /* indicates meta type      */

    int count = e.sysex_size();                 /* applies for meta, too    */
    add_varinum(midi::ulong(count));
    for (int i = 0; i < count; ++i)
        put(e.get_message()[i]);
}

/**
 *  Fills in the sequence number.  Writes 0xFF 0x00 0x02 ss ss, where ss ss is
 *  the variable-length value for the sequence number.  This function is used
 *  in the new midifile::write_song() function, which should be ready to go by
 *  the time you're reading this.  Compare this function to the beginning of
 *  track::fill().
 *
 * \warning
 *      This is an optional event, which must occur only at the start of a
 *      track, before any non-zero delta-time.  For Format 2 MIDI files, this
 *      is used to identify each track. If omitted, the sequences are numbered
 *      sequentially in the order the tracks appear.  For Format 1 files, this
 *      event should occur on the first track only.  So, are we writing a
 *      hybrid format?
 *
 * \param seq
 *      The sequence/track number to write.
 */

void
track::fill_seq_number (int seq)
{
    put_meta(EVENT_META_SEQ_NUMBER, 2);             /* 0x00, 2 bytes long   */
    add_short(midi::ushort(seq));
}

/**
 *  Fills in the sequence name.  Writes 0xFF 0x03, and then the track name.
 *  This function is used in the new midifile::write_song() function, which
 *  should be ready to go by the time you're reading this.
 *
 *  Compare this function to the beginning of track::fill().
 *
 * \param name
 *      The sequence/track name to set.  We could get this item from
 *      seq(), but the parameter allows the flexibility to change the
 *      name.
 */

void
track::fill_seq_name (const std::string & name)
{
    int len = int(name.length());
    put_meta(EVENT_META_TRACK_NAME, len);           /* 0x03, len bytes long */
    for (int i = 0; i < len; ++i)
        put(midi::byte(name[i]));
}

/*
 * Last, but certainly not least, write the end-of-track meta-event.
 *
 * \param deltatime
 *      The MIDI delta time to write before the meta-event itself.
 */

void
track::fill_meta_track_end (midi::pulse deltatime)
{
    put_meta(EVENT_META_END_OF_TRACK, 0, deltatime);    /* no data to add   */
}

#if defined SEQ66_USE_FILL_TIME_SIG_AND_TEMPO

/**
 *  Combines the two functions fill_tempo() and fill_time_signature().  This
 *  function is called only for track 0.  And it only puts out the events if
 *  the track does not contain tempo or time-signature events; in that case,
 *  it needs to grab the global values from the performance object and put
 *  them out.
 *
 * \param p
 *      The performance object that holds the time signature and tempo values.
 *
 * \param has_time_sig
 *      Indicates whether or not the current track (usually track 0) has a
 *      time signature event.  If so, then we do not need to fill in the
 *      global time signature value.
 *
 * \param has_tempo
 *      Indicates whether or not the current track (track 0) has a tempo
 *      event.  If so, then we do not need to fill in the global tempo value.
 */

void
track::fill_time_sig_and_tempo
(
    const performer & p,
    bool has_time_sig,
    bool has_tempo
)
{
    if (! has_tempo)
        fill_tempo(p);

    if (! has_time_sig)
        fill_time_sig(p);
}

/**
 *  Fill in the time-signature information.  This function is used only for
 *  the first track, and only if no such event is in the track data.
 *
 *  We now make sure that the proper values are part of the performer object for
 *  usage in this particular track.  For export, we cannot guarantee that the
 *  first (0th) track/sequence is exportable.
 *
 * \param p
 *      Provides the performance object from which we get some global MIDI
 *      parameters.
 */

void
track::fill_time_sig (const performer & p)
{
    int beatwidth = p.get_beat_width();
    int bpb = p.get_beats_per_bar();;
    int cpm = p.clocks_per_metronome();
    int get32pq = p.get_32nds_per_quarter();
    int bw = log2_power_of_2(beatwidth);
    put_meta(EVENT_META_TIME_SIGNATURE, 4);         /* 0x58 marker, 4 bytes */
    put(bpb);
    put(bw);
    put(cpm);
    put(get32pq);
}

/**
 *  Fill in the tempo information.  This function is used only for the first
 *  track, and only if no such event is int the track data.
 *
 *  We now make sure that the proper values are part of the performer object for
 *  usage in this particular track.  For export, we cannot guarantee that the
 *  first (0th) track/sequence is exportable.
 *
 * \change ca 2017-08-15
 *      Fixed issue #103, was writing tempo bytes in the wrong order here.
 *      Accidentally committed along with fruity changes, sigh, so go back a
 *      couple of commits to see the changes.
 *
 * \param p
 *      Provides the performance object from which we get some global MIDI
 *      parameters.
 */

void
track::fill_tempo (const performer & p)
{
    midi::byte t[4];                                  /* hold tempo bytes     */
    int usperqn = p.us_per_quarter_note();
    tempo_us_to_bytes(t, usperqn);
    put_meta(EVENT_META_SET_TEMPO, 3);              /* 0x51, 3 bytes long   */
    put(t[0]);                                      /* NOT 2, 1, 0!         */
    put(t[1]);
    put(t[2]);
}

#endif  // SEQ66_USE_FILL_TIME_SIG_AND_TEMPO

/**
 *  Fills in the Seq66-specific information for the current sequence:
 *  The MIDI buss number, the time-signature, and the MIDI channel.  Then, if
 *  we're not using the legacy output format, we add the "events" for the
 *  musical key, musical scale, and the background sequence for the current
 *  sequence. Finally, if tranpose support has been compiled into the program,
 *  we add that information as well.
 */

void
track::fill_proprietary ()
{
    put_seqspec(c_midibus, 1);
    put(seq().seq_midi_bus());                 /* MIDI buss number     */

    put_seqspec(c_timesig, 2);
    put(seq().get_beats_per_bar());
    put(seq().get_beat_width());

    put_seqspec(c_midichannel, 1);
    put(seq().seq_midi_channel());             /* 0 to 15 or 0x80      */
    if (! usr().global_seq_feature())
    {
        /**
         * New feature: save more sequence-specific values, if not saved
         * globally.  We use a single byte for the key and scale, and a long
         * for the background sequence.  We save these values only if they are
         * different from the defaults; in most cases they will have been left
         * alone by the user.  We save per-sequence values here only if the
         * global-background-sequence feature is not in force.
         */

        if (seq().musical_key() != c_key_of_C)
        {
            put_seqspec(c_musickey, 1);
            put(seq().musical_key());
        }
        if (seq().musical_scale() != c_scales_off)
        {
            put_seqspec(c_musicscale, 1);
            put(seq().musical_scale());
        }
        if (seq::valid(seq().background_sequence()))
        {
            put_seqspec(c_backsequence, 4);
            add_long(seq().background_sequence());
        }
    }

    /**
     *  Generally only drum patterns will not be transposable.
     */

    bool transpose = seq().transposable();
    put_seqspec(c_transpose, 1);                                /* byte     */
    put(midi::byte(transpose));
    if (seq().color() != c_seq_color_none)
    {
        put_seqspec(c_seq_color, 1);                            /* byte     */
        put(midi::byte(seq().color()));
    }
#if defined SEQ66_SEQUENCE_EDIT_MODE                            /* useful?  */
    if (seq().edit_mode() != sequence::editmode::note)
    {
        put_seqspec(c_seq_edit_mode, 1);                        /* byte     */
        put(seq().edit_mode_byte());
    }
#endif
    if (seq().loop_count_max() > 0)
    {
        put_seqspec(c_seq_loopcount, 2);                        /* short    */
        add_short(midi::ushort(seq().loop_count_max()));
    }
}

/**
 *  Fills in sequence events based on the trigger and events in the sequence
 *  associated with this track.
 *
 *  This calculation needs investigation.  The number of times the pattern is
 *  played is given by how many pattern lengths fit in the trigger length.
 *  But the commented calculation adds to the value of 1 already assigned.
 *  And what about triggers that are somehow of 0 size?  Let's try a different
 *  calculation, currently the same.
 *
 *      int times_played = 1;
 *      times_played += (trig.tick_end() - trig.tick_start()) / len;
 *
 * \param trig
 *      The current trigger to be processed.
 *
 * \param prev_timestamp
 *      The time-stamp of the previous event.
 *
 * \return
 *      The next time-stamp value is returned.
 */

midi::pulse
track::song_fill_seq_event
(
   const trigger & trig,
   midi::pulse prev_timestamp
)
{
    midi::pulse len = seq().get_length();
    midi::pulse trig_offset = trig.offset() % len;
    midi::pulse start_offset = trig.tick_start() % len;
    midi::pulse time_offset = trig.tick_start() + trig_offset - start_offset;
    int times_played = 1 + (trig.length() - 1) / len;
    if (trig_offset > start_offset)                 /* offset len too far   */
        time_offset -= len;

    int note_is_used[c_notes_count];
    for (int i = 0; i < c_notes_count; ++i)
        note_is_used[i] = 0;                        /* initialize to off    */

    for (int p = 0; p <= times_played; ++p, time_offset += len)
    {
        midi::pulse delta_time = 0;
        for (auto e : seq().events())               /* use a copy of event  */
        {
            midi::pulse timestamp = e.timestamp() + time_offset;
            if (timestamp >= trig.tick_start())     /* at/after trigger     */
            {
                /*
                 * Save the note; eliminate Note Off if Note On is unused.
                 */

                if (e.is_note())                    /* includes aftertouch  */
                {
                    midi::byte note = e.get_note();
                    if (trig.transposed())
                        e.transpose_note(trig.transpose());

                    if (e.is_note_on())
                    {
                        if (timestamp <= trig.tick_end())
                            ++note_is_used[note];   /* count the note       */
                        else
                            continue;               /* skip                 */
                    }
                    else if (e.is_note_off())
                    {
                        if (note_is_used[note] > 0)
                        {
                            /*
                             * We have a Note On, and if past the end of trigger,
                             * use the trigger end.
                             */

                            --note_is_used[note];   /* turn off the note    */
                            if (timestamp > trig.tick_end())
                                timestamp = trig.tick_end();
                        }
                        else
                            continue;               /* if no Note On, skip  */
                    }
                }
            }
            else
                continue;                           /* before trigger, skip */

            /*
             * If the event is past the trigger end, for non-notes, skip.
             */

            if (timestamp >= trig.tick_end())       /* event past trigger   */
            {
                if (! e.is_note())                  /* (also aftertouch)    */
                    continue;                       /* drop the event       */
            }

            delta_time = timestamp - prev_timestamp;
            prev_timestamp = timestamp;
            add_event(e, delta_time);               /* does it sort???      */
        }
    }
    return prev_timestamp;
}

/**
 *  Fills in one trigger for the sequence, for a song-performance export.
 *  There will be only one trigger, covering the beginning to the end of the
 *  fully unlooped track. Therefore, we use the older c_triggers_ex SeqSpec,
 *  which saves a byte, while indicating the sequence has already been
 *  transposed.
 *
 * Using all the trigger values seems to be the same as these values, but
 * we're basically zeroing the start and offset values to make "one big
 * trigger" for the whole pattern.
 *
 *      add_long(trig.tick_start());
 *      add_long(trig.tick_end());
 *      add_long(trig.offset());
 *
 * \param trig
 *      The current trigger to be processed.
 *
 * \param length
 *      Provides the total length of the sequence.
 *
 * \param prev_timestamp
 *      The time-stamp of the previous event, which is actually the first
 *      event.
 */

void
track::song_fill_seq_trigger
(
    const trigger & trig,
    midi::pulse length,
    midi::pulse prev_timestamp
)
{
    put_seqspec(c_triggers_ex, trigger::datasize(c_triggers_ex));
    add_long(0);                                /* start tick (see banner)  */
    add_long(trig.tick_end());                  /* the ending tick          */
    add_long(0);                                /* offset is done in event  */
    fill_proprietary();
    fill_meta_track_end(length - prev_timestamp);           /* delta time   */
}

/**
 *  This function fills the given track (sequence) with MIDI data from the
 *  current sequence, preparatory to writing it to a file.  Note that some of
 *  the events might not come out in the same order they were stored in (we
 *  see that with program-change events).  This function replaces
 *  sequence::fill_list().
 *
 *  Now, for sequence 0, an alternate format for writing the sequencer number
 *  chunk is "FF 00 00".  But that format can only occur in the first track,
 *  and the rest of the tracks then don't need a sequence number, since it is
 *  assumed to increment.  This application doesn't use that shortcut.
 *
 *  We have noticed differences in saving files in sets=4x8 versus sets=8x8,
 *  and pre-sorting the event list gets rid of some of the differences, except
 *  for the last, multi-line SeqSpec.  Some event-reordering still seems to
 *  occur, though.
 *
 * Stazed:
 *
 *      The "stazed" (seq32) code implements a function like this one
 *      using a function sequence::fill_proprietary_list() that we
 *      don't need for our implementation... it is part of our
 *      track::fill() function.
 *
 * Triggers:
 *
 *      Triggers are added by first calling add_varinum(0), which is needed
 *      because why?
 *
 *      Then 0xFF 0x7F is written, followed by the length value, which is the
 *      number of triggers at 3 long integers per trigger, plus the 4-byte
 *      code for triggers, c_triggers_ex = 0x24240008.
 *
 *      However, we're now extending triggers (c_trig_transpose = 0x24240020)
 *      to include a transposition byte which allows up to 5 octaves of
 *      tranposition either way, as a way to re-use patterns.  Inspired by
 *      Kraftwerk's "Europe Endless" background sequence, with patterns being
 *      shifted up and down in pitch.
 *
 * Meta and SysEx Events:
 *
 *      These events can now be detected and added to the list of bytes to
 *      dump.  However, historically Seq24 has forced Time Signature and Set
 *      Tempo events to be written to the container, and has ignored these
 *      events (after the first occurrence).  So we need to figure out what to
 *      do here yet; we need to distinguish between forcing these events and
 *      them being part of the edit.
 *
 * \threadunsafe
 *      The sequence object bound to this container needs to provide the
 *      locking mechanism when calling this function.
 *
 * \param track
 *      Provides the track number, re 0.  This number is masked into the track
 *      information.
 *
 * \param p
 *      The performance object that will hold some of the parameters needed
 *      when filling the MIDI container.  Not used!!!
 *
 * \param doseqspec
 *      If true (the default), writes out the SeqSpec information.  If false,
 *      we want to write out a regular MIDI track without this information; it
 *      writes a smaller file.
 */

void
track::fill (int track, const performer & /*p*/, bool doseqspec)
{
    eventlist evl = seq().events();           /* used below */
    evl.sort();
    if (doseqspec)
        fill_seq_number(track);

    fill_seq_name(seq().name());

#if defined SEQ66_USE_FILL_TIME_SIG_AND_TEMPO

    /**
     * To allow other sequencers to read Seq24/Seq66 files, we should
     * provide the Time Signature and Tempo meta events, in the 0th (first)
     * track (sequence).  These events must precede any "real" MIDI events.
     * We also need to skip this if tempo track support is in force.
     */

    if (track == 0)
    {
        fill_time_sig_and_tempo(p, evl.has_time_signature(), evl.has_tempo());
    }

#endif

    midi::pulse timestamp = 0;
    midi::pulse deltatime = 0;
    midi::pulse prevtimestamp = 0;
    for (const auto & e : evl)
    {
        timestamp = e.timestamp();
        deltatime = timestamp - prevtimestamp;
        if (deltatime < 0)                          /* midi::pulse == long    */
        {
            errprint("track::fill(): Bad delta-time, aborting");
            break;
        }
        prevtimestamp = timestamp;
        add_event(e, deltatime);                    /* does it sort???      */
    }
    if (doseqspec)
    {
        /*
         * Here, we add SeqSpec entries (specific to rtl66) for triggers
         * (c_triggers_ex or c_trig_transpose), the MIDI buss (c_midibus),
         * time signature (c_timesig), and MIDI channel (c_midichannel).
         * Should we restrict this to only track 0?  No, Seq66 saves these
         * events with each sequence.  Also, the datasize needs to be
         * calculated differently for c_trig_transpose versus c_triggers_ex.
         */

        const triggers::container & triggerlist = seq().triggerlist();
        bool transtriggers = ! rc().save_old_triggers();
        if (transtriggers)
            transtriggers = seq().any_trigger_transposed();

        if (transtriggers)
        {
            int datasize = seq().triggers_datasize(c_trig_transpose);
            put_seqspec(c_trig_transpose, datasize);
        }
        else
        {
            int datasize = seq().triggers_datasize(c_triggers_ex);
            put_seqspec(c_triggers_ex, datasize);
        }
        for (auto & t : triggerlist)
        {
            add_long(t.tick_start());
            add_long(t.tick_end());
            add_long(t.offset());
            if (transtriggers)
                add_byte(t.transpose_byte());
        }
        fill_proprietary();
    }

    /*
     * Last, but certainly not least, write the end-of-track meta-event.
     * If the nominal length of the sequence is less than the last timestamp,
     * we set the delta-time to 0.  Better would be to make sure this can
     * never happen.
     */

    midi::pulse len = seq().get_length();
    if (len < prevtimestamp)
        deltatime = 0;
    else
        deltatime = len - prevtimestamp;     /* meta track end   */

    fill_meta_track_end(deltatime);
}

}           // namespace rtl66

/*
 * track.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

