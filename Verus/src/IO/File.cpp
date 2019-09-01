#include "verus.h"

using namespace verus;
using namespace verus::IO;

File::File()
{
}

File::~File()
{
	Close();
}

bool File::Open(CSZ pathName, CSZ mode)
{
	Close();

#ifdef _WIN32
	const WideString pathNameW = Str::Utf8ToWide(pathName);
	const WideString modeW = Str::Utf8ToWide(mode);
	_pFile = _wfopen(_C(pathNameW), _C(modeW));
#else
	_pFile = fopen(pathName, mode);
#endif
	return nullptr != _pFile;
}

void File::Close()
{
	if (_pFile)
	{
		fclose(_pFile);
		_pFile = nullptr;
	}
}

INT64 File::Read(void* p, INT64 size)
{
	VERUS_RT_ASSERT(_pFile);
	memset(p, 0, static_cast<size_t>(size));
	return fread(p, 1, static_cast<size_t>(size), _pFile);
}

INT64 File::Write(const void* p, INT64 size)
{
	VERUS_RT_ASSERT(_pFile);
	return fwrite(p, 1, static_cast<size_t>(size), _pFile);
}

void File::Seek(INT64 offset, int origin)
{
	const int ret = fseek(_pFile, static_cast<long>(offset), origin);
	VERUS_RT_ASSERT(!ret);
}

INT64 File::GetPosition()
{
	return ftell(_pFile);
}
