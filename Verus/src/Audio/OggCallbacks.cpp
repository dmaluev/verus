#include "verus.h"

namespace verus
{
	namespace Audio
	{
		size_t read_func(void* ptr, size_t size, size_t nmemb, void* datasource)
		{
			POggDataSource pDS = static_cast<POggDataSource>(datasource);
			INT64 readLen = size * nmemb;
			const INT64 maxLen = pDS->_size - pDS->_cursor;
			if (readLen > maxLen)
				readLen = maxLen;
			memcpy(ptr, pDS->_p + pDS->_cursor, size_t(readLen));
			pDS->_cursor += readLen;
			return size_t(readLen);
		}

		int seek_func(void* datasource, ogg_int64_t offset, int whence)
		{
			POggDataSource pDS = static_cast<POggDataSource>(datasource);
			switch (whence)
			{
			case SEEK_SET: pDS->_cursor = offset; return 0;
			case SEEK_CUR: pDS->_cursor += offset; return 0;
			case SEEK_END: pDS->_cursor = pDS->_size - offset; return 0;
			}
			return -1;
		}

		int close_func(void* datasource)
		{
			POggDataSource pDS = static_cast<POggDataSource>(datasource);
			pDS->_cursor = 0;
			return 0;
		}

		long tell_func(void* datasource)
		{
			POggDataSource pDS = static_cast<POggDataSource>(datasource);
			return long(pDS->_cursor);
		}

		const ov_callbacks g_oggCallbacks = { read_func, seek_func, close_func, tell_func };
	}
}
