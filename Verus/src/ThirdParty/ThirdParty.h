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

// fast_atof (http://www.leapsecond.com/tools/fast_atof.c)
extern "C"
{
	double fast_atof(const char* p);
}

// GLM (https://github.com/g-truc/glm):
#define GLM_FORCE_CTOR_INIT
#include "glm-0.9.9.6/glm/glm/glm.hpp"
#include "glm-0.9.9.6/glm/glm/gtc/packing.hpp"
#include "glm-0.9.9.6/glm/glm/gtc/quaternion.hpp"
#include "glm-0.9.9.6/glm/glm/gtc/type_ptr.hpp"
#include "glm-0.9.9.6/glm/glm/gtx/euler_angles.hpp"
#include "glm-0.9.9.6/glm/glm/gtx/normal.hpp"
#include "glm-0.9.9.6/glm/glm/gtx/norm.hpp"
#include "glm-0.9.9.6/glm/glm/gtx/vector_angle.hpp"

// Dear ImGui (https://github.com/ocornut/imgui):
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

// JSON for Modern C++ (https://github.com/nlohmann/json):
#include "json.hpp"

// MD5:
#include "md5.h"

// Native File Dialog (https://github.com/mlabbe/nativefiledialog):
#include "nativefiledialog/nfd.h"

// pugixml (https://pugixml.org/):
#include "pugixml-1.10/pugixml.hpp"

// Sony Vector Math (https://github.com/glampert/vectormath):
#include "vectormath/sse/vectormath.hpp"

// UTF-8 (https://sourceforge.net/projects/utfcpp/):
#include "utf8.h"

// zlib (https://zlib.net/):
#include "zlib-1.2.11/zlib.h"
