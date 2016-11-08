#pragma once

#include "ui/Suite.h"

class EmptyTest : public ui::SuiteView {
  public:
	EmptyTest();

	void layout() override;
	void update() override;
	void draw( ui::Renderer *ren )	override;

  private:

};
