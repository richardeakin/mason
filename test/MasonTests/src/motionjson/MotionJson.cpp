/*
 Copyright (c) 2015, Richard Eakin - All rights reserved.

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

#include "MotionJson.h"
#include "mason/ScriptEaseFunctions.h"
#include "mason/Common.h"

#include "cinder/app/App.h"
#include "cinder/Utilities.h"

//#define LOG_MOTION_LOAD( args )		CI_LOG_V( args )
#define LOG_MOTION_LOAD( args )		((void)0)

using namespace ci;
using namespace std;

namespace mason {

// ----------------------------------------------------------------------------------------------------
// MotionManager
// ----------------------------------------------------------------------------------------------------

// static
MotionManager* MotionManager::instance()
{
	static MotionManager sInstance;
	return &sInstance;
}

MotionManager::MotionManager()
{
	initializeDartVM();
	registerBuilders();

	mTimeline = Timeline::create();
//	mTimeline->setLoop( true );
	app::timeline().add( mTimeline );
}

void MotionManager::registerBuilders()
{
	registerBuildType<float>(	"num"	  );
	registerBuildType<vec2>(	"Vector2" ); // vector_math.dart  types
	registerBuildType<vec3>(	"Vector3" );
	registerBuildType<vec4>(	"Vector4" );
	registerBuildType<vec2>(	"vec2"	  ); // cinder.dart vector types
	registerBuildType<vec3>(	"vec3"	  );
	registerBuildType<vec4>(	"vec4"	  );
	registerBuildType<Color>(	"Color"	  );
	registerBuildType<ColorA>(	"ColorA"  );
}

void MotionManager::load( const fs::path &scriptPath )
{
//	clear(); // not clearing for now to support looking at external anims

	try {
//		mScript = cidart::Script::create( scriptPath, mScriptOpts );

		string dataString = loadString( loadFile( scriptPath ) );

		Json::Reader reader;
		Json::Value data;
		bool success = reader.parse( dataString, data );
		CI_VERIFY( success);

		mCurrentScriptPath = scriptPath;

		collectEvents();
	}
	catch( Exception &exc ) {
		CI_LOG_EXCEPTION( "error creating Script", exc );
	}
}

void MotionManager::clear()
{
	mMotions.clear();
	mTimeline->clear();
}

namespace {

// There seems to be no better way to determine the T type for a generic obj than parsing it out of the class name
// - I tried using the instance type of "_value", but this is unreliable since it can also be of type Null.
//string parseValueType( Dart_Handle motionHandle )
//{
//	//	Dart_Handle valHandle = cidart::getField( motionHandle, "_value" );
//	//	CIDART_CHECK( valHandle );
//
//	//	string typeName = cidart::getTypeName( valHandle );
//
//	Dart_Handle thisObjClass = Dart_GetObjClass( motionHandle );
//	CIDART_CHECK( thisObjClass );
//	string thisObjClassName = cidart::getValue<string>( Dart_ToString( thisObjClass ) );
//
//	size_t beginTPos = thisObjClassName.find( "<" );
//	size_t endTPos = thisObjClassName.find( ">" );
//
//	CI_ASSERT( beginTPos != string::npos && endTPos != string::npos );
//
//	return thisObjClassName.substr( beginTPos + 1, endTPos - beginTPos - 1 );;
//}

} // anonymous namespace

void MotionManager::createMotion( const Json::Value &value )
{
#if 0
	string identifier = cidart::getField<string>( handle, "_nativeId" );

	// first check if we already have a Motion with this identifier and use that if we can
	auto motionIt = mMotions.find( identifier );
	if( motionIt != mMotions.end() ) {
		LOG_MOTION_LOAD( "Loading already constructed motion with identiier: " << identifier );
		motionIt->second->parseKeyFrames( handle );
	}
	else {
		string valueType = parseValueType( handle );
		LOG_MOTION_LOAD( "loading new Motion with value type: " << valueType );

		MotionBaseRef motion = mMotionFactory.build( valueType, identifier );
		if( ! motion ) {
			CI_LOG_E( "type not yet supported: " << valueType );
			return;
		}

		motion->parseKeyFrames( handle );

		mMotions.insert( make_pair( motion->getIdentifier(), motion ) );
	}
#endif
}

void MotionManager::collectEvents()
{
#if 0
	cidart::DartScope  enterScope;

	// FIXME: Dart_ThrowException is causing assertion failuer in dart internals on script error
	// - repro by using getField on wrong name

	Dart_Handle mapHandle = mScript->invoke( "allMotions" );
	CIDART_CHECK( mapHandle );

	Dart_Handle keysList = Dart_MapKeys( mapHandle );
	CIDART_CHECK( keysList );

	intptr_t numKeys;
	CIDART_CHECK( Dart_ListLength( keysList, &numKeys ) );

	for( intptr_t i = 0; i < numKeys; i++ ) {
		Dart_Handle keyHandle = Dart_ListGetAt( keysList, i );
		Dart_Handle valueHandle = Dart_MapGetAt( mapHandle, keyHandle );

		string keyString = cidart::getValue<string>( keyHandle );
		//		info[keyString] = valueHandle;

		LOG_MOTION_LOAD( "[" << i << "] " << keyString  );

		createMotion( valueHandle );
	}
#endif
}

// ----------------------------------------------------------------------------------------------------
// MotionBase
// ----------------------------------------------------------------------------------------------------

void MotionBase::parseKeyFrameBase( KeyFrameBase *keyFrame, const Json::Value &value )
{
#if 0
	keyFrame->duration = cidart::getField<float>( frameHandle, "duration" );
	keyFrame->delay = cidart::getField<float>( frameHandle, "delay" );

	// parse ease function
	Dart_Handle easeHandle = cidart::getField( frameHandle, "ease" );
	CIDART_CHECK( easeHandle );

	auto easeId = cidart::getField<std::string>( easeHandle, "_id" );

	// TODO: parse out special ease fns in more generic way
	if( easeId == "easeInOutAtan" ) {
		float arg0 = cidart::getField<float>( easeHandle, "_arg0" );
		keyFrame->easeFn = EaseInOutAtan( arg0 );
	}
	else if( easeId == "easeLookup" ) {
		vector<float> lookupTable( 100 );
		for( size_t i = 0; i < lookupTable.size(); i++ ) {
			float t = lmap<float>( (float)i, 0, lookupTable.size() - 1, 0, 1 );

			Dart_Handle evalArgs[] = { Dart_NewDouble( t ) };
			Dart_Handle returnHandle = Dart_Invoke( easeHandle, cidart::toDart( "processLookup" ), 1, evalArgs );
			if( Dart_IsError( returnHandle ) )
				throw cidart::DartException( string( "could not process eval, why: " ) + Dart_GetError( returnHandle ) );

			lookupTable[i] = cidart::getValue<float>( returnHandle );
		}

		keyFrame->easeFn = EaseLookup( lookupTable );

	}
	else {
		keyFrame->easeFn = getEaseMap().at( easeId );
	}

#endif
}

// ----------------------------------------------------------------------------------------------------
// Motion<T>
// ----------------------------------------------------------------------------------------------------

template <typename T>
void Motion<T>::parseKeyFrames( const Json::Value &value )
{
#if 0
	*mValue = mValueBegin = cidart::getField<T>( objHandle, "_valueBegin" );

	Dart_Handle keysFramesHandle = cidart::getField( objHandle, "keyFrames" );
	CIDART_CHECK( keysFramesHandle );

	intptr_t numFrames;
	CIDART_CHECK( Dart_ListLength( keysFramesHandle, &numFrames ) );

	mKeyFrames.resize( numFrames );

	for( intptr_t i = 0; i < numFrames; i++ ) {
		Dart_Handle frameHandle = Dart_ListGetAt( keysFramesHandle, i );

		auto &keyFrame = mKeyFrames[i];

		keyFrame.valueEnd = cidart::getField<T>( frameHandle, "valueEnd" );
		parseKeyFrameBase( &keyFrame, frameHandle );
	}
#endif
}

template <typename T>
void Motion<T>::play()
{
	*mValue = mValueBegin;

	if( mKeyFrames.empty() )
		return;

	auto timeline = MotionManager::instance()->getTimeline();

	// frame 0
	{
		auto &frame = mKeyFrames[0];
		timeline->apply( mValue, frame.valueEnd, frame.duration, frame.easeFn ).delay( frame.delay );
	}

	for( size_t i = 1; i < mKeyFrames.size(); i++ ) {
		auto &frame = mKeyFrames[i];
		timeline->appendTo( mValue, frame.valueEnd, frame.duration, frame.easeFn ).delay( frame.delay );
	}
}

template class Motion<float>;
template class Motion<vec2>;
template class Motion<vec3>;
template class Motion<vec4>;
template class Motion<Color>;
template class Motion<ColorA>;

} // namespace mason
