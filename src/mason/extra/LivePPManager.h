#pragma once

#include "cinder/Filesystem.h"
#include "cinder/Signals.h"

namespace mason {

class LivePPManager {
  public:
	static LivePPManager* instance();

	//! Connect a callback that will be called before the executable is patched (hot loaded).
	static ci::signals::Connection connectPrePatch( const std::function<void ()> &fn );
	//! Connect a callback that will be called after the executable is patched (hot loaded).
	static ci::signals::Connection connectPostPatch( const std::function<void ()> &fn );

	ci::signals::Signal<void ()>&	getSignalPrePatch()		{ return mSignalPrePatch; }
	ci::signals::Signal<void ()>&	getSignalPostPatch()	{ return mSignalPostPatch; }

	//! called from freestanding initLivePP method.
	bool initLivePP( const ci::fs::path &LivePPPath, const std::string &groupName );

private:
	LivePPManager();

	ci::signals::Signal<void ()>	mSignalPrePatch, mSignalPostPatch;
	ci::signals::ScopedConnection	mConnUpdate;
};

//! Called from the main app or executable to enabled Live++. Returns true on successful init.
//! \a LivePPPathRelativeToProject is the relative path from the executable's vcxproj file to the LivePP folder.
bool initLivePP( const ci::fs::path &LivePPPathRelativeToProject, const std::string &groupName );

} // namespace mason
