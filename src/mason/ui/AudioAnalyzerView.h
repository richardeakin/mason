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

#include "vu/View.h"
#include "mason/Mason.h"
#include "mason/Info.h"
#include "mason/audio/AudioAnalyzer.h"
#include "mason/ui/AudioViews.h"

namespace mason { namespace audio {

using AudioAnalyzerRef = std::shared_ptr<class AudioAnalyzer>;
using TrackRef = std::shared_ptr<class Track>;

} } // namespace mason::audio

// note: using namespace mason::mui so as not to conflict with vu::*. Oh well..
// TODO: move files to mui folder
namespace mason { namespace mui {

using AudioAnalyzerViewRef		= std::shared_ptr<class AudioAnalyzerView>;
using AudioTrackViewRef			= std::shared_ptr<class AudioTrackView>;

class MA_API AudioAnalyzerView : public ::vu::View {
  public:
	AudioAnalyzerView( const ci::Rectf &bounds = ci::Rectf::zero() );
	~AudioAnalyzerView();

	void load( const ma::Info &config );

	void setup();

  protected:

	void layout() override;
	void update() override;
	void draw( ::vu::Renderer *ren ) override;

  private:
	void reloadConfig();

	ma::Info	mLastConfig;

	std::vector<AudioTrackViewRef>			mTrackViews;
	ci::signals::ConnectionList				mConnections;

	friend class AudioTrackView;
};

class MA_API AudioTrackView : public ::vu::View {
  public:
	AudioTrackView( AudioAnalyzerView *parent, int trackIndex, const ci::Rectf &bounds = ci::Rectf::zero() );
	~AudioTrackView();

  protected:
	void layout() override;
	void update() override;
	void draw( ::vu::Renderer *ren ) override;

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

} } // namespace mason::mui
