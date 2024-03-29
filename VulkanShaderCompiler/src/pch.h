// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#ifdef _WIN32
#	include <Windows.h>
#endif

#include <sstream>

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#pragma comment(lib, "shaderc_combined.lib")

TBuiltInResource InitResources();

#ifdef _WIN32
#	define VERUS_DLL_EXPORT __declspec(dllexport)
#else
#	define VERUS_DLL_EXPORT __attribute__ ((visibility("default")))
#endif
