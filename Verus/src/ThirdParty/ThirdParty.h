#pragma once

// Base64 (https://sourceforge.net/projects/libb64/):
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

// GLM (https://github.com/g-truc/glm):
#define GLM_ENABLE_EXPERIMENTAL
#include "glm-0.9.9.5/glm/glm/glm.hpp"
#include "glm-0.9.9.5/glm/glm/gtc/quaternion.hpp"
#include "glm-0.9.9.5/glm/glm/gtc/type_ptr.hpp"
#include "glm-0.9.9.5/glm/glm/gtx/euler_angles.hpp"
#include "glm-0.9.9.5/glm/glm/gtx/normal.hpp"
#include "glm-0.9.9.5/glm/glm/gtx/norm.hpp"

// JSON for Modern C++:
#include "json.hpp"

// MD5:
#include "md5.h"

// Sony Vector Math (https://github.com/glampert/vectormath):
#include "vectormath/sse/vectormath.hpp"

// TinyXML 2 (https://github.com/leethomason/tinyxml2):
#include "tinyxml2-7.0.1/tinyxml2.h"

// UTF-8 (https://sourceforge.net/projects/utfcpp/):
#include "utf8.h"

// zlib (https://zlib.net/):
#include "zlib-1.2.11/zlib.h"
