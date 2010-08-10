/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _PITCH_H_
#define _PITCH_H_

#include <QString>

class Pitch
{
public:
    /**
     * Return the frequency at the given MIDI pitch plus centsOffset
     * cents (1/100ths of a semitone).  centsOffset does not have to
     * be in any particular range or sign.
     *
     * If concertA is non-zero, use that as the reference frequency
     * for the A at MIDI pitch 69; otherwise use the tuning frequency
     * specified in the application preferences (default 440Hz).
     */
    static float getFrequencyForPitch(int midiPitch,
				      float centsOffset = 0,
				      float concertA = 0.0);

    /**
     * Return the nearest MIDI pitch to the given frequency.
     *
     * If centsOffsetReturn is non-NULL, return in *centsOffsetReturn
     * the number of cents (1/100ths of a semitone) difference between
     * the given frequency and that of the returned MIDI pitch.  The
     * cents offset will be in the range [-50,50).
     * 
     * If concertA is non-zero, use that as the reference frequency
     * for the A at MIDI pitch 69; otherwise use the tuning frequency
     * specified in the application preferences (default 440Hz).
     */
    static int getPitchForFrequency(float frequency,
				    float *centsOffsetReturn = 0,
				    float concertA = 0.0);

    /**
     * Return the nearest MIDI pitch range to the given frequency
     * range, that is, the difference in MIDI pitch values between the
     * higher and lower frequencies.
     *
     * If centsOffsetReturn is non-NULL, return in *centsOffsetReturn
     * the number of cents (1/100ths of a semitone) difference between
     * the given frequency difference and the returned MIDI pitch
     * range.  The cents offset will be in the range [-50,50).
     * 
     * If concertA is non-zero, use that as the reference frequency
     * for the A at MIDI pitch 69; otherwise use the tuning frequency
     * specified in the application preferences (default 440Hz).
     */
    static int getPitchForFrequencyDifference(float frequencyA,
                                              float frequencyB,
                                              float *centsOffsetReturn = 0,
                                              float concertA = 0.0);

    /**
     * Return a string describing the given MIDI pitch, with optional
     * cents offset.  This consists of the note name, octave number
     * (with MIDI pitch 0 having octave number -2), and optional
     * cents.
     *
     * For example, "A#3" (A# in octave 3) or "C2-12c" (C in octave 2,
     * minus 12 cents).
     *
     * If useFlats is true, spell notes with flats instead of sharps,
     * e.g. Bb3 instead of A#3.
     */
    static QString getPitchLabel(int midiPitch,
				 float centsOffset = 0,
				 bool useFlats = false);

    /**
     * Return a string describing the nearest MIDI pitch to the given
     * frequency, with cents offset.
     *
     * If concertA is non-zero, use that as the reference frequency
     * for the A at MIDI pitch 69; otherwise use the tuning frequency
     * specified in the application preferences (default 440Hz).
     *
     * If useFlats is true, spell notes with flats instead of sharps,
     * e.g. Bb3 instead of A#3.
     */
    static QString getPitchLabelForFrequency(float frequency,
					     float concertA = 0.0,
					     bool useFlats = false);

    /**
     * Return a string describing the given pitch range in octaves,
     * semitones and cents.  This is in the form e.g. "1'2+4c".
     */
    static QString getLabelForPitchRange(int semis, float cents = 0);

    /**
     * Return true if the given frequency falls within the range of
     * MIDI note pitches, plus or minus half a semitone.  This is
     * equivalent to testing whether getPitchForFrequency returns a
     * pitch in the MIDI range (0 to 127 inclusive) with any cents
     * offset.
     *
     * If concertA is non-zero, use that as the reference frequency
     * for the A at MIDI pitch 69; otherwise use the tuning frequency
     * specified in the application preferences (default 440Hz).
     */
    static bool isFrequencyInMidiRange(float frequency,
                                       float concertA = 0.0);
};


#endif
