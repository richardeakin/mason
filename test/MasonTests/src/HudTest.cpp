
#include "HudTest.h"

#include "mason/Assets.h"
#include "mason/Hud.h"

#include "cinder/gl/gl.h"
#include "cinder/app/App.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"

using namespace ci;
using namespace std;

HudTest::HudTest()
{
	loadGlsl();
	testHudVars();
}

void HudTest::loadGlsl()
{
	mConnGlsl = ma::assets()->getShader( "hud/hudTest.vert", "hud/hudTest.frag", [this]( gl::GlslProgRef glsl ) {

		glsl->uniform( "uResolution", getSize() );

		if( mBatchRect )
			mBatchRect->replaceGlslProg( glsl );
		else {
			mBatchRect = gl::Batch::create( geom::Rect( Rectf( 0, 0, 1, 1 ) ), glsl );
		}
	} );
}

void HudTest::testHudVars()
{
	mBoolVar = true;
	auto checkBox = ma::hud()->checkBox( &mBoolVar, "Var<bool>" );
	checkBox->getSignalValueChanged().connect( -1, [this] {
		// FIXME: |warning| void ui::Graph::propagateTouchesEnded(app::TouchEvent &)[374] stray touch attempted to be removed
		CI_LOG_I( "mBoolVar: " << mBoolVar );
	} );

	mFloatVar = 3.0f;
	auto slider = ma::hud()->slider( &mFloatVar, "Var<float>" );
	slider->getSignalValueChanged().connect( -1, [this] {
		CI_LOG_I( "mFloatVar: " << mFloatVar );
	} );

	mVec2Var = vec2( 10, -10 );
	auto nbox2 = ma::hud()->numBox( &mVec2Var, "Var<vec2>" );
	nbox2->getSignalValueChanged().connect( -1, [this] {
		CI_LOG_I( "mVec2Var: " << mVec2Var.value() );
	} );

	mVec3Var = vec3( 100, -100, 0 );
	auto nbox3 = ma::hud()->numBox( &mVec3Var, "Var<vec3>" );
	nbox3->getSignalValueChanged().connect( -1, [this] {
		CI_LOG_I( "mVec3Var: " << mVec3Var.value() );
	} );

	mVec4Var = vec4( 1 );
	auto nbox4 = ma::hud()->numBox( &mVec4Var, "Var<vec4>" );
	nbox4->getSignalValueChanged().connect( -1, [this] {
		CI_LOG_I( "mVec4Var: " << mVec4Var.value() );
	} );
}

bool HudTest::keyDown( app::KeyEvent &event )
{
	bool handled = true;
	if( event.getChar() == 'i' ) {
		mTestIMMode = ! mTestIMMode;
	}
	else
		handled = false;

	return handled;
}

void HudTest::layout()
{
	if( mBatchRect ) {
		mBatchRect->getGlslProg()->uniform( "uResolution", getSize() );
	}
}

void HudTest::update()
{
	// test hud's immediate mode capabilities
	if( mTestIMMode ) {
		bool checkBoxA = 0, checkBoxB = 1;
		float sliderA = 1.5f;
		vec2 nbox2 = vec2( 5 );
		vec3 nbox3 = vec3( 1 );
		vec4 nbox4 = vec4( 2 );

		ma::hud()->checkBox( &checkBoxA, "IM checkbox A" );
		ma::hud()->checkBox( &checkBoxB, "IM checkbox B" );
		ma::hud()->slider( &sliderA, "IM slider A", ma::Hud::Options().min( -10 ).max( 10 ) );
		ma::hud()->numBox( &nbox2, "IM NumberBox2" );
		ma::hud()->numBox( &nbox3, "IM NumberBox3" );
		ma::hud()->numBox( &nbox4, "IM NumberBox4" );

		if( app::getElapsedFrames() % 30 == 0 )
			CI_LOG_I( "checkbox A: " << checkBoxA << ", checkBoxB: " << checkBoxB << ", sliderA: " << sliderA
				<< ", nbox2: " << nbox2 << ", nbox3: " << nbox3 << ", nbox4: " << nbox4 );
	}
}

void HudTest::draw( ui::Renderer *ren )
{
	ren->setColor( Color( 0.2f, 0.1f, 0.1f ) );
	ren->drawSolidRect( getBoundsLocal() );

	if( ! mBatchRect )
		return;

	gl::ScopedBlend blendScope( false );

	gl::ScopedMatrices matScope;
	gl::scale( getWidth(), getHeight(), 1.0f );
	mBatchRect->draw();
}
