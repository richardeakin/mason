#pragma once

#include "ui/Suite.h"

#include "mason/Mason.h"
#include "cinder/FileWatcher.h"
#include "mason/Info.h"
#include "ui/Control.h"

class MiscTest : public ui::SuiteView {
  public:
	MiscTest();

	void layout() override;
	void update() override;
	void draw( ui::Renderer *ren )	override;

	bool keyDown( ci::app::KeyEvent &event ) override;

  private:
	void testDict( const ma::Info &dict );
	void testConvertBack( const ma::Info &dict );
	void testPrintingDict();
	void testSetDictWithOperators();
	void testMergegDict();

	void addStressTestWatches();

	ci::signals::ScopedConnection		mConnDict;
};
