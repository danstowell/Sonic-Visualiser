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

#include "Pitch.h"
#include "Preferences.h"
#include "system/System.h"

#include <cmath>

float
Pitch::getFrequencyForPitch(int midiPitch,
			    float centsOffset,
			    float concertA)
{
    if (concertA <= 0.0) {
        concertA = Preferences::getInstance()->getTuningFrequency();
    }
    float p = float(midiPitch) + (centsOffset / 100);
    return concertA * powf(2.0, (p - 69.0) / 12.0);
}

int
Pitch::getPitchForFrequency(float frequency,
			    float *centsOffsetReturn,
			    float concertA)
{
    if (concertA <= 0.0) {
        concertA = Preferences::getInstance()->getTuningFrequency();
    }
    float p = 12.0 * (log(frequency / (concertA / 2.0)) / log(2.0)) + 57.0;

    int midiPitch = int(p + 0.00001);
    float centsOffset = (p - midiPitch) * 100.0;

    if (centsOffset >= 50.0) {
	midiPitch = midiPitch + 1;
	centsOffset = -(100.0 - centsOffset);
    }
    
    if (centsOffsetReturn) *centsOffsetReturn = centsOffset;
    return midiPitch;
}

int
Pitch::getPitchForFrequencyDifference(float frequencyA,
                                      float frequencyB,
                                      float *centsOffsetReturn,
                                      float concertA)
{
    if (concertA <= 0.0) {
        concertA = Preferences::getInstance()->getTuningFrequency();
    }

    if (frequencyA > frequencyB) {
        std::swap(frequencyA, frequencyB);
    }

    float pA = 12.0 * (log(frequencyA / (concertA / 2.0)) / log(2.0)) + 57.0;
    float pB = 12.0 * (log(frequencyB / (concertA / 2.0)) / log(2.0)) + 57.0;

    float p = pB - pA;

    int midiPitch = int(p + 0.00001);
    float centsOffset = (p - midiPitch) * 100.0;

    if (centsOffset >= 50.0) {
	midiPitch = midiPitch + 1;
	centsOffset = -(100.0 - centsOffset);
    }
    
    if (centsOffsetReturn) *centsOffsetReturn = centsOffset;
    return midiPitch;
}

static QString notes[] = {
    "C%1",  "C#%1", "D%1",  "D#%1",
    "E%1",  "F%1",  "F#%1", "G%1",
    "G#%1", "A%1",  "A#%1", "B%1"
};

static QString flatNotes[] = {
    "C%1",  "Db%1", "D%1",  "Eb%1",
    "E%1",  "F%1",  "Gb%1", "G%1",
    "Ab%1", "A%1",  "Bb%1", "B%1"
};

QString
Pitch::getPitchLabel(int midiPitch,
		     float centsOffset,
		     bool useFlats)
{
    int octave = -2;

    if (midiPitch < 0) {
	while (midiPitch < 0) {
	    midiPitch += 12;
	    --octave;
	}
    } else {
	octave = midiPitch / 12 - 2;
    }

    QString plain = (useFlats ? flatNotes : notes)[midiPitch % 12].arg(octave);

    int ic = lrintf(centsOffset);
    if (ic == 0) return plain;
    else if (ic > 0) return QString("%1+%2c").arg(plain).arg(ic);
    else return QString("%1%2c").arg(plain).arg(ic);
}

QString
Pitch::getPitchLabelForFrequency(float frequency,
				 float concertA,
				 bool useFlats)
{
    if (concertA <= 0.0) {
        concertA = Preferences::getInstance()->getTuningFrequency();
    }
    float centsOffset = 0.0;
    int midiPitch = getPitchForFrequency(frequency, &centsOffset, concertA);
    return getPitchLabel(midiPitch, centsOffset, useFlats);
}

QString
Pitch::getLabelForPitchRange(int semis, float cents)
{
    if (semis > 0) {
        while (cents < 0.f) {
            --semis;
            cents += 100.f;
        }
    }
    if (semis < 0) {
        while (cents > 0.f) {
            ++semis;
            cents -= 100.f;
        }
    }

    int ic = lrintf(cents);

    if (ic == 0) {
        if (semis >= 12) {
            return QString("%1'%2").arg(semis/12).arg(semis - 12*(semis/12));
        } else {
            return QString("%1").arg(semis);
        }
    } else {
        if (ic > 0) {
            if (semis >= 12) {
                return QString("%1'%2+%3c").arg(semis/12).arg(semis - 12*(semis/12)).arg(ic);
            } else {
                return QString("%1+%3c").arg(semis).arg(ic);
            }
        } else {
            if (semis >= 12) {
                return QString("%1'%2%3c").arg(semis/12).arg(semis - 12*(semis/12)).arg(ic);
            } else {
                return QString("%1%3c").arg(semis).arg(ic);
            }
        }
    }
}

bool
Pitch::isFrequencyInMidiRange(float frequency,
                              float concertA)
{
    float centsOffset = 0.0;
    int midiPitch = getPitchForFrequency(frequency, &centsOffset, concertA);
    return (midiPitch >= 0 && midiPitch < 128);
}

