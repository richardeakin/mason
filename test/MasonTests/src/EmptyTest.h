#pragma once

#include "vu/Suite.h"

class EmptyTest : public vu::SuiteView {
  public:
	EmptyTest();

	void layout() override;
	void update() override;
	void draw( vu::Renderer *ren )	override;

  private:

};
