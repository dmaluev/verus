// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

Utils::Utils(PBaseAllocator pAlloc)
{
	SetAllocator(pAlloc);
	srand(static_cast<unsigned int>(time(nullptr)));
}

Utils::~Utils()
{
}

void Utils::MakeEx(PBaseAllocator pAlloc)
{
	const float mx = FLT_MAX;
	const UINT32 imx = *(UINT32*)&mx;
	VERUS_RT_ASSERT(imx == 0x7F7FFFFF);

	Utils* p = static_cast<Utils*>(pAlloc->malloc(sizeof(Utils)));
	p = new(p)Utils(pAlloc);
}

void Utils::FreeEx(PBaseAllocator pAlloc)
{
	if (IsValidSingleton())
	{
		Free_Global();
		P()->~Utils();
		pAlloc->free(P());
		Assign(nullptr);
	}
	TestAllocCount();
}

void Utils::InitPaths()
{
	if (_companyFolderName.empty())
		_companyFolderName = "Verus Engine";

	wchar_t pathname[MAX_PATH] = {};
	if (_modulePath.empty())
	{
		GetModuleFileName(nullptr, pathname, MAX_PATH);
		PathRemoveFileSpec(pathname);
		_modulePath = Str::WideToUtf8(pathname);
	}

	_shaderPath = _modulePath + "/Data/Shaders";

	if (!_companyFolderName.empty() && !_writableFolderName.empty())
	{
		SZ prefPath = SDL_GetPrefPath(_C(_companyFolderName), _C(_writableFolderName));
		if (!prefPath)
			throw VERUS_RECOVERABLE << "SDL_GetPrefPath()";
		_writablePath = prefPath;
		SDL_free(prefPath);
		VERUS_LOG_INFO("Writable path: " << _writablePath);
	}
	else
	{
		VERUS_LOG_INFO("Writable path not specified");
	}
	VERUS_LOG_INFO("Module path: " << _modulePath);
}

void Utils::PushQuitEvent()
{
	SDL_Event event = {};
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

INT32 Utils::Cast32(INT64 x)
{
	if (x < std::numeric_limits<INT32>::min() || x > std::numeric_limits<INT32>::max())
		throw VERUS_RECOVERABLE << "Cast32 failed, x=" << x;
	return static_cast<INT32>(x);
}

UINT32 Utils::Cast32(UINT64 x)
{
	if (x < std::numeric_limits<UINT32>::min() || x > std::numeric_limits<UINT32>::max())
		throw VERUS_RECOVERABLE << "Cast32 failed, x=" << x;
	return static_cast<UINT32>(x);
}

void Utils::CopyColor(BYTE dest[4], UINT32 src)
{
	memcpy(dest, &src, sizeof(UINT32));
}

void Utils::CopyColor(UINT32& dest, const BYTE src[4])
{
	memcpy(&dest, src, sizeof(UINT32));
}

void Utils::CopyByteToInt4(const BYTE src[4], int dest[4])
{
	VERUS_FOR(i, 4)
		dest[i] = src[i];
}

void Utils::CopyIntToByte4(const int src[4], BYTE dest[4])
{
	VERUS_FOR(i, 4)
		dest[i] = src[i];
}

void Utils::ComputeEdgePadding(BYTE* pData, int dataPixelStride, const BYTE* pAlpha, int alphaPixelStride, int width, int height, int radius, int channelCount)
{
	channelCount = Math::Min(channelCount, dataPixelStride); // Assume one byte per channel.
	VERUS_RT_ASSERT(channelCount >= 1 && channelCount <= 4);
	if (!radius)
		radius = Math::Clamp((width + height) / 256, 1, 16);
	const int radiusSq = radius * radius;
	VERUS_P_FOR(i, height)
	{
		const int rowOffset = i * width;
		VERUS_FOR(j, width)
		{
			const BYTE alpha = pAlpha[alphaPixelStride * (rowOffset + j)];
			if (alpha)
				continue;
			BYTE* pDataChannels = &pData[dataPixelStride * (rowOffset + j)];

			int minRadiusSq = INT_MAX;
			BYTE color[4] = {};
			// Kernel:
			for (int di = -radius; di <= radius; ++di)
			{
				const int ki = i + di;
				if (ki < 0 || ki >= height)
					continue;
				for (int dj = -radius; dj <= radius; ++dj)
				{
					const int kj = j + dj;
					if (kj < 0 || kj >= width)
						continue;

					if (!di && !dj)
						continue; // Skip origin.
					const int lenSq = di * di + dj * dj;
					if (lenSq > radiusSq)
						continue;
					const BYTE kernelAlpha = pAlpha[alphaPixelStride * (ki * width + kj)];
					if (!kernelAlpha)
						continue;

					if (lenSq < minRadiusSq)
					{
						minRadiusSq = lenSq;
						BYTE* pKernelChannels = &pData[dataPixelStride * (ki * width + kj)];
						memcpy(color, pKernelChannels, channelCount);
					}
				}
			}

			if (minRadiusSq != INT_MAX)
				memcpy(pDataChannels, color, channelCount);
		}
	});
}

void Utils::TestAll()
{
	Convert::Test();
	Str::Test();
	Math::Test();
	Security::CipherRC4::Test();
}
