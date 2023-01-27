/*
Copyright (c) 2020, Richard Eakin project - All rights reserved.

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

#include "mason/scene/Lights.h"

#include "mason/imx/ImGuiStuff.h"
#include "imGuIZMOquat.h"

#include "cinder/Log.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace std;
namespace im = ImGui;

namespace mason::scene {

// ----------------------------------------------------------------------------------------------------
// Light
// ----------------------------------------------------------------------------------------------------

Light::Light( const std::string &label )
	: mLabel( label )
{
	//CI_LOG_I( "label: " << mLabel );
}

Light::~Light()
{
}

void Light::updateUI( int flags )
{
	im::ScopedId idScope( this );

	// TODO: add a better label
	if( im::TreeNodeEx( mLabel.c_str(), flags ) ) {
		im::ColorEdit3( "color", &mColor.r, ImGuiColorEditFlags_Float );
		im::DragFloat( "intensity", &mIntensity, 0.01f, 0.0f, 10e6 );
		if( mType == Type::Directional ) {
			if( im::DragFloat3( "dir", &mDir.x, 0.01f ) ) {
				if( glm::any( glm::isnan( mDir ) ) ) {
					mDir = vec3( 0, -1, 0 );
				}
				mDir = normalize( mDir );
			}
			im::PushStyleColor( ImGuiCol_FrameBg, vec4( 0 ) );
			im::gizmo3D( "##dir arrow", mDir, IMGUIZMO_DEF_SIZE, imguiGizmo::modeDirection );
			im::PopStyleColor();
		}
		else if( mType == Type::Point ) {
			im::DragFloat3( "pos", &mPos.x, 0.01f );
			im::DragFloat( "radius", &mRadius, 0.01f );
			im::DragFloat( "cuttoff", &mCutoff, 0.01f );
		}
		else if( mType == Type::Spot ) {
			im::DragFloat3( "pos", &mPos.x, 0.01f );
			im::DragFloat( "radius", &mRadius, 0.01f );
			im::DragFloat2( "angles", &mAngles.x, 0.02f, 0, M_PI );

			if( im::DragFloat3( "dir", &mDir.x, 0.01f ) ) {
				if( glm::any( glm::isnan( mDir ) ) ) {
					mDir = vec3( 0, -1, 0 );
				}
				mDir = normalize( mDir );
			}
			im::PushStyleColor( ImGuiCol_FrameBg, vec4( 0 ) );
			im::gizmo3D( "##dir arrow", mDir, IMGUIZMO_DEF_SIZE, imguiGizmo::modeDirection );
			im::PopStyleColor();
		}

		im::DragFloat4( "debug", &mDebug.x, 0.02f );

		im::TreePop();
	}
}

void Light::drawDebug() const
{
	gl::ScopedColor colorScope;
	gl::ScopedModelMatrix modelScope;

	if( mType == Type::Directional ) {
		static gl::BatchRef batch;
		if( !batch ) {
			auto wireArrow	 = geom::WireCylinder().height( 8 ) & ( geom::WireCone().height( 3 ).base( 2.5f ) >> geom::Translate( vec3( 0, 8, 0 ) ) );
			auto colorShader = gl::getStockShader( gl::ShaderDef().color() );

			batch = gl::Batch::create( wireArrow, colorShader );
		}

		gl::translate( vec3( -100, 130, 50 ) ); // TODO: where to place? Probably have to take an additional param that says where, since pos isn't used in this type
			// create a difference quat between up (arrow Batch's default) and where we want to be pointing
		auto rot = glm::rotation( vec3( 0, 1, 0 ), normalize( mDir ) );
		gl::rotate( rot );

		gl::color( Color( 1, 1, 0.2f ) );
		batch->draw();
	}
	else if( mType == Type::Point ) {
		static gl::BatchRef batch;
		if( !batch ) {
			auto wireSphere	 = geom::WireSphere().radius( 2 );
			auto colorShader = gl::getStockShader( gl::ShaderDef().color() );

			batch = gl::Batch::create( wireSphere, colorShader );
		}

		gl::translate( mPos );
		gl::color( Color( 1, 0.5f, 0.1f ) ); // matches color in sceneObject.frag, light1.color
		batch->draw();
	}
	else if( mType == Type::Spot ) {
		static gl::BatchRef batch;
		if( !batch ) {
			auto wireCone	   = geom::WireCone().height( 6 ).base( 3 ) >> geom::Translate( vec3( 0, -6, 0 ) ) >> geom::Rotate( M_PI, vec3( 1, 0, 0 ) );
			auto wireSpotlight = wireCone & geom::WireSphere();
			auto colorShader   = gl::getStockShader( gl::ShaderDef().color() );

			batch = gl::Batch::create( wireSpotlight, colorShader );
		}

		gl::translate( mPos );
		auto rot = glm::rotation( vec3( 0, 1, 0 ), normalize( mDir ) ); // create a difference quat between up (Batch's default) and where we want to be pointing
		gl::rotate( rot );

		gl::color( Color( 0.4f, 1, 0.1f ) ); // matches color in sceneObject.frag, light2.color
		batch->draw();
	}
}

// ----------------------------------------------------------------------------------------------------
// LightsController
// ----------------------------------------------------------------------------------------------------

namespace {

const GLuint BUFFER_BINDING_INDEX = 0; // TODO: this should probably be settable

} // anonymous namespace

Light& LightsController::addLight( const std::string &label )
{
	auto &result = mLights.emplace_back( label );
	return result;
}

void LightsController::clear()
{
	mLights.clear();
	mLightsData.clear();

	mLights.reserve( 3 ); // TODO: remove, just testing the destructors
}

// TODO: minimally update lights data / uniform buffer, only when a light has been added / removed / modified
void LightsController::updateShaderBuffer( const ci::gl::GlslProgRef &glsl )
{
	if( mLightsData.size() != mLights.size() ) {
		// init gpu buffer
		mBuffer = gl::Ssbo::create( mLightsData.size() * sizeof( LightData ), mLightsData.data(), GL_DYNAMIC_DRAW );
		glBindBufferBase( mBuffer->getTarget(), BUFFER_BINDING_INDEX, mBuffer->getId() );
	}

	mLightsData.resize( mLights.size() );
	if( mLights.empty() ) {
		return;
	}

	// update all mLightsData, then push to gpu
	for( size_t i = 0; i < mLights.size(); i++ ) {
		const auto &light = mLights[i];
		auto &data = mLightsData[i];

		//data.debug = light.mDebug;
		data.type = (int)light.mType;
		data.pos = light.mPos;
		data.dir = light.mDir;
		data.color = vec3( light.mColor );
		data.intensity = light.mIntensity;
		//data.ambientColor = ???; // TODO: review where this was used
		data.radius = light.mRadius;
		data.cuttoff = light.mCutoff;
		data.angles = light.mAngles;
	}

	bindBuffer();

	// update the gpu data
	//mBuffer->bufferSubData( 0, mLightsData.size() * sizeof( LightData ), mLightsData.data() ); // FIXME: figure out why this one doesn't work
	mBuffer->bufferData( mLightsData.size() * sizeof( LightData ), mLightsData.data(), GL_DYNAMIC_DRAW );
	
	// always setting this because of shader hot loading
	glsl->uniform( "uSceneLightsCount", (int)mLights.size() );
}

void LightsController::bindBuffer()
{
	//gl::context()->bindBufferBase( mBuff)

	glBindBufferBase( mBuffer->getTarget(), BUFFER_BINDING_INDEX, mBuffer->getId() );
}

void LightsController::updateUI()
{
	for( auto &light : mLights ) {
		light.updateUI( 0 );
	}
}

void LightsController::drawDebug()
{
	gl::ScopedDepth depthScope( true );

	for( const auto& light : mLights ) {
		light.drawDebug();
	}
}

} // namespace mason::scene
