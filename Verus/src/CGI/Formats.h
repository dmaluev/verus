// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		// See: https://vulkan.gpuinfo.org/listformats.php?platform=android
		enum class Format : int
		{
			// Special Formats:
			unormR10G10B10A2,
			sintR16,
			floatR11G11B10,

			// Unsigned Formats:
			unormR8,
			unormR8G8,
			unormR8G8B8A8,
			unormB8G8R8A8,
			srgbR8G8B8A8,
			srgbB8G8R8A8,

			// Floating-Point Formats:
			floatR16,
			floatR16G16,
			floatR16G16B16A16,

			// IEEE Formats:
			floatR32,
			floatR32G32,
			floatR32G32B32A32,

			// Depth-Stencil:
			unormD16,
			unormD24uintS8,
			floatD32,

			// Compressed Texture Formats:
			unormBC1, // Deprecated RGBA.
			unormBC2, // Deprecated RGBA.
			unormBC3, // Deprecated RGBA.
			unormBC4, // R.
			unormBC5, // RG.
			unormBC7, // RGBA.
			snormBC4, // R.
			snormBC5, // RG.
			srgbBC1, // Deprecated RGBA.
			srgbBC2, // Deprecated RGBA.
			srgbBC3, // Deprecated RGBA.
			srgbBC7 // RGBA.
		};
	}
}
