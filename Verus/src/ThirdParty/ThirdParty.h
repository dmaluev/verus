#pragma once

// Last updated on 13-Jun-2020

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
#include "glm/glm/glm.hpp"
#include "glm/glm/gtc/packing.hpp"
#include "glm/glm/gtc/quaternion.hpp"
#include "glm/glm/gtc/random.hpp"
#include "glm/glm/gtc/type_ptr.hpp"
#include "glm/glm/gtx/color_space.hpp"
#include "glm/glm/gtx/easing.hpp"
#include "glm/glm/gtx/euler_angles.hpp"
#include "glm/glm/gtx/norm.hpp"
#include "glm/glm/gtx/normal.hpp"
#include "glm/glm/gtx/vector_angle.hpp"

// Dear ImGui (https://github.com/ocornut/imgui):
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
static inline ImVec2 operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
static inline ImVec2 operator/(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
static inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
static inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
static inline ImVec2& operator*=(ImVec2& lhs, const float rhs) { lhs.x *= rhs; lhs.y *= rhs; return lhs; }
static inline ImVec2& operator/=(ImVec2& lhs, const float rhs) { lhs.x /= rhs; lhs.y /= rhs; return lhs; }
static inline ImVec4 operator+(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }
static inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }
static inline ImVec4 operator*(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w); }

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
