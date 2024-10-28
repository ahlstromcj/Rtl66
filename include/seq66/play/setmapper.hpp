#if ! defined RTL66_SETMAPPER_HPP
#define RTL66_SETMAPPER_HPP

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
 * \file          setmapper.hpp
 *
 *  This module declares a small manager for a set of sequences, to be used by
 *  the performer.
 *
 * \library       rtl66 library
 * \author        Chris Ahlstrom
 * \date          2019-02-12
 * \updates       2024-06-13
 * \license       GNU GPLv2 or above
 *
 *  This module also creates a small structure for managing sequence
 *  variables, to save on a bunch of arrays.  It manages screen-sets and
 *  mute-groups.  This class is meant to support the main mute groups, the
 *  mute groups from the 'mutes' file, the saved 'armed' statuses, and the
 *  current states of the tracks or sets.
 *
 *  In this class, access is either to a given set, the playing set, or to a
 *  sequence number that ranges from 0 up to the maximum number of sequences
 *  allowed in a given run of the application.
 */

#include "play/mutegroups.hpp"          /* seq66::mutegroups & mutegroup    */
#include "play/setmaster.hpp"           /* seq66::seqmanager and seqstatus  */

namespace seq66
{

/**
 *  Provides a class for managing screensets and mutegroups.  Much of the
 *  action will occur in the selected play-screen.
 */

class setmapper
{
    friend class performer;             /* a very good friend to have   */

private:

    /**
     *  Provides a reference to an external mute group container.  It can be
     *  used to mute and unmute all of the patterns in a set at once.  It can
     *  also be modified to change the pattern when the application is in
     *  Learn mode.
     */

    mutegroups & m_mute_groups;

    /**
     *  The number of loops/patterns in the set.  Saves a calculation of row x
     *  column.  It is important to note the the size of the set is constant
     *  throughout its lifetime (and the lifetime of the application).  This
     *  is the actual set size, not necessarily the maximum.  See the
     *  corresponding static variable.
     */

    int m_set_size;

    /**
     *  Holds a reference to the master set of sets.  This is currently always
     *  supplied by the performer object.  The set-master holds the container of
     *  all existing sets.
     */

    setmaster & m_set_master;

    /**
     *  Keeps track of created sequences, whether or not they are active.
     *  Used by the install_sequence() function.  Note that this value is not
     *  a suitable replacement for m_sequence_max, because there can be
     *  inactive sequences amidst the active sequences.
     */

    int m_sequence_count;

    /**
     *  A replacement for the c_max_sequence constant.  However, this value is
     *  already 32 * 32 = 1024, and is probably enough for any usage.  Famous
     *  last words?  Actually, this value could go up to 2047... 2048 is used
     *  to indicate that there is no background sequence.  See seq::limit().
     */

    seq::number m_sequence_max;

    /**
     *  Indicates the highest-number sequence.  This value starts as 0, to
     *  indicate no sequences loaded, and then contains the highest sequence
     *  number hitherto loaded, plus 1 so that it can be used as a for-loop
     *  limit similar to m_sequence_max.  It's maximum value should be
     *  m_sequence_max.
     *
     *  Currently meant only for limited context to try to squeeze a little
     *  extra speed out of playback.  There's no easy way to lower this value
     *  when the highest sequence is deleted, though.
     */

    seq::number m_sequence_high;

    /**
     *  Hold the number of the currently-in-edit sequence.  Moving this status
     *  from seqmenu into perform into setmapper for better centralized
     *  management.
     */

    seq::number m_edit_sequence;

    /**
     *  Provides a place to save the screenset for later copying into another
     *  screenset. This screenset has a set-number of -1, until it is in use,
     *  when it has a set-number of 0, but is outside the normal set of
     *  screensets, and is accessible only through a copy/paste mechanism.
     */

    screenset m_set_clipboard;

    /**
     *  Indicates which set is now in view and available for playback.  We
     *  guarantee this to be a valid value or a value (-1) that will be
     *  ignored.  We're not fans of throwing things.
     */

    screenset::number m_playscreen;

    /**
     *  To save set lookup during a number of operations, this pointer, owned
     *  by no one, really, stores a painter to the playing screen-set
     *  (play-screen) in the container.
     */

    screenset * m_playscreen_pointer;

    /**
     *  Indicates if the m_saved_armed_statuses[] values are the saved state
     *  of the sequences, and can be restored.
     */

    bool m_armed_saved;

    /**
     *  Holds the status of the current play-screen.
     */

    midi::booleans m_tracks_mute_state;

public:

    setmapper () = delete;

    /**
     *  Creates the array of values, setting them all to 0 (false).
     */

    setmapper
    (
        setmaster & mc,
        mutegroups & mgs,
        int rows        = screenset::c_default_rows,
        int columns     = screenset::c_default_columns
    );

    /*
     * The move and copy constructors, the move and copy assignment operators,
     * and the destructors are all compiler generated, or are deleted.
     */

    setmapper (const setmapper &) = delete;
    setmapper & operator = (const setmapper &) = delete;
    setmapper (setmapper &&) = delete;
    setmapper & operator = (setmapper &&) = delete;
    ~setmapper () = default;

private:

    /**
     *  Given the raw sequence number, returns the calculated set number.
     *
     * \param seqno
     *      The raw sequence number.  Normally, this value can range from 0
     *      to 1023, or whatever the maximum is based on set size and number
     *      of sets.  All seq::number values in setmapper are assumed to be
     *      in this range, whereas as they range from 0 to the set-size when
     *      used by screenset functions.
     *
     * \return
     *      Returns the calculated set number.  It is clamped to a valid
     *      value.
     */

    screenset::number seq_set (seq::number seqno) const
    {
        return clamp(seqno / m_set_size);
    }

    /**
     *  Checks the highest-numbered screen-set in existence, not counting
     *  the "dummy" screen-set, and returns the number of sequences that
     *  represents.  Useful in drawing the rows in the perfedit
     *  and avoiding vertical scrolling issues.
     */

    int sequences_in_sets () const
    {
        return screenset_size() * (master().highest_set() + 1);
    }

    screenset::number seq_set (seq::number s, int & offset) const;

    /**
     *  Gets the offset of the sequence (re 0) in its screen-set.  It assumes
     *  the pointer is good. Used only in performer::announce_sequence().
     *  But see grid_to_index() below.
     */

    int seq_to_offset (seq::cref s) const
    {
        return s.seq_number() % m_set_size;
    }

    seq::number grid_to_index (int row, int column) const
    {
        return play_screen()->grid_to_index(row, column);
    }

    seq::number grid_to_seq (int row, int column) const
    {
        return play_screen()->grid_to_seq(row, column);
    }

    seq::number grid_to_seq
    (
        screenset::number setno,
        int row, int column
    ) const
    {
        return play_screen()->grid_to_seq(setno, row, column);
    }

    bool seq_to_grid
    (
        seq::number seqno,
        int & row, int & column,
        bool global = false
    ) const
    {
        return play_screen()->seq_to_grid(seqno, row, column, global);
    }

    bool index_to_grid (seq::number seqno, int & row, int & column) const
    {
        return play_screen()->index_to_grid(seqno, row, column);
    }

    int max_slot_shift () const
    {
        return m_set_size / setmaster::Size();
    }

    int slot_shift_delta () const
    {
        return setmaster::Rows();
    }

    void clear ()
    {
        master().clear();
        m_sequence_count = 0;
        m_sequence_high = m_edit_sequence = seq::unassigned();
    }

    int sequence_count () const
    {
        return m_sequence_count;
    }

    int rows () const
    {
        return play_screen()->rows();
    }

    int columns () const
    {
        return play_screen()->columns();
    }

    bool group_event () const
    {
        return mutes().group_event();
    }

    bool group_error () const
    {
        return mutes().group_error();
    }

    /**
     *  group_mode() starts out true, and allows mute_group_tracks() to work.
     *  It is set and unset via the "gmute" MIDI control and the group-on/off
     *  keys.  m_mode_group_learn starts out false, and is set and unset via
     *  the "glearn" MIDI control and the group-learn press and release
     *  actions.
     */

    bool group_mode () const
    {
        return mutes().group_mode();
    }

    void group_mode (bool flag)
    {
        mutes().group_mode(flag);
    }

    void toggle_group_mode ()
    {
        mutes().toggle_group_mode();
    }

    bool any_in_edit () const
    {
        return master().any_in_edit();
    }

    bool is_seq_in_edit (seq::number seqno) const;
    bool reset ();

#if defined RTL66_USE_SCREENSET_RESET_SEQUENCES     /* currently unused */
    void reset_sequences (bool pause, sequence::playback mode)
    {
        for (auto & sset : sets())
            sset.second.reset_sequences(pause, mode);
    }
#endif

    void play_all_sets
    (
        midi::pulse tick, sequence::playback mode, bool resumenoteons
    );

    seq::number sequence_high () const
    {
        return m_sequence_high;
    }

    seq::number sequence_max () const
    {
        return m_sequence_max;
    }

    /**
     * \setter m_edit_sequence
     *
     * \param seqno
     *      Pass in seq::unassigned() (-1) to disable the edit-sequence number
     *      unconditionally.  Use unset_edit_sequence() to disable it if it
     *      matches the current edit-sequence number.
     */

    void set_edit_sequence (seq::number seqno)
    {
        m_edit_sequence = seqno;
    }

    /**
     * \setter m_edit_sequence
     *
     *      Disables the edit-sequence number if it matches the parameter.
     *
     * \param seqno
     *      The sequence number of the sequence to unset.
     */

    void unset_edit_sequence (seq::number seqno)
    {
        if (is_edit_sequence(seqno))
            set_edit_sequence(seq::unassigned());
    }

    bool any_modified_sequences () const;
    void unmodify_all_sequences ();
    void set_dirty (seq::number seqno = seq::all());

    /**
     * \getter m_edit_sequence
     *
     * \param seqno
     *      Tests the parameter against m_edit_sequence.  Returns true
     *      if that member is not -1, and the parameter matches it.
     */

    bool is_edit_sequence (seq::number seqno) const
    {
        return (m_edit_sequence != seq::unassigned()) &&
            (seqno == m_edit_sequence);
    }

    /**
     *  Checks if a sequence is exportable.
     *
     * \param seqno
     *      Provides the raw sequence number, which ranges from 0 to 1023.
     *      However, when the screenset calls is_exportable(), it get remapped
     *      to the range 0 to m_set_size - 1.
     */

    bool is_exportable (seq::number seqno) const
    {
        return screen(seqno).is_exportable(seqno);
    }

    bool is_dirty_main (seq::number seqno) const
    {
        return screen(seqno).is_dirty_main(seqno);
    }

    bool is_dirty_edit (seq::number seqno) const
    {
        return screen(seqno).is_dirty_edit(seqno);
    }

    bool is_dirty_perf (seq::number seqno) const
    {
        return screen(seqno).is_dirty_perf(seqno);
    }

    bool is_dirty_names (seq::number seqno) const
    {
        return screen(seqno).is_dirty_names(seqno);
    }

    int color (seq::number seqno) const
    {
        return screen(seqno).color(seqno);
    }

    bool color (seq::number seqno, int c)
    {
        return screen(seqno).color(seqno, c);
    }

    bool is_seq_active (seq::number seqno) const
    {
        return screen(seqno).active(seqno);
    }

    bool is_seq_recording (seq::number seqno) const
    {
        return screen(seqno).recording(seqno);
    }

    seq::number first_seq () const
    {
        return play_screen()->first_seq();
    }

    /*
     * So far, nobody calls this function.
     */

    void activate (int seqno, seq::number seqnum, bool flag = true)
    {
        screen(seqno).activate(seqno, seqnum, flag);
    }

    void off_sequences (seq::number seqno = seq::unassigned());

    /**
     *  Calls sequence::song_recording_start(m_current_tick) for all
     *  sequences.  Should be called only when not recording the performance
     *  data.  This is a Kepler34 feature.  However, rather than operating on
     *  all sets, it will operate on the play-screen only.  Added the snap
     *  parameter for issue #44.
     */

    void song_recording_start (midi::pulse tick, bool snap = true)
    {
        play_screen()->song_recording_start(tick, snap);
    }

    void song_recording_stop (midi::pulse tick)
    {
        play_screen()->song_recording_stop(tick);
    }

    /**
     *  Clears the snapshot statuses. Needed when disabling the queue mode.
     */

    void clear_snapshot ()
    {
        for (auto & sset : sets())
            sset.second.clear_snapshot();
    }

    /**
     *  For all active patterns/sequences, this function gets the playing
     *  status and saves it.  Inactive patterns get the value set to false.
     *  Used in unsetting the snapshot status (automation :: ctrlstatus ::
     *  snapshot). A rework of performer::save_playing_state().
     */

    void save_snapshot ()
    {
        for (auto & sset : sets())
            sset.second.save_snapshot();
    }

    /**
     *  For all active patterns/sequences, this function gets the playing
     *  status from the setmapper, and sets it for the sequence.  Used in
     *  unsetting the snapshot status (automation::ctrlstatus::snapshot).
     */

    void restore_snapshot ()
    {
        for (auto & sset : sets())
            sset.second.restore_snapshot();
    }

    /**
     *  Perhaps we need to check ONLY the play_screen()!!!
     */

    bool needs_update () const
    {
        for (const auto & sset : sets())
        {
            if (sset.second.needs_update())
                return true;
        }
        return false;
    }

    /*
     * exec_set_function(s) executes a set-handler for each set.
     * exec_set_function(s,p) runs a set-handler and a slot-handler for each
     * set.  exec_set_function(p) runs the slot-handler for all patterns in
     * all sets.  exec_slot_function(p) runs the slot-handler for the
     * play-screen patterns.
     */

    bool exec_set_function (screenset::sethandler s)
    {
        return master().exec_set_function(s);
    }

    bool exec_set_function (screenset::sethandler s, screenset::slothandler p)
    {
        return master().exec_set_function(s, p);
    }

    bool exec_set_function (screenset::slothandler p)
    {
        return master().exec_set_function(p);
    }

    bool exec_slot_function
    (
        screenset::slothandler p,
        bool use_set_offset = true
    )
    {
        return play_screen()->exec_slot_function(p, use_set_offset);
    }

    void set_last_ticks (midi::pulse tick)
    {
        for (auto & sset : sets())
            sset.second.set_last_ticks(tick);
    }

    void apply_song_transpose (seq::number seqno = seq::all());
    int trigger_count () const;
    midi::pulse max_trigger () const;
    midi::pulse max_timestamp () const;
    midi::pulse max_extent () const;
    void select_triggers_in_range
    (
        seq::number seqlow, seq::number seqhigh,
        midi::pulse tickstart, midi::pulse tickfinish
    );
    void unselect_triggers (seq::number seqno = seq::all());
    bool move_triggers
    (
        midi::pulse lefttick, midi::pulse righttick,
        bool direction, seq::number seqno = seq::all()
    );
    void copy_triggers
    (
        midi::pulse lefttick, midi::pulse righttick,
        seq::number seqno = seq::all()
    );

    void push_trigger_undo ()
    {
        for (auto & sset : sets())
            sset.second.push_trigger_undo();
    }

    void pop_trigger_undo ()
    {
        for (auto & sset : sets())
            sset.second.pop_trigger_undo();
    }

    void pop_trigger_redo ()
    {
        for (auto & sset : sets())
            sset.second.pop_trigger_redo();
    }

    /**
     *  Looks up the sequence with the given sequence number.
     *
     * Current implementation:
     *
     *      Use the static function seq_set() to calculate the desired set and
     *      offset into the set using the application-wide row and column
     *      size.  We make this work faster by calculating the set based on
     *      the sequence number.
     *
     * Alternate implemention:
     *
     *      Go through all the sets and all the sequences in each set until it
     *      finds the exact sequence number as set by set_active().
     *
     * \param seqno
     *      Provides the sequence number.  Historically, this value varies
     *      from 0 to 1023, and provided the index into a number of arrays.
     *      Although we now use containers of screensets and seq/sequence
     *      objects, the performer and midifile classes continue to number
     *      them as if in an array.
     *
     * \return
     *      Returns a shared-pointer to the desired sequence.  This pointer
     *      should be checked before being used.
     */

    const seq::pointer loop (seq::number seqno) const
    {
        return screen(seqno).loop(seqno);
    }

    /**
     *  Provides the private, non-const version of loop().
     *
     * \param seqno
     *      Provides the sequence number.
     *
     * \return
     *      Returns a pointer to the desired sequence.  This pointer should be
     *      checked before being used.
     */

    seq::pointer loop (seq::number seqno)
    {
        return screen(seqno).loop(seqno);
    }

    /**
     *  Converts an offset into the play-screen (ranging from 0 to
     *  m_set_size-1) into a sequence number in the range of the play-screen.
     *  This number can then be used for lookup via the loop() function.  A
     *  bad value (-1) is returned if the play-screen does not exist.
     */

    seq::number play_seq (seq::number seqno)
    {
        return play_screen()->play_seq(seqno);
    }

    void save_queued (int hotseq)
    {
        play_screen()->save_queued(hotseq);
    }

    void unqueue (int hotseq)
    {
        play_screen()->unqueue(hotseq);
    }

    bool armed () const;

    bool armed (seq::number seqno) const
    {
        return screen(seqno).armed(seqno);
    }

    void armed (seq::number seqno, bool flag)
    {
        screen(seqno).armed(seqno, flag);
    }

    bool muted (seq::number seqno) const
    {
        return ! armed(seqno);
    }

    void arm (seq::number seqno)
    {
        armed(seqno, true);
    }

    void mute (seq::number seqno)
    {
        armed(seqno, false);
    }

    void toggle (seq::number seqno = seq::all());
    void toggle_song_mute (seq::number seqno = seq::all());
    void toggle_playing_tracks ();

    void arm ()
    {
        for (auto & sset : sets())
            sset.second.arm();
    }

    void mute ()
    {
        for (auto & sset : sets())
            sset.second.mute();
    }

    void mute_all_tracks (bool flag)
    {
        if (flag)
            mute();
        else
            arm();
    }

    void apply_armed_statuses ()
    {
        for (auto & sset : sets())
            sset.second.apply_armed_statuses();
    }

    bool learn_armed_statuses ();

    void all_notes_off ()
    {
        for (auto & sset : sets())
            sset.second.all_notes_off();
    }

    void panic ()
    {
        for (auto & sset : sets())
            sset.second.panic();
    }

public:

    void show (bool showseqs = true) const;

    /**
     *  The screen() functions look for the screen-set that contains the
     *  specified (by number) sequence.  If not found, then the dummy
     *  screen-set is returned.
     *
     *  The play_screen() functions return the screen that is showing in the
     *  main Live grid.
     */

    const screenset & screen (seq::number seqno) const;
    screenset & screen (seq::number seqno);

    screenset * play_screen ()
    {
        return m_playscreen_pointer;
    }

    const screenset * play_screen () const
    {
        return m_playscreen_pointer;
    }

    screenset::number change_playscreen (int amount)
    {
        screenset::number result = m_playscreen + amount;
        return set_playscreen(result);
    }

    screenset::number playscreen_number () const
    {
        return m_playscreen;
    }

    seq::number playscreen_offset () const
    {
        return play_screen()->offset();
    }

    int playscreen_active_count () const
    {
        return play_screen()->active_count();
    }

    bool set_playscreen (screenset::number setno);
    bool set_playing_screenset (screenset::number setno);
    bool copy_screenset (screenset::number srcset, screenset::number destset);
    bool save_screenset (screenset::number srcset);
    bool paste_screenset (screenset::number destset);

    /*
     *  Encapsulates some calls used in mainwnd.
     */

    screenset::number increment_screenset (int amount)
    {
        return change_playscreen(amount);
    }

    screenset::number decrement_screenset (int amount)
    {
        return change_playscreen(-amount);
    }

    const std::string & name () const
    {
        return play_screen()->name();
    }

    std::string name (screenset::number setno) const;
    bool name (screenset::number setno, const std::string & nm);

    bool name (const std::string & nm)
    {
        return play_screen()->name(nm);
    }

    bool is_screenset_active (screenset::number setno) const
    {
        return master().is_screenset_active(setno);
    }

    bool is_screenset_available (screenset::number setno) const
    {
        /*
         * Now all 32 slots have a screenset, but inactive vs. active.
         *
         * return master().is_screenset_available(setno);
         */

         return master().is_screenset_active(setno);
    }

    /**
     *  A helper function for determining if:
     *
     *      -   the mode group is in force
     *      -   the sequence is in the range of the playing screenset
     *      -   playing screenset is the same as the current screenset
     *
     * We're not sure why the third test is necessary, so it is disabled.
     *
     * \param seqno
     *      Provides the index of the desired sequence.
     *
     * \return
     *      Returns true if the sequence adheres to the conditions noted above.
     */

    bool seq_in_playscreen (seq::number seqno)
    {
        return group_mode() && play_screen()->seq_in_set(seqno);
    }

    int screenset_size () const
    {
        return m_set_size;                  /* play_screen()->set_size()    */
    }

    bool install_sequence (sequence * s, seq::number & seqno);
    bool add_sequence (sequence * s, seq::number & seqno);
    bool remove_sequence (seq::number seqno);

    bool swap_sets (seq::number set0, seq::number set1)
    {
        return master().swap_sets(set0, set1);
    }

    bool set_mutes (mutegroup::number gmute, const midi::booleans & bits)
    {
        return mutes().set(gmute, bits);
    }

    bool apply_mutes (mutegroup::number gmute);
    bool unapply_mutes (mutegroup::number gmute);
    bool toggle_mutes (mutegroup::number gmute);
    bool toggle_active_mutes (mutegroup::number gmute);
    bool learn_mutes (bool learnmode, mutegroup::number gmute);

#if 0           // unused
    bool clear_mutes ();
#endif

    void select_and_mute_group (mutegroup::number group);
    void mute_group_tracks ();
    void sequence_playing_change
    (
        seq::number seqno, bool on, bool qinprogress = false
    );
    void sequence_playscreen_change
    (
        seq::number seqno, bool on, bool qinprogress = false
    );

private:

    bool fill_play_set (playset & p, bool clearit = true);
    bool add_to_play_set (playset & p, sequence * s);
    bool add_to_play_set (playset & p, screenset & s);
    bool add_all_sets_to_play_set (playset & p);
    void recount_sequences ();

    setmaster::container::iterator add_set (screenset::number setno)
    {
        return master().add_set(setno);
    }

    setmaster::container::iterator find_by_value (screenset::number setno)
    {
        return master().find_by_value(setno);
    }

    bool remove_set (screenset::number setno)
    {
        return master().remove_set(setno);
    }

    bool clear_set (screenset::number setno)
    {
        return master().clear_set(setno);
    }

    mutegroup::number clamp_group (mutegroup::number group) const
    {
        return mutes().clamp_group(group);
    }

    bool check_group (mutegroup::number group) const
    {
        return mutes().check_group(group);
    }

    screenset::number clamp (screenset::number offset) const
    {
        return master().clamp(offset);
    }

    screenset & dummy_screenset ()
    {
        return master().dummy_screenset();
    }

    const screenset & dummy_screenset () const
    {
        return master().dummy_screenset();
    }

    mutegroups & mutes ()
    {
        return m_mute_groups;
    }

    const mutegroups & mutes () const
    {
        return m_mute_groups;
    }

    setmaster & master ()
    {
        return m_set_master;
    }

    const setmaster & master () const
    {
        return m_set_master;
    }

    setmaster::container & sets ()
    {
        return master().set_container();
    }

    const setmaster::container & sets () const
    {
        return master().set_container();
    }

};              // class setmapper

}               // namespace seq66

#endif          // RTL66_SETMAPPER_HPP

/*
 * setmapper.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

