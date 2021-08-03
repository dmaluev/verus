// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace IO
	{
		// Treat a file as a stream.
		class File : public SeekableStream
		{
			FILE* _pFile = nullptr;

		public:
			File();
			~File();

			bool Open(CSZ pathname, CSZ mode = "rb");
			void Close();

			virtual INT64 Read(void* p, INT64 size) override;
			virtual INT64 Write(const void* p, INT64 size) override;

			virtual void Seek(INT64 offset, int origin) override;
			virtual INT64 GetPosition() override;

			FILE* GetFile() { return _pFile; }
		};
		VERUS_TYPEDEFS(File);
	}
}
