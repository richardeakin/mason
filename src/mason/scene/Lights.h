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

#pragma once

#include "cinder/Vector.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Ssbo.h"

#include <string>

namespace mason::scene {

using glm::vec2;
using glm::vec3;
using glm::vec4;

struct Light {
	Light( const std::string &label = "" );
	~Light();

	void updateUI( int flags = 0 );
	void drawDebug() const;

	enum class Type {
		Directional,
		Spot,
		Point,
		NumLightTypes
	};

	std::string	    mLabel;
	Type			mType   = Type::Directional;
	ci::vec3		mPos	= ci::vec3( 0 );		//! all except Directional
	ci::vec3		mDir	= ci::vec3( 0, -1, 0 ); //! all except Point
	ci::Color		mColor  = ci::Color::white(); //! all
	float           mIntensity = 1;
	float			mRadius = 0;					//! Spot and Point
	float			mCutoff = 0;					//! Point
	ci::vec2		mAngles = ci::vec2( 0 );		//! Spot
	ci::vec4		mDebug;
};

class LightsController {
public:

	// TODO: add for conveneince
	//Light& addDirectional();

	Light& addLight( const std::string &label = "" );

	void clear();

	size_t	getNumLights() const	{ return mLights.size(); }

	void updateShaderBuffer( const ci::gl::GlslProgRef &glsl );

	void updateUI();
	void drawDebug();

	// TODO: not sure if this is a good idea, might need to keep this scoped
	void bindBuffer();

private:
	//! Matches mason/lighting/lighting.glsl, loaded as a std430 uniform buffer
	struct LightData {
		vec3	pos;			// used for LIGHT_TYPE_SPOT and LIGHT_TYPE_POINT
		int		type;			//! Maps to Light::Type enum

		vec3	dir;			// used for LIGHT_TYPE_DIRECTIONAL and LIGHT_TYPE_SPOT
		float	cuttoff;		// TODO: call 'falloff'? attenuation cutoff, used for LIGHT_TYPE_SPOT and LIGHT_TYPE_POINT

		vec3	color;			// the color of the light
		float   intensity;

		vec3	ambientColor;	// ambient color does not vary with direction. This is usually black.
		float	radius;			// attenuation radius, used for LIGHT_TYPE_SPOT and LIGHT_TYPE_POINT

		vec2	angles;
		int     flags;
		int     padding;
	};

	std::vector<Light>		mLights;
	std::vector<LightData>	mLightsData;
	ci::gl::SsboRef			mBuffer;

	friend Light;
};

} // namespace mason::scene
