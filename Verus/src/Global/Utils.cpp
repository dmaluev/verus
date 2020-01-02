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
		_companyFolderName = "VerusEngine";

	wchar_t pathName[MAX_PATH] = {};
	if (_modulePath.empty())
	{
		GetModuleFileName(nullptr, pathName, MAX_PATH);
		PathRemoveFileSpec(pathName);
		_modulePath = Str::WideToUtf8(pathName);
	}

	_shaderPath = _modulePath + "/Data/Shaders";

	if (!_companyFolderName.empty() && !_writableFolderName.empty())
	{
		SHGetSpecialFolderPath(0, pathName, CSIDL_LOCAL_APPDATA, TRUE);
		_writablePath = Str::WideToUtf8(pathName);
		_writablePath += "\\";
		_writablePath += _companyFolderName;
		CreateDirectory(_C(Str::Utf8ToWide(_writablePath)), nullptr);
		_writablePath += "\\";
		_writablePath += _writableFolderName;
		CreateDirectory(_C(Str::Utf8ToWide(_writablePath)), nullptr);
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

void Utils::TestAll()
{
	Convert::Test();
	Str::Test();
	Math::Test();
	Security::CipherRC4::Test();
}
