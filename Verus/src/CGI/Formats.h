#pragma once

namespace verus
{
	namespace CGI
	{
		enum class Format : int
		{
			// Special Formats:
			unormB4G4R4A4,
			unormB5G6R5,
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
			unormBC1,
			unormBC2,
			unormBC3,
			unormBC7,
			srgbBC1,
			srgbBC2,
			srgbBC3,
			srgbBC7
		};
	}
}
