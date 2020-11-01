// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Audio
	{
		struct OggDataSource
		{
			const BYTE* _p = nullptr;
			INT64       _size = 0;
			INT64       _cursor = 0;
		};
		VERUS_TYPEDEFS(OggDataSource);

		size_t read_func(void*, size_t, size_t, void*);
		int seek_func(void*, ogg_int64_t, int);
		int close_func(void*);
		long tell_func(void*);

		extern const ov_callbacks g_oggCallbacks;
	}
}
