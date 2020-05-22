// note: changing this file will cause a full rebuild of both this project and libcinder

#if ! defined( CINDER_SHARED_BUILD )
//! This allows us to define our own assertion handlers (in psCommon.cpp)
#define CI_ENABLE_ASSERTS
#define CI_ENABLE_ASSERT_HANDLER
#define MA_ENABLE_ASSERT_OVERRIDE
#endif

//! Uncomment for better release build debugging
#define DC_DEBUG_RELEASE
