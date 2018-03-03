#include "verus.h"

namespace verus
{
	VERUS_CT_ASSERT(IO::Stream::bufferSize == 256);

	void Make_IO()
	{
		IO::FileSystem::Make();
		IO::Async::Make();
	}
	void Free_IO()
	{
		IO::Async::Free();
		IO::FileSystem::Free();
	}
}
