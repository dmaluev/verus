#pragma once

#define VERUS_CRNL       "\r\n"
#define VERUS_WHITESPACE " \t\r\n"

#define VERUS_ARRAY_LENGTH(x)   (sizeof(x)/sizeof(x[0]))
#define VERUS_LITERAL_LENGTH(x) (sizeof(x)-1)
#define VERUS_ZERO_MEM(x)       memset(&x, 0, sizeof(x))

#define VERUS_BUFFER_OFFSET(x) ((char*)nullptr+(x))

#define VERUS_BITMASK_SET(flags, mask)   ((flags) |= (mask))
#define VERUS_BITMASK_UNSET(flags, mask) ((flags) &= ~(mask))

#define VERUS_MAIN_DEFAULT_ARGS int argc, char* argv[]

#define VERUS_P(x)            private: x; public:
#define VERUS_PD(x)           protected: x; private:
#define VERUS_FRIEND(c, type) friend c type; type

#define VERUS_SMART_DELETE(p)       {if(p) {delete p;    p = nullptr;}}
#define VERUS_SMART_DELETE_ARRAY(p) {if(p) {delete [] p; p = nullptr;}}

#define _C(x) ((x).c_str())

#define VERUS_P_FOR(i, to)                      Parallel::For(0, to, [&](int i)
#define VERUS_U_FOR(i, to)                      for(UINT32 i = 0; i < to; ++i)
#define VERUS_FOR(i, to)                        for(int i = 0; i < to; ++i)
#define VERUS_FOREACH(T, v, it)                 for(T::iterator               it=(v).begin(),  itEnd=(v).end();  it!=itEnd; ++it)
#define VERUS_FOREACH_CONST(T, v, it)           for(T::const_iterator         it=(v).begin(),  itEnd=(v).end();  it!=itEnd; ++it)
#define VERUS_FOREACH_REVERSE(T, v, it)         for(T::reverse_iterator       it=(v).rbegin(), itEnd=(v).rend(); it!=itEnd; ++it)
#define VERUS_FOREACH_REVERSE_CONST(T, v, it)   for(T::const_reverse_iterator it=(v).rbegin(), itEnd=(v).rend(); it!=itEnd; ++it)
#define VERUS_FOREACH_X(T, v, it)               for(T::iterator               it=(v).begin(),  itEnd=(v).end();  it!=itEnd;)
#define VERUS_FOREACH_X_CONST(T, v, it)         for(T::const_iterator         it=(v).begin(),  itEnd=(v).end();  it!=itEnd;)
#define VERUS_FOREACH_X_REVERSE(T, v, it)       for(T::reverse_iterator       it=(v).rbegin(), itEnd=(v).rend(); it!=itEnd;)
#define VERUS_FOREACH_X_REVERSE_CONST(T, v, it) for(T::const_reverse_iterator it=(v).rbegin(), itEnd=(v).rend(); it!=itEnd;)
// For loops with erase() use this:
#define VERUS_WHILE(T, v, it)                   T::iterator it=(v).begin(); while(it!=(v).end())

#define VERUS_IF_FOUND_IN(T, map, x, it)\
	auto it = map.find(x);\
	if(it != map.end())

#define VERUS_TYPEDEFS(x)\
	typedef x& R##x;\
	typedef const x& Rc##x;\
	typedef x* P##x;\
	typedef const x* Pc##x

// Circular buffer's size must be power of two:
#define VERUS_CIRCULAR_ADD(x, mx)\
	x++;\
	x &= mx-1
#define VERUS_CIRCULAR_IS_FULL(r, w, mx)\
	(((w+1)&(mx-1)) == r)

// Colors:
#define VERUS_COLOR_RGBA(r,g,b,a) ((UINT32)((((a)&0xFF)<<24)|(((b)&0xFF)<<16)|(((g)&0xFF)<<8)|((r)&0xFF)))
#define VERUS_COLOR_TO_D3D(color) (((color)&0xFF00FF00)|(((color)>>16)&0xFF)|(((color)&0xFF)<<16))
#define VERUS_COLOR_WHITE         VERUS_COLOR_RGBA(255, 255, 255, 255)
#define VERUS_COLOR_BLACK         VERUS_COLOR_RGBA(0, 0, 0, 255)

#ifdef _WIN32
#	define VERUS_SDL_CENTERED _putenv("SDL_VIDEO_WINDOW_POS"); _putenv("SDL_VIDEO_CENTERED=1")
#else
#	define VERUS_SDL_CENTERED putenv("SDL_VIDEO_WINDOW_POS"); putenv("SDL_VIDEO_CENTERED=1")
#endif

#ifdef _WIN32
#	define VERUS_DLL_EXPORT __declspec(dllexport)
#else
#	define VERUS_DLL_EXPORT __attribute__ ((visibility("default")))
#endif

#define VERUS_HR(hr) std::hex << std::uppercase << "0x" << hr
