/*
 Copyright (c) 2014, Richard Eakin - All rights reserved.

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

#include "cinder/Signals.h"
#include "cinder/Log.h"
#include "cinder/Noncopyable.h"

// for Dispatch:
#include "cinder/Timeline.h"

#include "mason/Mason.h"

#include <vector>
#include <map>
#include <string>

namespace mason {

extern const char* NOTIFY_RESOURCE_RELOADED;
extern const char* NOTIFY_ERROR;

struct MA_API Notification {
};

// TODO: rework into Dispatch class?
class MA_API NotificationCenter : private ci::Noncopyable {
  public:

	typedef std::function<void ( const Notification& )> NotificationCallback;

	static void post( const char *name, const Notification &notification = Notification() )		{ instance()->postImpl( name, notification ); }
	static void listen( const char *name, const NotificationCallback &slot )					{ instance()->listenImpl( name, slot ); }

  private:
	static NotificationCenter* instance();

	void postImpl( const char *name, const Notification &notification );
	void listenImpl( const char *name, const NotificationCallback &slot );

	std::map<const char*, ci::signals::Signal<void ( const Notification & )> >	mRegisteredNotificiations;
};

//! Posts a NOTIFY_ERROR Notification whenever a log message of ERROR or higher occurs.
class MA_API LoggerNotification : public ci::log::Logger {
  public:
	void write( const ci::log::Metadata &meta, const std::string &text ) override;
};

// TODO: consider naming Scheduler
// - but maybe Score should be called scheduler. Need to look closer at the conceptual differences between the two
class MA_API Dispatch {
  public:
	// TODO (later): return a handle that can allow canceling
	static void once( double delaySeconds, const std::function<void ()> &func );

	//! Calls \a func on the main thread. If currently on a background thread, the call is async.
	static void onMain( const std::function<void ()> &func );

	static void setTimeline( const ci::TimelineRef &timeline );

	static ci::Timeline*	getTimeline();
};

} // namespace mason
