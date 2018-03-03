#pragma once

// Base64:
// https://sourceforge.net/projects/libb64/
#ifdef _WIN32
extern "C"
{
#	include "libb64/cencode.h"
#	include "libb64/cdecode.h"
}
#else
#	include "libb64/cencode.h"
#	include "libb64/cdecode.h"
#endif

// MD5:
#include "md5.h"

// UTF-8:
// https://sourceforge.net/projects/utfcpp/
#include "utf8.h"
