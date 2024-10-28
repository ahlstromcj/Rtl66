#if ! defined RTL66_MIDI_EVENTLIST_HPP
#define RTL66_MIDI_EVENTLIST_HPP

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
 * \file          eventlist.hpp
 *
 *  This module provides a stand-alone module for the event-list container
 *  used by the application.
 *
 * \library       rtl66
 * \author        Chris Ahlstrom
 * \date          2015-09-19
 * \updates       2023-09-23
 * \license       GNU GPLv2 or above
 *
 *  This module extracts the event-list functionality from the sequencer
 *  module, so that it is easier to try to replace it with some better
 *  container later.
 *
 *  In addition, this version contains some information from the performer and
 *  sequence classes of Seq66 that is of use in any MIDI application, so
 *  that this library can stand along for basic MIDI operations.
 *
 *  List versus Map:  #if defined or derivation from an interface?  For our
 *  purposes, #if defined might be simplest, and we only want to pick the
 *  fastest one, ultimately.
 *
 *  It turns out the the std::multimap implementation is a little bit faster
 *  in release mode, and a lot faster in debug mode.  Why?  Probably because
 *  the std::list implementation calls std::list::sort() a lot, and the
 *  std::multimap implementation is a lot faster at sorting.  But since the
 *  map iterator is slower, we stick with std::list.
 *
 *  But, based on this article:
 *
 * https://baptiste-wicht.com/posts/2012/12/cpp-benchmark-vector-list-deque.html
 *
 *  we will now use std::vector for the event list.
 */

/**
 *  We made some fixes for sequencer64 issue #141 to disable saving the tempo
 *  into first track. But for Seq66, we do want to be able to save the
 *  time signature of each pattern with that pattern (especially if different
 *  from the global time signature). Note that we would support only one
 *  time-signature per pattern, but unlimited tempo changes.  Also note that,
 *  unlike tempo, time-signature does not affect the playback of MIDI events.
 *  It changes how they are displayed.
 */

#undef   RTL66_USE_FILL_TIME_SIG_AND_TEMPO

#include <atomic>                       /* std::atomic<bool> usage          */

#include "midi/event.hpp"               /* midi::event, midi::event::buffer */
#include "midi/trackinfo.hpp"           /* data holders for MIDI parameters */

namespace midi
{

/**
 *  The eventlist class is a receptable for MIDI events.
 */

class eventlist
{
    friend class editable_events;       // access to verify_and_link()
    friend class midifile;              // access to print()
    friend class track;                 // any_selected_notes()

public:

    /**
     * Actions.  These variables represent actions that can be applied to a
     * selection of notes.  One idea would be to add a swing-quantize action.
     * We will reserve the value here, for notes only; not yet used or part of
     * the action menu.
     */

    enum class edit
    {
        select_all_notes = 1,
        select_all_events,
        select_inverse_notes,
        select_inverse_events,
        quantize_notes,
        quantize_events,
        randomize_events,
        tighten_events,
        tighten_notes,
        transpose_notes,            /* basic transpose          */
        reserved,                   /* later: quantize_swing    */
        transpose_harmonic,         /* harmonic transpose       */
        expand_pattern,
        compress_pattern,
        select_even_notes,
        select_odd_notes,
        swing_notes                 /* swing quantize           */
    };

    /**
     *  This enumeration is used in selecting events and note.  Se the
     *  select_note_events() and select_events() functions.
     */

    enum class select
    {
        selecting,      /**< Selection in progress.                     */
        select_one,     /**< To select a single event.                  */
        selected,       /**< The events are selected.                   */
        would_select,   /**< The events would be selected.              */
        deselect,       /**< To deselect event under the cursor.        */
        toggle,         /**< Toggle selection under cursor.             */
        remove,         /**< To remove one note under the cursor.       */
        onset,          /**< Kepler34, To select a single onset.        */
        is_onset        /**< New, from Kepler34, onsets selected.       */
    };

private:

    /**
     *  This list holds the current pattern/sequence events.
     */

    event::buffer m_events;

    /**
     *  Eventually we want to be able to move through events of a given type,
     *  such as Meta Text events.
     */

    bool m_match_iterating;
    event::iterator m_match_iterator;

    /**
     *  Provides an atomic flag to raise while sorting(), which can invalidate
     *  iterators while a user-interface is accessing the event list, or while
     *  clearing the event list.
     */

    std::atomic<bool> m_action_in_progress;

    /**
     *  Holds the length of the sequence holding this event-list,
     *  in pulses (ticks).  See sequence::m_length.  This value is merely the
     *  user-specified length of the track, not the actual time-stamp of the
     *  last event.
     */

    midi::pulse m_length;

    /**
     *  Provides the number of ticks to shave off of the end of painted notes.
     *  Also used when the user attempts to shrink a note to zero (or less
     *  than zero) length.
     */

    midi::pulse m_note_off_margin;

    /**
     *  A flag to indicate if an event was added or removed.  We may need to
     *  give client code a way to reload the sequence.  This is currently an
     *  issue when a seqroll and an eventedit/eventslots are active for the
     *  same sequence.
     */

    bool m_is_modified;

    /**
     *  A new flag to indicate that a tempo event has been added.  Legacy
     *  behavior forces the tempo to be written to the track-0 sequence,
     *  but we don't want to do that if the MIDI file (or the current event
     *  list) contains a tempo event.
     */

    bool m_has_tempo;

    /**
     *  A new flag to indicate that a time-signature event has been added.
     *  Legacy behavior forces the time-signature to be written to the track-0
     *  sequence, but we don't want to do that if the MIDI file (or the
     *  current event list) contains a time-signature event.
     */

    bool m_has_time_signature;

    /**
     *  Another flag.
     */

    bool m_has_key_signature;

    /**
     *  Stores the setting of usr().new_pattern_wraparound().  It is used in
     *  the link_new() function.
     */

    bool m_link_wraparound;

public:

    eventlist ();
    eventlist (const eventlist & rhs);                      /* = default;   */
    eventlist (eventlist &&) = delete;  // ???
    eventlist & operator = (const eventlist & rhs);         /* = default;   */
    eventlist & operator = (eventlist &&) = delete;

    virtual ~eventlist ()
    {
        // No code needed
    }

    /*
     * These operators are used in the scales, eventlist, editable_events,
     * and sequence classes.
     */

    event::iterator begin ()
    {
        return m_events.begin();
    }

    event::const_iterator cbegin () const
    {
        return m_events.cbegin();
    }

    event::iterator end ()
    {
        return m_events.end();
    }

    event::const_iterator cend () const
    {
        return m_events.cend();
    }

    /**
     *  Returns the number of events stored in m_events.  We like returning
     *  an integer instead of size_t, and rename the function so nobody is
     *  fooled.
     */

    int count () const
    {
        return int(m_events.size());
    }

    int playable_count () const;
    bool is_playable () const;
    midi::pulse get_min_timestamp () const;
    midi::pulse get_max_timestamp () const;
    bool add (const event & e);
    bool append (const event & e);
    void scan_meta_events ();

    bool empty () const
    {
        return m_events.empty();
    }

    midi::pulse length () const
    {
        return m_length;
    }

    midi::pulse note_off_margin () const
    {
        return m_note_off_margin;
    }

    bool is_modified () const
    {
        return m_is_modified;
    }

    bool has_tempo () const
    {
        return m_has_tempo;
    }

    bool has_time_signature () const
    {
        return m_has_time_signature;
    }

    bool has_key_signature () const
    {
        return m_has_key_signature;
    }

    /**
     * \setter m_is_modified
     *      This function may be needed by some of the sequence editors.
     *      But use it with great caution.
     */

    void unmodify ()
    {
        m_is_modified = false;
    }

    /**
     *  Provides a wrapper for the iterator form of erase(), which is the
     *  only one that sequence uses.  Currently, no check on removal is
     *  performered.  Sets the modified-flag.
     *
     * \param ie
     *      Provides the iterator to the event to be removed.
     *
     * \return
     *      Returns an iterator to the next element, or end() if the container
     *      is now empty.
     */

    event::iterator remove (event::iterator ie)
    {
        event::iterator result = m_events.erase(ie);
        m_is_modified = true;
        return result;
    }

    void clear ();
    void sort ();
    bool merge (const eventlist & el, bool presort = true);

    bool action_in_progress () const
    {
        return m_action_in_progress;
    }

    /**
     *  Dereference access for list or map.
     *
     * \param ie
     *      Provides the iterator to the event to which to get a reference.
     */

    static event & dref (event::iterator ie)
    {
        return *ie;
    }

    /**
     *  Dereference const access for list or map.
     *
     * \param ie
     *      Provides the iterator to the event to which to get a reference.
     */

    static const event & cdref (event::const_iterator ie)
    {
        return *ie;
    }

private:                                /* internal quantization functions  */

    bool add (event::buffer & evlist, const event & e);
    void merge (const event::buffer & evlist);

private:                                /* functions for friend sequence    */

    /*
     * The following functions provide internal for-loops that do not
     * involved data from the caller.
     */

    void link_new (bool wrap = false);
    void clear_links ();
    int note_count () const;
    void verify_and_link (midi::pulse slength = 0, bool wrap = false);
    bool edge_fix (midi::pulse snap, midi::pulse seqlength);
    bool remove_unlinked_notes ();
    bool quantize_events
    (
        midi::byte status, midi::byte cc, int snap,
        int divide, bool fixlink
    );
    bool quantize_all_events (int snap, int divide);
    bool quantize_notes (int snap, int divide);
    midi::pulse adjust_timestamp (event & er, midi::pulse deltatick);
    void scale_note_off (event & noteoff, double factor);
    midi::pulse apply_time_factor
    (
        double factor,
        bool savenotelength = false,
        bool relink = false
    );
    bool reverse_events (bool inplace = false, bool relink = false);
    bool move_selected_notes (midi::pulse delta_tick, int delta_note);
    bool move_selected_events (midi::pulse delta_tick);
    bool align_left (bool relink = false);
    bool randomize_selected (midi::byte status, int range);
    bool randomize_selected_notes (int range);
    bool jitter_events (int snap, int jitr);
    bool jitter_notes (int snap, int jitr);
    bool link_notes (event::iterator eon, event::iterator eoff);
    void link_tempos ();
    void clear_tempo_links ();
    bool mark_selected ();
    void mark_out_of_range (midi::pulse slength);
    void mark_all ();
    void unmark_all ();
    bool remove_event (event & e);
    event::iterator find_first_match
    (
        const event & e,
        midi::pulse starttick = 0
    );
    event::iterator find_next_match (const event & e);
    bool remove_first_match
    (
        const event & e,
        midi::pulse starttick = 0
    );
    bool remove_marked ();                  /* deprecated   */
    bool remove_selected ();
    void unpaint_all ();
    int count_selected_notes () const;
    bool any_selected_notes () const;
    int count_selected_events (midi::byte status, midi::byte cc) const;
    bool any_selected_events () const;
    bool any_selected_events (midi::byte status, midi::byte cc) const;
    void select_all ();
    void select_by_channel (int channel);
    void select_notes_by_channel (int channel);
    bool set_channels (int channel);
    void unselect_all ();
    int select_events
    (
        midi::pulse tick_s, midi::pulse tick_f,
        midi::byte status, midi::byte cc, select action
    );
    int select_event_handle
    (
        midi::pulse tick_s, midi::pulse tick_f,
        midi::byte astatus, midi::byte cc,
        midi::byte data
    );
    int select_note_events
    (
        midi::pulse tick_s, int note_h,
        midi::pulse tick_f, int note_l, select action
    );
    bool event_in_range
    (
        const event & e, midi::byte status,
        midi::pulse tick_s, midi::pulse tick_f
    ) const;
    bool get_selected_events_interval
    (
        midi::pulse & first, midi::pulse & last
    ) const;
    bool rescale (midi::ppqn oldppqn, midi::ppqn newppqn);
    bool stretch_selected (midi::pulse delta);
    bool grow_selected (midi::pulse delta, int snap);
    bool copy_selected (eventlist & clipbd);
    bool paste_selected (eventlist & clipbd, midi::pulse tick, int note);
    midi::pulse trim_timestamp (midi::pulse t) const;
    midi::pulse clip_timestamp
    (
        midi::pulse ontime, midi::pulse offtime, int snap
    ) const;
    void print () const;
    std::string to_string () const;
    void print_notes (const std::string & tag = "in list") const;

    const event::buffer & events () const
    {
        return m_events;
    }

    void length (midi::pulse len)
    {
        if (len > 0)
            m_length = len;
    }

};          // class eventlist

}           // namespace midi

#endif      // RTL66_MIDI_EVENTLIST_HPP

/*
 * eventlist.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

