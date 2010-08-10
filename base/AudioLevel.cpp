/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2000-2006 Chris Cannam.
*/

#include "AudioLevel.h"
#include <cmath>
#include <iostream>
#include <map>
#include <vector>
#include <cassert>
#include "system/System.h"

const float AudioLevel::DB_FLOOR = -1000.f;

struct FaderDescription
{
    FaderDescription(float _minDb, float _maxDb, float _zeroPoint) :
	minDb(_minDb), maxDb(_maxDb), zeroPoint(_zeroPoint) { }

    float minDb;
    float maxDb;
    float zeroPoint; // as fraction of total throw
};

static const FaderDescription faderTypes[] = {
    FaderDescription(-40.f,  +6.f, 0.75f), // short
    FaderDescription(-70.f, +10.f, 0.80f), // long
    FaderDescription(-70.f,   0.f, 1.00f), // IEC268
    FaderDescription(-70.f, +10.f, 0.80f), // IEC268 long
    FaderDescription(-40.f,   0.f, 1.00f), // preview
};

//typedef std::vector<float> LevelList;
//static std::map<int, LevelList> previewLevelCache;
//static const LevelList &getPreviewLevelCache(int levels);

float
AudioLevel::multiplier_to_dB(float multiplier)
{
    if (multiplier == 0.f) return DB_FLOOR;
    else if (multiplier < 0.f) return multiplier_to_dB(-multiplier);
    float dB = 10 * log10f(multiplier);
    return dB;
}

float
AudioLevel::dB_to_multiplier(float dB)
{
    if (dB == DB_FLOOR) return 0.f;
    float m = powf(10.f, dB / 10.f);
    return m;
}

/* IEC 60-268-18 fader levels.  Thanks to Steve Harris. */

static float iec_dB_to_fader(float db)
{
    float def = 0.0f; // Meter deflection %age

    if (db < -70.0f) {
        def = 0.0f;
    } else if (db < -60.0f) {
        def = (db + 70.0f) * 0.25f;
    } else if (db < -50.0f) {
        def = (db + 60.0f) * 0.5f + 5.0f;
    } else if (db < -40.0f) {
        def = (db + 50.0f) * 0.75f + 7.5f;
    } else if (db < -30.0f) {
        def = (db + 40.0f) * 1.5f + 15.0f;
    } else if (db < -20.0f) {
        def = (db + 30.0f) * 2.0f + 30.0f;
    } else {
        def = (db + 20.0f) * 2.5f + 50.0f;
    }

    return def;
}

static float iec_fader_to_dB(float def)  // Meter deflection %age
{
    float db = 0.0f;

    if (def >= 50.0f) {
	db = (def - 50.0f) / 2.5f - 20.0f;
    } else if (def >= 30.0f) {
	db = (def - 30.0f) / 2.0f - 30.0f;
    } else if (def >= 15.0f) {
	db = (def - 15.0f) / 1.5f - 40.0f;
    } else if (def >= 7.5f) {
	db = (def - 7.5f) / 0.75f - 50.0f;
    } else if (def >= 5.0f) {
	db = (def - 5.0f) / 0.5f - 60.0f;
    } else {
	db = (def / 0.25f) - 70.0f;
    }

    return db;
}

float
AudioLevel::fader_to_dB(int level, int maxLevel, FaderType type)
{
    if (level == 0) return DB_FLOOR;

    if (type == IEC268Meter || type == IEC268LongMeter) {

	float maxPercent = iec_dB_to_fader(faderTypes[type].maxDb);
	float percent = float(level) * maxPercent / float(maxLevel);
	float dB = iec_fader_to_dB(percent);
	return dB;

    } else { // scale proportional to sqrt(fabs(dB))

	int zeroLevel = int(maxLevel * faderTypes[type].zeroPoint);
    
	if (level >= zeroLevel) {
	    
	    float value = level - zeroLevel;
	    float scale = float(maxLevel - zeroLevel) /
		sqrtf(faderTypes[type].maxDb);
	    value /= scale;
	    float dB = powf(value, 2.f);
	    return dB;
	    
	} else {
	    
	    float value = zeroLevel - level;
	    float scale = zeroLevel / sqrtf(0.f - faderTypes[type].minDb);
	    value /= scale;
	    float dB = powf(value, 2.f);
	    return 0.f - dB;
	}
    }
}


int
AudioLevel::dB_to_fader(float dB, int maxLevel, FaderType type)
{
    if (dB == DB_FLOOR) return 0;

    if (type == IEC268Meter || type == IEC268LongMeter) {

	// The IEC scale gives a "percentage travel" for a given dB
	// level, but it reaches 100% at 0dB.  So we want to treat the
	// result not as a percentage, but as a scale between 0 and
	// whatever the "percentage" for our (possibly >0dB) max dB is.
	
	float maxPercent = iec_dB_to_fader(faderTypes[type].maxDb);
	float percent = iec_dB_to_fader(dB);
	int faderLevel = int((maxLevel * percent) / maxPercent + 0.01f);
	
	if (faderLevel < 0) faderLevel = 0;
	if (faderLevel > maxLevel) faderLevel = maxLevel;
	return faderLevel;

    } else {

	int zeroLevel = int(maxLevel * faderTypes[type].zeroPoint);

	if (dB >= 0.f) {
	    
            if (faderTypes[type].maxDb <= 0.f) {
                
                return maxLevel;

            } else {

                float value = sqrtf(dB);
                float scale = (maxLevel - zeroLevel) / sqrtf(faderTypes[type].maxDb);
                value *= scale;
                int level = int(value + 0.01f) + zeroLevel;
                if (level > maxLevel) level = maxLevel;
                return level;
            }
	    
	} else {

	    dB = 0.f - dB;
	    float value = sqrtf(dB);
	    float scale = zeroLevel / sqrtf(0.f - faderTypes[type].minDb);
	    value *= scale;
	    int level = zeroLevel - int(value + 0.01f);
	    if (level < 0) level = 0;
	    return level;
	}
    }
}

	
float
AudioLevel::fader_to_multiplier(int level, int maxLevel, FaderType type)
{
    if (level == 0) return 0.f;
    return dB_to_multiplier(fader_to_dB(level, maxLevel, type));
}

int
AudioLevel::multiplier_to_fader(float multiplier, int maxLevel, FaderType type)
{
    if (multiplier == 0.f) return 0;
    float dB = multiplier_to_dB(multiplier);
    int fader = dB_to_fader(dB, maxLevel, type);
    return fader;
}

/*
const LevelList &
getPreviewLevelCache(int levels)
{
    LevelList &ll = previewLevelCache[levels];
    if (ll.empty()) {
	for (int i = 0; i <= levels; ++i) {
	    float m = AudioLevel::fader_to_multiplier
		(i + levels/4, levels + levels/4, AudioLevel::PreviewLevel);
	    if (levels == 1) m /= 100; // noise
	    ll.push_back(m);
	}
    }
    return ll;
}
*/

int
AudioLevel::multiplier_to_preview(float m, int levels)
{
    assert(levels > 0);
    return multiplier_to_fader(m, levels, PreviewLevel);

    /* The original multiplier_to_preview which follows is not thread-safe.

    if (m < 0.f) return -multiplier_to_preview(-m, levels);

    const LevelList &ll = getPreviewLevelCache(levels);
    int result = -1;

    int lo = 0, hi = levels;

    // binary search
    int level = -1;
    while (result < 0) {
	int newlevel = (lo + hi) / 2;
	if (newlevel == level ||
	    newlevel == 0 ||
	    newlevel == levels) {
	    result = newlevel;
	    break;
	}
	level = newlevel;
	if (ll[level] >= m) {
	    hi = level;
	} else if (ll[level+1] >= m) {
	    result = level;
	} else {
	    lo = level;
	}
    }
		   
    return result;

    */
}

float
AudioLevel::preview_to_multiplier(int level, int levels)
{
    assert(levels > 0);
    return fader_to_multiplier(level, levels, PreviewLevel);
/*
    if (level < 0) return -preview_to_multiplier(-level, levels);
    const LevelList &ll = getPreviewLevelCache(levels);
    return ll[level];
*/
}
	

