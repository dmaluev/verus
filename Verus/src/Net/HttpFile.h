// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Net
	{
		// Provides ability to read files from the internet using HTTP.
		class HttpFile : public IO::Stream
		{
			static const int s_httpPort = 80;
			static const int s_schemeLength = 7;
			static const int s_maxContentLength = 1024 * 1024;

			typedef Map<String, String> TMapHeader;

			String           _url;
			Vector<BYTE>     _vData;
			Socket           _socket;
			std::thread      _thread;
			TMapHeader       _mapHeader;
			BYTE             _buffer[200];
			INT64            _offset = 0;
			INT64            _size = 0;
			int              _contentPart = 0;
			std::atomic_bool _ready;
			bool             _nullTerm = false;

		public:
			HttpFile();
			~HttpFile();

			bool Open(CSZ url);
			void Close();

			virtual INT64 Read(void* p, INT64 size) override;
			virtual INT64 Write(const void* p, INT64 size) override;

			INT64 GetSize();

			bool IsReady() const { return _ready; }
			void DownloadAsync(CSZ url, bool nullTerm = false);
			VERUS_P(void ThreadProc());
			const BYTE* GetData() const { return _vData.data(); }
		};
		VERUS_TYPEDEFS(HttpFile);
	}
}
