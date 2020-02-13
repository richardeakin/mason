#pragma once

#include "vu/Suite.h"

#include "mason/Mason.h"
#include "cinder/FileWatcher.h"
#include "mason/Info.h"
#include "vu/Control.h"

class MiscTest : public vu::SuiteView {
  public:
	MiscTest();

	void layout() override;
	void update() override;
	void draw( vu::Renderer *ren )	override;

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
