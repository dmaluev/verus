// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define SDL_VIDEO_DRIVER_WINRT 1
#define VERUS_INCLUDE_D3D12
#define VERUS_INCLUDE_PIX
#define XR_USE_GRAPHICS_API_D3D12
#include <verus.h>

#include "ThirdParty/imgui/imgui_impl_dx12.h"
#include "CGI/CGI.h"
