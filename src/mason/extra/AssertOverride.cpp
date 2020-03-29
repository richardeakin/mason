
#if defined( MA_ENABLE_ASSERT_OVERRIDE )

#include "cinder/Log.h"

// Our own handlers for assertions which print fatal messages instead.
// CI_LOG_F() will cause the breakpoint to be hit when in non-production mode.
namespace cinder {
//! Called when CI_ASSERT() fails
void assertionFailed( char const *expr, char const *function, char const *file, long line )
{
	CI_LOG_F( "*** [nw] Assertion Failed *** | expression: ( " << expr << " ), location: " << file << "[" << line << "], " << function );
}

//! Called when CI_ASSERT_MSG() fails
void assertionFailedMessage( char const *expr, char const *msg, char const *function, char const *file, long line )
{
	CI_LOG_F( "*** [nw] Assertion Failed *** | expression: ( " << expr << " ), location: " << file << "[" << line << "], " << function << "\n\tmessage: " );
}

} // namespace cinder

#endif // defined( CI_ENABLE_ASSERT_HANDLER )
