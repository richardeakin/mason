#pragma once

#include "ui/Suite.h"

#include "mason/Mason.h"
#include "mason/FlyCam.h"
#include "mason/Var.h"

class HudTest : public ui::SuiteView {
  public:
	HudTest();

	void layout() override;
	bool keyDown( ci::app::KeyEvent &event ) override;
	void update() override;
	void draw( ui::Renderer *ren )	override;

  private:
	void loadGlsl();
	void testHudVars();

	ci::gl::BatchRef	mBatchRect;
	ci::signals::ScopedConnection	mConnGlsl;

	bool mTestIMMode = false;

	ma::Var<bool>		mBoolVar;
	ma::Var<float>		mFloatVar;
	ma::Var<ci::vec2>	mVec2Var;
	ma::Var<ci::vec3>	mVec3Var;
	ma::Var<ci::vec4>	mVec4Var;
};
