// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#define VERUS_INCLUDE_COMPRESSONATOR
#include <verus.h>

using namespace verus;

void DeleteMipmap(CWSZ pathname);

CMP_BOOL CompressionCallback(CMP_FLOAT progress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
	std::wcout << _T("ProcessTexture progress = ") << static_cast<int>(progress) << _T("%") << std::endl;
	return false;
}

void Run()
{
	int argCount = 0;
	LPWSTR* argArray = CommandLineToArgvW(GetCommandLine(), &argCount);
	WideString pathnameW;
	std::wcout << _T("TextureTool") << std::endl;
	std::wcout << _T("Copyright (c) 2016-2021 Dmitry Maluev") << std::endl;
	if (argCount < 2)
	{
		std::wcout << _T("Enter file name: ");
		std::wcin >> pathnameW;
	}
	else
		pathnameW = argArray[argCount - 1];
	String pathname = Str::WideToUtf8(pathnameW);
	bool argDeleteMipmap = false;
	bool argFade = false;
	bool argNormalMap = false;
	bool argColorData = true;
	VERUS_FOR(i, argCount)
	{
		if (!_wcsicmp(argArray[i], L"--delete-mipmap"))
			argDeleteMipmap = true;
		if (!_wcsicmp(argArray[i], L"--fade"))
			argFade = true;
		if (!_wcsicmp(argArray[i], L"--normal-map"))
			argNormalMap = true;
		if (!_wcsicmp(argArray[i], L"--non-color-data"))
			argColorData = false;
	}
	LocalFree(argArray);

	if (argDeleteMipmap)
	{
		DeleteMipmap(_C(pathnameW));
		return;
	}

	CMP_ERROR cmpError = CMP_OK;

	CMP_MipSet srcMipSet = {};
	cmpError = CMP_LoadTexture(_C(pathname), &srcMipSet);
	if (CMP_OK != cmpError)
	{
		std::wcerr << _T("ERROR: CMP_LoadTexture(), ") << cmpError << std::endl;
		throw std::exception();
	}

	// FX image with extra info:
	Vector<UINT32> vMipFX[2];
	String pathnameFX(pathname);
	Str::ReplaceExtension(pathnameFX, ".FX.png");
	if (argNormalMap && IO::FileSystem::FileExist(_C(pathnameFX)))
	{
		std::wcout << _T("FX file: ") << Str::Utf8ToWide(pathnameFX) << std::endl;
		IO::Image imageFX;
		imageFX.Init(_C(pathnameFX));
		if (imageFX._width == srcMipSet.m_nWidth && imageFX._height == srcMipSet.m_nHeight)
		{
			const int pixelCount = imageFX._width * imageFX._height;
			vMipFX[0].resize(pixelCount);
			vMipFX[1].resize(pixelCount);
			VERUS_FOR(i, pixelCount)
				memcpy(&vMipFX[0][i], imageFX._p + imageFX._bytesPerPixel * i, imageFX._bytesPerPixel);
		}
		else
		{
			std::wcerr << _T("ERROR: FX image has different dimensions: ") << imageFX._width << _T("x") << imageFX._height << std::endl;
			throw std::exception();
		}
	}

	if (srcMipSet.m_nMipLevels <= 1)
		CMP_GenerateMIPLevels(&srcMipSet, 0);

	CMP_MipLevel* pMipLevel = nullptr;
	CMP_GetMipLevel(&pMipLevel, &srcMipSet, 0, 0);
	const BYTE* pData = pMipLevel->m_pbData;
	const int pixelCount = srcMipSet.m_nWidth * srcMipSet.m_nHeight;

	Vector<UINT32> vMip[2];
	vMip[0].resize(pixelCount);
	vMip[1].resize(pixelCount);

	const float fadePerMip = pow(0.02f, 1.f / srcMipSet.m_nMipLevels);

	int mip = 0;

	bool finish = false;
	do
	{
		std::wcout << _T("Processing mipmap ") << mip << std::endl;

		const int mipW = Math::Max(1, srcMipSet.m_nWidth >> mip);
		const int mipH = Math::Max(1, srcMipSet.m_nHeight >> mip);

		// Switch between in and out:
		const int inIndex = (mip & 0x1);
		const int outIndex = ((mip + 1) & 0x1);

		VERUS_FOR(i, mipH)
		{
			VERUS_FOR(j, mipW)
			{
				int offset = 0;
				UINT32 color = 0;
				UINT32 colorFX = 0;

				if (mip) // Generate mipmap level 1+:
				{
					offset = i * mipW + j;

					const int aboveMipW = mipW << 1;
					const int aboveMipH = mipH << 1;
					const int aboveMipEdgeW = aboveMipW - 1;
					const int aboveMipEdgeH = aboveMipH - 1;

					const int sampleA[2] = { Math::Clamp((j << 1) + 0, 0, aboveMipEdgeW), Math::Clamp((i << 1) + 0, 0, aboveMipEdgeH) };
					const int sampleB[2] = { Math::Clamp((j << 1) + 1, 0, aboveMipEdgeW), Math::Clamp((i << 1) + 0, 0, aboveMipEdgeH) };
					const int sampleC[2] = { Math::Clamp((j << 1) + 0, 0, aboveMipEdgeW), Math::Clamp((i << 1) + 1, 0, aboveMipEdgeH) };
					const int sampleD[2] = { Math::Clamp((j << 1) + 1, 0, aboveMipEdgeW), Math::Clamp((i << 1) + 1, 0, aboveMipEdgeH) };

					const int offsetA = sampleA[1] * aboveMipW + sampleA[0];
					const int offsetB = sampleB[1] * aboveMipW + sampleB[0];
					const int offsetC = sampleC[1] * aboveMipW + sampleC[0];
					const int offsetD = sampleD[1] * aboveMipW + sampleD[0];

					if (argNormalMap)
					{
						Vector4 data[4];
						float raw[4];
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetA], raw, false); data[0] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetB], raw, false); data[1] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetC], raw, false); data[2] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetD], raw, false); data[3] = Vector4::MakeFromPointer(raw);

						// Generate FX mipmap:
						if (!vMipFX[inIndex].empty())
						{
							const BYTE* rgbaA = reinterpret_cast<BYTE*>(&vMipFX[inIndex][offsetA]);
							const BYTE* rgbaB = reinterpret_cast<BYTE*>(&vMipFX[inIndex][offsetB]);
							const BYTE* rgbaC = reinterpret_cast<BYTE*>(&vMipFX[inIndex][offsetC]);
							const BYTE* rgbaD = reinterpret_cast<BYTE*>(&vMipFX[inIndex][offsetD]);
							BYTE rgbaOut[4] =
							{
								static_cast<BYTE>((rgbaA[0] + rgbaB[0] + rgbaC[0] + rgbaD[0]) >> 2),
								static_cast<BYTE>((rgbaA[1] + rgbaB[1] + rgbaC[1] + rgbaD[1]) >> 2),
								static_cast<BYTE>((rgbaA[2] + rgbaB[2] + rgbaC[2] + rgbaD[2]) >> 2),
								static_cast<BYTE>((rgbaA[3] + rgbaB[3] + rgbaC[3] + rgbaD[3]) >> 2)
							};
							memcpy(&colorFX, rgbaOut, 4);
						}

						const float prevAvgLength[4] =
						{
							data[0].getW(),
							data[1].getW(),
							data[2].getW(),
							data[3].getW()
						};
						data[0].setW(0);
						data[1].setW(0);
						data[2].setW(0);
						data[3].setW(0);

						float minAvgLength = 1;
						VERUS_FOR(i, 4) // To [-1 to 1]:
						{
							data[i] = data[i] * 2.f - Vector4(1, 1, 1, 0);
							minAvgLength = Math::Min(minAvgLength, prevAvgLength[i]);
						}

						Vector4 avg = (
							data[0] * prevAvgLength[0] +
							data[1] * prevAvgLength[1] +
							data[2] * prevAvgLength[2] +
							data[3] * prevAvgLength[3]) * 0.25f;
						float len = VMath::length(avg);

						if (len > minAvgLength) // Don't make it too smooth.
							len = (len + minAvgLength) * 0.5f;

						Vector4 dataOut = VMath::normalize(data[0] + data[1] + data[2] + data[3]);
						if (argFade)
							dataOut = VMath::normalize(VMath::lerp(fadePerMip, Vector4(0, 0, 1, 0), dataOut));
						dataOut = dataOut * 0.5f + Vector4(0.5f, 0.5f, 0.5f, 0); // To [0 to 1]:
						dataOut.setW(len);
						color = Convert::ColorFloatToInt32(dataOut.ToPointer(), false);
					}
					else
					{
						Vector4 data[4];
						float raw[4];
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetA], raw, argColorData); data[0] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetB], raw, argColorData); data[1] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetC], raw, argColorData); data[2] = Vector4::MakeFromPointer(raw);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetD], raw, argColorData); data[3] = Vector4::MakeFromPointer(raw);
						Vector4 dataOut = (data[0] + data[1] + data[2] + data[3]) * 0.25f;
						color = Convert::ColorFloatToInt32(dataOut.ToPointer(), argColorData);
					}
				}
				else // Handle topmost mipmap level:
				{
					if (argNormalMap)
					{
						const bool upsideDownBGRA = false;
						if (upsideDownBGRA)
						{
							// Input image is upside down BGRA (?):
							offset = (mipH - i - 1) * mipW + j;
							UINT32 input = 0;
							memcpy(&input, &pData[offset * 4], 4);
							reinterpret_cast<BYTE*>(&input)[3] >>= 2; // Divide alpha by 4.
							color = VERUS_COLOR_TO_D3D(input);
							offset = i * mipW + j;
						}
						else
						{
							offset = i * mipW + j;
							UINT32 input = 0;
							memcpy(&input, &pData[offset * 4], 4);
							reinterpret_cast<BYTE*>(&input)[3] = 0xFF;
							color = input;

							if (!vMipFX[inIndex].empty())
							{
								colorFX = vMipFX[inIndex][offset];
								BYTE* rgbaFX = reinterpret_cast<BYTE*>(&colorFX);
								rgbaFX[0] /= 4;
								rgbaFX[1] /= 4;
								rgbaFX[2] /= 4;
								rgbaFX[3] /= 4;
							}
						}
					}
					else // Simple copy for color:
					{
						offset = i * mipW + j;
						UINT32 input = 0;
						memcpy(&input, &pData[offset * 4], 4);
						color = input;
					}
				}
				vMip[outIndex][offset] = color;
				if (!vMipFX[outIndex].empty())
					vMipFX[outIndex][offset] = colorFX;
			}
		}

		if (argNormalMap)
		{
			// Shuffle channels, read from outIndex, write to inIndex:
			const int pixelCount = mipW * mipH;
			VERUS_FOR(i, pixelCount)
			{
				BYTE* rgbaFX = vMipFX[outIndex].empty() ? nullptr : reinterpret_cast<BYTE*>(&vMipFX[outIndex][i]);
				BYTE* rgba = reinterpret_cast<BYTE*>(&vMip[outIndex][i]);
				const int len = Math::Clamp(rgba[3] - 128, 0, 0xFF);
				vMip[inIndex][i] = VERUS_COLOR_RGBA(rgbaFX ? rgbaFX[0] : 0, rgba[1], len, rgba[0]);
			}

			// Save shuffled mip level, read from  inIndex:
			CMP_MipLevel* pDstMipLevel = nullptr;
			CMP_GetMipLevel(&pDstMipLevel, &srcMipSet, mip, 0);
			memcpy(pDstMipLevel->m_pbData, vMip[inIndex].data(), mipW * mipH * sizeof(UINT32));
		}
		else
		{
			CMP_MipLevel* pDstMipLevel = nullptr;
			CMP_GetMipLevel(&pDstMipLevel, &srcMipSet, mip, 0);
			memcpy(pDstMipLevel->m_pbData, vMip[outIndex].data(), mipW * mipH * sizeof(UINT32));
		}

		finish = (mipW == 1) && (mipH == 1);
		mip++; // Next level, switch between in and out.
	} while (!finish);

	CMP_MipSet dstMipSet = {};

	KernelOptions kernelOptions = {};
	kernelOptions.format = CMP_FORMAT_BC7;
	kernelOptions.fquality = 0.51f; // Tests show that 51% is production quality.

	cmpError = CMP_ProcessTexture(&srcMipSet, &dstMipSet, kernelOptions, CompressionCallback);
	if (CMP_OK != cmpError)
	{
		std::wcerr << _T("ERROR: CMP_ProcessTexture(), ") << cmpError << std::endl;
		throw std::exception();
	}

	Str::ReplaceExtension(pathname, ".dds");
	cmpError = CMP_SaveTexture(_C(pathname), &dstMipSet);
	if (CMP_OK != cmpError)
	{
		std::wcerr << _T("ERROR: CMP_SaveTexture(), ") << cmpError << std::endl;
		throw std::exception();
	}

	CMP_FreeMipSet(&srcMipSet);
	CMP_FreeMipSet(&dstMipSet);
}

void DeleteMipmap(CWSZ pathname)
{
	const String pathnameIn = Str::WideToUtf8(pathname);
	const String pathnameOut = pathnameIn + "-del-mip.dds";
	IO::File file, fileOut;
	if (file.Open(_C(pathnameIn)) && file.GetSize() > sizeof(IO::DDSHeader) && fileOut.Open(_C(pathnameOut), "wb"))
	{
		IO::DDSHeader header;
		file >> header;
		if (!header.Validate())
		{
			std::wcerr << _T("ERROR: Invalid DDS header: ") << Str::Utf8ToWide(pathnameIn) << std::endl;
			throw std::exception();
		}
		if (header.IsBC1() && header._mipMapCount > 1)
		{
			const int levelSize = IO::DDSHeader::ComputeBcLevelSize(header._width, header._height, true);

			header._mipMapCount--;
			header._width >>= 1;
			header._height >>= 1;
			header._pitchOrLinearSize = IO::DDSHeader::ComputeBcLevelSize(header._width, header._height, true);

			fileOut << header;
			file.Seek(sizeof(IO::DDSHeader) + levelSize, SEEK_SET);
			Vector<BYTE> vData;
			vData.resize(file.GetSize() - levelSize - sizeof(IO::DDSHeader));
			file.Read(vData.data(), vData.size());
			fileOut.Write(vData.data(), vData.size());
		}
		else
		{
			std::wcerr << _T("ERROR: Must be DXT1 with mipmaps") << std::endl;
			throw std::exception();
		}
	}
}

int main(VERUS_MAIN_DEFAULT_ARGS)
{
	try
	{
		Run();
	}
	catch (const std::exception& e)
	{
		std::wcerr << _T("EXCEPTION: ") << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
