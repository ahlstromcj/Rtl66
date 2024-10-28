#ifndef RTL66_NOTEMAPPER_HPP
#define RTL66_NOTEMAPPER_HPP

/*
 * This file is part of rtl66.
 *
 * rtl66 is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * rtl66 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with rtl66; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */

/**
 * \file          notemapper.hpp
 *
 *    This module provides functions for advanced MIDI/text conversions.
 *
 * \library       libmidipp
 * \author        Chris Ahlstrom
 * \date          2014-04-24
 * \updates       2024-06-13
 * \version       $Revision$
 * \license       GNU GPL
 *
 *    The mapping process works though static functions that reference a
 *    global notemapper object.
 *
 *    This object gets its setup from an INI file.  This INI file has an
 *    unnamed section with the following format:
 *
\verbatim
         gm-channel = 10
         device-channel = 16
\endverbatim
 *
 *    The "drum" sections are named for the GM note that is to be
 *    remapped.
 *
\verbatim
         [ Drum 35 ]
         gm-name  = Acoustic Bass Drum
         gm-note  = 35
         dev-note = 35
\endverbatim
 *
 */

#include <map>
#include <string>

#include "cfg/basesettings.hpp"         /* seq66::basesettings class        */
#include "midi/midibytes.hpp"           /* seq66::c_notes_count             */

namespace seq66
{

 /**
  * This class provides for some basic remappings to be done to MIDI
  * files, using the old and new facilities of libmidifilex.
  *
  *    It works by holding all sorts of standard C++ map objects that are
  *    used to translate from one numeric value to another.
  *
  *    For use in the midicvtpp application, a single global instance of
  *    this object is created, and is used in static C-style callback
  *    functions that can be used in the C library libmidifilex.
  */

class notemapper final : public basesettings
{
    friend void show_maps
    (
        const std::string & tag,
        const notemapper & container,
        bool full_output
    );

public:

    /**
     *  Provides a constant to indicate an inactive or invalid integer
     *  value.
     */

    static const int NOT_ACTIVE = -1;

private:

    /**
     *  This class is meant to extend the map of values with additional data
     *  that can be written out to summarize some information about the MIDI
     *  remapping that was done.  Instead of just the integer value to use,
     *  this class holds the names of the items on both ends of the mapping,
     *  plus a usage count.  We also added the "GM equivalent" name to this
     *  class as well.
     */

    class pair
    {

    private:

        /**
         *  Indicates if this is a reversed pair.  This boolean is needed to
         *  determine whether the dev-note or the gm-note is the key value.
         *
         *  For issue #124, clang deletes the assignment operator, so we
         *  get rid of the consts.
         */

        /* const */ bool m_is_reverse;

        /**
         *  The incoming note number from a non-GM compliant device.  This
         *  value is used as a "key" value in the map or the index in the
         *  array.
         */

        /* const */ int m_dev_value;

        /**
         *    The integer value to which the incoming (key) value is to be
         *    mapped.  This is the value of the drum note on a GM-compliant
         *    device.
         */

        /* const */ int m_gm_value;

        /**
         *  The name of the key as represented by the non-GM device.
         */

        /* const */ std::string m_dev_name;

        /**
         *    The name of the GM drum note or patch that is replacing the
         *    device's drum note of patch.  Sometimes there is no exact
         *    replacement, so it is good to know what GM sound is replacing the
         *    device's sound.
         */

        /* const */ std::string m_gm_name;

        /**
         *    The number of times this particular mapping was performed in the
         *    MIDI remapping operation.
         */

        int m_remap_count;

     public:

        pair () = delete;
        pair
        (
            int devvalue, int gmvalue,
            const std::string & devname,
            const std::string & gmname,
            bool reverse
        );
        pair (const pair &) = default;
        pair & operator = (const pair &) = default;
        ~pair () = default;

        int dev_value () const
        {
           return m_dev_value;
        }

        int gm_value () const
        {
           return m_gm_value;
        }

        const std::string & dev_name () const
        {
           return m_dev_name;
        }

        const std::string & gm_name () const
        {
           return m_gm_name;
        }

        void increment_count ()
        {
           ++m_remap_count;
        }

        int count () const
        {
           return m_remap_count;
        }

        std::string to_string () const;
        void show () const;

     };         // nested class pair

 private:

    /**
     *    Provides the type of the map between one set of values and
     *    another set of values.
     */

    using map = std::map<int, pair>;

 private:

    /**
     *  Indicates if we are in drums mode.  Only true if the user specified
     *  a valid drums (note-mapper) file that was successfully loaded.
     */

    bool m_mode;

    /**
     *    Indicates what kind of mapping is allegedly provided by the file.
     *    This can be one of the following values:
     *
     *       -  "drums".  The file describes mapping one pitch/channel to
     *          another pitch/channel, used mostly for coercing old drum
     *          machines to something akin to a General MIDI kit.
     *       -  "patches".  The file describes program (patch) mappings, used
     *          to map old devices patch change values to General MIDI.
     *          Not yet supported.
     *       -  "multi".  The file describes both "drums" and "patchs"
     *          mappings.  Not yet supported.
     *
     *    The name of this attribute in the INI file is "map-type".  Case
     *    is significant.
     */

    std::string m_map_type;

    /**
     * Provides the lowest and highest notes actually read into the map and
     * array.
     */

    int m_note_minimum;
    int m_note_maximum;

    /**
     *    Provides the channel to use for General MIDI drums.  This value
     *    is usually 9, meaning MIDI channel 10.  However, be careful, as
     *    externally, this value is always on a 1-16 scale, while
     *    internally it is reduced by 1 (a 0-15 scale) to save endless
     *    decrements.
     *
     *    The name of this attribute in the INI file is "gm-channel".  Case
     *    is significant.
     */

    int m_gm_channel;

    /**
     *    Provides the channel that is used by the native device.  Older
     *    MIDI equipment sometimes used channel 16 for percussion.
     *
     *    The name of this attribute in the INI file is "dev-channel".
     *    Case is significant.
     */

    int m_device_channel;

    /**
     *    Indicates that the mapping should occur in the reverse direction.
     *    That is, instead of mapping the notes from the device pitches and
     *    channel to General MIDI, the notes and channel should be mapped
     *    from General MIDI back to the device.  This option is useful for
     *    playing back General MIDI files on old equipment.
     *
     *    Note that this option is an INI option ("reverse"), as well as a
     *    command-line option.  It is specified by alternate means, such as
     *    a command-line parameter like "--reverse".
     */

    bool m_map_reversed;

    /**
     *    Provides the mapping between pitches.  If m_map_reversed is
     *    false, then the key is the GM pitch/note, and the value is the
     *    device pitch/note (which is the GM note needed to produce the
     *    same sound in GM as the device would have produced).  If
     *    m_map_reversed is true, then the key is the GM pitch/note, and
     *    the value is the device pitch/note, so that the MIDI file will be
     *    converted from GM mapping to device mapping.
     */

    map m_note_map;

    /**
     *  Provides a quick translation "map" for use while recording.
     */

    midi::byte m_note_array[c_notes_count];

    /**
     *    Indicates if the setup is valid.
     */

    bool m_is_valid;

 public:

    notemapper ();
    notemapper (const notemapper &) = default;
    notemapper & operator = (const notemapper &) = default;
    ~notemapper () = default;

    int convert (int incoming) const;

    midi::byte fast_convert (midi::byte incoming) const
    {
        return m_note_array[incoming];          /* no check done, for speed */
    }

    std::string to_string (int devnote) const;
    void show () const;

    bool mode () const
    {
        return m_mode;
    }

    void mode (bool m)
    {
        m_mode = m;
    }

    /**
     *    Determines if the value parameter is usable, or "active".
     *
     * \param value
     *    The integer value to be checked.
     *
     * \return
     *    Returns true if the value is not NOT_ACTIVE.
     */

    static bool active (int value)
    {
       return value != notemapper::NOT_ACTIVE;
    }

    /**
     *    Determines if both value parameters are usable, or "active".
     *
     * \param v1
     *    The first integer value to be checked.
     *
     * \param v2
     *    The second integer value to be checked.
     *
     * \return
     *    Returns true if both of the values are not NOT_ACTIVE.
     */

    static bool active (int v1, int v2)
    {
       return
       (
           v1 != notemapper::NOT_ACTIVE &&
           v2 != notemapper::NOT_ACTIVE
       );
    }

    bool add
    (
        int devnote, int gmnote,
        const std::string & devname, const std::string & gmname
    );
    int repitch (int channel, int input);

    const std::string & map_type () const
    {
       return m_map_type;
    }

    int note_minimum () const
    {
       return m_note_minimum;
    }

    int note_maximum () const
    {
       return m_note_maximum;
    }

    int gm_channel () const
    {
       return m_gm_channel + 1;
    }

    int device_channel () const
    {
       return m_device_channel + 1;
    }

    bool valid () const
    {
       return m_is_valid;
    }

    const map & list () const
    {
       return m_note_map;
    }

    bool map_reversed () const
    {
       return m_map_reversed;
    }

public:

    void map_type (const std::string & mp)
    {
        m_map_type = mp;
    }

    void map_reversed (bool flag)
    {
        m_map_reversed = flag;
    }

    void gm_channel (int ch)
    {
       m_gm_channel = ch - 1;
    }

};                  // class notemapper

}                   // namespace seq66

#endif              // RTL66_NOTEMAPPER_HPP

 /*
  * notemapper.hpp
  *
  * vim: sw=4 ts=4 wm=8 et ft=cpp
  */

