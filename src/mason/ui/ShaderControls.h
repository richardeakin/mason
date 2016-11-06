#include "ui/Button.h"
#include "ui/Slider.h"
#include "mason/ui/Controls.h"


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