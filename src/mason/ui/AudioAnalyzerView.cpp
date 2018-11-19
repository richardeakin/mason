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


#include "mason/ui/AudioAnalyzerView.h"
#include "ui/Layout.h"
#include "fmt/format.h"

#include "cinder/Log.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"

#include "mason/Hud.h"
#include "mason/Common.h"

#include "glm/gtx/string_cast.hpp"

using namespace ci;
using namespace std;

namespace mason { namespace mui {

const float PADDING = 8;
const float METER_WIDTH = 20;

// ----------------------------------------------------------------------------------------------------
// AudioAnalyzerView
// ----------------------------------------------------------------------------------------------------

AudioAnalyzerView::AudioAnalyzerView( const ci::Rectf &bounds )
	: View( bounds )
{	
	// TODO: use GridLayout (once it exists), so tracks can be stacked
	auto layout = make_shared<::ui::HorizontalLayout>();
	layout->setPadding( 12 ); // TODO: why is affecting the top portion of a single Track? Shouldn't.
	layout->setMargin( Rectf( 10, 10, 10, 10 ) );
	setLayout( layout );

	mConnections += ma::audio::analyzer()->getSignalTracksChanged().connect( signals::slot( this, &AudioAnalyzerView::reloadConfig ) );
}

AudioAnalyzerView::~AudioAnalyzerView()
{
}

void AudioAnalyzerView::setup()
{
	reloadConfig();
}

void AudioAnalyzerView::reloadConfig()
{
	load( mLastConfig );
}

void AudioAnalyzerView::load( const ma::Dictionary &config )
{
	mLastConfig = config;

	if( config.contains( "backgroundColor" ) ) {
		auto col = config.get<ColorA>( "backgroundColor" );
		getBackground()->setColor( col );
	}


	bool enabled = config.get<bool>( "enabled", false );
	setHidden( ! enabled );

	auto trackSize = config.get<vec2>( "trackSize", vec2( 300, 140 ) );
	float maxSpectrumFreq = config.get<float>( "maxSpectrumFreq", -1 );

	const auto &tracks = ma::audio::analyzer()->getTracks();
	CI_LOG_I( "num tracks: " << tracks.size() );

	//for( const auto &trackView : mTrackViews ) {
	//	trackView->removeFromParent();
	//}

	removeAllSubviews();
	mTrackViews.clear();

	for( int i = 0; i < tracks.size(); i++ ) {
		auto trackView = make_shared<AudioTrackView>( this, i );
		trackView->setSize( trackSize );

		// Set the max spectrum frequency to be nyquist or lower
		// TODO: there's probably a better way to set this, but will wait until bark stuff and bin summing is completed
		float nyquist = tracks[i]->getSampleRate() / 2;
		if( maxSpectrumFreq < 0 || maxSpectrumFreq > nyquist ) {
			maxSpectrumFreq = nyquist;
		}
		trackView->mSpectrumView->mMaxSpectrumFreq = maxSpectrumFreq;
		trackView->mFreqBandsView->mMaxSpectrumFreq = maxSpectrumFreq;

		addSubview( trackView );
		mTrackViews.push_back( trackView );
	}
}

void AudioAnalyzerView::layout()
{
	const auto &subviews = getSubviews();
	if( subviews.empty() )
		return;

	// wrap out size to fit the contents.
	// TODO: this should really be a part of either Layout or something similar to setFillParentEnabled()
	const auto lastViewBounds = subviews.back()->getBounds();
	const auto layoutMargins = getLayout()->getMargin();
	setSize( lastViewBounds.getLowerRight() + layoutMargins.getLowerRight() );
}

void AudioAnalyzerView::update()
{
	if( ! isHidden() ) {
		bool alignBandsToSpectrum = ma::hud()->checkBox( "align bands", true )->isEnabled();

		for( auto &trackView : mTrackViews ) {
			trackView->mSpectrumView->mMinMagValue = 0; // TODO: remove once mag scaling stuff is finished in AudioAnalyzer

			trackView->mFreqBandsView->mAlignBandsToSpectrum = alignBandsToSpectrum;
		}
	}
}

void AudioAnalyzerView::draw( ::ui::Renderer *ren )
{
	if( 0 ) {
		gl::ScopedColor red( 1, 1, 0 );
		gl::drawSolidCircle( getCenter(), 50 );
	}

}

// ----------------------------------------------------------------------------------------------------
// AudioTrackView
// ----------------------------------------------------------------------------------------------------

AudioTrackView::AudioTrackView( AudioAnalyzerView *parent, int trackIndex, const ci::Rectf &bounds )
	: View( bounds ), mParent( parent ), mTrackIndex( trackIndex )
{
	//CI_LOG_I( "this: " << hex << this << dec << ", trackIndex: " << mTrackIndex );

	auto track = ma::audio::analyzer()->getTrack( mTrackIndex );

	if( track->isSamplePlayer() ) {
		mSpectrogramView = make_shared<AudioSpectrogramView>( trackIndex );
		addSubview( mSpectrogramView );

		mSpectrogramView->load( track ); // TEMPORARY: disabling as it takes a while to load currently
	}

	mSpectrumView = make_shared<AudioSpectrumView>();
	addSubview( mSpectrumView );

	mFreqBandsView = make_shared<AudioFrequencyBandsView>();
	addSubview( mFreqBandsView );
}

AudioTrackView::~AudioTrackView()
{
	//CI_LOG_I( "this: " << hex << this << dec << ", trackIndex: " << mTrackIndex );
}

// TODO: use ui::Label instead
gl::TextureFontRef getTrackViewFont()
{
	static gl::TextureFontRef sTextFont;
	if( ! sTextFont )
		sTextFont = gl::TextureFont::create( Font( Font::getDefault().getName(), 18 ) );

	return sTextFont;
}

void AudioTrackView::layout()
{
	float numVerticalViews = (bool)mSpectrogramView ? 3.0f : 2.0f;
	float rectHeight = ( getHeight() - PADDING * ( numVerticalViews - 1 ) )  / numVerticalViews; // padding only used inbetween, edges are done with Layout

	Rectf rectBounds = getBoundsLocal();
	rectBounds.x1 += METER_WIDTH + PADDING;
	rectBounds.y2 = rectHeight;


	mSpectrumView->setBounds( rectBounds );

	rectBounds += vec2( 0, rectHeight + PADDING );
	mFreqBandsView->setBounds( rectBounds );

	if( mSpectrogramView ) {
		rectBounds += vec2( 0, rectHeight + PADDING );
		mSpectrogramView->setBounds( rectBounds );
	}
}

void AudioTrackView::update()
{
	auto track = ma::audio::analyzer()->getTrack( mTrackIndex );
	if( ! track )
		return;

	if( mSpectrogramView ) {
		mSpectrogramView->update( track );
	}

	mSpectrumView->updateData( track );
	mFreqBandsView->updateData( track );
}

void AudioTrackView::draw( ::ui::Renderer *ren )
{
	auto track = ma::audio::analyzer()->getTrack( mTrackIndex );
	if( ! track ) {
		ren->setColor( Color( 1, 0, 0 ) );
		ren->drawSolidRect( getBoundsLocal() );
		return;
	}

	const auto bounds = getBoundsLocal();

	float gain = track->getGain();
	float volume = track->getVolumeRMS(); // in db, 0:100
	float loudness = track->getLoudness(); // scaled volume, 0:1

	Rectf meterBounds = { bounds.x1, bounds.y1 + PADDING, bounds.x1 + METER_WIDTH, bounds.y2 };

	auto texFont = getTrackViewFont();

	{
		auto textColor = ColorA( 0.2f, 0.8f, 0.9f, 0.7f );

		vec2 pos = bounds.getUpperLeft();

		gl::ScopedColor color_( textColor );
		texFont->drawString( getTitle( track ), bounds.getUpperLeft() );

		pos += vec2( meterBounds.getWidth() + PADDING, 20 );
		texFont->drawString( "gain: " + fmt::format( "{0:.2f}", gain ), pos );

		pos += vec2( 0, 20 );
		texFont->drawString( "volume: " + fmt::format( "{0:.2f}", volume ), pos );

		if( track->isSamplePlayer() ) {
			pos += vec2( 0, 20 );
			texFont->drawString( "pos: " + fmt::format( "{0:.2f}s", track->getSamplePlaybackTime() ), pos );
		}
	}

	{
		// FIXME: this isn't drawing for mic
		// - I believe it is become volume is scaled so only 60-100db is shown, and mic is getting a lower signal than that.
		float volumeHeight = meterBounds.getHeight() * loudness;
		Rectf volumeBounds = meterBounds;
		volumeBounds.y1 = volumeBounds.y2 - volumeHeight;

		gl::ScopedColor color_( ColorA( 0, 1, 0.3f, 0.5f ) );
		gl::drawSolidRect( volumeBounds );
	}

	{
		auto meterColor = ColorA( 0, 1, 0, 0.5f );

		gl::ScopedColor color_( meterColor );
		gl::drawStrokedRect( meterBounds );
	}
}

const std::string& AudioTrackView::getTitle( const audio::TrackRef &track )
{
	if( mTitle.empty() ) {
		mTitle = "[" + to_string( track->getIndex() ) + "] " + track->getId();
	}

	return mTitle;
}

} } // namespace mason::mui
