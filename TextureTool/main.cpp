// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#define VERUS_INCLUDE_COMPRESSONATOR
#include <verus.h>

using namespace verus;

class TextureTool
{
	struct CursorGuard
	{
		CONSOLE_CURSOR_INFO _defaultCursorInfo;
		CursorGuard()
		{
			GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &_defaultCursorInfo);
			CONSOLE_CURSOR_INFO info = {};
			info.dwSize = 100;
			info.bVisible = FALSE;
			SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
		}
		~CursorGuard()
		{
			SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &_defaultCursorInfo);
		}
	};

	enum class TexMode : int
	{
		automatic,
		color,
		nonColor,
		a,
		n,
		x,
		all
	};

	String  _pathname;
	TexMode _texMode = TexMode::automatic;
	bool    _asIs = false;
	bool    _edgePaddingUsingAlpha = false;
	bool    _edgePaddingUsingFaces = false;
	bool    _deleteMip = false;

	static CMP_BOOL FeedbackProc(CMP_FLOAT progress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2);

public:
	TextureTool();
	~TextureTool();

	void Main(VERUS_MAIN_DEFAULT_ARGS);
	void ParseCommandLine(VERUS_MAIN_DEFAULT_ARGS);
	void PrintUsage();
	void ConvertTexture(TexMode currentTexMode);
	void ComputeEdgePadding();
};

CMP_BOOL TextureTool::FeedbackProc(CMP_FLOAT progress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
	const int edge = static_cast<int>(progress * 0.5f + 0.5f);
	wchar_t buffer[80];
	wcscpy(buffer, _T("\rCompressing ["));
	size_t offset = wcslen(buffer);
	VERUS_FOR(i, 50)
	{
		if (i < edge)
			buffer[i + offset] = _T('=');
		else
			buffer[i + offset] = _T(' ');
	}
	buffer[offset + 50] = 0;
	wsprintf(buffer + wcslen(buffer), _T("] %3d%%"), static_cast<int>(progress + 0.5f));
	wprintf(buffer);
	fflush(stdout);
	return false;
}

TextureTool::TextureTool()
{
}

TextureTool::~TextureTool()
{
}

void TextureTool::Main(VERUS_MAIN_DEFAULT_ARGS)
{
	if (argc <= 1)
	{
		PrintUsage();
		return;
	}
	ParseCommandLine(argc, argv);

	if (_edgePaddingUsingAlpha || _edgePaddingUsingFaces)
	{
		ComputeEdgePadding();
	}
	else if (_deleteMip)
	{
	}
	else
	{
		std::wcout << _T("Mode: ");
		switch (_texMode)
		{
		case TexMode::automatic: std::wcout << _T("automatic"); break;
		case TexMode::color:     std::wcout << _T("color"); break;
		case TexMode::nonColor:  std::wcout << _T("non-color"); break;
		case TexMode::a:         std::wcout << _T("a"); break;
		case TexMode::n:         std::wcout << _T("n"); break;
		case TexMode::x:         std::wcout << _T("x"); break;
		case TexMode::all:       std::wcout << _T("all"); break;
		}
		std::wcout << std::endl;

		if (TexMode::automatic == _texMode)
		{
			const String ext = Str::GetExtension(_C(_pathname));
			if (Str::EndsWith(_C(_pathname), _C(".X." + ext)))
				ConvertTexture(TexMode::nonColor);
			else
				ConvertTexture(TexMode::color);
		}
		else if (TexMode::all == _texMode)
		{
			const String pathname = _pathname;
			const String ext = Str::GetExtension(_C(pathname));

			ConvertTexture(TexMode::a);
			_pathname = pathname;
			Str::ReplaceExtension(_pathname, ".N.png");
			ConvertTexture(TexMode::n);
			_pathname = pathname;
			Str::ReplaceExtension(_pathname, _C(".X." + ext));
			ConvertTexture(TexMode::x);
		}
		else
		{
			ConvertTexture(_texMode);
		}
	}
}

void TextureTool::ParseCommandLine(VERUS_MAIN_DEFAULT_ARGS)
{
	VERUS_FOR(i, argc)
	{
		if (!strcmp(argv[i], "--tex-auto"))
			_texMode = TexMode::automatic;
		else if (!strcmp(argv[i], "--tex-color"))
			_texMode = TexMode::color;
		else if (!strcmp(argv[i], "--tex-non-color"))
			_texMode = TexMode::nonColor;
		else if (!strcmp(argv[i], "--tex-a"))
			_texMode = TexMode::a;
		else if (!strcmp(argv[i], "--tex-n"))
			_texMode = TexMode::n;
		else if (!strcmp(argv[i], "--tex-x"))
			_texMode = TexMode::x;
		else if (!strcmp(argv[i], "--tex-all"))
			_texMode = TexMode::all;
		else if (!strcmp(argv[i], "--as-is"))
			_asIs = true;
		else if (!strcmp(argv[i], "--edge-padding-using-alpha") || !strcmp(argv[i], "-epua"))
			_edgePaddingUsingAlpha = true;
		else if (!strcmp(argv[i], "--edge-padding-using-faces") || !strcmp(argv[i], "-epuf"))
			_edgePaddingUsingFaces = true;
		else if (!strcmp(argv[i], "--delete-mip"))
			_deleteMip = true;
		else if (i > 0 && i < argc - 1)
		{
			std::wcout << _T("WARNING: Unknown argument ") << Str::Utf8ToWide(argv[i]) << std::endl;
			std::cin.ignore();
		}
	}
#ifdef _WIN32
	int argCount = 0;
	LPWSTR* argArray = CommandLineToArgvW(GetCommandLine(), &argCount);
	_pathname = Str::WideToUtf8(argArray[argCount - 1]);
	LocalFree(argArray);
#endif
}

void TextureTool::PrintUsage()
{
	std::wcout << _T("Usage: TextureTool [options] <file>") << std::endl;
	std::wcout << std::endl;
	std::wcout << _T("TextureTool 2. Powered by Compressonator.") << std::endl;
	std::wcout << _T("Preferred output formats are DDS/BC5 for normal map and DDS/BC7 for everything else.") << std::endl;
	std::wcout << _T("Create a batch file with similar content to convert multiple files:") << std::endl;
	std::wcout << _T("  for %%f in (*.*) do TextureTool %%f") << std::endl;
	std::wcout << std::endl;
	std::wcout << _T("Options:") << std::endl;
	std::wcout << _T("  --tex-auto                        Use filename to select between color and non-color data (default).") << std::endl;
	std::wcout << _T("  --tex-color                       Convert texture with color (sRGB).") << std::endl;
	std::wcout << _T("  --tex-non-color                   Convert texture with non-color data (linear).") << std::endl;
	std::wcout << _T("  --tex-a                           Convert {Albedo.rgb, Alpha} texture.") << std::endl;
	std::wcout << _T("  --tex-n                           Convert Normal Map texture.") << std::endl;
	std::wcout << _T("  --tex-x                           Convert {Occlusion, Roughness, Metallic, Emission} texture.") << std::endl;
	std::wcout << _T("  --tex-all                         Convert all textures.") << std::endl;
	std::wcout << _T("  --as-is                           Don't use block compression.") << std::endl;
	std::wcout << _T("  --edge-padding-using-alpha, -epua Add edge padding to an image with alpha.") << std::endl;
	std::wcout << _T("  --edge-padding-using-faces, -epuf Add edge padding to an image, take alpha from Faces.png.") << std::endl;
	std::wcout << _T("  --delete-mip                      Delete topmost mip level.") << std::endl;
}

void TextureTool::ConvertTexture(TexMode currentTexMode)
{
	std::wcout << _T("Converting texture: ");
	switch (currentTexMode)
	{
	case TexMode::color:    std::wcout << _T("COLOR"); break;
	case TexMode::nonColor: std::wcout << _T("NON-COLOR"); break;
	case TexMode::a:        std::wcout << _T("A (Albedo, Alpha)"); break;
	case TexMode::n:        std::wcout << _T("N (Normal Map)"); break;
	case TexMode::x:        std::wcout << _T("X (Occlusion, Roughness, Metallic, Emission)"); break;
	default:
		std::wcerr << _T("ERROR: Unknown mode.") << std::endl;
		throw std::exception();
	}
	std::wcout << std::endl;
	std::wcout << _T("Location: ") << Str::Utf8ToWide(_pathname) << std::endl;

	CMP_ERROR cmpError = CMP_OK;

	CMP_MipSet srcMipSet = {};
	cmpError = CMP_LoadTexture(_C(_pathname), &srcMipSet);
	if (CMP_OK != cmpError)
	{
		std::wcerr << _T("ERROR: CMP_LoadTexture(); ") << cmpError << std::endl;
		throw std::exception();
	}

	if (srcMipSet.m_nMipLevels <= 1)
		CMP_GenerateMIPLevels(&srcMipSet, 0);

	CMP_MipLevel* pMipLevel = nullptr;
	CMP_GetMipLevel(&pMipLevel, &srcMipSet, 0, 0);
	const BYTE* pData = pMipLevel->m_pbData;
	const int pixelCount = srcMipSet.m_nWidth * srcMipSet.m_nHeight;

	bool normalMapToRoughness = false;
	const int bytesPerPixel = sizeof(UINT32);
	Vector<UINT32> vMip[4];
	vMip[0].resize(pixelCount);
	vMip[1].resize(pixelCount);
	if (TexMode::x == currentTexMode)
	{
		String pathname = _pathname;
		Str::ReplaceExtension(pathname, "");
		Str::ReplaceExtension(pathname, ".N.TempR.tga");
		if (IO::FileSystem::FileExist(_C(pathname)))
		{
			IO::Image image;
			image.Init(_C(pathname));
			if (image._width == srcMipSet.m_nWidth && image._height == srcMipSet.m_nHeight)
			{
				vMip[2].resize(pixelCount);
				vMip[3].resize(pixelCount);
				memcpy(vMip[3].data(), image._p, vMip[3].size() * bytesPerPixel);
				normalMapToRoughness = true;
			}
			else
			{
				std::wcout << _T("WARNING: Normal map for roughness has different size.") << std::endl;
				std::cin.ignore();
			}
		}

		if (!normalMapToRoughness)
		{
			std::wcout << _T("WARNING: Normal map for roughness not found.") << std::endl;
			std::cin.ignore();
		}
	}

	const bool sRGB = (TexMode::color == currentTexMode) || (TexMode::a == currentTexMode);
	bool normWarning = false;
	int mip = 0;

	bool finish = false;
	do
	{
		const int mipW = Math::Max(1, srcMipSet.m_nWidth >> mip);
		const int mipH = Math::Max(1, srcMipSet.m_nHeight >> mip);

		std::wcout << _T("Processing mip ") << std::setw(2) << mip << _T(" [");
		const int length = Math::HighestBit(mipW) + 1;
		VERUS_FOR(i, length)
			std::wcout << _T("=");
		std::wcout << _T("]") << std::endl;

		// Switch between in and out:
		const int inIndex = (mip & 0x1);
		const int outIndex = ((mip + 1) & 0x1);

		VERUS_FOR(i, mipH)
		{
			const int offsetH = i * mipW;
			VERUS_FOR(j, mipW)
			{
				int offset = offsetH;
				UINT32 color = 0;
				UINT32 color2 = 0;

				if (mip) // Generate mip level 1+:
				{
					offset += j;

					const int srcMipW = mipW << 1;
					const int srcMipH = mipH << 1;
					const int srcMipEdgeW = srcMipW - 1;
					const int srcMipEdgeH = srcMipH - 1;

					const int coordA[2] = { Math::Clamp((j << 1) + 0, 0, srcMipEdgeW), Math::Clamp((i << 1) + 0, 0, srcMipEdgeH) };
					const int coordB[2] = { Math::Clamp((j << 1) + 1, 0, srcMipEdgeW), Math::Clamp((i << 1) + 0, 0, srcMipEdgeH) };
					const int coordC[2] = { Math::Clamp((j << 1) + 0, 0, srcMipEdgeW), Math::Clamp((i << 1) + 1, 0, srcMipEdgeH) };
					const int coordD[2] = { Math::Clamp((j << 1) + 1, 0, srcMipEdgeW), Math::Clamp((i << 1) + 1, 0, srcMipEdgeH) };

					const int offsetA = coordA[1] * srcMipW + coordA[0];
					const int offsetB = coordB[1] * srcMipW + coordB[0];
					const int offsetC = coordC[1] * srcMipW + coordC[0];
					const int offsetD = coordD[1] * srcMipW + coordD[0];

					BYTE computedRoughness = 0;
					if (TexMode::n == currentTexMode || normalMapToRoughness) // N or X:
					{
						// Compute average normal:
						const int inIndexEx = normalMapToRoughness ? inIndex + 2 : inIndex;
						Vector4 data[4];
						float floats[4];
						Convert::ColorInt32ToFloat(vMip[inIndexEx][offsetA], floats, false); data[0] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndexEx][offsetB], floats, false); data[1] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndexEx][offsetC], floats, false); data[2] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndexEx][offsetD], floats, false); data[3] = Vector4::MakeFromPointer(floats);

						VERUS_FOR(i, 4) // To [-1 to 1]:
						{
							data[i] = data[i] * 2.f - Vector4::Replicate(1);
							data[i].setW(0);
						}
						const Vector4 averageNormal = VMath::normalize(data[0] + data[1] + data[2] + data[3]);
						Vector4 computedData = averageNormal;
						computedData = computedData * 0.5f + Vector4::Replicate(0.5f); // To [0 to 1]:
						computedData.setW(0);
						color = Convert::ColorFloatToInt32(computedData.ToPointer(), false);

						// Compute roughness from normal:
						const bool saveRoughnessToAlpha = TexMode::n == currentTexMode && 1 == mip;
						if (normalMapToRoughness || saveRoughnessToAlpha)
						{
							color2 = color;
							float averageRoughness = 0;
							VERUS_FOR(i, 4)
							{
								const float d = VMath::dot(averageNormal, data[i]);
								const float bias = 0.85f;
								const float scale = 1.f / (1.f - 0.85f);
								averageRoughness += 1.f - Math::Clamp<float>((d - bias) * scale, 0, 1);
							}
							averageRoughness = Math::Clamp<float>(averageRoughness * 0.25f, 0, 1);
							computedRoughness = Convert::UnormToUint8(averageRoughness);

							if (saveRoughnessToAlpha)
							{
								BYTE* pChannels = reinterpret_cast<BYTE*>(&color);
								pChannels[3] = computedRoughness;
							}
						}
					}
					if (TexMode::n != currentTexMode)
					{
						// Compute average value:
						Vector4 data[4];
						float floats[4];
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetA], floats, sRGB); data[0] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetB], floats, sRGB); data[1] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetC], floats, sRGB); data[2] = Vector4::MakeFromPointer(floats);
						Convert::ColorInt32ToFloat(vMip[inIndex][offsetD], floats, sRGB); data[3] = Vector4::MakeFromPointer(floats);

						Vector4 computedData = (data[0] + data[1] + data[2] + data[3]) * 0.25f;
						color = Convert::ColorFloatToInt32(computedData.ToPointer(), sRGB);

						if (normalMapToRoughness) // For X:
						{
							BYTE* pChannels = reinterpret_cast<BYTE*>(&color);
							pChannels[1] = Math::Max(pChannels[1], computedRoughness);
						}
					}
				}
				else // Handle topmost mip level:
				{
					offset += j;
					UINT32 input = 0;
					memcpy(&input, &pData[offset * bytesPerPixel], bytesPerPixel);
					// Normalize source normal map:
					if (TexMode::n == currentTexMode)
					{
						Vector4 data;
						float floats[4];
						Convert::ColorInt32ToFloat(input, floats, false); data = Vector4::MakeFromPointer(floats);
						data = data * 2.f - Vector4::Replicate(1); // To [-1 to 1]:
						data.setW(0);
						const float length = VMath::length(data);
						if (abs(length - 1.f) >= 0.01f)
							normWarning = true;
						data /= length;
						data = data * 0.5f + Vector4::Replicate(0.5f); // To [0 to 1]:
						data.setW(0);
						input = Convert::ColorFloatToInt32(data.ToPointer(), false);
					}
					// Add computed roughness from normal map:
					if (normalMapToRoughness)
					{
						const BYTE computedRoughness = vMip[outIndex + 2][offset] >> 24;
						BYTE* pChannels = reinterpret_cast<BYTE*>(&input);
						pChannels[1] = Math::Max(pChannels[1], computedRoughness);
					}
					color = input;
				}

				vMip[outIndex][offset] = color;
				if (mip && normalMapToRoughness)
					vMip[outIndex + 2][offset] = color2;
			}
		}

		// Copy to CMP_MipLevel:
		CMP_MipLevel* pDstMipLevel = nullptr;
		CMP_GetMipLevel(&pDstMipLevel, &srcMipSet, mip, 0);
		memcpy(pDstMipLevel->m_pbData, vMip[outIndex].data(), mipW * mipH * bytesPerPixel);

		finish = (mipW == 1) && (mipH == 1);
		mip++; // Next level, switch between in and out.

		if (2 == mip && TexMode::n == currentTexMode)
		{
			String pathname = _pathname;
			Str::ReplaceExtension(pathname, ".TempR.tga");
			IO::FileSystem::SaveImage(_C(pathname), vMip[outIndex].data(), mipW, mipH);
			std::wcout << _T("Saved normal map for roughness as: ") << Str::Utf8ToWide(pathname) << std::endl;
		}
	} while (!finish);

	if (normWarning)
	{
		std::wcout << _T("WARNING: Source normal map was not normalized.") << std::endl;
		std::cin.ignore();
	}

	Str::ReplaceExtension(_pathname, ".dds");
	if (_asIs)
	{
		cmpError = CMP_SaveTexture(_C(_pathname), &srcMipSet);
		if (CMP_OK != cmpError)
		{
			std::wcerr << _T("ERROR: CMP_SaveTexture(); ") << cmpError << std::endl;
			throw std::exception();
		}
	}
	else
	{
		CMP_MipSet dstMipSet = {};

		KernelOptions kernelOptions = {};
		if (TexMode::n == currentTexMode)
		{
			kernelOptions.format = CMP_FORMAT_BC5;
			kernelOptions.fquality = 0.8f; // Use 80% for BC5.
		}
		else
		{
			kernelOptions.format = CMP_FORMAT_BC7;
			kernelOptions.fquality = 0.6f; // Use 60% for BC7.
		}

		{
			CursorGuard cursorGuard;
			cmpError = CMP_ProcessTexture(&srcMipSet, &dstMipSet, kernelOptions, FeedbackProc);
			std::wcout << std::endl;
		}
		if (CMP_OK != cmpError)
		{
			std::wcerr << _T("ERROR: CMP_ProcessTexture(), ") << cmpError << std::endl;
			throw std::exception();
		}

		cmpError = CMP_SaveTexture(_C(_pathname), &dstMipSet);
		if (CMP_OK != cmpError)
		{
			std::wcerr << _T("ERROR: CMP_SaveTexture(); ") << cmpError << std::endl;
			throw std::exception();
		}

		CMP_FreeMipSet(&dstMipSet);
	}
	CMP_FreeMipSet(&srcMipSet);

	std::wcout << _T("Saved as: ") << Str::Utf8ToWide(_pathname) << std::endl;
}

void TextureTool::ComputeEdgePadding()
{
	if (!IO::FileSystem::FileExist(_C(_pathname)))
	{
		std::wcerr << _T("ERROR: File not found: ") << Str::Utf8ToWide(_pathname) << std::endl;
		throw std::exception();
	}
	IO::Image image;
	image.Init(_C(_pathname));

	IO::Image imageFaces;
	if (_edgePaddingUsingFaces)
	{
		Str::ReplaceFilename(_pathname, "Faces.png");
		if (!IO::FileSystem::FileExist(_C(_pathname)))
		{
			std::wcerr << _T("ERROR: File not found: ") << Str::Utf8ToWide(_pathname) << std::endl;
			throw std::exception();
		}
		imageFaces.Init(_C(_pathname));
		if (image._width != imageFaces._width || image._height != imageFaces._height)
		{
			std::wcerr << _T("ERROR: Faces image has different size.") << std::endl;
			throw std::exception();
		}
	}

	if (_edgePaddingUsingAlpha)
		Utils::ComputeEdgePadding(image._p, image._pixelStride, image._p + 3, image._pixelStride, image._width, image._height, 0, 3);
	else if (_edgePaddingUsingFaces)
		Utils::ComputeEdgePadding(image._p, image._pixelStride, imageFaces._p, imageFaces._pixelStride, image._width, image._height, 0, 4);

	Str::ReplaceFilename(_pathname, "ComputedEdgePadding.tga");
	IO::FileSystem::SaveImage(_C(_pathname), reinterpret_cast<UINT32*>(image._p), image._width, image._height, IO::ImageFormat::tga, image._pixelStride);
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
		TextureTool textureTool;
		textureTool.Main(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::wcerr << _T("EXCEPTION: ") << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
