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

#ifndef _REAL_TIME_H_
#define _REAL_TIME_H_

#include <iostream>
#include <string>

struct timeval;


/**
 * RealTime represents time values to nanosecond precision
 * with accurate arithmetic and frame-rate conversion functions.
 */

struct RealTime
{
    int sec;
    int nsec;

    int usec() const { return nsec / 1000; }
    int msec() const { return nsec / 1000000; }

    RealTime(): sec(0), nsec(0) {}
    RealTime(int s, int n);

    RealTime(const RealTime &r) :
	sec(r.sec), nsec(r.nsec) { }

    static RealTime fromSeconds(double sec);
    static RealTime fromMilliseconds(int msec);
    static RealTime fromTimeval(const struct timeval &);
    static RealTime fromXsdDuration(std::string xsdd);

    double toDouble() const;

    RealTime &operator=(const RealTime &r) {
	sec = r.sec; nsec = r.nsec; return *this;
    }

    RealTime operator+(const RealTime &r) const {
	return RealTime(sec + r.sec, nsec + r.nsec);
    }
    RealTime operator-(const RealTime &r) const {
	return RealTime(sec - r.sec, nsec - r.nsec);
    }
    RealTime operator-() const {
	return RealTime(-sec, -nsec);
    }

    bool operator <(const RealTime &r) const {
	if (sec == r.sec) return nsec < r.nsec;
	else return sec < r.sec;
    }

    bool operator >(const RealTime &r) const {
	if (sec == r.sec) return nsec > r.nsec;
	else return sec > r.sec;
    }

    bool operator==(const RealTime &r) const {
        return (sec == r.sec && nsec == r.nsec);
    }
 
    bool operator!=(const RealTime &r) const {
        return !(r == *this);
    }
 
    bool operator>=(const RealTime &r) const {
        if (sec == r.sec) return nsec >= r.nsec;
        else return sec >= r.sec;
    }

    bool operator<=(const RealTime &r) const {
        if (sec == r.sec) return nsec <= r.nsec;
        else return sec <= r.sec;
    }

    RealTime operator*(int m) const;
    RealTime operator/(int d) const;

    RealTime operator*(double m) const;
    RealTime operator/(double d) const;

    /**
     * Return the ratio of two times.
     */
    double operator/(const RealTime &r) const;

    /**
     * Return a human-readable debug-type string to full precision
     * (probably not a format to show to a user directly).  If align
     * is true, prepend " " to the start of positive values so that
     * they line up with negative ones (which start with "-").
     */ 
    std::string toString(bool align = false) const;

    /**
     * Convert a string as obtained from toString back to a RealTime
     * object.
     */
    static RealTime fromString(std::string);

    /**
     * Return a user-readable string to the nearest millisecond, in a
     * form like HH:MM:SS.mmm
     */
    std::string toText(bool fixedDp = false) const;

    /**
     * Return a user-readable string in which seconds are divided into
     * frames (presumably at a lower frame rate than audio rate,
     * e.g. 24 or 25 video frames), in a form like HH:MM:SS:FF.  fps
     * gives the number of frames per second, and must be integral
     * (29.97 not supported).
     */
    std::string toFrameText(int fps) const;

    /**
     * Return a user-readable string to the nearest second, in a form
     * like "6s" (for less than a minute) or "2:21" (for more).
     */
    std::string toSecText() const;

    /**
     * Return a string in xsd:duration format.
     */
    std::string toXsdDuration() const;

    /**
     * Convert a RealTime into a sample frame at the given sample rate.
     */
    static long realTime2Frame(const RealTime &r, unsigned int sampleRate);

    /**
     * Convert a sample frame at the given sample rate into a RealTime.
     */
    static RealTime frame2RealTime(long frame, unsigned int sampleRate);

    static const RealTime zeroTime;
};

std::ostream &operator<<(std::ostream &out, const RealTime &rt);
    
#endif
