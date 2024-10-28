#if ! defined RTL66_MUTEGROUP_HPP
#define RTL66_MUTEGROUP_HPP

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
 * \file          mutegroup.hpp
 *
 *  This module declares a linear vector class solely to hold the mute status
 *  of a number of sequences in a set, while also able to access
 *  patterns/loops by row and column.
 *
 * \library       rtl66 library
 * \author        Chris Ahlstrom
 * \date          2018-12-01
 * \updates       2024-06-13
 * \license       GNU GPLv2 or above
 *
 */

#include <functional>                   /* std::function, function objects  */
#include <vector>

#include "midi/midibytes.hpp"           /* seq66::midi::booleans, etc.      */
#include "play/screenset.hpp"           /* seq66::screenset constants       */

namespace seq66
{

/**
 *  Provides a class that represents an array the same size as a screenset,
 *  but holding armed statuses that can be saved an applied later.  Note that,
 *  unlike the array of mute-groups represented by the mutegroups class, the
 *  size and layout of each mute-group (mutegroup class), like screen-sets, is
 *  potentially modifiable by the configuration.
 */

class mutegroup
{

public:

    /**
     *  A revealing alias for mutegroup numbers.
     */

    using number = int;

    /**
     *  Provides an alias for functions that can be called on all groups.
     *  We might end up not using this, letting the settmapper do the work.
     */

    using grouphandler = std::function<bool (mutegroup &, mutegroup::number)>;

    static const int c_default_rows    = screenset::c_default_rows;
    static const int c_default_columns = screenset::c_default_columns;

private:

    /**
     *  Provides a mnemonic name for the group.  By default, it is of the
     *  format "Group 1", but can be modified directly in the mute-groups
     *  table in the Mutes tab.
     */

    std::string m_name;

    /**
     *  Indicates the current state of the mute-group, either on or off.
     *  Useful in toggling.
     */

    mutable bool m_group_state;

    /**
     *  The number of loops/patterns in the mute-group.  Saves a calculation
     *  of row x column.  It is important to note the the size of the group is
     *  constant throughout its lifetime (and the lifetime of the
     *  application).
     *
     *  For issue #124, we have removed the const from some member declarations
     *  so that the default constructor etc. are not deleted. Thanks to clang
     *  for uncovering that.
     */

    /* const */ int m_group_size;

    /**
     *  Holds a set of boolean values in a 1-D vector, but can be virtually
     *  arranged by row and column.  Note that we use midi::bool rather than
     *  bool, to avoid bitset issues.
     */

    midi::booleans m_mutegroup_vector;

    /**
     *  Indicates the number of virtual rows in a screen-set (bank), which is
     *  also the same number of virtual rows as a mute-group.  This value will
     *  generally be the same as the size used in the rest of the application.
     *  The default value is the historical value of 4 rows per set or
     *  mute-group.
     */

    /* const */ int m_rows;

    /**
     *  Indicates the number of virtual columns in a screen-set (bank), which
     *  is also the same number of virtual columns as a mute-group.  This
     *  value will generally be the same as the size used in the rest of the
     *  application.  The default value is the historical value of 8 columns
     *  per set or mute-group.
     */

    /* const */ int m_columns;

    /**
     *  Experimental option to swap rows and columns.  See the function
     *  swap_coordinates().  This swap doesn't apply to the number of rows and
     *  columns, but to whether incrementing the sequence number moves to the
     *  next or othe next column.
     */

    bool m_swap_coordinates;

    /**
     *  Indicates the group (akin to the set or bank number) represented by
     *  this mutegroup object.
     */

    number m_group;

    /**
     *  Indicates the screen-set offset (the number of the first loop/pattern
     *  in the screen-set).  This value is m_group_count * m_group.  This
     *  saves a calculation.
     */

    /* const */ int m_group_offset;

public:

    /*
     *  Creates the vector of values, setting them all to 0 (false).
     */

    mutegroup
    (
        number group    = 0,
        int rows        = c_default_rows,
        int columns     = c_default_columns
    );

    /*
     * The move and copy constructors, the move and copy assignment operators,
     * and the destructors are all compiler generated.
     */

    mutegroup (const mutegroup &) = default;
    mutegroup & operator = (const mutegroup &) = default;
    mutegroup (mutegroup &&) = default;
    mutegroup & operator = (mutegroup &&) = default;
    ~mutegroup () = default;

    /**
     *  Checks if a the sequence number is an assigned one, i.e. not equal to
     *  -1.
     */

    static bool none (number group)
    {
        return group == (-1);
    }

    /**
     *  Indicates that a mute-group number has not been assigned.
     */

    static number unassigned ()
    {
        return (-1);
    }

    void invalidate ()
    {
        m_group = unassigned();
    }

    bool valid () const
    {
        return m_group >= 0;    /* should check upper range at some point */
    }

    bool group_state () const
    {
        return m_group_state;
    }

    void group_state (bool f)
    {
        m_group_state = f;
    }

    int count () const
    {
        return int(m_mutegroup_vector.size());
    }

    int armed_count () const;
    bool armed (int index) const;
    void armed (int index, bool flag);

    bool muted (int index) const
    {
        return ! armed(index);
    }

    bool set (const midi::booleans & bits);

    const midi::booleans & zeroes () const
    {
        static midi::booleans s_bits(m_group_size, midi::boolean(false));
        return s_bits;
    }

    const midi::booleans & get () const
    {
        return m_mutegroup_vector;
    }

    const std::string & name () const
    {
        return m_name;
    }

    void name (const std::string & n)
    {
        m_name = n;
    }

    number group () const
    {
        return m_group;
    }

    int rows () const
    {
        return m_rows;
    }

    int columns () const
    {
        return m_columns;
    }

    bool swap_coordinates () const
    {
        return m_swap_coordinates;
    }

    bool any () const;
    void clear ();
    void show () const;

private:

    bool mute_to_grid (int group, int & row, int & column) const;

    /**
     *  Calculates the group index (i.e. a pattern number) given by the rows,
     *  columns, and the group-offset value.
     *
     * \return
     *      Returns 0 if the row or column were illegal.  But 0 is also a
     *      legal value.
     */

    int grid_to_mute (int row, int column);

};              // class mutegroup

/*
 * Global (free) midi::byte functions.
 */

extern std::string write_stanza_bits
(
    const midi::booleans & bitbucket,
    int grouping    = mutegroup::c_default_columns,
    bool newstyle   = true
);
extern bool parse_stanza_bits
(
    midi::booleans & target,
    const std::string & mutestanza
);

}               // namespace seq66

#endif          // RTL66_MUTEGROUP_HPP

/*
 * mutegroup.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

