#include "mason/scene/Component.h"
#include "mason/imx/ImGuiStuff.h"

#include "cinder/Log.h"
#include "cinder/CinderAssert.h"
#include "cinder/System.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

#define LOG_COMPONENT( stream )	CI_LOG_I( stream )
//#define LOG_COMPONENT (void(0))

namespace mason::scene {

Component::Component()
{
	LOG_COMPONENT( "this type: " << System::demangleTypeName( typeid( *this ).name() ) );
}

void Component::loadComponent( const ma::Info &info )
{
	LOG_COMPONENT( "label: " << mLabel );

	mUpdateEnabled = info.get( "update", mUpdateEnabled );
	mDrawEnabled = info.get( "draw", mDrawEnabled );
	mUIEnabled = info.get( "ui", mUIEnabled );

	load( info );
}

void Component::saveComponent( ma::Info &info ) const
{
	ma::Info component;
	component["update"] = mUpdateEnabled;
	component["draw"] = mDrawEnabled;
	component["ui"] = mUIEnabled;

	save( component );

	info[mLabel] = component;
}

void Component::layoutComponent( const Rectf &bounds )
{
	vec2 pos = bounds.getUpperLeft();
	vec2 size = bounds.getSize();

	if( mPos != pos || mSize != size ) {
		mPos = pos;
		mSize = size;
		LOG_COMPONENT( "pos: " << mPos << ", size: " << mSize );

		if( mSize != vec2( 0 ) ) {
			layout();
		}
	}
}

void Component::updateComponent( double currentTime, double deltaTime )
{
	if( mUpdateEnabled ) {
		update( currentTime, deltaTime );
	}
}

void Component::drawComponent()
{
	if( mDrawEnabled ) {
		draw();
	}
}

void Component::updateComponentUI()
{
	//if( ! mUIEnabled )
	//	return;
	
	if( im::Begin( mLabel.c_str(), &mUIEnabled ) ) {
		updateUI();
	}
	im::End();
}

void Component::enabledUI()
{
	im::ScopedId idScope( mLabel.c_str() );

	im::Text( ( mLabel + ":" ).c_str() );	im::SameLine();
	im::SetCursorPosX( 160 );
	im::Checkbox( "update", &mUpdateEnabled );		im::SameLine();
	im::Checkbox( "draw", &mDrawEnabled );			im::SameLine();
	im::Checkbox( "ui", &mUIEnabled );
}

} // namespace mason::scene
