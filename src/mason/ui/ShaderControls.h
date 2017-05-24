/*
Copyright (c) 2016, Richard Eakin - All rights reserved.

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

#include "ui/Control.h"
#include "mason/Mason.h"


namespace mason {

//! Maps a ui Control to a data type \t T.
template<typename T> struct ControlType {};

template<> struct ControlType<bool>			{ typedef ui::CheckBox TYPE; };
template<> struct ControlType<float>		{ typedef ui::HSlider TYPE; };
template<> struct ControlType<ci::vec2>		{ typedef ui::NumberBox2 TYPE; };
template<> struct ControlType<ci::vec3>		{ typedef ui::NumberBox3 TYPE; };
template<> struct ControlType<ci::vec4>		{ typedef ui::NumberBox4 TYPE; };

struct ShaderControlGroup;

struct ShaderControlBase {
	virtual ~ShaderControlBase() = default;

	virtual void	updateUniform() = 0;

	ci::gl::GlslProgRef	getShader() const	{ return mShader.lock(); }

	std::weak_ptr<ci::gl::GlslProg>		mShader;
	std::string							mUniformName;
	std::string							mShaderLine; // this is used to determine if the uniform control params have changed between reloads
	ci::signals::ScopedConnection		mConnValueChanged;
};

template<typename T>
struct ShaderControl : public ShaderControlBase {
	ShaderControl( const T &initialValue )
		: mValue( initialValue )
	{}

	void	updateUniform() override;

	Var<T>*		getVar()			{ return &mValue; }
	const T&	getValue() const	{ return mValue.value(); }

	typedef typename ControlType<T>::TYPE	ControlT;

	std::shared_ptr<ControlT>	mControl;

private:
	Var<T>	mValue;
};

typedef std::shared_ptr<ShaderControlBase>	ShaderControlBaseRef;

struct ShaderControlGroup {
	ci::gl::GlslProgRef	getShader() const	{ return mShader.lock(); }

	std::string							mLabel;
	std::vector<ShaderControlBaseRef>	mShaderControls;

	std::weak_ptr<ci::gl::GlslProg>		mShader; // used to tell when we should remove the control group
};

template <typename T>
void ShaderControl<T>::updateUniform()
{
	mValue = mControl->getValue();
	getShader()->uniform( mUniformName, mValue() );
}

}