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
#include "cinder/app/App.h"
#include "cinder/Timer.h"

#include "mason/Hud.h"
#include "mason/audio/OfflineContext.h"
#include "mason/Common.h"

#define CEREAL_ENABLED 1
#if CEREAL_ENABLED
#include "CinderCereal.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#endif

#include "glm/gtx/string_cast.hpp"

using namespace ci;
using namespace std;

#if 0

RT_IMPL( mason::mui::AudioAnalyzerView, rt::BuildSettings()
	//.vcxprojCpp( getMaworkRootPath() / "proj/vc2015/mawork.vcxproj" ) // TODO: these still need to be implemented
	//.vcxprojLinker()
	.include( getMaworkRootPath() / "src/mason/ui" ) // TODO: still needed?
	.vcxproj( getMaworkRootPath() / "proj/vc2015/mawork.vcxproj" )
	.linkAppObjs( false ) // TODO: clarify the purpose of this
	.libraryPath( "../../cinder/lib/msw/x64" )
	.libraryPath( "../../cinder/lib/msw/x64/Debug_Shared/v140" )
	.library( "cinder.lib" )
	.library( "OpenGL32.lib" )
	.library( "../../blocks/mason/lib/msw/x64/Debug_Shared/v140/mason.lib" )
	.library( "../../lib/msw/x64/Debug_Shared/v140/mawork.lib" )
	.verbose( true )
);

#endif

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

// TODO: use labels instead?
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

// ----------------------------------------------------------------------------------------------------
// AudioSpectrumView
// ----------------------------------------------------------------------------------------------------

AudioSpectrumView::AudioSpectrumView( const ci::Rectf &bounds )
	: View( bounds )
{
	initGl();
}

void AudioSpectrumView::initGl()
{
}

void AudioSpectrumView::updateData( const audio::TrackRef &track )
{
	mMagSpectrum = track->getMagSpectrum();

	float spectralCentroid = track->getSpectralCentroid();
	float nyquist = track->getSampleRate() / 2.0f;
	mMaxSpectrumBin = size_t( (float)mMagSpectrum.size() * mMaxSpectrumFreq / nyquist );

	mSpectralCentroidNormalized = spectralCentroid / mMaxSpectrumFreq;

	if( mDrawValueAtMouse ) {
		vec2 mousePos = app::App::get()->getMousePos() - app::getWindowPos();
		Rectf worldBounds = getWorldBounds();

		if( worldBounds.contains( mousePos ) ) {
			vec2 valuePos = mousePos - worldBounds.getUpperLeft();

			size_t bin = size_t( (float)mMaxSpectrumBin * std::min( 1.0f, valuePos.x / getWidth() ) );
			mBinIndexUnderMouse = (int)glm::clamp<size_t>( bin, 0, mMaxSpectrumBin - 1 );

			mFreqUnderMouse = mMaxSpectrumFreq * valuePos.x / getWidth();

			//float freqSizeOneBin = nyquist / (float)mMagSpectrum.size();
			//ma::hud()->showInfo( 2, { "bin width", to_string( freqSizeOneBin ) } );
		}
		else {
			//mBinIndexUnderMouse = -1;
		}
	}
}

void AudioSpectrumView::draw( ::ui::Renderer *ren )
{
	// draw main spectrum
	{
		gl::ScopedGlslProg glslScope( getStockShader( gl::ShaderDef().color() ) );

		ColorA bottomColor( 0, 0, 0.7f, 1 );

		size_t numBins = mMaxSpectrumBin;
		CI_ASSERT( numBins <= mMagSpectrum.size() );
		
		if( mMagSpectrum.empty() )
			return;

		float binWidth = getWidth() / (float)numBins;

		// TODO: draw this with gl::Batch
		gl::VertBatch batch( GL_TRIANGLE_STRIP );

		size_t currVertex = 0;
		float mag;
		Rectf bin( 0, 0, binWidth, getHeight() );
		for( size_t i = 0; i < numBins; i++ ) {
			mag = mMagSpectrum[i];

			float maxMagValue = 1;
			if( mScaleDecibels ) {
				maxMagValue = 100;
				mag = ci::audio::linearToDecibel( mag );
			}
			mag = glm::max<float>( 0, lmap( mag, mMinMagValue, maxMagValue, 0.0f, 1.0f ) );;


			bin.y1 = bin.y2 - mag * getHeight();

			batch.color( bottomColor );
			batch.vertex( bin.getLowerLeft() );
			batch.color( 0, mag, 0.7f );
			batch.vertex( bin.getUpperLeft() );

			bin += vec2( binWidth, 0 );
			currVertex += 2;
		}

		batch.color( bottomColor );
		batch.vertex( bin.getLowerLeft() );
		batch.color( 0, mag, 0.7f );
		batch.vertex( bin.getUpperLeft() );

		ren->setColor( Color( 0, 0.9f, 0 ) );

		batch.draw();
	}

	if( mDrawSpectralCentroid && mSpectralCentroidNormalized >= 0 ) {
		float barCenter = mSpectralCentroidNormalized * getWidth();
		Rectf verticalBar = { barCenter - 2, 0, barCenter + 2, getHeight() };

		ren->setColor( ColorA( mSpectralCentroidNormalized, 0.45f, 1, 0.4f ) );
		ren->drawSolidRect( verticalBar );
	}

	if( mDrawValueAtMouse && mBinIndexUnderMouse > -1 ) {
		// draw the magnitude and frequency of that bin at top right
		float mag = mMagSpectrum.at( (size_t)mBinIndexUnderMouse );
		float magDB = ci::audio::linearToDecibel( mag );

		string valueStr = fmt::format( "[{}] freq: {:.0f}hz, mag: {:.6f}, magDB: {:.4f}", mBinIndexUnderMouse, mFreqUnderMouse, mag, magDB );
		vec2 strSize = getTrackViewFont()->measureString( valueStr );
		ren->setColor( Color( 1, 1, 1 ) );
		getTrackViewFont()->drawString( valueStr, vec2( getWidth() - strSize.x - 4, 14 ) );
	}

	if( mBorderEnabled ) {
		ren->setColor( mBorderColor );
		ren->drawStrokedRect( getBoundsLocal() );
	}
}

// ----------------------------------------------------------------------------------------------------
// AudioSpectrogramView
// ----------------------------------------------------------------------------------------------------

ma::ColorLUT<ci::ColorAf> makeLutSpectrogram()
{
	vector<ma::ColorLUT<ci::ColorAf>::Stop> stops;
	stops.push_back( { 0.0f, ColorAf( 0, 0, 0, 0.2f ) } );
	stops.push_back( { 0.1f, Colorf( 0.1, 0, 0.3 ) } );
	stops.push_back( { 0.2f, Colorf( 0, 0.3, .5 ) } );
	stops.push_back( { 0.4f, Colorf( 0.7f, 0.5f, 0.1f ) } );
	stops.push_back( { 0.55f, Colorf( 1, 1, 0 ) } );
	stops.push_back( { 0.8f, Colorf( 1, 0, 0 ) } );

	return ColorLUT<ci::ColorAf>( 1000, stops );
}

AudioSpectrogramView::AudioSpectrogramView( int trackIndex, const ci::Rectf &bounds )
	: View( bounds ), mTrackIndex( trackIndex )
{
	CI_LOG_I( "bang. this: " << hex << this << dec );
	//initGl();
	mLUT = makeLutSpectrogram();
}

void AudioSpectrogramView::load( const audio::TrackRef &track )
{
	auto fileName = track->getSampleFilePath();
	if( readSTFTFromDisk( fileName ) ) {
		return;
	}

	auto ctx = make_shared<ma::audio::OfflineContext>();
	ctx->setFramesPerBlock( 2048 );
	ctx->setOuputNumChannels( 1 );

	if( ! fs::exists( fileName ) ) {
		// TODO: this is necessary at the moment because we're not going through AssetManager. Shouldn't be in the long run
		// - it is only needed when AudioAnalyzer loads an asset file from app's directory. In the case of data:// files, they get a full path
		// - maybe track should always store the full path?
		fileName = app::getAssetPath( fileName );
	}

	if( ! fs::exists( fileName ) ) {
		CI_LOG_E( "couldn't find track's sampleFile path at: " << fileName );
		return;
	}

	auto playerNode = ctx->makeNode<ci::audio::FilePlayerNode>( ci::audio::load( ci::loadFile( fileName ) ) );
	playerNode->setName( "<offline> BufferPlayerNode (" + fileName.string() + ")" );

	auto fftSize = track->getMonitorSpectralNode()->getFftSize();
	auto windowSize = track->getMonitorSpectralNode()->getWindowSize();
	auto monitorFormat = ci::audio::MonitorSpectralNode::Format().fftSize( fftSize ).windowSize( windowSize );

	CI_LOG_I( "monitor window size: " << windowSize << ", fft size: " << fftSize );

	auto monitor = ctx->makeNode<ci::audio::MonitorSpectralNode>( monitorFormat );

	playerNode >> monitor >> ctx->getOutput();

	// TODO: this should match window size or fft size
	// - it could be bigger but MonitorSpectralNode needs to be modified so that you 'drain' all of it's samples
	ci::audio::Buffer buffer( ctx->getFramesPerBlock(), ctx->getOutput()->getNumChannels() );
	CI_LOG_I( "buffer frames: " << buffer.getNumFrames() << ", channels: " << buffer.getNumChannels() );
	
	playerNode->start();
	ctx->enable();

	Timer timer1( true );

	while( ! playerNode->isEof() ) {
		ctx->pull( &buffer );

		auto spec = monitor->getMagSpectrum();
		mSTFT.push_back( spec );
	}

	timer1.stop();

	CI_LOG_I( "wrote " << mSTFT.size() << " STFT frames, memory occupuid: " << mSTFT.size() * mSTFT[0].size() * sizeof( float ) * 1e-6 << "mb" );


	CI_LOG_I( "seconds to compute STFT: " << timer1.getSeconds() );
	writeSTFTToDisk( fileName );
}

fs::path makeSTFTFileName( const ci::fs::path &sampleName )
{
	string outFileName = sampleName.filename().string() + ".cereal";
	//auto outFilePath = "build" / sampleName;
	//outFilePath.replace_filename( outFileName );

	auto outFilePath = fs::path( "build/audio/" + outFileName );
	return outFilePath;
}

bool AudioSpectrogramView::readSTFTFromDisk( const ci::fs::path &sampleName )
{
	auto filePath = makeSTFTFileName( sampleName );
	if( ! fs::exists( filePath ) ) {
		return false;
	}

#if	CEREAL_ENABLED
	Timer timer( true );

	std::ifstream is( filePath, std::ios::binary );
	cereal::BinaryInputArchive iarchive( is );

	iarchive( mSTFT );

	CI_LOG_I( "read archive in file: " << filePath << " in " << timer.getSeconds() << " seconds. STFT frames loaded: " << mSTFT.size() );

	return true;
#else
	return false;
#endif
}

void AudioSpectrogramView::writeSTFTToDisk( const ci::fs::path &sampleName )
{
	if( mSTFT.empty() ) {
		CI_LOG_E( "mSTFT empty, returning." );
		return;
	}

#if	CEREAL_ENABLED
	Timer timer( true );

	auto filePath = makeSTFTFileName( sampleName );
	if( ! fs::exists( filePath.parent_path() ) ) {
		fs::create_directories( filePath.parent_path() );
	}

	std::ofstream os( filePath, std::ios::binary ); // TODO: is this the readable format?
	cereal::BinaryOutputArchive archive( os );

	archive( mSTFT );

	CI_LOG_I( "wrote archive to file: " << filePath << " in " << timer.getSeconds() << " seconds." );
#endif
}

// TODO: think through how I can do this write progressively. Right now I don't know how wide it should be until EOF
// - should I load it directly into Texture?
// - going to want to render it inbetween anyway
// FIXME NEXT: mags coming out zero only in release mode
// - need to use double precision?
void AudioSpectrogramView::makeTex()
{
	CI_ASSERT( getWidth() && getHeight() );
	CI_ASSERT( ! mSTFT.empty() );

	float numBins = mSTFT[0].size();
	vec2 imgSize = getSize();
	auto surface = Surface32f( imgSize.x, imgSize.y, true );
	CI_LOG_I( "surface size: " << surface .getSize() );

	Timer timer( true );

	auto iter = surface.getIter();
	while( iter.line() ) {
		size_t bin = min<size_t>( numBins - round( float( iter.y() / imgSize.y ) * numBins ), numBins - 1 );
		while( iter.pixel() ) {
			size_t frame = min<size_t>( float( iter.x() / imgSize.x ) * mSTFT.size(), mSTFT.size() - 1 );

			float mag = mSTFT[frame][bin];
			mag = ci::audio::linearToDecibel( mag ) / 100.0f; // TODO: consider storing this in DB within the mSTFT frames

			//iter.r() = mag;
			//iter.g() = 0;
			//iter.b() = 0;

			auto col = mLUT.lookup( mag );
			iter.r() = col.r;
			iter.g() = col.g;
			iter.b() = col.b;
			iter.a() = col.a;
		}
	}

	timer.stop();

	mTex = gl::Texture::create( surface );

	CI_LOG_I( "tex size: " << mTex->getSize() << ", bounds: " << getBounds() << ", seconds to pack into Surface: " << timer.getSeconds() );

	if( 0 ) {
		size_t testFrame = 7547;
		const auto &magSpectrum = mSTFT[testFrame];
		CI_LOG_I( "frame: " << testFrame );
		for( size_t bin = 0; bin < magSpectrum.size(); bin++ ) {
			float mag = magSpectrum[bin];
			float magDB = ci::audio::linearToDecibel( mag ) / 100.0f;
			CI_LOG_I( "[" << bin << "] " << mag << ", " << magDB << " db" );
		}
	}
}

void AudioSpectrogramView::update( const audio::TrackRef &track )
{
	mTrackTimePercent = float( track->getSamplePlaybackTime() / track->getSampleDuration() );

	// update band info under mouse
	if( mDrawValueAtMouse && ! mSTFT.empty() ) {
		vec2 mousePos = app::App::get()->getMousePos() - app::getWindowPos();
		Rectf worldBounds = getWorldBounds();


		if( worldBounds.contains( mousePos ) ) {
			vec2 valuePos = mousePos - worldBounds.getUpperLeft();

			float numBins = (float)mSTFT[0].size();

			size_t bin = min<size_t>( numBins - round( float( valuePos.y / getHeight() ) * numBins ), numBins - 1 );
			size_t frame = min<size_t>( float( valuePos.x / getWidth() ) * mSTFT.size(), mSTFT.size() - 1 );

			mBinUnderMouse = { frame, bin };
		}
		else {
			//mBandIndexUnderMouse = -1;
		}
	}
	else {
		mBinUnderMouse = { -1, -1 };
	}

}

void AudioSpectrogramView::draw( ::ui::Renderer *ren )
{
	if( ! mSTFT.empty() && ! mTex ) {
		makeTex();
	}

	// TODO: use ImageView
	if( mTex ) {
		gl::draw( mTex, getBoundsLocal() );
	}

	if( 0 ) {
		auto tex = gl::Texture::create( mLUT.makeSurface32a() );
		gl::draw( tex, Rectf( 6, 6, 260, 40 ) );
	}

	// draw current time in track
	{
		float barCenter = mTrackTimePercent * getWidth();
		Rectf verticalBar = { barCenter - 2, 0, barCenter + 2, getHeight() };

		ren->setColor( ColorA( 1, 0.45f, 1, 0.4f ) );
		ren->drawSolidRect( verticalBar );
	}

	if( mDrawValueAtMouse && mBinUnderMouse.x >= 0 && mBinUnderMouse.y >= 0 ) {
		// draw the magnitude and frequency at that bin
		float magLinear = mSTFT[mBinUnderMouse.x][mBinUnderMouse.y];
		float magDb = ci::audio::linearToDecibel( magLinear ) / 100.0f;

		string valueStr = fmt::format( "[{}, {}] mag linear: {:.6f}, db: {:.6f}", mBinUnderMouse.x, mBinUnderMouse.y, magLinear, magDb );
		vec2 strSize = getTrackViewFont()->measureString( valueStr );
		ren->setColor( Color( 1, 1, 1 ) );
		getTrackViewFont()->drawString( valueStr, vec2( getWidth() - strSize.x - 4, 14 ) );
	}

	if( mBorderEnabled ) {
		ren->setColor( mBorderColor );
		ren->drawStrokedRect( getBoundsLocal() );
	}

}

bool AudioSpectrogramView::touchesBegan( app::TouchEvent &event )
{
	auto &firstTouch = event.getTouches().front();
	vec2 pos = toLocal( firstTouch.getPos() );

	setTrackPos( pos.x );
	//updateValue( pos );
	firstTouch.setHandled();
	return true;
}

bool AudioSpectrogramView::touchesMoved( app::TouchEvent &event )
{
	auto &firstTouch = event.getTouches().front();
	vec2 pos = toLocal( firstTouch.getPos() );
	setTrackPos( pos.x );

	firstTouch.setHandled();
	return true;
}

void AudioSpectrogramView::setTrackPos( float x )
{
	auto track = ma::audio::analyzer()->getTrack( mTrackIndex );
	if( ! track )
		return;

	double time = track->getSampleDuration() * x / getWidth();
	glm::clamp<double>( time, 0.0, track->getSamplePlaybackTime() );
	
	// TODO: refactor AudioAnalyzer so that individual tracks can be seeked
	mason::audio::analyzer()->seek( time, true );
}

// ----------------------------------------------------------------------------------------------------
// AudioBarkBandsView
// ----------------------------------------------------------------------------------------------------

AudioFrequencyBandsView::AudioFrequencyBandsView( const ci::Rectf &bounds )
	: View( bounds )
{
	initGl();
}

void AudioFrequencyBandsView::initGl()
{
}


void AudioFrequencyBandsView::updateData( const audio::TrackRef &track )
{
	mBands = track->getFrequencyBands();

	if( mMaxSpectrumFreq < 0 )
		mMaxSpectrumFreq = track->getSampleRate() / 2.0f;

	// update band info under mouse
	if( mDrawValueAtMouse ) {
		vec2 mousePos = app::App::get()->getMousePos() - app::getWindowPos();
		Rectf worldBounds = getWorldBounds();

		if( worldBounds.contains( mousePos ) ) {
			vec2 valuePos = mousePos - worldBounds.getUpperLeft();

			if( mAlignBandsToSpectrum ) {
				// find bin whose frequency range covers this pos
				float freq = mMaxSpectrumFreq * valuePos.x / getWidth();
				for( const auto &band : mBands ) {
					if( freq > band.mFreqLower && freq <= band.mFreqUpper ) {
						mBandIndexUnderMouse = band.mIndex;
						break;
					}
				}
			}
			else {
				int bandIndex = int( (float)mBands.size() * std::min( 1.0f, valuePos.x / getWidth() ) );
				mBandIndexUnderMouse = glm::clamp<int>( bandIndex, 0, mBands.size() - 1 );
			}
		}
		else {
			//mBandIndexUnderMouse = -1;
		}
	}
}

void AudioFrequencyBandsView::draw( ::ui::Renderer *ren )
{
	// draw frequency bands as bars
	// TODO: draw with instanced rectangle batch
	{
		gl::ScopedGlslProg glslScope( getStockShader( gl::ShaderDef().color() ) );

		ColorA bottomColor( 0, 0, 0.7f, 1 );

		size_t numBands = mBands.size();
		CI_ASSERT( numBands );
		float bandWidth = getWidth() / (float)numBands; // used if ! mAlignBandsToSpectrum
		float freqScale = getWidth() / mMaxSpectrumFreq; // used when aligning. TODO: rename

		for( size_t b = 0; b < numBands; b++ ) {
			const auto &band = mBands[b];

			Rectf binRect;

			if( mAlignBandsToSpectrum ) {
				if( band.mFreqLower >= mMaxSpectrumFreq )
					break;

				vec2 ul = { band.mFreqLower * freqScale, getHeight() * float( 1.0 - band.mMag ) };
				vec2 lr = { band.mFreqUpper * freqScale, getHeight() };
				lr.x = min<float>( lr.x, getWidth() ); // clamp to border
				binRect = { ul, lr };
			}
			else {
				vec2 ul = { bandWidth * b, getHeight() * float( 1.0 - band.mMag ) };
				vec2 lr = { ul.x + bandWidth, getHeight() };
				binRect = { ul, lr };
			}

			gl::color( Color( 0.75f, 0, 0 ) );
			gl::drawStrokedRect( binRect, 2 );

			gl::color( ColorA( 0.75f, 0.4f, 0.15f, 1 ) );
			gl::drawSolidRect( binRect );
		}
	}

	if( mDrawValueAtMouse && mBandIndexUnderMouse >= 0 ) {
		// draw the magnitude and frequency at that bin
		CI_ASSERT( mBandIndexUnderMouse < mBands.size() );
		const auto &band = mBands[mBandIndexUnderMouse];
		string valueStr = fmt::format( "[{}] bins: {} - {}, freq: {:.0f} - {:.0f}hz, {:.3f}db scaled", band.mIndex, band.mBinLower, band.mBinUpper, band.mFreqLower, band.mFreqUpper, band.mMag );
		vec2 strSize = getTrackViewFont()->measureString( valueStr );
		ren->setColor( Color( 1, 1, 1 ) );
		getTrackViewFont()->drawString( valueStr, vec2( getWidth() - strSize.x - 4, 14 ) );
	}

	if( mBorderEnabled ) {
		ren->setColor( mBorderColor );
		ren->drawStrokedRect( getBoundsLocal() );
	}
}

} } // namespace mason::mui
