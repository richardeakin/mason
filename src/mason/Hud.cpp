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

#include "mason/Hud.h"
#include "mason/Notifications.h"

#include "cinder/app/App.h"
#include "cinder/Log.h"
#include "cinder/gl/gl.h"

#include "cppformat/format.h"
#include "glm/gtc/epsilon.hpp"

using namespace ci;
using namespace std;

//#define LOG_HUD( stream )	CI_LOG_I( stream )
#define LOG_HUD( stream )	((void)0)

//#define LOG_SHADER_CTL( stream )	CI_LOG_I( stream )
#define LOG_SHADER_CTL( stream )	((void)0)

const vec2 INFO_ROW_SIZE = vec2( 200, 20 );
const float PADDING = 6;

namespace mason {

Hud* hud()
{
	return Hud::instance();
}

// static
Hud* Hud::instance()
{
	static Hud *sInstance = nullptr;
	if( ! sInstance ) {
		sInstance = new Hud;
	}

	return sInstance;
}

Hud::Hud()
{
	initViews();
	registerSignals();
	registerNotifications();
}

void Hud::clear()
{
	mUserViewsGrid->setHidden( true );
	mUserViewsGrid->removeAllSubviews();

	mUserViewsFreeFloating->setHidden( true );
	mUserViewsFreeFloating->removeAllSubviews();

	mShaderControlGroups.clear();

	mViewAttribs.clear();
}

void Hud::registerSignals()
{
	mGraph->getWindow()->getSignalResize().connect( [this] {
		layout();
	} );

	app::App::get()->getSignalUpdate().connect( [] { hud()->update(); } );
#if defined( MASON_CLEANUP_AT_SHUTDOWN )
	app::App::get()->getSignalShutdown().connect( [] { delete sInstance; } );
#endif
}

void Hud::registerNotifications()
{
	NotificationCenter::listen( NOTIFY_ERROR, [this]( const Notification &notification ) {
		if( mDrawUpdateIndicators )
			mShouldIndicateFailure = true;
	} );
	NotificationCenter::listen( NOTIFY_RESOURCE_RELOADED, [this]( const Notification &notification ) {
		if( mDrawUpdateIndicators )
			mShouldIndicateSuccess = true;
	} );
}

void Hud::initViews()
{
	auto window = app::getWindow();

	mGraph = make_shared<ui::Graph>();
	mGraph->setSize( window->getSize() );
	mGraph->connectEvents( ui::Graph::EventOptions().priority( 100 ) );
	mGraph->setLabel( "Hud Graph" );

	mInfoLabel = make_shared<ui::LabelGrid>();
	mInfoLabel->setTextColor( Color::white() );
	mInfoLabel->getBackground()->setColor( ColorA::gray( 0, 0.3f ) );
	mGraph->addSubview( mInfoLabel );

	mUserViewsGrid = make_shared<ui::View>();
	mUserViewsGrid->setLabel( "user views (grid)" );
	mUserViewsGrid->setHidden( true );
	mUserViewsGrid->getBackground()->setColor( ColorA( 0, 0, 0.2f, 0.3f ) );
	mGraph->addSubview( mUserViewsGrid );

	{
		auto layout =  make_shared<ui::VerticalLayout>();
		layout->setPadding( 4 );
		mUserViewsGrid->setLayout( layout );
	}

	mUserViewsFreeFloating = make_shared<ui::View>();
	mUserViewsFreeFloating->setLabel( "user views (free floating)" );
	mUserViewsFreeFloating->setHidden( true );
	mUserViewsFreeFloating->setFillParentEnabled();
	mGraph->addSubview( mUserViewsFreeFloating );

	// FIXME: not adhearing to resize (even with setFillsParent)
	mBorderView = make_shared<ui::StrokedRectView>();
	mBorderView->setFillParentEnabled();
	mBorderView->setLineWidth( 12 );
	mBorderView->setPlacement( ui::StrokedRectView::Placement::INSIDE );
	mBorderView->setColor( ColorA::zero() );
	mGraph->addSubview( mBorderView );
}

void Hud::setBounds( const ci::Rectf &bounds )
{
	mFullScreen = false;
	mGraph->setBounds( bounds );
	layout();
}

void Hud::setFullScreen()
{
	mFullScreen = true;
	layout();
}

void Hud::addView( const ui::ViewRef &view, const string &label, const Options &options )
{
	if( findView( label ) ) {
		LOG_HUD( "removing old subview for label: " << label );
		removeView( view );
	}

	view->setLabel( label );

	if( options.mPositioning == ViewPositioning::ABSOLUTE ) {
		mUserViewsFreeFloating->addSubview( view );
		mUserViewsFreeFloating->setHidden( false );
	}
	else {
		mUserViewsGrid->addSubview( view );
		mUserViewsGrid->setHidden( false );
	}

	auto &attribs = mViewAttribs[view];
	attribs.mOptions = options;
}

const ui::ViewRef& Hud::findView( const std::string &label ) const
{
	for( const auto &vp : mViewAttribs ) {
		CI_ASSERT( vp.first );
		if( vp.first->getLabel() == label ) {
			return vp.first;
		}
	}

	// return a null ViewRef when we can't find a View with the corresponding label.
	static ui::ViewRef sNullView;
	return sNullView;
}

void Hud::removeView( const ui::ViewRef &view )
{
	auto it = mViewAttribs.find( view );
	if( it == mViewAttribs.end() ) {
		CI_LOG_W( "could not find view to remove: " << view->getName() );
		return;
	}

	if( it->second.mOptions.mPositioning == ViewPositioning::ABSOLUTE ) {
		mUserViewsFreeFloating->removeSubview( view );

		if( mUserViewsFreeFloating->getSubviews().empty() )
			mUserViewsFreeFloating->setHidden( true );
	}
	else {
		mUserViewsGrid->removeSubview( view );

		if( mUserViewsGrid->getSubviews().empty() )
			mUserViewsGrid->setHidden( true );
	}

	mViewAttribs.erase( it );
}

void Hud::removeViewsWithPrefix( const std::string &prefix )
{
	vector<ui::ViewRef> viewsToRemove;
	for( const auto &va : mViewAttribs ) {
		const auto &view = va.first;

		if( view->getLabel().empty() )
			continue;

		auto it = std::mismatch( prefix.begin(), prefix.end(), view->getLabel().begin() );

		if( it.first == prefix.end() )	{
			CI_LOG_I( "found prefix for label: " << view->getLabel() << ", removing" );
			viewsToRemove.push_back( view );
		}
	}

	for( const auto &view : viewsToRemove )
		removeView( view );
}

void Hud::clearViewsMarkedForRemoval()
{
	vector<ui::ViewRef>	viewsToRemove;
	for( const auto &va : mViewAttribs ) {
		if( va.second.mMarkedForRemoval )
			viewsToRemove.push_back( va.first );
	}

	for( const auto &view : viewsToRemove )
		removeView( view );
}

void Hud::resizeInfoLabel()
{
	const int numRows = mInfoLabel->getNumRows();
	vec2 windowSize = vec2( getGraph()->getSize() );
	vec2 labelSize = { INFO_ROW_SIZE.x, INFO_ROW_SIZE.y * numRows };
	mInfoLabel->setBounds( { windowSize - labelSize - PADDING, windowSize - PADDING } ); // anchor bottom right
}

void Hud::showInfo( size_t rowIndex, const std::vector<std::string> &textColumns )
{
	mInfoLabel->setRow( rowIndex, textColumns );
}

void Hud::layout()
{
	if( mFullScreen ) {
		mGraph->setPos( vec2( 0 ) );
		mGraph->setSize( app::getWindowSize() );
	}
	else
		mGraph->setNeedsLayout();

	auto offset = vec2( PADDING, PADDING );

	// TODO: set width to maximum subview width
	mUserViewsGrid->setSize( vec2( 200, mGraph->getHeight() ) );

	resizeInfoLabel();
}

void Hud::update()
{
	// remove any ShaderControlGroups that have an expired shader
	mShaderControlGroups.erase( remove_if( mShaderControlGroups.begin(), mShaderControlGroups.end(),
							[]( const ShaderControlGroup &group ) {
								bool shaderExpired = ! ( group.getShader() );
								if( shaderExpired ) {
									LOG_SHADER_CTL( "removing group group for expired shader: " << group.mLabel );
								}

								return shaderExpired;
							} ),
		mShaderControlGroups.end() );

	mGraph->propagateUpdate();

	if( mShowFps )
		mInfoLabel->setRow( 0, { "fps:",  fmt::format( "{}", app::App::get()->getAverageFps() ) } );

	if( ! glm::epsilonEqual( INFO_ROW_SIZE.y * mInfoLabel->getNumRows(), mInfoLabel->getHeight(), 0.01f ) )
		resizeInfoLabel();

	updateNotificationBorder();
}

void Hud::draw()
{
	clearViewsMarkedForRemoval();

	mGraph->propagateDraw();

	// Mark any non-persistent views and needing to be removed next loop, unless user interacts with it again
	for( auto &va : mViewAttribs ) {
		if( va.second.mOptions.mImmediateMode )
			va.second.mMarkedForRemoval = true;
	}
}

void Hud::updateNotificationBorder()
{
	// check if we should flash border to indicate success or failure notifications. success (green) only flashes if there wasn't a failure this frm
	if( mShouldIndicateFailure ) {
		mIsInidicatingFailure = true; // don't allow success to be indicated until failure completes
		app::timeline().apply( mBorderView->getColorAnim(), ColorA( 0.5f, 0, 0 ), ColorA( 0, 0, 0, 0 ), 1.2f, EaseInCubic() )
			.startTime( (float)app::getElapsedSeconds() )
			.finishFn( [this] { mIsInidicatingFailure = false; } )
		;
	}
	else if( ! mIsInidicatingFailure && mShouldIndicateSuccess ) {
		app::timeline().apply( mBorderView->getColorAnim(), ColorA( 0.1f, 0.5f, 0.1f ), ColorA( 0, 0, 0, 0 ), 0.9f, EaseInQuad() )
			.startTime( (float)app::getElapsedSeconds() )
		;
	}

	mShouldIndicateFailure = false;
	mShouldIndicateSuccess = false;
}

// ----------------------------------------------------------------------------------------------------
// Var manipulators
// ----------------------------------------------------------------------------------------------------

namespace {
const Rectf DEFAULT_SLIDER_BOUNDS = Rectf( 0, 0, 190, 40 );
const Rectf DEFAULT_CHECKBOX_BOUNDS = Rectf( 0, 0, 100, 40 );
}

ui::HSliderRef Hud::slider( float *x, const std::string &label, Options options )
{
	options.immediateMode( true );
	auto slider = findOrMakeSlider( *x, label, options, nullptr );

	*x = slider->getValue();

	return slider;
}

ui::HSliderRef Hud::slider( Var<float> *x, const std::string &label, Options options )
{
	options.immediateMode( false );
	x->setOwner( this );

	auto slider = findOrMakeSlider( *x, label, options,
				   [&x]( Attribs &attribs ) {
					   attribs.mAnyValue = x->ptr(); // store a pointer in a form that retains type information
					   attribs.mPointerToValue = x->ptr(); // also store the raw pointer so we don't need to any_cast during cloneAndReplaceTarget
				   } );

	// When the value changes, we use the slider to look up the current Attrib and then update the user's value pointer.
	auto sliderWeakPtr = weak_ptr<ui::HSlider>( slider );
	slider->getSignalValueChanged().connect( [this, sliderWeakPtr] {
		auto slider = sliderWeakPtr.lock();
		if( ! slider )
			return;

		auto &attribs = mViewAttribs[slider];
		if( attribs.mMarkedForRemoval )
			return;

		float *x = boost::any_cast<float *>( attribs.mAnyValue );
		*x = slider->getValue();
	} );

	return slider;
}

template <typename T>
std::shared_ptr<ui::NumberBoxT<T>> Hud::numBox( T *x, const std::string &label, Options options )
{
	options.immediateMode( true );
	auto nbox = findOrMakeNumberBoxN( *x, label, options, nullptr );

	*x = nbox->getValue();

	return nbox;
}

template <typename T>
std::shared_ptr<ui::NumberBoxT<T>> Hud::numBox( Var<T> *x, const std::string &label, Options options )
{
	options.immediateMode( false );
	x->setOwner( this );

	auto nbox = findOrMakeNumberBoxN( x->value(), label, options,
		[&x]( Attribs &attribs ) {
		attribs.mAnyValue = x->ptr(); // store a pointer in a form that retains type information
		attribs.mPointerToValue = x->ptr(); // also store the raw pointer so we don't need to any_cast during cloneAndReplaceTarget
	} );

	// When the value changes, we use the slider to look up the current Attrib and then update the user's value pointer.
	auto nboxWeakPtr = weak_ptr<ui::NumberBoxT<T>>( nbox );
	nbox->getSignalValueChanged().connect( [this, nboxWeakPtr] {
		auto nbox = nboxWeakPtr.lock();
		if( ! nbox )
			return;

		auto &attribs = mViewAttribs[nbox];
		if( attribs.mMarkedForRemoval )
			return;

		T *x = boost::any_cast<T *>( attribs.mAnyValue );
		*x = nbox->getValue();
	} );

	return nbox;
}

ui::CheckBoxRef Hud::checkBox( bool *x, const std::string &label, Options options )
{
	options.immediateMode( true );

	auto checkBox = findOrMakeCheckBox( *x, label, options, nullptr );
	*x = checkBox->isEnabled();

	return checkBox;
}

ui::CheckBoxRef Hud::checkBox( const std::string &label, bool defaultValue, const Options &options )
{
	bool x = defaultValue;
	return checkBox( &x, label, options );
}

ui::CheckBoxRef Hud::checkBox( Var<bool> *x, const std::string &label, Options options )
{
	options.immediateMode( false );
	x->setOwner( this );

	auto checkBox = findOrMakeCheckBox( *x, label, options,
					   [&x]( Attribs &attribs ) {
						   attribs.mAnyValue = x->ptr(); // store a pointer in a form that retains type information
						   attribs.mPointerToValue = x->ptr(); // also store the raw pointer so we don't need to any_cast during cloneAndReplaceTarget
					   } );

	// When the value changes, we use the slider to look up the current Attrib and then update the user's value pointer.
	auto checkBoxWeakPtr = weak_ptr<ui::CheckBox>( checkBox );
	checkBox->getSignalValueChanged().connect( [this, checkBoxWeakPtr] {
		auto checkBox = checkBoxWeakPtr.lock();
		if( ! checkBox )
			return;

		auto &attribs = mViewAttribs[checkBox];
		if( attribs.mMarkedForRemoval )
			return;

		bool *x = boost::any_cast<bool *>( attribs.mAnyValue );
		*x = checkBox->isEnabled();
	} );
	
	return checkBox;
}

ui::HSliderRef Hud::findOrMakeSlider( float initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn )
{
	const auto &view = findView( label );
	auto slider = dynamic_pointer_cast<ui::HSlider>( view );
	if( ! slider ) {
		slider = make_shared<ui::HSlider>( DEFAULT_SLIDER_BOUNDS );
		slider->setTitle( label );
		slider->getBackground()->setColor( ColorA::gray( 0, 0.6f ) );
		slider->setValue( initialValue );

		hud()->addView( slider, label, options );
	}

	if( options.mMinSet )
		slider->setMin( options.mMin );
	if( options.mMaxSet )
		slider->setMax( options.mMax );

	// mark as not needing to be removed yet, needed for both persistent and IM controls
	auto &attribs = mViewAttribs[slider];
	attribs.mMarkedForRemoval = false;

	if( updateAttribsFn )
		updateAttribsFn( attribs );

	return slider;
}

ui::CheckBoxRef Hud::findOrMakeCheckBox( bool initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn )
{
	const auto &view = findView( label );
	auto checkBox = dynamic_pointer_cast<ui::CheckBox>( view );
	if( ! checkBox ) {
		checkBox = make_shared<ui::CheckBox>( DEFAULT_CHECKBOX_BOUNDS );
		checkBox->setTitle( label );
		checkBox->setTitleColor( ColorA( 1, 1, 1, 1 ) );
		checkBox->setEnabled( initialValue );

		hud()->addView( checkBox, label, options );
	}

	// mark as not needing to be removed yet, needed for both persistent and IM controls
	auto &attribs = mViewAttribs[checkBox];
	attribs.mMarkedForRemoval = false;

	if( updateAttribsFn )
		updateAttribsFn( attribs );

	return checkBox;
}

template <typename T>
shared_ptr<ui::NumberBoxT<T>> Hud::findOrMakeNumberBoxN( const T &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn )
{
	const auto &view = findView( label );
	auto nbox = dynamic_pointer_cast<ui::NumberBoxT<T>>( view );
	if( ! nbox ) {
		nbox = make_shared<ui::NumberBoxT<T>>( DEFAULT_SLIDER_BOUNDS );
		nbox->setTitle( label );
		nbox->getBackground()->setColor( ColorA::gray( 0, 0.6f ) );
		nbox->setValue( initialValue );

		hud()->addView( nbox, label, options );
	}

	if( options.mMinSet )
		nbox->setMin( options.mMin );
	if( options.mMaxSet )
		nbox->setMax( options.mMax );
	if( options.mStepSet )
		nbox->setStep( options.mStep );

	// mark as not needing to be removed yet, needed for both persistent and IM controls
	auto &attribs = mViewAttribs[nbox];
	attribs.mMarkedForRemoval = false;

	if( updateAttribsFn )
		updateAttribsFn( attribs );

	return nbox;
}

// ----------------------------------------------------------------------------------------------------
// VarOwner
// ----------------------------------------------------------------------------------------------------

void Hud::removeTarget( void *target )
{
	if( ! target )
		return;

	for( auto &va : mViewAttribs ) {
		if( va.second.mPointerToValue == target ) {
			va.second.mMarkedForRemoval = true;
			return;
		}
	}
}

void Hud::cloneAndReplaceTarget( void *target, void *replacementTarget )
{
	if( ! target )
		return;

	for( auto &va : mViewAttribs ) {
		if( va.second.mPointerToValue == target ) {
			// TODO: need to clone or is it not necessary?
			va.second.mPointerToValue = replacementTarget;
			return;
		}
	}
	
}

// ----------------------------------------------------------------------------------------------------
// Shader Controls
// ----------------------------------------------------------------------------------------------------

namespace {

const std::string whitespaceChars = " \t";

//! Parses a floating point number. Returns true on success.
bool parseFloat( const std::string &valueStr, size_t *pos, float *result )
{
	if( *pos >= valueStr.size() )
		return false;

	size_t posValueBegin = valueStr.find_first_not_of( whitespaceChars, *pos + 1 ); // move forward to first value
	if( posValueBegin == string::npos )
		return false;

	size_t posValueEnd = valueStr.find_first_of( whitespaceChars + ",)", posValueBegin + 1 );
	if( posValueEnd == string::npos )
		return false;

	string valueTrimmed = valueStr.substr( posValueBegin, posValueEnd - posValueBegin );
	try {
		*result = stof( valueTrimmed );
		*pos = posValueEnd + 1;
	}
	catch( std::invalid_argument &exc ) {
		CI_LOG_EXCEPTION( "failed to convert value string '" << valueStr << "' to a float",  exc );
		return false;
	}

	return true;
}

//! Parses an expression in the form of "x = value", where value is a floating point number. Returns true on success.
bool parseFloat( const std::string &varName, const std::string &line, size_t pos, float *result )
{
	size_t posValueBegin = line.find( varName, pos );
	if( posValueBegin == string::npos )
		return false;

	// advance to first non-whitespace char after '='
	posValueBegin = line.find_first_of( "=", posValueBegin );
	if( posValueBegin == string::npos )
		return false;

	posValueBegin = line.find_first_not_of( whitespaceChars, posValueBegin + 1 );
	size_t posValueEnd = line.find_first_of( whitespaceChars + ",", posValueBegin + 1 );
	if( posValueEnd != string::npos )
		posValueEnd = line.size(); // If there was no more whitepsace or comma, set to end of line

	string valueStr = line.substr( posValueBegin, posValueEnd - posValueBegin );

	// TODO: use parseFloat() variant above
	try {
		*result = stof( valueStr );
		return true;
	}
	catch( std::invalid_argument &exc ) {
		CI_LOG_EXCEPTION( "failed to convert value string '" << valueStr << "' to a float",  exc );
	}
	
	return false;
}

//! Parses a vec2 from string into glm equivalent
void parseVec( const std::string &valueStr, vec2 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec2( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec2( result->x );
	}
}

//! Parses a vec3 from string into glm equivalent
void parseVec( const std::string &valueStr, vec3 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec3( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec3( result->x );
	}
	else if( ! parseFloat( valueStr, &pos, &result->z ) ) {
		// Parsing x and y succeeded, z failed.
		CI_ASSERT_NOT_REACHABLE();
	}
}

//! Parses a vec4 from string into glm equivalent
void parseVec( const std::string &valueStr, vec4 *result )
{
	size_t pos = valueStr.find_first_of( whitespaceChars + "(", 0 ); // move forward to end of 'vecN('
	if( ! parseFloat( valueStr, &pos, &result->x ) ) {
		*result = vec4( 0 );
	}
	else if( ! parseFloat( valueStr, &pos, &result->y ) ) {
		// Use the one-value constructor
		*result = vec4( result->x );
	}
	else if( ! parseFloat( valueStr, &pos, &result->z ) ) {
		// Parsing x and y succeeded, z failed.
		CI_ASSERT_NOT_REACHABLE();
	}
	else if( ! parseFloat( valueStr, &pos, &result->w ) ) {
		// Parsing x and y succeeded and z succeeded, w failed.
		CI_ASSERT_NOT_REACHABLE();
	}
}

} // anonymous namespace

void Hud::addShaderControls( const ci::gl::GlslProgRef &shader, const std::vector<std::pair<ci::fs::path, std::string>> &shaderSources )
{
	ShaderControlGroup group;

	for( const auto &sp : shaderSources ) {
		group.mLabel = sp.first.filename().string(); // taking the last source filename that we process as the group label

		// search for lines with "hud:" that begin with "uniform"
		istringstream input( sp.second );
		string line;
		while( getline( input, line ) ) {
			size_t posHudStr = line.find( "hud:" );
			if( posHudStr == string::npos )
				continue;

			// If we find a line that says 'hud: disable', then disregard controls for the entire shader (accounting for '// ' string
			// TODO: this should only disregard controls for the current source file
			if( posHudStr <= 3 && line.find( "hud: disable" ) <= 3 ) {
				LOG_SHADER_CTL( "shader controls disabled" );
				return;
			}

			size_t posFoundUniformStr = line.find( "uniform" );
			// don't evaluate uniforms that aren't at the beginning of the line, simple way to allow them to be commented out
			if( posFoundUniformStr != 0 )
				continue;

			// parse uniform type
			size_t posTypeBegin = posFoundUniformStr + 7; // advance to end of "uniform"
			posTypeBegin = line.find_first_not_of( whitespaceChars, posTypeBegin ); // find next non-whitespace char
			size_t posTypeEnd = line.find_first_of( whitespaceChars, posTypeBegin + 1 ); // find next whitespace char
			if( posTypeBegin == string::npos || posTypeEnd == string::npos ) {
				CI_LOG_E( "could not parse param type" );
				continue;
			}

			string paramType = line.substr( posTypeBegin, posTypeEnd - posTypeBegin );

			// parse uniform name.
			// TODO: allow this to be an alternate name
			// - means dropping the requirement for ':' and 'hud' being the end of the line
			size_t posUniformNameBegin = line.find_first_not_of( whitespaceChars, posTypeEnd + 1 );
			size_t posUniformNameEnd = line.find_first_of( whitespaceChars + ";", posUniformNameBegin + 1 );
			string uniformName = line.substr( posUniformNameBegin, posUniformNameEnd - posUniformNameBegin );

			// parse param label, which directly follows 'hud:" in quotes
			size_t posLabelBegin = line.find( "\"", posFoundUniformStr );
			size_t posLabelEnd = line.find( "\"", posLabelBegin + 1 );

			if( posLabelBegin == string::npos || posLabelEnd == string::npos ) {
				CI_LOG_E( "could not parse param label" );
				continue;
			}

			string paramLabel = line.substr( posLabelBegin + 1, posLabelEnd - posLabelBegin - 1 );
			LOG_SHADER_CTL( "- param: " << paramLabel << ", uniform: " << uniformName << ", type: " << paramType );

			// parse default value after type, var name and '='
			string defaultValue;
			size_t posDefaultValueBegin = line.find( "=", posTypeEnd );
			if( posDefaultValueBegin != string::npos && posDefaultValueBegin < posHudStr ) {
				posDefaultValueBegin = line.find_first_not_of( whitespaceChars, posDefaultValueBegin + 1 ); // skip whitespace
				size_t posDefaultValueEnd = line.find_first_of( ";", posDefaultValueBegin + 1 );
				defaultValue = line.substr( posDefaultValueBegin, posDefaultValueEnd - posDefaultValueBegin );
				LOG_SHADER_CTL( "\tdefault value: " << defaultValue );
			}

			// parse min and max for controls that can use them
			Hud::Options controlOptions;
			float minValue, maxValue, stepValue;

			bool hasMinValue = parseFloat( "min", line, posLabelEnd, &minValue );
			if( hasMinValue ) {
				LOG_SHADER_CTL( "\tmin value: " << minValue );
				controlOptions.min( minValue );
			}
			bool hasMaxValue = parseFloat( "max", line, posLabelEnd, &maxValue );
			if( hasMaxValue ) {
				LOG_SHADER_CTL( "\tmax value: " << maxValue );
				controlOptions.max( maxValue );
			}
			bool hasStepValue = parseFloat( "step", line, posLabelEnd, &stepValue );
			if( hasStepValue ) {
				LOG_SHADER_CTL( "\tstep value: " << stepValue );
				controlOptions.step( stepValue );
			}

			// make persistent controls based on uniform type
			// TODO: Try to make this templated. only part I don't know about yet is the methods checkBox, slider, etc.
			ShaderControlBaseRef shaderControl;
			if( paramType == "bool" ) {
				bool initialValue = defaultValue == "true" ? true : false;
				auto boolShaderControl = make_shared<ShaderControl<bool>>( initialValue );
				shaderControl = boolShaderControl;

				boolShaderControl->mControl = checkBox( boolShaderControl->getVar(), paramLabel, controlOptions );
				shaderControl->mConnValueChanged = boolShaderControl->mControl->getSignalValueChanged().connect( -1, signals::slot( boolShaderControl.get(), &ShaderControl<bool>::updateUniform ) );
			}
			else if( paramType == "float" ) {
				float initialValue = ( ! defaultValue.empty() ) ? stof( defaultValue ) : 0;
				auto floatShaderControl = make_shared<ShaderControl<float>>( initialValue );
				shaderControl = floatShaderControl;

				floatShaderControl->mControl = slider( floatShaderControl->getVar(), paramLabel, controlOptions );
				shaderControl->mConnValueChanged = floatShaderControl->mControl->getSignalValueChanged().connect( -1, signals::slot( floatShaderControl.get(), &ShaderControl<float>::updateUniform ) );
			}
			else if( paramType == "vec2" ) {
				vec2 initialValue;
				if( ! defaultValue.empty() )
					parseVec( defaultValue, &initialValue );

				auto vecControl = make_shared<ShaderControl<vec2>>( initialValue );
				shaderControl = vecControl;

				vecControl->mControl = numBox( vecControl->getVar(), paramLabel, controlOptions );
				shaderControl->mConnValueChanged = vecControl->mControl->getSignalValueChanged().connect( -1, signals::slot( vecControl.get(), &ShaderControl<vec2>::updateUniform ) );
			}
			else if( paramType == "vec3" ) {
				vec3 initialValue;
				if( ! defaultValue.empty() )
					parseVec( defaultValue, &initialValue );

				auto vecControl = make_shared<ShaderControl<vec3>>( initialValue );
				shaderControl = vecControl;

				vecControl->mControl = numBox( vecControl->getVar(), paramLabel, controlOptions );
				shaderControl->mConnValueChanged = vecControl->mControl->getSignalValueChanged().connect( -1, signals::slot( vecControl.get(), &ShaderControl<vec3>::updateUniform ) );
			}
			else if( paramType == "vec4" ) {
				vec4 initialValue;
				if( ! defaultValue.empty() )
					parseVec( defaultValue, &initialValue );

				auto vecControl = make_shared<ShaderControl<vec4>>( initialValue );
				shaderControl = vecControl;

				vecControl->mControl = numBox( vecControl->getVar(), paramLabel, controlOptions );
				shaderControl->mConnValueChanged = vecControl->mControl->getSignalValueChanged().connect( -1, signals::slot( vecControl.get(), &ShaderControl<vec4>::updateUniform ) );
			}

			shaderControl->mShader = shader;
			shaderControl->mUniformName = uniformName;
			shaderControl->mShaderLine = line;
			shaderControl->updateUniform();

			group.mShaderControls.push_back( shaderControl );
		}
	}

	LOG_SHADER_CTL( "label: " << group.mLabel );

	// If any 'hud:' shader controls were added, save them in mShaderControlGroups
	if( ! group.mShaderControls.empty() ) {
		// first search for an existing group with this label, replace it if needed
		bool alreadyExists = false;
#if 1
		// - for now, just removing old first
		mShaderControlGroups.erase( remove_if( mShaderControlGroups.begin(), mShaderControlGroups.end(),
								[&group]( const ShaderControlGroup &existingGroup ) {
									return group.mLabel == existingGroup.mLabel;
								} ),
			mShaderControlGroups.end() );

#else
		// TODO: later reconsider if this is nice to have
		// - currently the above still retains the values because new controls aren't actually created when they already exist
		// - only benefit I know of atm is that changing a default in the shader would cause it to change the UI
		//     - but this won't be hugely useful once there is a NumberBox-like control

		for( auto &existingGroup : mShaderControlGroups ) {
			if( existingGroup.mLabel == group.mLabel ) {
				LOG_SHADER_CTL( "group already exists with label: " << existingGroup.mLabel );

				alreadyExists = true;
				// remove controls from existing group with uniforms that no longer have a control
				auto &existingControls = existingGroup.mShaderControls;
				auto &currentControls = group.mShaderControls;
				for( size_t i = 0; i < existingControls.size(); i++ ) {
					auto existingControl = existingControls[i];
					auto foundIt = find_if( currentControls.begin(), currentControls.end(),
											[&existingControl]( const ShaderControlBaseRef &currentControl ) {
												return existingControl->mUniformName == currentControl->mUniformName;
											} );

					if( foundIt == currentControls.end() ) {
						LOG_SHADER_CTL( "[merge] removing old control at index: " << i << ", name: " << existingControl->mControl->getName() );
						existingControls.erase( existingControls.begin() + i );
					}
				}


				// add controls with new uniforms to existing group
				for( size_t i = 0; i < currentControls.size(); i++ ) {
					auto &currentControl = currentControls[i];
					auto foundIt = find_if( existingControls.begin(), existingControls.end(),
											[&currentControl]( const ShaderControlBaseRef &existingControl ) {
												return currentControl->mUniformName == existingControl->mUniformName;
											} );

					if( foundIt == existingControls.end() ) {
						LOG_SHADER_CTL( "[merge] adding new control at index: " << i << ", name: " << currentControl->mControl->getName() );
						existingControls.push_back( currentControl );
					}
					else {
						// TODO: check if the shader line doesn't match the old one, and update as needed
						if( (*foundIt)->mShaderLine == currentControl->mShaderLine ) {
							LOG_SHADER_CTL( "[merge] control already exists and line the same at index: " << i << ", name: " << currentControl->mControl->getName() );
						}
						else {
							LOG_SHADER_CTL( "[merge] control already exists but line has changed at index: " << i << ", name: " << currentControl->mControl->getName() );
						}
					}
				}
			}
		}
#endif

		// If completely new group, add it to the stack
		if( ! alreadyExists ) {
			LOG_SHADER_CTL( "Adding new group with label: " << group.mLabel );

			shader->setLabel( group.mLabel );
			group.mShader = shader;

			mShaderControlGroups.push_back( group );
		}
	}
}

template std::shared_ptr<ui::NumberBoxT<float>>		Hud::findOrMakeNumberBoxN( const float &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );
template std::shared_ptr<ui::NumberBoxT<ci::vec2>>	Hud::findOrMakeNumberBoxN( const ci::vec2 &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );
template std::shared_ptr<ui::NumberBoxT<ci::vec3>>	Hud::findOrMakeNumberBoxN( const ci::vec3 &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );
template std::shared_ptr<ui::NumberBoxT<ci::vec4>>	Hud::findOrMakeNumberBoxN( const ci::vec4 &initialValue, const std::string &label, const Options &options, const std::function<void( Attribs &attribs )> &updateAttribsFn );
template std::shared_ptr<ui::NumberBoxT<float>>		Hud::numBox( float *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<ci::vec2>>	Hud::numBox( ci::vec2 *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<ci::vec3>>	Hud::numBox( ci::vec3 *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<ci::vec4>>	Hud::numBox( ci::vec4 *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<float>>		Hud::numBox( float *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<ci::vec3>>	Hud::numBox( Var<ci::vec3> *x, const std::string &label, Options options );
template std::shared_ptr<ui::NumberBoxT<ci::vec4>>	Hud::numBox( Var<ci::vec4> *x, const std::string &label, Options options );

} // namespace mason
