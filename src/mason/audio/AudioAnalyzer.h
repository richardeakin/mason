/*
Copyright (c) 2017, libcinder project - All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided
that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

//
//  ============================================================
//  Hotkeys
//  - everything for AudioAnalyzer requires Control to be held down
//
//  - /: toggle context (enables or disables entire audio graph)
//  - m: toggle microphone or sample input
//  - s: test seek
//  - p: print track info
//  - d: toggle draw UI
//  - 1-5: select a Track to be edited
//  - 0: select all tracks to be edited
//  - Up / Down: adjust gain
//
//  ============================================================

#pragma once

#include "cinder/audio/NodeEffects.h"
#include "cinder/audio/SamplePlayerNode.h"
#include "cinder/audio/MonitorNode.h"

#include "cinder/Filesystem.h"
#include "cinder/app/KeyEvent.h"
#include "cinder/Signals.h"

#include "mason/Dictionary.h"

#include <array>

namespace mason { namespace audio {

using AudioAnalyzerRef = std::shared_ptr<class AudioAnalyzer>;

//! Describes one frequency band
struct FrequencyBand {
	int		mIndex;						//! Band index in vector
	float	mFreqLower, mFreqUpper;		// Hertz
	int		mBinLower, mBinUpper;		//! Corresponds to mag spectrum. Upper is exclusive
	float	mMag;						//! Mags are normalized between 0-1, with 1 equal to 100db
}; 

class Track {
public:
	Track( const std::string &id, int trackId );
	~Track();

	//! Mags are currently linear.
	const std::vector<float>& getMagSpectrum() const			{ return mMagSpectrum; }

	void	setGain( float value, double rampSeconds = 0.3 );
	void	incrGain( float amount, double rampSeconds = 0.3 );
	float	getGain() const	{ return mGain->getValue(); }
	void	setSpectrumSmoothingFactor( float x );
	float	getSpectrumSmoothFactor() const;
	void	setVolumeSmoothingFactor( float x )	{ mVolumeSmoothingFactor = x; }
	float	getVolumeSmoothFactor() const	{ return mVolumeSmoothingFactor; }

	//! The Root-mean-square of the time-domain audio signal in decibels, range is 0:100
	float	getVolumeRMS() const;
	//! This is a more usable parameter than getVolumeRMS(), as it scales the audible portion (min mag:100) to a value ranging between 0:1
	float	getLoudness() const;
	//!
	void setMinMagValue( float x )				{ mMinMagValue = x; }
	float getMinMagValue() const	{ return mMinMagValue; }

	//! Returns the centroid of the frequency spectrum, which is the center of mass of all frequency values. Largely correlates with the 'brightness' of a sound.
	float	getSpectralCentroid() const;
	//! Returns the samplerate of this Track
	float	getSampleRate() const;

	//!
	const std::string&	getId() const;
	//!
	int	getIndex() const	{ return mIndex; }

	void setBarkBandsEnabled( bool enable );
	const std::vector<FrequencyBand>&	getFrequencyBands() const	{ return mFreqBands; }



	//!
	bool	isSamplePlayer() const	{ return (bool)mSamplePlayerNode; }
	//!
	double	getSamplePlaybackTime() const;
	//!
	double	getSampleDuration() const;

	ci::audio::GainNodeRef	getGainNode() const	{ return mGain; }
	ci::audio::SamplePlayerNodeRef	getSamplePlayerNode() const	{ return mSamplePlayerNode; }
	ci::audio::MonitorSpectralNodeRef	getMonitorSpectralNode() const	{ return mMonitorSpectralNode; }

	ci::fs::path	getSampleFilePath() const;
	
private:
	void initBarkBands();
	void sumFrequencyBands();
	void update();

	ci::audio::InputNodeRef				mInputNode; // this points to one of the following three nodes, based on inputType:
	ci::audio::FilePlayerNodeRef		mFilePlayerNode; // TODO: do I need FilePlayerNode / BufferPlayerNode around? using SamplePlayerNodeRef instead
	ci::audio::BufferPlayerNodeRef		mBufferPlayerNode;
	ci::audio::SamplePlayerNodeRef		mSamplePlayerNode;
	ci::audio::InputDeviceNodeRef		mInputDeviceNode;

	ci::audio::MonitorSpectralNodeRef	mMonitorSpectralNode;
	ci::audio::GainNodeRef				mGain;
	std::vector<float>					mMagSpectrum;
	std::string							mSampleFileName;
	std::string							mId;
	int									mIndex = -1;
	float								mGainValue = 1;
	float								mVolumeRMS = 0;
	float								mVolumeSmoothingFactor = 0.5;
	float								mMinMagValue = 0; // value to scale audible portion of magnitudes.
	ci::signals::ScopedConnection		mConnAudioFile;

	std::vector<FrequencyBand>			mFreqBands;


	friend class AudioAnalyzer;
};

enum class InputType {
	FILE_PLAYER,
	BUFFER_PLAYER, // TODO: test this one
	INPUT_DEVICE,
	UNKNOWN
};

using TrackRef = std::shared_ptr<mason::audio::Track>;
using AudioAnalyzerRef = std::shared_ptr<class mason::audio::AudioAnalyzer>;

class AudioAnalyzer {
public:

	//! Call whenever configuration changes
	void initialize( const Dictionary &config );
	
	AudioAnalyzer();
	AudioAnalyzer( const AudioAnalyzer& ) = delete;
	AudioAnalyzer& operator=( const AudioAnalyzer& ) = delete;
	~AudioAnalyzer();

	//! Enables audio processing
	void enable();
	//! Disables audio processing
	void disable();
	//! Returns whether audio processing is enabled or not
	bool isEnabled() const;
	//! Toggle audio processing
	void setEnabled( bool b )	{ b ? enable() : disable(); }
	//! Resumes playback for all sample tracks.
	void play();
	//! Pauses playback for all sample tracks. seeking is audible while paused.
	void pause();
	//! Toggles play() / pause.
	void setPaused( bool b )	{ b ? pause() : play(); }
	//!
	bool isPaused() const		{ return mIsPaused; }
	//! Seeks samples to the specified time
	void seek( double time, bool playSnippet = false );
	//! Set the master gain (volume) in normalized (0:1) dB. This only effects what you hear, not what you see.
	void setMasterGain( float value, double rampSeconds = 0.3 );
	//! Returns the master game in normalized (0:1) dB.
	float getMasterGain() const	{ return mMasterGain->getValue(); }

	void update();

	const std::vector<TrackRef>& getTracks() const		{ return mTracks; }
	
	const ma::audio::TrackRef&	getTrack( int index ) const;
		
	ci::signals::Signal<void ()>&	getSignalTracksChanged()	{ return mSignalTracksChanged; }

private:
	void initEntry( const Dictionary &config );
	void initContext( const Dictionary &config );
	void initTracks( const Dictionary &config );
	ci::fs::path resolvePath( const std::string &url );

	void keyDown( ci::app::KeyEvent &event );
	void printInfo();
	
	ci::audio::GainNodeRef			mMasterGain;
	std::vector<TrackRef>			mTracks;
	ci::signals::Signal<void ()>	mSignalTracksChanged;
	bool							mIsPaused = true;
	double							mSeekRampSeconds = 0.02;

	ci::signals::ScopedConnection	mConnKeyDown;

	std::map<std::string, ci::audio::InputDeviceNodeRef>	mSharedInputDeviceNodes; // in the case of multi-channel inputs, share InputDeviceNodes
	std::map<ci::fs::path, std::pair<ci::audio::BufferRef, ci::fs::file_time_type>>	mBufferCache; // only load audio Buffers once until they are updated on file
};

//! Global AudioAnalyzer instance
AudioAnalyzer* analyzer();
//! Sets the global Tracker instance
void setAnalyzerInstance( const std::shared_ptr<AudioAnalyzer> &analyzer );


} } // namepsace mason::audio
