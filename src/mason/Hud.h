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

#include "mason/Mason.h"
#include "mason/Var.h"
#include "mason/ShaderControls.h"

#include "vu/Graph.h"
#include "vu/Label.h"
#include "vu/Control.h"

#include "cinder/gl/GlslProg.h"

#include <any>
#include <map>

namespace mason {

// TODO: maybe Hud should be a View itself that defaults to the size of the window
// - still want it to be able to create its own vu::Graph if one doesn't exist
// - could also perhaps inherit from Graph
//    - this is problematic as it means that Hud inherits all of Graph's public api, which isn't very nice
class MA_API Hud : public VarOwner {
  public:
	enum class ViewPositioning {
		RELATIVE,
		ABSOLUTE
	};

	//! Optional parameters passed in when adding a custom View or control
	struct Options {
		Options() {}

		Options&	positioning( ViewPositioning p )	{ mPositioning = p; return *this; }
		Options&	immediateMode( bool im )			{ mImmediateMode = im; return *this; }
		Options&	min( float x )						{ mMin = x; mMinSet = true; return *this; }
		Options&	max( float x )						{ mMax = x; mMaxSet = true; return *this; }
		Options&	step( float x )						{ mStep = x; mStepSet = true; return *this; }

		ViewPositioning mPositioning = ViewPositioning::RELATIVE;
		bool			mImmediateMode = false;
		float			mMin = 0;
		float			mMax = 0;
		float			mStep = 0;

		bool			mMinSet = false;
		bool			mMaxSet = false;
		bool			mStepSet = false;
	};

	static Hud* instance();

	void draw();

	void clear();

	//! Sets the bounds that the Hud uses to draw into window. By default, it is the full size of the window.
	void setBounds( const ci::Rectf &bounds );
	//! Sets the bounds to match the size of the app::Window (this is the default).
	void setFillWindow();
	//! Returns whether the bounds matches the size of the app::Window (true until you call setBounds()).
	bool isFillWindowEnabled() const	{ return mFullScreen; }

	vu::GraphRef getGraph() const					{ return mGraph; }
	vu::StrokedRectViewRef	getBorderView() const	{ return mBorderView; }
	//! Returns a LabelGrid that users can display information with.
	vu::LabelGridRef    getInfoLabel() const { return mInfoLabel; }

	void addView( const vu::ViewRef &view, const std::string &label, const Options &options = Options() );
	void removeView( const vu::ViewRef &view );
	void removeViewsWithPrefix( const std::string &prefix );

	const vu::ViewRef&	findView( const std::string &label ) const;

	//! Parses the Shader's format and source, finds uniforms that have a 'hud:' id in them, adds controls for them.
	void addShaderControls( const ci::gl::GlslProgRef &shader, const std::vector<std::pair<ci::fs::path, std::string>> &shaderSources );

	ma::Var<bool>* getVarDrawUpdateIndicators() { return &mDrawUpdateIndicators; }

	//! Adds a slider to the Hud that manipulates \c x in 'immediate mode', that is it must be called once per draw loop.
	vu::HSliderRef slider( float *x, const std::string &label, Options options = Options() );
	//! Adds a slider to the Hud that manipulates \c x in 'persistent mode', and is only removed once `x` is destroyed (it is not owned by the Hud).
	vu::HSliderRef slider( Var<float> *x, const std::string &label, Options options = Options() );
	//! Adds a CheckBox to the Hud that manipulates \c x in 'immediate mode', that is it must be called once per draw loop.
	vu::CheckBoxRef checkBox( bool *x, const std::string &label, Options options = Options() );
	//! Adds a CheckBox to the Hud in 'immediate mode', that is it must be called once per draw loop. Get the value by called `isEnabled()` on the result.
	vu::CheckBoxRef checkBox( const std::string &label, bool defaultValue = false, const Options &options = Options() );
	//! Adds a CheckBox to the Hud that manipulates \c x in 'persistent mode', and is only removed once `x` is destroyed (it is not owned by the Hud).
	vu::CheckBoxRef checkBox( Var<bool> *x, const std::string &label, Options options = Options() );
	// TODO: add overloads that take step, min, and max sampe as ImGui
	//!
	template <typename T>
	std::shared_ptr<vu::NumberBoxT<T>> numBox( T *x, const std::string &label, Options options = Options() );
	//!
	template <typename T>
	std::shared_ptr<vu::NumberBoxT<T>> numBox( Var<T> *x, const std::string &label, Options options = Options() );

	//! Displays the app's average frames per second at row index 0 in the info panel
	void showFps( bool show = true )	{ mShowFps = show; }
	//! Displays a set of strings at the provided row index.
	void showInfo( size_t rowIndex, const std::vector<std::string> &textColumns );
	//! Displays a pair of strings at the provided row index.
	void showInfo( size_t rowIndex, const std::string &leftText, const std::string &rightText );
	//! Hides the Hud, it will still update but not be drawn
	void	setHidden( bool hidden = true )			{ mGraph->setHidden( hidden ); }
	bool	isHidden() const						{ return mGraph->isHidden(); }

	// VarOwner implementation. TODO: make private or protected if possible
	void removeTarget( void *target ) override;
	void cloneAndReplaceTarget( void *target, void *replacementTarget ) override;

	//! Sets the value of an Attrib managed by the Hud (usually the value of a Control) to \t result. Returns success or failure.
	template <typename T>
	bool	getAttribValue( const std::string &label, T *result ) const;

private:
	//! Private data mapped to each View
	struct Attribs {
		Attribs() {}

		Options mOptions;
		bool	mMarkedForRemoval = false;

		std::any	mAnyValue;
		void*		mPointerToValue = nullptr;
	};

	Hud();
	void initViews();
	void registerSignals();
	void registerNotifications();
	void resizeInfoLabel();
	void clearViewsMarkedForRemoval();
	void updateNotificationBorder();

	vu::HSliderRef findOrMakeSlider( float initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );
	vu::CheckBoxRef findOrMakeCheckBox( bool initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );

	template <typename T>
	std::shared_ptr<vu::NumberBoxT<T>> findOrMakeNumberBoxN( const T &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );

	void layout();
	void update();

	vu::GraphRef			mGraph;
	vu::StrokedRectViewRef	mBorderView;
	vu::LabelGridRef		mInfoLabel;
	vu::ViewRef				mUserViewsGrid; // child of mUserViews
	vu::ViewRef				mUserViewsFreeFloating; // child of mUserViews

	std::map<vu::ViewRef, Attribs>	mViewAttribs;

	ma::Var<bool>		mDrawUpdateIndicators = true;
	bool		mShouldIndicateFailure = false;
	bool		mShouldIndicateSuccess = false;
	bool		mIsInidicatingFailure = false;
	bool		mFullScreen = true;

	bool	mShowFps = true;

	std::vector<ShaderControlGroup>		mShaderControlGroups;
};

template <typename T>
bool Hud::getAttribValue( const std::string &label, T *result ) const
{
	for( const auto &vp : mViewAttribs ) {
		if( vp.first->getLabel() == label ) {
			const auto &attrib = vp.second;
			try {
				*result = *boost::any_cast<T*>( attrib.mAnyValue );
				return true;
			}
			catch( boost::bad_any_cast & ) {
				// keep trying..
			}
		}
	}

	return false; // couldn't find
}

//! Returns a pointer to the global Hud instance
MA_API Hud* hud();

} // namespace mason
