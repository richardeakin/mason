/*
 Copyright (c) 2017, Richard Eakin - All rights reserved.

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

#include "cinder/Cinder.h"
#include "cinder/audio/Context.h"

namespace mason { namespace audio {

using OfflineContextRef = std::shared_ptr<class OfflineContext>;
using OutputOfflineNodeRef = std::shared_ptr<class OutputOfflineNode>;

class OutputOfflineNode : public ci::audio::OutputNode {
  public:
	OutputOfflineNode( ci::audio::Node::Format &format = ci::audio::Node::Format() );

	size_t getOutputSampleRate() override	{ return mSampleRate; }
	size_t getOutputFramesPerBlock() override	{ return mFramesPerBlock; }

	void initialize() override;

  private:

	size_t mSampleRate = 48000;
	size_t mFramesPerBlock = 2048;

	friend class OfflineContext;
};


class OfflineContext : public ci::audio::Context {
  public:
    OfflineContext();

	// overriddden to control our OutputNode copies (don't really need to do anything)
	void disable() override;

	void setSampleRate( size_t sampleRate );
	void setFramesPerBlock( size_t numFrames );
	void setOuputNumChannels( size_t numChannels );
	
	// TODO: no need to have these methods here. Probably want the current ci::audio::Context class to be something like RealtimeContext / ContextRealtime
	ci::audio::OutputDeviceNodeRef	createOutputDeviceNode( const ci::audio::DeviceRef &device, const ci::audio::Node::Format &format = ci::audio::Node::Format() )	override	{ return nullptr; }
	ci::audio::InputDeviceNodeRef	createInputDeviceNode( const ci::audio::DeviceRef &device, const ci::audio::Node::Format &format = ci::audio::Node::Format() )	override	{ return nullptr; }

	const ci::audio::OutputNodeRef& getOutput() override;

	void pull( ci::audio::Buffer *buffer );

	// TODO: this is pretty hacky, but I need to keep both an OutputNodeRef and OutputOfflineNodeRef
	// - reason being is that the virtual getOutput() returns a const ref to OutputNodeRef, which will slice the shared_ptr when it converts
	// - also pretty hack that I need to lazy load it still, as you can't use Context::makeNode<> from the Context's constructor
	OutputOfflineNodeRef		mOutputOffline;
};

} } // namespace mason::audio
