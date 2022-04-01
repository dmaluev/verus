// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

// Remember kids, unsigned integers are bad, mkay?

#define FALSE          0
#define TRUE           1
typedef int            BOOL;
typedef unsigned char  BYTE;
typedef short          INT16;
typedef unsigned short UINT16;
typedef int            INT32;
typedef unsigned int   UINT32;
#ifdef _WIN32
typedef __int64          INT64;
typedef unsigned __int64 UINT64;
#else
typedef long long          INT64;
typedef unsigned long long UINT64;
#endif

#ifndef _WIN32
#	define _stricmp strcasecmp
#	define _strnicmp strncasecmp
#endif

namespace verus
{
	typedef char* SZ;
	typedef const char* CSZ;
	typedef wchar_t* WSZ;
	typedef const wchar_t* CWSZ;

	typedef std::string       String;
	typedef std::wstring      WideString;
	typedef std::stringstream StringStream;

	typedef glm::quat   Quaternion;
	typedef glm::ivec2  int2;
	typedef glm::ivec3  int3;
	typedef glm::ivec4  int4;
	typedef glm::vec2   float2;
	typedef glm::vec3   float3;
	typedef glm::vec4   float4;
	typedef glm::mat4   matrix;
	typedef glm::mat3x4 mataff;
	typedef UINT16      half;

	VERUS_TYPEDEFS(String);
	VERUS_TYPEDEFS(WideString);
	VERUS_TYPEDEFS(Quaternion);
}
