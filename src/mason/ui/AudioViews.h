/*
Copyright (c) 2018, libcinder project - All rights reserved.

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

#include "ui/View.h"
#include "ui/Label.h"
#include "mason/Mason.h"
#include "mason/LUT.h"
#include "cinder/audio/Buffer.h"
#include "cinder/gl/VboMesh.h"

// TODO: decouple these views from AudioAnalyzer
#include "mason/audio/AudioAnalyzer.h"

// note: using namespace mason::mui so as not to conflict with ui::*. Oh well..
namespace mason { namespace mui {

using AudioBufferViewRef		= std::shared_ptr<class AudioBufferView>;
using AudioSpectrumViewRef		= std::shared_ptr<class AudioSpectrumView>;
using AudioSpectrogramViewRef	= std::shared_ptr<class AudioSpectrogramView>;
using AudioBarkBandsViewRef		= std::shared_ptr<class AudioFrequencyBandsView>;

//! Displays the entire contents of an audio::Buffer as a waveform plot.
// TODO: support calling load() methods async
class MA_API AudioBufferView : public ::ui::View {
  public:
	AudioBufferView( const ci::Rectf &bounds = ci::Rectf::zero() );

	// TODO: make this overload take float* + count, so it is more generic
	void load( const std::vector<float> &samples, size_t pixelsPerVertex = 2 );

	void load( const ci::audio::BufferRef &buffer, size_t pixelsPerVertex = 2 );

	void setTitle( const std::string &title );

	//void enableScaleDecibels( bool b = true )	{ mScaleDecibels = b; }
	//bool getScaleDecibels() const				{ return mScaleDecibels; }

	void setBorderEnabled( bool b = true )			{ mBorderEnabled = b; }
	bool getBorderEnabled() const					{ return mBorderEnabled; }

	void setBorderColor( const ci::ColorA &color )	{ mBorderColor = color; }
	const ci::ColorA& getBorderColor() const		{ return mBorderColor; }

  private:
	void layout() override;
	void update() override;
	void draw( ::ui::Renderer *ren ) override;

  private:
	enum CalcMode { MIN_MAX, AVERAGE };

	void loadWaveform( const float *samples, size_t numSamples, const ci::vec2 &waveSize, size_t pixelsPerVertex, CalcMode mode );

	//bool					mScaleDecibels = false;
	bool					mBorderEnabled = true;
	//bool					mDrawValueAtMouse = true;
	ci::ColorA				mBorderColor = ci::ColorA( 0.5f, 0.5f, 0.5f, 1 );
	ci::ColorA				mColorMinMax = ci::ColorA::gray( 0.5f );
	ci::ColorA				mColorAverage = ci::ColorA::gray( 0.75f );
	ui::LabelRef			mTitleLabel;

	std::vector<ci::gl::VboMeshRef>	mWaveformVbos;
};

//! Shows one frame of the magnitude spectrum
class MA_API AudioSpectrumView : public ::ui::View {
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
class MA_API AudioSpectrogramView : public ::ui::View {
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
class MA_API AudioFrequencyBandsView : public ::ui::View {
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
