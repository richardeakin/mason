#pragma once

#include "ui/Suite.h"

#include "mason/Mason.h"
#include "mason/FileWatcher.h"
#include "mason/Dictionary.h"
#include "mason/ui/Controls.h"
#include "mason/Common.h"

class MiscTest : public ui::SuiteView {
  public:
	MiscTest();

	void layout() override;
	void update() override;
	void draw( ui::Renderer *ren )	override;

  private:
	void testDict( const ma::Dictionary &dict );
	void testConnectionList();

	mason::ScopedWatch mWatchDict;
	mason::ConnectionList mConnectionList;
};
