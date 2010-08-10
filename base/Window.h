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

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <cmath>
#include <iostream>
#include <string>
#include <map>
#include <cstdlib>

#include <cstdlib>

enum WindowType {
    RectangularWindow,
    BartlettWindow,
    HammingWindow,
    HanningWindow,
    BlackmanWindow,
    GaussianWindow,
    ParzenWindow,
    NuttallWindow,
    BlackmanHarrisWindow
};

template <typename T>
class Window
{
public:
    /**
     * Construct a windower of the given type.
     */
    Window(WindowType type, size_t size) : m_type(type), m_size(size) { encache(); }
    Window(const Window &w) : m_type(w.m_type), m_size(w.m_size) { encache(); }
    Window &operator=(const Window &w) {
	if (&w == this) return *this;
	m_type = w.m_type;
	m_size = w.m_size;
	encache();
	return *this;
    }
    virtual ~Window() { delete[] m_cache; }
    
    void cut(T *src) const { cut(src, src); }
    void cut(T *src, T *dst) const {
	for (size_t i = 0; i < m_size; ++i) dst[i] = src[i] * m_cache[i];
    }

    T getArea() { return m_area; }
    T getValue(size_t i) { return m_cache[i]; }

    WindowType getType() const { return m_type; }
    size_t getSize() const { return m_size; }

    // The names used by these functions are un-translated, for use in
    // e.g. XML I/O.  Use Preferences::getPropertyValueLabel if you
    // want translated names for use in the user interface.
    static std::string getNameForType(WindowType type);
    static WindowType getTypeForName(std::string name);

protected:
    WindowType m_type;
    size_t m_size;
    T *m_cache;
    T m_area;
    
    void encache();
    void cosinewin(T *, T, T, T, T);
};

template <typename T>
void Window<T>::encache()
{
    int n = int(m_size);
    T *mult = new T[n];
    int i;
    for (i = 0; i < n; ++i) mult[i] = 1.0;

    switch (m_type) {
		
    case RectangularWindow:
	for (i = 0; i < n; ++i) {
	    mult[i] *= 0.5;
	}
	break;
	    
    case BartlettWindow:
	for (i = 0; i < n/2; ++i) {
	    mult[i] *= (i / T(n/2));
	    mult[i + n/2] *= (1.0 - (i / T(n/2)));
	}
	break;
	    
    case HammingWindow:
        cosinewin(mult, 0.54, 0.46, 0.0, 0.0);
	break;
	    
    case HanningWindow:
        cosinewin(mult, 0.50, 0.50, 0.0, 0.0);
	break;
	    
    case BlackmanWindow:
        cosinewin(mult, 0.42, 0.50, 0.08, 0.0);
	break;
	    
    case GaussianWindow:
	for (i = 0; i < n; ++i) {
            mult[i] *= pow(2, - pow((i - (n-1)/2.0) / ((n-1)/2.0 / 3), 2));
	}
	break;
	    
    case ParzenWindow:
    {
        int N = n-1;
        for (i = 0; i < N/4; ++i) {
            T m = 2 * pow(1.0 - (T(N)/2 - i) / (T(N)/2), 3);
            mult[i] *= m;
            mult[N-i] *= m;
        }
        for (i = N/4; i <= N/2; ++i) {
            int wn = i - N/2;
            T m = 1.0 - 6 * pow(wn / (T(N)/2), 2) * (1.0 - abs(wn) / (T(N)/2));
            mult[i] *= m;
            mult[N-i] *= m;
        }            
        break;
    }

    case NuttallWindow:
        cosinewin(mult, 0.3635819, 0.4891775, 0.1365995, 0.0106411);
	break;

    case BlackmanHarrisWindow:
        cosinewin(mult, 0.35875, 0.48829, 0.14128, 0.01168);
        break;
    }
	
    m_cache = mult;

    m_area = 0;
    for (int i = 0; i < n; ++i) {
        m_area += m_cache[i];
    }
    m_area /= n;
}

template <typename T>
void Window<T>::cosinewin(T *mult, T a0, T a1, T a2, T a3)
{
    int n = int(m_size);
    for (int i = 0; i < n; ++i) {
        mult[i] *= (a0
                    - a1 * cos((2 * M_PI * i) / n)
                    + a2 * cos((4 * M_PI * i) / n)
                    - a3 * cos((6 * M_PI * i) / n));
    }
}

template <typename T>
std::string
Window<T>::getNameForType(WindowType type)
{
    switch (type) {
    case RectangularWindow:    return "rectangular";
    case BartlettWindow:       return "bartlett";
    case HammingWindow:        return "hamming";
    case HanningWindow:        return "hanning";
    case BlackmanWindow:       return "blackman";
    case GaussianWindow:       return "gaussian";
    case ParzenWindow:         return "parzen";
    case NuttallWindow:        return "nuttall";
    case BlackmanHarrisWindow: return "blackman-harris";
    }

    std::cerr << "WARNING: Window::getNameForType: unknown type "
              << type << std::endl;

    return "unknown";
}

template <typename T>
WindowType
Window<T>::getTypeForName(std::string name)
{
    if (name == "rectangular")     return RectangularWindow;
    if (name == "bartlett")        return BartlettWindow;
    if (name == "hamming")         return HammingWindow;
    if (name == "hanning")         return HanningWindow;
    if (name == "blackman")        return BlackmanWindow;
    if (name == "gaussian")        return GaussianWindow;
    if (name == "parzen")          return ParzenWindow;
    if (name == "nuttall")         return NuttallWindow;
    if (name == "blackman-harris") return BlackmanHarrisWindow;

    std::cerr << "WARNING: Window::getTypeForName: unknown name \""
              << name << "\", defaulting to \"hanning\"" << std::endl;

    return HanningWindow;
}

#endif
