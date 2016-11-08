#include "MotionTest.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"

using namespace ci;
using namespace std;

MotionTest::MotionTest()
{
}

void MotionTest::layout()
{
}

void MotionTest::update()
{
}

void MotionTest::draw( ui::Renderer *ren )
{
	gl::ScopedColor col( Color( 0.3f, 0.1f, 0 ) );
	gl::drawSolidRect( Rectf( 0, 0, getWidth(), getHeight() ) );
}
