#include "MiscTest.h"

#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/CinderAssert.h"
#include "cinder/app/App.h"

#include "mason/Mason.h"
#include "jsoncpp/json.h"

using namespace ci;
using namespace std;

const fs::path JSON_FILENAME = "dictionary_test.json";

MiscTest::MiscTest()
{
	auto nbox = make_shared<ui::NumberBox>( Rectf( 200, 360, 280, 400 ) );
	nbox->setTitle( "val" );
	//nbox->setBackgroundEnabled( false );
	addSubview( nbox );

	auto nbox3 = make_shared<ui::NumberBox3>( Rectf( 200, 410, 380, 450 ) );
	ui::NumberBox3* nbox3Ptr = nbox3.get();
//	nbox3->getSignalValueChanged().connect( [nbox3Ptr] { CI_LOG_I( "nbox3 value: " << nbox3Ptr->getValue(); ); } );

	addSubview( nbox3 );

	try {
		mConnDict = FileWatcher::instance().watch( JSON_FILENAME, [this] ( const WatchEvent &event ) {
			try {
				auto dict = ma::Dictionary::convert<Json::Value>( loadFile( event.getFile() ) );
				testDict( dict );
				testConvertBack( dict );

				CI_LOG_I( "testDict completed." );
			}
			catch( exception &exc ) {
				CI_LOG_EXCEPTION( "failed to load Dictionary", exc );
			}
		} );
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load Dictionary", exc );
	}

	testPrintingDict();
	testSetDictWithOperators();
	testMergegDict();
}

void MiscTest::testDict( const ma::Dictionary &dict )
{
	CI_LOG_I( "number of elements: " << dict.getSize() );

	// test get<T> methods
	{
		CI_LOG_I( "testing get<T> methods.." );
		auto a = dict.get<int>( "a" );
		auto b = dict.get<float>( "b" );
		auto bAsDouble = dict.get<double>( "b" );
		auto c = dict.get<string>( "c" );
		auto v2 = dict.get<vec2>( "vec2" );
		auto dv2 = dict.get<dvec2>( "vec2" );

		CI_LOG_I( "a: " << a << ", b: " << b << ", bAsDouble: " << bAsDouble << ", c: " << c << ", v2: " << v2 << ", dv2: " << dv2 );
	}

	{
		CI_LOG_I( "testing Value conversion for basic types.." );
		int a = dict["a"];
		float b = dict["b"];
		double bAsDouble = dict["b"];
		string c = dict["c"];
		CI_LOG_I( "a: " << a << ", b: " << b << ", bAsDouble: " << bAsDouble << ", c: " << c );
	}

	// nested Dictionary, copied
	auto nested = dict.get<ma::Dictionary>( "nested" );
	CI_LOG_I( "nested blah string: " << nested.get<string>( "blah" ) );

	// nested Dictionary without copying
	const auto &nestedRef = dict.getStrict<ma::Dictionary>( "nested" );
	CI_LOG_I( "nestedRef days: " << nestedRef.get<int>( "days" ) );

	// nested using operator conversion syntax
	{
		string blah = dict["nested"]["blah"];
		float days = dict["nested"]["days"];
		CI_LOG_I( R"(dict["nested"]["blah"]: )" << blah << R"(, dict["nested"]["days"]: )" << days );
	}

	// vector with mixed types
	auto mixedArray = dict.get<vector<boost::any>>( "mixedArray" );
	string typesStr;
	for( size_t i = 0; i < mixedArray.size(); i++ ) {
		typesStr += "[" + to_string( i ) + "] " + System::demangleTypeName( mixedArray[i].type().name() ) + ", ";
	}
	CI_LOG_I( "num elements in mixedArray: " << mixedArray.size() << "', types: " << typesStr );

	// vector with uniform elemnt types
	auto oddNumbers = dict.get<vector<int>>( "oddNumbers" );
	string oddNumbersStr;
	for( size_t i = 0; i < oddNumbers.size(); i++ ) {
		oddNumbersStr += "[" + to_string( i ) + "] " + to_string( oddNumbers[i] ) + ", ";
	}
	CI_LOG_I( "num elements in oddNumbers: " << oddNumbers.size() << ", values: " << oddNumbersStr );
}

void MiscTest::testConvertBack( const ma::Dictionary &dict )
{
	auto json = dict.convert<Json::Value>();
	CI_LOG_I( "dict -> json:\n" << json );
}

void MiscTest::testPrintingDict()
{
	ma::Dictionary d;
	d.set( "a", 2 );
	d.set( "b", "blah" );
	d.set( "vec2", vec2( 0, 1 ) );

	CI_LOG_I( "dict:\n" << d );
}

void MiscTest::testSetDictWithOperators()
{
	ma::Dictionary d;
	d["a"] = 2;
	d["b"] = "blah";
	d["vec2"] = vec2( 0, 1 );

	CI_LOG_I( "dict:\n" << d );
}

void MiscTest::testMergegDict()
{
	ma::Dictionary a;
	a.set( "a", 2 );
	//a.set( "aa", "hey" );

	{
		ma::Dictionary sub;
		sub.set( "b", "blah" );
		sub.set( "vec2", vec2( 0, 1 ) );

		a.set( "sub", sub );

		// TODO: what to do about if a user adds a vector<int> or something like that?
		vector<boost::any> arr = { 1, 2, 3, 4 } ;
		a.set( "array", arr );
	}
	ma::Dictionary b;
	b.set( "a", 3 );
	{
		ma::Dictionary sub;
		sub.set( "b", "updated" );

		b.set( "sub", sub );

		vector<boost::any> arr = { 2, "yo"} ;
		b.set( "array", arr );
	}

	a.merge( b );

	auto json = a.convert<Json::Value>();
	CI_LOG_I( "merged:\n" << json );
}

void MiscTest::addStressTestWatches()
{
	// for profiling
	const int fileCount = 100;
	CI_LOG_I( "adding " << fileCount << " watches." );

	try {
		for( int i = 0; i < fileCount; i++ ) {
			FileWatcher::instance().watch( JSON_FILENAME, [this] ( const WatchEvent &event ) {
				// not doing anything, just watching em.
			} );
		}
	}
	catch( exception &exc ) {
		CI_LOG_EXCEPTION( "failed to load Dictionary", exc );
	}
}

void MiscTest::layout()
{
}

bool MiscTest::keyDown( ci::app::KeyEvent &event )
{
	if( event.isControlDown() )
		return false;

	bool handled = true;
	if( event.getChar() == 'u' ) {
		CI_LOG_I( "unwatching " << JSON_FILENAME );
		FileWatcher::instance().unwatch( JSON_FILENAME );
	}
	else if( event.getChar() == 'w' ) {
		addStressTestWatches();
	}
	else if( event.getChar() == 'f' ) {
		FileWatcher::instance().setWatchingEnabled( ! FileWatcher::instance().isWatchingEnabled() );
		CI_LOG_I( "watching enabled: " << FileWatcher::instance().isWatchingEnabled() );
	}
	else
		handled = false;

	return handled;
}


void MiscTest::update()
{
}

void MiscTest::draw( ui::Renderer *ren )
{
	gl::ScopedColor col( Color( 0.3f, 0.1f, 0 ) );
	gl::drawSolidRect( Rectf( 0, 0, getWidth(), getHeight() ) );
}
