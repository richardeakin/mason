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

#pragma once

#include "mason/Factory.h"
#include "mason/FileWatcher.h"

#include "cinder/DataSource.h"
#include "cinder/Timeline.h"

#include "jsoncpp/json.h"

#include <map>

namespace mason {

typedef std::shared_ptr<class MotionBase>		MotionBaseRef;

struct KeyFrameBase {
	float		duration, delay;
	ci::EaseFn	easeFn;
};

//! Base class used to store in container, concrete subclasses are templated
class MotionBase {
public:

	virtual void parseKeyFrames( const Json::Value &value ) = 0;
	virtual void play() = 0;

//	static void createNative( Dart_NativeArguments args );

	const std::string&	getIdentifier() const	{ return mIdentifier; }

protected:
	MotionBase( const std::string &identifier )
	: mIdentifier( identifier )
	{}

	void parseKeyFrameBase( KeyFrameBase *keyFrame, const Json::Value &value );

	std::string		mIdentifier;
};

template <typename T>
struct KeyFrame : public KeyFrameBase {
	T			valueEnd;
};

template <typename T>
class Motion : public MotionBase {
public:
	Motion( const std::string &identifier )
	: MotionBase( identifier ), mValue( &mValueInternal )
	{}
	Motion( const std::string &identifier, ci::Anim<T> *anim )
	: MotionBase( identifier ), mValue( anim )
	{}

	const T& getValue() const		{ return *mValue; }

	void parseKeyFrames( const Json::Value &value ) override;
	void play() override;

private:
	T							mValueBegin;
	ci::Anim<T>					mValueInternal;
	ci::Anim<T>*				mValue;
	std::vector<KeyFrame<T>>	mKeyFrames;
};

template<typename T> using MotionRef = std::shared_ptr<class Motion<T>>;

class MotionManager {
  public:
	typedef std::map<std::string, MotionBaseRef>	MotionContainerT;

	static MotionManager* instance();

	void load( const ci::fs::path &scriptPath );

	const MotionContainerT&	getMotions() const	{ return mMotions; }
	MotionContainerT&		getMotions()		{ return mMotions; }

	void clear();

	void createMotion( const Json::Value &value );

	void collectEvents();

	template <typename T>
	void add( ci::Anim<T> *anim, const std::string &identifier );

	template <typename T>
	void registerBuildType( const std::string &scriptTypeName );

	template <typename T>
	T getValueForObject( const std::string &identifier );

	template <typename T>
	MotionRef<T> getMotion( const std::string &identifier );

	ci::TimelineRef	getTimeline() const	{ return mTimeline; }

  private:
	MotionManager();

	void registerBuilders();

	Factory<MotionBase, std::string>	mMotionFactory;

	MotionContainerT		mMotions;
	ci::TimelineRef			mTimeline;

	Json::Value				mRoot;
	ci::fs::path			mCurrentScriptPath;
};

// ----------------------------------------------------------------------------------------------------
// Motion implementation
// ----------------------------------------------------------------------------------------------------

template <typename T>
void MotionManager::registerBuildType( const std::string &scriptTypeName )
{
	mMotionFactory.registerBuilder<Motion<T>>( scriptTypeName );
}

template <typename T>
void MotionManager::add( ci::Anim<T> *anim, const std::string &identifier )
{
	auto motion = std::make_shared<Motion<T>>( identifier, anim );

	mMotions[identifier] = motion;
}

template <typename T>
MotionRef<T> MotionManager::getMotion( const std::string &identifier )
{
	auto objIt = mMotions.find( identifier );
	if( objIt == mMotions.end() ) {
//		CI_LOG_E( "could not find object with id: " << identifier );
		return {};
	}

	return std::dynamic_pointer_cast<Motion<T> >( objIt->second );
}

template <typename T>
T MotionManager::getValueForObject( const std::string &identifier )
{
	auto typedObj = getMotion<T>( identifier );
	if( ! typedObj ) {
//		CI_LOG_E( "could not cast object to specified type: " << typeid( T ).name() );
		return {};
	}
	
	return typedObj->getValue();
}

} // namespace mason
