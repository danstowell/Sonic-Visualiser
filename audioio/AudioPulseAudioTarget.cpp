/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifdef HAVE_LIBPULSE

#include "AudioPulseAudioTarget.h"
#include "AudioCallbackPlaySource.h"

#include <QMutexLocker>

#include <iostream>
#include <cassert>
#include <cmath>

#define DEBUG_AUDIO_PULSE_AUDIO_TARGET 1
//#define DEBUG_AUDIO_PULSE_AUDIO_TARGET_PLAY 1

AudioPulseAudioTarget::AudioPulseAudioTarget(AudioCallbackPlaySource *source) :
    AudioCallbackPlayTarget(source),
    m_mutex(QMutex::Recursive),
    m_loop(0),
    m_api(0),
    m_context(0),
    m_stream(0),
    m_loopThread(0),
    m_bufferSize(0),
    m_sampleRate(0),
    m_latency(0),
    m_done(false)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget: Initialising for PulseAudio" << std::endl;
#endif

    m_loop = pa_mainloop_new();
    if (!m_loop) {
        std::cerr << "ERROR: AudioPulseAudioTarget: Failed to create main loop" << std::endl;
        return;
    }

    m_api = pa_mainloop_get_api(m_loop);

    //!!! handle signals how?

    m_bufferSize = 20480;
    m_sampleRate = 44100;
    if (m_source && (m_source->getSourceSampleRate() != 0)) {
	m_sampleRate = m_source->getSourceSampleRate();
    }
    m_spec.rate = m_sampleRate;
    m_spec.channels = 2;
    m_spec.format = PA_SAMPLE_FLOAT32NE;

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget: Creating context" << std::endl;
#endif

    m_context = pa_context_new(m_api, source->getClientName().toLocal8Bit().data());
    if (!m_context) {
        std::cerr << "ERROR: AudioPulseAudioTarget: Failed to create context object" << std::endl;
        return;
    }

    pa_context_set_state_callback(m_context, contextStateChangedStatic, this);

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget: Connecting to default server..." << std::endl;
#endif

    pa_context_connect(m_context, 0, // default server
                       (pa_context_flags_t)PA_CONTEXT_NOAUTOSPAWN, 0);

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget: Starting main loop" << std::endl;
#endif

    m_loopThread = new MainLoopThread(m_loop);
    m_loopThread->start();

#ifdef DEBUG_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget: initialised OK" << std::endl;
#endif
}

AudioPulseAudioTarget::~AudioPulseAudioTarget()
{
    std::cerr << "AudioPulseAudioTarget::~AudioPulseAudioTarget()" << std::endl;

    if (m_source) {
        m_source->setTarget(0, m_bufferSize);
    }

    shutdown();

    QMutexLocker locker(&m_mutex);

    if (m_stream) pa_stream_unref(m_stream);

    if (m_context) pa_context_unref(m_context);

    if (m_loop) {
        pa_signal_done();
        pa_mainloop_free(m_loop);
    }

    m_stream = 0;
    m_context = 0;
    m_loop = 0;

    std::cerr << "AudioPulseAudioTarget::~AudioPulseAudioTarget() done" << std::endl;
}

void 
AudioPulseAudioTarget::shutdown()
{
    m_done = true;
}

bool
AudioPulseAudioTarget::isOK() const
{
    return (m_context != 0);
}

double
AudioPulseAudioTarget::getCurrentTime() const
{
    if (!m_stream) return 0.0;
    
    pa_usec_t usec = 0;
    pa_stream_get_time(m_stream, &usec);
    return usec / 1000000.f;
}

void
AudioPulseAudioTarget::sourceModelReplaced()
{
    m_source->setTargetSampleRate(m_sampleRate);
}

void
AudioPulseAudioTarget::streamWriteStatic(pa_stream *stream,
                                         size_t length,
                                         void *data)
{
    AudioPulseAudioTarget *target = (AudioPulseAudioTarget *)data;
    
    assert(stream == target->m_stream);

    target->streamWrite(length);
}

void
AudioPulseAudioTarget::streamWrite(size_t requested)
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET_PLAY
    std::cout << "AudioPulseAudioTarget::streamWrite(" << requested << ")" << std::endl;
#endif
    if (m_done) return;

    QMutexLocker locker(&m_mutex);

    pa_usec_t latency = 0;
    int negative = 0;
    if (!pa_stream_get_latency(m_stream, &latency, &negative)) {
        int latframes = (latency / 1000000.f) * float(m_sampleRate);
        if (latframes > 0) m_source->setTargetPlayLatency(latframes);
    }

    static float *output = 0;
    static float **tmpbuf = 0;
    static size_t tmpbufch = 0;
    static size_t tmpbufsz = 0;

    size_t sourceChannels = m_source->getSourceChannelCount();

    // Because we offer pan, we always want at least 2 channels
    if (sourceChannels < 2) sourceChannels = 2;

    size_t nframes = requested / (sourceChannels * sizeof(float));

    if (nframes > m_bufferSize) {
        std::cerr << "WARNING: AudioPulseAudioTarget::streamWrite: nframes " << nframes << " > m_bufferSize " << m_bufferSize << std::endl;
    }

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET_PLAY
    std::cout << "AudioPulseAudioTarget::streamWrite: nframes = " << nframes << std::endl;
#endif

    if (!tmpbuf || tmpbufch != sourceChannels || int(tmpbufsz) < nframes) {

	if (tmpbuf) {
	    for (size_t i = 0; i < tmpbufch; ++i) {
		delete[] tmpbuf[i];
	    }
	    delete[] tmpbuf;
	}

        if (output) {
            delete[] output;
        }

	tmpbufch = sourceChannels;
	tmpbufsz = nframes;
	tmpbuf = new float *[tmpbufch];

	for (size_t i = 0; i < tmpbufch; ++i) {
	    tmpbuf[i] = new float[tmpbufsz];
	}

        output = new float[tmpbufsz * tmpbufch];
    }
	
    size_t received = m_source->getSourceSamples(nframes, tmpbuf);

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET_PLAY
    std::cerr << "requested " << nframes << ", received " << received << std::endl;

    if (received < nframes) {
        std::cerr << "*** WARNING: Wrong number of frames received" << std::endl;
    }
#endif

    float peakLeft = 0.0, peakRight = 0.0;

    for (size_t ch = 0; ch < 2; ++ch) {
	
	float peak = 0.0;

	if (ch < sourceChannels) {

	    // PulseAudio samples are interleaved
	    for (size_t i = 0; i < nframes; ++i) {
                if (i < received) {
                    output[i * 2 + ch] = tmpbuf[ch][i] * m_outputGain;
                    float sample = fabsf(output[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                } else {
                    output[i * 2 + ch] = 0;
                }
	    }

	} else if (ch == 1 && sourceChannels == 1) {

	    for (size_t i = 0; i < nframes; ++i) {
                if (i < received) {
                    output[i * 2 + ch] = tmpbuf[0][i] * m_outputGain;
                    float sample = fabsf(output[i * 2 + ch]);
                    if (sample > peak) peak = sample;
                } else {
                    output[i * 2 + ch] = 0;
                }
	    }

	} else {
	    for (size_t i = 0; i < nframes; ++i) {
		output[i * 2 + ch] = 0;
	    }
	}

	if (ch == 0) peakLeft = peak;
	if (ch > 0 || sourceChannels == 1) peakRight = peak;
    }

#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET_PLAY
    std::cerr << "calling pa_stream_write with "
              << nframes * tmpbufch * sizeof(float) << " bytes" << std::endl;
#endif

    pa_stream_write(m_stream, output, nframes * tmpbufch * sizeof(float),
                    0, 0, PA_SEEK_RELATIVE);

    m_source->setOutputLevels(peakLeft, peakRight);

    return;
}

void
AudioPulseAudioTarget::streamStateChangedStatic(pa_stream *stream,
                                                void *data)
{
    AudioPulseAudioTarget *target = (AudioPulseAudioTarget *)data;
    
    assert(stream == target->m_stream);

    target->streamStateChanged();
}

void
AudioPulseAudioTarget::streamStateChanged()
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget::streamStateChanged" << std::endl;
#endif
    QMutexLocker locker(&m_mutex);

    switch (pa_stream_get_state(m_stream)) {

        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;

        case PA_STREAM_READY:
        {
            std::cerr << "AudioPulseAudioTarget::streamStateChanged: Ready" << std::endl;

            pa_usec_t latency = 0;
            int negative = 0;
            if (pa_stream_get_latency(m_stream, &latency, &negative)) {
                std::cerr << "AudioPulseAudioTarget::streamStateChanged: Failed to query latency" << std::endl;
            }
            std::cerr << "Latency = " << latency << " usec" << std::endl;
            int latframes = (latency / 1000000.f) * float(m_sampleRate);
            std::cerr << "that's " << latframes << " frames" << std::endl;

            const pa_buffer_attr *attr;
            if (!(attr = pa_stream_get_buffer_attr(m_stream))) {
                std::cerr << "AudioPulseAudioTarget::streamStateChanged: Cannot query stream buffer attributes" << std::endl;
                m_source->setTarget(this, m_bufferSize);
                m_source->setTargetSampleRate(m_sampleRate);
                if (latframes != 0) m_source->setTargetPlayLatency(latframes);
            } else {
                int targetLength = attr->tlength;
                std::cerr << "AudioPulseAudioTarget::streamStateChanged: stream target length = " << targetLength << std::endl;
                m_source->setTarget(this, targetLength);
                m_source->setTargetSampleRate(m_sampleRate);
                if (latframes == 0) latframes = targetLength;
                std::cerr << "latency = " << latframes << std::endl;
                m_source->setTargetPlayLatency(latframes);
            }
        }
            break;

        case PA_STREAM_FAILED:
        default:
            std::cerr << "AudioPulseAudioTarget::streamStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << std::endl;
            //!!! do something...
            break;
    }
}

void
AudioPulseAudioTarget::contextStateChangedStatic(pa_context *context,
                                                 void *data)
{
    AudioPulseAudioTarget *target = (AudioPulseAudioTarget *)data;
    
    assert(context == target->m_context);

    target->contextStateChanged();
}

void
AudioPulseAudioTarget::contextStateChanged()
{
#ifdef DEBUG_AUDIO_PULSE_AUDIO_TARGET
    std::cerr << "AudioPulseAudioTarget::contextStateChanged" << std::endl;
#endif
    QMutexLocker locker(&m_mutex);

    switch (pa_context_get_state(m_context)) {

        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            std::cerr << "AudioPulseAudioTarget::contextStateChanged: Ready"
                      << std::endl;

            m_stream = pa_stream_new(m_context, "stream", &m_spec, 0);
            assert(m_stream); //!!!
            
            pa_stream_set_state_callback(m_stream, streamStateChangedStatic, this);
            pa_stream_set_write_callback(m_stream, streamWriteStatic, this);
            pa_stream_set_overflow_callback(m_stream, streamOverflowStatic, this);
            pa_stream_set_underflow_callback(m_stream, streamUnderflowStatic, this);
            if (pa_stream_connect_playback
                (m_stream, 0, 0,
                 pa_stream_flags_t(PA_STREAM_INTERPOLATE_TIMING |
                                   PA_STREAM_AUTO_TIMING_UPDATE),
                 0, 0)) { //??? return value
                std::cerr << "AudioPulseAudioTarget: Failed to connect playback stream" << std::endl;
            }

            break;

        case PA_CONTEXT_TERMINATED:
            std::cerr << "AudioPulseAudioTarget::contextStateChanged: Terminated" << std::endl;
            //!!! do something...
            break;

        case PA_CONTEXT_FAILED:
        default:
            std::cerr << "AudioPulseAudioTarget::contextStateChanged: Error: "
                      << pa_strerror(pa_context_errno(m_context)) << std::endl;
            //!!! do something...
            break;
    }
}

void
AudioPulseAudioTarget::streamOverflowStatic(pa_stream *, void *)
{
    std::cerr << "AudioPulseAudioTarget::streamOverflowStatic: Overflow!" << std::endl;
}

void
AudioPulseAudioTarget::streamUnderflowStatic(pa_stream *, void *data)
{
    std::cerr << "AudioPulseAudioTarget::streamUnderflowStatic: Underflow!" << std::endl;
    AudioPulseAudioTarget *target = (AudioPulseAudioTarget *)data;
    if (target && target->m_source) {
        target->m_source->audioProcessingOverload();
    }
}

#endif /* HAVE_PULSEAUDIO */

