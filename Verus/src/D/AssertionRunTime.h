#pragma once

#ifdef _DEBUG
#	define VERUS_RT_ASSERT(x) assert(x)
#	define VERUS_RT_FAIL(x)   assert(!x)
#else
#	ifdef VERUS_RELEASE_DEBUG
#		define VERUS_RT_ASSERT(x) SDL_assert_release(x)
#		define VERUS_RT_FAIL(x)   SDL_assert_release(!x)
#	else
#		define VERUS_RT_ASSERT(x)
#		define VERUS_RT_FAIL(x)
#	endif
#endif
