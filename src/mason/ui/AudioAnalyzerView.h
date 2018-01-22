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

#pragma once

#pragma optimize( "", off ) // TODO: make this a common project wide macro

#include "ui/View.h"
#include "mason/Mason.h"
#include "mason/Dictionary.h"
#include "mason/LUT.h"
#include "mason/audio/AudioAnalyzer.h"

#if defined( CINDER_RUNTIME_PRESENT )
#include "runtime/Factory.h"
#include "runtime/Virtual.h"
#else
#define rt_virtual 
#endif

namespace mason { namespace audio {

using AudioAnalyzerRef = std::shared_ptr<class AudioAnalyzer>;
using TrackRef = std::shared_ptr<class Track>;

} } // namespace mason::audio

// note: using namespace mason::mui so as not to conflict with ui::*. Oh well..
// TODO: move files to mui folder
namespace mason { namespace mui {

using AudioAnalyzerViewRef		= std::shared_ptr<class AudioAnalyzerView>;
using AudioTrackViewRef			= std::shared_ptr<class AudioTrackView>;
using AudioSpectrumViewRef		= std::shared_ptr<class AudioSpectrumView>;
using AudioSpectrogramViewRef	= std::shared_ptr<class AudioSpectrogramView>;
using AudioBarkBandsViewRef		= std::shared_ptr<class AudioFrequencyBandsView>;

class AudioAnalyzerView : public ::ui::View {
	//RT_DECL
  public:
	AudioAnalyzerView( const ci::Rectf &bounds = ci::Rectf::zero() );
	~AudioAnalyzerView();

	rt_virtual void load( const ma::Dictionary &config );

	rt_virtual void setup();

  protected:

	void layout() override;
	void update() override;
	void draw( ::ui::Renderer *ren ) override;

  private:
	void reloadConfig();

	ma::Dictionary	mLastConfig;

	std::vector<AudioTrackViewRef>			mTrackViews;
	ci::signals::ConnectionList				mConnections;

	friend class AudioTrackView;
};

class AudioTrackView : public ::ui::View {
  public:
	AudioTrackView( AudioAnalyzerView *parent, int trackIndex, const ci::Rectf &bounds = ci::Rectf::zero() );
	~AudioTrackView();

  protected:
	void layout() override;
	void update() override;
	void draw( ::ui::Renderer *ren ) override;

  private:
	const std::string&	getTitle( const audio::TrackRef &track );

	AudioAnalyzerView*		mParent = nullptr;
	int						mTrackIndex = -1;
	std::string				mTitle;

	AudioSpectrogramViewRef	mSpectrogramView;
	AudioSpectrumViewRef	mSpectrumView;
	AudioBarkBandsViewRef	mFreqBandsView;

	friend class AudioAnalyzerView;
	friend class AudioSpectrogramView;
};

//! Shows one frame of the magnitude spectrum
class AudioSpectrumView : public ::ui::View {
  public:
	AudioSpectrumView( const ci::Rectf &bounds = ci::Rectf::zero() );

	void enableScaleDecibels( bool b = true )	{ mScaleDecibels = b; }
	bool getScaleDecibels() const				{ return mScaleDecibels; }

	void enableBorder( bool b = true )			{ mBorderEnabled = b; }
	bool getBorderEnabled() const				{ return mBorderEnabled; }

	void setBorderColor( const ci::ColorA &color )	{ mBorderColor = color; }
	const ci::ColorA& getBorderColor() const		{ return mBorderColor; }

	void setMinMagValue( float x )				{ mMinMagValue = x; }

	void updateData( const audio::TrackRef &track );

  private:
	void draw( ::ui::Renderer *ren ) override;

  private:
	void initGl();

	bool					mScaleDecibels = true;
	bool					mBorderEnabled = true;
	bool					mDrawSpectralCentroid = true;
	bool					mDrawValueAtMouse = true;
	float					mMinMagValue = 0;
	float					mMaxSpectrumFreq = -1; // defaults to nyquist, can be set to something lower in config
	size_t					mMaxSpectrumBin; // can be set to lower than mMagSpectrum.size() by setting maxSpectrumFreq in config
	ci::ColorA				mBorderColor = ci::ColorA( 0.5f, 0.5f, 0.5f, 1 );
	std::vector<float>		mMagSpectrum;
	float					mSpectralCentroidNormalized = -1;

	int						mBinIndexUnderMouse = -1;
	float					mFreqUnderMouse = 0;

	friend class AudioAnalyzerView;
};

//! Shows the magnitude spectrum of an entire audio::Source
class AudioSpectrogramView : public ::ui::View {
  public:
	AudioSpectrogramView( int trackIndex, const ci::Rectf &bounds = ci::Rectf::zero() );

	void load( const audio::TrackRef &track );

	void update( const audio::TrackRef &track );

  protected:
	void draw( ::ui::Renderer *ren ) override;
	bool touchesBegan( ci::app::TouchEvent &event )	override;
	bool touchesMoved( ci::app::TouchEvent &event )	override;


  private:
	void makeTex();
	bool readSTFTFromDisk( const ci::fs::path &sampleName );
	void writeSTFTToDisk( const ci::fs::path &sampleName );
	void setTrackPos( float x );

	std::vector<std::vector<float>>	mSTFT; // actual analysis data
	ci::gl::TextureRef				mTex;
	ma::ColorLUT<ci::ColorAf>		mLUT;

	bool				mDrawValueAtMouse = true;
	ci::ivec2			mBinUnderMouse = { -1, -1 };

	float				mTrackTimePercent = 0;
	int					mTrackIndex = -1;

	bool				mBorderEnabled = true;
	ci::ColorA			mBorderColor = ci::ColorA( 0.5f, 0.5f, 0.5f, 1 );
};

//! Visualizes a Track's Frequency Bands. TODO: make this a generic BarGraphView or something.
class AudioFrequencyBandsView : public ::ui::View {
public:
	AudioFrequencyBandsView( const ci::Rectf &bounds = ci::Rectf::zero() );

	void enableScaleDecibels( bool b = true )	{ mScaleDecibels = b; }
	bool getScaleDecibels() const				{ return mScaleDecibels; }

	void enableBorder( bool b = true )			{ mBorderEnabled = b; }
	bool getBorderEnabled() const				{ return mBorderEnabled; }

	void setBorderColor( const ci::ColorA &color )	{ mBorderColor = color; }
	const ci::ColorA& getBorderColor() const		{ return mBorderColor; }


	void updateData( const audio::TrackRef &track );

private:
	void draw( ::ui::Renderer *ren ) override;

private:
	void initGl();
	
	bool					mScaleDecibels = true;
	bool					mBorderEnabled = true;
	bool					mDrawValueAtMouse = true;
	bool					mAlignBandsToSpectrum = false;
	float					mMaxSpectrumFreq = -1; // defaults to nyquist, can be set to something lower in config
	ci::ColorA				mBorderColor = ci::ColorA( 0.5f, 0.5f, 0.5f, 1 );

	std::vector<ma::audio::FrequencyBand>		mBands;

	int mBandIndexUnderMouse = -1;

	friend class AudioAnalyzerView;
};

} } // namespace mason::mui
