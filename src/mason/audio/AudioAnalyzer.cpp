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

/*
Notes on device names that are appearing in Windows:

Roland Octa-Capture:
"1-2 (OCTA-CAPTURE)"
"3-4 (OCTA-CAPTURE)"
"5-6 (OCTA-CAPTURE)"

Maya 44 USB:
"MAYA44USB Ch12 (MAYA44USB Audio driver)"
"MAYA44USB Ch34 (MAYA44USB Audio driver)"

*/

#include "cinder/app/App.h"
#include "cinder/audio/audio.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Timer.h"

#include "mason/audio/AudioAnalyzer.h"
#include "mason/Assets.h"
#include "mason/Common.h"
#include "mason/Hud.h"

#if defined( CINDER_MSW )
#include "cinder/audio/msw/ContextWasapi.h"
#endif

//#define LOG_ANALYZER( stream )	CI_LOG_I( stream )
#define LOG_ANALYZER( stream )	( (void)( 0 ) )

// uncomment for my audio_dev branch, until the exclusive mode stuff makes it into master branch
#define CINDER_AUDIO_MSW_HAS_EXCLUSIVE_MODE

using namespace ci;
using namespace std;

namespace mason { namespace audio {

namespace {

InputType getInputTypeFromString( const std::string &typeStr )
{
	if( typeStr == "file" )
		return InputType::FILE_PLAYER;
	else if( typeStr == "buffer" )
		return InputType::BUFFER_PLAYER;
	else if( typeStr == "device" )
		return InputType::INPUT_DEVICE;
	else
		return InputType::UNKNOWN;
}

} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// Track
// ----------------------------------------------------------------------------------------------------

Track::Track( const std::string &id, int trackIndex )
	: mId( id ), mIndex( trackIndex )
{
}

Track::~Track()
{
	if( mMonitorSpectralNode )
		mMonitorSpectralNode->disconnectAllOutputs();
}

void Track::setGain( float value, double rampSeconds )
{
	mGainValue = max( value, 0.0f );
	mGain->getParam()->applyRamp( mGainValue, rampSeconds );
}

void Track::incrGain( float amount, double rampSeconds )
{
	auto endTimeAndValue = mGain->getParam()->findEndTimeAndValue();
	setGain( endTimeAndValue.second + amount, rampSeconds );
}

float Track::getVolumeRMS() const
{
	return mVolumeRMS;
}

float Track::getLoudness() const
{
	return glm::max<float>( 0, lmap( mVolumeRMS, mMinMagValue, 100.0f, 0.0f, 1.0f ) );
}

float Track::getSpectralCentroid() const
{
	return mMonitorSpectralNode->getSpectralCentroid();
}

float Track::getSampleRate() const
{
	return (float)ci::audio::master()->getSampleRate();
}

double Track::getSamplePlaybackTime() const
{
	if( ! mSamplePlayerNode )
		return -1;

	return mSamplePlayerNode->getReadPositionTime();
}

double Track::getSampleDuration() const
{
	if( ! mSamplePlayerNode )
		return -1;

	return mSamplePlayerNode->getNumSeconds();
}

void Track::setSpectrumSmoothingFactor( float x )
{
	mMonitorSpectralNode->setSmoothingFactor( x );
}

float Track::getSpectrumSmoothFactor() const
{
	return mMonitorSpectralNode->getSmoothingFactor();
}

void Track::update()
{
	float volume = ci::audio::linearToDecibel( mMonitorSpectralNode->getVolume() );

	ma::hud()->showInfo( 2, { "volume", to_string( volume ) } );

	// TODO: can also try just using a very small smoothing factor for larger values
	// - or just provide two separate control values
	if( volume > mVolumeRMS ) {
		mVolumeRMS = volume;

		//if( mVolumeRMS > mVolumeRMSLastOnset + mOnsetVolumeChange ) {
			// only send out onsets if volume changes substantially
			mVolumeRMSLastOnset = mVolumeRMS;
			mSignalOnset.emit(); // hokey horrible onsets - whenever volume gets louder (time thresholding in app)
//		}
	}
	else {
		mVolumeRMS = mVolumeRMS * mVolumeSmoothingFactor + volume * ( 1 - mVolumeSmoothingFactor );
		mVolumeRMSLastOnset = mVolumeRMSLastOnset * mVolumeSmoothingFactor + volume * ( 1 - mVolumeSmoothingFactor );
	}
	
	mMagSpectrum = mMonitorSpectralNode->getMagSpectrum();

	sumFrequencyBands();
}

const std::string& Track::getId() const
{
	return mId;
}

void Track::setBarkBandsEnabled( bool enable )
{
	if( enable ) {
		initBarkBands();
	}
	else {
		mFreqBands.clear();
	}
}

void Track::initBarkBands()
{
	CI_ASSERT( mMonitorSpectralNode );
	CI_ASSERT( mMonitorSpectralNode->isInitialized() );

	// https://ccrma.stanford.edu/~jos/bbt/Bark_Frequency_Scale.html
	const array<float, 27> barkBandEdges = {
		0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270,
		1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300,
		6400, 7700, 9500, 12000, 15500, 20500, 27000
	};

	float sr = getSampleRate();
	float N = mMonitorSpectralNode->getFftSize();
	int numBins = mMonitorSpectralNode->getNumBins();

	for( size_t i = 0; i < barkBandEdges.size() - 1; i++ ) {
		float freqLower = barkBandEdges[i];
		float freqUpper = barkBandEdges[i + 1];
		FrequencyBand b;
		b.mIndex = i;
		b.mFreqLower = barkBandEdges[i];
		b.mFreqUpper = barkBandEdges[i + 1];
		b.mBinLower = glm::clamp<int>( round( b.mFreqLower * N / sr ), 0, numBins - 1 );
		b.mBinUpper = glm::clamp<int>( round( b.mFreqUpper * N / sr ), 0, numBins );
		b.mMag = 0.5;
		mFreqBands.push_back( b );
	}
}

void Track::sumFrequencyBands()
{
	//bool usePower = ma::hud()->checkBox( "bark power", false )->isEnabled();
	bool usePower = false;

	for( size_t b = 0; b < mFreqBands.size(); b++ ) {
		auto &band = mFreqBands[b];

		band.mMag = 0;
		for( size_t n = band.mBinLower; n < band.mBinUpper; n++ ) {
			CI_ASSERT( n < mMagSpectrum.size() );
			float mag = mMagSpectrum[n];
			//if( usePower )
			//	mag *= mag;

			band.mMag += mag;
		}

		if( usePower )
			band.mMag *= band.mMag;


		const float maxMagValue = 100;
		band.mMag = ci::audio::linearToDecibel( band.mMag );
		band.mMag = glm::max<float>( 0, lmap( band.mMag, mMinMagValue, maxMagValue, 0.0f, 1.0f ) );
	}
}

ci::fs::path Track::getSampleFilePath() const
{
	// TODO: re-enable this for mawork
	// - will likely want a signal for customizing resolving the file path
	//return resolvePath( mSampleFileName );
	return mSampleFileName;
}

// ----------------------------------------------------------------------------------------------------
// AudioAnalyzer
// ----------------------------------------------------------------------------------------------------

namespace {
AudioAnalyzerRef sInstance;
}

AudioAnalyzer* analyzer()
{
	if( ! sInstance ) {
		sInstance = shared_ptr<AudioAnalyzer>( new AudioAnalyzer );
	}

	return sInstance.get();
}

void setAnalyzerInstance( const std::shared_ptr<AudioAnalyzer> &analyzer )
{
	sInstance = analyzer;
}

AudioAnalyzer::AudioAnalyzer()
{
	mConnKeyDown = app::getWindow()->getSignalKeyDown().connect( signals::slot( this, &AudioAnalyzer::keyDown ) );
}

AudioAnalyzer::~AudioAnalyzer()
{
	CI_LOG_I( "instance: " << hex << this << dec );
}

void AudioAnalyzer::enable()
{
	auto ctx = ci::audio::master();
	ctx->enable();

	for( const auto &track : mTracks ) {
		if( track->mInputDeviceNode )
			track->mInputDeviceNode->enable();
	}

	CI_LOG_I( "complete. context samplerate: " << ctx->getSampleRate() << ", frames per block: " << ctx->getFramesPerBlock() );
}

void AudioAnalyzer::disable()
{
	auto ctx = ci::audio::master();
	ctx->disable();

	for( const auto &track : mTracks ) {
		if( track->mInputDeviceNode )
			track->mInputDeviceNode->disable();
	}

	CI_LOG_I( "complete" );
}

bool AudioAnalyzer::isEnabled() const
{
	return ci::audio::master()->isEnabled();
}

void AudioAnalyzer::play()
{
	if( ! mIsPaused )
		return;

	mIsPaused = false;

	for( const auto &track : mTracks ) {
		if( track->mSamplePlayerNode ) {
			track->mGain->setValue( 0 );
			track->mGain->getParam()->applyRamp( track->mGainValue, mSeekRampSeconds, ci::audio::Param::Options().rampFn( &ci::audio::rampOutQuad ) );
			double audioTime = ci::audio::master()->getNumProcessedSeconds();
			LOG_ANALYZER( "[" << audioTime << "] enabling track '" << track->getId() );
			track->mInputNode->enable();
		}
	}
}

void AudioAnalyzer::pause()
{
	if( mIsPaused )
		return;

	mIsPaused = true;

	// delay this and ramp down first
	for( const auto &track : mTracks ) {
		if( track->mSamplePlayerNode ) {
			track->mGain->getParam()->applyRamp( 0, mSeekRampSeconds, ci::audio::Param::Options().rampFn( &ci::audio::rampOutQuad ) );
			//track->mSamplePlayerNode->disable( ci::audio::master()->getNumProcessedSeconds() + mSeekRampSeconds + 0.01 );

			double audioTime = ci::audio::master()->getNumProcessedSeconds();
			double disableWhen = audioTime + mSeekRampSeconds + 0.01;
			LOG_ANALYZER( "[" << audioTime << "] disabling track '" << track->getId() << "' at " << disableWhen );
			track->mSamplePlayerNode->disable( disableWhen );
		}
	}
}

void AudioAnalyzer::seek( double time, bool playSnippet )
{	
	if( ! ci::audio::master()->isEnabled() || ! mIsPaused )
		playSnippet = false;

	for( const auto &track : mTracks ) {
		if( track->mSamplePlayerNode ) {
			track->mSamplePlayerNode->seekToTime( time );
			LOG_ANALYZER( "seeked track '" << track->getId() << "' to time: " << time << "s. playSnippet: " << playSnippet );
			if( playSnippet ) {
				double playSnippetDuration = 0.5;
				track->mSamplePlayerNode->enable();
				track->mGain->getParam()->applyRamp( 0, 0.01 );
				track->mGain->getParam()->appendRamp( track->mGainValue, mSeekRampSeconds / 2, ci::audio::Param::Options().rampFn( &ci::audio::rampOutQuad ) );
				track->mGain->getParam()->appendRamp( 0, mSeekRampSeconds / 2, ci::audio::Param::Options().rampFn( &ci::audio::rampOutQuad ).delay( playSnippetDuration ) );

				double audioTime = ci::audio::master()->getNumProcessedSeconds();
				double disableWhen = audioTime + mSeekRampSeconds + playSnippetDuration + 0.02 ;
				LOG_ANALYZER( "\t-[" << audioTime << "] disabling track '" << track->getId() << "' at " << disableWhen );
				track->mSamplePlayerNode->disable( disableWhen );

				// TODO: should probably seek back to `time` upon disable
				// - don't need to currently with Tracker since unpausing will issue another seek
				//track->mSamplePlayerNode->seekToTime( time );
			}
		}
	}
}

void AudioAnalyzer::setMasterGain( float value, double rampSeconds )
{
	value = glm::clamp( value, 0.0f, 2.0f );
	// TODO: convert from normalized DB, same as tracks.
	mMasterGain->getParam()->applyRamp( value, rampSeconds, ci::audio::Param::Options().rampFn( &ci::audio::rampOutQuad ) );
}

void AudioAnalyzer::initialize( const Info &config )
{
	initEntry( config );
}

void AudioAnalyzer::initEntry( const ma::Info &config )
{
	bool audioEnabled = config.get<bool>( "enabled" );
	float masterGain = config.get<float>( "masterGain", 1.0f );
	mSeekRampSeconds = config.get<double>( "seekRampSeconds", mSeekRampSeconds );

	// only init Context if necessary (this could be a hot reload)
	// - TODO: check whether dev params changed (exclusive, frames per block)
	bool contextNeedsInit = false;
	if( ! mMasterGain ) {
		contextNeedsInit = true;
	}

	if( contextNeedsInit ) {
		initContext( config );
	}

	initTracks( config );

	setMasterGain( masterGain );
	setEnabled( audioEnabled );
}

void AudioAnalyzer::initContext( const ma::Info &config )
{
	auto ctx = ci::audio::master();
	ctx->disable();

	//CI_LOG_I( "audio graph before:\n" << ctx->printGraphToString() );
	
#if defined( CINDER_AUDIO_MSW_HAS_EXCLUSIVE_MODE )
	if( config.contains( "wasapiExclusive" ) ) {
		bool wasapiExclusive = config.get<bool>( "wasapiExclusive", false );
		if( wasapiExclusive ) {
			CI_LOG_I( "enabling WASAPI exclusive mode" );
			// TODO: test with audio_dev branch
			dynamic_cast<ci::audio::msw::ContextWasapi *>( ctx )->setExclusiveModeEnabled();
		}
	}
#endif
	
	// FIXME: this should dictate the size of blocks that come through Node::process() methods, but it doesn't seem to on Windows at the moment.
	// - will address with audio_device updates
	ci::audio::DeviceRef outputDev = ci::audio::Device::getDefaultOutput();
#if 1
	// adjust output node's framesPerBlock
	if( config.contains( "framesPerBlock" ) ) {
		size_t framesPerBlock = config.get<size_t>( "framesPerBlock" );
		CI_LOG_I( "config framesPerBlock: " << framesPerBlock );
		if( outputDev->getFramesPerBlock() != framesPerBlock ) {
			outputDev->updateFormat( ci::audio::Device::Format().framesPerBlock( framesPerBlock ) );
		}		
	}

#endif

	// FIXME: this is sometime causing a crash
	// - when OutputDeviceNodeWasapi is destroyed but the render loop is still running
	// - can cause it by forcing reload with clearCache = true from main app, then saving config.json
	// - it looks like there are two instances of WasapiRenderClientImpl, both within runRenderThread() (on diffeernt threads)
	ctx->setOutput( ctx->createOutputDeviceNode( outputDev ) );

	mMasterGain = ctx->makeNode<ci::audio::GainNode>();
	mMasterGain->setName( "master gain" );
	mMasterGain >> ctx->getOutput();	
}

void AudioAnalyzer::initTracks( const ma::Info &config )
{
	auto tracks = config.get<std::vector<ma::Info>>( "tracks" );
	auto fftSize = config.get<size_t>( "fftSize", 1024 );
	auto windowSize = config.get<size_t>( "windowSize", 512 );

	auto ctx = ci::audio::master();
	auto monitorFormat = ci::audio::MonitorSpectralNode::Format().fftSize( fftSize ).windowSize( windowSize );
	
	mTracks.clear(); // TODO: this is going to clear audio buffers - add option to cache them
	for( size_t i = 0; i < tracks.size(); i++ ) {
		// TODO: move individual track looking to a different method and wrap in a try / catch, so other tracks continue to load
		const auto &trackInfo = tracks[i];
		const auto id = trackInfo.get<string>( "id" );

		const bool enabled = trackInfo.get<bool>( "enabled", true );
		if( ! enabled ) {
			CI_LOG_I( "\t[" << i << "] - disabled - id: " << id );
			continue;
		}

		const auto gain = trackInfo.get<float>( "gain" );
		const auto typeStr = trackInfo.get<string>( "inputType" );

		auto inputType = getInputTypeFromString( typeStr );
		if( inputType == InputType::UNKNOWN ) {
			CI_LOG_E( "unexpected track type '" << typeStr << "' for track with id: " << id );
			continue;
		}

		CI_LOG_I( "\t[" << i << "] id: " << id << ", input type: " << typeStr );

		// Create initial track
		auto track = make_shared<ma::audio::Track>( id, (int)i );	
		track->mGain = ctx->makeNode( new ci::audio::GainNode( gain ) );
		track->mGainValue = gain;
		track->mMonitorSpectralNode = ctx->makeNode( new ci::audio::MonitorSpectralNode( monitorFormat ) );

		track->mGain >> track->mMonitorSpectralNode >> mMasterGain;

		if( inputType == InputType::FILE_PLAYER || inputType == InputType::BUFFER_PLAYER ) {
			const auto loopEnabled = trackInfo.get<bool>( "loopEnabled", false );
			track->mSampleFileName = trackInfo.get<string>( "fileName" );
			auto audioFilePath = resolvePathFromUrl( track->mSampleFileName );
			CI_LOG_I( "\t- file path: " << audioFilePath );

			weak_ptr<Track>	weakTrack = track; // avoid circular ownership in track callback lambdas

			if( inputType == InputType::FILE_PLAYER ) {
				track->mFilePlayerNode = ci::audio::master()->makeNode<ci::audio::FilePlayerNode>();
				track->mFilePlayerNode->setLoopEnabled( loopEnabled );
				track->mFilePlayerNode->setName( "FilePlayerNode (" + track->mSampleFileName + ")" );
				track->mInputNode = track->mSamplePlayerNode = track->mFilePlayerNode;
				track->mConnAudioFile = ma::assets()->getFile( audioFilePath, [this, weakTrack]( DataSourceRef dataSource ) {
					auto source = ci::audio::load( dataSource, ci::audio::master()->getSampleRate() );
					auto track = weakTrack.lock();
					track->mFilePlayerNode->setSourceFile( source );
					CI_LOG_I( "\t- [FilePlayerNode] loaded SourceFile with source at path: " << dataSource->getFilePath() );
					
				} );
			}
			else if( inputType == InputType::BUFFER_PLAYER ) {
				track->mBufferPlayerNode = ci::audio::master()->makeNode<ci::audio::BufferPlayerNode>();
				track->mBufferPlayerNode->setLoopEnabled( loopEnabled );
				track->mBufferPlayerNode->setName( "BufferPlayerNode (" + track->mSampleFileName + ")" );
				track->mInputNode = track->mSamplePlayerNode = track->mBufferPlayerNode;
				auto bufferIt = mBufferCache.find( audioFilePath );
				if( bufferIt != mBufferCache.end() ) {
					track->mBufferPlayerNode->setBuffer( bufferIt->second.first );
					CI_LOG_I( "\t- [BufferPlayerNode] loaded Buffer from cache with source at path: " << audioFilePath );
				}
				track->mConnAudioFile = ma::assets()->getFile( audioFilePath, [this, audioFilePath, weakTrack]( DataSourceRef dataSource ) {
					// TODO: load to BufferRef async

					auto timeLastWrite = fs::last_write_time( dataSource->getFilePath() );
					auto bufferIt = mBufferCache.find( audioFilePath );
					if( bufferIt != mBufferCache.end() && timeLastWrite <= bufferIt->second.second ) {
						CI_LOG_I( "\t---- loaded Buffer already up to date." );
						return;
					}

					auto source = ci::audio::load( dataSource, ci::audio::master()->getSampleRate() );
					auto buffer = source->loadBuffer();
					mBufferCache[audioFilePath] = { buffer, timeLastWrite };
					auto track = weakTrack.lock();
					track->mBufferPlayerNode->loadBuffer( source );
					CI_LOG_I( "\t- [BufferPlayerNode] loaded SourceFile with source at path: " << dataSource->getFilePath() );
				} );
			}

			if( trackInfo.contains( "loopBegin" ) ) {
				float loopBegin = trackInfo.get<double>( "loopBegin" );
				track->mSamplePlayerNode->setLoopBeginTime( loopBegin );
			}
			if( trackInfo.contains( "loopEnd" ) ) {
				float loopEnd = trackInfo.get<double>( "loopEnd" );
				track->mSamplePlayerNode->setLoopEndTime( loopEnd );
			}

			// connect up whoever was assigned as input
			track->mInputNode >> track->mGain >> track->mMonitorSpectralNode;
		}
		else if( inputType == InputType::INPUT_DEVICE ) {
			const auto deviceName = trackInfo.get<string>( "deviceName" );

			const size_t channelOffset = trackInfo.get<size_t>( "channelOffset" );
			const size_t numChannels = trackInfo.get<size_t>( "numChannels" );

			// If a different track is already using this Input
			auto devIt = mSharedInputDeviceNodes.find( deviceName );
			if( devIt != mSharedInputDeviceNodes.end() ) {
				track->mInputDeviceNode = devIt->second;
			}
			else if( deviceName == "default" ) {
				track->mInputDeviceNode = ctx->createInputDeviceNode();
			}
			else {
				auto inputDevice = ci::audio::Device::findDeviceByName( deviceName ); // FIXME: need to protect against grabing an output device by name. I thought I added other methods for this?
				if( ! inputDevice ) {
					CI_LOG_E( "could not find input device with name: " << deviceName );
					continue;
				}
				CI_ASSERT( inputDevice->getNumInputChannels() > 0 );

				track->mInputDeviceNode = ctx->createInputDeviceNode( inputDevice );
				track->mInputNode = track->mInputDeviceNode;
			}

			if( channelOffset == 0 && numChannels == track->mInputDeviceNode->getDevice()->getNumInputChannels() ) {
				track->mInputDeviceNode >> track->mGain >> track->mMonitorSpectralNode;
			}
			else {
				// we need a router to split up channels. But first check that we won't go out of range in the device's channels
				if( channelOffset + numChannels > track->mInputDeviceNode->getDevice()->getNumInputChannels() ) {
					CI_LOG_E( "channel params out of range for track '" << id << "'. channelOffset: " << channelOffset << ", numChannels: " << numChannels );
					continue;
				}
				auto router = ctx->makeNode( new ci::audio::ChannelRouterNode( ci::audio::Node::Format().channels( 1 ) ) );
				track->mInputDeviceNode >> track->mGain >> router->route( channelOffset, 0, numChannels ) >> track->mMonitorSpectralNode;			
			}
			track->mInputDeviceNode->setName( "InputDeviceNode (" + deviceName + ")" );
			track->mInputNode = track->mInputDeviceNode;
		}

		track->setBarkBandsEnabled( true ); // TODO: make optional?

		mTracks.push_back( track );
	}

	seek( 0, false );

	CI_LOG_I( "complete." );
	mSignalTracksChanged.emit();
}

ci::fs::path AudioAnalyzer::resolvePathFromUrl( const std::string &url )
{
	auto result = mSignalResolvePath.emit( url );
	if( ! result.empty() )
		return result;

	// default: try ci::app asset system
	return app::getAssetPath( url );
}

const ma::audio::TrackRef& AudioAnalyzer::getTrack( int index ) const
{
	if( index < mTracks.size() ) {
		return mTracks[(size_t)index];
	}

	static ma::audio::TrackRef sNullTrack;
	return sNullTrack;
}

// You need to hold down control for all audio hot-keys
void AudioAnalyzer::keyDown( app::KeyEvent &event )
{
	bool handled = true;
	if( event.isShiftDown() ) {
		if( event.getChar() == '?' ) {
			setEnabled( ! ci::audio::master()->isEnabled() );
			CI_LOG_I( "audio::Context enabled: " << boolalpha << ci::audio::master()->isEnabled() << ", AudioAnalyzer paused: " << mIsPaused << dec );
		}
		else if( event.getChar() == ' ' ) {
			setPaused( ! isPaused() );
			CI_LOG_I( "playing: " << ! isPaused() );
		}
		else if( event.getChar() == 'P' ) {
			printInfo();
		}
		else if( event.getChar() == 'D' ) {
			CI_LOG_I( "audio hardware devices devices:\n" << ci::audio::Device::printDevicesToString() );
		}
		else if( event.getChar() == '_' ) {
			float gain = getMasterGain() - 0.1f;
			CI_LOG_I( "setting master gain to: " << gain );
			setMasterGain( gain );
		}
		else if( event.getChar() == '+' ) {
			float gain = getMasterGain() + 0.1f;
			CI_LOG_I( "setting master gain to: " << gain );
			setMasterGain( gain );
		}
		else
			handled = false;
	}
	else
		handled = false;
	
	event.setHandled( handled );
}

void AudioAnalyzer::update()
{
	bool analyzerParamsEnabled = ma::hud()->checkBox( "analyzer params", false )->isEnabled();
	if( analyzerParamsEnabled && ! mTracks.empty() ) {
		const auto &firstTrack = mTracks[0];
		float volumeSmoothning = firstTrack->getVolumeSmoothFactor(); 
		float spectrumSmoothning = firstTrack->getSpectrumSmoothFactor();

		ma::hud()->slider( &volumeSmoothning, "volume smoothing" );
		ma::hud()->slider( &spectrumSmoothning, "spectrum smoothing" );

		float minMagValue = 0;
		ma::hud()->slider( &minMagValue, "min mag", ma::Hud::Options().min( 0 ).max( 100 ) );

		for( const auto &track : mTracks ) {
			track->setVolumeSmoothingFactor( volumeSmoothning );
			track->setSpectrumSmoothingFactor( spectrumSmoothning );
			track->setMinMagValue( minMagValue );
		}
	}

	for( const auto &track : mTracks ) {
		track->update();
	}	
}

void AudioAnalyzer::printInfo()
{
	stringstream str;
	auto ctx = ci::audio::master();

	str << "\n-------------- Context info --------------\n";
	str << "enabled: " << boolalpha << ctx->isEnabled() << ", samplerate: " << ctx->getSampleRate() << ", frames per block: " << ctx->getFramesPerBlock() << endl;
	str << "-------------- Tracks: " << mTracks.size() << " --------------\n";
	for( size_t i = 0; i < mTracks.size(); i++ ) {
		const auto &track = mTracks[i];
		str << "[" << i << "] '" << track->getId() << "'";	
		str << " gain: " << track->getGain();
		
		str << endl;
	}
	
	str << "-------------- Graph configuration: --------------" << endl;
	str << ci::audio::master()->printGraphToString();
	str << "--------------------------------------------------" << endl;

	CI_LOG_I( str.str() );
}

} } // namespace mason::audio
