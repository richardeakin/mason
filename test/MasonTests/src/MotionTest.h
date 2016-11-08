#pragma once

#include "ui/Suite.h"

class MotionTest : public ui::SuiteView {
  public:
	MotionTest();

	void layout() override;
	void update() override;
	void draw( ui::Renderer *ren ) override;

  private:

};
