#pragma once

namespace verus
{
	namespace CGI
	{
		enum class Format : UINT16
		{
			unormB4G4R4A4,
			unormB5G6R5,

			// Unsigned Formats:
			unormR8,
			unormR8G8,
			unormR8G8B8A8,
			unormB8G8R8A8,

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
			srgbBC1,
			unormBC2,
			srgbBC2,
			unormBC3,
			srgbBC3
		};
	}
}
